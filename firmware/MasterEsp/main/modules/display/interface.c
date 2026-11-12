#include "interface.h"

#include <math.h>

static lv_obj_t *s_title, *s_speed, *s_status, *s_ltr, *s_roll, *s_water;

static lv_obj_t *make_label(lv_obj_t *parent, int x, int y, int width,const lv_font_t *font, const char *initial){
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, initial);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, width);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    return label;
}

void interface_init(lv_display_t *display){
    lv_obj_t *screen = lv_display_get_screen_active(display);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101820), 0);
    lv_obj_set_style_text_color(screen, lv_color_white(), 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    s_title = make_label(screen, 12, 8, 456, LV_FONT_DEFAULT, "CISTABLE | CABINA");
    s_speed = make_label(screen, 20, 52, 440, LV_FONT_DEFAULT, "Speed: --.- mph");
    s_status = make_label(screen, 20, 116, 440, LV_FONT_DEFAULT, "Esperando datos");
    s_ltr = make_label(screen, 20, 158, 440, LV_FONT_DEFAULT, "LTR: --");
    s_roll = make_label(screen, 20, 198, 440, LV_FONT_DEFAULT, "Roll: --.- deg");
    s_water = make_label(screen, 20, 238, 440, LV_FONT_DEFAULT, "Agua: -- gal");
}

void interface_update(const gps_module_snapshot_t *gps, const display_telemetry_t *telemetry){
    (void)gps;
    if (telemetry == NULL) return;
    const bool moving = telemetry->speed_kmh >= 3.0f;
    const float abs_ltr = fabsf(telemetry->ltr);
    const bool blink = (lv_tick_get() / 500U) % 2U != 0U;
    lv_color_t color = lv_color_hex(0xFFFFFF);
    const char *status = "DETENIDO";

    if (telemetry->static_risk && !moving) {
        status = blink ? "RIESGO ESTATICO" : "DETENGA Y ASEGURE";
        color = lv_color_hex(blink ? 0xFF2020 : 0xFFC107);
    } else if (moving && abs_ltr >= 0.60f) {
        status = "PELIGRO CRITICO";
        color = lv_color_hex(0xFF2020);
    } else if (moving && abs_ltr >= 0.35f) {
        status = "PRECAUCION";
        color = lv_color_hex(0xFFC107);
    } else if (moving) {
        status = "SEGURO";
        color = lv_color_hex(0x20D060);
    }

    lv_obj_set_style_text_color(s_title, color, 0);
    lv_obj_set_style_text_color(s_status, color, 0);
    lv_obj_set_style_text_color(s_ltr, color, 0);
    lv_obj_set_style_text_color(s_roll, color, 0);
    lv_label_set_text_fmt(s_speed, "Speed: %.1f mph", telemetry->speed_kmh * 0.6213711922f);
    lv_label_set_text_fmt(s_status, "%s | IMU %s/%s", status,
                          telemetry->imu_local_ok ? "OK" : "FALLA",
                          telemetry->imu_remote_ok ? "OK" : "FALLA");
    lv_label_set_text_fmt(s_ltr, "LTR: %+.3f", telemetry->ltr);
    lv_label_set_text_fmt(s_roll, "Roll: %+.1f deg", telemetry->roll_deg);
    if (telemetry->water_ok) {
        lv_label_set_text_fmt(s_water, "Agua: %.1f gal (%.0f%%)",
                              telemetry->water_gallons, telemetry->water_percent);
    } else {
        lv_label_set_text(s_water, "Agua: sin lectura");
    }
}
