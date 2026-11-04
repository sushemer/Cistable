#include "field_espnow.h"
#include "control_cabina.h"
#include "pantalla.h"
#include "display.h"
#include "seleccion.h"
#include "demo.h"
#include "configuracion_modelo.h"
#include "presentacion.h"
#include "diagnostico_servicio.h"
#include "captura_runtime.h"
#include "perfil_mesa.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "modules/gps/gps_module.h"
#include "modules/imu/imu_module.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "cableado_cabina.h"
#include <math.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

static const char *TAG="CABINA";
static cc_state model;
static ci_diagnostics diagnostics;
static QueueHandle_t ui_queue;
static cc_config ui_config;
static void config_record(const char *text) {puts(text);cap_metadata(text);}
static bool wiring_valid(void) {
#ifndef CONFIG_IDF_TARGET_ESP32S3
    return false;
#endif
    if((CONFIG_CI_LCD_SPI_HOST!=2&&CONFIG_CI_LCD_SPI_HOST!=3)||
       (CONFIG_CI_IMU_SPI_HOST!=2&&CONFIG_CI_IMU_SPI_HOST!=3)||CI_LCD_HOST==CI_CABIN_IMU_HOST) return false;
#ifndef CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    if(CONFIG_ESP_CONSOLE_UART_NUM!=0) return false;
#ifdef CONFIG_ESP_CONSOLE_UART_TX_GPIO
    if(CONFIG_ESP_CONSOLE_UART_TX_GPIO!=-1&&CONFIG_ESP_CONSOLE_UART_TX_GPIO!=43) return false;
    if(CONFIG_ESP_CONSOLE_UART_RX_GPIO!=-1&&CONFIG_ESP_CONSOLE_UART_RX_GPIO!=44) return false;
#endif
#endif
    int pins[16]={CONFIG_CI_LCD_SCLK,CONFIG_CI_LCD_MOSI,CONFIG_CI_LCD_DC,CONFIG_CI_LCD_RST,CONFIG_CI_LCD_CS};
    unsigned n=5;
    /* A disabled driver does not disconnect its existing physical wires.
     * Reserve the complete configured board map, including the optional button. */
    pins[n++]=CONFIG_CI_BUTTON_GPIO;
    const int imu[]={CONFIG_CI_IMU_MOSI,CONFIG_CI_IMU_MISO,CONFIG_CI_IMU_SCLK,
        CONFIG_CI_IMU_CS,CONFIG_CI_IMU_INT,CONFIG_CI_IMU_RST};
    for(unsigned k=0;k<6;k++) pins[n++]=imu[k];
    pins[n++]=CONFIG_CI_GPS_RX;pins[n++]=CONFIG_CI_GPS_TX;
    for(unsigned i=0;i<n;i++) {
        /* Only module-exposed S3 GPIOs: reserve memory26..37, nonexistent22..25,
         * boot0/3/45/46, USB19/20, UART43/44, possible board LEDs38/48. */
        int p=pins[i];
        bool allowed=p==1||p==2||(p>=4&&p<=18)||p==21||(p>=39&&p<=42)||p==47;
        if(!allowed||!GPIO_IS_VALID_OUTPUT_GPIO(p)) return false;
        for(unsigned j=0;j<i;j++) if(pins[i]==pins[j]) return false;
    }
    return true; /* Module restrictions checked; development PCB still pending. */
}
static estado_sistema_t display_state(cc_output o,bool model_a) {
    return ci_display_state(o,model_a);
}
static void ui_task(void *watermark) {
    lv_obj_t *label=watermark;cc_output output={.status=MA_MISSING,.mode=CC_NO_DATA,.reason="sin muestras"};
    uint64_t last=0;
    for(;;) {
        xQueueReceive(ui_queue,&output,pdMS_TO_TICKS(20));
        bool selected=seleccion_actualizar();uint64_t now=esp_timer_get_time();
        if(!selected||now-last<250000) continue;
        display_lock();
#ifdef CONFIG_CI_DEMO
#ifdef CONFIG_CI_DEMO_EXPIRY
        lv_label_set_text_fmt(label,"DEMO CADUCIDAD | %s",ci_model_notice(output.status));
#else
        lv_label_set_text_fmt(label,"DEMO CONTINUA | %s",ci_model_notice(output.status));
#endif
#elif defined(CONFIG_CI_PIPA_VIRTUAL)
        char text[320],fill[20]; /* libc supports floats; LVGL builtin printf has them OFF */
        if(ui_config.scenario_water.status==MA_SIMULATED&&isfinite(ui_config.scenario_water.volume_percent))
            snprintf(fill,sizeof(fill),"%.0f%%",ui_config.scenario_water.volume_percent);
        else snprintf(fill,sizeof(fill),"--");
        if(seleccion_modelo_a()&&(output.ltr.status==MA_EXPERIMENTAL||output.ltr.status==MA_LTR_LIMIT)&&isfinite(output.ltr.signed_value))
            snprintf(text,sizeof(text),"PIPA VIRTUAL / EXPERIMENTAL\nLTR estimado %+.3f | Agua %s CONFIG.\n%s\nVerde no garantiza seguridad",output.ltr.signed_value,fill,output.reason);
        else snprintf(text,sizeof(text),"PIPA VIRTUAL / EXPERIMENTAL\nLTR -- | Agua %s CONFIGURADA\nSIN RESULTADO: %s",fill,seleccion_modelo_a()?output.reason:"Modelo B sin implementacion");
        lv_label_set_text(label,text);
#else
        lv_label_set_text_fmt(label,"EXPERIMENTAL | %s",ci_model_notice(output.status));
#endif
        display_unlock();
        estado_sistema_t state=display_state(output,seleccion_modelo_a());
        pantalla_mostrar_estado(&state);last=now;
    }
}
void app_main(void) {
    cc_init(&model);cc_config config=ci_live_model_config();
    bool wiring=wiring_valid();
    if(!wiring) {ESP_LOGE(TAG,"WIRING_INVALID: GPIO reservado/conflictivo; no se inicializa ningun periferico. Revisar menuconfig.");return;}
    ESP_ERROR_CHECK(pantalla_init());seleccion_init();
    display_lock();
    lv_obj_t *label=lv_label_create(lv_layer_top());
#ifdef CONFIG_CI_DEMO
    lv_label_set_text(label,"DEMOSTRACION - DATOS SINTETICOS");
    config=ci_demo_config();
#else
    lv_label_set_text(label,"EXPERIMENTAL - NO VALIDADO");
#endif
    lv_obj_set_style_text_font(label,&lv_font_montserrat_14,0);
    lv_obj_set_style_text_color(label,lv_color_hex(0xFFB000),0);
    lv_obj_set_width(label,470);lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);
    lv_obj_align(label,LV_ALIGN_TOP_MID,0,0);display_unlock();
    ui_config=config;
    ui_queue=xQueueCreate(1,sizeof(cc_output));ESP_ERROR_CHECK(ui_queue?ESP_OK:ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreate(ui_task,"cabina_ui",6144,label,1,NULL)==pdPASS?ESP_OK:ESP_ERR_NO_MEM);
