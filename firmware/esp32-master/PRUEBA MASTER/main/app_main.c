#include "field_espnow.h"
#include "tank_payload.h"
#include "modules/imu/imu_module.h"
#include "modules/gps/gps_module.h"

#include <inttypes.h>
#include <math.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

#define ESPNOW_CHANNEL 1U

static const char *TAG = "MASTER";
static int64_t s_last_rx_us;
static bool s_no_data_reported;
static uint32_t s_rx_sequence;
static TankPayload s_rear_data;
static int8_t s_last_rssi_dbm;
static bool s_last_rssi_valid;
static imu_bno085_sample_t s_front_imu;
static int64_t s_last_master_record_us;

static void apply_tank_payload(const TankPayload *payload)
{
    s_rear_data = *payload;
    s_rx_sequence++;
    s_last_rx_us = esp_timer_get_time();
    s_no_data_reported = false;

    ESP_LOGI(TAG, "TankPayload RX: flags=0x%02X seq=%" PRIu32,
             payload->sensor_status_flags, s_rx_sequence);
}

static void emit_master_record(void)
{
    gps_module_snapshot_t gps = {0};
    const bool have_gps = gps_module_get_snapshot(&gps);
    const bool online = s_last_rx_us > 0 && esp_timer_get_time() - s_last_rx_us <= 3000000;
    const float nan = NAN;
    const float latitude = have_gps && gps.sample.rmc_data_valid ? gps.sample.latitude_deg : nan;
    const float longitude = have_gps && gps.sample.rmc_data_valid ? gps.sample.longitude_deg : nan;
    const float speed = have_gps && gps.speed_valid ? gps.displayed_speed_kmh : nan;
    const float hdop = have_gps && gps.sample.gga_data_valid ? gps.sample.hdop : nan;
    const uint8_t flags = online ? s_rear_data.sensor_status_flags : 0U;

    ESP_LOGI(TAG, "GPS_DATA|%" PRIu64 "|%s|%.6f|%.6f|%.3f|%u|%.2f|%d",
             (uint64_t)esp_timer_get_time(), have_gps ? gps_validation_state_name(gps.validation.state) : "SIN_DATOS",
             latitude, longitude, speed, have_gps ? gps.sample.satellites : 0U, hdop,
             have_gps && gps.speed_valid ? 1 : 0);
    ESP_LOGI(TAG, "IMU_DATA|1|%" PRIu64 "|%d|%.3f|%.3f|%.3f|%.3f|%.3f|%.3f|%.3f|%.3f|%.3f",
             (uint64_t)s_front_imu.sample_time_us,
             s_front_imu.online ? 1 : 0,
             s_front_imu.has_orientation ? s_front_imu.roll_deg : nan,
             s_front_imu.has_orientation ? s_front_imu.pitch_deg : nan,
             s_front_imu.has_orientation ? s_front_imu.yaw_deg : nan,
             s_front_imu.has_linear_acceleration ? s_front_imu.linear_accel_m_s2[0] : nan,
             s_front_imu.has_linear_acceleration ? s_front_imu.linear_accel_m_s2[1] : nan,
             s_front_imu.has_linear_acceleration ? s_front_imu.linear_accel_m_s2[2] : nan,
             s_front_imu.has_gyro ? s_front_imu.gyro_rad_s[0] : nan,
             s_front_imu.has_gyro ? s_front_imu.gyro_rad_s[1] : nan,
             s_front_imu.has_gyro ? s_front_imu.gyro_rad_s[2] : nan);
    ESP_LOGI(TAG, "IMU_DATA|2|%" PRIu64 "|%d|%.3f|%.3f|nan|nan|%.3f|nan|nan|nan|nan",
             (uint64_t)esp_timer_get_time(),
             online && (flags & STATUS_IMU1_OK) ? 1 : 0,
             (flags & STATUS_IMU1_OK) ? s_rear_data.roll_1 : nan,
             (flags & STATUS_IMU1_OK) ? s_rear_data.pitch_1 : nan,
             (flags & STATUS_IMU1_OK) ? s_rear_data.ay_1 : nan);
    ESP_LOGI(TAG, "RSSI_DATA|%d|%d|%" PRIu32,
             s_last_rssi_valid ? (int)s_last_rssi_dbm : 0,
             s_last_rssi_valid ? 1 : 0, s_rx_sequence);
    ESP_LOGI(TAG, "WATER_DATA|%.3f|%d|%02X",
             (online && (flags & STATUS_RS485_OK)) ? s_rear_data.water_level : nan,
             online && (flags & STATUS_RS485_OK) ? 1 : 0, flags);

    ESP_LOGI(TAG, "MASTERDATA|2|%" PRIu64 "|%s|%.6f|%.6f|%.3f|%u|%.2f|%" PRIu32 "|%d|%.2f|%.2f|%.3f|%.3f|%.3f|%d|%.2f|%.2f|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%.2f|%.2f|%.3f|%.3f|%.3f|%.2f|%.2f|%.3f|%.3f|%.3f|%.3f|%" PRIu32,
             (uint64_t)esp_timer_get_time(), have_gps ? gps_validation_state_name(gps.validation.state) : "SIN_DATOS",
             latitude, longitude, speed, have_gps ? gps.sample.satellites : 0U, hdop,
             have_gps ? gps.validation.rmc_age_ms : UINT32_MAX,
             s_front_imu.online ? 1 : 0,
             s_front_imu.online && s_front_imu.has_orientation ? s_front_imu.roll_deg : nan,
             s_front_imu.online && s_front_imu.has_orientation ? s_front_imu.pitch_deg : nan,
             s_front_imu.online && s_front_imu.has_linear_acceleration ? s_front_imu.linear_accel_m_s2[0] : nan,
             s_front_imu.online && s_front_imu.has_linear_acceleration ? s_front_imu.linear_accel_m_s2[1] : nan,
             s_front_imu.online && s_front_imu.has_linear_acceleration ? s_front_imu.linear_accel_m_s2[2] : nan,
             online ? 1 : 0, s_rx_sequence, flags,
             (flags & STATUS_IMU1_OK) ? s_rear_data.roll_1 : nan,
             (flags & STATUS_IMU1_OK) ? s_rear_data.pitch_1 : nan,
             (flags & STATUS_IMU1_OK) ? s_rear_data.ay_1 : nan,
             (flags & STATUS_RS485_OK) ? s_rear_data.water_level : nan);
}

