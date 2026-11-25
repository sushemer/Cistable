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
    return pendulum_simulate_vehicle_step(model, state, inertia_about_pivot_kg_m2,
                                          0.0f, 0.0f, dt_s);
}

bool pendulum_simulate_vehicle_step(const pendulum_model_t *model,
                                    pendulum_state_t *state,
                                    float inertia_about_pivot_kg_m2,
                                    float chassis_roll_rad,
                                    float lateral_accel_m_s2,
                                    float dt_s)
{
    if (model == NULL || state == NULL || !valid_positive(model->mass_kg) ||
        !valid_positive(model->pivot_to_cg_m) || !valid_positive(model->gravity_m_s2) ||
        !valid_positive(inertia_about_pivot_kg_m2) || !valid_positive(dt_s) ||
        !isfinite(model->damping_coefficient) || !isfinite(chassis_roll_rad) ||
        !isfinite(lateral_accel_m_s2) || !isfinite(state->angle_rad) ||
        !isfinite(state->angular_rate_rad_s)) return false;

    const float absolute_angle_rad = state->angle_rad + chassis_roll_rad;
    const float angular_acceleration =
        -(model->mass_kg * model->pivot_to_cg_m / inertia_about_pivot_kg_m2) *
            (model->gravity_m_s2 * sinf(absolute_angle_rad) +
             lateral_accel_m_s2 * cosf(absolute_angle_rad)) -
        model->damping_coefficient * state->angular_rate_rad_s;
    state->angular_rate_rad_s += angular_acceleration * dt_s;
    state->angle_rad += state->angular_rate_rad_s * dt_s;
    return isfinite(state->angle_rad) && isfinite(state->angular_rate_rad_s);
}

bool pendulum_lateral_acceleration(const pendulum_model_t *model,
                                   const pendulum_state_t *state,
                                   float inertia_about_pivot_kg_m2,
                                   float chassis_roll_rad,
                                   float lateral_accel_m_s2,
                                   float *acceleration_m_s2)
{
    if (acceleration_m_s2 == NULL) return false;
    *acceleration_m_s2 = 0.0f;
    if (model == NULL || state == NULL || !valid_positive(model->mass_kg) ||
        !valid_positive(model->pivot_to_cg_m) || !valid_positive(model->gravity_m_s2) ||
        !valid_positive(inertia_about_pivot_kg_m2) ||
        !isfinite(model->damping_coefficient) || !isfinite(chassis_roll_rad) ||
        !isfinite(lateral_accel_m_s2) || !isfinite(state->angle_rad) ||
        !isfinite(state->angular_rate_rad_s)) {
        return false;
    }

    const float absolute_angle_rad = state->angle_rad + chassis_roll_rad;
    const float angular_acceleration =
        -(model->mass_kg * model->pivot_to_cg_m / inertia_about_pivot_kg_m2) *
            (model->gravity_m_s2 * sinf(absolute_angle_rad) +
             lateral_accel_m_s2 * cosf(absolute_angle_rad)) -
        model->damping_coefficient * state->angular_rate_rad_s;
    *acceleration_m_s2 = model->pivot_to_cg_m *
        (angular_acceleration * cosf(absolute_angle_rad) -
         state->angular_rate_rad_s * state->angular_rate_rad_s *
             sinf(absolute_angle_rad));
    return isfinite(*acceleration_m_s2);
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

bool pendulum_compute_trammel_params(float fill_ratio,
                                     float semi_axis_a_m,
                                     float semi_axis_b_m,
                                     float liquid_mass_kg,
                                     trammel_params_t *out)
{
    if (out == NULL) return false;
    *out = (trammel_params_t){0};
    if (!isfinite(fill_ratio) || !isfinite(semi_axis_a_m) ||
        !isfinite(semi_axis_b_m) || !isfinite(liquid_mass_kg) ||
        fill_ratio < 0.05f || fill_ratio > 0.95f ||
        semi_axis_a_m <= 0.0f || semi_axis_b_m <= 0.0f ||
        liquid_mass_kg <= 0.0f) {
        return false;
    }

    const float lambda = semi_axis_a_m / semi_axis_b_m;
    if (!isfinite(lambda) || lambda < 0.5f || lambda > 2.5f) return false;
    const float d = fill_ratio;
    const float d2 = d * d;
    const float d3 = d2 * d;
    const float l2 = lambda * lambda;
    const float bp_ratio = 1.089f + 0.726f*d - 0.1379f*lambda
                         - 0.953f*d2 - 1.216f*lambda*d
                         + 0.05141f*l2 - 0.06107f*d3
                         + 0.5739f*lambda*d2 + 0.1632f*l2*d;
    const float mp_ratio = 0.7844f - 1.729f*d + 0.3351f*lambda
                         + 1.156f*d2 + 0.7256f*lambda*d
                         - 0.1254f*l2 - 0.3219f*d3
                         - 0.9152f*lambda*d2 + 0.08043f*l2*d;

    float bp = bp_ratio * semi_axis_b_m;
    const float ap = bp * lambda;
    const float mp = mp_ratio * liquid_mass_kg;
    const float mf = liquid_mass_kg - mp;
    const float bcg_ratio = d;
    const float bcg = bcg_ratio * semi_axis_b_m;
    float bf = 0.0f;
    if (mf > 1e-6f) {
        const float bf_offset = (liquid_mass_kg * (semi_axis_b_m - bcg)
                                 - mf * (semi_axis_b_m - bp))
                              / (mf * semi_axis_b_m);
        bf = semi_axis_b_m - bf_offset * semi_axis_b_m;
    }
    bp = fmaxf(0.0f, bp);

    if (!isfinite(bp) || !isfinite(ap) || !isfinite(mp) ||
        !isfinite(mf) || !isfinite(bf) ||
        bp <= 0.0f || ap <= 0.0f || mp < 0.0f ||
        mp > liquid_mass_kg || mf < 0.0f) {
        return false;
    }
    const float omega_n = sqrtf(fmaxf(9.80665f * bp / (ap * ap), 0.0f));
    if (!isfinite(omega_n) || omega_n <= 0.0f) return false;

    out->bp_m = bp;
    out->ap_m = ap;
    out->mp_kg = mp;
    out->mf_kg = mf;
    out->bf_m = bf;
    out->omega_n_rad_s = omega_n;
    out->is_valid = true;
    return true;
}
