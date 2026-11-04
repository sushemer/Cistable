#include "control_cabina.h"
#include <math.h>
#include <string.h>

cc_config cc_pending_config(void) {
    cc_config c={0};c.rear_r=(ma_vec3){0.64,-0.42,-0.94};
    c.mounting=(ma_quat){NAN,NAN,NAN,NAN};
    c.dry_mass_kg=c.fixed_water_mass_kg=c.reference_ground_m=c.track_m=NAN;
    c.dry_position=c.fixed_water_position=(ma_vec3){NAN,NAN,NAN};
    /* Software trial policy, not identified physical parameters. */
    c.max_report_age_us=100000;c.max_skew_us=20000;c.water_hold_us=2000000;
    c.alpha_min_dt_s=0.002;c.alpha_max_dt_s=0.05;c.alpha_filter_tau_s=0.03;
    c.stopped_kmh=0.5;c.moving_kmh=2;c.risk_warning=0.6;c.risk_critical=0.75;c.risk_hysteresis=0.03;
    c.raw_to_m=c.sensor_height_m=c.water_offset_m=NAN;c.level_tolerance_rad=0.03;
    c.min_report_quality=2; /* SH2 medium; configurable trial criterion. */
    return c;
}
void cc_init(cc_state *s) {
    memset(s,0,sizeof(*s));s->status=MA_MISSING;
    s->dynamic_reason="sin observaciones";
    s->water.status=MA_MISSING;s->water.cg_status=MA_MISSING;
    s->water.volume_m3=s->water.mass_kg=s->water.cg_ground_m=s->water.volume_percent=NAN;
    s->ltr=(ma_ltr){MA_MISSING,NAN,NAN,NAN};
}
bool cc_arm(cc_state *s,ma_pendulum_state initial) {
    if(!s||!s->stream.initialized||!isfinite(initial.angle_rad)||!isfinite(initial.rate_radps)||
        (initial.status!=MA_EXPERIMENTAL&&initial.status!=MA_SIMULATED)) return false;
    s->pendulum=initial;s->armed=true;s->status=MA_INCOMPLETE;
    s->dynamic_reason="rearme: esperando dos parejas nuevas";
    s->ltr.status=MA_INCOMPLETE;
    s->last_integrated_us=0;s->calculation_expires_local=0;
    s->alpha.initialized=false;
    s->last_accel_us=s->last_gyro_us=s->last_orientation_us=0;
    s->last_accel_sequence=s->last_gyro_sequence=0;
    return true; /* require two new observations for alpha after explicit rearm */
}
static cc_output missing(ma_status status,const char *reason) {
    return (cc_output){status,CC_NO_DATA,0,NAN,{status,NAN,NAN,NAN},reason};
}
static void motion(cc_state *s,const cc_config *c,cc_conditions v) {
    if(!v.speed_valid||!isfinite(v.speed_kmh)||v.speed_kmh<0||
        !isfinite(c->stopped_kmh)||!isfinite(c->moving_kmh)||c->stopped_kmh<0||c->moving_kmh<=c->stopped_kmh) {
        s->motion_known=false;return;
    }
    if(v.speed_kmh<=c->stopped_kmh) {s->moving=false;s->motion_known=true;}
    else if(v.speed_kmh>=c->moving_kmh) {s->moving=true;s->motion_known=true;}
}
static bool usable(ma_status status) {
    return status==MA_VALID||status==MA_EXPERIMENTAL||status==MA_SIMULATED;
}
static bool finite3(ma_vec3 v) {return isfinite(v.x)&&isfinite(v.y)&&isfinite(v.z);}
static bool positive(double v) {return isfinite(v)&&v>0;}
static bool latched(ma_status status) {
    return status==MA_LTR_LIMIT||status==MA_OUT_OF_DOMAIN||status==MA_NUMERICAL_ERROR||status==MA_NO_SUPPORT;
}
static void invalidate_dynamic(cc_state *s,ma_status status,const char *reason) {
    if(s->armed) {
        ++s->disarm_count;s->last_disarm_status=status;s->last_disarm_reason=reason;
    }
    s->armed=false;s->alpha.initialized=false;s->calculation_expires_local=0;
    s->dynamic_reason=reason;
    s->arm_reference_level=false;s->arm_reference_expires_local=0;
    s->pendulum.status=status;s->status=status;
    s->ltr.status=status;
    /* Keep numeric state and last_integrated_us only for diagnosis, never reuse
     * them as a valid initial condition without cc_arm(). */
}
void cc_invalidate_water(cc_state *s) {
    if(!s) return;
    s->have_water=false;
    invalidate_dynamic(s,MA_INCOMPLETE,"operador: volumen invalidado por llenado/descarga");
}
void cc_apply_config(cc_state *s,cc_config *active,const cc_config *next) {
    if(!s||!active||!next) return;
    invalidate_dynamic(s,MA_INCOMPLETE,"configuracion cambiada; estado inicial y rearme requeridos");
    s->last_integrated_us=s->last_accel_us=s->last_gyro_us=s->last_orientation_us=0;
    s->last_accel_sequence=s->last_gyro_sequence=0;s->have_water=false;
    s->arm_reference_level=false;s->arm_reference_expires_local=0;
    s->pendulum=(ma_pendulum_state){MA_MISSING,NAN,NAN};
    s->water.status=s->water.cg_status=MA_MISSING;
    *active=*next;s->configuration_id=next->configuration_id;
}
static void adopt_config(cc_state *s,const cc_config *c) {
    if(s->configuration_id==c->configuration_id) return;
    cc_config copy=*c;cc_apply_config(s,&copy,c);
}
static bool water_numbers_valid(const ma_water *w,bool dynamic) {
    if(!usable(w->status)||!isfinite(w->mass_kg)||w->mass_kg<0||
       !isfinite(w->volume_m3)||w->volume_m3<0||!isfinite(w->depth_m)||w->depth_m<0||
       !isfinite(w->volume_percent)||w->volume_percent<0||w->volume_percent>100) return false;
    return !dynamic||(w->mass_kg>0&&w->volume_m3>0&&usable(w->cg_status)&&
        isfinite(w->cg_ground_m)&&w->cg_ground_m>=0);
}
static bool water_current(const cc_state *s,const cc_config *c,uint64_t now,bool dynamic) {
    if(c->virtual_profile) return s->have_water&&s->water.status==MA_SIMULATED&&water_numbers_valid(&s->water,dynamic);
    return s->have_water&&water_numbers_valid(&s->water,dynamic)&&now>=s->water_updated_local&&
        now-s->water_updated_local<=c->water_hold_us;
}
cc_output cc_current(cc_state *s,const cc_config *c,uint64_t now,uint64_t rx,cc_conditions v) {
    if(!s||!c) return missing(MA_INVALID,"argumentos");
    adopt_config(s,c);
    motion(s,c,v);
    if(now<rx||now-rx>c->max_report_age_us) {
        if(s->armed) invalidate_dynamic(s,MA_STALE,"recepcion antigua; continuidad invalidada");
        return missing(MA_STALE,"recepcion antigua; continuidad invalidada");
    }
    if(!s->motion_known) {
        if(s->armed) invalidate_dynamic(s,MA_MISSING,"movimiento desconocido; GPS invalido");
        return missing(MA_MISSING,"movimiento desconocido; GPS invalido");
    }
    if(s->status==MA_LTR_LIMIT)
        return (cc_output){MA_LTR_LIMIT,CC_LIMIT,2,NAN,s->ltr,
            "limite experimental registrado; valor retenido, rearme requerido"};
    if(latched(s->status)) return missing(s->status,ma_status_name(s->status));
    if(s->armed&&(!v.no_filling_or_draining_confirmed||!water_current(s,c,now,true)))
        invalidate_dynamic(s,MA_INCOMPLETE,!v.no_filling_or_draining_confirmed?
            "ausencia de flujo no confirmada":"volumen retenido ausente/invalido/caducado");
    if(s->armed&&s->last_integrated_us&&now>s->calculation_expires_local)
        invalidate_dynamic(s,MA_STALE,"vigencia del calculo vencida sin nueva pareja");
    if(!s->moving) {
        if(c->virtual_profile&&water_current(s,c,now,false))
            return (cc_output){MA_EXPERIMENTAL,CC_LEVEL,0,s->water.volume_percent,s->ltr,
                s->armed&&usable(s->status)?"PIPA VIRTUAL: volumen CONFIGURADO, no medicion de agua":s->dynamic_reason};
        if(!c->water_calibration_known||!c->mounting_known||!v.stationary_settled_confirmed||
           now>s->water_orientation_expires_local||!water_current(s,c,now,false))
            return missing(MA_STALE,"agua/orientacion ausente, invalida, no asentada o antigua");
        return (cc_output){s->water.status,CC_LEVEL,0,s->water.volume_percent,s->ltr,
            "agua de ultimo reposo valido, con edad acotada; LTR independiente"};
    }
    if(!s->armed||!usable(s->status)) return missing(s->status,s->dynamic_reason?s->dynamic_reason:ma_status_name(s->status));
    return (cc_output){s->status,CC_MOVING,s->risk,NAN,s->ltr,"clasificacion experimental"};
}
static bool report_fresh(const ct_frame *f,uint64_t t,uint64_t rx,uint64_t now,const cc_config *c) {
    uint64_t age;
    return c->latency_bound_known&&ct_age_us(f->sent_us,t,rx,now,&age)&&
        c->latency_bound_us<=c->max_report_age_us&&age<=c->max_report_age_us-c->latency_bound_us;
}
static bool report_expiry(const ct_frame *f,uint64_t t,uint64_t rx,uint64_t now,const cc_config *c,uint64_t *out) {
    uint64_t age;
    if(!report_fresh(f,t,rx,now,c)||!ct_age_us(f->sent_us,t,rx,now,&age)) return false;
    uint64_t remaining=c->max_report_age_us-age-c->latency_bound_us;
    if(now>UINT64_MAX-remaining) return false;
    *out=now+remaining;return true;
}
/* Independent hydrostatic path. No configured/rigid/lever, accel or gyro gate. */
static void update_water(cc_state *s,const cc_config *c,const ct_frame *f,uint64_t rx,uint64_t now,cc_conditions v) {
    if(c->virtual_profile) {
        s->water=c->scenario_water;s->have_water=water_numbers_valid(&s->water,true)&&s->water.status==MA_SIMULATED;
        s->water_updated_local=0; /* no sensor acquisition time exists for a scenario */
        return;
    }
    s->water_orientation_expires_local=0;
    if(!c->water_calibration_known) {s->have_water=false;return;}
    if(!c->mounting_known||!s->motion_known||s->moving||!v.stationary_settled_confirmed||
       !(f->flags&CT_ORIENTATION)||c->min_report_quality>3||
       f->orientation_quality>3||f->orientation_quality<c->min_report_quality||
       !positive(c->level_tolerance_rad)||c->level_tolerance_rad>=1.5707963267948966) return;
    ma_quat orientation={f->quaternion[0],-f->quaternion[1],-f->quaternion[2],-f->quaternion[3]},mounting;
    ma_vec3 g_sensor,g;
    if(ma_quat_normalize_checked(orientation,&orientation)!=MA_VALID||
       ma_quat_normalize_checked(c->mounting,&mounting)!=MA_VALID||
       ma_rotate_vector(orientation,(ma_vec3){0,0,-9.81},&g_sensor)!=MA_VALID||
       ma_rotate_vector(mounting,g_sensor,&g)!=MA_VALID||
       g.z>=0||hypot(g.x,g.y)>9.81*sin(c->level_tolerance_rad)||
       !report_expiry(f,f->orientation_us,rx,now,c,&s->water_orientation_expires_local)) return;
    if(!(f->flags&CT_WATER)||!report_fresh(f,f->water_us,rx,now,c)) return;
    if(!isfinite(c->sensor_height_m)||c->sensor_height_m<0||!isfinite(c->water_offset_m)||
       (f->water_quantity==CT_RAW_REGISTER&&!positive(c->raw_to_m))) {s->have_water=false;return;}
    ma_tank tank=ma_model_a_tank();ma_geometry geometry=c->geometry;
    /* Only legacy all-zero configs request historical geometry. Explicit
     * invalid menu values must reach validation, never a silent fallback. */
    if(geometry.width_m==0&&geometry.height_m==0&&geometry.length_m==0&&geometry.bottom_m==0&&geometry.capacity_m3==0&&
       ma_geometry_make(&tank,&geometry)!=MA_VALID) {s->have_water=false;return;}
    bool raw=f->water_quantity==CT_RAW_REGISTER;
    ma_water_input input={{c->simulation?MA_SIMULATED:MA_EXPERIMENTAL,now,MA_CLOCK_CABIN,-1},
        raw?MA_WATER_DEPTH_M:(f->water_quantity==CT_DEPTH_M?MA_WATER_DEPTH_M:MA_WATER_GAUGE_KPA),
        raw?f->water*c->raw_to_m:f->water,MA_VALID,c->sensor_height_m,c->water_offset_m};
    ma_water_policy policy={c->max_report_age_us,true,true,true,f->water_quantity==CT_GAUGE_KPA};
    ma_status status=ma_estimate_water(&geometry,&input,&policy,now,MA_CLOCK_CABIN,&s->water);
    uint64_t age;
    s->have_water=usable(status)&&water_numbers_valid(&s->water,false)&&
        ct_age_us(f->sent_us,f->water_us,rx,now,&age)&&age<=now&&c->latency_bound_us<=now-age;
    if(s->have_water) s->water_updated_local=now-age-c->latency_bound_us;
}
static cc_output reject_dynamic(cc_state *s,const cc_config *c,uint64_t now,uint64_t rx,
    cc_conditions v,ma_status status,const char *reason) {
    invalidate_dynamic(s,status,reason);
    cc_output output=cc_current(s,c,now,rx,v);
    if(output.mode==CC_LEVEL) return output; /* useful level does not validate dynamics */
    return missing(status,reason);
}
/* Strict numerical consistency at one configured filling point, NOT sensor
 * uncertainty, hysteresis or automatic parameter identification by level. */
