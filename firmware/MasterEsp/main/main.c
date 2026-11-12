#include "field_espnow.h"
#include "tank_payload.h"
#include "modules/display/display_module.h"
#include "modules/imu/imu_module.h"
#include "modules/gps/gps_module.h"
#include "modules/ltr/ltr_module.h"
#include <math.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define ESPNOW_CHANNEL 1U
#define GRAVITY_M_S2 9.80665f
#define VEHICLE_TRACK_M 1.60f
#define CG_HEIGHT_EMPTY_M 1.20f
#define CG_HEIGHT_FULL_M 1.50f
#define WATER_TANK_RADIUS_CM 50.0f
#define WATER_TANK_LENGTH_CM 200.0f
#define WATER_TANK_MAX_LEVEL_CM (2.0f * WATER_TANK_RADIUS_CM)
#define SENSOR_TIMEOUT_US 3000000LL
#define STATIC_RISK_LTR_THRESHOLD 0.35f
#define STATIC_RISK_ROLL_THRESHOLD_DEG 8.0f
#define IMU_LOG_INTERVAL_US 500000LL

static const char *TAG = "MASTER";
static TankPayload s_rear_data;
static imu_bno085_sample_t s_front_imu;
static display_telemetry_t s_telemetry;
static ltr_result_t s_imu1_ltr;
static ltr_result_t s_imu2_ltr;
static SemaphoreHandle_t s_data_mutex;
static int64_t s_last_rx_us;
static int64_t s_last_imu_log_us;
static int64_t s_last_imu_sample_us;
static int64_t s_last_gps_log_us;

static float clampf_local(float value, float minimum, float maximum)
{
    return fminf(fmaxf(value, minimum), maximum);
}

static float water_liters(float level_cm)
{
    const float level = clampf_local(level_cm, 0.0f, WATER_TANK_MAX_LEVEL_CM);
    const float radius = WATER_TANK_RADIUS_CM;
    const float area_cm2 = radius * radius * acosf((radius - level) / radius) -
                           (radius - level) * sqrtf(fmaxf(0.0f, 2.0f * radius * level - level * level));
    return fmaxf(0.0f, area_cm2 * WATER_TANK_LENGTH_CM / 1000.0f);
}

static void calculate_telemetry(int64_t now_us)
{
    const bool rear_online = s_last_rx_us > 0 && now_us - s_last_rx_us <= SENSOR_TIMEOUT_US;
    const bool remote_ok = rear_online && (s_rear_data.sensor_status_flags & STATUS_IMU2_OK) &&
                           isfinite(s_rear_data.roll_1) && isfinite(s_rear_data.ay_1);
    const bool local_ok = s_front_imu.online && s_front_imu.has_orientation &&
                          s_front_imu.has_linear_acceleration && isfinite(s_front_imu.roll_deg) &&
                          isfinite(s_front_imu.linear_accel_m_s2[1]);
    float roll = 0.0f, ay = 0.0f;
    unsigned count = 0;
    if (local_ok) { roll += s_front_imu.roll_deg; ay += s_front_imu.linear_accel_m_s2[1]; ++count; }
    if (remote_ok) { roll += s_rear_data.roll_1; ay += s_rear_data.ay_1; ++count; }
    if (count > 0) { roll /= (float)count; ay /= (float)count; }

    const bool water_ok = rear_online && (s_rear_data.sensor_status_flags & STATUS_RS485_OK) &&
                          isfinite(s_rear_data.water_level);
    const float level = water_ok ? clampf_local(s_rear_data.water_level, 0.0f, WATER_TANK_MAX_LEVEL_CM) : 0.0f;
    const float water_percent = 100.0f * level / WATER_TANK_MAX_LEVEL_CM;
    const float cg_height = CG_HEIGHT_EMPTY_M + (CG_HEIGHT_FULL_M - CG_HEIGHT_EMPTY_M) * water_percent / 100.0f;

    /* Signed LTR model from the specification. Linear acceleration already
       excludes gravity, so do not subtract g from ay a second time. */
    const ltr_config_t ltr_config = {
        .cg_height_m = cg_height,
        .track_width_m = VEHICLE_TRACK_M,
        .gravity_m_s2 = GRAVITY_M_S2,
        .minimum_value = -1.0f,
        .maximum_value = 1.0f,
    };
    ltr_result_t ltr_result = {0};
    const bool ltr_valid = count > 0 &&
                           ltr_calculate(&ltr_config, roll, ay, &ltr_result);
    const float ltr = ltr_valid ? ltr_result.value : 0.0f;
    s_imu1_ltr = (ltr_result_t){0};
    s_imu2_ltr = (ltr_result_t){0};
    if (local_ok) {
        (void)ltr_calculate(&ltr_config, s_front_imu.roll_deg,
                            s_front_imu.linear_accel_m_s2[1], &s_imu1_ltr);
    }
    if (remote_ok) {
        (void)ltr_calculate(&ltr_config, s_rear_data.roll_1,
                            s_rear_data.ay_1, &s_imu2_ltr);
    }

    s_telemetry.roll_deg = roll;
    s_telemetry.lateral_accel_m_s2 = ay;
    s_telemetry.ltr = ltr;
    s_telemetry.water_level_cm = level;
    s_telemetry.water_gallons = water_liters(level) / 3.785411784f;
    s_telemetry.water_percent = water_percent;
    s_telemetry.imu_local_ok = local_ok;
    s_telemetry.imu_remote_ok = remote_ok;
    s_telemetry.water_ok = water_ok;
    s_telemetry.rear_online = rear_online;
    s_telemetry.static_risk = ltr >= STATIC_RISK_LTR_THRESHOLD || fabsf(roll) >= STATIC_RISK_ROLL_THRESHOLD_DEG;
}

