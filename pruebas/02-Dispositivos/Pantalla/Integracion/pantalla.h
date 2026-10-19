#ifndef PANTALLA_H
#define PANTALLA_H

#include "esp_err.h"
#include "estado_sistema.h"

// Inicializa display y UI y muestra INICIO. Llamar una vez desde app_main,
// antes de crear tareas consumidoras. No inicializa radio ni simulacion.
esp_err_t pantalla_init(void);

// Muestra INICIO nuevamente; requiere pantalla_init().
esp_err_t pantalla_mostrar_inicio(void);

// Copia los valores a la UI sin conservar el puntero. Gestiona el bloqueo LVGL.
// Movimiento: riesgo; detenido: nivel; sin datos: aviso. NULL o campos invalidos
// muestran SIN DATOS y devuelven ESP_ERR_INVALID_ARG. Antes de init devuelve
// ESP_ERR_INVALID_STATE. Usar desde tareas, nunca desde ISR/callback WiFi
// ni con el bloqueo LVGL adquirido (puede esperar por el mutex).
esp_err_t pantalla_mostrar_estado(const estado_sistema_t *estado);

#endif