static ma_status partition_status(const cc_config *c,const ma_water *w) {
    const ma_pendulum_parameters *p=&c->pendulum;
    if(!water_numbers_valid(w,true)||!usable(p->status)) return MA_INCOMPLETE;
    if(!positive(p->oscillating_mass_kg)||!isfinite(c->fixed_water_mass_kg)||c->fixed_water_mass_kg<0||
       !finite3(c->fixed_water_position)||!isfinite(p->center_z_m)||!positive(p->semi_z_m)||
       !positive(p->semi_y_m)||!positive(c->dry_mass_kg)||!finite3(c->dry_position)||
       !isfinite(c->reference_ground_m)||c->reference_ground_m<0||!positive(c->track_m)||
       !isfinite(p->damping_nm_s)||p->damping_nm_s<0||!positive(p->max_angle_rad)||
       p->max_angle_rad>=1.5707963267948966||!positive(p->max_dt_s)||!positive(p->max_step_s)||
       p->max_step_s>p->max_dt_s||!positive(c->alpha_min_dt_s)||!positive(c->alpha_max_dt_s)||
       c->alpha_min_dt_s>=c->alpha_max_dt_s||!isfinite(c->alpha_filter_tau_s)||c->alpha_filter_tau_s<0)
        return MA_INVALID;
    double total=c->fixed_water_mass_kg+p->oscillating_mass_kg;
    if(!isfinite(total)) return MA_NUMERICAL_ERROR;
    if(fabs(total-w->mass_kg)>1e-6*fmax(1,w->mass_kg)) return MA_INCOMPLETE;
    double rest_z=(p->oscillating_mass_kg*(p->center_z_m-p->semi_z_m)+
        c->fixed_water_mass_kg*c->fixed_water_position.z)/w->mass_kg+c->reference_ground_m;
    if(!isfinite(rest_z)) return MA_NUMERICAL_ERROR;
    return fabs(rest_z-w->cg_ground_m)>1e-5?MA_INCOMPLETE:MA_VALID;
}
static void commit_observation(cc_state *s,const ct_frame *f,ma_alpha_state alpha,uint64_t expiry) {
    s->alpha=alpha;s->last_accel_us=f->accel_us;s->last_gyro_us=f->gyro_us;
    s->last_orientation_us=f->orientation_us;s->last_accel_sequence=f->accel_sequence;
    s->last_gyro_sequence=f->gyro_sequence;s->last_integrated_us=f->accel_us;
    s->calculation_expires_local=expiry;
}
cc_output cc_process(cc_state *s,const cc_config *c,const ct_frame *f,uint64_t rx,uint64_t now,cc_conditions v) {
    if(!s||!c||!f) {if(s) invalidate_dynamic(s,MA_INVALID,"argumentos");return missing(MA_INVALID,"argumentos");}
    adopt_config(s,c);
    uint32_t previous_packet=s->stream.sequence;
    ct_acceptance accepted=ct_accept(&s->stream,f);
    if(accepted==CT_DUPLICATE) return cc_current(s,c,now,rx,v);
    if(accepted==CT_REVERSED||accepted==CT_BAD) return reject_dynamic(s,c,now,rx,v,MA_INVALID,"secuencia/sesion rechazada");
    if(accepted==CT_ACCEPT) s->lost_packets+=(uint32_t)(f->sequence-previous_packet)-1u;
    if(accepted==CT_NEW_SESSION) {
        invalidate_dynamic(s,MA_MISSING,"nueva sesion de origen");s->have_water=false;s->last_integrated_us=0;
        s->last_accel_us=s->last_gyro_us=s->last_orientation_us=0;
        s->last_accel_sequence=s->last_gyro_sequence=0;
    }
    motion(s,c,v);update_water(s,c,f,rx,now,v);
    bool direct=c->virtual_profile&&c->bypass_position;
    if(!c->configured||!c->mounting_known||(!direct&&(!c->rigid_assumed||!c->position_accepted))||
       (c->bypass_position&&!c->virtual_profile))
        return reject_dynamic(s,c,now,rx,v,MA_INCOMPLETE,!c->configured?"configuracion dinamica pendiente":
            !c->mounting_known?"montaje sin configurar":
            c->bypass_position&&!c->virtual_profile?"traslado desactivado en pipa real":
            !c->rigid_assumed?(c->virtual_profile?"traslado automovil: cuerpo rigido no aceptado":"solidaridad chasis/cisterna no aceptada"):
            "brazo de palanca no aceptado");
    if(!s->motion_known||!v.no_filling_or_draining_confirmed||!water_current(s,c,now,true)||
       !c->mass_accounting_complete||!c->track_justified)
        return reject_dynamic(s,c,now,rx,v,MA_INCOMPLETE,!s->motion_known?"movimiento desconocido; GPS invalido":
            !v.no_filling_or_draining_confirmed?"ausencia de flujo no confirmada":!water_current(s,c,now,true)?
            "volumen retenido ausente/invalido/caducado":!c->mass_accounting_complete?"contabilidad de masa incompleta":"via sin justificar");
    ma_status ps=partition_status(c,&s->water);
    if(ps!=MA_VALID) return reject_dynamic(s,c,now,rx,v,ps,"particion fija no consistente con el llenado/CG actual");
    if(!isfinite(c->risk_warning)||!isfinite(c->risk_critical)||!isfinite(c->risk_hysteresis)||
       c->risk_warning<=0||c->risk_critical<=c->risk_warning||c->risk_critical>=1||
       c->risk_hysteresis<0||c->risk_hysteresis>=c->risk_warning)
        return reject_dynamic(s,c,now,rx,v,MA_INCOMPLETE,"umbrales experimentales invalidos");
    if((f->flags&7)!=7||!report_fresh(f,f->accel_us,rx,now,c)||
       !report_fresh(f,f->gyro_us,rx,now,c)||!report_fresh(f,f->orientation_us,rx,now,c))
        return reject_dynamic(s,c,now,rx,v,MA_STALE,"reportes ausentes/antiguos o latencia sin medir");
    if(c->min_report_quality>3||f->accel_quality>3||f->gyro_quality>3||f->orientation_quality>3||
       f->accel_quality<c->min_report_quality||f->gyro_quality<c->min_report_quality||f->orientation_quality<c->min_report_quality)
        return reject_dynamic(s,c,now,rx,v,MA_INCOMPLETE,"calidad SH2 insuficiente/desconocida");
    uint64_t minimum=f->accel_us,maximum=minimum;
    if(f->gyro_us<minimum) minimum=f->gyro_us;
    if(f->orientation_us<minimum) minimum=f->orientation_us;
    if(f->gyro_us>maximum) maximum=f->gyro_us;
    if(f->orientation_us>maximum) maximum=f->orientation_us;
    if(maximum-minimum>c->max_skew_us)
        return reject_dynamic(s,c,now,rx,v,MA_INCOMPLETE,"reportes fuera del skew admitido");
    double g0=c->gravity_constant>0?c->gravity_constant:9.81;
    if(!isfinite(g0)||(c->virtual_profile&&!(c->gravity_constant>0)))
        return reject_dynamic(s,c,now,rx,v,MA_INVALID,"gravedad configurada invalida");
    ma_vec3 accel,omega,gravity_sensor,gravity;ma_quat mounting;
    ma_quat orientation={f->quaternion[0],-f->quaternion[1],-f->quaternion[2],-f->quaternion[3]};
    if(ma_quat_normalize_checked(orientation,&orientation)!=MA_VALID||ma_quat_normalize_checked(c->mounting,&mounting)!=MA_VALID||
       ma_rotate_vector(mounting,(ma_vec3){f->acceleration[0],f->acceleration[1],f->acceleration[2]},&accel)!=MA_VALID||
       ma_rotate_vector(mounting,(ma_vec3){f->gyro[0],f->gyro[1],f->gyro[2]},&omega)!=MA_VALID||
       ma_rotate_vector(orientation,(ma_vec3){0,0,-g0},&gravity_sensor)!=MA_VALID||
       ma_rotate_vector(mounting,gravity_sensor,&gravity)!=MA_VALID)
        return reject_dynamic(s,c,now,rx,v,MA_INVALID,"transformacion invalida");
    s->arm_reference_level=gravity.z<0&&hypot(gravity.x,gravity.y)<=g0*sin(c->level_tolerance_rad);
    report_expiry(f,f->orientation_us,rx,now,c,&s->arm_reference_expires_local);
    /* Validate present measurements before presenting a retained model fault.
     * This does not rearm it or consume any observation. */
    if(!s->armed&&latched(s->status)) return cc_current(s,c,now,rx,v);
    if(!s->armed) {
        s->status=MA_INCOMPLETE;s->dynamic_reason="rearme explicito requerido";
        return cc_current(s,c,now,rx,v);
    }
    if(s->last_accel_us) {
        uint32_t da=f->accel_sequence-s->last_accel_sequence,dg=f->gyro_sequence-s->last_gyro_sequence;
        if(da>=0x80000000u||dg>=0x80000000u)
            return reject_dynamic(s,c,now,rx,v,MA_INVALID,"secuencia de reporte invertida");
        if(da>1||dg>1) {
            if(da>1) s->lost_accel_reports+=da-1u;
            if(dg>1) s->lost_gyro_reports+=dg-1u;
            return reject_dynamic(s,c,now,rx,v,MA_STALE,"salto/sobrescritura de reporte; rearme requerido");
        }
        if((da==0&&f->accel_us!=s->last_accel_us)||(dg==0&&f->gyro_us!=s->last_gyro_us)||
           (da&&f->accel_us<=s->last_accel_us)||(dg&&f->gyro_us<=s->last_gyro_us)||f->orientation_us<s->last_orientation_us)
            return reject_dynamic(s,c,now,rx,v,MA_INVALID,"tiempo/secuencia de reporte incoherentes");
        if(now>s->calculation_expires_local)
            return reject_dynamic(s,c,now,rx,v,MA_STALE,"continuidad vencida antes de recibir la pareja; rearme requerido");
        if(!da||!dg) {
            s->dynamic_reason="esperando pareja adyacente; ultimo resultado aun vigente";
            cc_output output=cc_current(s,c,now,rx,v);
            if(usable(output.status)) output.reason="esperando pareja adyacente; ultimo resultado aun vigente";
            return output; /* do not consume either report; no integration or alpha update */
        }
    }
    double dt=s->last_integrated_us?(f->accel_us-s->last_integrated_us)*1e-6:0;
    if(s->last_integrated_us&&(!positive(dt)||dt<c->alpha_min_dt_s||dt>c->alpha_max_dt_s||dt>c->pendulum.max_dt_s))
        return reject_dynamic(s,c,now,rx,v,MA_STALE,"intervalo dinamico no cubierto; rearme requerido");
    ma_alpha_state next_alpha=s->alpha;ma_vec3 alpha,reference;
    ma_vector_report gyro={{c->simulation?MA_SIMULATED:MA_EXPERIMENTAL,f->gyro_us,MA_CLOCK_TANK,f->gyro_quality},omega};
    ma_status as=ma_alpha_update(&next_alpha,gyro,f->session,c->alpha_min_dt_s,c->alpha_max_dt_s,c->alpha_filter_tau_s,&alpha);
    uint64_t expiry;
    if(!report_expiry(f,minimum,rx,now,c,&expiry)) return reject_dynamic(s,c,now,rx,v,MA_INVALID,"edad no representable");
    /* Also bound waiting by the maximum integrable interval, even if report
     * freshness would permit displaying the last result for longer. */
    double horizon=fmin(c->alpha_max_dt_s,c->pendulum.max_dt_s)*1e6;
    uint64_t limit=horizon>=(double)UINT64_MAX?UINT64_MAX:(uint64_t)horizon,age;
    if(!ct_age_us(f->sent_us,f->accel_us,rx,now,&age)||age>limit||c->latency_bound_us>limit-age)
        return reject_dynamic(s,c,now,rx,v,MA_STALE,"intervalo dinamico ya vencido");
    uint64_t remaining=limit-age-c->latency_bound_us;
    uint64_t interval_expiry=now>UINT64_MAX-remaining?UINT64_MAX:now+remaining;
    if(interval_expiry<expiry) expiry=interval_expiry;
    if(as==MA_MISSING&&!s->last_integrated_us) {
        commit_observation(s,f,next_alpha,expiry);s->status=MA_MISSING;
        s->dynamic_reason="primera pareja: esperando derivada angular";
        return cc_current(s,c,now,rx,v); /* declared initial state anchored to first new pair */
    }
    if(!usable(as)) return reject_dynamic(s,c,now,rx,v,as,"derivada angular no disponible");
    ma_status correction;
    if(direct) {reference=accel;correction=MA_EXPERIMENTAL;}
    else correction=ma_correct_position(accel,omega,alpha,c->rear_r,c->rigid_assumed,&reference);
    if(correction!=MA_EXPERIMENTAL||!finite3(reference))
        return reject_dynamic(s,c,now,rx,v,correction==MA_EXPERIMENTAL?MA_NUMERICAL_ERROR:correction,"correccion de posicion invalida");
    ma_pendulum_input input={c->simulation?MA_SIMULATED:MA_EXPERIMENTAL,reference,omega,alpha,gravity,dt};
    ma_pendulum_output out=ma_pendulum_step(&c->pendulum,&s->pendulum,&input);
    if(out.status!=input.status) return reject_dynamic(s,c,now,rx,v,out.status,ma_status_name(out.status));
    ma_vec3 dry,water;double dry_moment,water_moment;
    ma_status ds=ma_fixed_mass_force(c->dry_mass_kg,c->dry_position,&input,&dry,&dry_moment);
    ma_status ws=ma_fixed_mass_force(c->fixed_water_mass_kg,c->fixed_water_position,&input,&water,&water_moment);
    if(ds!=input.status||ws!=input.status)
        return reject_dynamic(s,c,now,rx,v,ds!=input.status?ds:ws,ds!=input.status?"fuerza seca invalida":"fuerza de agua fija invalida");
    double moment=dry_moment+water_moment+out.roll_moment_nm-c->reference_ground_m*(dry.y+water.y+out.force_n.y);
    double normal=-dry.z-water.z-out.force_n.z;
    if(!isfinite(moment)||!isfinite(normal)) return reject_dynamic(s,c,now,rx,v,MA_NUMERICAL_ERROR,"suma de fuerzas/momentos no finita");
    ma_vehicle vehicle={input.status,c->dry_mass_kg,c->dry_position.z+c->reference_ground_m,
        c->track_m,c->mass_accounting_complete,c->track_justified,moment,normal};
    ma_ltr ltr=ma_truck_ltr(&vehicle);
    if(ltr.status!=input.status&&ltr.status!=MA_LTR_LIMIT)
        return reject_dynamic(s,c,now,rx,v,ltr.status,ma_status_name(ltr.status));
    /* Transactional commit: no rejected interval advances source/model time. */
    commit_observation(s,f,next_alpha,expiry);s->ltr=ltr;s->pendulum=out.next;s->status=ltr.status;
    s->dynamic_reason="paso completo experimental/sintetico; no validado";
    if(ltr.status==MA_LTR_LIMIT) {
        invalidate_dynamic(s,MA_LTR_LIMIT,"limite experimental calculado: magnitud LTR >= 1");
        return cc_current(s,c,now,rx,v);
    }
    double magnitude=ltr.magnitude;
    if(magnitude>=c->risk_critical) s->risk=2;
    else if(s->risk==2&&magnitude>=c->risk_critical-c->risk_hysteresis) s->risk=2;
    else if(magnitude>=c->risk_warning) s->risk=1;
    else if(s->risk&&magnitude>=c->risk_warning-c->risk_hysteresis) s->risk=1;
    else s->risk=0;
    return cc_current(s,c,now,rx,v);
}
