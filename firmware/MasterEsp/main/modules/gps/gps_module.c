#include "gps_module.h"

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

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
static bool s_clock_synced;

// Ajusta el reloj UTC del ESP32 con la fecha/hora RMC y deja la zona local en Tijuana.
static void sync_clock_from_gps(const gps_sample_t *sample)
{
    if (s_clock_synced || sample == NULL || !sample->rmc_date_valid ||
        !sample->rmc_data_valid || !sample->rmc_status_valid) return;

    struct tm utc = {0};
    utc.tm_year = (int)sample->rmc_year - 1900;
    utc.tm_mon = (int)sample->rmc_month - 1;
    utc.tm_mday = sample->rmc_day;
    utc.tm_hour = (int)(sample->rmc_time_of_day_ms / 3600000U);
    utc.tm_min = (int)((sample->rmc_time_of_day_ms / 60000U) % 60U);
    utc.tm_sec = (int)((sample->rmc_time_of_day_ms / 1000U) % 60U);

    setenv("TZ", "UTC0", 1);
    tzset();
    const time_t epoch = mktime(&utc);
    if (epoch < 0) return;

    struct timeval tv = {
        .tv_sec = epoch,
        .tv_usec = (suseconds_t)((sample->rmc_time_of_day_ms % 1000U) * 1000U),
    };
    if (settimeofday(&tv, NULL) != 0) return;

    setenv("TZ", "PST8PDT,M3.2.0/2,M11.1.0/2", 1);
    tzset();
    s_clock_synced = true;
    ESP_LOGI(TAG, "Reloj sincronizado por GPS: %04u-%02u-%02u %02u:%02u:%02u UTC",
             sample->rmc_year, sample->rmc_month, sample->rmc_day,
             (unsigned)(sample->rmc_time_of_day_ms / 3600000U),
             (unsigned)((sample->rmc_time_of_day_ms / 60000U) % 60U),
             (unsigned)((sample->rmc_time_of_day_ms / 1000U) % 60U));
}

static void gps_reader_task(void *arg)
{
    (void)arg;
    gps_sample_t sample = {0};
    char line[GPS_LINE_SIZE];
    gps_device_init();
    ESP_LOGI(TAG, "GPS6MV2 UART=%d baud=%d RX=%d TX=%d", GPS_UART_PORT, GPS_UART_BAUD_RATE, GPS_UART_RXD, GPS_UART_TXD);

    while (true) {
        if (!gps_device_read_line(line, sizeof(line), pdMS_TO_TICKS(20))) {
            // Cede CPU aunque el GPS todavía no haya completado una línea.
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }
        const int64_t now_us = esp_timer_get_time();
        gps_nmea_result_t parsed = {0};
        (void)gps_nmea_process_line(&sample, line, now_us, &parsed);
        sync_clock_from_gps(&sample);
        ESP_LOGI(TAG, "NMEA %s parsed=%s checksum=%s fix=%d sats=%u lat=%.6f lon=%.6f speed=%.2f km/h hdop=%.2f",
                 parsed.type[0] != '\0' ? parsed.type : "UNKNOWN",
                 parsed.parsed ? "yes" : "no",
                 parsed.checksum_valid ? "ok" : "bad",
                 sample.fix_quality, (unsigned)sample.satellites,
                 sample.latitude_deg, sample.longitude_deg, sample.speed_kmh, sample.hdop);
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

        // Evita que una transmisión continua del GPS monopolice la CPU.
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void gps_module_start(void)
{
    memset(&s_snapshot, 0, sizeof(s_snapshot));
    s_clock_synced = false;
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
