#include "modelo_a.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void near(double a,double b,double eps) { assert(isfinite(a)&&fabs(a-b)<eps); }
int main(void) {
    ma_tank t=ma_model_a_tank(); ma_geometry g; ma_water w;
    assert(ma_geometry_make(&t,&g)==MA_VALID);
    assert(ma_water_at_depth(&g,0,1000,&w)==MA_VALID);
    assert(w.mass_kg==0 && w.cg_status==MA_MISSING && isnan(w.cg_ground_m));
    ma_water_at_depth(&g,g.height_m,1000,&w);
    near(w.volume_m3,g.capacity_m3,1e-12); near(w.cg_ground_m,g.bottom_m+g.height_m/2,1e-12);
    ma_water_at_depth(&g,g.height_m/2,1000,&w); near(w.volume_percent,50,1e-10);
    near(w.cg_ground_m,g.bottom_m+g.height_m/2-2*g.height_m/(3*3.141592653589793),1e-12);
    ma_water_at_depth(&g,g.height_m/4,1000,&w); assert(fabs(w.volume_percent-25)>1);
    double previous=-1;
    for (int n=0;n<=1000;n++) {
        double h=g.height_m*n/1000.0, inverse;
        assert(ma_water_at_depth(&g,h,1000,&w)==MA_VALID);
        assert(w.volume_m3>previous); previous=w.volume_m3;
        assert(ma_depth_at_volume(&g,w.volume_m3,&inverse)==MA_VALID); near(h,inverse,1e-11);
        if (n) assert(w.cg_ground_m>=g.bottom_m-1e-12 && w.cg_ground_m<=g.bottom_m+h+1e-12);
    }
    ma_water_at_depth(&g,g.height_m*1e-10,1000,&w);
    assert(w.volume_m3>0); assert(w.cg_ground_m>=g.bottom_m-1e-12);
    assert(ma_water_at_depth(&g,-1e-12,1000,&w)==MA_INVALID);
    assert(isnan(w.volume_m3));
    assert(ma_water_at_depth(&g,g.height_m+1e-12,1000,&w)==MA_INVALID);
    assert(ma_water_at_depth(&g,NAN,1000,&w)==MA_INVALID);
    assert(ma_water_at_depth(&g,INFINITY,1000,&w)==MA_INVALID);
    assert(ma_water_at_depth(&g,1,-1,&w)==MA_INVALID);
    double h; assert(ma_depth_at_volume(&g,-1,&h)==MA_INVALID && isnan(h));
    assert(ma_depth_at_volume(&g,g.capacity_m3+1,&h)==MA_INVALID);
    ma_tank bad=t; bad.thickness_m=1; ma_geometry invalid;
    assert(ma_geometry_make(&bad,&invalid)==MA_INVALID);
    bad=t; bad.width_m=NAN; assert(ma_geometry_make(&bad,&invalid)==MA_INVALID);
    ma_vehicle vehicle={0}; ma_ltr ltr=ma_truck_ltr(&vehicle);
    assert(ltr.status==MA_INCOMPLETE && isnan(ltr.signed_value) && isnan(ltr.magnitude));
    vehicle=(ma_vehicle){MA_VALID,1,1,1,true,true};
    assert(ma_truck_ltr(&vehicle).status==MA_UNAVAILABLE);
    assert(ma_pendulum_step(NULL,NULL,NULL).status==MA_UNAVAILABLE);
    assert(isnan(ma_pendulum_step(NULL,NULL,NULL).roll_moment_nm));
    ma_water_input i={{MA_VALID,100,MA_CLOCK_TANK,-1},MA_WATER_RAW_REGISTER,42,MA_VALID,0,0};
    ma_water_policy p={50,true,true,true,false};
    assert(ma_estimate_water(&g,&i,&p,110,MA_CLOCK_TANK,&w)==MA_INCOMPLETE);
    i.quantity=MA_WATER_DEPTH_M; i.value=0.5;
    assert(ma_estimate_water(&g,&i,&p,110,MA_CLOCK_TANK,&w)==MA_VALID);
    assert(ma_estimate_water(&g,&i,&p,151,MA_CLOCK_TANK,&w)==MA_STALE && isnan(w.mass_kg));
    assert(ma_estimate_water(&g,&i,&p,110,MA_CLOCK_CABIN,&w)==MA_INCOMPLETE);
    assert(ma_estimate_water(&g,&i,&p,99,MA_CLOCK_TANK,&w)==MA_INVALID);
    p.stationary=false; assert(ma_estimate_water(&g,&i,&p,110,MA_CLOCK_TANK,&w)==MA_INCOMPLETE);
    p.stationary=true; i.quantity=MA_WATER_GAUGE_KPA;
    assert(ma_estimate_water(&g,&i,&p,110,MA_CLOCK_TANK,&w)==MA_INCOMPLETE);
    p.gauge_pressure_confirmed=true; i.value=4.905;
    assert(ma_estimate_water(&g,&i,&p,110,MA_CLOCK_TANK,&w)==MA_VALID); near(w.depth_m,0.5,1e-12);
    i.stamp.status=MA_SIMULATED; assert(ma_estimate_water(&g,&i,&p,110,MA_CLOCK_TANK,&w)==MA_SIMULATED);
    ma_two_imus imus={0}; imus.left.id=IMU_IZQUIERDA; imus.right.id=IMU_DERECHA;
    ma_vec3 d; assert(ma_imu_accel_difference(&imus,110,MA_CLOCK_TANK,50,10,&d)==MA_MISSING);
    imus.left.acceleration_mps2=(ma_vector_report){{MA_VALID,100,MA_CLOCK_TANK,-1},{1,2,3}};
    imus.right.acceleration_mps2=(ma_vector_report){{MA_VALID,105,MA_CLOCK_TANK,-1},{0,1,2}};
    assert(ma_imu_accel_difference(&imus,110,MA_CLOCK_TANK,50,10,&d)==MA_INCOMPLETE);
    imus.left.accel_kind=imus.right.accel_kind=MA_ACCEL_LINEAR;
    imus.left.mounting_status=imus.right.mounting_status=MA_VALID;
    imus.left.sensor_to_vehicle=imus.right.sensor_to_vehicle=(ma_quat){1,0,0,0};
    assert(ma_imu_accel_difference(&imus,110,MA_CLOCK_TANK,50,10,&d)==MA_VALID); near(d.y,1,1e-12);
    assert(ma_imu_accel_difference(&imus,110,MA_CLOCK_TANK,50,1,&d)==MA_INCOMPLETE);
    assert(ma_imu_accel_difference(&imus,200,MA_CLOCK_TANK,50,10,&d)==MA_STALE);
    imus.right.acceleration_mps2.stamp.clock=MA_CLOCK_CABIN;
    assert(ma_imu_accel_difference(&imus,110,MA_CLOCK_TANK,50,10,&d)==MA_INCOMPLETE);
    imus.right.acceleration_mps2.stamp.clock=MA_CLOCK_TANK;
    imus.right.acceleration_mps2.stamp.status=MA_SIMULATED;
    assert(ma_imu_accel_difference(&imus,110,MA_CLOCK_TANK,50,10,&d)==MA_INVALID);
    imus.right.acceleration_mps2.stamp.status=MA_VALID;
    imus.left.sensor_to_vehicle=(ma_quat){0,0,0,1}; /* 180 degrees around Z */
    assert(ma_imu_accel_difference(&imus,110,MA_CLOCK_TANK,50,10,&d)==MA_VALID);
    near(d.x,-1,1e-12); near(d.y,-3,1e-12); near(d.z,1,1e-12);
    imus.left.sensor_to_vehicle=(ma_quat){0,0,0,0};
    assert(ma_imu_accel_difference(&imus,110,MA_CLOCK_TANK,50,10,&d)==MA_INVALID);
    assert(ma_depth_at_volume(&g,ma_simulation_1300_us_gal_m3(),&h)==MA_VALID);
    ma_water_at_depth(&g,h,1000,&w);
    printf("PASS: geometry, inverse (1001 points), centroid, invalid inputs, states, water policy, two IMUs\n");
    printf("Experimental: capacity=%.9f L; 1300 gal US=%.9f L, depth=%.9f m, CG=%.9f m, volume=%.6f%%\n",
        g.capacity_m3*1000,w.volume_m3*1000,h,w.cg_ground_m,w.volume_percent);
    return 0;
}
