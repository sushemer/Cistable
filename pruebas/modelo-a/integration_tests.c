#include "control_cabina.h"
#include "demo.h"
#include "presentacion.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <float.h>
static void near(double a,double b,double eps) {assert(isfinite(a)&&fabs(a-b)<eps);}
static ma_pendulum_state integrate(ma_pendulum_parameters p,double step,double duration) {
    ma_pendulum_state s={MA_SIMULATED,0.2,0};
    ma_pendulum_input i={MA_SIMULATED,{0,0,0},{0,0,0},{0,0,0},{0,0,-9.81},step};
    p.max_step_s=step;p.max_dt_s=0.1;
    for(unsigned k=0;k<(unsigned)lround(duration/step);k++) {
        ma_pendulum_output o=ma_pendulum_step(&p,&s,&i);assert(o.status==MA_SIMULATED);s=o.next;
    }
    return s;
}
static void review_regressions(void) {
    ma_stamp stamp={MA_EXPERIMENTAL,100,MA_CLOCK_TANK,3};
    assert(ma_freshness(stamp,110,MA_CLOCK_TANK,10)==MA_EXPERIMENTAL);
    assert(ma_freshness(stamp,111,MA_CLOCK_TANK,10)==MA_STALE);
    assert(ma_freshness(stamp,99,MA_CLOCK_TANK,10)==MA_INVALID);
    assert(ma_freshness(stamp,110,MA_CLOCK_CABIN,10)==MA_INCOMPLETE);
    stamp.clock=MA_CLOCK_UNKNOWN;
    assert(ma_freshness(stamp,110,MA_CLOCK_UNKNOWN,10)==MA_INCOMPLETE);
    ma_tank tank=ma_model_a_tank();ma_geometry geometry;ma_geometry_make(&tank,&geometry);
    ma_water_input water={{MA_EXPERIMENTAL,100,MA_CLOCK_TANK,3},MA_WATER_DEPTH_M,0.4,MA_VALID,0,0};
    ma_water_policy policy={10,true,true,true,false};ma_water result;
    assert(ma_estimate_water(&geometry,&water,&policy,110,MA_CLOCK_TANK,&result)==MA_EXPERIMENTAL);
    assert(result.cg_status==MA_EXPERIMENTAL&&isfinite(result.mass_kg));
    assert(ma_estimate_water(&geometry,&water,&policy,111,MA_CLOCK_TANK,&result)==MA_STALE&&isnan(result.mass_kg));
    assert(ma_estimate_water(&geometry,&water,&policy,99,MA_CLOCK_TANK,&result)==MA_INVALID);
    water.stamp.clock=MA_CLOCK_UNKNOWN;
    assert(ma_estimate_water(&geometry,&water,&policy,110,MA_CLOCK_UNKNOWN,&result)==MA_INCOMPLETE);
    ma_two_imus imus={0};imus.rear.id=IMU_TRASERA;imus.cabin.id=IMU_CABINA;
    imus.rear.accel_kind=imus.cabin.accel_kind=MA_ACCEL_LINEAR;
    imus.rear.mounting_status=imus.cabin.mounting_status=MA_VALID;
    imus.rear.sensor_to_vehicle=imus.cabin.sensor_to_vehicle=(ma_quat){1,0,0,0};
    imus.rear.acceleration_mps2=(ma_vector_report){{MA_EXPERIMENTAL,100,MA_CLOCK_TANK,3},{1,2,3}};
    imus.cabin.acceleration_mps2=(ma_vector_report){{MA_VALID,100,MA_CLOCK_TANK,3},{0,0,0}};
    ma_vec3 delta;
    assert(ma_imu_accel_difference(&imus,110,MA_CLOCK_TANK,10,0,&delta)==MA_EXPERIMENTAL);
    near(delta.y,2,1e-12);
    assert(ma_imu_accel_difference(&imus,111,MA_CLOCK_TANK,10,0,&delta)==MA_STALE&&isnan(delta.y));
    assert(ma_imu_accel_difference(&imus,99,MA_CLOCK_TANK,10,0,&delta)==MA_INVALID);
    imus.rear.acceleration_mps2.stamp.clock=MA_CLOCK_UNKNOWN;
    assert(ma_imu_accel_difference(&imus,110,MA_CLOCK_TANK,10,0,&delta)==MA_INCOMPLETE);
    ma_quat normalized;
    assert(ma_quat_normalize_checked((ma_quat){sqrt(1.00005),0,0,0},&normalized)==MA_VALID);
    near(normalized.w,1,1e-15);
    assert(ma_rotate_vector(normalized,(ma_vec3){1,2,3},&delta)==MA_VALID);
    const ma_quat invalid[]={{0,0,0,0},{2,0,0,0},{NAN,0,0,0},{INFINITY,0,0,0}};
    for(unsigned k=0;k<4;k++) assert(ma_quat_normalize_checked(invalid[k],&normalized)==MA_INVALID&&isnan(normalized.w));
    ct_frame bad_quaternion=ci_demo_frame(10000);uint8_t wire[CT_WIRE_SIZE];
    bad_quaternion.quaternion[0]=0;assert(!ct_encode(&bad_quaternion,wire));
    bad_quaternion.quaternion[0]=2;assert(!ct_encode(&bad_quaternion,wire));
    ma_vehicle vehicle={MA_EXPERIMENTAL,1000,1,2,true,true,0,1000};
    const double magnitudes[]={0.999999,1.0,1.000001,0.999999};
    for(int sign=-1;sign<=1;sign+=2) for(unsigned k=0;k<4;k++) {
        vehicle.roll_moment_nm=sign*1000*magnitudes[k];
        ma_ltr ltr=ma_truck_ltr(&vehicle);
        assert(ltr.status==(magnitudes[k]>=1?MA_LTR_LIMIT:MA_EXPERIMENTAL));
        near(ltr.signed_value,sign*magnitudes[k],1e-12);
    }
    vehicle.total_normal_n=0;assert(ma_truck_ltr(&vehicle).status==MA_NO_SUPPORT);
    vehicle.total_normal_n=-1;assert(ma_truck_ltr(&vehicle).status==MA_NO_SUPPORT);
    vehicle.total_normal_n=NAN;assert(ma_truck_ltr(&vehicle).status==MA_INVALID);
    vehicle.total_normal_n=1000;vehicle.roll_moment_nm=DBL_MAX;
    assert(ma_truck_ltr(&vehicle).status==MA_NUMERICAL_ERROR);
    ma_pendulum_parameters p={MA_EXPERIMENTAL,10,0.5,0.3,0,0,1.0,0.05,0.001};
    ma_pendulum_state state={MA_EXPERIMENTAL,1.01,0};
    ma_pendulum_input input={MA_EXPERIMENTAL,{0,0,0},{0,0,0},{0,0,0},{0,0,-9.81},0.01};
    assert(ma_pendulum_step(&p,&state,&input).status==MA_OUT_OF_DOMAIN);
    state.angle_rad=0.99;state.rate_radps=10;
    assert(ma_pendulum_step(&p,&state,&input).status==MA_OUT_OF_DOMAIN);
    state.angle_rad=0;state.rate_radps=1e300;
    assert(ma_pendulum_step(&p,&state,&input).status==MA_NUMERICAL_ERROR);
    state.rate_radps=0;input.dt_s=NAN;
    assert(ma_pendulum_step(&p,&state,&input).status==MA_INVALID);
    input.dt_s=0.01;p.status=MA_MISSING;
    assert(ma_pendulum_step(&p,&state,&input).status==MA_INCOMPLETE);
    /* Force an actual pipeline crossing with entirely synthetic parameters. */
    cc_config c=ci_demo_config();c.track_m=0.1;
    c.mounting=(ma_quat){sqrt(1.00005),0,0,0};
    cc_state engine;cc_init(&engine);cc_conditions stopped={true,0,true,true},moving={true,10,false,true};
    ct_frame f=ci_demo_frame(10000);
    cc_process(&engine,&c,&f,10000,10000,stopped);
    assert(cc_arm(&engine,(ma_pendulum_state){MA_SIMULATED,0,0}));
    f=ci_demo_frame(20000);f.quaternion[0]=(float)sqrt(1.00005);
    cc_output o=cc_process(&engine,&c,&f,20000,20000,stopped);assert(o.mode==CC_LEVEL);
    f=ci_demo_frame(30000);f.acceleration[1]=10;
    o=cc_process(&engine,&c,&f,30000,30000,moving);
    assert(o.status==MA_LTR_LIMIT&&o.mode==CC_LIMIT&&!engine.armed&&o.ltr.magnitude>=1);
    double retained=o.ltr.signed_value;
    estado_sistema_t screen=ci_display_state(o,true);
    assert(screen.modo==MODO_EN_MOVIMIENTO&&screen.riesgo==RIESGO_LIMITE_EXPERIMENTAL);
    f=ci_demo_frame(40000); /* lower acceleration cannot silently clear recorded limit */
    o=cc_process(&engine,&c,&f,40000,40000,moving);assert(o.mode==CC_LIMIT);
    near(o.ltr.signed_value,retained,1e-12);
    assert(cc_current(&engine,&c,140001,40000,moving).status==MA_STALE);
    assert(cc_arm(&engine,(ma_pendulum_state){MA_SIMULATED,0,0}));
    /* Regression for the historical abort: rearm must not retain seq=3,
     * then misinterpret the first new seq=5 as a lost sample and disarm. */
    assert(engine.armed&&!engine.alpha.initialized);
    assert(engine.last_accel_us==0&&engine.last_gyro_us==0&&engine.last_orientation_us==0);
    assert(engine.last_accel_sequence==0&&engine.last_gyro_sequence==0);
    f=ci_demo_frame(50000);o=cc_process(&engine,&c,&f,50000,50000,moving);
    assert(o.status==MA_MISSING&&o.mode==CC_NO_DATA);
    assert(engine.armed); /* waiting for alpha, not another unsolicited disarm */
    f=ci_demo_frame(60000);o=cc_process(&engine,&c,&f,60000,60000,moving);
    assert(o.status==MA_SIMULATED&&o.mode==CC_MOVING&&o.ltr.magnitude<1);
    const ma_status failures[]={MA_OUT_OF_DOMAIN,MA_NUMERICAL_ERROR,MA_NO_SUPPORT,MA_INVALID,MA_MISSING,MA_STALE};
    for(unsigned k=0;k<6;k++) {
        o.status=failures[k];
        assert(ci_display_state(o,true).modo==MODO_SIN_DATOS);
    }
    assert(strcmp(ci_model_notice(MA_OUT_OF_DOMAIN),ci_model_notice(MA_NO_SUPPORT))!=0);
    const ma_status latched_invalid[]={MA_OUT_OF_DOMAIN,MA_NUMERICAL_ERROR,MA_NO_SUPPORT};
    for(unsigned k=0;k<3;k++) {
        engine.status=latched_invalid[k];engine.armed=false;
        f=ci_demo_frame(70000+10000*k);
        o=cc_process(&engine,&c,&f,f.sent_us,f.sent_us,moving);
        assert(o.status==latched_invalid[k]&&ci_display_state(o,true).modo==MODO_SIN_DATOS);
        assert(cc_current(&engine,&c,f.sent_us,f.sent_us,stopped).status==latched_invalid[k]);
    }
    f=ci_demo_frame(100000);c.mounting=(ma_quat){2,0,0,0};
    assert(cc_process(&engine,&c,&f,100000,100000,moving).status==MA_INVALID);
    puts("PASS: limit causes/transitions/latch/rearm/UI, experimental freshness, checked quaternion normalization");
}
int main(void) {
    ma_vec3 r={0.64,-0.42,-0.94},out;
    assert(ma_correct_position((ma_vec3){0,0,0},(ma_vec3){0,0,0},(ma_vec3){0,0,0},r,false,&out)==MA_INCOMPLETE);
    assert(isnan(out.x));
    /* Sensor acceleration solely due to alpha x r, alpha=(0,0,2). */
    assert(ma_correct_position((ma_vec3){0.84,1.28,0},(ma_vec3){0,0,0},(ma_vec3){0,0,2},r,true,&out)==MA_EXPERIMENTAL);
    near(out.x,0,1e-12);near(out.y,0,1e-12);
    assert(ma_correct_position((ma_vec3){-2.56,1.68,0},(ma_vec3){0,0,2},(ma_vec3){0,0,0},r,true,&out)==MA_EXPERIMENTAL);
    near(out.x,0,1e-12);near(out.y,0,1e-12);
    ma_rotate_vector((ma_quat){sqrt(0.5),0,0,sqrt(0.5)},(ma_vec3){1,0,0},&out);
    near(out.x,0,1e-12);near(out.y,1,1e-12);
    ma_alpha_state derivative={0};
    ma_vector_report gyro={{MA_VALID,10000,MA_CLOCK_TANK,3},{0,0,0}};
    assert(ma_alpha_update(&derivative,gyro,1,0.001,0.05,0,&out)==MA_MISSING);
    gyro.stamp.time_us=20000;gyro.value.z=0.02;
    assert(ma_alpha_update(&derivative,gyro,1,0.001,0.05,0,&out)==MA_VALID);near(out.z,2,1e-12);
    assert(ma_alpha_update(&derivative,gyro,1,0.001,0.05,0,&out)==MA_INVALID);
    gyro.stamp.time_us=30000;gyro.value.z=0.04;
    ma_alpha_update(&derivative,gyro,1,0.001,0.05,0.03,&out);near(out.z,2,1e-12);
    gyro.stamp.time_us=200000;
    assert(ma_alpha_update(&derivative,gyro,1,0.001,0.05,0,&out)==MA_STALE);
    assert(ma_alpha_update(&derivative,gyro,2,0.001,0.05,0,&out)==MA_MISSING);
    ma_pendulum_parameters p={MA_SIMULATED,10,0.5,0.3,0,0,1.4,0.05,0.001};
    ma_pendulum_state equilibrium={MA_SIMULATED,0,0};
    ma_pendulum_input i={MA_SIMULATED,{0,0,0},{0,0,0},{0,0,0},{0,0,-9.81},0.01};
    ma_pendulum_output o=ma_pendulum_step(&p,&equilibrium,&i);
    assert(o.status==MA_SIMULATED);near(o.next.angle_rad,0,1e-12);near(o.force_n.z,-98.1,1e-10);
    /* Constant lateral excitation equilibrium tan(theta)=-ay*A/(g*B). */
    i.acceleration_ref_mps2.y=1;
    equilibrium.angle_rad=atan(-p.semi_y_m/(9.81*p.semi_z_m));
    o=ma_pendulum_step(&p,&equilibrium,&i);near(o.next.rate_radps,0,1e-12);
    near(o.force_n.y,-10,1e-10);
    i.dt_s=0;assert(ma_pendulum_step(&p,&equilibrium,&i).status==MA_INVALID);
    i.dt_s=NAN;assert(ma_pendulum_step(&p,&equilibrium,&i).status==MA_INVALID);
    i.dt_s=0.1;assert(ma_pendulum_step(&p,&equilibrium,&i).status==MA_INVALID);
    i.dt_s=0.01;equilibrium.angle_rad=1.5;
    assert(ma_pendulum_step(&p,&equilibrium,&i).status==MA_OUT_OF_DOMAIN);
    /* Numerical convergence and conservative energy for unforced undamped motion. */
    ma_pendulum_state s1=integrate(p,0.04,2),s2=integrate(p,0.02,2),ref=integrate(p,0.001,2);
    double e1=fabs(s1.angle_rad-ref.angle_rad),e2=fabs(s2.angle_rad-ref.angle_rad);
    assert(e2<e1/8);
    double initial=10*9.81*0.3*(1-cos(0.2));
    double metric=0.25*cos(ref.angle_rad)*cos(ref.angle_rad)+0.09*sin(ref.angle_rad)*sin(ref.angle_rad);
    double energy=0.5*10*metric*ref.rate_radps*ref.rate_radps+10*9.81*0.3*(1-cos(ref.angle_rad));
    near(energy,initial,1e-8);
    p.damping_nm_s=2;ma_pendulum_state damp=integrate(p,0.002,5);
    metric=0.25*cos(damp.angle_rad)*cos(damp.angle_rad)+0.09*sin(damp.angle_rad)*sin(damp.angle_rad);
    energy=5*metric*damp.rate_radps*damp.rate_radps+29.43*(1-cos(damp.angle_rad));assert(energy<initial/4);
    ma_vehicle v={MA_SIMULATED,1000,1,2,true,true,9810,9810};
    ma_ltr ltr=ma_truck_ltr(&v);assert(ltr.status==MA_LTR_LIMIT);near(ltr.signed_value,1,1e-12);
    v.roll_moment_nm=-19620;ltr=ma_truck_ltr(&v);near(ltr.signed_value,-2,1e-12);near(ltr.limited_signed,-1,1e-12);
    v.total_normal_n=0;assert(ma_truck_ltr(&v).status==MA_NO_SUPPORT);
    v.mass_accounting_complete=false;assert(ma_truck_ltr(&v).status==MA_INCOMPLETE);
    ma_vec3 force;double moment;
    ma_pendulum_input fixed={MA_EXPERIMENTAL,{0,2,0},{0,0,0},{0,0,0},{0,0,-9.81},0.01};
    assert(ma_fixed_mass_force(100,(ma_vec3){0,0,1},&fixed,&force,&moment)==MA_EXPERIMENTAL);
    near(force.y,-200,1e-12);near(force.z,-981,1e-12);near(moment,200,1e-12);
    /* Canonical packet, not sizeof(struct), and exact byte ordering. */
    ct_frame f=ci_demo_frame(10000),decoded;uint8_t bytes[CT_WIRE_SIZE];
    f.session=0x12345678;assert(ct_encode(&f,bytes));assert(bytes[8]==0x78&&bytes[11]==0x12);
    assert(ct_decode(bytes,sizeof(bytes),&decoded));near(decoded.water,f.water,1e-12);
    assert(!ct_decode(bytes,17,&decoded));bytes[4]=3;assert(!ct_decode(bytes,sizeof(bytes),&decoded));bytes[4]=2;
    f.gyro[1]=NAN;assert(!ct_encode(&f,bytes));f.gyro[1]=0;
    ct_stream stream={0};assert(ct_accept(&stream,&f)==CT_NEW_SESSION);
    assert(ct_accept(&stream,&f)==CT_DUPLICATE);
    ++f.sequence;f.sent_us++;assert(ct_accept(&stream,&f)==CT_ACCEPT);
    f.session=9;assert(ct_accept(&stream,&f)==CT_NEW_SESSION);
    f.session=0x12345678;assert(ct_accept(&stream,&f)==CT_REVERSED);
    uint64_t age;assert(ct_age_us(1000,900,900000,900030,&age));assert(age==130);
    assert(!ct_age_us(1000,1100,0,1,&age));assert(!ct_age_us(1000,900,900001,900000,&age));
    uint8_t ack[CT_ACK_SIZE];uint32_t ack_session,ack_sequence;uint64_t ack_sent;
    ct_ack_encode(123,456,789,ack);
    assert(ct_ack_decode(ack,sizeof(ack),&ack_session,&ack_sequence,&ack_sent));
    assert(ack_session==123&&ack_sequence==456&&ack_sent==789);
    assert(!ct_ack_decode(ack,19,&ack_session,&ack_sequence,&ack_sent));
    uint8_t mb[7]={1,3,2,0x12,0x34,0,0};uint16_t raw=0;uint8_t exception=0;
    uint16_t crc=ct_crc16(mb,5);mb[5]=(uint8_t)crc;mb[6]=(uint8_t)(crc>>8);
    for(unsigned k=0;k<7;k++) assert(ct_modbus_decode(mb,k,1,&raw,&exception)==CT_MB_PARTIAL);
    assert(ct_modbus_decode(mb,7,1,&raw,&exception)==CT_MB_OK&&raw==0x1234);
    mb[6]^=1;assert(ct_modbus_decode(mb,7,1,&raw,&exception)==CT_MB_BAD);
    uint8_t ex[5]={1,0x83,2,0,0};crc=ct_crc16(ex,3);ex[3]=(uint8_t)crc;ex[4]=(uint8_t)(crc>>8);
    assert(ct_modbus_decode(ex,5,1,&raw,&exception)==CT_MB_EXCEPTION&&exception==2);
    /* Full synthetic pipeline: water -> force partition -> dynamics -> display policy. */
    cc_config c=ci_demo_config();cc_state engine;cc_init(&engine);cc_output output={0};
    for(uint64_t t=10000;t<=4000000;t+=10000) {
        ct_frame sample=ci_demo_frame(t);cc_conditions conditions=ci_demo_conditions(t);
        output=cc_process(&engine,&c,&sample,t+100000000,t+100000000,conditions);
        if(t==10000) assert(cc_arm(&engine,(ma_pendulum_state){MA_SIMULATED,0,0}));
        if(t==20000) assert(output.mode==CC_LEVEL);
        if(t>3100000) {assert(output.status==MA_SIMULATED);assert(output.mode==CC_MOVING);}
    }
    assert(isfinite(output.ltr.signed_value));
    ct_frame duplicate=ci_demo_frame(4000000);
    output=cc_process(&engine,&c,&duplicate,104000000,104200000,ci_demo_conditions(4000000));
    assert(output.mode==CC_NO_DATA); /* duplicate cannot renew original sample */
    output=cc_current(&engine,&c,104000000,104000000,(cc_conditions){false,NAN,false,false});assert(output.mode==CC_NO_DATA);
    ct_frame gap=ci_demo_frame(4100000);
    output=cc_process(&engine,&c,&gap,104100000,104100000,ci_demo_conditions(4100000));assert(!engine.armed&&output.mode==CC_NO_DATA);
    gap.session++;gap.sequence=1;
    output=cc_process(&engine,&c,&gap,104100001,104100001,ci_demo_conditions(4100000));
    assert(!engine.armed&&!engine.have_water&&output.mode==CC_NO_DATA);
    cc_config pending=cc_pending_config();cc_init(&engine);f=ci_demo_frame(10000);
    output=cc_process(&engine,&pending,&f,100000,100000,ci_demo_conditions(10000));
    assert(output.mode==CC_NO_DATA&&output.status==MA_INCOMPLETE);
    review_regressions();
    puts("PASS: axes, lever arm, alpha, elliptical dynamics equilibrium/energy/convergence/damping,");
    puts("      signed LTR/forces, v2 serialization, restart/duplicates/gaps, Modbus, full synthetic pipeline, failure-safe UI policy");
    return 0;
}
