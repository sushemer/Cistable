#include "gps_imu_fusion.h"

#include <math.h>
#include <stddef.h>

#define IMU_STATIONARY_ACCEL_THRESHOLD_M_S2 0.20f
#define IMU_DYNAMIC_ACCEL_THRESHOLD_M_S2    0.35f
#define IMU_IMPACT_ACCEL_THRESHOLD_M_S2     2.00f
#define GPS_STATIONARY_SPEED_THRESHOLD_KMH  1.00f
#define GPS_SPEED_CHANGE_THRESHOLD_KMH      1.00f
#define IMU_STATIONARY_CONFIRM_US           1000000LL
#define IMU_DATA_MAX_AGE_US                 1000000LL
#define GPS_PREVIOUS_SAMPLE_MAX_AGE_US      2500000LL

// sensor_task es el único dueño de este módulo; no se comparte entre tareas.
static bool s_has_acceleration;
static float s_acceleration_norm;
static int64_t s_acceleration_sample_us;
static bool s_candidate_stationary;
static int64_t s_candidate_since_us;
static uint32_t s_stationary_ms;
static bool s_have_previous_gps;
static float s_previous_gps_speed_kmh;
static int64_t s_previous_gps_sample_us;
static int64_t s_current_gps_sample_us;
static bool s_current_has_previous_sample;
static bool s_current_has_speed_delta;
static float s_current_speed_delta_kmh;

static uint32_t elapsed_ms(int64_t newer_us, int64_t older_us)
{
    if (older_us < 0 || newer_us < older_us) return 0U;
    const uint64_t elapsed = (uint64_t)(newer_us - older_us) / 1000ULL;
    return elapsed > UINT32_MAX ? UINT32_MAX : (uint32_t)elapsed;
}

void gps_imu_fusion_reset(void)
{
    s_has_acceleration = false;
    s_acceleration_norm = NAN;
    s_acceleration_sample_us = 0;
    s_candidate_stationary = false;
    s_candidate_since_us = 0;
    s_stationary_ms = 0U;
    s_have_previous_gps = false;
    s_previous_gps_speed_kmh = NAN;
    s_previous_gps_sample_us = 0;
    s_current_gps_sample_us = 0;
    s_current_has_previous_sample = false;
    s_current_has_speed_delta = false;
    s_current_speed_delta_kmh = NAN;
}

void gps_imu_fusion_update_acceleration(float ax_m_s2, float ay_m_s2,
                                        float az_m_s2, int64_t sample_time_us)
{
    const float norm = sqrtf(ax_m_s2 * ax_m_s2 + ay_m_s2 * ay_m_s2 + az_m_s2 * az_m_s2);
    if (sample_time_us <= 0 || !isfinite(norm)) {
        s_has_acceleration = false;
        s_acceleration_norm = NAN;
        s_acceleration_sample_us = 0;
        s_candidate_stationary = false;
        s_candidate_since_us = 0;
        s_stationary_ms = 0U;
        return;
    }
    if (sample_time_us == s_acceleration_sample_us) return;

    s_has_acceleration = true;
    s_acceleration_norm = norm;
    s_acceleration_sample_us = sample_time_us;
    if (norm > IMU_STATIONARY_ACCEL_THRESHOLD_M_S2) {
        s_candidate_stationary = false;
        s_candidate_since_us = 0;
        s_stationary_ms = 0U;
    } else if (!s_candidate_stationary || sample_time_us < s_candidate_since_us) {
        s_candidate_stationary = true;
        s_candidate_since_us = sample_time_us;
        s_stationary_ms = 0U;
    } else {
        s_stationary_ms = elapsed_ms(sample_time_us, s_candidate_since_us);
    }
}

