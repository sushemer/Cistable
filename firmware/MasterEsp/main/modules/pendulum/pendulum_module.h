#pragma once

#include <stdbool.h>

typedef struct {
    float mass_kg;
    float pivot_to_cg_m;
    float gravity_m_s2;
    float period_s;
} pendulum_measurement_t;

typedef struct {
    bool valid;
    float inertia_about_pivot_kg_m2;
    float inertia_about_cg_kg_m2;
    float angular_frequency_rad_s;
} pendulum_inertia_result_t;

typedef struct {
    float mass_kg;
    float pivot_to_cg_m;
    float gravity_m_s2;
    float damping_coefficient;
} pendulum_model_t;

typedef struct {
    float angle_rad;
    float angular_rate_rad_s;
} pendulum_state_t;

/* Estimate inertia from a physical-pendulum period measurement.
 * I_pivot = T^2*m*g*d/(4*pi^2), I_cg = I_pivot - m*d^2. */
bool pendulum_estimate_inertia(const pendulum_measurement_t *measurement,
                               pendulum_inertia_result_t *result);

/* Simulate one physical-pendulum time step using semi-implicit Euler integration. */
bool pendulum_simulate_step(const pendulum_model_t *model,
                            pendulum_state_t *state,
                            float inertia_about_pivot_kg_m2,
                            float dt_s);

/* Small-angle period implied by a model and pivot inertia. */
float pendulum_period_from_inertia(const pendulum_model_t *model,
                                   float inertia_about_pivot_kg_m2);
