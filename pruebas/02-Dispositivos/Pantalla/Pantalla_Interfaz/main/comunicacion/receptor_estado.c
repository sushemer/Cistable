#include "receptor_estado.h"
#include "esp_timer.h"

#define RECEPTOR_TIMEOUT_US 1500000LL

static bool hay_recepcion_valida;
static uint16_t ultima_secuencia;
static int64_t ultima_recepcion_valida_us;
static estado_sistema_t estado_actual = {MODO_SIN_DATOS, RIESGO_SEGURO, 0};

void receptor_estado_init(void)
{
    hay_recepcion_valida = false;
    ultima_secuencia = 0;
    ultima_recepcion_valida_us = 0;
    estado_actual = (estado_sistema_t){MODO_SIN_DATOS, RIESGO_SEGURO, 0};
}

static void actualizar_timeout(int64_t ahora_us)
{
    if (hay_recepcion_valida &&
        ahora_us - ultima_recepcion_valida_us >= RECEPTOR_TIMEOUT_US) {
        estado_actual = (estado_sistema_t){MODO_SIN_DATOS, RIESGO_SEGURO, 0};
        hay_recepcion_valida = false;
    }
}

void receptor_estado_actualizar_timeout(void)
{
    actualizar_timeout(esp_timer_get_time());
}

bool receptor_estado_procesar(const paquete_espnow_t *paquete)
{
    const int64_t ahora_us = esp_timer_get_time();
    // Expirar antes de comparar permite recuperarse sin una llamada periodica previa.
    actualizar_timeout(ahora_us);
    estado_sistema_t nuevo_estado;
    if (!paquete_a_estado(paquete, &nuevo_estado)) return false;

    if (hay_recepcion_valida) {
        uint16_t delta = (uint16_t)(paquete->secuencia - ultima_secuencia);
        if (delta == 0 || delta >= 0x8000) return false;
    }

    ultima_secuencia = paquete->secuencia;
    ultima_recepcion_valida_us = ahora_us;
    estado_actual = nuevo_estado;
    hay_recepcion_valida = true;
    return true;
}

bool receptor_estado_obtener(estado_sistema_t *estado)
{
    if (estado == NULL) return false;
    receptor_estado_actualizar_timeout();
    *estado = estado_actual;
    return true;
}
