#include "interface.h"

#include <math.h>

#include "driver/gpio.h"
#include "recursos/camiones.h"

#define TRUCK_SELECT_BUTTON_GPIO GPIO_NUM_1

static lv_obj_t *s_risk_panel;
static lv_obj_t *s_risk_text;
static lv_obj_t *s_truck_small;
static lv_obj_t *s_truck_large;
static unsigned s_selected_truck;
static bool s_button_was_pressed;

static const lv_color_t COLOR_SAFE = LV_COLOR_MAKE(0x00, 0xA6, 0x3E);
static const lv_color_t COLOR_CAUTION = LV_COLOR_MAKE(0xFF, 0xB0, 0x00);
static const lv_color_t COLOR_DANGER = LV_COLOR_MAKE(0xD5, 0x00, 0x00);
static const lv_color_t COLOR_NO_DATA = LV_COLOR_MAKE(0x45, 0x4B, 0x50);

static void apply_truck_selection(void)
{
    const bool small_selected = s_selected_truck == 0U;
    lv_obj_set_style_image_opa(s_truck_small, small_selected ? LV_OPA_COVER : LV_OPA_30, 0);
    lv_obj_set_style_image_opa(s_truck_large, small_selected ? LV_OPA_30 : LV_OPA_COVER, 0);
}

static void select_next_truck(void)
{
    s_selected_truck = (s_selected_truck + 1U) % 2U;
    apply_truck_selection();
}

void interface_poll_truck_button(void)
{
    const bool pressed = gpio_get_level(TRUCK_SELECT_BUTTON_GPIO) == 0;
    if (pressed && !s_button_was_pressed) {
        select_next_truck();
    }
    s_button_was_pressed = pressed;
}

void interface_init(lv_display_t *display)
{
    lv_obj_t *screen = lv_display_get_screen_active(display);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    gpio_config_t button_config = {
        .pin_bit_mask = 1ULL << TRUCK_SELECT_BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&button_config));

    s_risk_panel = lv_obj_create(screen);
    lv_obj_remove_style_all(s_risk_panel);
    lv_obj_set_pos(s_risk_panel, 24, 24);
    lv_obj_set_size(s_risk_panel, 432, 190);
    lv_obj_set_style_bg_opa(s_risk_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_risk_panel, 12, 0);
    lv_obj_remove_flag(s_risk_panel, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    s_risk_text = lv_label_create(s_risk_panel);
    lv_obj_set_style_text_font(s_risk_text, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_align(s_risk_text, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_risk_text);

    s_truck_small = lv_image_create(screen);
    lv_image_set_src(s_truck_small, &camion_pequeno);
    lv_obj_align(s_truck_small, LV_ALIGN_BOTTOM_LEFT, 60, -18);

    s_truck_large = lv_image_create(screen);
    lv_image_set_src(s_truck_large, &camion_grande);
    lv_obj_align(s_truck_large, LV_ALIGN_BOTTOM_RIGHT, -60, -18);

    lv_label_set_text(s_risk_text, "SIN DATOS");
    lv_obj_set_style_bg_color(s_risk_panel, COLOR_NO_DATA, 0);
    lv_obj_set_style_text_color(s_risk_text, lv_color_white(), 0);
    apply_truck_selection();
}

void interface_update(const gps_module_snapshot_t *gps,
                      const display_telemetry_t *telemetry)
{
    (void)gps;
    if (telemetry == NULL) return;

    const bool has_sensor_data = telemetry->imu_local_ok || telemetry->imu_remote_ok;
    const float abs_ltr = fabsf(telemetry->ltr);
    const float abs_roll = fabsf(telemetry->roll_deg);
    const bool moving = telemetry->speed_kmh >= 3.0f;

    lv_color_t panel_color = COLOR_NO_DATA;
    lv_color_t text_color = lv_color_white();
    const char *status = "SIN DATOS";
    if (has_sensor_data) {
        if ((moving && abs_ltr >= 0.60f) || abs_roll >= 15.0f) {
            status = "PELIGRO";
            panel_color = COLOR_DANGER;
        } else if ((moving && abs_ltr >= 0.35f) || abs_roll >= 8.0f || telemetry->static_risk) {
            status = "PRECAUCION";
            panel_color = COLOR_CAUTION;
            text_color = lv_color_black();
        } else {
            status = "SEGURO";
            panel_color = COLOR_SAFE;
        }
    }

    lv_obj_set_style_bg_color(s_risk_panel, panel_color, 0);
    lv_obj_set_style_text_color(s_risk_text, text_color, 0);
    lv_label_set_text(s_risk_text, status);
    lv_obj_center(s_risk_text);
    apply_truck_selection();
}
