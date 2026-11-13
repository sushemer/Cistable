#include "pendulum_module.h"

#include <math.h>

#define PENDULUM_PI 3.14159265358979323846f

static bool valid_positive(float value)
{
    return isfinite(value) && value > 0.0f;
}

bool pendulum_estimate_inertia(const pendulum_measurement_t *measurement,
                               pendulum_inertia_result_t *result)
{
    if (result == NULL) return false;
    *result = (pendulum_inertia_result_t){0};
    if (measurement == NULL || !valid_positive(measurement->mass_kg) ||
        !valid_positive(measurement->pivot_to_cg_m) ||
        !valid_positive(measurement->gravity_m_s2) ||
        !valid_positive(measurement->period_s)) return false;

    const float omega = 2.0f * PENDULUM_PI / measurement->period_s;
    const float inertia_pivot = measurement->mass_kg * measurement->gravity_m_s2 *
                                measurement->pivot_to_cg_m / (omega * omega);
    const float inertia_cg = inertia_pivot - measurement->mass_kg *
                             measurement->pivot_to_cg_m * measurement->pivot_to_cg_m;
    if (!isfinite(inertia_pivot) || !isfinite(inertia_cg) || inertia_cg < 0.0f) return false;

    result->valid = true;
    result->inertia_about_pivot_kg_m2 = inertia_pivot;
    result->inertia_about_cg_kg_m2 = inertia_cg;
    result->angular_frequency_rad_s = omega;
    return true;
}

bool pendulum_simulate_step(const pendulum_model_t *model,
                            pendulum_state_t *state,
                            float inertia_about_pivot_kg_m2,
                            float dt_s)
{
    if (model == NULL || state == NULL || !valid_positive(model->mass_kg) ||
        !valid_positive(model->pivot_to_cg_m) || !valid_positive(model->gravity_m_s2) ||
        !valid_positive(inertia_about_pivot_kg_m2) || !valid_positive(dt_s) ||
        !isfinite(model->damping_coefficient) || !isfinite(state->angle_rad) ||
        !isfinite(state->angular_rate_rad_s)) return false;

    const float angular_acceleration =
        -(model->mass_kg * model->gravity_m_s2 * model->pivot_to_cg_m /
          inertia_about_pivot_kg_m2) * sinf(state->angle_rad) -
        model->damping_coefficient * state->angular_rate_rad_s;
    state->angular_rate_rad_s += angular_acceleration * dt_s;
    state->angle_rad += state->angular_rate_rad_s * dt_s;
    return isfinite(state->angle_rad) && isfinite(state->angular_rate_rad_s);
}

float pendulum_period_from_inertia(const pendulum_model_t *model,
                                   float inertia_about_pivot_kg_m2)
{
    if (model == NULL || !valid_positive(model->mass_kg) ||
        !valid_positive(model->pivot_to_cg_m) || !valid_positive(model->gravity_m_s2) ||
        !valid_positive(inertia_about_pivot_kg_m2)) return NAN;
    return 2.0f * PENDULUM_PI * sqrtf(inertia_about_pivot_kg_m2 /
                                       (model->mass_kg * model->gravity_m_s2 *
                                        model->pivot_to_cg_m));
}
