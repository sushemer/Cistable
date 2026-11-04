#include "captura_runtime.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "driver/uart_vfs.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_psram.h"
#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include <stdio.h>
#include <string.h>
typedef struct {uint64_t time;uint32_t id;uint16_t n;uint8_t kind,flags,payload[CAP_PAYLOAD_MAX];} item;
static QueueHandle_t queue;
static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
static uint64_t attempted,written,dropped,high_water;
static uint32_t next_id;
const char *cap_console_name(void) {
#ifdef CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    return "USB_SERIAL_JTAG";
#else
    return "UART0_TX43_RX44";
#endif
}
unsigned cap_console_baud(void) {
#ifdef CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    return 0;
#else
    return CONFIG_ESP_CONSOLE_UART_BAUDRATE;
#endif
}
static void emit(uint8_t kind,uint8_t flags,uint64_t time,const uint8_t *p,size_t n) {
    if(!queue) return;
    item v={.time=time,.n=(uint16_t)n,.kind=kind,.flags=flags};
    portENTER_CRITICAL(&mux);v.id=++next_id;++attempted;portEXIT_CRITICAL(&mux);
    if(n>CAP_PAYLOAD_MAX) {portENTER_CRITICAL(&mux);++dropped;portEXIT_CRITICAL(&mux);return;}
    if(n) memcpy(v.payload,p,n);
    if(xQueueSend(queue,&v,0)!=pdTRUE) {portENTER_CRITICAL(&mux);++dropped;portEXIT_CRITICAL(&mux);}
    else {uint64_t used=uxQueueMessagesWaiting(queue);portENTER_CRITICAL(&mux);if(used>high_water) high_water=used;portEXIT_CRITICAL(&mux);}
}
static void writer(void *unused) {
    (void)unused;item v;uint8_t wire[CAP_FRAME_MAX];uint64_t last=0;
    for(;;) {
        if(xQueueReceive(queue,&v,pdMS_TO_TICKS(20))==pdTRUE) {
            size_t n=cap_encode(wire,v.kind,v.flags,v.id,v.time,v.payload,v.n);
            size_t count=fwrite(wire,1,n,stdout);
            portENTER_CRITICAL(&mux);if(count==n) ++written;else ++dropped;portEXIT_CRITICAL(&mux);
        }
        uint64_t now=esp_timer_get_time();
        if(now-last>=1000000) {
            uint8_t p[112];portENTER_CRITICAL(&mux);
            cap_u64(p,attempted,8);cap_u64(p+8,written,8);cap_u64(p+16,dropped,8);cap_u64(p+24,high_water,8);
            portEXIT_CRITICAL(&mux);cap_u64(p+32,uxQueueMessagesWaiting(queue),8);
            field_espnow_stats radio;field_espnow_get_stats(&radio);
            cap_u64(p+40,radio.rx_callbacks,8);cap_u64(p+48,radio.rx_delivered,8);cap_u64(p+56,radio.rx_skipped,8);
            cap_u64(p+64,radio.rx_rejected_input,8);cap_u64(p+72,radio.tx_submitted,8);cap_u64(p+80,radio.tx_completed,8);
            cap_u64(p+88,radio.tx_failed,8);cap_u64(p+96,radio.tx_busy,8);cap_u64(p+104,radio.sending,8);
            emit(CAP_STATS,0,now,p,sizeof(p));last=now;
        }
    }
}
esp_err_t cap_start(bool enabled,const char *role,unsigned rate) {
    if(!enabled) return ESP_OK;
#ifdef CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    usb_serial_jtag_vfs_set_tx_line_endings(ESP_LINE_ENDINGS_LF);
#else
    if(cap_console_baud()<921600) return ESP_ERR_INVALID_ARG;
    uart_vfs_dev_port_set_tx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM,ESP_LINE_ENDINGS_LF); /* binary 0x0a */
