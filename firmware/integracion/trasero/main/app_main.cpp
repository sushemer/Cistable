/* Source wiring: Pruebas-Gael/PRUEBA SLAVE; one BNO085 on this board. */
#include "imu_bno085.h"
#include "telemetria.h"
#include "field_espnow.h"
#include "captura_runtime.h"
#include "perfil_mesa.h"
#include "driver/uart.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_log.h"
#include <cstring>
#include <cmath>
#include <inttypes.h>
#include <cstdio>
#include "sdkconfig.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static constexpr uint16_t WATER_REGISTER=0x0004; // Twig agua2; physical scale still unconfirmed.
static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
static uint16_t water_raw;
static uint64_t water_time;
static uint32_t water_sequence;
static bool water_ok;
static ct_modbus_result water_result=CT_MB_PARTIAL;
static uint8_t water_exception;
static uint64_t water_attempt_time,water_successes,water_errors;
static const char *TAG="TRASERO_V2";
struct rear_diagnostic {
    ct_frame frame;uint64_t attempt_us,successes,errors,rtt_us;
    ct_modbus_result result;uint8_t exception;int queue_result;bool encoded,attempted;
};
static QueueHandle_t diagnostic_queue;
static void diagnostic_task(void *) {
    rear_diagnostic d{};
    for(;;) if(xQueueReceive(diagnostic_queue,&d,portMAX_DELAY)==pdTRUE) {
#ifdef CONFIG_CI_REAR_WATER_ENABLED
        const char *result=d.result==CT_MB_OK?"OK":d.result==CT_MB_EXCEPTION?"EXCEPTION":
            d.result==CT_MB_PARTIAL?"TIMEOUT_OR_PARTIAL":"BAD_CRC_ADDRESS_LENGTH_OR_UART";
#else
        const char *result="DISABLED_SENSOR_DISCONNECTED_NO_TRANSACTION";
#endif
        std::printf("V2|session=%" PRIu32 "|tx_seq=%" PRIu32 "|source_sent_us=%" PRIu64
            "|acc_seq=%" PRIu32 "|gyro_seq=%" PRIu32 "|orientation_seq=%" PRIu32
            "|queue_result=%d|encode_ok=%u|last_rtt_us=%" PRIu64 "|attempted=%u|time_kind=CANDIDATE_BUILD\n",
            d.frame.session,d.frame.sequence,d.frame.sent_us,d.frame.accel_sequence,d.frame.gyro_sequence,
            d.frame.orientation_sequence,d.queue_result,unsigned(d.encoded),d.rtt_us,unsigned(d.attempted));
        std::printf("REAR_MB|snapshot_source_us=%" PRIu64 "|transaction_end_source_us=%" PRIu64
            "|last_good_source_us=%" PRIu64 "|water_seq=%" PRIu32 "|register=0x%04x|raw_counts=%.0f"
            "|flag=%u|result=%s|exception=%u|successes=%" PRIu64 "|errors=%" PRIu64
            "|unit=COUNTS_SCALE_UNKNOWN\n",d.frame.sent_us,d.attempt_us,d.frame.water_us,
            d.frame.water_sequence,unsigned(d.frame.water_register),double(d.frame.water),
            unsigned((d.frame.flags&CT_WATER)!=0),result,unsigned(d.exception),d.successes,d.errors);
        field_espnow_stats radio{};field_espnow_get_stats(&radio);
        std::printf("RADIO|role=REAR|target_hz=%u|tx_submitted=%" PRIu64 "|tx_completed=%" PRIu64
            "|tx_failed=%" PRIu64 "|tx_busy=%" PRIu64 "|sending=%u\n",unsigned(CI_RATE_HZ),
            radio.tx_submitted,radio.tx_completed,radio.tx_failed,radio.tx_busy,unsigned(radio.sending));
    }
}

