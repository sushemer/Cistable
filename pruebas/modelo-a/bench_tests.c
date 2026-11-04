#include "diagnostico.h"
#include "demo.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static cc_conditions stop(void) {return (cc_conditions){true,0,true,true};}
static cc_conditions move(void) {return (cc_conditions){true,10,false,true};}
static void water_expiry(void) {
    cc_config c=ci_demo_config();c.water_hold_us=cc_pending_config().water_hold_us;
    cc_state s;cc_init(&s);ci_diagnostics d={0};ci_model_event events[2];cc_output o={0};
    for(uint64_t t=10000;t<=2070000;t+=10000) {
        ct_frame f=ci_demo_frame(t);ci_diag_receive(&d,&f,t,&c);
        o=cc_process(&s,&c,&f,t,t,t<=30000?stop():move());
        ci_diag_observe(&d,&s,o,t,events);
        if(t==10000) assert(cc_arm(&s,(ma_pendulum_state){MA_SIMULATED,0,0}));
        if(t==2030000) assert(s.armed&&o.mode==CC_MOVING); /* exact retention boundary */
        if(t==2040000) {
            assert(!s.armed&&o.mode==CC_NO_DATA&&s.disarm_count==1);
            assert(strstr(s.last_disarm_reason,"volumen retenido"));
            assert(d.dynamic_available==false);
            ci_report_diagnostic r=ci_diag_report(&f,3,t,t,&c);
            assert(r.status==MA_EXPERIMENTAL&&r.partial_age_us==0); /* fresh raw != current volume */
        }
    }
    assert(s.disarm_count==1&&d.no_dynamic_us>=30000);
    ct_frame f=ci_demo_frame(2080000);o=cc_process(&s,&c,&f,2080000,2080000,stop());
    assert(o.mode==CC_LEVEL&&!s.armed); /* fresh settled estimate does NOT rearm */
    assert(cc_arm(&s,(ma_pendulum_state){MA_SIMULATED,0,0}));
    f=ci_demo_frame(2090000);o=cc_process(&s,&c,&f,2090000,2090000,move());
    ci_diag_observe(&d,&s,o,2090000,events);
    f=ci_demo_frame(2100000);o=cc_process(&s,&c,&f,2100000,2100000,move());
    unsigned n=ci_diag_observe(&d,&s,o,2100000,events);
    assert(n==1&&events[0].recovered&&events[0].operator_required&&events[0].missing_interval_us==60000);
    cc_invalidate_water(&s);assert(s.disarm_count==2&&!s.have_water&&!s.armed);
    ci_diag_observe(&d,&s,o,2110000,events);
    assert(strstr(events[0].reason,"operador"));
    cc_invalidate_water(&s);assert(s.disarm_count==2); /* rejection while already disarmed isn't a new event */
    puts("PASS bench: continuous real-policy water expiry despite fresh raw, explicit recovery and disarm causes");
}
static void diagnostics(void) {
    ci_diagnostics d={0};cc_config c=cc_pending_config();ct_frame f=ci_demo_frame(10000);
    ci_diag_receive(&d,&f,900000,&c);
    ci_report_diagnostic r=ci_diag_report(&f,0,900000,900010,&c);
    assert(r.age_known&&r.partial_age_us==10&&r.status==MA_INCOMPLETE); /* unsynced clocks, unknown latency */
    c.latency_bound_known=true;c.latency_bound_us=20;
    assert(ci_diag_report(&f,0,900000,900010,&c).status==MA_EXPERIMENTAL);
    assert(ci_diag_report(&f,0,900000,1000001,&c).status==MA_STALE);
    assert(ci_diag_report(&f,0,900000,899999,&c).status==MA_INVALID);
    ci_diag_receive(&d,&f,900100,&c);assert(d.duplicates==1&&d.received_local_us==900000);
    f=ci_demo_frame(20000);f.sequence=3;ci_diag_receive(&d,&f,910000,&c);
    assert(d.gaps==1&&d.timing_pairs==1); /* packet gap != report gap */
    f=ci_demo_frame(40000);f.sequence=4;ci_diag_receive(&d,&f,920000,&c);
    assert(d.lost[0]==1&&d.lost[1]==1&&d.timing_breaks==1);
    f.session++;f.sequence=0;ci_diag_receive(&d,&f,930000,&c);assert(d.restarts==1);
    f.sequence=1;f.sent_us=50000;ci_diag_receive(&d,&f,940000,&c);assert(d.repeats[0]==1);
    f.sequence=0;ci_diag_receive(&d,&f,950000,&c);assert(d.rejected==1);
    f.flags=0;assert(ci_diag_report(&f,0,1,1,&c).status==MA_MISSING);
    cc_state s;cc_init(&s);cc_output o={.status=MA_INCOMPLETE,.reason="configuracion dinamica pendiente"};
    /* MinGW tmpfile() may target an unwritable Windows directory. Keep the
     * formatter fixture in the runner's existing workspace build directory. */
    FILE *file=fopen("pruebas/modelo-a/build/diagnostic-sample.txt","w+");assert(file);
    ci_diag_print(file,&d,&c,&s,o,950000);
    ci_diag_event_print(file,(ci_model_event){.reason="test cause",.operator_required=true});
    rewind(file);char text[8192];size_t count=fread(text,1,sizeof(text)-1,file);text[count]=0;fclose(file);
    assert(strstr(text,"partial_age_us=")&&strstr(text,"NOT_TRANSMITTED_CST2")&&strstr(text,"accel_m_s2="));
    assert(strstr(text,"q_convention=")&&strstr(text,"SOURCE_STRUCTURE_ONLY_NOT_MODEL")&&strstr(text,"cause=test cause"));
    puts("PASS bench: per-report ages/validity, duplicates/restarts/losses, timing probe and structured records");
}
static void demos(void) {
    for(unsigned visual=0;visual<2;visual++) {
        cc_config c=ci_demo_config();cc_state s;cc_init(&s);cc_output o={0};
        for(uint64_t t=10000;t<=75000000;t+=10000) {
            ct_frame f=visual?ci_demo_visual_frame(t):ci_demo_frame(t);
            o=cc_process(&s,&c,&f,t,t,visual?ci_demo_visual_conditions(t):ci_demo_conditions(t));
            if(t==10000) assert(cc_arm(&s,(ma_pendulum_state){MA_SIMULATED,0,0}));
            if(visual&&t>30000) assert(o.mode==CC_LEVEL||o.mode==CC_MOVING);
        }
        assert(visual?(s.armed&&s.disarm_count==0):(!s.armed&&o.mode==CC_NO_DATA&&s.disarm_count==1));
    }
    assert(!cc_pending_config().configured&&cc_pending_config().water_hold_us==2000000);
    puts("PASS bench: visual demo stays available for 75 s; expiry demo deliberately stops; live defaults unchanged");
}
static void experimental_category(void) {
    /* Fictitious test fixture only; verify that complete experimental estimates
     * are accepted without ever being promoted to physical validation. */
    cc_config c=ci_demo_config();c.simulation=false;c.pendulum.status=MA_EXPERIMENTAL;
    cc_state s;cc_init(&s);
    for(uint64_t t=10000;t<=40000;t+=10000) {
        ct_frame f=ci_demo_frame(t);cc_output o=cc_process(&s,&c,&f,t,t,t<=30000?stop():move());
        if(t==10000) assert(cc_arm(&s,(ma_pendulum_state){MA_EXPERIMENTAL,0,0}));
        if(t>=30000) assert(o.status==MA_EXPERIMENTAL&&s.status==MA_EXPERIMENTAL&&s.ltr.status==MA_EXPERIMENTAL);
    }
    puts("PASS bench: complete experimental configuration accepted; classification stays experimental");
}
int main(void) {water_expiry();diagnostics();demos();experimental_category();return 0;}
