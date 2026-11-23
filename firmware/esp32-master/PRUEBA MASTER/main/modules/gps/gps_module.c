#include "gps_module.h"

#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gps_config.h"
#include "gps_device.h"
#include "gps_nmea.h"

static const char *TAG = "GPS";
static gps_module_snapshot_t s_snapshot;
static portMUX_TYPE s_snapshot_mux = portMUX_INITIALIZER_UNLOCKED;
static const gps_validation_config_t s_validation_config = {
    .min_satellites = GPS_MIN_SATELLITES,
    .max_hdop = GPS_MAX_HDOP,
    .max_age_ms = GPS_MAX_AGE_MS,
    .max_epoch_skew_ms = GPS_MAX_EPOCH_SKEW_MS,
};

static void gps_reader_task(void *arg)
{
    (void)arg;
    gps_sample_t sample = {0};
    char line[GPS_LINE_SIZE];
    gps_device_init();
    ESP_LOGI(TAG, "GPS6MV2 UART=%d baud=%d RX=%d TX=%d", GPS_UART_PORT, GPS_UART_BAUD_RATE, GPS_UART_RXD, GPS_UART_TXD);

    while (true) {
        if (!gps_device_read_line(line, sizeof(line), pdMS_TO_TICKS(20))) continue;
        const int64_t now_us = esp_timer_get_time();
        gps_nmea_result_t parsed = {0};
        (void)gps_nmea_process_line(&sample, line, now_us, &parsed);
        gps_module_snapshot_t next = {
            .sample = sample,
            .validation = gps_validation_evaluate(&sample, &s_validation_config, now_us),
            .displayed_speed_kmh = sample.speed_kmh,
            .speed_valid = sample.rmc_data_valid && sample.rmc_status_valid,
            .snapshot_us = now_us,
        };
        portENTER_CRITICAL(&s_snapshot_mux);
        s_snapshot = next;
        portEXIT_CRITICAL(&s_snapshot_mux);
    }
}

void gps_module_start(void)
{
    memset(&s_snapshot, 0, sizeof(s_snapshot));
    if (xTaskCreate(gps_reader_task, "gps_reader", 6144, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "No se pudo crear la tarea GPS");
    }
}

bool gps_module_get_snapshot(gps_module_snapshot_t *snapshot)
{
    if (snapshot == NULL) return false;
    portENTER_CRITICAL(&s_snapshot_mux);
    *snapshot = s_snapshot;
    portEXIT_CRITICAL(&s_snapshot_mux);
    if (snapshot->snapshot_us > 0) {
        snapshot->validation = gps_validation_evaluate(&snapshot->sample, &s_validation_config, esp_timer_get_time());
    }
    return snapshot->snapshot_us > 0;
}
