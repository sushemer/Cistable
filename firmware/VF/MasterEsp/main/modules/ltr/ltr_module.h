#pragma once

#include <stdbool.h>

typedef struct {
    float cg_height_m;
    float track_width_m;
    float gravity_m_s2;
    float minimum_value;
    float maximum_value;
} ltr_config_t;

typedef struct {
    bool valid;
    float roll_deg;
    float lateral_accel_m_s2;
    float effective_lateral_accel_m_s2;
    float raw_value;
    float value;
} ltr_result_t;

typedef struct {
    float bank_angle_rad;
    float low_pass_state_rad;
} bank_estimator_t;

/*
 * Calculates signed Load Transfer Ratio using the model from the specification:
 *
 *   ay_effective = ay*cos(roll) + g*sin(roll)
 *   LTR          = 2*h*ay_effective/(track*g)
 *
 * ay must be linear acceleration in m/s^2; gravity must not be subtracted again.
 */
bool ltr_calculate(const ltr_config_t *config,
                   float roll_deg,
                   float lateral_accel_m_s2,
                   ltr_result_t *result);

/* Maximum lateral acceleration magnitude before either turn direction reaches |LTR|. */
bool ltr_max_safe_lateral_accel(const ltr_config_t *config,
                               float roll_deg,
                               float ltr_limit,
                               float *max_accel_m_s2);

bool ltr_estimate_bank_angle(float vx_mps,
                             float yaw_rate_rps,
                             float ay_mps2,
                             bool speed_valid,
                             float dt_s,
                             bank_estimator_t *state);

bool ltr_calculate_dynamic(const ltr_config_t *config,
                           float roll_deg,
                           float lateral_accel_m_s2,
                           float chassis_moment_Nm,
                           float slosh_moment_Nm,
                           float total_mass_kg,
                           ltr_result_t *result);