static void sensor_task(void *arg)
{
    (void)arg;
    while (true) {
        field_espnow_rx_t received = {0};
        TankPayload slave_sample = {0};
        int64_t slave_sample_time_us = 0;
        bool new_slave_sample = false;
        if (field_espnow_receive(&received, pdMS_TO_TICKS(20))) {
            ESP_LOGI(TAG, "ESP-NOW RX from " MACSTR " len=%u RSSI=%d dBm",
                     MAC2STR(received.source_mac), (unsigned)received.data_len,
                     received.rssi_valid ? received.rssi_dbm : 0);
            if (received.data_len == sizeof(TankPayload)) {
                memcpy(&s_rear_data, received.raw_data, sizeof(s_rear_data));
                s_last_rx_us = esp_timer_get_time();
                slave_sample = s_rear_data;
                slave_sample_time_us = s_last_rx_us;
                new_slave_sample = true;
                ESP_LOGI(TAG, "ESP-NOW data: roll=%.2f pitch=%.2f ay=%.2f water=%.2f flags=0x%02X",
                         s_rear_data.roll_1, s_rear_data.pitch_1, s_rear_data.ay_1,
                         s_rear_data.water_level, (unsigned)s_rear_data.sensor_status_flags);
                ESP_LOGI(TAG, "[DATA] IMU2 RX: roll=%.3f pitch=%.3f ay_m_s2=%.3f water_level_cm=%.3f status_flags=0x%02X",
                         s_rear_data.roll_1, s_rear_data.pitch_1, s_rear_data.ay_1,
                         s_rear_data.water_level, (unsigned)s_rear_data.sensor_status_flags);
            } else {
                ESP_LOGW(TAG, "ESP-NOW frame ignored: expected %u bytes, got %u",
                         (unsigned)sizeof(TankPayload), (unsigned)received.data_len);
            }
        }
        imu_bno085_sample_t sample = {0};
        bool new_imu_sample = false;
        if (imu_module_get_sample(&sample)) {
            s_front_imu = sample;
            const int64_t now_us = esp_timer_get_time();
            if (sample.sample_time_us != s_last_imu_sample_us) {
                new_imu_sample = true;
                s_last_imu_sample_us = sample.sample_time_us;
            }
            if (new_imu_sample && now_us - s_last_imu_log_us >= IMU_LOG_INTERVAL_US) {
                ESP_LOGI(TAG, "IMU sample: roll=%.2f pitch=%.2f yaw=%.2f accel=(%.2f, %.2f, %.2f) gyro=(%.3f, %.3f, %.3f)",
                         sample.roll_deg, sample.pitch_deg, sample.yaw_deg,
                         sample.linear_accel_m_s2[0], sample.linear_accel_m_s2[1], sample.linear_accel_m_s2[2],
                         sample.gyro_rad_s[0], sample.gyro_rad_s[1], sample.gyro_rad_s[2]);
                s_last_imu_log_us = now_us;
            }
        }
        if (xSemaphoreTake(s_data_mutex, portMAX_DELAY) == pdTRUE) {
            calculate_telemetry(esp_timer_get_time());
            if (new_imu_sample) {
                ESP_LOGI(TAG, "[DATA] IMU1,time_us=%" PRId64 ",roll_deg=%.3f,pitch_deg=%.3f,yaw_deg=%.3f,accel_x=%.3f,accel_y=%.3f,accel_z=%.3f,gyro_x=%.4f,gyro_y=%.4f,gyro_z=%.4f,ltr=%.5f,valid=%u",
                         sample.sample_time_us, sample.roll_deg, sample.pitch_deg, sample.yaw_deg,
                         sample.linear_accel_m_s2[0], sample.linear_accel_m_s2[1], sample.linear_accel_m_s2[2],
                         sample.gyro_rad_s[0], sample.gyro_rad_s[1], sample.gyro_rad_s[2],
                         s_imu1_ltr.valid ? s_imu1_ltr.value : 0.0f,
                         (unsigned)s_imu1_ltr.valid);
            }
            if (new_slave_sample) {
                ESP_LOGI(TAG, "[DATA] IMU2,time_us=%" PRId64 ",roll_deg=%.3f,pitch_deg=%.3f,ay_m_s2=%.3f,water_level_cm=%.3f,status_flags=0x%02X,ltr=%.5f,valid=%u",
                         slave_sample_time_us, slave_sample.roll_1, slave_sample.pitch_1,
                         slave_sample.ay_1, slave_sample.water_level,
                         (unsigned)slave_sample.sensor_status_flags,
                         s_imu2_ltr.valid ? s_imu2_ltr.value : 0.0f,
                         (unsigned)s_imu2_ltr.valid);
            }
            if (new_imu_sample || new_slave_sample) {
                ESP_LOGI(TAG, "[DATA] IMU_A,time_us=%" PRId64 ",roll_deg=%.3f,ay_m_s2=%.3f,ltr=%.5f,sensors_used=%u",
                         esp_timer_get_time(), s_telemetry.roll_deg,
                         s_telemetry.lateral_accel_m_s2, s_telemetry.ltr,
                         (unsigned)(s_imu1_ltr.valid + s_imu2_ltr.valid));
            }
            xSemaphoreGive(s_data_mutex);
        }
    }
}

