#ifndef MODELO_A_H
#define MODELO_A_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

/* SI throughout. Status zero deliberately means missing, never green. */
typedef enum { MA_MISSING, MA_VALID, MA_STALE, MA_INCOMPLETE,
    MA_SIMULATED, MA_UNAVAILABLE, MA_INVALID, MA_EXPERIMENTAL, MA_OUT_OF_DOMAIN,
    MA_LTR_LIMIT, MA_NUMERICAL_ERROR, MA_NO_SUPPORT } ma_status;
const char *ma_status_name(ma_status status);
typedef enum { MA_MEASURED, MA_REPORTED, MA_ASSUMED, MA_DERIVED, MA_PENDING } ma_origin;
typedef struct { const char *name, *unit, *source; double value;
    ma_origin origin; bool confirmed; } ma_parameter;
extern const ma_parameter ma_model_a_parameters[];
extern const size_t ma_model_a_parameter_count;
typedef struct { double width_m, height_m, length_m, thickness_m, base_m; } ma_tank;
ma_tank ma_model_a_tank(void);
typedef struct { double width_m, height_m, length_m, bottom_m, capacity_m3; } ma_geometry;
typedef struct { ma_status status, cg_status; double depth_m, volume_m3,
    mass_kg, volume_percent, height_percent, cg_ground_m; } ma_water;
ma_status ma_geometry_make(const ma_tank *tank, ma_geometry *out);
ma_status ma_geometry_from_interior(double width_m,double height_m,double length_m,double bottom_ground_m,ma_geometry *out);
ma_status ma_water_at_depth(const ma_geometry *g, double depth_m, double density, ma_water *out);
ma_status ma_depth_at_volume(const ma_geometry *g, double volume_m3, double *out_m);
double ma_simulation_1300_us_gal_m3(void);

typedef enum { MA_CLOCK_UNKNOWN, MA_CLOCK_TANK, MA_CLOCK_CABIN, MA_CLOCK_SIM } ma_clock;
typedef struct { ma_status status; uint64_t time_us; ma_clock clock;
    int quality; /* -1 unknown, otherwise source-specific; not a universal scale */
} ma_stamp;
ma_status ma_freshness(ma_stamp sample, uint64_t now_us, ma_clock clock, uint64_t max_age_us);
/* VALID refers to data validity, not physical validation of the truck model.
 * VALID, EXPERIMENTAL and SIMULATED all require matching known clocks and age. */
typedef struct { double x, y, z; } ma_vec3;
typedef struct { double w, x, y, z; } ma_quat;
typedef enum { IMU_TRASERA, IMU_CABINA } ma_imu_id;
typedef enum { MA_ACCEL_UNKNOWN, MA_ACCEL_RAW_WITH_GRAVITY, MA_ACCEL_LINEAR, MA_ACCEL_GRAVITY } ma_accel_kind;
typedef struct { ma_stamp stamp; ma_vec3 value; } ma_vector_report;
typedef struct { ma_imu_id id; ma_accel_kind accel_kind;
    ma_vector_report acceleration_mps2, angular_velocity_radps;
    ma_stamp orientation_stamp; ma_quat orientation;
    ma_status mounting_status; ma_quat sensor_to_vehicle;
    /* Vehicle X forward, Y left, Z up. Positions remain unknown. */
} ma_imu_sample;
typedef struct { ma_imu_sample rear, cabin; } ma_two_imus;
/* Only simultaneous, aligned linear acceleration reports are comparable. */
ma_status ma_imu_accel_difference(const ma_two_imus *imus, uint64_t now_us,
    ma_clock clock, uint64_t max_age_us, uint64_t max_skew_us, ma_vec3 *rear_minus_cabin);
typedef enum { MA_WATER_RAW_REGISTER, MA_WATER_DEPTH_M, MA_WATER_GAUGE_KPA } ma_water_quantity;
typedef struct { ma_stamp stamp; ma_water_quantity quantity; double value;
    ma_status calibration_status; double sensor_above_bottom_m, offset_m;
} ma_water_input;
typedef struct { uint64_t max_age_us; bool stationary, level, settled;
    bool gauge_pressure_confirmed; } ma_water_policy;
/* Raw register from Twig has no confirmed physical scale: never converted. */
ma_status ma_estimate_water(const ma_geometry *g, const ma_water_input *input,
    const ma_water_policy *policy, uint64_t now_us, ma_clock clock, ma_water *out);

typedef struct { ma_status status; double fixed_mass_kg, fixed_cg_ground_m,
    aggregate_track_m; bool mass_accounting_complete, track_model_justified;
    double roll_moment_nm, total_normal_n; } ma_vehicle;
typedef struct { ma_status status; double signed_value, magnitude, limited_signed; } ma_ltr;
/* Aggregate quasi-static support balance; moment includes ALL masses once. */
ma_ltr ma_truck_ltr(const ma_vehicle *vehicle);
typedef struct { ma_status status;
    double oscillating_mass_kg, semi_y_m, semi_z_m, center_z_m;
    double damping_nm_s, max_angle_rad, max_dt_s, max_step_s;
} ma_pendulum_parameters;
typedef struct { ma_status status; double angle_rad, rate_radps; } ma_pendulum_state;
typedef struct { ma_status status; ma_vec3 acceleration_ref_mps2, omega_radps,
    alpha_radps2, gravity_mps2; double dt_s; } ma_pendulum_input;
typedef struct { ma_status status; double lateral_force_n, roll_moment_nm;
    ma_vec3 force_n, position_m; ma_pendulum_state next; } ma_pendulum_output;
ma_pendulum_output ma_pendulum_step(const ma_pendulum_parameters *parameters,
    const ma_pendulum_state *state, const ma_pendulum_input *input);

ma_status ma_rotate_vector(ma_quat sensor_to_vehicle, ma_vec3 in, ma_vec3 *out);
/* Bounded numerical normalization only: |norm_squared-1| <= 1e-4.
 * Rejects zero, NaN, infinity and arbitrary scale. Not a mounting calibration. */
ma_status ma_quat_normalize_checked(ma_quat input, ma_quat *out);
/* Linear acceleration only, gravity already removed by SH2. r=reference->sensor.
 * Explicit common-rigid-body assumption required. No cabin/rear averaging. */
ma_status ma_correct_position(ma_vec3 linear, ma_vec3 omega, ma_vec3 alpha,
    ma_vec3 r, bool common_rigid_body_assumed, ma_vec3 *reference);
typedef struct { bool initialized; uint32_t session; ma_stamp last;
    ma_vec3 omega, filtered_alpha; } ma_alpha_state;
ma_status ma_alpha_update(ma_alpha_state *state, ma_vector_report gyro, uint32_t session,
    double min_dt_s, double max_dt_s, double filter_tau_s, ma_vec3 *alpha);
/* alpha_update validates source-clock continuity/dt, not wall-clock age:
 * caller must check freshness first (CST2 controller uses report_fresh). */
/* Fixed mass force and moment about reference, in prescribed rotating frame. */
ma_status ma_fixed_mass_force(double mass, ma_vec3 position,
    const ma_pendulum_input *input, ma_vec3 *force, double *roll_moment);
#ifdef __cplusplus
}
#endif
#endif
