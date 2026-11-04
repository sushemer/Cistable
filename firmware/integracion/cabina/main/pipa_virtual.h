#ifndef CI_PIPA_VIRTUAL_H
#define CI_PIPA_VIRTUAL_H
#include "control_cabina.h"
#define CI_DRY_COMPONENTS 5
typedef struct {double mass_kg;ma_vec3 cg_ground_m;} ci_component;
typedef struct {
    double width_i_m,height_i_m,length_i_m,bottom_ground_m,fill_volume_percent;
    double density_kg_m3,gravity_m_s2,track_m;
    ci_component dry[CI_DRY_COMPONENTS]; /* chassis/cabin, shell, platform/pump,
                                        * diesel, occupants/equipment */
    bool override_dry_cg;ma_vec3 dry_cg_ground_m;
    double zeta,mass_scale,cg_shift_m,max_angle_rad;
    ma_quat mounting;bool mounting_confirmed,lever_enabled,rigid_assumed;
    ma_vec3 sensor_r_m;
    uint64_t latency_bound_us;
} ci_virtual_parameters;
typedef struct {
    ma_status status;
    double surface_width_m,equivalent_depth_m,omega_rad_s,frequency_hz,length_m;
    double nominal_mass_fraction,oscillating_ground_m,fixed_ground_m;
    double dry_mass_kg;ma_vec3 dry_cg_ground_m;
    const char *reason;
} ci_virtual_derived;
ci_virtual_parameters ci_virtual_defaults(void);
ma_status ci_virtual_derive(const ci_virtual_parameters *p,cc_config *out,ci_virtual_derived *derived);
ma_quat ci_mounting_zyx(double roll_deg,double pitch_deg,double yaw_deg);
#endif