void app_main(void){
    gps_module_start();
    if (!imu_module_start()) {
        ESP_LOGE(TAG, "No se pudo iniciar la IMU frontal; el master continuara.");
    }
    ESP_ERROR_CHECK(field_espnow_init(ESPNOW_CHANNEL));

    uint8_t local_mac[FIELD_MAC_LENGTH];
    ESP_ERROR_CHECK(field_espnow_get_mac(local_mac));
    ESP_LOGI(TAG, "Master listo; MAC de estación=" MACSTR ", canal=%u", MAC2STR(local_mac), ESPNOW_CHANNEL);
    ESP_LOGI(TAG, "Receptor exclusivo TankPayload de 17 bytes; sin protocolo de 104 bytes ni ACK de aplicacion.");
    s_last_rx_us = 0;

    while (true) {
        field_espnow_rx_t received = {0};
        if (field_espnow_receive(&received, pdMS_TO_TICKS(100))) {
            s_last_rssi_dbm = received.rssi_dbm;
            s_last_rssi_valid = received.rssi_valid;
            ESP_LOGI(TAG, "ESP-NOW RX de " MACSTR ": %u bytes, RSSI %s%d dBm", MAC2STR(received.source_mac), received.data_len,
                     received.rssi_valid ? "" : "N/D ", received.rssi_valid ? (int)received.rssi_dbm : 0);
            if (received.rssi_valid && received.data_len == sizeof(TankPayload)) {
                ESP_LOGI(TAG, "RSSI_RECIBIDO|%d dBm", (int)received.rssi_dbm);
            }

            if (received.data_len == sizeof(TankPayload)) {
                TankPayload payload = {0};
                memcpy(&payload, received.raw_data, sizeof(payload));
                apply_tank_payload(&payload);
            } 
            else {
                ESP_LOGW(TAG, "Paquete descartado: se esperaban 17 bytes y llegaron %u", received.data_len);
            }
        }

        (void)imu_module_get_sample(&s_front_imu);
        const int64_t now_us = esp_timer_get_time();
        if (!s_no_data_reported && (s_last_rx_us == 0 || now_us - s_last_rx_us >= 3000000)) {
            ESP_LOGW(TAG, "Sin TankPayload validos durante 3 s");
            s_no_data_reported = true;
        }
        if (now_us - s_last_master_record_us >= 1000000) {
            emit_master_record();
            s_last_master_record_us = now_us;
        }
    }
}