static void gui_task(void *arg)
{
    (void)arg;
    while (true) {
        gps_module_snapshot_t gps = {0};
        display_telemetry_t telemetry = {0};
        const bool have_gps = gps_module_get_snapshot(&gps);
        if (xSemaphoreTake(s_data_mutex, portMAX_DELAY) == pdTRUE) {
            telemetry = s_telemetry;
            xSemaphoreGive(s_data_mutex);
        }
        telemetry.speed_kmh = have_gps && gps.speed_valid && gps.validation.usable_for_speed ? gps.displayed_speed_kmh : 0.0f;
        if (have_gps && gps.snapshot_us != s_last_gps_log_us) {
            ESP_LOGI(TAG, "[DATA] GPS,time_us=%" PRId64 ",lat_deg=%.7f,lon_deg=%.7f,speed_kmh=%.3f,speed_mph=%.3f,fix=%d,satellites=%u,hdop=%.2f,ltr=%.5f,telemetry_roll_deg=%.3f",
                     gps.snapshot_us, gps.sample.latitude_deg, gps.sample.longitude_deg,
                     gps.displayed_speed_kmh, gps.displayed_speed_kmh * 0.6213711922f,
                     gps.sample.fix_quality, (unsigned)gps.sample.satellites, gps.sample.hdop,
                     telemetry.ltr, telemetry.roll_deg);
            s_last_gps_log_us = gps.snapshot_us;
        }
        display_module_update(have_gps ? &gps : NULL, &telemetry);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
    s_data_mutex = xSemaphoreCreateMutex();
    if (s_data_mutex == NULL) { ESP_LOGE(TAG, "No se pudo crear el mutex"); return; }
    gps_module_start();
    if (!imu_module_start()) ESP_LOGE(TAG, "No se pudo iniciar la IMU local");
    ESP_LOGI(TAG, "Starting LCD/LVGL");
    display_module_start();
    ESP_LOGI(TAG, "LCD/LVGL initialization returned");
    ESP_ERROR_CHECK(field_espnow_init(ESPNOW_CHANNEL));
    uint8_t mac[FIELD_MAC_LENGTH];
    ESP_ERROR_CHECK(field_espnow_get_mac(mac));
    ESP_LOGI(TAG, "Sistema de advertencia listo; MAC=" MACSTR, MAC2STR(mac));
    xTaskCreate(sensor_task, "rollover_sensor", 8192, NULL, 5, NULL);
    xTaskCreate(gui_task, "rollover_gui", 4096, NULL, 2, NULL);
}
