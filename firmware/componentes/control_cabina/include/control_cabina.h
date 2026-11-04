#ifndef CONTROL_CABINA_H
#define CONTROL_CABINA_H
#include "modelo_a.h"
#include "telemetria.h"
typedef enum { CC_NO_DATA, CC_LEVEL, CC_MOVING, CC_LIMIT } cc_display_mode;
typedef struct {
    bool configured, simulation, mounting_known, rigid_assumed, position_accepted;
    bool latency_bound_known, mass_accounting_complete, track_justified;
    ma_quat mounting; ma_vec3 rear_r;
    uint64_t max_report_age_us, max_skew_us, latency_bound_us, water_hold_us;
    double alpha_min_dt_s, alpha_max_dt_s, alpha_filter_tau_s;
    ma_pendulum_parameters pendulum;
    double dry_mass_kg; ma_vec3 dry_position;
    double fixed_water_mass_kg; ma_vec3 fixed_water_position;
    double reference_ground_m, track_m;
    double stopped_kmh, moving_kmh, risk_warning, risk_critical, risk_hysteresis;
    bool water_calibration_known; double raw_to_m, sensor_height_m, water_offset_m;
    double level_tolerance_rad;
    uint8_t min_report_quality;
    /* Zero defaults preserve the real/synthetic legacy paths. Configured water
     * has NO acquisition stamp and is never refreshed by a Modbus flag. */
    bool virtual_profile,bypass_position;
    ma_geometry geometry;
    ma_water scenario_water;
    double gravity_constant;
    uint64_t configuration_id;
} cc_config;
typedef struct {
    ct_stream stream; ma_alpha_state alpha;
    ma_pendulum_state pendulum; ma_water water;
    uint64_t water_updated_local, last_accel_us, last_gyro_us, last_orientation_us, calculation_expires_local;
    /* Source clock of the last committed state (or first priming observation).
     * Never advanced by a rejected/waiting pair. */
    uint64_t last_integrated_us, water_orientation_expires_local;
    uint32_t last_accel_sequence, last_gyro_sequence;
    /* Cumulative observed gaps since cc_init, retained across explicit rearm.
     * Report gaps count overwritten/unavailable reports since committed pair. */
    uint64_t lost_packets, lost_accel_reports, lost_gyro_reports;
    uint64_t disarm_count;
    ma_status last_disarm_status;
    const char *last_disarm_reason; /* static diagnostic text, not a validity flag */
    const char *dynamic_reason;
    bool armed, moving, motion_known, have_water;
    uint64_t configuration_id,arm_reference_expires_local;
    bool arm_reference_level;
    unsigned risk;
    ma_status status; ma_ltr ltr;
} cc_state;
typedef struct { bool speed_valid; double speed_kmh;
    bool stationary_settled_confirmed, no_filling_or_draining_confirmed; } cc_conditions;
typedef struct { ma_status status; cc_display_mode mode; unsigned risk;
    double water_percent; ma_ltr ltr; const char *reason; } cc_output;
cc_config cc_pending_config(void);
void cc_init(cc_state *state);
/* Explicit initial state in current stream session, never automatic tare/zero. */
bool cc_arm(cc_state *state,ma_pendulum_state initial);
/* Operator reports filling/draining: invalidate retained volume and dynamics. */
void cc_invalidate_water(cc_state *state);
/* Use after deriving a changed scenario. Preserves diagnostics and stream,
 * invalidates numerical history; explicit rearm with two new pairs required. */
void cc_apply_config(cc_state *state,cc_config *active,const cc_config *next);
cc_output cc_process(cc_state *state,const cc_config *config,const ct_frame *frame,
    uint64_t received_local_us,uint64_t now_local_us,cc_conditions conditions);
cc_output cc_current(cc_state *state,const cc_config *config,uint64_t now_local_us,
    uint64_t received_local_us,cc_conditions conditions);
#endif
