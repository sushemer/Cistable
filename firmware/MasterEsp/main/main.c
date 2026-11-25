#include "field_espnow.h"
#include "board_spi_pins.h"
#include "tank_payload.h"
#include "modules/display/display_module.h"
#include "modules/imu/imu_module.h"
#include "modules/gps/gps_module.h"
#include "modules/gps/gps_imu_fusion.h"
#include "modules/ltr/ltr_module.h"
#include "modules/pendulum/pendulum_module.h"
#include "modules/sd_logger/sd_logger.h"
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
#define VEHICLE_WHEELBASE_M 3.80f
#define VEHICLE_GROSS_MASS_KG 5443.0f
#define CG_HEIGHT_EMPTY_M 1.20f
#define CG_HEIGHT_FULL_M 1.50f
#define WATER_TANK_LENGTH_CM 304.0f
#define WATER_TANK_WIDTH_CM 220.0f
#define WATER_TANK_HEIGHT_CM 124.0f
#define WATER_SENSOR_FULL_SCALE 120.0f
#define WATER_LEVEL_OVERRIDE 65
#if WATER_LEVEL_OVERRIDE < -1 || WATER_LEVEL_OVERRIDE > 120
#error "WATER_LEVEL_OVERRIDE must be -1 (slave reading) or a value from 0 to 120"
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
#define SLAVE_RX_WAIT_MS 5U
#define SLAVE_LOG_INTERVAL_US 1000000LL
#define LCD_STARTUP_DELAY_MS 2000U

