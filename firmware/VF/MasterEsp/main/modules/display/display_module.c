#include "display_module.h"

#include "display_port.h"
#include "interface.h"
#include "esp_log.h"

static const char *TAG = "DISPLAY";
static bool s_previous_speed_turn_risk;
static bool s_previous_dynamic_risk;
static bool s_previous_pitch_risk;

void display_module_start(void){
    ESP_LOGI(TAG, "Initializing ST7796 LCD and LVGL");
    lv_display_t *display = display_init();
    if (display == NULL) {
        ESP_LOGE(TAG, "LCD initialization returned NULL");
        return;
    }
    display_lock();
    interface_init(display);
    display_unlock();
    ESP_LOGI(TAG, "ST7796 LCD and LVGL ready");
}

void display_module_update(const gps_module_snapshot_t *gps,
                           const display_telemetry_t *telemetry){
    if (telemetry != NULL) {
        if (telemetry->speed_turn_risk && !s_previous_speed_turn_risk) {
            ESP_LOGW(TAG, "Advertencia de velocidad para curva: velocidad segura %.1f km/h",
                     telemetry->max_safe_speed_kmh);
        } else if (!telemetry->speed_turn_risk && s_previous_speed_turn_risk) {
            ESP_LOGI(TAG, "Advertencia de velocidad para curva desactivada");
        }
        if (telemetry->dynamic_risk && !s_previous_dynamic_risk) {
            ESP_LOGW(TAG, "Advertencia de vuelco previsto: LTR en 0.5 s %.3f",
                     telemetry->predicted_ltr);
        } else if (!telemetry->dynamic_risk && s_previous_dynamic_risk) {
            ESP_LOGI(TAG, "Advertencia de vuelco previsto desactivada");
        }
        if (telemetry->pitch_risk && !s_previous_pitch_risk) {
            ESP_LOGW(TAG, "Advertencia de pendiente longitudinal: inclinacion=%.1f grados LTR=%.3f previsto=%.3f",
                     telemetry->pitch_deg, telemetry->pitch_ltr,
                     telemetry->predicted_pitch_ltr);
        } else if (!telemetry->pitch_risk && s_previous_pitch_risk) {
            ESP_LOGI(TAG, "Advertencia de pendiente longitudinal desactivada");
        }
        s_previous_speed_turn_risk = telemetry->speed_turn_risk;
        s_previous_dynamic_risk = telemetry->dynamic_risk;
        s_previous_pitch_risk = telemetry->pitch_risk;
    }
    display_lock();
    interface_poll_truck_button();
    interface_update(gps, telemetry);
    display_unlock();
}
