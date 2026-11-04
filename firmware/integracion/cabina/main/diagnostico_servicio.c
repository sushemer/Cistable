#include "diagnostico_servicio.h"
#include "field_espnow.h"
#include "modules/gps/gps_module.h"
#include "modules/imu/imu_module.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include <inttypes.h>
typedef struct {ci_diagnostics d;cc_config c;cc_state s;cc_output o;uint64_t now;} snapshot;
static QueueHandle_t snapshots,events;
static uint64_t period_us,last_publish;
static snapshot published; /* single producer; keep out of app_main's stack */
static void logger(void *unused) {
    (void)unused;static snapshot v;
    for(;;) {
        if(xQueueReceive(snapshots,&v,pdMS_TO_TICKS(20))==pdTRUE) {
            uint64_t printed=esp_timer_get_time();
            printf("DIAG_PRINT|print_local_us=%" PRIu64 "|evaluation_local_us=%" PRIu64 "|queue_age_us=%" PRIu64 "\n",
                printed,v.now,printed>=v.now?printed-v.now:0);
            ci_diag_print(stdout,&v.d,&v.c,&v.s,v.o,v.now);
            field_espnow_stats radio={0};field_espnow_get_stats(&radio);
            printf("RADIO|role=CABINA|rx_callbacks=%" PRIu64 "|rx_delivered=%" PRIu64 "|rx_skipped=%" PRIu64
                "|tx_submitted=%" PRIu64 "|tx_completed=%" PRIu64 "|tx_failed=%" PRIu64 "\n",
                radio.rx_callbacks,radio.rx_delivered,radio.rx_skipped,radio.tx_submitted,radio.tx_completed,radio.tx_failed);
            imu_bno085_sample_t imu={0};bool online=imu_module_get_sample(&imu);
            printf("MASTERDATA|3|log_local_us=%" PRIu64 "|rx_local_us=%" PRIu64
                "|rx_count=%" PRIu64 "|legacy_count=%" PRIu64 "|source_session=%" PRIu32
                "|source_tx_sequence=%" PRIu32 "|lost_packets=%" PRIu64
                "|lost_accel_reports=%" PRIu64 "|lost_gyro_reports=%" PRIu64
                "|armed=%u|model_source_us=%" PRIu64 "|dynamic_status=%s|status=%s"
                "|ltr_signed=%.5f|ltr_status=%s|reason=%s|cabin_online=%u\n",
                v.now,v.d.received_local_us,v.d.packets+v.d.bad_wire,v.d.legacy_wire,
                v.s.stream.session,v.s.stream.sequence,v.s.lost_packets,v.s.lost_accel_reports,
                v.s.lost_gyro_reports,(unsigned)v.s.armed,v.s.last_integrated_us,ma_status_name(v.s.status),
                ma_status_name(v.o.status),v.o.ltr.signed_value,ma_status_name(v.o.ltr.status),v.o.reason,(unsigned)online);
            gps_module_snapshot_t gps={0};bool have=gps_module_get_snapshot(&gps);
            printf("GPS_DIAG|have=%u|rmc_local_us=%" PRId64 "|gga_local_us=%" PRId64
                "|rmc_age_ms=%" PRIu32 "|gga_age_ms=%" PRIu32 "|epoch_skew_ms=%" PRIu32
                "|state=%s|usable_speed=%u|speed_kmh=%.4f|satellites=%u|hdop=%.3f|reason=%s\n",
                (unsigned)have,gps.sample.rmc_received_us,gps.sample.gga_received_us,
                gps.validation.rmc_age_ms,gps.validation.gga_age_ms,gps.validation.epoch_skew_ms,
                gps_validation_state_name(gps.validation.state),(unsigned)(have&&gps.validation.usable_for_speed&&gps.speed_valid),
                (double)gps.displayed_speed_kmh,(unsigned)gps.sample.satellites,(double)gps.sample.hdop,
                gps.validation.reason?gps.validation.reason:"no_snapshot");
            printf("CABIN_IMU|online=%u|accel_local_us=%" PRIu64 "|gyro_local_us=%" PRIu64
                "|orientation_local_us=%" PRIu64 "|accel_m_s2=%.5f,%.5f,%.5f|gyro_rad_s=%.5f,%.5f,%.5f"
                "|q_wxyz=%.6f,%.6f,%.6f,%.6f|scope=DIAGNOSTIC_ONLY\n",
                (unsigned)online,imu.acceleration_time_us,imu.gyro_time_us,imu.orientation_time_us,
                (double)imu.linear_accel_m_s2[0],(double)imu.linear_accel_m_s2[1],(double)imu.linear_accel_m_s2[2],
                (double)imu.gyro_rad_s[0],(double)imu.gyro_rad_s[1],(double)imu.gyro_rad_s[2],
                (double)imu.quaternion_wxyz[0],(double)imu.quaternion_wxyz[1],(double)imu.quaternion_wxyz[2],(double)imu.quaternion_wxyz[3]);
            printf("MEM|free_internal=%u|largest_dma=%u\n",(unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
        }
        ci_model_event event;
        for(unsigned k=0;k<32&&xQueueReceive(events,&event,0)==pdTRUE;k++) ci_diag_event_print(stdout,event);
    }
}
esp_err_t ci_diag_service_start(uint32_t period_ms) {
    if(!period_ms) return ESP_OK;
#ifdef CONFIG_CI_FULL_CAPTURE
    if(period_ms<1000) period_ms=1000; /* preserve full-capture serial budget */
#endif
    period_us=(uint64_t)(period_ms<100?100:period_ms)*1000;
    snapshots=xQueueCreate(1,sizeof(snapshot));events=xQueueCreate(32,sizeof(ci_model_event));
    if(!snapshots||!events) return ESP_ERR_NO_MEM;
    return xTaskCreate(logger,"bench_diag",8192,NULL,1,NULL)==pdPASS?ESP_OK:ESP_ERR_NO_MEM;
}
void ci_diag_service_tick(ci_diagnostics *d,const cc_config *c,const cc_state *s,cc_output o,uint64_t now) {
    ci_model_event list[2];unsigned n=ci_diag_observe(d,s,o,now,list);
    if(!period_us) return;
    for(unsigned k=0;k<n;k++) if(xQueueSend(events,&list[k],0)!=pdTRUE) ++d->event_drops;
    if(now-last_publish<period_us) return;
    published=(snapshot){*d,*c,*s,o,now};
    xQueueOverwrite(snapshots,&published);last_publish=now;
}
