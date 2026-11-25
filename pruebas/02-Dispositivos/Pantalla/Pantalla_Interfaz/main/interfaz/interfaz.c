/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#include "interfaz.h"
#include "recursos/camiones.h"

enum { INICIO, CALIBRACION, SELECCION_CAMION, RIESGO, NIVEL, SIN_DATOS, NUM_PANTALLAS };
static lv_obj_t *pantallas[NUM_PANTALLAS];
static lv_obj_t *indicador_riesgo;
static lv_obj_t *texto_riesgo;
static lv_obj_t *texto_porcentaje;
static lv_obj_t *barra_nivel;
static lv_obj_t *texto_cuenta;
static lv_obj_t *tarjetas_camion[2];
static lv_obj_t *imagenes_camion[2];
static lv_obj_t *nombres_camion[2];
static lv_obj_t *lineas_seleccion[2];
static lv_obj_t *aviso_confirmacion;
static lv_timer_t *timer_confirmacion;

#define COLOR_FONDO 0x000000
#define COLOR_PANEL 0x1B1F22
#define COLOR_TEXTO 0xF2F2F2
#define COLOR_SECUNDARIO 0x9EA4A8
#define COLOR_AGUA 0x2196F3
#define RADIO_PANEL 12

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
    static const uint32_t colores[] = {0x00A63E, 0xFFB000, 0xD50000};
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

void interfaz_mostrar_calibracion(uint8_t segundos)
{
    lv_label_set_text_fmt(texto_cuenta, "%u", (unsigned)segundos);
    mostrar_pantalla(CALIBRACION);
}

