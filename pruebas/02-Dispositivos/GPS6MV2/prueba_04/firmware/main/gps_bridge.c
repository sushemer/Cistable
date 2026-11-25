#include "gps_bridge.h"

#include <math.h>
#include <inttypes.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "gps_config.h"
#include "gps_data.h"
#include "gps_device.h"
#include "gps_imu_fusion.h"
#include "gps_nmea.h"
#include "gps_validation.h"

#define GPS_TASK_STACK_SIZE 4096
#define GPS_TASK_PRIORITY   5

static const char *TAG = "GPS";

static bool uart_stats_changed(const gps_device_stats_t *current, const gps_device_stats_t *previous){
    return current->uart_overflow != previous->uart_overflow || current->uart_buffer_full != previous->uart_buffer_full ||
           current->uart_frame_error != previous->uart_frame_error || current->uart_parity_error != previous->uart_parity_error ||
           current->overlong_lines != previous->overlong_lines;
}

static void report_uart_errors(void){
    static gps_device_stats_t reported;
    gps_device_stats_t current = {0};
    gps_device_get_stats(&current);
    if (!uart_stats_changed(&current, &reported)) return;
    ESP_LOGW(TAG, "GPS_EVENT|UART_ERRORS|overflow=%" PRIu32 "|buffer_full=%" PRIu32 "|frame=%" PRIu32 "|parity=%" PRIu32 "|overlong=%" PRIu32,
             current.uart_overflow, current.uart_buffer_full, current.uart_frame_error, current.uart_parity_error, current.overlong_lines);
    reported = current;
}

static float value_or_nan(bool valid, float value){ return valid ? value : NAN; }

// Registra GPS crudo y, en una línea separada, la velocidad corregida con IMU.
static void emit_gps_records(const char *line, const gps_sample_t *sample, const gps_nmea_result_t *nmea,
                             const gps_validation_result_t *validation, const gps_imu_fusion_result_t *fusion,
                             int64_t received_us){
    const bool gps_measurement_valid = sample->rmc_status_valid && sample->rmc_data_valid && sample->fix_quality > 0 &&
                                       sample->satellites >= GPS_MIN_SATELLITES && sample->hdop > 0.0f && sample->hdop <= GPS_MAX_HDOP;
    ESP_LOGI(TAG, "GPSDIAG|1|%" PRIu64 "|%s|%d|%d|%" PRId32 "|%d|%d|%" PRId32 "|%.6f|%.6f|%.3f|%.3f|%.2f|%u|%.2f|%" PRIu32 "|%" PRIu32 "|%s|%s|%d|nan|nan|nan|nan|%s",
             (uint64_t)received_us, nmea->type, nmea->checksum_valid ? 1 : 0, nmea->parsed ? 1 : 0,
             nmea->has_time ? (int32_t)nmea->time_of_day_ms : -1, sample->rmc_status_valid ? 1 : 0, sample->fix_quality,
             validation->epoch_skew_ms == UINT32_MAX ? -1 : (int32_t)validation->epoch_skew_ms,
             value_or_nan(sample->rmc_data_valid, sample->latitude_deg), value_or_nan(sample->rmc_data_valid, sample->longitude_deg),
             value_or_nan(sample->rmc_data_valid, sample->speed_knots), value_or_nan(sample->rmc_data_valid, sample->speed_kmh),
             value_or_nan(sample->rmc_data_valid, sample->course_deg), (unsigned)sample->satellites,
             value_or_nan(sample->gga_data_valid, sample->hdop), validation->rmc_age_ms, validation->gga_age_ms,
             gps_validation_state_name(validation->state), fusion->state, fusion->speed_usable ? 1 : 0, line);
    if (strcmp(nmea->type, "GPRMC") == 0 || strcmp(nmea->type, "GNRMC") == 0) {
        ESP_LOGI(TAG, "GPSFUSION|1|%" PRIu64 "|%.3f|%.3f|%.3f|%.5f|%s|%" PRIu32 "|%s|%d|%d",
                 (uint64_t)received_us, fusion->gps_speed_kmh, fusion->corrected_speed_kmh, fusion->speed_delta_kmh,
                 fusion->acceleration_norm_m_s2, fusion->state, fusion->stationary_ms,
                 gps_validation_state_name(validation->state), gps_measurement_valid ? 1 : 0, fusion->imu_fresh ? 1 : 0);
    }
}

static void gps_task(void *argument){
    (void)argument;
    gps_sample_t sample;
    char line[GPS_LINE_SIZE];
    gps_sample_init(&sample);
    gps_device_init();
    const gps_validation_config_t config = {
        .min_satellites = GPS_MIN_SATELLITES, .max_hdop = GPS_MAX_HDOP,
        .max_age_ms = GPS_MAX_AGE_MS, .max_epoch_skew_ms = GPS_MAX_EPOCH_SKEW_MS,
    };
    ESP_LOGI(TAG, "GPS UART listo: puerto=%d baud=%d", GPS_UART_PORT, GPS_UART_BAUD_RATE);
    while (true) {
        report_uart_errors();
        if (!gps_device_read_line(line, sizeof(line), pdMS_TO_TICKS(20))) continue;
        const int64_t received_us = esp_timer_get_time();
        gps_nmea_result_t nmea = {0};
        (void)gps_nmea_process_line(&sample, line, received_us, &nmea);
        const gps_validation_result_t validation = gps_validation_evaluate(&sample, &config, received_us);
        const bool gps_valid = sample.rmc_status_valid && sample.rmc_data_valid && sample.fix_quality > 0 &&
                               sample.satellites >= GPS_MIN_SATELLITES && sample.hdop > 0.0f && sample.hdop <= GPS_MAX_HDOP;
        const gps_imu_fusion_result_t fusion = gps_imu_fusion_evaluate(sample.speed_kmh, gps_valid, received_us);
        emit_gps_records(line, &sample, &nmea, &validation, &fusion, received_us);
    }
}

void gps_bridge_start(void){
    if (xTaskCreate(gps_task, "gps_reader", GPS_TASK_STACK_SIZE, NULL, GPS_TASK_PRIORITY, NULL) != pdPASS)
        ESP_LOGE(TAG, "No se pudo iniciar la tarea de lectura GPS");
}