#ifdef CONFIG_CI_FULL_CAPTURE
    ESP_ERROR_CHECK(cap_start(true,"CABINA",CI_RATE_HZ));
#else
    ESP_ERROR_CHECK(cap_start(false,"CABINA",CI_RATE_HZ));
#endif
    cap_board_report("CABINA");
    ci_config_report(&config,config_record);
    char wiring_text[272];
    snprintf(wiring_text,sizeof(wiring_text),"WIRING|board_expected=ESP32_S3_N16R8|LCD_SPI=%d|SCK=%d|MOSI=%d|DC=%d|RST=%d|CS=%d|MISO=NC|touch=OFF|button_gpio=%d",
        CONFIG_CI_LCD_SPI_HOST,CONFIG_CI_LCD_SCLK,CONFIG_CI_LCD_MOSI,CONFIG_CI_LCD_DC,CONFIG_CI_LCD_RST,CONFIG_CI_LCD_CS,CONFIG_CI_BUTTON_GPIO);
    config_record(wiring_text);
    unsigned live_mask=0,button_enabled=0;
#if defined(CONFIG_CI_ENABLE_GPS) && !defined(CONFIG_CI_DEMO)
    live_mask|=1;
#endif
#if defined(CONFIG_CI_ENABLE_CABIN_IMU) && !defined(CONFIG_CI_DEMO)
    live_mask|=2;
#endif
#ifdef CONFIG_CI_ENABLE_BUTTON
    button_enabled=1;
#endif
    snprintf(wiring_text,sizeof(wiring_text),"WIRING_ENABLED|LCD=1|GPS=%u|CABIN_IMU_DIAGNOSTIC=%u|button=%u|console=%s|baud=%u|live_inputs_skipped_in_demo=%u",
        live_mask&1,(live_mask>>1)&1,button_enabled,cap_console_name(),cap_console_baud(),(unsigned)config.simulation);
    config_record(wiring_text);
    snprintf(wiring_text,sizeof(wiring_text),"WIRING|IMU_SPI=%d|MOSI=%d|MISO=%d|SCK=%d|CS=%d|INT=%d|RST=%d|GPS=UART2_RX%d_TX%d|physical_check=PENDING|demo_skips_live_peripherals=%u",
        CONFIG_CI_IMU_SPI_HOST,CONFIG_CI_IMU_MOSI,CONFIG_CI_IMU_MISO,CONFIG_CI_IMU_SCLK,CONFIG_CI_IMU_CS,CONFIG_CI_IMU_INT,CONFIG_CI_IMU_RST,CONFIG_CI_GPS_RX,CONFIG_CI_GPS_TX,(unsigned)config.simulation);
    config_record(wiring_text);
    field_espnow_observers(cap_rx,cap_tx_done);
    ESP_ERROR_CHECK(ci_diag_service_start(CONFIG_CI_DIAG_PERIOD_MS));
