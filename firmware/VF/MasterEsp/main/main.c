#include "field_espnow.h"
#include "board_spi_pins.h"
#include "tank_payload.h"
#include "modules/display/display_module.h"
#include "modules/imu/imu_module.h"
#include "modules/gps/gps_module.h"
#include "modules/ltr/ltr_module.h"
#include "modules/pendulum/pendulum_module.h"
#include "network_client.h"
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

#ifdef CONFIG_CISTERNA_TEST_MODE
#undef CISTERNA_TEST_MODE
#undef CISTERNA_TEST_VALUE
#define CISTERNA_TEST_MODE 1
#define CISTERNA_TEST_VALUE CONFIG_CISTERNA_TEST_VALUE
#else
#define CISTERNA_TEST_MODE 0
#define CISTERNA_TEST_VALUE 65.0f
#endif

#define ESPNOW_CHANNEL 1U
#define GRAVITY_M_S2 9.80665f
#define VEHICLE_TRACK_M 1.60f
#define VEHICLE_WHEELBASE_M 3.80f
#define VEHICLE_GROSS_MASS_KG 5443.0f
#define CG_HEIGHT_EMPTY_M 1.20f
#define CG_HEIGHT_FULL_M 1.50f
#define WATER_TANK_SEMI_AXIS_A_M 1.10f
#define WATER_TANK_SEMI_AXIS_B_M 0.62f
#define WATER_TANK_LENGTH_M 3.04f
#define WATER_LIQUID_DENSITY_KG_M3 1000.0f
#define DAMPING_RATIO_LAT 0.07f
#define DAMPING_RATIO_LONG 0.07f
#define WATER_TANK_LENGTH_CM 304.0f
#define WATER_TANK_WIDTH_CM 220.0f
#define WATER_TANK_HEIGHT_CM 124.0f
#define WATER_SENSOR_FULL_SCALE 120.0f
#ifdef CONFIG_CISTERNA_TEST_MODE
#define WATER_LEVEL_OVERRIDE CONFIG_CISTERNA_TEST_VALUE
#else
#define WATER_LEVEL_OVERRIDE (-1)
#endif
#define PENDULUM_MIN_WATER_PERCENT 10.0f
#define PENDULUM_MAX_WATER_PERCENT 75.0f
#define PENDULUM_LENGTH_M 0.50f
#define PENDULUM_LONG_LENGTH_M 1.00f
#define PENDULUM_DAMPING 0.60f
#define SENSOR_TIMEOUT_US 3000000LL
#define STATIC_RISK_LTR_THRESHOLD 0.35f
#define FRONT_TARE_SAMPLE_COUNT 30U
#define REAR_TARE_SAMPLE_COUNT 15U
#define TARE_STATIONARY_GYRO_RAD_S 0.05f
#define PITCH_DEADBAND_DEG 10.0f
#define STATIC_PITCH_RISK_THRESHOLD_DEG 15.0f
#define PITCH_LTR_WARNING_THRESHOLD 0.45f
#define CRITICAL_LTR_THRESHOLD 0.80f
#define STATIC_RISK_ROLL_THRESHOLD_DEG 8.0f
#define REFERENCE_TURN_RADIUS_M 45.0f
#define ROLL_PREDICTION_HORIZON_S 0.50f
#define DEG_TO_RAD 0.01745329251994329577f
#define IMU_LOG_INTERVAL_US 500000LL
#define SLAVE_RX_WAIT_MS 10U
#define SENSOR_TASK_PERIOD_MS 10U
#define SLAVE_LOG_INTERVAL_US 1000000LL
#define LCD_STARTUP_DELAY_MS 2000U

static const char *TAG = "MASTER";
static TankPayload s_rear_data;
static imu_bno085_sample_t s_front_imu;
static display_telemetry_t s_telemetry;
static ltr_result_t s_imu1_ltr;
static ltr_result_t s_imu2_ltr;
static pendulum_state_t s_water_pendulum_state;
static pendulum_state_t s_long_pendulum_state;
static int64_t s_last_pendulum_step_us;
static bool s_water_pendulum_active;
static bool s_pendulum_error_reported;
static bool s_ltr_limit_error_reported;
static bool s_ltr_degraded;
static bank_estimator_t s_bank_estimator;
static bool s_config_valid;
static bool s_long_wave_risk;
static float s_semi_axis_a_m = WATER_TANK_SEMI_AXIS_A_M;
static float s_semi_axis_b_m = WATER_TANK_SEMI_AXIS_B_M;
static float s_tank_length_m = WATER_TANK_LENGTH_M;
static float s_liquid_density_kg_m3 = WATER_LIQUID_DENSITY_KG_M3;
static float s_damping_ratio_lat = DAMPING_RATIO_LAT;
static float s_damping_ratio_long = DAMPING_RATIO_LONG;
static float s_slosh_enable_pct = PENDULUM_MIN_WATER_PERCENT;
static float s_slosh_disable_low_pct = 5.0f;
static float s_slosh_disable_high_pct = PENDULUM_MAX_WATER_PERCENT;
static float s_front_roll_offset_deg;
static float s_front_pitch_offset_deg;
static float s_rear_roll_offset_deg;
static float s_rear_pitch_offset_deg;
static float s_front_roll_sum_deg;
static float s_front_pitch_sum_deg;
static float s_rear_roll_sum_deg;
static float s_rear_pitch_sum_deg;
static unsigned s_front_tare_samples;
static unsigned s_rear_tare_samples;
static bool s_front_tare_complete;
static bool s_rear_tare_complete;
static SemaphoreHandle_t s_data_mutex;
static int64_t s_last_rx_us;
static int64_t s_last_imu_log_us;
static int64_t s_last_imu_sample_us;
static int64_t s_last_slave_log_us;
static volatile bool s_imu_ready;
static volatile bool s_espnow_ready;
static volatile bool s_display_ready;
static bool s_sd_ready;
SemaphoreHandle_t s_ap_send_sem;
static float s_vehicle_track_m = VEHICLE_TRACK_M;
static float s_cg_height_empty_m = CG_HEIGHT_EMPTY_M;
static float s_cg_height_full_m = CG_HEIGHT_FULL_M;
static float s_full_tank_value = WATER_SENSOR_FULL_SCALE;
static float s_tank_length_cm = WATER_TANK_LENGTH_CM;
static float s_tank_width_cm = WATER_TANK_WIDTH_CM;
static float s_tank_height_cm = WATER_TANK_HEIGHT_CM;

