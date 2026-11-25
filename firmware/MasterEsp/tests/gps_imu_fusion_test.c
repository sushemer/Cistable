#include "gps_imu_fusion.h"
#include "gps_validation.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned s_assertions;

#define CHECK(condition) do { \
    ++s_assertions; \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static bool near(float actual, float expected)
{
    return isfinite(actual) && fabsf(actual - expected) < 0.001f;
}

static gps_validation_config_t validation_config(void)
{
    return (gps_validation_config_t){
        .min_satellites = 4U,
        .max_hdop = 4.0f,
        .max_age_ms = 1800U,
        .max_epoch_skew_ms = 200U,
    };
}

static gps_sample_t valid_gps_sample(int64_t received_us)
{
    return (gps_sample_t){
        .has_rmc = true,
        .rmc_status_valid = true,
        .rmc_data_valid = true,
        .rmc_time_of_day_ms = 43200000U,
        .rmc_received_us = received_us,
        .speed_kmh = 12.5f,
        .has_gga = true,
        .gga_data_valid = true,
        .gga_time_of_day_ms = 43200000U,
        .gga_received_us = received_us,
        .fix_quality = 1,
        .satellites = 8U,
        .hdop = 1.2f,
    };
}

static void test_gps_quality_states(void)
{
    const gps_validation_config_t config = validation_config();
    gps_sample_t sample = {0};
    gps_validation_result_t result = gps_validation_evaluate(&sample, &config, 2000000LL);
    CHECK(result.state == GPS_VALIDATION_NO_DATA);
    CHECK(!result.usable_for_speed);

    sample = valid_gps_sample(1000000LL);
    result = gps_validation_evaluate(&sample, &config, 1200000LL);
    CHECK(result.state == GPS_VALIDATION_VALIDO);
    CHECK(result.usable_for_speed);
    CHECK(result.rmc_age_ms == 200U && result.gga_age_ms == 200U);

    sample.rmc_status_valid = false;
    sample.fix_quality = 0;
    result = gps_validation_evaluate(&sample, &config, 1200000LL);
    CHECK(result.state == GPS_VALIDATION_SIN_FIX);
    CHECK(!result.usable_for_speed);

    sample = valid_gps_sample(1000000LL);
    sample.satellites = 3U;
    result = gps_validation_evaluate(&sample, &config, 1200000LL);
    CHECK(result.state == GPS_VALIDATION_DEGRADADO);
    CHECK(strcmp(result.reason, "pocos_satellites") == 0);

    sample = valid_gps_sample(1000000LL);
    sample.gga_time_of_day_ms += 500U;
    result = gps_validation_evaluate(&sample, &config, 1200000LL);
    CHECK(result.state == GPS_VALIDATION_DEGRADADO);
    CHECK(strcmp(result.reason, "epocas_rmc_gga_no_coinciden") == 0);

    sample = valid_gps_sample(1000000LL);
    result = gps_validation_evaluate(&sample, &config, 3000001LL);
    CHECK(result.state == GPS_VALIDATION_OBSOLETO);
    CHECK(!result.usable_for_speed);
}

static void test_fusion_missing_and_imu_stale(void)
{
    gps_imu_fusion_reset();
    gps_imu_fusion_result_t result = gps_imu_fusion_evaluate(NAN, false, 0, 1000LL);
    CHECK(strcmp(result.state, "GPS_SIN_DATOS") == 0);
    CHECK(!result.speed_usable);

    result = gps_imu_fusion_evaluate(12.5f, true, 1000000LL, 1000000LL);
    CHECK(strcmp(result.state, "SIN_IMU") == 0);
    CHECK(result.speed_usable && near(result.corrected_speed_kmh, 12.5f));

    gps_imu_fusion_reset();
    gps_imu_fusion_update_acceleration(0.5f, 0.0f, 0.0f, 1000000LL);
    result = gps_imu_fusion_evaluate(10.0f, true, 2000000LL, 2200001LL);
    CHECK(!result.imu_fresh);
    CHECK(strcmp(result.state, "SIN_IMU") == 0);
    CHECK(near(result.corrected_speed_kmh, 10.0f));

    result = gps_imu_fusion_evaluate(NAN, false, 0, 2300000LL);
    CHECK(strcmp(result.state, "GPS_SIN_DATOS") == 0);
    CHECK(!result.speed_usable);

    gps_imu_fusion_reset();
    gps_imu_fusion_update_acceleration(0.5f, 0.0f, 0.0f, 1000000LL);
    result = gps_imu_fusion_evaluate(NAN, false, 0, 1000000LL);
    CHECK(strcmp(result.state, "GPS_SIN_DATOS_IMU_MOVIMIENTO") == 0);
}

