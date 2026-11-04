#include "configuracion_modelo.h"
#include "pipa_virtual.h"
#include "sdkconfig.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <inttypes.h>
#ifndef CONFIG_CI_DEMO
static double number(const char *s) {char *end;double v=strtod(s,&end);return end==s||*end?NAN:v;}
#endif
static ci_virtual_parameters parameters;
static ci_virtual_derived derived={.status=MA_MISSING,.reason="sin derivacion virtual en este perfil"};
const char *ci_profile_name(void) {
#if defined(CONFIG_CI_PIPA_VIRTUAL)
    return "PIPA_VIRTUAL_ENTRADAS_REALES";
#elif defined(CONFIG_CI_DEMO)
    return "DEMO_SINTETICA";
#else
    return "PIPA_REAL_INCOMPLETA";
#endif
}
cc_config ci_live_model_config(void) {
    cc_config c=cc_pending_config();
#ifdef CONFIG_CI_PIPA_VIRTUAL
    ci_virtual_parameters p=ci_virtual_defaults();
    p.width_i_m=number(CONFIG_CI_VIRTUAL_WIDTH_M);p.height_i_m=number(CONFIG_CI_VIRTUAL_HEIGHT_M);
    p.length_i_m=number(CONFIG_CI_VIRTUAL_LENGTH_M);p.bottom_ground_m=number(CONFIG_CI_VIRTUAL_BOTTOM_M);
    p.fill_volume_percent=CONFIG_CI_VIRTUAL_FILL_PERCENT;p.track_m=number(CONFIG_CI_VIRTUAL_TRACK_M);
    p.dry[0].mass_kg=number(CONFIG_CI_VIRTUAL_CHASSIS_MASS_KG);p.dry[0].cg_ground_m.z=number(CONFIG_CI_VIRTUAL_CHASSIS_CG_M);
    p.dry[1].mass_kg=number(CONFIG_CI_VIRTUAL_SHELL_MASS_KG);
    p.dry[2].mass_kg=number(CONFIG_CI_VIRTUAL_PLATFORM_MASS_KG);p.dry[2].cg_ground_m.z=number(CONFIG_CI_VIRTUAL_PLATFORM_CG_M);
    p.dry[3].mass_kg=number(CONFIG_CI_VIRTUAL_DIESEL_MASS_KG);p.dry[3].cg_ground_m.z=number(CONFIG_CI_VIRTUAL_DIESEL_CG_M);
    p.dry[4].mass_kg=number(CONFIG_CI_VIRTUAL_CREW_MASS_KG);p.dry[4].cg_ground_m.z=number(CONFIG_CI_VIRTUAL_CREW_CG_M);
    p.dry_cg_ground_m=(ma_vec3){number(CONFIG_CI_VIRTUAL_CG_X_M),number(CONFIG_CI_VIRTUAL_CG_Y_M),number(CONFIG_CI_VIRTUAL_CG_Z_M)};
#ifdef CONFIG_CI_VIRTUAL_OVERRIDE_CG
    p.override_dry_cg=true;
#endif
    p.zeta=number(CONFIG_CI_VIRTUAL_ZETA);p.mass_scale=number(CONFIG_CI_VIRTUAL_MASS_SCALE);
    p.max_angle_rad=CONFIG_CI_VIRTUAL_MAX_ANGLE_MRAD/1000.0;p.latency_bound_us=CONFIG_CI_VIRTUAL_LATENCY_MS*1000ULL;
    p.mounting=ci_mounting_zyx(CONFIG_CI_VIRTUAL_ROLL_DEG,CONFIG_CI_VIRTUAL_PITCH_DEG,CONFIG_CI_VIRTUAL_YAW_DEG);
    p.sensor_r_m=(ma_vec3){CONFIG_CI_VIRTUAL_R_X_MM/1000.0,CONFIG_CI_VIRTUAL_R_Y_MM/1000.0,CONFIG_CI_VIRTUAL_R_Z_MM/1000.0};
#ifdef CONFIG_CI_VIRTUAL_MOUNT_CONFIRMED
    p.mounting_confirmed=true;
#endif
#ifdef CONFIG_CI_VIRTUAL_LEVER
    p.lever_enabled=true;
#endif
#ifdef CONFIG_CI_VIRTUAL_RIGID
    p.rigid_assumed=true;
#endif
    parameters=p;ci_virtual_derive(&p,&c,&derived);
#elif !defined(CONFIG_CI_DEMO)
    ma_geometry_from_interior(number(CONFIG_CI_REAL_WIDTH_M),number(CONFIG_CI_REAL_HEIGHT_M),number(CONFIG_CI_REAL_LENGTH_M),number(CONFIG_CI_REAL_BOTTOM_M),&c.geometry);
    c.reference_ground_m=c.geometry.bottom_m+c.geometry.height_m/2;
    c.dry_mass_kg=number(CONFIG_CI_REAL_DRY_MASS_KG);c.track_m=number(CONFIG_CI_REAL_TRACK_M);
    c.dry_position=(ma_vec3){number(CONFIG_CI_REAL_CG_X_M),number(CONFIG_CI_REAL_CG_Y_M),number(CONFIG_CI_REAL_CG_Z_M)-c.reference_ground_m};
    c.rear_r=(ma_vec3){CONFIG_CI_REAL_R_X_MM/1000.0,CONFIG_CI_REAL_R_Y_MM/1000.0,CONFIG_CI_REAL_R_Z_MM/1000.0};
    c.position_accepted=true; /* vector/reference confirmed, not mounting */
    c.mounting=ci_mounting_zyx(CONFIG_CI_REAL_ROLL_DEG,CONFIG_CI_REAL_PITCH_DEG,CONFIG_CI_REAL_YAW_DEG);
#ifdef CONFIG_CI_REAL_MOUNT_CONFIRMED
    c.mounting_known=true;
#endif
#ifdef CONFIG_CI_REAL_RIGID
    c.rigid_assumed=true;
#endif
#ifndef CONFIG_CI_REAL_LEVER
    c.bypass_position=true; /* forbidden hypothesis for real model */
#endif
#endif
    return c;
}
void ci_config_report(const cc_config *c,void (*emit)(const char *)) {
    char s[272];
#define REPORT(...) do {snprintf(s,sizeof(s),__VA_ARGS__);emit(s);} while(0)
    REPORT("CONFIG|profile=%s|id=%" PRIu64 "|classification=EXPERIMENTAL|configured=%u",ci_profile_name(),c->configuration_id,(unsigned)c->configured);
    REPORT("CONFIG|lever_enabled=%u|r_m=%.9g,%.9g,%.9g|reference=%s|axes=X_forward,Y_left,Z_up",(unsigned)!c->bypass_position,c->rear_r.x,c->rear_r.y,c->rear_r.z,c->virtual_profile?"chosen_car_origin_to_IMU":"tank_geometric_center_to_IMU");
    REPORT("CONFIG|sensor_to_vehicle_wxyz=%.12g,%.12g,%.12g,%.12g|mounting_checked=%u|rigid_assumed=%u",c->mounting.w,c->mounting.x,c->mounting.y,c->mounting.z,(unsigned)c->mounting_known,(unsigned)c->rigid_assumed);
    REPORT("CONFIG|interior_width_height_length_bottom_m=%.12g,%.12g,%.12g,%.12g|reference_ground_m=%.12g",c->geometry.width_m,c->geometry.height_m,c->geometry.length_m,c->geometry.bottom_m,c->reference_ground_m);
    REPORT("CONFIG|dry_mass_kg=%.12g|dry_xyz_origin_m=%.12g,%.12g,%.12g|track_m=%.12g",c->dry_mass_kg,c->dry_position.x,c->dry_position.y,c->dry_position.z,c->track_m);
    REPORT("CONFIG|fixed_mass_kg=%.12g|fixed_xyz_origin_m=%.12g,%.12g,%.12g",c->fixed_water_mass_kg,c->fixed_water_position.x,c->fixed_water_position.y,c->fixed_water_position.z);
    REPORT("CONFIG|pendulum_mass_kg=%.12g|A_B_pivotZ_m=%.12g,%.12g,%.12g|c_Nm_s=%.12g|domain_rad=%.12g",c->pendulum.oscillating_mass_kg,c->pendulum.semi_y_m,c->pendulum.semi_z_m,c->pendulum.center_z_m,c->pendulum.damping_nm_s,c->pendulum.max_angle_rad);
    REPORT("CONFIG|water_source=%s|fill_volume_percent=%.12g|depth_m=%.12g|volume_m3=%.12g|mass_kg=%.12g|cg_ground_m=%.12g",c->virtual_profile?"CONFIGURED_SIMULATED_NO_ACQUISITION_TIME":"REAL_SENSOR_PENDING",c->scenario_water.volume_percent,c->scenario_water.depth_m,c->scenario_water.volume_m3,c->scenario_water.mass_kg,c->scenario_water.cg_ground_m);
    REPORT("CONFIG|g_m_s2=%.12g|latency_budget_assumed_us=%" PRIu64 "|freshness_us=%" PRIu64 "|skew_us=%" PRIu64 "|dtmax_s=%.9g",c->gravity_constant,c->latency_bound_us,c->max_report_age_us,c->max_skew_us,c->pendulum.max_dt_s);
    REPORT("CONFIG|warning_critical_hysteresis=%.9g,%.9g,%.9g|green_is_not_safety_guarantee=1",c->risk_warning,c->risk_critical,c->risk_hysteresis);
    if(c->virtual_profile) {
        REPORT("CONFIG|requested_fill_volume_percent=%.12g|water_derived_status=%s|water_cg_status=%s",parameters.fill_volume_percent,ma_status_name(c->scenario_water.status),ma_status_name(c->scenario_water.cg_status));
        REPORT("CONFIG|surrogate=RECTANGLE_VOLUME_AND_SURFACE_WIDTH|omega_rad_s=%.12g|frequency_hz=%.12g|surface_width_m=%.12g|equivalent_depth_m=%.12g",derived.omega_rad_s,derived.frequency_hz,derived.surface_width_m,derived.equivalent_depth_m);
        REPORT("CONFIG|rho_kg_m3=%.12g|zeta_assumed=%.12g|modal_mass_scale=%.12g|cg_override=%u|derivation_status=%s|reason=%s",parameters.density_kg_m3,parameters.zeta,parameters.mass_scale,(unsigned)parameters.override_dry_cg,ma_status_name(derived.status),derived.reason);
        for(unsigned i=0;i<CI_DRY_COMPONENTS;i++) REPORT("CONFIG|component=%u|mass_kg=%.12g|cg_ground_xyz_m=%.12g,%.12g,%.12g|class=%s",i,parameters.dry[i].mass_kg,parameters.dry[i].cg_ground_m.x,parameters.dry[i].cg_ground_m.y,i==1?c->reference_ground_m:parameters.dry[i].cg_ground_m.z,(i==1||i==3)?"RECOVERED_ESTIMATE":"SCENARIO_ASSUMPTION");
        REPORT("CONFIG|excitation=%s|angular_inputs=RETAINED|source=Dodge2000_Table3.1_p47|not_car_LTR=1|not_validated=1",c->bypass_position?"ALIGNED_LINEAR_IMU_APPLIED_AT_VIRTUAL_ORIGIN":"POSITION_CORRECTED_CAR_ORIGIN_APPLIED_TO_VIRTUAL_ORIGIN");
    }
#undef REPORT
}
