#ifndef PROTOCOLO_ESTADO_H
#define PROTOCOLO_ESTADO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "estado_sistema.h"

#define PROTOCOLO_ESTADO_VERSION   1
#define PROTOCOLO_ESTADO_LONGITUD  6

#define PROTOCOLO_MODO_SIN_DATOS      0
#define PROTOCOLO_MODO_EN_MOVIMIENTO  1
#define PROTOCOLO_MODO_DETENIDO       2

#define PROTOCOLO_RIESGO_SEGURO       0
#define PROTOCOLO_RIESGO_PRECAUCION   1
#define PROTOCOLO_RIESGO_PELIGRO      2

// Representacion interna decodificada; no transmitir esta struct directamente.
typedef struct {
    uint8_t version;
    uint8_t modo;
    uint8_t riesgo;
    uint8_t nivel_agua_pct;
    uint16_t secuencia;
} paquete_espnow_t;

// Formato: version, modo, riesgo, nivel, secuencia LSB, secuencia MSB.
// Solo valida punteros y longitud. La validacion semantica es paquete_a_estado().
// En caso de error, la salida permanece intacta.
bool paquete_decodificar(const uint8_t *datos, size_t longitud,
                         paquete_espnow_t *paquete);

// Valida version y campos relevantes al modo antes de escribir la salida.
// Detenido ignora riesgo; sin datos ignora riesgo y nivel.
// En caso de error, la salida permanece intacta.
bool paquete_a_estado(const paquete_espnow_t *paquete, estado_sistema_t *estado);

#endif
