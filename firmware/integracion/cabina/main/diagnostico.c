#include "diagnostico.h"
#include <inttypes.h>
#include <math.h>
#include <string.h>
static bool usable(ma_status s) {return s==MA_VALID||s==MA_EXPERIMENTAL||s==MA_SIMULATED;}
static uint64_t stamp(const ct_frame *f,unsigned k) {
    const uint64_t t[]={f->accel_us,f->gyro_us,f->orientation_us,f->water_us};return t[k];
}
static uint32_t sequence(const ct_frame *f,unsigned k) {
    const uint32_t n[]={f->accel_sequence,f->gyro_sequence,f->orientation_sequence,f->water_sequence};return n[k];
}
ci_report_diagnostic ci_diag_report(const ct_frame *f,unsigned k,uint64_t rx,uint64_t now,const cc_config *c) {
    ci_report_diagnostic r={0};r.status=MA_INVALID;if(!f||!c||k>3) return r;
    r.present=(f->flags&(1u<<k))!=0;
    r.age_known=ct_age_us(f->sent_us,stamp(f,k),rx,now,&r.partial_age_us);
    if(!r.present) {r.status=MA_MISSING;return r;}
    if(!r.age_known) return r;
    if(r.partial_age_us>c->max_report_age_us) {r.status=MA_STALE;return r;}
    if(k<3) {
        const uint8_t q[]={f->accel_quality,f->gyro_quality,f->orientation_quality};
        if(q[k]>3||q[k]<c->min_report_quality||c->min_report_quality>3) {r.status=MA_INCOMPLETE;return r;}
    }
    if(!c->latency_bound_known) {r.status=MA_INCOMPLETE;return r;}
    r.status=c->latency_bound_us>c->max_report_age_us-r.partial_age_us?MA_STALE:MA_EXPERIMENTAL;
    return r; /* Eligible report, NOT calibrated/physically validated measurement. */
}
static void temporal_probe(ci_diagnostics *d,const ct_frame *f,const cc_config *c) {
    /* Necessary timing checks only. No fictitious physical parameters, arm or LTR.
     * Transport latency and mounting remain separate unresolved requirements. */
    const char *failure=NULL;
    if((f->flags&7)!=7) failure="report_absent";
    uint64_t lo=f->accel_us,hi=lo;
    for(unsigned k=0;k<3;k++) {
        uint64_t t=stamp(f,k);if(t<lo) lo=t;if(t>hi) hi=t;
        if(t>f->sent_us||f->sent_us-t>c->max_report_age_us) failure="source_age";
    }
    if(hi-lo>c->max_skew_us) failure="skew";
    if(!failure&&d->pair_initialized) {
        uint32_t a=f->accel_sequence-d->pair_a,g=f->gyro_sequence-d->pair_g;
        if(a>1||g>1) failure="source_gap_or_reverse";
        else if((a&&f->accel_us<=d->pair_at)||(g&&f->gyro_us<=d->pair_gt)||
            (!a&&f->accel_us!=d->pair_at)||(!g&&f->gyro_us!=d->pair_gt)) failure="source_time";
        else if((f->sent_us-d->pair_at)*1e-6>c->alpha_max_dt_s||
                (f->sent_us-d->pair_gt)*1e-6>c->alpha_max_dt_s) failure="interval_expired";
        else if(!a||!g) {++d->timing_waits;d->timing_reason="waiting_adjacent";return;}
        else {
            double da=(f->accel_us-d->pair_at)*1e-6,dg=(f->gyro_us-d->pair_gt)*1e-6;
            if(da<c->alpha_min_dt_s||dg<c->alpha_min_dt_s||da>c->alpha_max_dt_s||dg>c->alpha_max_dt_s)
                failure="interval";
        }
        if(!failure) ++d->timing_pairs;
    }
    if(failure) {++d->timing_breaks;d->pair_initialized=false;d->timing_reason=failure;return;}
    d->pair_initialized=true;d->pair_a=f->accel_sequence;d->pair_g=f->gyro_sequence;
    d->pair_at=f->accel_us;d->pair_gt=f->gyro_us;d->timing_reason="structural_pair_only";
}
void ci_diag_receive(ci_diagnostics *d,const ct_frame *f,uint64_t rx,const cc_config *c) {
    uint32_t old=d->stream.sequence;bool existed=d->stream.initialized;
    ct_acceptance a=ct_accept(&d->stream,f);++d->packets;
    if(a==CT_DUPLICATE) {++d->duplicates;return;}
    if(a==CT_BAD||a==CT_REVERSED) {++d->rejected;return;}
    if(a==CT_NEW_SESSION) {
        if(existed) ++d->restarts;
        memset(d->report_seen,0,sizeof(d->report_seen));d->pair_initialized=false;
    } else d->gaps+=(uint32_t)(f->sequence-old)-1u;
    for(unsigned k=0;k<4;k++) if(f->flags&(1u<<k)) {
        uint32_t n=sequence(f,k);uint64_t t=stamp(f,k);
        if(d->report_seen[k]) {
            uint32_t delta=n-d->report_seq[k];
            if(!delta) ++d->repeats[k];
            else if(delta>=0x80000000u||t<=d->report_time[k]) ++d->backwards[k];
            else if(delta>1) d->lost[k]+=delta-1u;
        }
        d->report_seq[k]=n;d->report_time[k]=t;d->report_seen[k]=true;
    }
    temporal_probe(d,f,c);d->frame=*f;d->received_local_us=rx;d->have_frame=true;
}
unsigned ci_diag_observe(ci_diagnostics *d,const cc_state *s,cc_output o,uint64_t now,ci_model_event events[2]) {
    bool available=s->armed&&usable(s->status)&&s->last_integrated_us&&now<=s->calculation_expires_local;
    if(d->tick_initialized&&now>=d->last_tick_us) {
        uint64_t dt=now-d->last_tick_us;
        if(!d->dynamic_available) d->no_dynamic_us+=dt;
        if(!d->screen_available) d->no_screen_us+=dt;
    } else {d->missing_start_us=now;d->tick_initialized=true;}
    unsigned n=0;
    if(s->disarm_count!=d->observed_disarms) {
        events[n++]=(ci_model_event){now,s->disarm_count,s->stream.session,s->stream.sequence,0,
            false,true,s->last_disarm_status,s->last_disarm_reason};
        d->observed_disarms=s->disarm_count;
    }
    if(available&&!d->dynamic_available) {
        d->last_missing_interval_us=now-d->missing_start_us;
        events[n++]=(ci_model_event){now,s->disarm_count,s->stream.session,s->stream.sequence,
            d->last_missing_interval_us,true,s->disarm_count>0,s->status,"resultado dinamico recuperado"};
    } else if(!available&&d->dynamic_available) d->missing_start_us=now;
    d->dynamic_available=available;d->screen_available=o.mode!=CC_NO_DATA;d->last_tick_us=now;
    return n;
}
void ci_diag_event_print(FILE *file,ci_model_event e) {
    fprintf(file,"MODEL_EVENT|event_local_us=%" PRIu64 "|kind=%s|disarm_id=%" PRIu64
        "|session=%" PRIu64 "|tx_seq=%" PRIu64 "|status=%s|cause=%s|missing_interval_us=%" PRIu64
        "|operator_required=%u\n",e.event_local_us,e.recovered?"RECOVER":"DISARM",e.id,e.session,e.sequence,
        ma_status_name(e.status),e.reason?e.reason:"unknown",e.missing_interval_us,(unsigned)e.operator_required);
}
void ci_diag_print(FILE *file,const ci_diagnostics *d,const cc_config *c,const cc_state *s,cc_output o,uint64_t now) {
    fprintf(file,"BENCH|evaluation_local_us=%" PRIu64 "|rx_local_us=%" PRIu64 "|session=%" PRIu32
        "|tx_seq=%" PRIu32 "|source_sent_us=%" PRIu64 "|packets=%" PRIu64 "|packet_gaps=%" PRIu64
        "|duplicates=%" PRIu64 "|restarts=%" PRIu64 "|rejected=%" PRIu64 "|bad_wire=%" PRIu64
        "|have_frame=%u|simulation=%u|latency_bound_known=%u|latency_bound_us=%" PRIu64 "\n",
        now,d->received_local_us,d->frame.session,d->frame.sequence,d->frame.sent_us,d->packets,d->gaps,
        d->duplicates,d->restarts,d->rejected,d->bad_wire,(unsigned)d->have_frame,(unsigned)c->simulation,
        (unsigned)c->latency_bound_known,c->latency_bound_us);
    static const char *names[]={"accel","gyro","orientation","water"};
    for(unsigned k=0;k<4;k++) {
        ci_report_diagnostic r=ci_diag_report(&d->frame,k,d->received_local_us,now,c);
        fprintf(file,"REAR_REPORT|kind=%s|source_us=%" PRIu64 "|seq=%" PRIu32
            "|present=%u|age_known=%u|partial_age_us=%" PRIu64 "|fresh_status=%s"
            "|repeated=%" PRIu64 "|lost=%" PRIu64 "|backwards=%" PRIu64 "\n",names[k],stamp(&d->frame,k),
            sequence(&d->frame,k),(unsigned)r.present,(unsigned)r.age_known,r.partial_age_us,ma_status_name(r.status),
            d->repeats[k],d->lost[k],d->backwards[k]);
    }
    const ct_frame *f=&d->frame;
    fprintf(file,"REAR_VALUES|accel_m_s2=%.6f,%.6f,%.6f|gyro_rad_s=%.6f,%.6f,%.6f"
        "|q_wxyz=%.7f,%.7f,%.7f,%.7f|q_convention=SH2_GAME_RV_sensor_to_reference"
        "|axes=raw_sensor|quality=%u,%u,%u|water_value=%.6f|water_unit=%s|water_unit_enum=%u|register=0x%04x"
        "|modbus_detail=NOT_TRANSMITTED_CST2_SEE_REAR_MB\n",
        (double)f->acceleration[0],(double)f->acceleration[1],(double)f->acceleration[2],
        (double)f->gyro[0],(double)f->gyro[1],(double)f->gyro[2],(double)f->quaternion[0],
        (double)f->quaternion[1],(double)f->quaternion[2],(double)f->quaternion[3],
        (unsigned)f->accel_quality,(unsigned)f->gyro_quality,(unsigned)f->orientation_quality,
        (double)f->water,f->water_quantity==CT_RAW_REGISTER?"RAW_COUNTS":
        f->water_quantity==CT_DEPTH_M?"DEPTH_M":"GAUGE_KPA",(unsigned)f->water_quantity,(unsigned)f->water_register);
    bool water_age_known=!c->virtual_profile&&s->have_water&&now>=s->water_updated_local;
    uint64_t water_age=water_age_known?now-s->water_updated_local:0;
    if(c->virtual_profile) fprintf(file,"SCENARIO|profile=PIPA_VIRTUAL_ENTRADAS_REALES|water=CONFIGURED_SIMULATED|acquisition_time=NONE|sensor_timeout=NOT_APPLICABLE|fill_volume_percent=%.6f|ltr_estimated=%.6f\n",c->scenario_water.volume_percent,o.ltr.signed_value);
    fprintf(file,"MODEL_HEALTH|status=%s|mode=%u|reason=%s|armed=%u|dynamic_status=%s|dynamic_reason=%s|disarms=%" PRIu64
        "|last_disarm=%s|no_dynamic_total_us=%" PRIu64 "|no_screen_total_us=%" PRIu64
        "|current_missing_us=%" PRIu64 "|operator_required=%u|event_drops=%" PRIu64
        "|water_have=%u|water_estimate_local_us=%" PRIu64 "|water_hold_us=%" PRIu64
        "|water_age_known=%u|water_estimate_age_us=%" PRIu64 "|volume_age_within_hold=%u"
        "|configured=%u|mounting=%u|rigid=%u|position=%u|water_calibration=%u|mass_complete=%u|track_justified=%u\n",
        ma_status_name(o.status),(unsigned)o.mode,o.reason,(unsigned)s->armed,ma_status_name(s->status),
        s->dynamic_reason?s->dynamic_reason:"unknown",s->disarm_count,
        s->last_disarm_reason?s->last_disarm_reason:"none",d->no_dynamic_us,d->no_screen_us,
        d->dynamic_available?0:now-d->missing_start_us,(unsigned)(!s->armed&&s->disarm_count>0),d->event_drops,
        (unsigned)s->have_water,s->water_updated_local,c->water_hold_us,(unsigned)water_age_known,water_age,
        (unsigned)(water_age_known&&water_age<=c->water_hold_us),(unsigned)c->configured,
        (unsigned)c->mounting_known,(unsigned)c->rigid_assumed,(unsigned)c->position_accepted,
        (unsigned)c->water_calibration_known,(unsigned)c->mass_accounting_complete,(unsigned)c->track_justified);
    fprintf(file,"TIMING_PROBE|scope=SOURCE_STRUCTURE_ONLY_NOT_MODEL|pairs=%" PRIu64
        "|waits=%" PRIu64 "|breaks=%" PRIu64 "|reason=%s|mounting_known=%u|latency_known=%u\n",
        d->timing_pairs,d->timing_waits,d->timing_breaks,d->timing_reason?d->timing_reason:"no_frame",
        (unsigned)c->mounting_known,(unsigned)c->latency_bound_known);
}
