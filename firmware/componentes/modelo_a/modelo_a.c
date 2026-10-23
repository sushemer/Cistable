#include "modelo_a.h"
#include <math.h>
#include <float.h>

#define PI 3.14159265358979323846
#define P(n,u,v,o,s) {n,u,s,v,o,false}
const ma_parameter ma_model_a_parameters[] = {
    P("width","m",2.20,MA_MEASURED,"notas: exterior provisional"),
    P("height","m",1.24,MA_MEASURED,"notas: exterior provisional"),
    P("length","m",3.04,MA_MEASURED,"notas: exterior provisional"),
    P("thickness","m",0.0047625,MA_REPORTED,"notas: 3/16 inch; verificar espesor"),
    P("base","m",1.16,MA_MEASURED,"notas: base exterior"),
    P("center_noted","m",1.73,MA_MEASURED,"discrepa con 1.78 derivado"),
    P("wheelbase","m",3.92,MA_MEASURED,"centros; placa dice 5.30"),
    P("front_spacing","m",2.01,MA_MEASURED,"referencia transversal pendiente"),
    P("rear_spacing","m",1.81,MA_MEASURED,"referencia transversal pendiente"),
    P("baffles","count",2,MA_REPORTED,"transversales; zonas comunicadas"),
    P("baffle_height_fraction","1",0.8,MA_REPORTED,"aproximada; aberturas pendientes"),
    P("capacity_low","gal",1200,MA_REPORTED,"capitan; tipo de galon no confirmado"),
    P("capacity_high","gal",1500,MA_REPORTED,"no es capacidad nominal exacta"),
    P("water_density","kg/m3",1000,MA_ASSUMED,"constante de trabajo"),
    P("gravity","m/s2",9.81,MA_ASSUMED,"constante de trabajo"),
    P("diesel_volume","m3",0.209,MA_ASSUMED,"escenario combustible"),
    P("diesel_density","kg/m3",832,MA_ASSUMED,"provisional"),
    P("diesel_mass","kg",173.888,MA_DERIVED,"209 L * 0.832 kg/L"),
    P("gvwr","kg",15876,MA_REPORTED,"placa; NO masa vacia ni garantia"),
    P("tank_mass_estimate","kg",924,MA_DERIVED,"aprox: acero 7850 kg/m3; pared, 2 extremos planos, 2 rompeolas 80%; sin orificios ni accesorios"),
    P("chassis_cabin_mass","kg",NAN,MA_PENDING,"medir"),
    P("platform_accessories_mass","kg",NAN,MA_PENDING,"medir"),
    P("occupants_equipment_mass","kg",NAN,MA_PENDING,"medir"),
    P("component_centers_of_gravity","m",NAN,MA_PENDING,"medir todos; no duplicar combustible"),
    P("aggregate_track","m",NAN,MA_PENDING,"justificar modelo por ejes"),
    P("imu_positions","m",NAN,MA_PENDING,"montaje simetrico propuesto cerca de 1.52 m longitudinal; medir"),
    P("pendulum_parameters","SI",NAN,MA_PENDING,"identificacion futura")
};
const size_t ma_model_a_parameter_count = sizeof(ma_model_a_parameters)/sizeof(ma_model_a_parameters[0]);

const char *ma_status_name(ma_status s) {
    switch(s) {
    case MA_MISSING: return "datos ausentes";
    case MA_VALID: return "datos validos";
    case MA_STALE: return "datos antiguos";
    case MA_INCOMPLETE: return "configuracion incompleta";
    case MA_SIMULATED: return "simulado/experimental";
    case MA_UNAVAILABLE: return "modelo no disponible";
    default: return "entrada invalida";
    }
}
ma_tank ma_model_a_tank(void) { return (ma_tank){2.20,1.24,3.04,0.0047625,1.16}; }
double ma_simulation_1300_us_gal_m3(void) { return 1300.0*3.785411784/1000.0; }
static bool positive(double x) { return isfinite(x) && x>0; }
static bool geometry_ok(const ma_geometry *g) {
    if (!g || !positive(g->width_m) || !positive(g->height_m) ||
        !positive(g->length_m) || !positive(g->capacity_m3) ||
        !isfinite(g->bottom_m) || g->bottom_m<0) return false;
    double expected=PI*g->width_m*g->height_m*g->length_m/4;
    return positive(expected) && fabs(expected-g->capacity_m3)<=32*DBL_EPSILON*expected;
}
ma_status ma_geometry_make(const ma_tank *t, ma_geometry *o) {
    if (!o) return MA_INVALID;
    *o=(ma_geometry){NAN,NAN,NAN,NAN,NAN};
    if (!t || !positive(t->width_m) || !positive(t->height_m) ||
        !positive(t->length_m) || !positive(t->thickness_m) ||
        !isfinite(t->base_m) || t->base_m<0) return MA_INVALID;
    ma_geometry g={t->width_m-2*t->thickness_m,t->height_m-2*t->thickness_m,
        t->length_m-2*t->thickness_m,t->base_m+t->thickness_m,0};
    g.capacity_m3=PI*g.width_m*g.height_m*g.length_m/4;
    if (!geometry_ok(&g)) return MA_INVALID;
    *o=g; return MA_VALID;
}
static ma_water no_water(ma_status s) {
    return (ma_water){s,s,NAN,NAN,NAN,NAN,NAN,NAN};
}
/* Integrating elliptical strips: A=integral 2a sqrt(1-z^2/b^2) dz.
 * Near empty, theta-sin(theta)cos(theta) needs a series to avoid cancellation. */
