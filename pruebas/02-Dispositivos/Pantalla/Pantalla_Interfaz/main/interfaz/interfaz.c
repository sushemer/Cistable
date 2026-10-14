/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#include "interfaz.h"

enum { INICIO, RIESGO, NIVEL, SIN_DATOS, NUM_PANTALLAS };
static lv_obj_t *pantallas[NUM_PANTALLAS];
static lv_obj_t *indicador_riesgo;
static lv_obj_t *texto_riesgo;
static lv_obj_t *texto_porcentaje;
static lv_obj_t *barra_nivel;

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

static void mostrar_pantalla(int pantalla)
{
    for (int i = 0; i < NUM_PANTALLAS; i++) {
        lv_obj_add_flag(pantallas[i], LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_remove_flag(pantallas[pantalla], LV_OBJ_FLAG_HIDDEN);
}

void interfaz_mostrar_inicio(void)
{
    mostrar_pantalla(INICIO);
}

void interfaz_mostrar_riesgo(riesgo_t riesgo)
{
    static const uint32_t colores[] = {0x00FF00, 0xFFFF00, 0xFF0000};
    static const char * const estados[] = {"SEGURO", "PRECAUCION", "PELIGRO"};
    if (riesgo != RIESGO_SEGURO && riesgo != RIESGO_PRECAUCION && riesgo != RIESGO_PELIGRO) {
        interfaz_mostrar_sin_datos();
        return;
    }
    lv_obj_set_style_bg_color(indicador_riesgo, lv_color_hex(colores[riesgo]), 0);
    lv_obj_set_style_text_color(texto_riesgo,
                               riesgo == RIESGO_PELIGRO ? lv_color_white() : lv_color_black(), 0);
    lv_label_set_text(texto_riesgo, estados[riesgo]);
    lv_obj_center(texto_riesgo);
    mostrar_pantalla(RIESGO);
}

void interfaz_mostrar_nivel(uint8_t porcentaje)
{
    if (porcentaje > 100) porcentaje = 100;
    lv_label_set_text_fmt(texto_porcentaje, "%u%%", (unsigned)porcentaje);
    lv_bar_set_value(barra_nivel, porcentaje, LV_ANIM_OFF);
    mostrar_pantalla(NIVEL);
}

void interfaz_mostrar_sin_datos(void)
{
    mostrar_pantalla(SIN_DATOS);
}

void interfaz_init(lv_display_t *disp)
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
    texto_porcentaje = crear_texto(pantallas[NIVEL], "78%", 96, &lv_font_montserrat_48);
    lv_obj_set_style_text_color(texto_porcentaje, lv_color_hex(0x009FFF), 0);
    barra_nivel = lv_bar_create(pantallas[NIVEL]);
    lv_obj_remove_style_all(barra_nivel);
    lv_obj_set_pos(barra_nivel, 24, 196);
    lv_obj_set_size(barra_nivel, 432, 52);
    lv_obj_set_style_bg_color(barra_nivel, lv_color_hex(0x252525), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(barra_nivel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(barra_nivel, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(barra_nivel, lv_color_hex(0x009FFF), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(barra_nivel, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(barra_nivel, 8, LV_PART_INDICATOR);
    lv_bar_set_range(barra_nivel, 0, 100);
    lv_bar_set_value(barra_nivel, 78, LV_ANIM_OFF);
    crear_texto(pantallas[NIVEL], "CISTERNA", 268, &lv_font_montserrat_24);

    crear_texto(pantallas[SIN_DATOS], "SIN DATOS", 110, &lv_font_montserrat_48);
    lv_obj_t *aviso = crear_texto(pantallas[SIN_DATOS], "ESPERANDO DATOS VALIDOS",
                                190, &lv_font_montserrat_24);
    lv_obj_set_style_text_color(aviso, lv_color_hex(0xCCCCCC), 0);

}
