#include "modelo_a.h"
#include <math.h>

static bool finite3(ma_vec3 v) { return isfinite(v.x)&&isfinite(v.y)&&isfinite(v.z); }
static ma_vec3 add(ma_vec3 a,ma_vec3 b) { return (ma_vec3){a.x+b.x,a.y+b.y,a.z+b.z}; }
static ma_vec3 scale(ma_vec3 a,double k) { return (ma_vec3){a.x*k,a.y*k,a.z*k}; }
static ma_vec3 cross(ma_vec3 a,ma_vec3 b) { return (ma_vec3){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
static double dot(ma_vec3 a,ma_vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
static bool positive(double v) { return isfinite(v)&&v>0; }
static ma_vec3 nan3(void) { return (ma_vec3){NAN,NAN,NAN}; }
static bool usable(ma_status s) { return s==MA_VALID||s==MA_SIMULATED||s==MA_EXPERIMENTAL; }

ma_status ma_quat_normalize_checked(ma_quat q,ma_quat *out) {
    if(!out) return MA_INVALID;
    *out=(ma_quat){NAN,NAN,NAN,NAN};
    double n=q.w*q.w+q.x*q.x+q.y*q.y+q.z*q.z;
    if(!isfinite(n)||fabs(n-1)>1e-4) return MA_INVALID;
    double norm=sqrt(n);
    *out=(ma_quat){q.w/norm,q.x/norm,q.y/norm,q.z/norm};
    return MA_VALID;
}

ma_status ma_rotate_vector(ma_quat q,ma_vec3 in,ma_vec3 *out) {
    if (!out) return MA_INVALID;
    *out=nan3();
    double n=q.w*q.w+q.x*q.x+q.y*q.y+q.z*q.z;
    if (!finite3(in)||!isfinite(n)||fabs(n-1)>1e-6) return MA_INVALID;
    ma_vec3 v={q.x,q.y,q.z}, t=scale(cross(v,in),2);
    *out=add(in,add(scale(t,q.w),cross(v,t)));
    return finite3(*out)?MA_VALID:MA_INVALID;
}
ma_status ma_correct_position(ma_vec3 a,ma_vec3 w,ma_vec3 alpha,ma_vec3 r,bool rigid,ma_vec3 *out) {
    if (!out) return MA_INVALID;
    *out=nan3();
    if (!rigid) return MA_INCOMPLETE;
    if (!finite3(a)||!finite3(w)||!finite3(alpha)||!finite3(r)) return MA_INVALID;
    *out=add(a,scale(add(cross(alpha,r),cross(w,cross(w,r))),-1));
    return finite3(*out)?MA_EXPERIMENTAL:MA_INVALID;
}
ma_status ma_alpha_update(ma_alpha_state *s,ma_vector_report g,uint32_t session,
    double min_dt,double max_dt,double tau,ma_vec3 *out) {
    if (!s||!out) return MA_INVALID;
    *out=nan3();
    if (!positive(min_dt)||!positive(max_dt)||min_dt>=max_dt||!isfinite(tau)||tau<0 ||
        !finite3(g.value)||!usable(g.stamp.status)||g.stamp.clock==MA_CLOCK_UNKNOWN) {
        s->initialized=false; return MA_INVALID;
    }
    if (!s->initialized||s->session!=session||s->last.clock!=g.stamp.clock) {
        *s=(ma_alpha_state){true,session,g.stamp,g.value,{0,0,0}};
        return MA_MISSING; /* First sample is not zero angular acceleration. */
    }
    if (g.stamp.time_us<=s->last.time_us) return MA_INVALID; /* duplicate or reverse */
    double dt=(g.stamp.time_us-s->last.time_us)*1e-6;
    if (dt<min_dt) return MA_INVALID;
    if (dt>max_dt) {
        *s=(ma_alpha_state){true,session,g.stamp,g.value,{0,0,0}};
        return MA_STALE;
    }
    ma_vec3 raw=scale(add(g.value,scale(s->omega,-1)),1/dt);
    double gain=dt/(tau+dt);
    ma_vec3 next=add(s->filtered_alpha,scale(add(raw,scale(s->filtered_alpha,-1)),gain));
    if (!finite3(next)) { s->initialized=false; return MA_INVALID; }
    s->filtered_alpha=next; s->omega=g.value; s->last=g.stamp;
    *out=next; return g.stamp.status;
}
static ma_vec3 body_accel(const ma_pendulum_input *i,ma_vec3 r,ma_vec3 velocity) {
    return add(i->acceleration_ref_mps2,add(cross(i->alpha_radps2,r),
        add(cross(i->omega_radps,cross(i->omega_radps,r)),scale(cross(i->omega_radps,velocity),2))));
}
ma_status ma_fixed_mass_force(double m,ma_vec3 r,const ma_pendulum_input *i,ma_vec3 *force,double *moment) {
    if (!force||!moment) return MA_INVALID;
    *force=nan3(); *moment=NAN;
    if (!i||!isfinite(m)||m<0||!finite3(r)||!usable(i->status)||
        !finite3(i->acceleration_ref_mps2)||!finite3(i->omega_radps)||
        !finite3(i->alpha_radps2)||!finite3(i->gravity_mps2)) return MA_INVALID;
    *force=scale(add(i->gravity_mps2,scale(body_accel(i,r,(ma_vec3){0,0,0}),-1)),m);
    *moment=cross(r,*force).x;
    return finite3(*force)&&isfinite(*moment)?i->status:MA_NUMERICAL_ERROR;
}
/* Elliptical constrained mass: r=(0,A sin(theta),z0-B cos(theta)).
 * Tangential projection of Newton's law includes prescribed frame rotation.
 * This is an independently derived reduced model, not Zheng's parameter fit. */
static double rhs(const ma_pendulum_parameters *p,const ma_pendulum_input *i,
    double th,double rate,ma_vec3 *rout,ma_vec3 *accout) {
    double s=sin(th),c=cos(th);
    ma_vec3 r={0,p->semi_y_m*s,p->center_z_m-p->semi_z_m*c};
    ma_vec3 tangent={0,p->semi_y_m*c,p->semi_z_m*s};
    ma_vec3 curvature={0,-p->semi_y_m*s,p->semi_z_m*c};
    ma_vec3 base=add(body_accel(i,r,scale(tangent,rate)),scale(curvature,rate*rate));
    double theta_dd=(dot(tangent,add(i->gravity_mps2,scale(base,-1)))-
        p->damping_nm_s*rate/p->oscillating_mass_kg)/dot(tangent,tangent);
    if (rout) *rout=r;
    if (accout) *accout=add(base,scale(tangent,theta_dd));
    return theta_dd;
}
ma_pendulum_output ma_pendulum_step(const ma_pendulum_parameters *p,
    const ma_pendulum_state *s,const ma_pendulum_input *i) {
    ma_pendulum_output o={.status=MA_UNAVAILABLE,.lateral_force_n=NAN,.roll_moment_nm=NAN,
        .force_n={NAN,NAN,NAN},.position_m={NAN,NAN,NAN},.next={MA_UNAVAILABLE,NAN,NAN}};
    if (!p||!s||!i) return o;
    o.status=MA_INCOMPLETE;
    if (!usable(p->status)||!usable(s->status)||!usable(i->status)) return o;
    o.status=MA_INVALID;
    if (!positive(p->oscillating_mass_kg)||!positive(p->semi_y_m)||!positive(p->semi_z_m)||
        !isfinite(p->center_z_m)||!isfinite(p->damping_nm_s)||p->damping_nm_s<0||
        !positive(p->max_angle_rad)||p->max_angle_rad>=1.5707963267948966||
        !positive(p->max_dt_s)||!positive(p->max_step_s)||p->max_step_s>p->max_dt_s||
        !positive(i->dt_s)||i->dt_s>p->max_dt_s||!isfinite(s->angle_rad)||!isfinite(s->rate_radps)||
        !finite3(i->acceleration_ref_mps2)||!finite3(i->omega_radps)||
        !finite3(i->alpha_radps2)||!finite3(i->gravity_mps2)) return o;
    if (fabs(s->angle_rad)>p->max_angle_rad) { o.status=MA_OUT_OF_DOMAIN; return o; }
    double steps=ceil(i->dt_s/p->max_step_s);
    if (!isfinite(steps)||steps>10000) return o;
    int n=(int)steps; double dt=i->dt_s/n,th=s->angle_rad,v=s->rate_radps;
    for (int k=0;k<n;k++) {
        double a=rhs(p,i,th,v,0,0);
        double v2=v+a*dt/2, a2=rhs(p,i,th+v*dt/2,v2,0,0);
        double v3=v+a2*dt/2, a3=rhs(p,i,th+v2*dt/2,v3,0,0);
        double v4=v+a3*dt, a4=rhs(p,i,th+v3*dt,v4,0,0);
        th+=dt*(v+2*v2+2*v3+v4)/6; v+=dt*(a+2*a2+2*a3+a4)/6;
        if (!isfinite(th)||!isfinite(v)) { o.status=MA_NUMERICAL_ERROR; return o; }
        if (fabs(th)>p->max_angle_rad) { o.status=MA_OUT_OF_DOMAIN; return o; }
    }
    ma_vec3 r,acc; rhs(p,i,th,v,&r,&acc);
    ma_vec3 f=scale(add(i->gravity_mps2,scale(acc,-1)),p->oscillating_mass_kg);
    if (!finite3(f)) { o.status=MA_NUMERICAL_ERROR; return o; }
    bool sim=p->status==MA_SIMULATED||s->status==MA_SIMULATED||i->status==MA_SIMULATED;
    /* Prevent silently combining real and synthetic inputs. */
    if (sim && !(p->status==MA_SIMULATED&&s->status==MA_SIMULATED&&i->status==MA_SIMULATED)) return o;
    o.status=sim?MA_SIMULATED:MA_EXPERIMENTAL;
    double moment=cross(r,f).x;
    if(!isfinite(moment)) {o.status=MA_NUMERICAL_ERROR;return o;}
    o.force_n=f; o.position_m=r; o.lateral_force_n=f.y; o.roll_moment_nm=moment;
    o.next=(ma_pendulum_state){o.status,th,v}; return o;
}
ma_ltr ma_truck_ltr(const ma_vehicle *v) {
    ma_ltr o={MA_INCOMPLETE,NAN,NAN,NAN};
    if (!v||!usable(v->status)||!v->mass_accounting_complete||!v->track_model_justified||
        !positive(v->fixed_mass_kg)||!positive(v->fixed_cg_ground_m)||!positive(v->aggregate_track_m)) return o;
    if (!isfinite(v->total_normal_n)||!isfinite(v->roll_moment_nm)) { o.status=MA_INVALID; return o; }
    if (v->total_normal_n<=0) { o.status=MA_NO_SUPPORT; return o; }
    double numerator=2*v->roll_moment_nm,denominator=v->total_normal_n*v->aggregate_track_m;
    if(!isfinite(numerator)||!isfinite(denominator)||denominator<=0) {o.status=MA_NUMERICAL_ERROR;return o;}
    o.signed_value=numerator/denominator;
    if (!isfinite(o.signed_value)) { o.signed_value=NAN; o.status=MA_NUMERICAL_ERROR; return o; }
    o.magnitude=fabs(o.signed_value); o.limited_signed=fmax(-1,fmin(1,o.signed_value));
    o.status=o.magnitude>=1?MA_LTR_LIMIT:(v->status==MA_SIMULATED?MA_SIMULATED:MA_EXPERIMENTAL);
    return o;
}
