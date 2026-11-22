/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#include "lvgl.h"

enum { INICIO, RIESGO, NIVEL, SIN_DATOS, NUM_PANTALLAS };
static lv_obj_t *pantallas[NUM_PANTALLAS];
static lv_obj_t *indicador_riesgo;
static lv_obj_t *texto_riesgo;
static unsigned paso;
static const uint32_t duraciones_ms[] = {2000, 3000, 3000, 3000, 4000, 3000};

static lv_obj_t *crear_texto(lv_obj_t *padre, const char *texto,
                           int y, const lv_font_t *fuente)
{
    lv_obj_t *label = lv_label_create(padre);
    lv_label_set_text(label, texto);
    lv_obj_set_width(label, 432);
    lv_obj_set_pos(label, 24, y);
    lv_obj_set_style_text_font(label, fuente, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    return label;
}

static void mostrar_paso(void)
{
    for (int i = 0; i < NUM_PANTALLAS; i++) {
        lv_obj_add_flag(pantallas[i], LV_OBJ_FLAG_HIDDEN);
    }
    int pantalla = INICIO;
    if (paso >= 1 && paso <= 3) {
        static const uint32_t colores[] = {0x00FF00, 0xFFFF00, 0xFF0000};
        static const char *estados[] = {"SEGURO", "PRECAUCION", "PELIGRO"};
        pantalla = RIESGO;
        lv_obj_set_style_bg_color(indicador_riesgo, lv_color_hex(colores[paso - 1]), 0);
        lv_obj_set_style_text_color(texto_riesgo,
                                   paso == 3 ? lv_color_white() : lv_color_black(), 0);
        lv_label_set_text(texto_riesgo, estados[paso - 1]);
        lv_obj_center(texto_riesgo);
    } else if (paso == 4) {
        pantalla = NIVEL;
    } else if (paso == 5) {
        pantalla = SIN_DATOS;
    }
    lv_obj_remove_flag(pantallas[pantalla], LV_OBJ_FLAG_HIDDEN);
}

// Ejecutado por lv_timer_handler(), bajo el bloqueo LVGL existente.
static void simulacion_cb(lv_timer_t *timer)
{
    paso = (paso + 1) % 6;
    mostrar_paso();
    lv_timer_set_period(timer, duraciones_ms[paso]);
}

void example_lvgl_demo_ui(lv_display_t *disp)
{
    lv_obj_t *scr = lv_display_get_screen_active(disp);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(scr, lv_color_white(), 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < NUM_PANTALLAS; i++) {
        pantallas[i] = lv_obj_create(scr);
        lv_obj_remove_style_all(pantallas[i]);
        lv_obj_set_pos(pantallas[i], 0, 0);
        lv_obj_set_size(pantallas[i], 480, 320);
        lv_obj_remove_flag(pantallas[i], LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(pantallas[i], LV_OBJ_FLAG_HIDDEN);
    }
    crear_texto(pantallas[INICIO], "BOMBEROS TIJUANA", 70, &lv_font_montserrat_28);
    crear_texto(pantallas[INICIO], "ASISTENCIA DE CISTERNA", 120, &lv_font_montserrat_24);
    crear_texto(pantallas[INICIO], "INICIANDO...", 210, &lv_font_montserrat_28);

    crear_texto(pantallas[RIESGO], "RIESGO DE VOLCADURA", 28, &lv_font_montserrat_24);
    indicador_riesgo = lv_obj_create(pantallas[RIESGO]);
    lv_obj_remove_style_all(indicador_riesgo);
    lv_obj_set_pos(indicador_riesgo, 24, 88);
    lv_obj_set_size(indicador_riesgo, 432, 170);
    lv_obj_set_style_bg_opa(indicador_riesgo, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(indicador_riesgo, 16, 0);
    lv_obj_remove_flag(indicador_riesgo, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    texto_riesgo = lv_label_create(indicador_riesgo);
    lv_obj_set_style_text_font(texto_riesgo, &lv_font_montserrat_48, 0);

    crear_texto(pantallas[NIVEL], "NIVEL DE AGUA", 28, &lv_font_montserrat_28);
    lv_obj_t *porcentaje = crear_texto(pantallas[NIVEL], "78%", 96, &lv_font_montserrat_48);
    lv_obj_set_style_text_color(porcentaje, lv_color_hex(0x009FFF), 0);
    lv_obj_t *barra = lv_bar_create(pantallas[NIVEL]);
    lv_obj_remove_style_all(barra);
    lv_obj_set_pos(barra, 24, 196);
    lv_obj_set_size(barra, 432, 52);
    lv_obj_set_style_bg_color(barra, lv_color_hex(0x252525), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(barra, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(barra, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(barra, lv_color_hex(0x009FFF), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(barra, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(barra, 8, LV_PART_INDICATOR);
    lv_bar_set_range(barra, 0, 100);
    lv_bar_set_value(barra, 78, LV_ANIM_OFF);
    crear_texto(pantallas[NIVEL], "CISTERNA", 268, &lv_font_montserrat_24);

    crear_texto(pantallas[SIN_DATOS], "SIN DATOS", 110, &lv_font_montserrat_48);
    lv_obj_t *aviso = crear_texto(pantallas[SIN_DATOS], "ESPERANDO DATOS VALIDOS",
                                190, &lv_font_montserrat_24);
    lv_obj_set_style_text_color(aviso, lv_color_hex(0xCCCCCC), 0);

    paso = 0;
    mostrar_paso();
    lv_timer_t *timer = lv_timer_create(simulacion_cb, duraciones_ms[paso], NULL);
    LV_ASSERT_MALLOC(timer);
}