static ct_modbus_result read_water(uint16_t *raw,uint8_t *exception) {
    uint8_t request[8]={1,3,uint8_t(WATER_REGISTER>>8),uint8_t(WATER_REGISTER),0,1,0,0};
    uint16_t crc=ct_crc16(request,6);request[6]=uint8_t(crc);request[7]=uint8_t(crc>>8);
    uart_flush_input(UART_NUM_1);
    if(uart_write_bytes(UART_NUM_1,request,8)!=8) return CT_MB_BAD;
    uint8_t response[7];size_t used=0;
    int64_t deadline=esp_timer_get_time()+120000;
    while(esp_timer_get_time()<deadline) {
        int n=uart_read_bytes(UART_NUM_1,response+used,1,pdMS_TO_TICKS(2));
        if(n<=0) continue;
        used+=size_t(n);
        ct_modbus_result result=ct_modbus_decode(response,used,1,raw,exception);
        if(result!=CT_MB_PARTIAL) return result;
        if(used==sizeof(response)) return CT_MB_BAD;
    }
    return CT_MB_PARTIAL; // Explicit timeout or partial response, never last value marked current.
}
static void water_task(void *) {
    for(;;) {
        uint16_t raw=0;uint8_t exception=0;
        ct_modbus_result result=read_water(&raw,&exception);
        portENTER_CRITICAL(&mux);
        water_ok=result==CT_MB_OK;
        water_result=result;water_exception=exception;water_attempt_time=esp_timer_get_time();
        if(water_ok) ++water_successes;else ++water_errors;
        if(water_ok) {water_raw=raw;water_time=esp_timer_get_time();++water_sequence;}
        portEXIT_CRITICAL(&mux);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
extern "C" void app_main(void) {
#ifdef CONFIG_CI_REAR_WATER_ENABLED
    uart_config_t uart{};
    uart.baud_rate=9600;uart.data_bits=UART_DATA_8_BITS;uart.parity=UART_PARITY_DISABLE;
    uart.stop_bits=UART_STOP_BITS_1;uart.flow_ctrl=UART_HW_FLOWCTRL_DISABLE;uart.source_clk=UART_SCLK_DEFAULT;
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1,512,0,0,nullptr,0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1,&uart));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1,17,13,14,UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_set_mode(UART_NUM_1,UART_MODE_RS485_HALF_DUPLEX));
#endif
    imu_bno085_config_t cfg{};
    cfg.spi_host=SPI2_HOST;cfg.mosi_gpio=GPIO_NUM_18;cfg.miso_gpio=GPIO_NUM_16;cfg.sclk_gpio=GPIO_NUM_15;
    cfg.cs_gpio=GPIO_NUM_10;cfg.int_gpio=GPIO_NUM_11;cfg.reset_gpio=GPIO_NUM_12;
    cfg.spi_clock_hz=3000000;cfg.report_period_us=CI_REPORT_PERIOD_US;cfg.service_period_ms=2;
    cfg.install_isr_service=true;cfg.name="imu_rear";
    imu_bno085_handle_t imu=nullptr;
    ESP_ERROR_CHECK(imu_bno085_create(&cfg,&imu));
    esp_err_t start=imu_bno085_start(imu);
    if(start!=ESP_OK) ESP_LOGE(TAG,"IMU no disponible: %s",esp_err_to_name(start));
#ifdef CONFIG_CI_FULL_CAPTURE
    ESP_ERROR_CHECK(cap_start(true,"REAR",CI_RATE_HZ));
#else
    ESP_ERROR_CHECK(cap_start(false,"REAR",CI_RATE_HZ));
#endif
    cap_board_report("REAR");
#ifdef CONFIG_CI_REAR_WATER_ENABLED
    cap_metadata("REAR_WATER|enabled=1|source=REAL_RAW_REGISTER_SCALE_UNKNOWN");
#else
    ESP_LOGW(TAG,"REAR_WATER|enabled=0|no_transaction|no_sample_timestamp|sensor_disconnected_for_virtual_trial");
    cap_metadata("REAR_WATER|enabled=0|no_transaction|no_sample_timestamp|sensor_disconnected_for_virtual_trial");
#endif
    field_espnow_observers(cap_rx,cap_tx_done);
    ESP_ERROR_CHECK(field_espnow_init(1));
    if(CONFIG_CI_REAR_DIAG_PERIOD_MS) {
        diagnostic_queue=xQueueCreate(1,sizeof(rear_diagnostic));
        ESP_ERROR_CHECK(diagnostic_queue?ESP_OK:ESP_ERR_NO_MEM);
        ESP_ERROR_CHECK(xTaskCreate(diagnostic_task,"rear_diag",4096,nullptr,1,nullptr)==pdPASS?ESP_OK:ESP_ERR_NO_MEM);
    }
#ifdef CONFIG_CI_REAR_WATER_ENABLED
    xTaskCreate(water_task,"water",4096,nullptr,4,nullptr);
