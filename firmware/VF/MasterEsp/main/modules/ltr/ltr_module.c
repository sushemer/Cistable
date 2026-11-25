#include "ltr_module.h"

#include <math.h>

#define LTR_DEG_TO_RAD 0.01745329251994329577f

bool ltr_calculate(const ltr_config_t *config,
                   float roll_deg,
                   float lateral_accel_m_s2,
                   ltr_result_t *result)
{
    if (result == NULL) return false;
    *result = (ltr_result_t){0};
    if (config == NULL || !isfinite(config->cg_height_m) || config->cg_height_m <= 0.0f ||
        !isfinite(config->track_width_m) || config->track_width_m <= 0.0f ||
        !isfinite(config->gravity_m_s2) || config->gravity_m_s2 <= 0.0f ||
        !isfinite(config->minimum_value) || !isfinite(config->maximum_value) ||
        config->minimum_value > config->maximum_value ||
        !isfinite(roll_deg) || !isfinite(lateral_accel_m_s2)) return false;

    const float roll_rad = roll_deg * LTR_DEG_TO_RAD;
    const float effective_accel = lateral_accel_m_s2 * cosf(roll_rad) +
                                  config->gravity_m_s2 * sinf(roll_rad);
    const float raw = (2.0f * config->cg_height_m * effective_accel) /
                      (config->track_width_m * config->gravity_m_s2);

    result->valid = true;
    result->roll_deg = roll_deg;
    result->lateral_accel_m_s2 = lateral_accel_m_s2;
    result->effective_lateral_accel_m_s2 = effective_accel;
    result->raw_value = raw;
    result->value = fminf(fmaxf(raw, config->minimum_value), config->maximum_value);
    return true;
}

bool ltr_max_safe_lateral_accel(const ltr_config_t *config,
                               float roll_deg,
                               float ltr_limit,
                               float *max_accel_m_s2)
{
    if (max_accel_m_s2 == NULL) return false;
    *max_accel_m_s2 = 0.0f;
    if (config == NULL || !isfinite(config->cg_height_m) || config->cg_height_m <= 0.0f ||
        !isfinite(config->track_width_m) || config->track_width_m <= 0.0f ||
        !isfinite(config->gravity_m_s2) || config->gravity_m_s2 <= 0.0f ||
        !isfinite(roll_deg) || !isfinite(ltr_limit) || ltr_limit <= 0.0f) {
        return false;
    }

    const float roll_rad = roll_deg * LTR_DEG_TO_RAD;
    const float ltr_scale = 2.0f * config->cg_height_m /
                            (config->track_width_m * config->gravity_m_s2);
    const float static_ltr = ltr_scale * config->gravity_m_s2 * sinf(roll_rad);
    const float lateral_ltr_per_accel = ltr_scale * cosf(roll_rad);
    const float remaining_ltr = ltr_limit - fabsf(static_ltr);
    if (remaining_ltr <= 0.0f) return true;
    if (fabsf(lateral_ltr_per_accel) <= 1e-6f) return false;

    *max_accel_m_s2 = remaining_ltr / fabsf(lateral_ltr_per_accel);
    return isfinite(*max_accel_m_s2);
}

bool ltr_estimate_bank_angle(float vx_mps,
                             float yaw_rate_rps,
                             float ay_mps2,
                             bool speed_valid,
                             float dt_s,
                             bank_estimator_t *state)
{
    if (state == NULL) return false;
    if (!isfinite(vx_mps) || !isfinite(yaw_rate_rps) ||
        !isfinite(ay_mps2) || !isfinite(dt_s) ||
        dt_s < 0.001f || dt_s > 0.020f) {
        state->bank_angle_rad = 0.0f;
        state->low_pass_state_rad = 0.0f;
        return false;
    }

    float target = 0.0f;
    if (speed_valid && vx_mps >= 3.0f) {
        const float argument = fminf(fmaxf(
            (vx_mps * yaw_rate_rps - ay_mps2) / 9.80665f,
            -1.0f), 1.0f);
        target = asinf(argument);
    }

    const float tau_s = 0.5f;
    const float alpha = dt_s / (tau_s + dt_s);
    state->low_pass_state_rad += alpha * (target - state->low_pass_state_rad);
    state->bank_angle_rad = state->low_pass_state_rad;
    return isfinite(state->bank_angle_rad);
}

bool ltr_calculate_dynamic(const ltr_config_t *config,
                           float roll_deg,
                           float lateral_accel_m_s2,
                           float chassis_moment_Nm,
                           float slosh_moment_Nm,
                           float total_mass_kg,
                           ltr_result_t *result)
{
    if (result == NULL) return false;
    *result = (ltr_result_t){0};
    if (config == NULL || !isfinite(chassis_moment_Nm) ||
        !isfinite(slosh_moment_Nm) || !isfinite(total_mass_kg) ||
        total_mass_kg <= 0.0f) {
        return false;
    }
    if (!ltr_calculate(config, roll_deg, lateral_accel_m_s2, result)) {
        return false;
    }
    const float denominator = total_mass_kg * config->gravity_m_s2 *
                              config->track_width_m;
    if (!isfinite(denominator) || denominator <= 0.0f) return false;
    const float moment_ltr = 2.0f * (chassis_moment_Nm + slosh_moment_Nm) /
                             denominator;
    if (!isfinite(moment_ltr)) return false;
    result->raw_value = moment_ltr;
    result->value = fminf(fmaxf(moment_ltr,
                                config->minimum_value),
                          config->maximum_value);
    return true;
}
