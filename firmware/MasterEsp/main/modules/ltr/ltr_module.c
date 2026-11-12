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
