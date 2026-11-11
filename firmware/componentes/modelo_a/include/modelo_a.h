#ifndef MODELO_A_H
#define MODELO_A_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* SI throughout. Status zero deliberately means missing, never green. */
typedef enum { MA_MISSING, MA_VALID, MA_STALE, MA_INCOMPLETE,
    MA_SIMULATED, MA_UNAVAILABLE, MA_INVALID } ma_status;
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
ma_status ma_water_at_depth(const ma_geometry *g, double depth_m, double density, ma_water *out);
ma_status ma_depth_at_volume(const ma_geometry *g, double volume_m3, double *out_m);
double ma_simulation_1300_us_gal_m3(void);

typedef enum { MA_CLOCK_UNKNOWN, MA_CLOCK_TANK, MA_CLOCK_CABIN, MA_CLOCK_SIM } ma_clock;
typedef struct { ma_status status; uint64_t time_us; ma_clock clock;
    int quality; /* -1 unknown, otherwise source-specific; not a universal scale */
} ma_stamp;
ma_status ma_freshness(ma_stamp sample, uint64_t now_us, ma_clock clock, uint64_t max_age_us);
typedef struct { double x, y, z; } ma_vec3;
typedef struct { double w, x, y, z; } ma_quat;
typedef enum { IMU_IZQUIERDA, IMU_DERECHA } ma_imu_id;
typedef enum { MA_ACCEL_UNKNOWN, MA_ACCEL_RAW_WITH_GRAVITY, MA_ACCEL_LINEAR, MA_ACCEL_GRAVITY } ma_accel_kind;
typedef struct { ma_stamp stamp; ma_vec3 value; } ma_vector_report;
typedef struct { ma_imu_id id; ma_accel_kind accel_kind;
    ma_vector_report acceleration_mps2, angular_velocity_radps;
    ma_stamp orientation_stamp; ma_quat orientation;
    ma_status mounting_status; ma_quat sensor_to_vehicle;
    /* Vehicle X forward, Y left, Z up. Positions remain unknown. */
} ma_imu_sample;
typedef struct { ma_imu_sample left, right; } ma_two_imus;
/* Only simultaneous, aligned linear acceleration reports are comparable. */
ma_status ma_imu_accel_difference(const ma_two_imus *imus, uint64_t now_us,
    ma_clock clock, uint64_t max_age_us, uint64_t max_skew_us, ma_vec3 *left_minus_right);
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
    aggregate_track_m; bool mass_accounting_complete, track_model_justified; } ma_vehicle;
typedef struct { ma_status status; double signed_value, magnitude; } ma_ltr;
/* No carried-over heuristic: truck LTR unavailable in this delivery. */
ma_ltr ma_truck_ltr(const ma_vehicle *vehicle);
typedef struct { ma_status status; double oscillating_mass_kg, length_m, damping; } ma_pendulum_parameters;
typedef struct { ma_status status; double angle_rad, rate_radps; } ma_pendulum_state;
typedef struct { ma_two_imus imus; ma_water water; double dt_s; } ma_pendulum_input;
typedef struct { ma_status status; double lateral_force_n, roll_moment_nm; } ma_pendulum_output;
ma_pendulum_output ma_pendulum_step(const ma_pendulum_parameters *parameters,
    const ma_pendulum_state *state, const ma_pendulum_input *input);
#endif
