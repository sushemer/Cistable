#include "display_module.h"

#include "display_port.h"
#include "interface.h"
#include "esp_log.h"

static const char *TAG = "DISPLAY";

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
    display_lock();
    interface_poll_truck_button();
    interface_update(gps, telemetry);
    display_unlock();
}
