#include "pantalla.h"
#include <stdbool.h>
#include "display.h"
#include "interfaz.h"

static bool inicializada;

static bool estado_valido(const estado_sistema_t *estado)
{
    if (estado == NULL) return false;
    switch (estado->modo) {
    case MODO_EN_MOVIMIENTO:
        return estado->riesgo == RIESGO_SEGURO ||
               estado->riesgo == RIESGO_PRECAUCION ||
               estado->riesgo == RIESGO_PELIGRO;
    case MODO_DETENIDO:
        return estado->nivel_agua_pct <= 100;
    case MODO_SIN_DATOS:
        return true;
    default:
        return false;
    }
}

esp_err_t pantalla_init(void)
{
    if (inicializada) return ESP_ERR_INVALID_STATE;
    lv_display_t *display = display_init();
    display_lock();
    interfaz_init(display);
    interfaz_mostrar_inicio();
    inicializada = true;
    display_unlock();
    return ESP_OK;
}

esp_err_t pantalla_mostrar_inicio(void)
{
    if (!inicializada) return ESP_ERR_INVALID_STATE;
    display_lock();
    interfaz_mostrar_inicio();
    display_unlock();
    return ESP_OK;
}

esp_err_t pantalla_mostrar_estado(const estado_sistema_t *estado)
{
    if (!inicializada) return ESP_ERR_INVALID_STATE;
    bool valido = estado_valido(estado);
    display_lock();
    if (!valido) {
        interfaz_mostrar_sin_datos();
    } else {
        switch (estado->modo) {
        case MODO_EN_MOVIMIENTO:
            interfaz_mostrar_riesgo(estado->riesgo);
            break;
        case MODO_DETENIDO:
            interfaz_mostrar_nivel(estado->nivel_agua_pct);
            break;
        case MODO_SIN_DATOS:
            interfaz_mostrar_sin_datos();
            break;
        }
    }
    display_unlock();
    return valido ? ESP_OK : ESP_ERR_INVALID_ARG;
}