static void test_stationary_correction_and_suspicious_speed(void)
{
    gps_imu_fusion_reset();
    gps_imu_fusion_update_acceleration(0.0f, 0.0f, 0.0f, 1000000LL);
    gps_imu_fusion_result_t result = gps_imu_fusion_evaluate(0.8f, true, 1000000LL, 1000000LL);
    CHECK(strcmp(result.state, "CONFIRMANDO_REPOSO") == 0);
    CHECK(near(result.corrected_speed_kmh, 0.8f));

    gps_imu_fusion_update_acceleration(0.0f, 0.0f, 0.0f, 1500000LL);
    gps_imu_fusion_update_acceleration(0.0f, 0.0f, 0.0f, 2000000LL);
    result = gps_imu_fusion_evaluate(1.0f, true, 2000000LL, 2000000LL);
    CHECK(result.stationary_confirmed);
    CHECK(strcmp(result.state, "ESTACIONARIO_IMU") == 0);
    CHECK(result.speed_usable && near(result.corrected_speed_kmh, 0.0f));
    result = gps_imu_fusion_evaluate(0.6f, true, 2000000LL, 2000000LL);
    CHECK(strcmp(result.state, "ESTACIONARIO_IMU") == 0);
    CHECK(near(result.corrected_speed_kmh, 0.0f));

    gps_imu_fusion_update_acceleration(0.0f, 0.0f, 0.0f, 2500000LL);
    gps_imu_fusion_update_acceleration(0.0f, 0.0f, 0.0f, 3000000LL);
    result = gps_imu_fusion_evaluate(3.2f, true, 3000000LL, 3000000LL);
    CHECK(strcmp(result.state, "GPS_SOSPECHOSO_IMU_QUIETA") == 0);
    CHECK(result.speed_usable && near(result.corrected_speed_kmh, 3.2f));

    gps_imu_fusion_reset();
    gps_imu_fusion_update_acceleration(0.0f, 0.0f, 0.0f, 1000000LL);
    gps_imu_fusion_update_acceleration(0.0f, 0.0f, 0.0f, 1500000LL);
    gps_imu_fusion_update_acceleration(0.0f, 0.0f, 0.0f, 2000000LL);
    result = gps_imu_fusion_evaluate(NAN, false, 0, 2000000LL);
    CHECK(strcmp(result.state, "GPS_SIN_DATOS_IMU_QUIETA") == 0);
    CHECK(!result.speed_usable);
}

static void test_low_speed_with_imu_motion(void)
{
    gps_imu_fusion_reset();
    gps_imu_fusion_update_acceleration(0.5f, 0.0f, 0.0f, 1000000LL);
    const gps_imu_fusion_result_t result = gps_imu_fusion_evaluate(0.4f, true, 1000000LL, 1000000LL);
    CHECK(strcmp(result.state, "BAJA_VEL_IMU_MOVIMIENTO") == 0);
    CHECK(result.speed_usable && near(result.corrected_speed_kmh, 0.4f));
}

static void test_motion_states_and_single_gps_sample(void)
{
    gps_imu_fusion_reset();
    gps_imu_fusion_update_acceleration(0.5f, 0.0f, 0.0f, 1000000LL);
    gps_imu_fusion_result_t result = gps_imu_fusion_evaluate(5.0f, true, 1000000LL, 1000000LL);
    CHECK(strcmp(result.state, "INICIO_MOVIMIENTO") == 0);

    gps_imu_fusion_update_acceleration(0.5f, 0.0f, 0.0f, 2000000LL);
    result = gps_imu_fusion_evaluate(7.0f, true, 2000000LL, 2000000LL);
    CHECK(strcmp(result.state, "ACELERANDO") == 0);
    CHECK(near(result.speed_delta_kmh, 2.0f));
    result = gps_imu_fusion_evaluate(7.0f, true, 2000000LL, 2050000LL);
    CHECK(strcmp(result.state, "ACELERANDO") == 0);

    gps_imu_fusion_update_acceleration(0.5f, 0.0f, 0.0f, 3000000LL);
    result = gps_imu_fusion_evaluate(4.0f, true, 3000000LL, 3000000LL);
    CHECK(strcmp(result.state, "FRENANDO") == 0);
    CHECK(near(result.speed_delta_kmh, -3.0f));

    gps_imu_fusion_update_acceleration(0.5f, 0.0f, 0.0f, 4000000LL);
    result = gps_imu_fusion_evaluate(4.5f, true, 4000000LL, 4000000LL);
    CHECK(strcmp(result.state, "MOVIMIENTO_CONSTANTE") == 0);
}

int main(void)
{
    test_gps_quality_states();
    test_fusion_missing_and_imu_stale();
    test_stationary_correction_and_suspicious_speed();
    test_low_speed_with_imu_motion();
    test_motion_states_and_single_gps_sample();
    printf("GPS+IMU tests passed (%u assertions).\n", s_assertions);
    return EXIT_SUCCESS;
}