#ifdef CONFIG_CI_DEMO
#ifdef CONFIG_CI_DEMO_EXPIRY
    ESP_LOGW(TAG,"DEMO_SCENARIO|EXPIRY|synthetic_water_hold_us=%" PRIu64,config.water_hold_us);
#else
    ESP_LOGW(TAG,"DEMO_SCENARIO|CONTINUOUS|synthetic_settled_stop_every_us=12000000");
#endif
#endif
#ifndef CONFIG_CI_DEMO
#ifdef CONFIG_CI_ENABLE_GPS
    if(wiring) gps_module_start();
#endif
#ifdef CONFIG_CI_ENABLE_CABIN_IMU
    if(wiring&&!imu_module_start()) ESP_LOGE(TAG,"IMU cabina no disponible");
#endif
    ESP_ERROR_CHECK(field_espnow_init(1)); /* The only radio init and RX callback. */
#endif
    ESP_LOGW(TAG,"ESP32-S3 N16R8 esperado, placa sin verificar: IMU/GPS/LCD independientes. Boton opcional GPIO%d; deshabilitado implica Modelo A.",CONFIG_CI_BUTTON_GPIO);
    int input_flags=fcntl(STDIN_FILENO,F_GETFL,0);
    if(input_flags>=0) fcntl(STDIN_FILENO,F_SETFL,input_flags|O_NONBLOCK);
    bool settled_confirmed=false,no_flow_confirmed=false;
    ESP_LOGW(TAG,"Operador: s=confirmar reposo asentado; f=sin llenado/descarga; u=invalidar volumen; a=armar equilibrio experimental");
    uint64_t last_ui=0,last_log=0,rx_time=0,demo_time=10000;
    uint32_t receptions=0,legacy_count=0;bool demo_armed=false;
    cc_output output={.status=MA_MISSING,.mode=CC_NO_DATA,.reason="sin muestras"};
    cc_conditions conditions={0};
    while(true) {
        uint64_t now=esp_timer_get_time();
        uint64_t previous=model.last_integrated_us,disarms_before=model.disarm_count;
        ma_status status_before=model.status;bool processed_frame=false;
        ma_status output_before=output.status;cc_display_mode mode_before=output.mode;
#ifdef CONFIG_CI_DEMO
#ifdef CONFIG_CI_DEMO_EXPIRY
        ct_frame frame=ci_demo_frame(demo_time);
        conditions=ci_demo_conditions(demo_time);rx_time=now;
#else
        ct_frame frame=ci_demo_visual_frame(demo_time);
        conditions=ci_demo_visual_conditions(demo_time);rx_time=now;
#endif
        if(!demo_armed&&model.stream.initialized) {
            demo_armed=cc_arm(&model,(ma_pendulum_state){MA_SIMULATED,0,0});
        }
        ci_diag_receive(&diagnostics,&frame,now,&config);
        output=cc_process(&model,&config,&frame,now,now,conditions);
        cap_process(NULL,&model,output,previous,model.last_integrated_us!=previous?
            (model.status==MA_MISSING&&model.armed?1:2):0,8,now);
        processed_frame=true;
        demo_time+=10000;
#else
        (void)demo_time;(void)demo_armed;
        gps_module_snapshot_t gps={0};
        conditions.speed_valid=gps_module_get_snapshot(&gps)&&gps.validation.usable_for_speed&&gps.speed_valid;
        conditions.speed_kmh=conditions.speed_valid?gps.displayed_speed_kmh:NAN;
        /* Deliberately pending: GPS zero alone cannot prove settled/level or no flow. */
        int command=getchar();
        if(command==EOF) clearerr(stdin);
        if(!conditions.speed_valid||conditions.speed_kmh>config.stopped_kmh) settled_confirmed=false;
        if(command=='s'&&conditions.speed_valid&&conditions.speed_kmh<=config.stopped_kmh) settled_confirmed=true;
        if(command=='f') no_flow_confirmed=true;
        if(command=='u') {no_flow_confirmed=false;cc_invalidate_water(&model);}
        conditions.stationary_settled_confirmed=settled_confirmed;
        conditions.no_filling_or_draining_confirmed=no_flow_confirmed;
        if(command=='a'&&config.configured&&config.mounting_known&&settled_confirmed&&model.have_water&&no_flow_confirmed&&
            (!config.virtual_profile||(model.arm_reference_level&&now<=model.arm_reference_expires_local))) {
            bool armed=cc_arm(&model,(ma_pendulum_state){MA_EXPERIMENTAL,0,0});
            ESP_LOGW(TAG,"Equilibrio theta=0/rate=0 supuesto expresamente por operador; armado=%u",(unsigned)armed);
        }
        if(command=='f'||command=='s'||command=='a'||command=='u')
            ESP_LOGW(TAG,"OPERATOR|command=%c|settled=%u|no_flow=%u|configured=%u|armed=%u|classification=EXPERIMENTAL",
                command,(unsigned)settled_confirmed,(unsigned)no_flow_confirmed,(unsigned)config.configured,(unsigned)model.armed);
        if(command=='f'||command=='s'||command=='a'||command=='u')
            cap_command(command,&model,(uint8_t)(conditions.speed_valid|settled_confirmed<<1|no_flow_confirmed<<2),now);
        if(command=='a'&&!model.armed)
            ESP_LOGW(TAG,"ARM_REJECT|configured=%u|mounting=%u|GPS_rest_settled=%u|water=%u|no_flow=%u|level_reference_fresh=%u",
                (unsigned)config.configured,(unsigned)config.mounting_known,(unsigned)settled_confirmed,
                (unsigned)model.have_water,(unsigned)no_flow_confirmed,
                (unsigned)(model.arm_reference_level&&now<=model.arm_reference_expires_local));
        field_espnow_rx_t received;
        bool accepted=false;
        if(field_espnow_receive(&received,0)) {
            now=esp_timer_get_time();
            ct_frame frame;
            ++receptions;
            bool duplicate=false;
            if(ct_decode(received.raw_data,received.data_len,&frame)) {
                rx_time=received.received_us;
                ci_diag_receive(&diagnostics,&frame,rx_time,&config);
                duplicate=model.stream.initialized&&model.stream.session==frame.session&&model.stream.sequence==frame.sequence;
                output=cc_process(&model,&config,&frame,rx_time,now,conditions);accepted=true;
                if(frame.sequence%10==0) {
                    uint8_t ack[CT_ACK_SIZE];
                    ct_ack_encode(frame.session,frame.sequence,frame.sent_us,ack);
                    field_espnow_try_send(ack,sizeof(ack));
                }
            } else {
                ++diagnostics.bad_wire;
                if(received.data_len==17) {
                ++diagnostics.legacy_wire;
                /* Legacy: copy for diagnostics only, never synthesize missing XYZ/gyro/time. */
                float values[4];memcpy(values,received.raw_data,16);++legacy_count;
                if(CONFIG_CI_DIAG_PERIOD_MS&&now-last_log>=(uint64_t)CONFIG_CI_DIAG_PERIOD_MS*1000) {
                    ESP_LOGI(TAG,
                        "LEGACY|rx_local_us=%" PRIu64 "|source_sample_time=UNKNOWN|roll_deg=%.3f|pitch_deg=%.3f|ay=%.3f|water_raw=%.3f|flags=%u|rx_count=%" PRIu32 "|legacy_count=%" PRIu32,
                        received.received_us,(double)values[0],(double)values[1],(double)values[2],(double)values[3],(unsigned)received.raw_data[16],receptions,legacy_count);
                    last_log=now;
                }
                }
            }
            if(!accepted) output=cc_current(&model,&config,now,rx_time,conditions);
            uint8_t decision=!accepted?4:duplicate?3:model.last_integrated_us!=previous&&model.last_integrated_us?
                (model.status==MA_MISSING&&model.armed?1:2):0;
            cap_process(&received,&model,output,previous,decision,
                (uint8_t)(conditions.speed_valid|settled_confirmed<<1|no_flow_confirmed<<2),now);
            processed_frame=true;
        }
        if(!accepted&&!processed_frame) output=cc_current(&model,&config,now,rx_time,conditions);
#endif
        if(!processed_frame&&(model.disarm_count!=disarms_before||model.status!=status_before||
            output.status!=output_before||output.mode!=mode_before))
            cap_process(NULL,&model,output,previous,5,
                (uint8_t)(conditions.speed_valid|conditions.stationary_settled_confirmed<<1|conditions.no_filling_or_draining_confirmed<<2),now);
        (void)settled_confirmed;(void)no_flow_confirmed;(void)receptions;(void)legacy_count;(void)last_log;(void)rx_time;
        ci_diag_service_tick(&diagnostics,&config,&model,output,now);
        if(now-last_ui>=250000) {xQueueOverwrite(ui_queue,&output);last_ui=now;}
#ifdef CONFIG_CI_DEMO
        vTaskDelay(pdMS_TO_TICKS(10));
#else
        vTaskDelay(pdMS_TO_TICKS(1));
#endif
    }
}
