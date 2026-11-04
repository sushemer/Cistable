#ifndef CI_DIAGNOSTICO_H
#define CI_DIAGNOSTICO_H
#include "control_cabina.h"
#include <stdio.h>
typedef struct {
    uint64_t event_local_us,id,session,sequence,missing_interval_us;
    bool recovered,operator_required;ma_status status;const char *reason;
} ci_model_event;
typedef struct {
    ct_stream stream;ct_frame frame;bool have_frame;
    uint64_t received_local_us,packets,duplicates,restarts,rejected,bad_wire,legacy_wire;
    uint64_t gaps,repeats[4],lost[4],backwards[4];
    uint32_t report_seq[4];uint64_t report_time[4];bool report_seen[4];
    uint64_t timing_pairs,timing_waits,timing_breaks;
    uint32_t pair_a,pair_g;uint64_t pair_at,pair_gt;bool pair_initialized;
    const char *timing_reason;
    bool tick_initialized,dynamic_available,screen_available;
    uint64_t last_tick_us,missing_start_us,no_dynamic_us,no_screen_us,last_missing_interval_us;
    uint64_t observed_disarms,event_drops;
} ci_diagnostics;
typedef struct {bool present,age_known;uint64_t partial_age_us;ma_status status;} ci_report_diagnostic;
void ci_diag_receive(ci_diagnostics *d,const ct_frame *f,uint64_t rx,const cc_config *c);
ci_report_diagnostic ci_diag_report(const ct_frame *f,unsigned report,uint64_t rx,uint64_t now,const cc_config *c);
/* Called every control iteration, not just on the rate-limited print path. */
unsigned ci_diag_observe(ci_diagnostics *d,const cc_state *s,cc_output o,uint64_t now,ci_model_event events[2]);
void ci_diag_print(FILE *file,const ci_diagnostics *d,const cc_config *c,const cc_state *s,cc_output o,uint64_t now);
void ci_diag_event_print(FILE *file,ci_model_event event);
#endif