void interfaz_mostrar_seleccion(modelo_camion_t seleccionado)
{
    for (int i = 0; i < 2; i++) {
        bool activa = i == (int)seleccionado;
        lv_obj_set_style_bg_color(tarjetas_camion[i],
                                 lv_color_hex(activa ? COLOR_PANEL : COLOR_FONDO), 0);
        lv_obj_set_style_text_color(nombres_camion[i],
                                   lv_color_hex(activa ? COLOR_TEXTO : COLOR_SECUNDARIO), 0);
        lv_obj_set_style_image_opa(imagenes_camion[i], activa ? LV_OPA_COVER : LV_OPA_50, 0);
        lv_obj_set_style_bg_opa(lineas_seleccion[i], activa ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    }
    mostrar_pantalla(SELECCION_CAMION);
}

// Timer ejecutado por LVGL bajo el bloqueo existente; no detiene la recepcion.
static void ocultar_confirmacion(lv_timer_t *timer)
{
    lv_obj_add_flag(aviso_confirmacion, LV_OBJ_FLAG_HIDDEN);
    lv_timer_pause(timer);
}

void interfaz_confirmar_seleccion(modelo_camion_t seleccionado)
{
    lv_label_set_text(aviso_confirmacion, seleccionado == CAMION_1 ?
                      "MODELO A CONFIRMADO" : "MODELO B CONFIRMADO");
    lv_obj_remove_flag(aviso_confirmacion, LV_OBJ_FLAG_HIDDEN);
    lv_timer_reset(timer_confirmacion);
    lv_timer_resume(timer_confirmacion);
}

void interfaz_init(lv_display_t *disp)
{
    lv_obj_t *scr = lv_display_get_screen_active(disp);
    lv_obj_set_style_bg_color(scr, lv_color_hex(COLOR_FONDO), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(scr, lv_color_hex(COLOR_TEXTO), 0);
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

    crear_texto(pantallas[CALIBRACION], "PREPARANDO SISTEMA", 28, &lv_font_montserrat_28);
    texto_cuenta = crear_texto(pantallas[CALIBRACION], "15", 112, &lv_font_montserrat_48);
    lv_obj_t *calibrando = crear_texto(pantallas[CALIBRACION], "Calibrando sensores", 212,
                                      &lv_font_montserrat_24);
    lv_obj_set_style_text_color(calibrando, lv_color_hex(COLOR_SECUNDARIO), 0);
    lv_obj_t *detenido = crear_texto(pantallas[CALIBRACION], "Mantenga el vehiculo detenido", 252,
                                    &lv_font_montserrat_24);
    lv_obj_set_style_text_color(detenido, lv_color_hex(COLOR_SECUNDARIO), 0);

    crear_texto(pantallas[SELECCION_CAMION], "SELECCIONE EL CAMION", 28, &lv_font_montserrat_28);
    for (int i = 0; i < 2; i++) {
        tarjetas_camion[i] = lv_obj_create(pantallas[SELECCION_CAMION]);
        lv_obj_remove_style_all(tarjetas_camion[i]);
        lv_obj_set_pos(tarjetas_camion[i], 24 + i * 224, 80);
        lv_obj_set_size(tarjetas_camion[i], 208, 156);
        lv_obj_set_style_bg_opa(tarjetas_camion[i], LV_OPA_COVER, 0);
        lv_obj_set_style_radius(tarjetas_camion[i], RADIO_PANEL, 0);
        lv_obj_remove_flag(tarjetas_camion[i], LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        // Recursos constantes en flash; objetos creados una sola vez.
        lv_obj_t *imagen = lv_image_create(tarjetas_camion[i]);
        imagenes_camion[i] = imagen;
        lv_image_set_src(imagen, i == 0 ? &camion_pequeno : &camion_grande);
        lv_obj_align(imagen, LV_ALIGN_TOP_MID, 0, 8);
        lv_obj_t *nombre = lv_label_create(tarjetas_camion[i]);
        nombres_camion[i] = nombre;
        lv_label_set_text(nombre, i == 0 ? "MODELO A" : "MODELO B");
        lv_obj_set_style_text_font(nombre, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_align(nombre, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(nombre, LV_ALIGN_TOP_MID, 0, 108);
        lineas_seleccion[i] = lv_obj_create(tarjetas_camion[i]);
        lv_obj_remove_style_all(lineas_seleccion[i]);
        lv_obj_set_size(lineas_seleccion[i], 112, 3);
        lv_obj_align(lineas_seleccion[i], LV_ALIGN_BOTTOM_MID, 0, -5);
        lv_obj_set_style_bg_color(lineas_seleccion[i], lv_color_hex(COLOR_TEXTO), 0);
        lv_obj_remove_flag(lineas_seleccion[i], LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_t *instruccion_corta = crear_texto(pantallas[SELECCION_CAMION], "CORTA: CAMBIAR", 252,
                                             &lv_font_montserrat_24);
    lv_obj_set_style_text_color(instruccion_corta, lv_color_hex(COLOR_SECUNDARIO), 0);
    lv_obj_t *instruccion_larga = crear_texto(pantallas[SELECCION_CAMION], "MANTENER 1 s: CONFIRMAR", 284,
                                             &lv_font_montserrat_24);
    lv_obj_set_style_text_color(instruccion_larga, lv_color_hex(COLOR_SECUNDARIO), 0);

    crear_texto(pantallas[RIESGO], "RIESGO DE VOLCADURA", 28, &lv_font_montserrat_28);
    indicador_riesgo = lv_obj_create(pantallas[RIESGO]);
    lv_obj_remove_style_all(indicador_riesgo);
    lv_obj_set_pos(indicador_riesgo, 24, 96);
    lv_obj_set_size(indicador_riesgo, 432, 170);
    lv_obj_set_style_bg_opa(indicador_riesgo, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(indicador_riesgo, RADIO_PANEL, 0);
    lv_obj_remove_flag(indicador_riesgo, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    texto_riesgo = lv_label_create(indicador_riesgo);
    lv_obj_set_style_text_font(texto_riesgo, &lv_font_montserrat_48, 0);

    crear_texto(pantallas[NIVEL], "NIVEL DE AGUA", 28, &lv_font_montserrat_28);
    texto_porcentaje = crear_texto(pantallas[NIVEL], "78%", 92, &lv_font_montserrat_48);
    lv_obj_set_style_text_color(texto_porcentaje, lv_color_hex(COLOR_AGUA), 0);
    barra_nivel = lv_bar_create(pantallas[NIVEL]);
    lv_obj_remove_style_all(barra_nivel);
    lv_obj_set_pos(barra_nivel, 24, 204);
    lv_obj_set_size(barra_nivel, 432, 28);
    lv_obj_set_style_bg_color(barra_nivel, lv_color_hex(COLOR_PANEL), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(barra_nivel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(barra_nivel, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(barra_nivel, lv_color_hex(COLOR_AGUA), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(barra_nivel, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(barra_nivel, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_bar_set_range(barra_nivel, 0, 100);
    lv_bar_set_value(barra_nivel, 78, LV_ANIM_OFF);
    lv_obj_t *referencia_vacio = crear_texto(pantallas[NIVEL], "0 %", 244, &lv_font_montserrat_14);
    lv_obj_set_style_text_align(referencia_vacio, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_color(referencia_vacio, lv_color_hex(COLOR_SECUNDARIO), 0);
    lv_obj_t *referencia_lleno = crear_texto(pantallas[NIVEL], "100 %", 244, &lv_font_montserrat_14);
    lv_obj_set_style_text_align(referencia_lleno, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(referencia_lleno, lv_color_hex(COLOR_SECUNDARIO), 0);
    lv_obj_t *cisterna = crear_texto(pantallas[NIVEL], "CISTERNA", 280, &lv_font_montserrat_24);
    lv_obj_set_style_text_color(cisterna, lv_color_hex(COLOR_SECUNDARIO), 0);

    crear_texto(pantallas[SIN_DATOS], "SIN DATOS", 110, &lv_font_montserrat_48);
    lv_obj_t *aviso = crear_texto(pantallas[SIN_DATOS], "ESPERANDO DATOS VALIDOS",
                                190, &lv_font_montserrat_24);
    lv_obj_set_style_text_color(aviso, lv_color_hex(COLOR_SECUNDARIO), 0);

    // Aviso compartido sobre las pantallas, creado una sola vez.
    // Queda debajo del bloque de riesgo (termina en y=266).
    aviso_confirmacion = crear_texto(scr, "", 276, &lv_font_montserrat_24);
    lv_obj_set_height(aviso_confirmacion, 40);
    lv_obj_set_style_pad_top(aviso_confirmacion, 5, 0);
    lv_obj_set_style_bg_color(aviso_confirmacion, lv_color_hex(COLOR_PANEL), 0);
    lv_obj_set_style_bg_opa(aviso_confirmacion, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(aviso_confirmacion, RADIO_PANEL, 0);
    lv_obj_add_flag(aviso_confirmacion, LV_OBJ_FLAG_HIDDEN);
    timer_confirmacion = lv_timer_create(ocultar_confirmacion, 2500, NULL);
    LV_ASSERT_MALLOC(timer_confirmacion);
    lv_timer_pause(timer_confirmacion);


    
}