gps_imu_fusion_result_t gps_imu_fusion_evaluate(float gps_speed_kmh,
                                                bool gps_measurement_valid,
                                                int64_t gps_sample_time_us,
                                                int64_t now_us)
{
    const bool speed_finite = isfinite(gps_speed_kmh) && gps_speed_kmh >= 0.0f;
    const bool gps_valid = gps_measurement_valid && speed_finite && gps_sample_time_us > 0 &&
                           now_us >= gps_sample_time_us;
    const bool imu_fresh = s_has_acceleration && s_acceleration_sample_us > 0 &&
                           now_us >= s_acceleration_sample_us &&
                           now_us - s_acceleration_sample_us <= IMU_DATA_MAX_AGE_US;
    const bool stationary = imu_fresh && s_candidate_stationary &&
                            s_stationary_ms >= (uint32_t)(IMU_STATIONARY_CONFIRM_US / 1000LL);
    const bool previous_gps_fresh = s_have_previous_gps && gps_valid &&
                                    gps_sample_time_us > s_previous_gps_sample_us &&
                                    gps_sample_time_us - s_previous_gps_sample_us <= GPS_PREVIOUS_SAMPLE_MAX_AGE_US;

    gps_imu_fusion_result_t result = {
        .gps_speed_kmh = gps_valid ? gps_speed_kmh : NAN,
        .corrected_speed_kmh = NAN,
        .speed_delta_kmh = NAN,
        .acceleration_norm_m_s2 = imu_fresh ? s_acceleration_norm : NAN,
        .stationary_ms = imu_fresh ? s_stationary_ms : 0U,
        .imu_fresh = imu_fresh,
        .gps_measurement_valid = gps_valid,
        .stationary_confirmed = stationary,
        .speed_usable = false,
        .state = "GPS_SIN_DATOS",
    };

    if (!gps_valid) {
        s_have_previous_gps = false;
        s_current_gps_sample_us = 0;
        s_current_has_previous_sample = false;
        s_current_has_speed_delta = false;
        s_current_speed_delta_kmh = NAN;
        if (imu_fresh && stationary) result.state = "GPS_SIN_DATOS_IMU_QUIETA";
        else if (imu_fresh && s_acceleration_norm >= IMU_DYNAMIC_ACCEL_THRESHOLD_M_S2)
            result.state = "GPS_SIN_DATOS_IMU_MOVIMIENTO";
        return result;
    }

    if (gps_sample_time_us != s_current_gps_sample_us) {
        s_current_has_previous_sample = previous_gps_fresh;
        if (previous_gps_fresh) {
            s_current_has_speed_delta = true;
            s_current_speed_delta_kmh = gps_speed_kmh - s_previous_gps_speed_kmh;
        } else {
            s_current_has_speed_delta = false;
            s_current_speed_delta_kmh = NAN;
        }
        s_current_gps_sample_us = gps_sample_time_us;
        s_previous_gps_speed_kmh = gps_speed_kmh;
        s_previous_gps_sample_us = gps_sample_time_us;
        s_have_previous_gps = true;
    }
    if (s_current_has_speed_delta) result.speed_delta_kmh = s_current_speed_delta_kmh;

    result.corrected_speed_kmh = gps_speed_kmh;
    result.speed_usable = true;
    if (!imu_fresh) {
        result.state = "SIN_IMU";
        return result;
    }
    if (stationary && gps_speed_kmh <= GPS_STATIONARY_SPEED_THRESHOLD_KMH) {
        result.corrected_speed_kmh = 0.0f;
        result.state = "ESTACIONARIO_IMU";
    } else if (stationary) {
        // Aceleración lineal baja no demuestra que un vehículo esté detenido.
        // Se marca la discrepancia, pero se conserva la velocidad GPS medida.
        result.state = "GPS_SOSPECHOSO_IMU_QUIETA";
    } else if (s_acceleration_norm >= IMU_IMPACT_ACCEL_THRESHOLD_M_S2) {
        result.state = "VIBRACION_IMPACTO";
    } else if (gps_speed_kmh <= GPS_STATIONARY_SPEED_THRESHOLD_KMH &&
               s_acceleration_norm >= IMU_DYNAMIC_ACCEL_THRESHOLD_M_S2) {
        result.state = "BAJA_VEL_IMU_MOVIMIENTO";
    } else if (s_acceleration_norm <= IMU_STATIONARY_ACCEL_THRESHOLD_M_S2) {
        result.state = "CONFIRMANDO_REPOSO";
    } else if (s_acceleration_norm < IMU_DYNAMIC_ACCEL_THRESHOLD_M_S2) {
        result.state = "IMU_TRANSICION";
    } else if (!s_current_has_previous_sample) {
        result.state = "INICIO_MOVIMIENTO";
    } else if (s_current_speed_delta_kmh >= GPS_SPEED_CHANGE_THRESHOLD_KMH) {
        result.state = "ACELERANDO";
    } else if (s_current_speed_delta_kmh <= -GPS_SPEED_CHANGE_THRESHOLD_KMH) {
        result.state = "FRENANDO";
    } else {
        result.state = "MOVIMIENTO_CONSTANTE";
    }
    return result;
}

