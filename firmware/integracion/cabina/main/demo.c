#include "demo.h"
#include <math.h>
cc_config ci_demo_config(void) {
    cc_config c=cc_pending_config();
    c.configured=c.simulation=c.mounting_known=c.rigid_assumed=c.position_accepted=true;
    c.latency_bound_known=c.mass_accounting_complete=c.track_justified=true;
    c.mounting=(ma_quat){1,0,0,0};c.rear_r=(ma_vec3){0,0,0};
    c.latency_bound_us=0;c.alpha_filter_tau_s=0;c.water_hold_us=60000000;
    c.water_calibration_known=true;c.raw_to_m=1;c.sensor_height_m=0;c.water_offset_m=0;
    ma_tank t=ma_model_a_tank();ma_geometry g;ma_water w;
    ma_geometry_make(&t,&g);ma_water_at_depth(&g,g.height_m/2,1000,&w);
    c.dry_mass_kg=2000;c.reference_ground_m=1;c.dry_position=(ma_vec3){0,0,0.3};c.track_m=2;
    c.fixed_water_mass_kg=w.mass_kg/2;c.fixed_water_position=(ma_vec3){0,0,w.cg_ground_m-1};
    c.pendulum=(ma_pendulum_parameters){MA_SIMULATED,w.mass_kg/2,0.3,0.2,
        w.cg_ground_m-1+0.2,20,1.2,0.05,0.002};
    return c;
}
ct_frame ci_demo_frame(uint64_t t) {
    ma_tank tank=ma_model_a_tank();ma_geometry g;ma_geometry_make(&tank,&g);
    ct_frame f={0};f.session=0x44454d4f;f.sequence=(uint32_t)(t/10000);
    f.sent_us=f.accel_us=f.gyro_us=f.orientation_us=f.water_us=t;
    f.accel_sequence=f.gyro_sequence=f.orientation_sequence=f.water_sequence=f.sequence;
    f.quaternion[0]=1;f.flags=15;f.water_quantity=CT_DEPTH_M;f.water=(float)(g.height_m/2);
    f.accel_quality=f.gyro_quality=f.orientation_quality=3;
    if(t>3000000) f.acceleration[1]=(float)(1.5*sin((t-3000000)*1e-6));
    return f;
}
cc_conditions ci_demo_conditions(uint64_t t) {
    return (cc_conditions){true,t<3000000?0:10,t<3000000,true};
}
cc_conditions ci_demo_visual_conditions(uint64_t t) {
    return ci_demo_conditions(t%12000000);
}
ct_frame ci_demo_visual_frame(uint64_t t) {
    ct_frame f=ci_demo_frame(t);uint64_t phase=t%12000000;
    f.acceleration[1]=phase>3000000?(float)(1.5*sin((phase-3000000)*1e-6)):0;
    return f; /* Monotonic source clock/session/sequences; periodic synthetic rest. */
}