#endif
    uint32_t session=esp_random();if(!session) session=1;
    uint32_t sequence=0;uint64_t last_log=0;
    uint64_t last_rtt_us=0;
    uint64_t last_attempt_slot=UINT64_MAX;ct_frame last_submitted{};
    for(;;) {
        imu_bno085_sample_t s{};imu_bno085_get_sample(imu,&s);
        ct_frame f{};f.session=session;f.sequence=sequence;f.sent_us=esp_timer_get_time();
        f.accel_us=s.acceleration_time_us;f.gyro_us=s.gyro_time_us;f.orientation_us=s.orientation_time_us;
        f.accel_sequence=s.acceleration_sequence;f.gyro_sequence=s.gyro_sequence;f.orientation_sequence=s.orientation_sequence;
        std::memcpy(f.acceleration,s.linear_accel_m_s2,sizeof(f.acceleration));
        std::memcpy(f.gyro,s.gyro_rad_s,sizeof(f.gyro));std::memcpy(f.quaternion,s.quaternion_wxyz,sizeof(f.quaternion));
        f.accel_quality=s.acceleration_quality;f.gyro_quality=s.gyro_quality;f.orientation_quality=s.orientation_quality;
        if(s.has_linear_acceleration&&f.sent_us-f.accel_us<100000) f.flags|=CT_ACCEL;
        if(s.has_gyro&&f.sent_us-f.gyro_us<100000) f.flags|=CT_GYRO;
        if(s.has_orientation&&f.sent_us-f.orientation_us<100000) f.flags|=CT_ORIENTATION;
        portENTER_CRITICAL(&mux);
        rear_diagnostic diagnostic{};
        diagnostic.attempt_us=water_attempt_time;diagnostic.successes=water_successes;diagnostic.errors=water_errors;
        diagnostic.result=water_result;diagnostic.exception=water_exception;
        f.water=water_raw;f.water_us=water_time;f.water_sequence=water_sequence;
        if(water_ok&&f.sent_us>=water_time&&f.sent_us-water_time<500000) f.flags|=CT_WATER;
        portEXIT_CRITICAL(&mux);
#ifndef CONFIG_CI_REAR_WATER_ENABLED
        f.water=NAN;f.water_us=0;f.water_sequence=0; /* absent, not a Modbus response */
#endif
        f.water_quantity=CT_RAW_REGISTER;f.water_register=WATER_REGISTER;
        uint8_t bytes[CT_WIRE_SIZE];
        field_espnow_rx_t rx;
        if(field_espnow_receive(&rx,0)) {
            uint32_t ack_session,ack_sequence;uint64_t original;
            if(ct_ack_decode(rx.raw_data,rx.data_len,&ack_session,&ack_sequence,&original)&&
               ack_session==session&&original<=f.sent_us&&f.sent_us-original<1000000) {
                last_rtt_us=f.sent_us-original; /* one source clock; includes both radios and scheduling */
            }
        }
        bool encoded=ct_encode(&f,bytes);esp_err_t ret=ESP_ERR_NOT_FINISHED;
        bool fresh=f.accel_sequence!=last_submitted.accel_sequence||f.gyro_sequence!=last_submitted.gyro_sequence||
            f.orientation_sequence!=last_submitted.orientation_sequence||f.water_sequence!=last_submitted.water_sequence;
        uint64_t slot=f.sent_us/CI_REPORT_PERIOD_US;
        bool attempted=fresh&&slot!=last_attempt_slot;
        if(attempted) {
            last_attempt_slot=slot; /* one attempt per current slot, never replay missed slots */
            ret=encoded?field_espnow_try_send(bytes,sizeof(bytes)):ESP_ERR_INVALID_ARG;
            cap_tx(encoded?bytes:nullptr,encoded?sizeof(bytes):0,ret);
            if(ret==ESP_OK) {last_submitted=f;++sequence;}
        }
        uint64_t diag_period=(uint64_t)(CONFIG_CI_REAR_DIAG_PERIOD_MS<100?100:CONFIG_CI_REAR_DIAG_PERIOD_MS)*1000;
#ifdef CONFIG_CI_FULL_CAPTURE
        if(diag_period<1000000) diag_period=1000000;
#endif
        if(diagnostic_queue&&f.sent_us-last_log>=diag_period) {
            diagnostic.frame=f;diagnostic.rtt_us=last_rtt_us;diagnostic.encoded=encoded;diagnostic.queue_result=int(ret);
            diagnostic.attempted=attempted;
            xQueueOverwrite(diagnostic_queue,&diagnostic);last_log=f.sent_us;
        }
        vTaskDelay(pdMS_TO_TICKS(1)); // Poll newest reports; no backlog or catch-up burst.
    }
}