static void provide_network_status(network_status_payload_t *status)
{
    if (status == NULL) return;
    status->roll_deg = s_telemetry.roll_deg;
    status->pitch_deg = s_telemetry.pitch_deg;
    status->lateral_accel_m_s2 = s_telemetry.lateral_accel_m_s2;
    status->gps_speed_kmh = s_telemetry.speed_kmh;
    status->water_level = s_telemetry.water_level_cm;
    status->ltr = s_telemetry.ltr;
    status->ltr_static = s_telemetry.ltr_static;
    status->ltr_dynamic = s_telemetry.ltr_dynamic;
    status->bank_angle_deg = s_telemetry.bank_angle_deg;
    status->suspension_roll_deg = s_telemetry.suspension_roll_deg;
    if (s_telemetry.imu_local_ok) status->flags |= NETWORK_FLAG_IMU_OK;
    if (s_telemetry.speed_valid) status->flags |= NETWORK_FLAG_GPS_OK;
    if (s_telemetry.water_ok) status->flags |= NETWORK_FLAG_WATER_OK;
    if (CISTERNA_TEST_MODE) status->flags |= NETWORK_FLAG_WATER_SIMULATED;
    if (s_config_valid) status->flags |= NETWORK_FLAG_CONFIG_VALID;
    if (s_ltr_degraded) status->flags |= NETWORK_FLAG_LTR_DEGRADED;
    if (s_long_wave_risk) status->flags |= NETWORK_FLAG_LONG_WAVE_RISK;
    if (s_sd_ready) status->flags |= NETWORK_FLAG_SD_OK;
    if (s_rear_data.sensor_status_flags & STATUS_WATER_SIMULATED) {
        status->flags |= NETWORK_FLAG_WATER_SIMULATED;
    }
}

void network_apply_tank_config(const network_tank_config_payload_t *config)
{
    if (config == NULL) return;
    if (config->config_version != NETWORK_TANK_CONFIG_VERSION ||
        !isfinite(config->vehicle_track_m) ||
        !isfinite(config->semi_axis_a_m) ||
        !isfinite(config->semi_axis_b_m) ||
        !isfinite(config->tank_length_m) ||
        !isfinite(config->cg_height_empty_m) ||
        !isfinite(config->cg_height_full_m) ||
        !isfinite(config->full_tank_value) ||
        !isfinite(config->liquid_density_kg_m3) ||
        !isfinite(config->damping_ratio_lat) ||
        !isfinite(config->damping_ratio_long) ||
        !isfinite(config->slosh_enable_pct) ||
        !isfinite(config->slosh_disable_low_pct) ||
        !isfinite(config->slosh_disable_high_pct) ||
        config->vehicle_track_m <= 0.0f ||
        config->semi_axis_a_m <= 0.0f ||
        config->semi_axis_b_m <= 0.0f ||
        config->tank_length_m <= 0.0f ||
        config->cg_height_empty_m <= 0.0f ||
        config->cg_height_full_m < config->cg_height_empty_m ||
        config->full_tank_value <= 0.0f ||
        config->liquid_density_kg_m3 <= 0.0f ||
        config->damping_ratio_lat < 0.0f ||
        config->damping_ratio_long < 0.0f ||
        config->damping_ratio_lat > 1.0f ||
        config->damping_ratio_long > 1.0f ||
        config->slosh_enable_pct < 0.0f ||
        config->slosh_enable_pct > 100.0f ||
        config->slosh_disable_low_pct < 0.0f ||
        config->slosh_disable_high_pct <= config->slosh_disable_low_pct ||
        config->slosh_disable_high_pct > 100.0f) {
        ESP_LOGW(TAG, "Configuracion dinamica del tanque rechazada");
        return;
    }
    if (s_data_mutex != NULL && xSemaphoreTake(s_data_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "No se pudo aplicar la configuracion dinamica del tanque");
        return;
    }
    s_vehicle_track_m = config->vehicle_track_m;
    s_cg_height_empty_m = config->cg_height_empty_m;
    s_cg_height_full_m = config->cg_height_full_m;
    s_full_tank_value = config->full_tank_value;
    s_semi_axis_a_m = config->semi_axis_a_m;
    s_semi_axis_b_m = config->semi_axis_b_m;
    s_tank_length_m = config->tank_length_m;
    s_liquid_density_kg_m3 = config->liquid_density_kg_m3;
    s_damping_ratio_lat = config->damping_ratio_lat;
    s_damping_ratio_long = config->damping_ratio_long;
    s_slosh_enable_pct = config->slosh_enable_pct;
    s_slosh_disable_low_pct = config->slosh_disable_low_pct;
    s_slosh_disable_high_pct = config->slosh_disable_high_pct;
    s_tank_length_cm = s_tank_length_m * 100.0f;
    s_tank_width_cm = s_semi_axis_a_m * 200.0f;
    s_tank_height_cm = s_semi_axis_b_m * 200.0f;
    s_config_valid = true;
    if (s_data_mutex != NULL) xSemaphoreGive(s_data_mutex);
    ESP_LOGI(TAG, "Tanque actualizado: track=%.2f emptyCG=%.2f fullCG=%.2f full=%0.2f",
             s_vehicle_track_m, s_cg_height_empty_m, s_cg_height_full_m, s_full_tank_value);
}

