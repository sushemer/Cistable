#ifndef INTERFAZ_H
#define INTERFAZ_H

#include "lvgl.h"
#include "estado_sistema.h"

// Llamar una vez, con el bloqueo LVGL adquirido.
void interfaz_init(lv_display_t *display);

// Llamar despues de interfaz_init(), con el bloqueo LVGL adquirido.
void interfaz_mostrar_inicio(void);
void interfaz_mostrar_riesgo(riesgo_t riesgo);
void interfaz_mostrar_nivel(uint8_t porcentaje);
void interfaz_mostrar_sin_datos(void);

#endif
