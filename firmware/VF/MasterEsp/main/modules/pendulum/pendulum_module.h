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
    float position_y_m;
    float position_z_m;
    float velocity_y_m_s;
    float velocity_z_m_s;
    float acceleration_y_bob_m_s2;
    float acceleration_z_bob_m_s2;
    float moment_roll_Nm;
    float moment_pitch_Nm;
} pendulum_state_t;

typedef struct {
    float bp_m;
    float ap_m;
    float mp_kg;
    float mf_kg;
    float bf_m;
    float omega_n_rad_s;
    bool is_valid;
} trammel_params_t;

/* Estimate inertia from a physical-pendulum period measurement.
 * I_pivot = T^2*m*g*d/(4*pi^2), I_cg = I_pivot - m*d^2. */
bool pendulum_estimate_inertia(const pendulum_measurement_t *measurement,
                               pendulum_inertia_result_t *result);

/* Simulate one physical-pendulum time step using semi-implicit Euler integration. */
bool pendulum_simulate_step(const pendulum_model_t *model,
                            pendulum_state_t *state,
                            float inertia_about_pivot_kg_m2,
                            float dt_s);

/* Simulate tank-fluid motion under vehicle roll and lateral acceleration. */
bool pendulum_simulate_vehicle_step(const pendulum_model_t *model,
                                    pendulum_state_t *state,
                                    float inertia_about_pivot_kg_m2,
                                    float chassis_roll_rad,
                                    float lateral_accel_m_s2,
                                    float dt_s);

/* Horizontal bob acceleration relative to the chassis, in m/s^2. */
bool pendulum_lateral_acceleration(const pendulum_model_t *model,
                                   const pendulum_state_t *state,
                                   float inertia_about_pivot_kg_m2,
                                   float chassis_roll_rad,
                                   float lateral_accel_m_s2,
                                   float *acceleration_m_s2);

/* Small-angle period implied by a model and pivot inertia. */
float pendulum_period_from_inertia(const pendulum_model_t *model,
                                   float inertia_about_pivot_kg_m2);

bool pendulum_compute_trammel_params(float fill_ratio,
                                     float semi_axis_a_m,
                                     float semi_axis_b_m,
                                     float liquid_mass_kg,
                                     trammel_params_t *out);
