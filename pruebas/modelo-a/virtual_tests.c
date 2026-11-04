#include "pipa_virtual.h"
#include <math.h>
#include "demo.h"
#include <stdio.h>
#include <string.h>
#include <float.h>
#define PI 3.14159265358979323846
static unsigned failures;
static void check(bool ok,const char *s) {if(!ok){fprintf(stderr,"FAIL virtual: %s\n",s);failures++;}}
static cc_conditions moving(void) {return (cc_conditions){true,10,false,true};}
static ct_frame frame(uint32_t n) {
    ct_frame f={0};f.session=42;f.sequence=n;f.flags=CT_ACCEL|CT_GYRO|CT_ORIENTATION;
    f.sent_us=f.accel_us=f.gyro_us=f.orientation_us=(uint64_t)n*10000;
    f.accel_sequence=f.gyro_sequence=f.orientation_sequence=n;
    f.accel_quality=f.gyro_quality=f.orientation_quality=3;f.quaternion[0]=1;
    f.water=NAN;return f; /* NO fake Modbus report or water acquisition time */
}
static cc_output step(cc_state *s,cc_config *c,uint32_t n) {
    ct_frame f=frame(n);return cc_process(s,c,&f,f.sent_us,f.sent_us,moving());
}
static double energy(const cc_config *c,ma_pendulum_state s) {
    double m=c->pendulum.oscillating_mass_kg,l=c->pendulum.semi_y_m;
    return .5*m*l*l*s.rate_radps*s.rate_radps+m*9.81*l*(1-cos(s.angle_rad));
}
static ma_pendulum_input input(double ay) {
    return (ma_pendulum_input){MA_EXPERIMENTAL,{0,ay,0},{0,0,0},{0,0,0},{0,0,-9.81},.001};
}
static double run_response(ci_virtual_parameters p) {
    cc_config c;ci_virtual_derived d;check(ci_virtual_derive(&p,&c,&d)==MA_EXPERIMENTAL,"sensitivity config");
    ma_pendulum_state s={MA_EXPERIMENTAL,0,0};double peak=0;
    for(unsigned n=0;n<10000;n++) {
        ma_pendulum_input i=input(.15*sin(n*.001*d.omega_rad_s*.8));
        ma_pendulum_output o=ma_pendulum_step(&c.pendulum,&s,&i);
        check(o.status==MA_EXPERIMENTAL,"sensitivity in domain");s=o.next;
        ma_vec3 dry,fixed;double md,mf;
        check(ma_fixed_mass_force(c.dry_mass_kg,c.dry_position,&i,&dry,&md)==MA_EXPERIMENTAL,"dry sensitivity force");
        check(ma_fixed_mass_force(c.fixed_water_mass_kg,c.fixed_water_position,&i,&fixed,&mf)==MA_EXPERIMENTAL,"fixed sensitivity force");
        double moment=md+mf+o.roll_moment_nm-c.reference_ground_m*(dry.y+fixed.y+o.force_n.y);
        double ltr=2*moment/((-dry.z-fixed.z-o.force_n.z)*c.track_m);
        peak=fmax(peak,fabs(ltr));
    }
    return peak;
}
int main(void) {
    ci_virtual_parameters p=ci_virtual_defaults();cc_config c;ci_virtual_derived d;
    check(ci_virtual_derive(&p,&c,&d)==MA_EXPERIMENTAL,"default derives experimentally");
    check(!c.mounting_known&&!p.lever_enabled&&c.bypass_position,"mount remains pending, independent car lever off");
    check(c.rear_r.x==0&&c.rear_r.y==0&&c.rear_r.z==0,"no inherited truck vector");
    double M=c.scenario_water.mass_kg,mp=c.pendulum.oscillating_mass_kg,mf=c.fixed_water_mass_kg;
    check(fabs(mp+mf-M)<1e-9&&mp>0&&mf>0,"mass partition");
    double cg=(mp*(c.pendulum.center_z_m-c.pendulum.semi_z_m)+mf*c.fixed_water_position.z)/M+c.reference_ground_m;
    check(fabs(cg-c.scenario_water.cg_ground_m)<1e-12,"static centroid conserved");
    check(fabs(c.scenario_water.volume_percent-50)<1e-9&&c.scenario_water.status==MA_SIMULATED,"volume fill configured");
    check(fabs(c.scenario_water.depth_m-p.height_i_m/2)<1e-9,"ellipse volume inversion");
    printf("NUMERIC dry_kg=%.9f dry_cg_ground_z=%.9f capacity_m3=%.9f depth_m=%.9f volume_m3=%.9f water_kg=%.9f water_cg_ground=%.9f\n",d.dry_mass_kg,d.dry_cg_ground_m.z,c.geometry.capacity_m3,c.scenario_water.depth_m,c.scenario_water.volume_m3,M,cg);
    printf("NUMERIC surface_width=%.9f equivalent_depth=%.9f omega=%.9f frequency_hz=%.9f length=%.9f mp=%.9f mf=%.9f rest_ground=%.9f fixed_ground=%.9f pivot_ground=%.9f c_Nms=%.9f\n",d.surface_width_m,d.equivalent_depth_m,d.omega_rad_s,d.frequency_hz,d.length_m,mp,mf,d.oscillating_ground_m,d.fixed_ground_m,c.pendulum.center_z_m+c.reference_ground_m,c.pendulum.damping_nm_s);
    ma_pendulum_state zero={MA_EXPERIMENTAL,0,0};ma_pendulum_input i=input(0);
    ma_pendulum_output equilibrium=ma_pendulum_step(&c.pendulum,&zero,&i);
    check(equilibrium.status==MA_EXPERIMENTAL&&equilibrium.next.angle_rad==0&&equilibrium.next.rate_radps==0,"equilibrium");
    ma_pendulum_state plus=zero,minus=zero;
    for(unsigned n=0;n<1000;n++) {
        i=input(.1);ma_pendulum_output a=ma_pendulum_step(&c.pendulum,&plus,&i);
        i=input(-.1);ma_pendulum_output b=ma_pendulum_step(&c.pendulum,&minus,&i);
        check(a.status==MA_EXPERIMENTAL&&b.status==MA_EXPERIMENTAL,"signed excitation valid");
        plus=a.next;minus=b.next;
        check(fabs(plus.angle_rad+minus.angle_rad)<1e-12&&fabs(a.force_n.y+b.force_n.y)<1e-8,"sign symmetry");
    }
    cc_config undamped=c;undamped.pendulum.damping_nm_s=0;
    ma_pendulum_state free={MA_EXPERIMENTAL,.0001,0};double previous=free.angle_rad,first=-1,second=-1;
    for(unsigned n=1;n<=12000;n++) {
        i=input(0);ma_pendulum_output o=ma_pendulum_step(&undamped.pendulum,&free,&i);check(o.status==MA_EXPERIMENTAL,"free oscillation");free=o.next;
        if(previous>0&&free.angle_rad<=0){double t=(n-1)*.001+.001*previous/(previous-free.angle_rad);if(first<0) first=t;else {second=t;break;}}
        previous=free.angle_rad;
    }
    /* Independent linear analytical period, not the integrator recurrence. */
    double analytical=2*PI/sqrt(9.81*(PI/d.surface_width_m)*tanh(PI*d.equivalent_depth_m/d.surface_width_m));
    check(first>0&&second>first&&fabs((second-first)/analytical-1)<1e-6,"small oscillation vs analytical period");
    printf("PERIOD measured=%.12g analytical=%.12g\n",second-first,analytical);
    free=(ma_pendulum_state){MA_EXPERIMENTAL,.05,0};double initial=energy(&c,free),last=initial;
    for(unsigned n=0;n<10000;n++) {i=input(0);ma_pendulum_output o=ma_pendulum_step(&c.pendulum,&free,&i);free=o.next;double e=energy(&c,free);check(e<=last+1e-9,"damped energy monotone");last=e;}
    check(last<initial*.1,"damping dissipates energy");
    p.mounting_confirmed=true;check(ci_virtual_derive(&p,&c,&d)==MA_EXPERIMENTAL,"confirmed fixture axes");
    cc_state s;cc_init(&s);step(&s,&c,1);check(cc_arm(&s,zero),"explicit arm");
    step(&s,&c,2);step(&s,&c,3);check(s.status==MA_EXPERIMENTAL&&s.armed,"real report path with configured water");
    for(uint32_t n=4;n<=310;n++) step(&s,&c,n);
    check(s.armed&&s.status==MA_EXPERIMENTAL&&s.water_updated_local==0,"scenario persists beyond sensor timeout WITHOUT water timestamps");
    check(s.arm_reference_level,"reference checks aligned gravity at rest");
    /* Bypass must ignore even a large configured car vector, while preserving
     * omega/alpha and the full mounting/gravity transformation. */
    ci_virtual_parameters other=p;other.sensor_r_m=(ma_vec3){8,-7,6};cc_config direct;ci_virtual_derive(&other,&direct,&d);
    cc_state s1,s2;cc_init(&s1);cc_init(&s2);
    for(uint32_t n=1;n<=8;n++) {
        ct_frame real=frame(n);real.gyro[0]=(float)(.001*n);real.acceleration[1]=.01f;
        cc_process(&s1,&c,&real,real.sent_us,real.sent_us,moving());
        cc_process(&s2,&direct,&real,real.sent_us,real.sent_us,moving());
        if(n==1){check(cc_arm(&s1,zero)&&cc_arm(&s2,zero),"direct fixtures arm");}
    }
    check(s1.status==MA_EXPERIMENTAL&&s2.status==MA_EXPERIMENTAL&&fabs(s1.ltr.signed_value-s2.ltr.signed_value)<1e-12,"disabled position correction ignores vector only");
    check(fabs(s1.alpha.filtered_alpha.x)>0,"angular acceleration retained with position bypass");
    ct_frame f=frame(311);f.acceleration[0]=NAN;cc_process(&s,&c,&f,f.sent_us,f.sent_us,moving());check(!s.armed&&s.status==MA_INVALID,"invalid input disarms");
    step(&s,&c,312);check(!s.armed,"no automatic rearm");check(cc_arm(&s,zero),"rearm after invalid");step(&s,&c,313);step(&s,&c,314);
    step(&s,&c,316);check(!s.armed&&s.lost_accel_reports==1,"report loss disarms, loss visible");
    check(cc_arm(&s,zero),"rearm after loss");step(&s,&c,317);step(&s,&c,318);
    cc_current(&s,&c,3300000,3180000,moving());check(!s.armed,"expired communication disarms scenario too");
    step(&s,&c,331);check(cc_arm(&s,zero),"arm before config change");step(&s,&c,332);step(&s,&c,333);
    cc_config next;p.fill_volume_percent=60;ci_virtual_derive(&p,&next,&d);uint64_t oldid=c.configuration_id,before=s.disarm_count;
    check(next.scenario_water.depth_m>c.scenario_water.depth_m&&
        fabs(next.pendulum.semi_y_m-c.pendulum.semi_y_m)>1e-3&&
        next.pendulum.oscillating_mass_kg!=c.pendulum.oscillating_mass_kg&&
        next.pendulum.damping_nm_s!=c.pendulum.damping_nm_s,"fill recalculates depth, modal mass, length and damping");
    cc_apply_config(&s,&c,&next);check(c.configuration_id!=oldid&&!s.armed&&s.last_integrated_us==0&&!s.alpha.initialized&&s.disarm_count==before+1,"fill change invalidates dynamics");
    check(fabs(c.scenario_water.volume_percent-60)<1e-9,"fill dependencies recalculated");
    step(&s,&c,334);check(!s.armed,"changed fill requires explicit rearm");check(cc_arm(&s,zero),"changed scenario rearm");step(&s,&c,335);step(&s,&c,336);check(s.status==MA_EXPERIMENTAL,"two fresh pairs after config change");
    p.fill_volume_percent=0;check(ci_virtual_derive(&p,&next,&d)==MA_OUT_OF_DOMAIN&&!next.configured&&next.scenario_water.mass_kg==0,"empty static only");
    p.fill_volume_percent=100;check(ci_virtual_derive(&p,&next,&d)==MA_OUT_OF_DOMAIN&&!next.configured,"full static only");
    p=ci_virtual_defaults();p.dry[0].mass_kg=NAN;check(ci_virtual_derive(&p,&next,&d)==MA_INVALID,"NaN mass rejected");
    p=ci_virtual_defaults();p.mounting=(ma_quat){0,0,0,0};check(ci_virtual_derive(&p,&next,&d)==MA_INVALID,"invalid quaternion rejected");
    ma_vec3 rotated;check(ma_rotate_vector(ci_mounting_zyx(0,0,90),(ma_vec3){1,0,0},&rotated)==MA_VALID&&fabs(rotated.y-1)<1e-12,"full mounting transform signs");
    p=ci_virtual_defaults();p.mass_scale=10;check(ci_virtual_derive(&p,&next,&d)==MA_INVALID,"negative complementary mass rejected");
    for(unsigned n=20;n<=80;n+=10) {
        p=ci_virtual_defaults();p.fill_volume_percent=n;
        check(ci_virtual_derive(&p,&next,&d)==MA_EXPERIMENTAL,"admissible fill range");
        double modal=next.pendulum.oscillating_mass_kg,fixed=next.fixed_water_mass_kg,total=next.scenario_water.mass_kg;
        check(fabs(modal+fixed-total)<1e-8&&modal>0&&fixed>0,"fill range mass conservation");
        double centroid=(modal*(next.pendulum.center_z_m-next.pendulum.semi_z_m)+fixed*next.fixed_water_position.z)/total+next.reference_ground_m;
        check(fabs(centroid-next.scenario_water.cg_ground_m)<1e-10,"fill range centroid conservation");
    }
    p=ci_virtual_defaults();p.width_i_m*=.9;
    check(ci_virtual_derive(&p,&next,&d)==MA_EXPERIMENTAL&&next.geometry.capacity_m3<c.geometry.capacity_m3,"geometry dependencies recalculated");
    cc_config invalid_geometry=ci_demo_config();invalid_geometry.geometry=(ma_geometry){NAN,NAN,NAN,NAN,NAN};
    cc_state invalid_state;cc_init(&invalid_state);ct_frame synthetic=ci_demo_frame(10000);
    cc_process(&invalid_state,&invalid_geometry,&synthetic,10000,10000,(cc_conditions){true,0,true,true});
    check(!invalid_state.have_water,"explicit invalid geometry cannot use silent legacy fallback");
    p=ci_virtual_defaults();
    for(unsigned n=0;n<3;n++){p.cg_shift_m=(n-1.0)*.2;printf("SENSITIVITY cg_shift_m=%g peak_abs_LTR=%.9f\n",p.cg_shift_m,run_response(p));}
    p=ci_virtual_defaults();for(unsigned n=0;n<3;n++){p.mass_scale=.8+.2*n;printf("SENSITIVITY modal_mass_scale=%g peak_abs_LTR=%.9f\n",p.mass_scale,run_response(p));}
    p=ci_virtual_defaults();for(unsigned n=0;n<3;n++){p.zeta=.05*n;printf("SENSITIVITY zeta=%g peak_abs_LTR=%.9f\n",p.zeta,run_response(p));}
    if(failures){fprintf(stderr,"%u failures\n",failures);return 1;}
    puts("VIRTUAL TESTS PASS (software only, no physical validation)");return 0;
}