static void network_init_task(void *arg)
{
    (void)arg;
    while (!s_espnow_ready) vTaskDelay(pdMS_TO_TICKS(100));
    while (network_client_start(NETWORK_NODE_MASTER, provide_network_status) != ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    vTaskDelete(NULL);
}

void telemetry_tare_sensors(void)
{
    const bool mutex_available = s_data_mutex != NULL;
    if (mutex_available && xSemaphoreTake(s_data_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Unable to lock sensor data for tare reset");
        return;
    }

    s_front_roll_offset_deg = 0.0f;
    s_front_pitch_offset_deg = 0.0f;
    s_rear_roll_offset_deg = 0.0f;
    s_rear_pitch_offset_deg = 0.0f;
    s_front_roll_sum_deg = 0.0f;
    s_front_pitch_sum_deg = 0.0f;
    s_rear_roll_sum_deg = 0.0f;
    s_rear_pitch_sum_deg = 0.0f;
    s_front_tare_samples = 0;
    s_rear_tare_samples = 0;
    s_front_tare_complete = false;
    s_rear_tare_complete = false;

    if (mutex_available) xSemaphoreGive(s_data_mutex);
    ESP_LOGI(TAG, "Sensor mounting tare reset; collecting %u front and %u rear samples",
             FRONT_TARE_SAMPLE_COUNT, REAR_TARE_SAMPLE_COUNT);
}

static void calibrate_front_mounting_offset(const imu_bno085_sample_t *sample)
{
    if (s_front_tare_complete || sample == NULL || !sample->online ||
        !sample->has_orientation || !isfinite(sample->roll_deg) ||
        !isfinite(sample->pitch_deg)) {
        return;
    }
    if (sample->has_gyro) {
        const float gx = sample->gyro_rad_s[0];
        const float gy = sample->gyro_rad_s[1];
        const float gz = sample->gyro_rad_s[2];
        if (!isfinite(gx) || !isfinite(gy) || !isfinite(gz) ||
            sqrtf(gx * gx + gy * gy + gz * gz) >= TARE_STATIONARY_GYRO_RAD_S) {
            return;
        }
    }

    s_front_roll_sum_deg += sample->roll_deg;
    s_front_pitch_sum_deg += sample->pitch_deg;
    ++s_front_tare_samples;
    s_front_roll_offset_deg = s_front_roll_sum_deg / (float)s_front_tare_samples;
    s_front_pitch_offset_deg = s_front_pitch_sum_deg / (float)s_front_tare_samples;
    if (s_front_tare_samples >= FRONT_TARE_SAMPLE_COUNT) {
        s_front_tare_complete = true;
        ESP_LOGI(TAG, "Front IMU mounting tare complete: roll=%.2f pitch=%.2f deg",
                 s_front_roll_offset_deg, s_front_pitch_offset_deg);
    }
}

static void calibrate_rear_mounting_offset(const TankPayload *sample)
{
    if (s_rear_tare_complete || sample == NULL ||
        !(sample->sensor_status_flags & STATUS_IMU2_OK) ||
        !isfinite(sample->roll_1) || !isfinite(sample->pitch_1)) {
        return;
    }

    s_rear_roll_sum_deg += sample->roll_1;
    s_rear_pitch_sum_deg += sample->pitch_1;
    ++s_rear_tare_samples;
    s_rear_roll_offset_deg = s_rear_roll_sum_deg / (float)s_rear_tare_samples;
    s_rear_pitch_offset_deg = s_rear_pitch_sum_deg / (float)s_rear_tare_samples;
    if (s_rear_tare_samples >= REAR_TARE_SAMPLE_COUNT) {
        s_rear_tare_complete = true;
        ESP_LOGI(TAG, "Rear IMU mounting tare complete: roll=%.2f pitch=%.2f deg",
                 s_rear_roll_offset_deg, s_rear_pitch_offset_deg);
    }
}

static void display_init_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "Waiting %u ms before LCD startup", (unsigned)LCD_STARTUP_DELAY_MS);
    vTaskDelay(pdMS_TO_TICKS(LCD_STARTUP_DELAY_MS));
    ESP_LOGI(TAG, "Starting LCD/LVGL");
    display_module_start();
    s_display_ready = true;
    ESP_LOGI(TAG, "LCD/LVGL initialization returned");
    vTaskDelete(NULL);
}

