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

// Reinicia el estado de la fusión. Llamar una vez al iniciar el Master.
void gps_imu_fusion_reset(void);

// Actualiza la aceleración lineal con una muestra nueva de la BNO085.
void gps_imu_fusion_update_acceleration(float ax_m_s2, float ay_m_s2,
                                        float az_m_s2, int64_t sample_time_us);

// Combina una medición que ya pasó gps_validation con la última aceleración IMU.
// gps_sample_time_us identifica la época RMC para no comparar la misma lectura
// varias veces mientras sensor_task repite el ciclo.
gps_imu_fusion_result_t gps_imu_fusion_evaluate(float gps_speed_kmh,
                                                bool gps_measurement_valid,
                                                int64_t gps_sample_time_us,
                                                int64_t now_us);

