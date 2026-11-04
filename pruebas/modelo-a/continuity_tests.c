#include "control_cabina.h"
#include "demo.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <float.h>

static unsigned failures;
static void check(bool condition,const char *name) {
    if(!condition) {fprintf(stderr,"FAIL continuity: %s\n",name);++failures;}
}
static cc_conditions parked(void) {return (cc_conditions){true,0,true,true};}
static cc_conditions moving(void) {return (cc_conditions){true,10,false,true};}
static void setup(cc_state *s,cc_config *c) {
    *c=ci_demo_config();cc_init(s);
    for(uint64_t t=10000;t<=30000;t+=10000) {
        ct_frame f=ci_demo_frame(t);cc_process(s,c,&f,t,t,parked());
        if(t==10000) check(cc_arm(s,(ma_pendulum_state){MA_SIMULATED,0,0}),"fixture arm");
    }
    check(s->armed&&s->status==MA_SIMULATED,"fixture integrates");
}
static cc_output step(cc_state *s,cc_config *c,uint64_t t,cc_conditions conditions) {
    ct_frame f=ci_demo_frame(t);return cc_process(s,c,&f,t,t,conditions);
}
static void interrupted_step(unsigned cause,const char *name) {
    cc_state s;cc_config c;setup(&s,&c);cc_config original=c;
    cc_conditions v=moving();
    switch(cause) {
    case 0:v.no_filling_or_draining_confirmed=false;break;
    case 1:c.fixed_water_mass_kg+=1;break;
    case 2:c.fixed_water_position.z+=0.01;break;
    case 3:c.mounting=(ma_quat){2,0,0,0};break;
    case 4:c.position_accepted=false;break;
    case 5:s.water.cg_ground_m=NAN;break;
    case 6:s.water.status=MA_INVALID;break;
    case 7:s.water.mass_kg=NAN;break;
    case 8:c.pendulum.oscillating_mass_kg=NAN;break;
    case 9:s.water.cg_status=MA_INVALID;break;
    case 10:c.fixed_water_mass_kg=NAN;break;
    case 11:c.fixed_water_mass_kg=-1;break;
    case 12:c.fixed_water_position.z=NAN;break;
    case 13:c.pendulum.status=MA_MISSING;break;
    case 14:c.pendulum.center_z_m=NAN;break;
    case 15:c.dry_mass_kg=NAN;break;
    case 16:c.dry_position.x=NAN;break;
    case 17:c.rear_r.x=NAN;break; /* ma_correct_position must be checked */
    case 18:c.dry_position.y=DBL_MAX;break; /* fixed-force moment overflow */
    case 19:c.reference_ground_m=NAN;break;
    case 20:c.track_m=NAN;break;
    case 21:c.risk_warning=NAN;break;
    }
    ma_pendulum_state before=s.pendulum;
    cc_output o=step(&s,&c,40000,v);
    check(!s.armed,name);
    check(o.status!=MA_SIMULATED&&o.status!=MA_EXPERIMENTAL,"invalid step not a calculated result");
    check(s.pendulum.angle_rad==before.angle_rad&&s.pendulum.rate_radps==before.rate_radps,"invalid step does not advance dynamics");
    check(s.last_integrated_us==30000,"early return cannot advance committed source time");
    if(cause==17) check(o.status==MA_INVALID,"ma_correct_position failure is propagated");
    if(cause==18) check(o.status==MA_NUMERICAL_ERROR,"ma_fixed_mass_force failure is propagated");
    c=original;
    /* Restore data without explicit arm; this must NOT revive old dynamics. */
    step(&s,&c,50000,parked());
    check(!s.armed,"restored prerequisites cannot auto resume");
}
int main(void) {
    const char *causes[]={"no flow interrupted disarms","mass mismatch disarms","centroid mismatch disarms",
        "invalid transform disarms","dynamic config change disarms","NaN centroid rejected",
        "invalid water status rejected","NaN water mass rejected","NaN moving mass rejected",
        "invalid centroid status rejected","NaN fixed mass rejected","negative fixed mass rejected",
        "NaN fixed centroid rejected","missing pendulum parameters rejected","NaN ellipse center rejected",
        "NaN dry mass rejected","NaN dry centroid rejected","invalid position correction rejected",
        "fixed-force overflow rejected","NaN reference height rejected","NaN track rejected","NaN risk threshold rejected"};
    for(unsigned k=0;k<sizeof(causes)/sizeof(causes[0]);k++) interrupted_step(k,causes[k]);
    cc_config c=ci_demo_config();cc_state s;cc_init(&s);
    c.configured=c.position_accepted=c.rigid_assumed=false;c.rear_r=(ma_vec3){NAN,NAN,NAN};
    ct_frame f=ci_demo_frame(10000);f.flags=CT_ORIENTATION|CT_WATER;
    cc_output o=cc_process(&s,&c,&f,10000,10000,parked());
    check(o.mode==CC_LEVEL&&isfinite(o.water_percent),"level independent of LTR/lever/rigidity/gyro/accel");
    c.water_calibration_known=false;f=ci_demo_frame(20000);f.flags=CT_ORIENTATION|CT_WATER;
    o=cc_process(&s,&c,&f,20000,20000,parked());check(o.mode==CC_NO_DATA,"level still requires calibration");
    setup(&s,&c);s.pendulum.angle_rad=0.15;ma_pendulum_state before=s.pendulum;
    f=ci_demo_frame(35000);f.sequence=4;f.accel_sequence=3;f.accel_us=30000;
    f.gyro_sequence=4;f.gyro_us=35000;
    o=cc_process(&s,&c,&f,35000,35000,moving());
    check(s.armed,"one async report may wait");
    check(s.pendulum.angle_rad==before.angle_rad,"waiting report cannot advance state");
    check(o.status==MA_SIMULATED,"waiting keeps only still-current result");
    f=ci_demo_frame(40000);f.sequence=5;f.gyro_sequence=4;f.gyro_us=35000;
    o=cc_process(&s,&c,&f,40000,40000,moving());
    check(o.status==MA_SIMULATED&&s.armed,"async adjacent reports form one bounded pair");
    check(s.last_integrated_us==40000&&s.alpha.last.time_us==35000,"each source time commits only at the completed pair");
    /* Same inputs, integrate the full 30->40 ms interval independently. */
    ma_pendulum_input expected_input={MA_SIMULATED,{0,0,0},{0,0,0},{0,0,0},{0,0,-9.81},0.01};
    ma_pendulum_output expected=ma_pendulum_step(&c.pendulum,&before,&expected_input);
    check(expected.status==MA_SIMULATED&&fabs(s.pendulum.angle_rad-expected.next.angle_rad)<1e-14,
        "waiting does not shorten the integrated interval");
    setup(&s,&c);before=s.pendulum;
    f=ci_demo_frame(35000);f.sequence=4;f.gyro_sequence=3;f.gyro_us=30000;f.accel_sequence=4;
    cc_process(&s,&c,&f,35000,35000,moving());
    check(s.armed&&s.last_integrated_us==30000,"accel-first asynchronous arrival waits without consuming");
    f=ci_demo_frame(40000);f.sequence=5;f.accel_sequence=4;f.accel_us=35000;
    o=cc_process(&s,&c,&f,40000,40000,moving());
    check(s.armed&&o.status==MA_SIMULATED&&s.last_integrated_us==35000,"gyro completes accel-first pair");
    setup(&s,&c);f=ci_demo_frame(40000);f.accel_sequence=f.gyro_sequence=3;f.accel_us=f.gyro_us=30000;
    before=s.pendulum;cc_process(&s,&c,&f,40000,40000,moving());
    check(s.armed&&s.last_integrated_us==30000&&s.pendulum.angle_rad==before.angle_rad,"new packet repeating reports cannot reintegrate");
    cc_process(&s,&c,&f,40000,130001,moving());
    check(!s.armed,"repeated reports cannot extend calculation lifetime");
    setup(&s,&c);f=ci_demo_frame(40000);f.sequence=5; /* lost packet, no source report lost */
    o=cc_process(&s,&c,&f,40000,40000,moving());
    check(s.armed&&o.status==MA_SIMULATED,"packet loss with adjacent source reports admits one covered interval");
    check(s.lost_packets==1&&s.lost_accel_reports==0&&s.lost_gyro_reports==0,"packet loss is recorded separately from source loss");
    setup(&s,&c);
    f=ci_demo_frame(35000);f.sequence=4;f.accel_sequence=3;f.accel_us=30000;f.gyro_sequence=4;
    cc_process(&s,&c,&f,35000,35000,moving());
    f.sequence=5;f.sent_us=f.gyro_us=f.orientation_us=36000;f.gyro_sequence=5;
    cc_process(&s,&c,&f,36000,36000,moving());
    check(!s.armed,"two unmatched gyro reports expose a lost report immediately");
    check(s.lost_gyro_reports==1,"unconsumed source overwrite is recorded");
    setup(&s,&c);f=ci_demo_frame(30000);
    o=cc_process(&s,&c,&f,30000,140001,moving());
    check(!s.armed&&o.status==MA_STALE,"stale duplicate invalidates dynamic continuity");
    setup(&s,&c);f=ci_demo_frame(100000);f.sequence=4;f.accel_sequence=f.gyro_sequence=4;
    o=cc_process(&s,&c,&f,100000,100000,moving());
    check(!s.armed&&o.mode==CC_NO_DATA,"long dt disarms even without sequence gap");
    setup(&s,&c);step(&s,&c,50000,moving());check(!s.armed,"lost complete pair disarms");
    check(s.lost_packets==1&&s.lost_accel_reports==1&&s.lost_gyro_reports==1,"lost complete pair has explicit counters");
    setup(&s,&c);f=ci_demo_frame(40000);f.orientation_us=10000;
    cc_process(&s,&c,&f,40000,40000,moving());check(!s.armed,"excess skew disarms");
    setup(&s,&c);f=ci_demo_frame(31000);f.sequence=f.accel_sequence=f.gyro_sequence=4;
    cc_process(&s,&c,&f,31000,31000,moving());check(!s.armed,"short derivative dt disarms");
    /* Missing/invalid prerequisites and the query-only path cannot preserve arm. */
    for(unsigned k=0;k<8;k++) {
        setup(&s,&c);f=ci_demo_frame(40000);cc_conditions v=moving();
        if(k==0) f.flags&=~CT_ACCEL;
        if(k==1) f.gyro_quality=1;
        if(k==2) {f.accel_sequence=3;f.accel_us=31000;}
        if(k==3) {f.sent_us=45000;f.accel_us=41000;f.accel_sequence=3;}
        if(k==4) f.session++;
        if(k==5) v.speed_valid=false;
        if(k==6) c.alpha_filter_tau_s=NAN;
        if(k==7) {f.sequence=2;}
        before=s.pendulum;o=cc_process(&s,&c,&f,45000,45000,v);
        check(!s.armed&&o.mode==CC_NO_DATA,"early rejected observation disarms");
        check(s.pendulum.angle_rad==before.angle_rad,"rejected observation never commits a partial step");
    }
    setup(&s,&c);o=cc_current(&s,&c,40000,40000,(cc_conditions){false,0,false,true});
    check(!s.armed&&o.mode==CC_NO_DATA,"unknown GPS queried without packet disarms");
    setup(&s,&c);o=cc_current(&s,&c,40000,40000,(cc_conditions){true,10,false,false});
    check(!s.armed&&o.mode==CC_NO_DATA,"flow revoked without packet disarms");
    setup(&s,&c);o=cc_current(&s,&c,130001,130001,moving());
    check(!s.armed&&o.status==MA_STALE,"fresh receiver timestamp cannot mask expired source calculation");
    setup(&s,&c);f=ci_demo_frame(90000);f.accel_sequence=f.gyro_sequence=3;f.accel_us=f.gyro_us=f.orientation_us=30000;
    cc_process(&s,&c,&f,90000,90000,moving());
    check(!s.armed,"waiting cannot exceed maximum dynamic interval even while reports are fresh");
    setup(&s,&c);f=ci_demo_frame(40000);
    cc_process(&s,&c,&f,90000,90000,moving());
    check(!s.armed&&s.last_integrated_us==30000,"late complete pair cannot revive expired dynamics even if source dt is short");
    setup(&s,&c);step(&s,&c,50000,moving());
    check(cc_arm(&s,(ma_pendulum_state){MA_SIMULATED,0,0}),"explicit reinitialization after source gap");
    o=step(&s,&c,60000,moving());check(s.armed&&o.status==MA_MISSING,"reinitialization first new pair primes derivative");
    o=step(&s,&c,70000,moving());check(s.armed&&o.status==MA_SIMULATED,"reinitialization second new pair integrates");
    /* Hydrostatic output keeps its own gates, independent of dynamics. */
    for(unsigned k=0;k<6;k++) {
        c=ci_demo_config();cc_init(&s);c.configured=c.rigid_assumed=c.position_accepted=false;
        f=ci_demo_frame(10000);f.flags=CT_ORIENTATION|CT_WATER;cc_conditions v=parked();
        if(k==0) c.mounting_known=false;
        if(k==1) v.stationary_settled_confirmed=false;
        if(k==2) f.orientation_quality=1;
        if(k==3) {f.quaternion[0]=(float)cos(0.1);f.quaternion[1]=(float)sin(0.1);}
        if(k==4) c.latency_bound_known=false;
        if(k==5) f.flags&=~CT_ORIENTATION;
        o=cc_process(&s,&c,&f,10000,10000,v);check(o.mode==CC_NO_DATA,"level retains mounting/settled/quality/level/freshness gates");
    }
    c=ci_demo_config();cc_init(&s);c.configured=c.rigid_assumed=c.position_accepted=false;
    f=ci_demo_frame(10000);o=cc_process(&s,&c,&f,10000,10000,parked());check(o.mode==CC_LEVEL,"level-only fixture");
    o=cc_current(&s,&c,110001,110001,parked());check(o.mode==CC_NO_DATA,"level expires with orientation even if water retained");
    setup(&s,&c);f=ci_demo_frame(40000);f.water+=0.01f;
    o=cc_process(&s,&c,&f,40000,40000,parked());
    check(!s.armed&&o.mode==CC_LEVEL,"changed fill keeps measured percentage but invalidates fixed dynamic partition");
    check(o.ltr.status==MA_INCOMPLETE,"independent level cannot label retained LTR as calculated");
    fprintf(failures?stderr:stdout,"CONTINUITY: %u failure(s)\n",failures);
    return failures?1:0;
}