static void imu_init_task(void *arg)
{
    (void)arg;
    while (!imu_module_start()) {
        ESP_LOGW(TAG, "IMU no disponible; reintento en 5 segundos");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
    s_imu_ready = true;
    vTaskDelete(NULL);
}

static void espnow_init_task(void *arg)
{
    (void)arg;
    while (field_espnow_init(ESPNOW_CHANNEL) != ESP_OK) {
        ESP_LOGW(TAG, "ESP-NOW no disponible; reintento en 5 segundos");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
    s_espnow_ready = true;
    uint8_t mac[FIELD_MAC_LENGTH];
    if (field_espnow_get_mac(mac) == ESP_OK) {
        ESP_LOGI(TAG, "Sistema de advertencia listo; MAC=" MACSTR, MAC2STR(mac));
    }
    vTaskDelete(NULL);
}

static float clampf_local(float value, float minimum, float maximum)
{
    return fminf(fmaxf(value, minimum), maximum);
}

static float water_liters(float level_cm)
{
    const float level = clampf_local(level_cm, 0.0f, s_tank_height_cm);
    const float semi_width = s_tank_width_cm / 2.0f;
    const float semi_height = s_tank_height_cm / 2.0f;
    const float normalized_height = clampf_local((level - semi_height) / semi_height,
                                                  -1.0f, 1.0f);
    const float area_cm2 = semi_width * semi_height *
        (asinf(normalized_height) + 1.57079632679489661923f +
         normalized_height * sqrtf(fmaxf(0.0f, 1.0f -
                                           normalized_height * normalized_height)));
    return fmaxf(0.0f, area_cm2 * s_tank_length_cm / 1000.0f);
}

static void calculate_telemetry(int64_t now_us, float speed_kmh, bool speed_valid)
{
    const bool rear_online = s_last_rx_us > 0 && now_us - s_last_rx_us <= SENSOR_TIMEOUT_US;
    const bool remote_ok = rear_online && (s_rear_data.sensor_status_flags & STATUS_IMU2_OK) &&
                           isfinite(s_rear_data.roll_1) && isfinite(s_rear_data.ay_1);
    const bool local_ok = s_front_imu.online && s_front_imu.has_orientation &&
                          isfinite(s_front_imu.roll_deg);
    const bool local_pitch_ok = local_ok && isfinite(s_front_imu.pitch_deg);
    const bool local_accel_ok = local_ok && s_front_imu.has_linear_acceleration;
    const float front_ay = local_accel_ok &&
        isfinite(s_front_imu.linear_accel_m_s2[1]) ?
        s_front_imu.linear_accel_m_s2[1] : 0.0f;
    const float front_ax = local_accel_ok &&
        isfinite(s_front_imu.linear_accel_m_s2[0]) ?
        s_front_imu.linear_accel_m_s2[0] : 0.0f;
    const float rear_ay = remote_ok ? s_rear_data.ay_1 : 0.0f;
    const float front_roll = local_ok ?
        s_front_imu.roll_deg - s_front_roll_offset_deg : 0.0f;
    const float rear_roll = remote_ok ?
        s_rear_data.roll_1 - s_rear_roll_offset_deg : 0.0f;
    float roll = 0.0f, ay = 0.0f;
    unsigned count = 0;
    if (local_ok) { roll += front_roll; ay += front_ay; ++count; }
    if (remote_ok) { roll += rear_roll; ay += rear_ay; ++count; }
    if (count > 0) { roll /= (float)count; ay /= (float)count; }

    const bool use_water_override = WATER_LEVEL_OVERRIDE >= 0;
    const bool slave_water_ok = rear_online &&
        (s_rear_data.sensor_status_flags & STATUS_RS485_OK) &&
        isfinite(s_rear_data.water_level);
    const bool water_ok = use_water_override || slave_water_ok;
    const float sensor_level = use_water_override ?
        (float)WATER_LEVEL_OVERRIDE :
        (slave_water_ok ?
            clampf_local(s_rear_data.water_level, 0.0f, s_full_tank_value) : 0.0f);
    const float level = s_tank_height_cm * sensor_level / s_full_tank_value;
    const float tank_capacity_liters = water_liters(s_tank_height_cm);
    const float water_volume_liters = water_liters(level);
    const float water_percent = tank_capacity_liters > 0.0f ?
        100.0f * water_volume_liters / tank_capacity_liters : 0.0f;
    const float cg_height = s_cg_height_empty_m +
        (s_cg_height_full_m - s_cg_height_empty_m) * water_percent / 100.0f;

    float risk_roll = roll;
    if (local_ok && fabsf(front_roll) > fabsf(risk_roll)) {
        risk_roll = front_roll;
    }
    if (remote_ok && fabsf(rear_roll) > fabsf(risk_roll)) {
        risk_roll = rear_roll;
    }

    const bool remote_pitch_ok = remote_ok && isfinite(s_rear_data.pitch_1);
    const float front_pitch = local_pitch_ok ?
        s_front_imu.pitch_deg - s_front_pitch_offset_deg : 0.0f;
    const float rear_pitch = remote_pitch_ok ?
        s_rear_data.pitch_1 - s_rear_pitch_offset_deg : 0.0f;
    float measured_pitch = front_pitch;
    if (remote_pitch_ok && fabsf(rear_pitch) > fabsf(measured_pitch)) {
        measured_pitch = rear_pitch;
    }
    const float risk_pitch = copysignf(
        fmaxf(fabsf(measured_pitch) - PITCH_DEADBAND_DEG, 0.0f), measured_pitch);

    float local_risk_accel = front_ay;
    if (speed_valid && local_ok && s_front_imu.has_gyro && isfinite(speed_kmh) &&
        isfinite(s_front_imu.gyro_rad_s[2])) {
        const float turn_lateral_accel = (speed_kmh / 3.6f) * s_front_imu.gyro_rad_s[2];
        if (fabsf(turn_lateral_accel) > fabsf(local_risk_accel)) {
            local_risk_accel = turn_lateral_accel;
        }
    }
    const float base_risk_lateral_accel =
        fabsf(local_risk_accel) > fabsf(rear_ay) ? local_risk_accel : rear_ay;

    const bool pendulum_active = water_ok &&
        water_percent >= s_slosh_enable_pct &&
        water_percent < s_slosh_disable_high_pct &&
        !(s_water_pendulum_active &&
          water_percent <= s_slosh_disable_low_pct);
    if (pendulum_active != s_water_pendulum_active) {
        ESP_LOGI(TAG, "Tank dynamics: %s at %.1f%% fill",
                 pendulum_active ? "PENDULUM" : "SOLID", water_percent);
    }
    float effective_cg_height = cg_height;
    float slosh_ay = 0.0f;
    float long_slosh_ax = 0.0f;
    float water_mass_kg = water_volume_liters *
                          s_liquid_density_kg_m3 / 1000.0f;
    trammel_params_t trammel = {0};
    if (pendulum_active) {
        const float fill_ratio = water_percent / 100.0f;
        (void)pendulum_compute_trammel_params(fill_ratio, s_semi_axis_a_m,
                                               s_semi_axis_b_m, water_mass_kg,
                                               &trammel);
    }
    if (pendulum_active) {
        if (!s_water_pendulum_active) {
            s_water_pendulum_state = (pendulum_state_t){0};
            s_long_pendulum_state = (pendulum_state_t){0};
            s_last_pendulum_step_us = now_us;
        }
        float dt_s = s_last_pendulum_step_us > 0 ?
            (float)(now_us - s_last_pendulum_step_us) / 1000000.0f : 0.005f;
        dt_s = clampf_local(dt_s, 0.001f, 0.02f);
        s_last_pendulum_step_us = now_us;
        const float lateral_length = trammel.is_valid ?
            trammel.bp_m : PENDULUM_LENGTH_M;
        const float lateral_mass = trammel.is_valid ?
            trammel.mp_kg : water_mass_kg;
        const float lateral_inertia = trammel.is_valid ?
            lateral_mass * lateral_length * lateral_length :
            water_mass_kg * PENDULUM_LENGTH_M * PENDULUM_LENGTH_M;
        const float lateral_damping = trammel.is_valid ?
            2.0f * s_damping_ratio_lat * trammel.omega_n_rad_s :
            PENDULUM_DAMPING;
        const pendulum_model_t model = {
            .mass_kg = lateral_mass,
            .pivot_to_cg_m = lateral_length,
            .gravity_m_s2 = GRAVITY_M_S2,
            .damping_coefficient = lateral_damping,
        };
        const float inertia_about_pivot = lateral_inertia;
        if (lateral_mass > 0.0f) {
            if (!pendulum_simulate_vehicle_step(&model, &s_water_pendulum_state,
                                                inertia_about_pivot,
                                                risk_roll * 0.01745329251994329577f,
                                                base_risk_lateral_accel, dt_s)) {
                if (!s_pendulum_error_reported) {
                    ESP_LOGE(TAG, "Pendulum simulation step failed; resetting fluid state");
                    s_pendulum_error_reported = true;
                }
                s_water_pendulum_state = (pendulum_state_t){0};
            } else {
                s_pendulum_error_reported = false;
                float pendulum_bob_accel = 0.0f;
                if (pendulum_lateral_acceleration(&model, &s_water_pendulum_state,
                                                  inertia_about_pivot,
                                                  risk_roll * 0.01745329251994329577f,
                                                  base_risk_lateral_accel,
                                                  &pendulum_bob_accel)) {
                    slosh_ay = pendulum_bob_accel *
                               (water_mass_kg / VEHICLE_GROSS_MASS_KG);
                } else if (!s_pendulum_error_reported) {
                    ESP_LOGE(TAG, "Pendulum slosh acceleration calculation failed");
                    s_pendulum_error_reported = true;
                }
            }
            const float tank_ratio = s_semi_axis_b_m > 0.0f ?
                s_tank_length_m / (2.0f * s_semi_axis_a_m) : 0.0f;
            s_long_wave_risk = isfinite(tank_ratio) && tank_ratio > 2.5f;
            const float long_length = (tank_ratio <= 2.5f && isfinite(tank_ratio) &&
                                       tank_ratio > 0.0f) ?
                s_tank_length_m / 2.0f : PENDULUM_LONG_LENGTH_M;
            const pendulum_model_t long_model = {
                .mass_kg = lateral_mass,
                .pivot_to_cg_m = long_length,
                .gravity_m_s2 = GRAVITY_M_S2,
                .damping_coefficient =
                    2.0f * s_damping_ratio_long *
                    sqrtf(fmaxf(GRAVITY_M_S2 / fmaxf(long_length, 0.001f), 0.0f)),
            };
            const float long_inertia_about_pivot = lateral_mass *
                long_length * long_length;
            if (!pendulum_simulate_vehicle_step(&long_model, &s_long_pendulum_state,
                                                long_inertia_about_pivot,
                                                risk_pitch * DEG_TO_RAD,
                                                front_ax, dt_s)) {
                if (!s_pendulum_error_reported) {
                    ESP_LOGE(TAG, "Longitudinal pendulum simulation step failed; resetting fluid state");
                    s_pendulum_error_reported = true;
                }
                s_long_pendulum_state = (pendulum_state_t){0};
            } else {
                float pendulum_bob_ax = 0.0f;
                if (pendulum_lateral_acceleration(&long_model, &s_long_pendulum_state,
                                                  long_inertia_about_pivot,
                                                  risk_pitch * DEG_TO_RAD, front_ax,
                                                  &pendulum_bob_ax)) {
                    long_slosh_ax = pendulum_bob_ax *
                                    (water_mass_kg / VEHICLE_GROSS_MASS_KG);
                } else if (!s_pendulum_error_reported) {
                    ESP_LOGE(TAG, "Longitudinal pendulum slosh calculation failed");
                    s_pendulum_error_reported = true;
                }
            }
            effective_cg_height += water_percent / 100.0f * PENDULUM_LENGTH_M *
                                   fabsf(sinf(s_water_pendulum_state.angle_rad));
        }
    } else {
        s_water_pendulum_state = (pendulum_state_t){0};
        s_long_pendulum_state = (pendulum_state_t){0};
        s_last_pendulum_step_us = now_us;
    }
    s_water_pendulum_active = pendulum_active;
    const float risk_lateral_accel = base_risk_lateral_accel + slosh_ay;

    float dt_bank_s = s_last_pendulum_step_us > 0 ?
        (float)(now_us - s_last_pendulum_step_us) / 1000000.0f : 0.010f;
    dt_bank_s = clampf_local(dt_bank_s, 0.001f, 0.020f);
    const float yaw_rate_rps = local_ok && s_front_imu.has_gyro &&
        isfinite(s_front_imu.gyro_rad_s[2]) ?
        s_front_imu.gyro_rad_s[2] : 0.0f;
    const float vx_mps = (speed_valid && isfinite(speed_kmh) &&
                          speed_kmh >= 0.0f) ? speed_kmh / 3.6f : 0.0f;
    (void)ltr_estimate_bank_angle(vx_mps, yaw_rate_rps,
                                  base_risk_lateral_accel,
                                  speed_valid, dt_bank_s,
                                  &s_bank_estimator);
    const float bank_angle_deg = s_bank_estimator.bank_angle_rad * 57.29577951308232f;
    const float suspension_roll_deg = risk_roll - bank_angle_deg;

    /* Signed LTR model from the specification. Linear acceleration already
       excludes gravity, so do not subtract g from ay a second time. */
    const ltr_config_t ltr_config = {
        .cg_height_m = effective_cg_height,
        .track_width_m = s_vehicle_track_m,
        .gravity_m_s2 = GRAVITY_M_S2,
        .minimum_value = -1.0f,
        .maximum_value = 1.0f,
    };
    float ltr = 0.0f;
    s_imu1_ltr = (ltr_result_t){0};
    s_imu2_ltr = (ltr_result_t){0};
    if (local_ok) {
        (void)ltr_calculate(&ltr_config, front_roll,
                            local_risk_accel + slosh_ay, &s_imu1_ltr);
        if (s_imu1_ltr.valid) ltr = s_imu1_ltr.value;
    }
    if (remote_ok) {
        (void)ltr_calculate(&ltr_config, rear_roll,
                            rear_ay + slosh_ay, &s_imu2_ltr);
        if (s_imu2_ltr.valid && fabsf(s_imu2_ltr.value) > fabsf(ltr)) {
            ltr = s_imu2_ltr.value;
        }
    }

    ltr_result_t dynamic_ltr_result = {0};
    s_ltr_degraded = false;
    if (pendulum_active && trammel.is_valid) {
        const float roll_rad = risk_roll * DEG_TO_RAD;
        const float effective_accel = risk_lateral_accel * cosf(roll_rad) +
                                      GRAVITY_M_S2 * sinf(roll_rad);
        const float total_mass_kg = VEHICLE_GROSS_MASS_KG + trammel.mf_kg;
        const float chassis_moment = total_mass_kg * effective_accel *
                                     effective_cg_height;
        const float ry = trammel.ap_m *
                         sinf(s_water_pendulum_state.angle_rad);
        const float rz = trammel.bp_m *
                         cosf(s_water_pendulum_state.angle_rad);
        const float fy = trammel.mp_kg * slosh_ay;
        const float fz = trammel.mp_kg * GRAVITY_M_S2;
        const float slosh_moment = rz * fy - ry * fz;
        s_ltr_degraded = !ltr_calculate_dynamic(
            &ltr_config, risk_roll, risk_lateral_accel,
            chassis_moment, slosh_moment, total_mass_kg,
            &dynamic_ltr_result);
    } else {
        s_ltr_degraded = pendulum_active && !trammel.is_valid;
        dynamic_ltr_result.valid = s_imu1_ltr.valid || s_imu2_ltr.valid;
        dynamic_ltr_result.value = ltr;
    }
    if (s_ltr_degraded) {
        ESP_LOGW(TAG, "LTR dynamic model degraded; static fallback active");
        dynamic_ltr_result = s_imu1_ltr.valid ? s_imu1_ltr :
                              s_imu2_ltr.valid ? s_imu2_ltr :
                              (ltr_result_t){.valid=true,.value=ltr};
    }

    float max_safe_ay_warn = 0.0f;
    float max_safe_ay_flip = 0.0f;
    const bool safe_warn_valid = ltr_max_safe_lateral_accel(
        &ltr_config, risk_roll, STATIC_RISK_LTR_THRESHOLD, &max_safe_ay_warn);
    const bool safe_flip_valid = ltr_max_safe_lateral_accel(
        &ltr_config, risk_roll, 1.0f, &max_safe_ay_flip);
    if ((!safe_warn_valid || !safe_flip_valid) && !s_ltr_limit_error_reported) {
        ESP_LOGW(TAG, "Unable to calculate safe lateral acceleration for roll=%.2f deg",
                 risk_roll);
        s_ltr_limit_error_reported = true;
    } else if (safe_warn_valid && safe_flip_valid) {
        s_ltr_limit_error_reported = false;
    }

    const float pitch_cornering_factor = fmaxf(cosf(fabsf(risk_pitch) * DEG_TO_RAD), 0.0f);
    max_safe_ay_warn *= pitch_cornering_factor;
    max_safe_ay_flip *= pitch_cornering_factor;
    const float max_safe_speed_kmh =
        sqrtf(fmaxf(max_safe_ay_warn, 0.2f) * REFERENCE_TURN_RADIUS_M) * 3.6f;
    const bool valid_speed = speed_valid && isfinite(speed_kmh) && speed_kmh >= 0.0f;
    const float speed_m_s = valid_speed ? speed_kmh / 3.6f : 0.0f;
    const float min_turn_radius_m = valid_speed ?
        (speed_m_s * speed_m_s) / fmaxf(max_safe_ay_flip, 0.2f) : 0.0f;
    const bool speed_turn_risk = valid_speed && speed_kmh > 5.0f &&
                                 speed_kmh > max_safe_speed_kmh;

    float predicted_roll_deg = risk_roll;
    if (local_ok && s_front_imu.has_gyro && isfinite(s_front_imu.gyro_rad_s[0])) {
        const float gyro_x_deg_s = s_front_imu.gyro_rad_s[0] *
                                   57.29577951308232f;
        predicted_roll_deg += gyro_x_deg_s * ROLL_PREDICTION_HORIZON_S;
    }
    ltr_result_t predicted_ltr_result = {0};
    const bool predicted_ltr_valid = count > 0 &&
        ltr_calculate(&ltr_config, predicted_roll_deg, risk_lateral_accel,
                      &predicted_ltr_result);
    const float predicted_ltr = predicted_ltr_valid ? predicted_ltr_result.value : 0.0f;
    const bool static_risk = fabsf(ltr) >= STATIC_RISK_LTR_THRESHOLD ||
                             fabsf(risk_roll) >= STATIC_RISK_ROLL_THRESHOLD_DEG;
    const bool dynamic_risk = predicted_ltr_valid &&
        (fabsf(predicted_ltr) >= STATIC_RISK_LTR_THRESHOLD ||
         fabsf(predicted_roll_deg) >= STATIC_RISK_ROLL_THRESHOLD_DEG);

    const float longitudinal_risk_accel = front_ax + long_slosh_ax;
    const ltr_config_t pitch_ltr_config = {
        .cg_height_m = effective_cg_height,
        .track_width_m = VEHICLE_WHEELBASE_M,
        .gravity_m_s2 = GRAVITY_M_S2,
        .minimum_value = -1.0f,
        .maximum_value = 1.0f,
    };
    const bool pitch_data_valid = local_pitch_ok || remote_pitch_ok;
    ltr_result_t pitch_ltr_result = {0};
    const bool pitch_ltr_valid = pitch_data_valid &&
        ltr_calculate(&pitch_ltr_config, risk_pitch, longitudinal_risk_accel,
                      &pitch_ltr_result);
    const float pitch_ltr = pitch_ltr_valid ? pitch_ltr_result.value : 0.0f;
    float predicted_pitch_deg = risk_pitch;
    if (local_ok && s_front_imu.has_gyro && isfinite(s_front_imu.gyro_rad_s[1])) {
        const float gyro_y_deg_s = s_front_imu.gyro_rad_s[1] * 57.29577951308232f;
        predicted_pitch_deg += gyro_y_deg_s * ROLL_PREDICTION_HORIZON_S;
    }
    ltr_result_t predicted_pitch_ltr_result = {0};
    const bool predicted_pitch_ltr_valid = pitch_data_valid &&
        ltr_calculate(&pitch_ltr_config, predicted_pitch_deg,
                      longitudinal_risk_accel, &predicted_pitch_ltr_result);
    const float predicted_pitch_ltr = predicted_pitch_ltr_valid ?
        predicted_pitch_ltr_result.value : 0.0f;
    const bool pitch_risk = pitch_data_valid &&
        (fabsf(pitch_ltr) >= PITCH_LTR_WARNING_THRESHOLD ||
         fabsf(predicted_pitch_ltr) >= PITCH_LTR_WARNING_THRESHOLD ||
         fabsf(risk_pitch) >= STATIC_PITCH_RISK_THRESHOLD_DEG);

    s_telemetry.roll_deg = risk_roll;
    s_telemetry.pitch_deg = measured_pitch;
    s_telemetry.lateral_accel_m_s2 = risk_lateral_accel;
    s_telemetry.ltr = ltr;
    s_telemetry.ltr_static = ltr;
    s_telemetry.ltr_dynamic = dynamic_ltr_result.valid ?
                              dynamic_ltr_result.value : ltr;
    s_telemetry.bank_angle_deg = bank_angle_deg;
    s_telemetry.suspension_roll_deg = suspension_roll_deg;
    s_telemetry.pitch_ltr = pitch_ltr;
    s_telemetry.predicted_pitch_ltr = predicted_pitch_ltr;
    s_telemetry.max_safe_speed_kmh = max_safe_speed_kmh;
    s_telemetry.min_turn_radius_m = min_turn_radius_m;
    s_telemetry.predicted_ltr = predicted_ltr;
    s_telemetry.critical_ltr_threshold = CRITICAL_LTR_THRESHOLD;
    s_telemetry.water_level_cm = level;
    s_telemetry.water_gallons = water_liters(level) / 3.785411784f;
    s_telemetry.water_percent = water_percent;
    s_telemetry.imu_local_ok = local_ok;
    s_telemetry.imu_remote_ok = remote_ok;
    s_telemetry.water_ok = water_ok;
    s_telemetry.rear_online = rear_online;
    s_telemetry.static_risk = static_risk;
    s_telemetry.speed_turn_risk = speed_turn_risk;
    s_telemetry.dynamic_risk = dynamic_risk;
    s_telemetry.pitch_risk = pitch_risk;
}

static void sensor_task(void *arg)
{
    (void)arg;
    while (true) {
        field_espnow_rx_t received = {0};
        TankPayload slave_sample = {0};
        bool new_slave_sample = false;
        if (s_espnow_ready) {
            bool have_rx = field_espnow_receive(&received, pdMS_TO_TICKS(SLAVE_RX_WAIT_MS));
            unsigned received_count = 0U;
            while (have_rx && received_count < 8U) {
                if (received.data_len == sizeof(TankPayload)) {
                    TankPayload received_sample = {0};
                    memcpy(&received_sample, received.raw_data, sizeof(received_sample));
                    if (xSemaphoreTake(s_data_mutex, portMAX_DELAY) == pdTRUE) {
                        s_rear_data = received_sample;
                        s_last_rx_us = esp_timer_get_time();
                        calibrate_rear_mounting_offset(&received_sample);
                        slave_sample = received_sample;
                        new_slave_sample = true;
                        xSemaphoreGive(s_data_mutex);
                    } else {
                        ESP_LOGE(TAG, "Unable to lock sensor data for rear IMU update");
                    }
                } else {
                    ESP_LOGW(TAG, "ESP-NOW frame ignored: expected %u bytes, got %u",
                             (unsigned)sizeof(TankPayload), (unsigned)received.data_len);
                }
                have_rx = field_espnow_receive(&received, 0);
                ++received_count;
            }
            const int64_t now_us = esp_timer_get_time();
            if (new_slave_sample && now_us - s_last_slave_log_us >= SLAVE_LOG_INTERVAL_US) {
                ESP_LOGI(TAG, "ESP-NOW RX: roll=%.2f pitch=%.2f ay=%.2f water=%.2f flags=0x%02X",
                         slave_sample.roll_1, slave_sample.pitch_1, slave_sample.ay_1,
                         slave_sample.water_level, (unsigned)slave_sample.sensor_status_flags);
                s_last_slave_log_us = now_us;
            }
        }
        imu_bno085_sample_t sample = {0};
        const bool got_imu_sample = s_imu_ready && imu_module_get_sample(&sample);
        bool new_imu_sample = false;
        if (got_imu_sample) {
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
            gps_module_snapshot_t gps = {0};
            const bool have_gps = gps_module_get_snapshot(&gps);
            const bool speed_valid = have_gps && gps.speed_valid &&
                                     gps.validation.usable_for_speed;
            const float speed_kmh = speed_valid ? gps.displayed_speed_kmh : 0.0f;
            if (got_imu_sample) {
                s_front_imu = sample;
                if (new_imu_sample) calibrate_front_mounting_offset(&sample);
            }
            calculate_telemetry(esp_timer_get_time(), speed_kmh, speed_valid);
            xSemaphoreGive(s_data_mutex);
            xSemaphoreGive(s_ap_send_sem);
        }
        vTaskDelay(pdMS_TO_TICKS(SENSOR_TASK_PERIOD_MS));
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
        telemetry.speed_valid = have_gps && gps.speed_valid &&
                                gps.validation.usable_for_speed;
        if (s_display_ready) display_module_update(have_gps ? &gps : NULL, &telemetry);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
    s_data_mutex = xSemaphoreCreateMutex();
    if (s_data_mutex == NULL) { ESP_LOGE(TAG, "No se pudo crear el mutex"); return; }
    s_ap_send_sem = xSemaphoreCreateBinary();
    if (s_ap_send_sem == NULL) { ESP_LOGE(TAG, "No se pudo crear el semaforo de envio"); return; }
    ESP_ERROR_CHECK(board_shared_spi_init());
    gps_module_start();
    xTaskCreate(imu_init_task, "imu_init", 4096, NULL, 3, NULL);
    xTaskCreate(display_init_task, "display_init", 8192, NULL, 2, NULL);
    xTaskCreate(espnow_init_task, "espnow_init", 4096, NULL, 3, NULL);
    xTaskCreate(sensor_task, "rollover_sensor", 8192, NULL, 5, NULL);
    xTaskCreate(gui_task, "rollover_gui", 4096, NULL, 2, NULL);
    xTaskCreate(network_init_task, "network_init", 4096, NULL, 2, NULL);
}