static double segment(double theta) {
    if (theta<0.01) {
        double q=theta*theta;
        return theta*q*(2.0/3-q*(2.0/15-q*(4.0/315-q*2.0/2835)));
    }
    return theta-sin(theta)*cos(theta);
}
ma_status ma_water_at_depth(const ma_geometry *g, double h, double rho, ma_water *o) {
    if (!o) return MA_INVALID;
    *o=no_water(MA_INVALID);
    if (!geometry_ok(g) || !positive(rho) || !isfinite(h) || h<0 || h>g->height_m) return MA_INVALID;
    double a=g->width_m/2, b=g->height_m/2;
    bool lower=h<=b;
    double edge=(lower?h:g->height_m-h)/b;
    double theta=2*asin(sqrt(edge/2));
    double area_small=a*b*segment(theta);
    double area=lower?area_small:PI*a*b-area_small;
    double v=area*g->length_m, mass=v*rho;
    if (!isfinite(mass)) return MA_INVALID;
    *o=(ma_water){MA_VALID,MA_MISSING,h,v,mass,100*v/g->capacity_m3,100*h/g->height_m,NAN};
    if (h>0 && area>0) {
        double s=sin(theta);
        double relative=-(2*a*b*b/3)*s*s*s/area;
        o->cg_ground_m=g->bottom_m+b+relative;
        o->cg_status=MA_VALID;
    }
    return MA_VALID;
}
ma_status ma_depth_at_volume(const ma_geometry *g, double v, double *out) {
    if (!out) return MA_INVALID;
    *out=NAN;
    if (!geometry_ok(g) || !isfinite(v) || v<0 || v>g->capacity_m3) return MA_INVALID;
    if (v==0 || v==g->capacity_m3) { *out=v==0?0:g->height_m; return MA_VALID; }
    double lo=0, hi=g->height_m;
    for (int i=0;i<80;i++) {
        double mid=(lo+hi)/2; ma_water w;
        ma_water_at_depth(g,mid,1000,&w);
        if (w.volume_m3<v) lo=mid; else hi=mid;
    }
    *out=(lo+hi)/2; return MA_VALID;
}
ma_status ma_freshness(ma_stamp s, uint64_t now, ma_clock clock, uint64_t age) {
    if (s.status!=MA_VALID && s.status!=MA_SIMULATED) return s.status;
    if (s.clock==MA_CLOCK_UNKNOWN || s.clock!=clock) return MA_INCOMPLETE;
    if (now<s.time_us) return MA_INVALID;
    return now-s.time_us>age?MA_STALE:s.status;
}
static bool vec_ok(ma_vec3 v) { return isfinite(v.x)&&isfinite(v.y)&&isfinite(v.z); }
static bool quat_ok(ma_quat q) {
    double n=q.w*q.w+q.x*q.x+q.y*q.y+q.z*q.z;
    return isfinite(n) && fabs(n-1)<1e-6;
}
static ma_vec3 rotate(ma_quat q, ma_vec3 v) {
    ma_vec3 t={2*(q.y*v.z-q.z*v.y),2*(q.z*v.x-q.x*v.z),2*(q.x*v.y-q.y*v.x)};
    return (ma_vec3){v.x+q.w*t.x+q.y*t.z-q.z*t.y,
        v.y+q.w*t.y+q.z*t.x-q.x*t.z,v.z+q.w*t.z+q.x*t.y-q.y*t.x};
}
ma_status ma_imu_accel_difference(const ma_two_imus *p, uint64_t now, ma_clock clock,
    uint64_t age, uint64_t skew, ma_vec3 *o) {
    if (!o) return MA_INVALID;
    *o=(ma_vec3){NAN,NAN,NAN};
    if (!p || p->left.id!=IMU_IZQUIERDA || p->right.id!=IMU_DERECHA) return MA_INVALID;
    const ma_imu_sample *l=&p->left,*r=&p->right;
    ma_status ls=ma_freshness(l->acceleration_mps2.stamp,now,clock,age);
    ma_status rs=ma_freshness(r->acceleration_mps2.stamp,now,clock,age);
    if (ls!=MA_VALID && ls!=MA_SIMULATED) return ls;
    if (rs!=MA_VALID && rs!=MA_SIMULATED) return rs;
    if (ls!=rs) return MA_INVALID; /* no mixed simulation/real comparison */
    if (l->mounting_status!=MA_VALID || r->mounting_status!=MA_VALID ||
        l->accel_kind!=MA_ACCEL_LINEAR || r->accel_kind!=MA_ACCEL_LINEAR) return MA_INCOMPLETE;
    uint64_t lt=l->acceleration_mps2.stamp.time_us, rt=r->acceleration_mps2.stamp.time_us;
    if ((lt>rt?lt-rt:rt-lt)>skew) return MA_INCOMPLETE;
    if (!quat_ok(l->sensor_to_vehicle) || !quat_ok(r->sensor_to_vehicle) ||
        !vec_ok(l->acceleration_mps2.value) || !vec_ok(r->acceleration_mps2.value)) return MA_INVALID;
    ma_vec3 a=rotate(l->sensor_to_vehicle,l->acceleration_mps2.value);
    ma_vec3 b=rotate(r->sensor_to_vehicle,r->acceleration_mps2.value);
    ma_vec3 d={a.x-b.x,a.y-b.y,a.z-b.z};
    if (!vec_ok(d)) return MA_INVALID;
    *o=d; return ls;
}
ma_status ma_estimate_water(const ma_geometry *g, const ma_water_input *i,
    const ma_water_policy *p, uint64_t now, ma_clock clock, ma_water *o) {
    if (!o) return MA_INVALID;
    *o=no_water(MA_INVALID);
    if (!i || !p) return MA_INVALID;
    ma_status s=ma_freshness(i->stamp,now,clock,p->max_age_us);
    if (s!=MA_VALID && s!=MA_SIMULATED) { *o=no_water(s); return s; }
    if (i->quantity==MA_WATER_RAW_REGISTER) { *o=no_water(MA_INCOMPLETE); return MA_INCOMPLETE; }
    if (i->calibration_status!=MA_VALID || !p->stationary || !p->level || !p->settled) {
        *o=no_water(MA_INCOMPLETE); return MA_INCOMPLETE;
    }
    if (!geometry_ok(g) || !isfinite(i->value) || i->value<0 || !isfinite(i->sensor_above_bottom_m) ||
        i->sensor_above_bottom_m<0 || i->sensor_above_bottom_m>g->height_m ||
        !isfinite(i->offset_m)) return MA_INVALID;
    double h=i->value;
    if (i->quantity==MA_WATER_GAUGE_KPA) {
        if (!p->gauge_pressure_confirmed) { *o=no_water(MA_INCOMPLETE); return MA_INCOMPLETE; }
        h=i->value*1000/(1000*9.81);
    } else if (i->quantity!=MA_WATER_DEPTH_M) return MA_INVALID;
    ma_status result=ma_water_at_depth(g,h+i->sensor_above_bottom_m+i->offset_m,1000,o);
    if (result==MA_VALID) { o->status=s; if (o->cg_status==MA_VALID) o->cg_status=s; return s; }
    return result;
}
ma_ltr ma_truck_ltr(const ma_vehicle *v) {
    ma_ltr o={MA_INCOMPLETE,NAN,NAN};
    if (!v || v->status!=MA_VALID || !v->mass_accounting_complete || !v->track_model_justified ||
        !positive(v->fixed_mass_kg) || !positive(v->fixed_cg_ground_m) || !positive(v->aggregate_track_m)) return o;
    o.status=MA_UNAVAILABLE; return o;
}
ma_pendulum_output ma_pendulum_step(const ma_pendulum_parameters *p,
    const ma_pendulum_state *s, const ma_pendulum_input *i) {
    (void)p; (void)s; (void)i;
    return (ma_pendulum_output){MA_UNAVAILABLE,NAN,NAN};
}
