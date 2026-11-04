#include "pipa_virtual.h"
#include <math.h>
#include <string.h>
#define PI 3.14159265358979323846
static bool positive(double x) {return isfinite(x)&&x>0;}
static bool finite3(ma_vec3 v) {return isfinite(v.x)&&isfinite(v.y)&&isfinite(v.z);}
ma_quat ci_mounting_zyx(double roll,double pitch,double yaw) {
    double r=roll*PI/360,p=pitch*PI/360,y=yaw*PI/360;
    return (ma_quat){cos(y)*cos(p)*cos(r)+sin(y)*sin(p)*sin(r),
        cos(y)*cos(p)*sin(r)-sin(y)*sin(p)*cos(r),
        cos(y)*sin(p)*cos(r)+sin(y)*cos(p)*sin(r),
        sin(y)*cos(p)*cos(r)-cos(y)*sin(p)*sin(r)};
}
ci_virtual_parameters ci_virtual_defaults(void) {
    ci_virtual_parameters p={0};
    p.width_i_m=2.210475;p.height_i_m=1.210475;p.length_i_m=3.030475;p.bottom_ground_m=1.1647625;
    p.fill_volume_percent=50;p.density_kg_m3=1000;p.gravity_m_s2=9.81;p.track_m=1.81;
    p.dry[0]=(ci_component){5000,{0,0,1.0}}; /* generic virtual medium truck scenario */
    p.dry[1]=(ci_component){924,{0,0,NAN}}; /* recovered shell estimate; z derived at tank centre */
    p.dry[2]=(ci_component){600,{0,0,1.3}}; /* virtual platform/pump/accessories, no shell/diesel */
    p.dry[3]=(ci_component){173.888,{0,0,0.8}}; /* recovered 209 L x .832 kg/L */
    p.dry[4]=(ci_component){200,{0,0,1.2}}; /* virtual occupants/equipment */
    p.dry_cg_ground_m=(ma_vec3){0,0,1.13};
    p.zeta=0.05;p.mass_scale=1;p.max_angle_rad=0.2;
    p.mounting=(ma_quat){1,0,0,0}; /* placeholder, NOT confirmed */
    p.sensor_r_m=(ma_vec3){0,0,0}; /* independent CAR origin; no truck vector copied */
    p.latency_bound_us=20000; /* explicit provisional transport/observation budget */
    return p;
}
static uint64_t hash_bytes(uint64_t h,const void *p,size_t n) {
    const unsigned char *b=p;for(size_t k=0;k<n;k++) {h^=b[k];h*=1099511628211ULL;}return h;
}
/* Hash semantic values individually: no uninitialised padding or pointers. */
static uint64_t identity(const ci_virtual_parameters *p) {
    uint64_t h=1469598103934665603ULL;
    const double a[]={p->width_i_m,p->height_i_m,p->length_i_m,p->bottom_ground_m,p->fill_volume_percent,
        p->density_kg_m3,p->gravity_m_s2,p->track_m,p->dry_cg_ground_m.x,p->dry_cg_ground_m.y,p->dry_cg_ground_m.z,
        p->zeta,p->mass_scale,p->cg_shift_m,p->max_angle_rad,p->mounting.w,p->mounting.x,p->mounting.y,p->mounting.z,
        p->sensor_r_m.x,p->sensor_r_m.y,p->sensor_r_m.z};
    h=hash_bytes(h,a,sizeof(a));
    for(unsigned k=0;k<CI_DRY_COMPONENTS;k++) {
        const double v[]={p->dry[k].mass_kg,p->dry[k].cg_ground_m.x,p->dry[k].cg_ground_m.y,p->dry[k].cg_ground_m.z};
        h=hash_bytes(h,v,sizeof(v));
    }
    const uint8_t flags[]={p->override_dry_cg,p->mounting_confirmed,p->lever_enabled,p->rigid_assumed};
    h=hash_bytes(h,flags,sizeof(flags));h=hash_bytes(h,&p->latency_bound_us,sizeof(p->latency_bound_us));
    return h?h:1;
}
ma_status ci_virtual_derive(const ci_virtual_parameters *p,cc_config *out,ci_virtual_derived *d) {
    if(!p||!out||!d) return MA_INVALID;
    *out=cc_pending_config();*d=(ci_virtual_derived){.status=MA_INVALID,.reason="parametro virtual invalido"};
    out->virtual_profile=true;out->configuration_id=identity(p);
    out->bypass_position=!p->lever_enabled;out->rear_r=p->sensor_r_m;
    out->mounting=p->mounting;out->mounting_known=p->mounting_confirmed;
    out->rigid_assumed=p->rigid_assumed;out->position_accepted=p->lever_enabled&&finite3(p->sensor_r_m);
    if(ma_geometry_from_interior(p->width_i_m,p->height_i_m,p->length_i_m,p->bottom_ground_m,&out->geometry)!=MA_VALID||
       !positive(p->density_kg_m3)||!positive(p->gravity_m_s2)||!positive(p->track_m)||
       !isfinite(p->fill_volume_percent)||p->fill_volume_percent<0||p->fill_volume_percent>100) return d->status;
    out->reference_ground_m=p->bottom_ground_m+p->height_i_m/2;out->gravity_constant=p->gravity_m_s2;
    double h;
    if(ma_depth_at_volume(&out->geometry,out->geometry.capacity_m3*(p->fill_volume_percent/100),&h)!=MA_VALID||
       ma_water_at_depth(&out->geometry,h,p->density_kg_m3,&out->scenario_water)!=MA_VALID) return d->status;
    out->scenario_water.status=MA_SIMULATED;
    if(out->scenario_water.cg_status==MA_VALID) out->scenario_water.cg_status=MA_SIMULATED;
    if(p->fill_volume_percent<20||p->fill_volume_percent>80) {
        d->status=MA_OUT_OF_DOMAIN;d->reason="solo hidrostatica: llenado fuera de 20..80 por ciento";return d->status;
    }
    ma_quat checked;
    if(!isfinite(p->zeta)||p->zeta<0||p->zeta>=1||!positive(p->mass_scale)||
       !isfinite(p->cg_shift_m)||!positive(p->max_angle_rad)||p->max_angle_rad>0.3||
       !finite3(p->sensor_r_m)||ma_quat_normalize_checked(p->mounting,&checked)!=MA_VALID||
       p->latency_bound_us>=out->max_report_age_us) return d->status;
    out->mounting=checked;
    ma_vec3 weighted={0,0,0};double dry=0;
    for(unsigned k=0;k<CI_DRY_COMPONENTS;k++) {
        ma_vec3 q=p->dry[k].cg_ground_m;
        if(k==1) q.z=out->reference_ground_m; /* shell centre follows geometry */
        double m=p->dry[k].mass_kg;
        if(!isfinite(m)||m<0||!finite3(q)||q.z<0) return d->status;
        dry+=m;weighted.x+=m*q.x;weighted.y+=m*q.y;weighted.z+=m*q.z;
    }
    if(!positive(dry)) return d->status;
    ma_vec3 cg=p->override_dry_cg?p->dry_cg_ground_m:(ma_vec3){weighted.x/dry,weighted.y/dry,weighted.z/dry};
    cg.z+=p->cg_shift_m;
    if(!finite3(cg)||!positive(cg.z)) return d->status;
    out->dry_mass_kg=dry;out->dry_position=(ma_vec3){cg.x,cg.y,cg.z-out->reference_ground_m};
    /* Dodge (2000), table 3.1. Rectangle surrogate preserves total volume,
     * longitudinal length and free-surface width; NOT elliptical eigenanalysis. */
    double a=p->width_i_m/2,b=p->height_i_m/2;
    double z=(h-b)/b,B=2*a*sqrt(fmax(0,1-z*z));
    if(!positive(B)) return d->status;
    double heq=out->scenario_water.volume_m3/(p->length_i_m*B),k=PI/B;
    if(!positive(heq)||!positive(k)) return d->status;
    double t=tanh(k*heq),omega2=p->gravity_m_s2*k*t;
    double fraction=8*B*t/(PI*PI*PI*heq),M=out->scenario_water.mass_kg;
    if(!positive(omega2)||!positive(fraction)) return d->status;
    double mp=M*fraction*p->mass_scale,mf=M-mp,l=p->gravity_m_s2/omega2;
    if(!positive(B)||!positive(heq)||!positive(omega2)||!positive(mp)||!positive(mf)||!positive(l)) return d->status;
    /* Table hinge minus length, relative to the surrogate liquid CG; vertical
     * translation maps that CG to the ACTUAL elliptical static centroid. */
    double rest=out->scenario_water.cg_ground_m+heq/2-2*tanh(k*heq/2)/k;
    double fixed=(M*out->scenario_water.cg_ground_m-mp*rest)/mf;
    double omega=sqrt(omega2),c=2*p->zeta*mp*l*l*omega;
    if(!positive(B)||!positive(heq)||!positive(omega2)||!positive(mp)||!positive(mf)||!positive(l)||
       !isfinite(rest)||!isfinite(fixed)||!isfinite(c)||c<0||
       fabs(mp+mf-M)>1e-6*fmax(1,M)||
       fabs((mp*rest+mf*fixed)/M-out->scenario_water.cg_ground_m)>1e-5) return d->status;
    out->fixed_water_mass_kg=mf;out->fixed_water_position=(ma_vec3){0,0,fixed-out->reference_ground_m};
    out->pendulum=(ma_pendulum_parameters){MA_EXPERIMENTAL,mp,l,l,
        rest-out->reference_ground_m+l,c,p->max_angle_rad,0.05,0.001};
    out->track_m=p->track_m;out->mass_accounting_complete=out->track_justified=true;
    out->latency_bound_known=true;out->latency_bound_us=p->latency_bound_us;
    out->configured=true; /* physical model complete as estimates; mounting still gated */
    *d=(ci_virtual_derived){MA_EXPERIMENTAL,B,heq,omega,omega/(2*PI),l,fraction,rest,fixed,dry,cg,
        "rectangulo equivalente, primer modo circular; no validado"};
    return d->status;
}
