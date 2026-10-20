#include "interface.h"

#include <math.h>

static lv_obj_t *s_speed, *s_gps, *s_front, *s_rear, *s_imu1, *s_imu2, *s_water;

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
    make_label(screen, 12, 8, 456, &lv_font_montserrat_24, "CISTABLE | TELEMETRIA");
    s_speed = make_label(screen, 20, 52, 440, &lv_font_montserrat_48, "--.- km/h");
    s_gps = make_label(screen, 20, 116, 440, &lv_font_montserrat_20, "GPS: esperando datos");
    s_front = make_label(screen, 20, 151, 440, &lv_font_montserrat_16, "IMU frontal: sin lectura");
    s_rear = make_label(screen, 20, 180, 440, &lv_font_montserrat_16, "Nodo trasero: desconectado");
    s_imu1 = make_label(screen, 20, 208, 440, &lv_font_montserrat_16, "IMU trasera 1: sin lectura");
    s_imu2 = make_label(screen, 20, 236, 440, &lv_font_montserrat_16, "IMU trasera 2: sin lectura");
    s_water = make_label(screen, 20, 264, 440, &lv_font_montserrat_16, "Agua (crudo): sin lectura");
}

void interface_update(const gps_module_snapshot_t *gps, const imu_bno085_sample_t *front_imu, const TankPayload *rear, bool rear_online){
    if (gps != NULL && gps->speed_valid && gps->validation.usable_for_speed) {
        lv_label_set_text_fmt(s_speed, "%.1f km/h", gps->displayed_speed_kmh);
        lv_label_set_text_fmt(s_gps, "GPS: %s | sat=%u | HDOP=%.1f",gps_validation_state_name(gps->validation.state), gps->sample.satellites, gps->sample.hdop);
    } 
    else {
        lv_label_set_text(s_speed, "--.- km/h");
        lv_label_set_text_fmt(s_gps, "GPS: %s", gps == NULL ? "sin datos" : gps_validation_state_name(gps->validation.state));
    }
    if (front_imu != NULL && front_imu->online && front_imu->has_orientation)
        lv_label_set_text_fmt(s_front, "IMU frontal roll=%.1f pitch=%.1f deg", front_imu->roll_deg, front_imu->pitch_deg);
    else lv_label_set_text(s_front, "IMU frontal: sin lectura");
    lv_label_set_text(s_rear, rear_online ? "Nodo trasero: conectado" : "Nodo trasero: sin enlace");
    if (rear_online && rear != NULL && (rear->sensor_status_flags & STATUS_IMU1_OK)){
        lv_label_set_text_fmt(s_imu1, "IMU trasera 1 roll=%.1f pitch=%.1f deg", rear->roll_1, rear->pitch_1);
    }
    else lv_label_set_text(s_imu1, "IMU trasera 1: sin lectura");

    if (rear_online && rear != NULL && (rear->sensor_status_flags & STATUS_IMU2_OK)){
        lv_label_set_text_fmt(s_imu2, "IMU trasera 2 roll=%.1f pitch=%.1f deg", rear->roll_2, rear->pitch_2);
    }
    else lv_label_set_text(s_imu2, "IMU trasera 2: sin lectura");

    if (rear_online && rear != NULL && (rear->sensor_status_flags & STATUS_RS485_OK)){
        lv_label_set_text_fmt(s_water, "Agua: %.2f", rear->water_level);
    }
    else lv_label_set_text(s_water, "Agua (crudo): sin lectura");
}