static const char *TAG = "MASTER";
static TankPayload s_rear_data;
static imu_bno085_sample_t s_front_imu;
static display_telemetry_t s_telemetry;
static gps_imu_fusion_result_t s_gps_imu_fusion = {
    .gps_speed_kmh = NAN,
    .corrected_speed_kmh = NAN,
    .speed_delta_kmh = NAN,
    .acceleration_norm_m_s2 = NAN,
    .state = "GPS_SIN_DATOS",
};
static ltr_result_t s_imu1_ltr;
static ltr_result_t s_imu2_ltr;
static pendulum_state_t s_water_pendulum_state;
static pendulum_state_t s_long_pendulum_state;
static int64_t s_last_pendulum_step_us;
static bool s_water_pendulum_active;
static bool s_pendulum_error_reported;
static bool s_ltr_limit_error_reported;
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
static int64_t s_last_gps_log_us;
static volatile bool s_imu_ready;
static volatile bool s_espnow_ready;
static volatile bool s_display_ready;
static int64_t s_last_event_us;

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
    sd_logger_wait_startup();
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
    const float level = clampf_local(level_cm, 0.0f, WATER_TANK_HEIGHT_CM);
    const float semi_width = WATER_TANK_WIDTH_CM / 2.0f;
    const float semi_height = WATER_TANK_HEIGHT_CM / 2.0f;
    const float normalized_height = clampf_local((level - semi_height) / semi_height,
                                                  -1.0f, 1.0f);
    const float area_cm2 = semi_width * semi_height *
        (asinf(normalized_height) + 1.57079632679489661923f +
         normalized_height * sqrtf(fmaxf(0.0f, 1.0f -
                                           normalized_height * normalized_height)));
    return fmaxf(0.0f, area_cm2 * WATER_TANK_LENGTH_CM / 1000.0f);
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
            clampf_local(s_rear_data.water_level, 0.0f, WATER_SENSOR_FULL_SCALE) : 0.0f);
    const float level = WATER_TANK_HEIGHT_CM * sensor_level / WATER_SENSOR_FULL_SCALE;
    const float tank_capacity_liters = water_liters(WATER_TANK_HEIGHT_CM);
    const float water_volume_liters = water_liters(level);
    const float water_percent = tank_capacity_liters > 0.0f ?
        100.0f * water_volume_liters / tank_capacity_liters : 0.0f;
    const float cg_height = CG_HEIGHT_EMPTY_M +
        (CG_HEIGHT_FULL_M - CG_HEIGHT_EMPTY_M) * water_percent / 100.0f;

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
        water_percent >= PENDULUM_MIN_WATER_PERCENT &&
        water_percent < PENDULUM_MAX_WATER_PERCENT;
    if (pendulum_active != s_water_pendulum_active) {
        ESP_LOGI(TAG, "Tank dynamics: %s at %.1f%% fill",
                 pendulum_active ? "PENDULUM" : "SOLID", water_percent);
    }
    float effective_cg_height = cg_height;
    float slosh_ay = 0.0f;
    float long_slosh_ax = 0.0f;
    float water_mass_kg = water_volume_liters;
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
        const pendulum_model_t model = {
            .mass_kg = water_mass_kg,
            .pivot_to_cg_m = PENDULUM_LENGTH_M,
            .gravity_m_s2 = GRAVITY_M_S2,
            .damping_coefficient = PENDULUM_DAMPING,
        };
        const float inertia_about_pivot = water_mass_kg *
            PENDULUM_LENGTH_M * PENDULUM_LENGTH_M;
        if (water_mass_kg > 0.0f) {
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
            const pendulum_model_t long_model = {
                .mass_kg = water_mass_kg,
                .pivot_to_cg_m = PENDULUM_LONG_LENGTH_M,
                .gravity_m_s2 = GRAVITY_M_S2,
                .damping_coefficient = PENDULUM_DAMPING,
            };
            const float long_inertia_about_pivot = water_mass_kg *
                PENDULUM_LONG_LENGTH_M * PENDULUM_LONG_LENGTH_M;
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

    /* Signed LTR model from the specification. Linear acceleration already
       excludes gravity, so do not subtract g from ay a second time. */
    const ltr_config_t ltr_config = {
        .cg_height_m = effective_cg_height,
        .track_width_m = VEHICLE_TRACK_M,
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
        int64_t slave_sample_time_us = 0;
        bool new_slave_sample = false;
        if (s_espnow_ready) {
            bool have_rx = field_espnow_receive(&received, pdMS_TO_TICKS(SLAVE_RX_WAIT_MS));
            while (have_rx) {
                if (received.data_len == sizeof(TankPayload)) {
                    TankPayload received_sample = {0};
                    memcpy(&received_sample, received.raw_data, sizeof(received_sample));
                    if (xSemaphoreTake(s_data_mutex, portMAX_DELAY) == pdTRUE) {
                        s_rear_data = received_sample;
                        s_last_rx_us = esp_timer_get_time();
                        calibrate_rear_mounting_offset(&received_sample);
                        slave_sample = received_sample;
                        slave_sample_time_us = s_last_rx_us;
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
            if (got_imu_sample) {
                s_front_imu = sample;
                if (new_imu_sample) calibrate_front_mounting_offset(&sample);
            }
            if (new_imu_sample && sample.has_linear_acceleration) {
                gps_imu_fusion_update_acceleration(sample.linear_accel_m_s2[0],
                                                   sample.linear_accel_m_s2[1],
                                                   sample.linear_accel_m_s2[2],
                                                   sample.sample_time_us);
            }
            const int64_t fusion_now_us = esp_timer_get_time();
            s_gps_imu_fusion = gps_imu_fusion_evaluate(
                gps.displayed_speed_kmh,
                have_gps && gps.speed_valid && gps.validation.usable_for_speed,
                gps.sample.rmc_received_us,
                fusion_now_us);
            const bool speed_valid = s_gps_imu_fusion.speed_usable;
            const float speed_kmh = speed_valid ? s_gps_imu_fusion.corrected_speed_kmh : 0.0f;
            calculate_telemetry(esp_timer_get_time(), speed_kmh, speed_valid);
            if (new_imu_sample) {
                sd_logger_logf(TAG, "[DATA] IMU1,time_us=%" PRId64 ",roll_deg=%.3f,pitch_deg=%.3f,yaw_deg=%.3f,accel_x=%.3f,accel_y=%.3f,accel_z=%.3f,gyro_x=%.4f,gyro_y=%.4f,gyro_z=%.4f,ltr=%.5f,valid=%u",
                         sample.sample_time_us, sample.roll_deg, sample.pitch_deg, sample.yaw_deg,
                         sample.linear_accel_m_s2[0], sample.linear_accel_m_s2[1], sample.linear_accel_m_s2[2],
                         sample.gyro_rad_s[0], sample.gyro_rad_s[1], sample.gyro_rad_s[2],
                         s_imu1_ltr.valid ? s_imu1_ltr.value : 0.0f,
                         (unsigned)s_imu1_ltr.valid);
            }
            if (new_slave_sample) {
                sd_logger_logf(TAG, "[DATA] IMU2,time_us=%" PRId64 ",roll_deg=%.3f,pitch_deg=%.3f,ay_m_s2=%.3f,water_level_cm=%.3f,status_flags=0x%02X,ltr=%.5f,valid=%u",
                         slave_sample_time_us, slave_sample.roll_1, slave_sample.pitch_1,
                         slave_sample.ay_1, slave_sample.water_level,
                         (unsigned)slave_sample.sensor_status_flags,
                         s_imu2_ltr.valid ? s_imu2_ltr.value : 0.0f,
                         (unsigned)s_imu2_ltr.valid);
            }
            if (new_imu_sample || new_slave_sample) {
                sd_logger_logf(TAG, "[DATA] IMU_A,time_us=%" PRId64 ",roll_deg=%.3f,ay_m_s2=%.3f,ltr=%.5f,sensors_used=%u,max_safe_speed_kmh=%.2f,min_turn_radius_m=%.2f,predicted_ltr=%.5f,speed_turn_risk=%u,dynamic_risk=%u,pitch_deg=%.3f,pitch_ltr=%.5f,predicted_pitch_ltr=%.5f,pitch_risk=%u",
                         esp_timer_get_time(), s_telemetry.roll_deg,
                         s_telemetry.lateral_accel_m_s2, s_telemetry.ltr,
                         (unsigned)(s_imu1_ltr.valid + s_imu2_ltr.valid),
                         s_telemetry.max_safe_speed_kmh, s_telemetry.min_turn_radius_m,
                         s_telemetry.predicted_ltr,
                         (unsigned)s_telemetry.speed_turn_risk,
                         (unsigned)s_telemetry.dynamic_risk,
                         s_telemetry.pitch_deg, s_telemetry.pitch_ltr,
                         s_telemetry.predicted_pitch_ltr,
                         (unsigned)s_telemetry.pitch_risk);
            }
            xSemaphoreGive(s_data_mutex);
        }
        if (!s_espnow_ready) vTaskDelay(pdMS_TO_TICKS(SLAVE_RX_WAIT_MS));
    }
}

static void gui_task(void *arg)
{
    (void)arg;
    while (true) {
        gps_module_snapshot_t gps = {0};
        display_telemetry_t telemetry = {0};
        gps_imu_fusion_result_t fusion = {0};
        const bool have_gps = gps_module_get_snapshot(&gps);
        if (xSemaphoreTake(s_data_mutex, portMAX_DELAY) == pdTRUE) {
            telemetry = s_telemetry;
            fusion = s_gps_imu_fusion;
            xSemaphoreGive(s_data_mutex);
        }
        telemetry.speed_kmh = fusion.speed_usable ? fusion.corrected_speed_kmh : 0.0f;
        telemetry.speed_valid = fusion.speed_usable;
        if (have_gps && gps.snapshot_us != s_last_gps_log_us) {
            const char *gps_reason = gps.validation.reason != NULL ? gps.validation.reason : "sin_datos";
            const char *fusion_state = fusion.state != NULL ? fusion.state : "GPS_SIN_DATOS";
            sd_logger_logf(TAG, "[DATA] GPS,time_us=%" PRId64 ",lat_deg=%.7f,lon_deg=%.7f,speed_kmh=%.3f,speed_mph=%.3f,fix=%d,satellites=%u,hdop=%.2f,ltr=%.5f,telemetry_roll_deg=%.3f,gps_status=%s,gps_reason=%s,rmc_age_ms=%" PRIu32 ",gga_age_ms=%" PRIu32 ",epoch_skew_ms=%" PRIu32 ",speed_corrected_kmh=%.3f,fusion_state=%s,imu_accel_norm_m_s2=%.4f,imu_stationary_ms=%" PRIu32 ",imu_fresh=%u,speed_usable=%u",
                           gps.snapshot_us, gps.sample.latitude_deg, gps.sample.longitude_deg,
                           gps.displayed_speed_kmh, gps.displayed_speed_kmh * 0.6213711922f,
                           gps.sample.fix_quality, (unsigned)gps.sample.satellites, gps.sample.hdop,
                           telemetry.ltr, telemetry.roll_deg,
                           gps_validation_state_name(gps.validation.state), gps_reason,
                           gps.validation.rmc_age_ms, gps.validation.gga_age_ms,
                           gps.validation.epoch_skew_ms, fusion.corrected_speed_kmh,
                           fusion_state, fusion.acceleration_norm_m_s2, fusion.stationary_ms,
                           (unsigned)fusion.imu_fresh, (unsigned)fusion.speed_usable);
            s_last_gps_log_us = gps.snapshot_us;
        }
        const int64_t now_us = esp_timer_get_time();
        if (now_us - s_last_event_us >= 1000000) {
            const char *gps_reason = gps.validation.reason != NULL ? gps.validation.reason : "sin_datos";
            const char *fusion_state = fusion.state != NULL ? fusion.state : "GPS_SIN_DATOS";
            sd_logger_logf(TAG, "[EVENT] time_us=%" PRId64 ",gps_status=%s,gps_quality_state=%s,gps_reason=%s,fusion_state=%s,imu_status=%s,espnow_status=%s,ltr=%.5f,static_risk=%u,max_safe_speed_kmh=%.2f,min_turn_radius_m=%.2f,predicted_ltr=%.5f,speed_turn_risk=%u,dynamic_risk=%u,pitch_deg=%.3f,pitch_ltr=%.5f,predicted_pitch_ltr=%.5f,pitch_risk=%u",
                           now_us, have_gps && gps.validation.usable_for_speed ? "UTILIZABLE" : "SIN_FIX_UTIL",
                           gps_validation_state_name(gps.validation.state), gps_reason, fusion_state,
                           telemetry.imu_local_ok ? "OK" : "NO_DISPONIBLE",
                           telemetry.rear_online ? "RX_ACTIVO" : "SIN_RX",
                           telemetry.ltr, (unsigned)telemetry.static_risk,
                           telemetry.max_safe_speed_kmh, telemetry.min_turn_radius_m,
                           telemetry.predicted_ltr,
                           (unsigned)telemetry.speed_turn_risk,
                           (unsigned)telemetry.dynamic_risk,
                           telemetry.pitch_deg, telemetry.pitch_ltr,
                           telemetry.predicted_pitch_ltr,
                           (unsigned)telemetry.pitch_risk);
            s_last_event_us = now_us;
        }
        if (s_display_ready) display_module_update(have_gps ? &gps : NULL, &telemetry);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
    gps_imu_fusion_reset();
    s_data_mutex = xSemaphoreCreateMutex();
    if (s_data_mutex == NULL) { ESP_LOGE(TAG, "No se pudo crear el mutex"); return; }
    ESP_ERROR_CHECK(board_shared_spi_init());
    gps_module_start();
    if (!sd_logger_start()) {
        ESP_LOGE(TAG, "No se pudo iniciar el servicio de registro microSD");
    }
    xTaskCreate(imu_init_task, "imu_init", 4096, NULL, 3, NULL);
    xTaskCreate(display_init_task, "display_init", 8192, NULL, 2, NULL);
    xTaskCreate(espnow_init_task, "espnow_init", 4096, NULL, 3, NULL);
    xTaskCreate(sensor_task, "rollover_sensor", 8192, NULL, 5, NULL);
    xTaskCreate(gui_task, "rollover_gui", 4096, NULL, 2, NULL);
}