#endif
    setvbuf(stdout,NULL,_IONBF,0);
    queue=xQueueCreate(32,sizeof(item));if(!queue) return ESP_ERR_NO_MEM;
    char text[CAP_PAYLOAD_MAX];int n=snprintf(text,sizeof(text),
        "schema=1 role=%s rate_target_hz=%u transport=%s baud=%u endian=little f64=IEEE754 source_time=driver_observation mounting=PENDING old_water=SIMULATED old_gps=HARDWARE_FAULT",
        role,rate,cap_console_name(),cap_console_baud());
    if(n<0||(size_t)n>=sizeof(text)) return ESP_ERR_INVALID_SIZE;
    emit(CAP_BOOT,0,esp_timer_get_time(),(const uint8_t *)text,(size_t)n);
    return xTaskCreate(writer,"capture",4096,NULL,1,NULL)==pdPASS?ESP_OK:ESP_ERR_NO_MEM;
}
void cap_rx(const field_espnow_rx_t *r) {
    if(!queue) return;
    uint8_t p[CAP_PAYLOAD_MAX];if(r->data_len>250) return;
    cap_u64(p,r->receive_id,8);memcpy(p+8,r->source_mac,6);p[14]=(uint8_t)r->rssi_dbm;p[15]=r->rssi_valid;
    memcpy(p+16,r->raw_data,r->data_len);emit(CAP_RX,0,r->received_us,p,16+r->data_len);
}
void cap_metadata(const char *text) {
    if(text) emit(CAP_CONFIG,0,esp_timer_get_time(),(const uint8_t *)text,strlen(text));
}
void cap_board_report(const char *role) {
    esp_chip_info_t chip;esp_chip_info(&chip);uint32_t physical=0,usable=0;
    esp_err_t err=esp_flash_get_physical_size(esp_flash_default_chip,&physical);
    esp_flash_get_size(esp_flash_default_chip,&usable);
    char s[272];
    snprintf(s,sizeof(s),"HW_EXPECTED|role=%s|module_advertised=ESP32-S3-WROOM-1-N16R8|flash_bytes=16777216|flash=QUAD_QIO_80MHz|psram_bytes=8388608|psram=OCTAL_80MHz|board_revision=UNCONFIRMED",role);
    puts(s);cap_metadata(s);
    snprintf(s,sizeof(s),"HW_CONFIG|target=%s|flash_size=%s|flash_mode=%s|flash_frequency=%s|psram_oct_requested=%u|psram_memtest_requested=%u|normal_malloc_stays_internal=1",
        CONFIG_IDF_TARGET,CONFIG_ESPTOOLPY_FLASHSIZE,CONFIG_ESPTOOLPY_FLASHMODE,CONFIG_ESPTOOLPY_FLASHFREQ,
#ifdef CONFIG_SPIRAM_MODE_OCT
        1u,
#else
        0u,
#endif
#ifdef CONFIG_SPIRAM_MEMTEST
        1u);
#else
        0u);
#endif
    puts(s);cap_metadata(s);
    snprintf(s,sizeof(s),"HW_OBSERVED|chip_model=%d|cores=%d|revision=%d|flash_physical_bytes=%lu|flash_api=%s|flash_usable_bytes=%lu|psram_initialized=%u|psram_bytes=%u",
        (int)chip.model,chip.cores,chip.revision,(unsigned long)physical,esp_err_to_name(err),(unsigned long)usable,(unsigned)esp_psram_is_initialized(),(unsigned)esp_psram_get_size());
    puts(s);cap_metadata(s);
    snprintf(s,sizeof(s),"HW_MEMORY|internal_free=%u|dma_largest=%u|psram_heap_total=%u|transport=%s|baud=%u|sizes_do_not_identify_module_or_PCB=1",
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA),
        (unsigned)heap_caps_get_total_size(MALLOC_CAP_SPIRAM),cap_console_name(),cap_console_baud());
    puts(s);cap_metadata(s);
}
static void tx_record(uint8_t kind,const uint8_t *data,size_t n,int code) {
    if(!queue) return;
    if(n>250) return;
    uint8_t p[CAP_PAYLOAD_MAX];cap_u64(p,(uint32_t)code,4);if(n) memcpy(p+4,data,n);
    emit(kind,0,esp_timer_get_time(),p,n+4);
}
void cap_tx_done(const uint8_t *data,size_t n,int status) {tx_record(CAP_TX_DONE,data,n,status);}
void cap_tx(const uint8_t *data,size_t n,int result) {tx_record(CAP_TX,data,n,result);}
void cap_process(const field_espnow_rx_t *r,const cc_state *s,cc_output o,uint64_t previous,uint8_t decision,uint8_t conditions,uint64_t now) {
    if(!queue) return;
    static uint64_t last_disarm_count;
    uint8_t p[216]={0};cap_u64(p,r?r->receive_id:0,8);
    cap_u64(p+8,s->stream.session,4);cap_u64(p+12,s->stream.sequence,4);
    cap_u64(p+16,r?r->received_us:0,8);cap_u64(p+24,previous,8);cap_u64(p+32,s->last_integrated_us,8);
    cap_u64(p+40,s->last_gyro_us,8);cap_u64(p+48,s->calculation_expires_local,8);cap_u64(p+56,s->disarm_count,8);
    cap_f64(p+64,s->pendulum.angle_rad);cap_f64(p+72,s->pendulum.rate_radps);cap_f64(p+80,s->ltr.signed_value);
    cap_f64(p+88,s->alpha.filtered_alpha.x);cap_f64(p+96,s->alpha.filtered_alpha.y);cap_f64(p+104,s->alpha.filtered_alpha.z);
    p[112]=(uint8_t)s->status;p[113]=(uint8_t)o.status;p[114]=(uint8_t)o.mode;p[115]=s->armed;
    p[116]=decision;p[117]=conditions;
    bool disarmed=s->disarm_count!=last_disarm_count;last_disarm_count=s->disarm_count;
    const char *cause=disarmed?s->last_disarm_reason:(decision==5?o.reason:(s->dynamic_reason?s->dynamic_reason:o.reason));
    size_t n=cause?strlen(cause):0;p[119]=disarmed?2:0;
    if(n>96) {n=96;p[119]|=1;}
    p[118]=(uint8_t)n;memcpy(p+120,cause?cause:"",n);emit(CAP_PROCESS,0,now,p,120+n);
}
void cap_command(int command,const cc_state *s,uint8_t conditions,uint64_t now) {
    uint8_t p[4]={(uint8_t)command,s->armed,conditions,(uint8_t)s->status};emit(CAP_COMMAND,0,now,p,4);
}
