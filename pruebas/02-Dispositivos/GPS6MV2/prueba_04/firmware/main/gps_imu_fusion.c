#include "gps_imu_fusion.h"

#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

#define IMU_STATIONARY_ACCEL_THRESHOLD_M_S2 0.20f
#define IMU_DYNAMIC_ACCEL_THRESHOLD_M_S2    0.35f
#define IMU_IMPACT_ACCEL_THRESHOLD_M_S2     2.00f
#define GPS_STATIONARY_SPEED_THRESHOLD_KMH  1.00f
#define GPS_SPEED_CHANGE_THRESHOLD_KMH      1.00f
#define IMU_STATIONARY_CONFIRM_MS           1000U
#define IMU_DATA_MAX_AGE_MS                 1000U

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static bool s_has_acceleration;
static float s_acceleration_norm;
static int64_t s_acceleration_received_us;
static bool s_candidate_stationary;
static int64_t s_candidate_since_us;
static uint32_t s_stationary_ms;
static bool s_previous_gps_valid;
static float s_previous_gps_speed_kmh;
static int64_t s_previous_gps_us;

void gps_imu_fusion_update_acceleration(float ax, float ay, float az, int64_t received_us){
    const float norm = sqrtf(ax * ax + ay * ay + az * az);
    const bool candidate = isfinite(norm) && norm <= IMU_STATIONARY_ACCEL_THRESHOLD_M_S2;
    portENTER_CRITICAL(&s_lock);
    s_has_acceleration = isfinite(norm);
    s_acceleration_norm = norm;
    s_acceleration_received_us = received_us;
    if (!candidate) {
        s_candidate_stationary = false;
        s_candidate_since_us = 0;
        s_stationary_ms = 0;
    } else if (!s_candidate_stationary) {
        s_candidate_stationary = true;
        s_candidate_since_us = received_us;
        s_stationary_ms = 0;
    } else if (received_us >= s_candidate_since_us) {
        const uint64_t elapsed_ms = (uint64_t)(received_us - s_candidate_since_us) / 1000ULL;
        s_stationary_ms = elapsed_ms > UINT32_MAX ? UINT32_MAX : (uint32_t)elapsed_ms;
    }
    portEXIT_CRITICAL(&s_lock);
}

gps_imu_fusion_result_t gps_imu_fusion_evaluate(float gps_speed_kmh, bool gps_measurement_valid, int64_t now_us){
    bool has_acceleration;
    float acceleration_norm;
    int64_t acceleration_received_us;
    uint32_t stationary_ms;
    portENTER_CRITICAL(&s_lock);
    has_acceleration = s_has_acceleration;
    acceleration_norm = s_acceleration_norm;
    acceleration_received_us = s_acceleration_received_us;
    stationary_ms = s_stationary_ms;
    portEXIT_CRITICAL(&s_lock);

    const bool imu_fresh = has_acceleration && acceleration_received_us > 0 && now_us >= acceleration_received_us &&
                           (uint64_t)(now_us - acceleration_received_us) / 1000ULL <= IMU_DATA_MAX_AGE_MS;
    const bool stationary = imu_fresh && stationary_ms >= IMU_STATIONARY_CONFIRM_MS;
    const bool speed_finite = isfinite(gps_speed_kmh) && gps_speed_kmh >= 0.0f;
    const bool previous_fresh = s_previous_gps_valid && s_previous_gps_us > 0 && now_us >= s_previous_gps_us &&
                                (uint64_t)(now_us - s_previous_gps_us) / 1000ULL <= 2500U;
    const float speed_delta = previous_fresh && speed_finite ? gps_speed_kmh - s_previous_gps_speed_kmh : NAN;
    gps_imu_fusion_result_t result = {
        .gps_speed_kmh = speed_finite ? gps_speed_kmh : NAN,
        .corrected_speed_kmh = NAN,
        .speed_delta_kmh = speed_delta,
        .acceleration_norm_m_s2 = imu_fresh ? acceleration_norm : NAN,
        .stationary_ms = stationary_ms,
        .imu_fresh = imu_fresh,
        .gps_measurement_valid = gps_measurement_valid && speed_finite,
        .stationary_confirmed = stationary,
        .speed_usable = false,
        .state = "GPS_SIN_DATOS",
    };

    if (!result.gps_measurement_valid) {
        if (imu_fresh && stationary) result.state = "GPS_SIN_DATOS_IMU_QUIETA";
        else if (imu_fresh && acceleration_norm >= IMU_DYNAMIC_ACCEL_THRESHOLD_M_S2) result.state = "GPS_SIN_DATOS_IMU_MOVIMIENTO";
        s_previous_gps_valid = false;
        return result;
    }
    if (!imu_fresh) {
        result.corrected_speed_kmh = gps_speed_kmh;
        result.state = "SIN_IMU";
        s_previous_gps_valid = true;
        s_previous_gps_speed_kmh = gps_speed_kmh;
        s_previous_gps_us = now_us;
        return result;
    }

    // La aceleración no mide por sí sola una velocidad constante; las velocidades altas se conservan.
    if (stationary && gps_speed_kmh <= GPS_STATIONARY_SPEED_THRESHOLD_KMH) {
        result.corrected_speed_kmh = 0.0f;
        result.speed_usable = true;
        result.state = "ESTACIONARIO_IMU";
    } else if (stationary && gps_speed_kmh > GPS_STATIONARY_SPEED_THRESHOLD_KMH &&
               (!previous_fresh || s_previous_gps_speed_kmh <= GPS_STATIONARY_SPEED_THRESHOLD_KMH)) {
        result.corrected_speed_kmh = 0.0f;
        result.speed_usable = true;
        result.state = "GPS_SOSPECHOSO_IMU_QUIETA";
    } else if (acceleration_norm >= IMU_IMPACT_ACCEL_THRESHOLD_M_S2) {
        result.corrected_speed_kmh = gps_speed_kmh;
        result.speed_usable = true;
        result.state = "VIBRACION_IMPACTO";
    } else if (gps_speed_kmh <= GPS_STATIONARY_SPEED_THRESHOLD_KMH &&
               acceleration_norm >= IMU_DYNAMIC_ACCEL_THRESHOLD_M_S2) {
        result.corrected_speed_kmh = gps_speed_kmh;
        result.speed_usable = true;
        result.state = "BAJA_VEL_IMU_MOVIMIENTO";
    } else if (!previous_fresh || s_previous_gps_speed_kmh <= GPS_STATIONARY_SPEED_THRESHOLD_KMH) {
        result.corrected_speed_kmh = gps_speed_kmh;
        result.speed_usable = true;
        result.state = "INICIO_MOVIMIENTO";
    } else if (isfinite(speed_delta) && speed_delta >= GPS_SPEED_CHANGE_THRESHOLD_KMH) {
        result.corrected_speed_kmh = gps_speed_kmh;
        result.speed_usable = true;
        result.state = "ACELERANDO";
    } else if (isfinite(speed_delta) && speed_delta <= -GPS_SPEED_CHANGE_THRESHOLD_KMH) {
        result.corrected_speed_kmh = gps_speed_kmh;
        result.speed_usable = true;
        result.state = "FRENANDO";
    } else {
        result.corrected_speed_kmh = gps_speed_kmh;
        result.speed_usable = true;
        result.state = "MOVIMIENTO_CONSTANTE";
    }
    s_previous_gps_valid = true;
    s_previous_gps_speed_kmh = gps_speed_kmh;
    s_previous_gps_us = now_us;
    return result;
}
