#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float gps_speed_kmh;
    float corrected_speed_kmh;
    float speed_delta_kmh;
    float acceleration_norm_m_s2;
    uint32_t stationary_ms;
    bool imu_fresh;
    bool gps_measurement_valid;
    bool stationary_confirmed;
    bool speed_usable;
    const char *state;
} gps_imu_fusion_result_t;

// Actualiza la aceleración lineal más reciente de la IMU.
void gps_imu_fusion_update_acceleration(float ax, float ay, float az, int64_t received_us);

// Clasifica la velocidad GPS usando el estado reciente de la IMU.
gps_imu_fusion_result_t gps_imu_fusion_evaluate(float gps_speed_kmh, bool gps_measurement_valid, int64_t now_us);
