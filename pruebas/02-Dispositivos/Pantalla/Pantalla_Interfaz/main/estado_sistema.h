#ifndef ESTADO_SISTEMA_H
#define ESTADO_SISTEMA_H

#include <stdint.h>

typedef enum {
    MODO_EN_MOVIMIENTO,
    MODO_DETENIDO,
    MODO_SIN_DATOS
} modo_sistema_t;

typedef enum {
    RIESGO_SEGURO,
    RIESGO_PRECAUCION,
    RIESGO_PELIGRO
} riesgo_t;

typedef struct {
    modo_sistema_t modo;
    riesgo_t riesgo;
    uint8_t nivel_agua_pct;
} estado_sistema_t;

#endif
