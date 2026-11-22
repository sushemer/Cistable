#include "protocolo_estado.h"

bool paquete_decodificar(const uint8_t *datos, size_t longitud,
                         paquete_espnow_t *paquete)
{
    if (datos == NULL || paquete == NULL || longitud != PROTOCOLO_ESTADO_LONGITUD) {
        return false;
    }

    paquete_espnow_t resultado = {
        .version = datos[0],
        .modo = datos[1],
        .riesgo = datos[2],
        .nivel_agua_pct = datos[3],
        .secuencia = (uint16_t)datos[4] | ((uint16_t)datos[5] << 8),
    };
    *paquete = resultado;
    return true;
}

bool paquete_a_estado(const paquete_espnow_t *paquete, estado_sistema_t *estado)
{
    if (paquete == NULL || estado == NULL || paquete->version != PROTOCOLO_ESTADO_VERSION) {
        return false;
    }

    // Valores deterministas para los campos sin significado en el modo actual.
    estado_sistema_t resultado = {
        .modo = MODO_SIN_DATOS,
        .riesgo = RIESGO_SEGURO,
        .nivel_agua_pct = 0,
    };

    switch (paquete->modo) {
    case PROTOCOLO_MODO_EN_MOVIMIENTO:
        if (paquete->nivel_agua_pct > 100) return false;
        resultado.modo = MODO_EN_MOVIMIENTO;
        resultado.nivel_agua_pct = paquete->nivel_agua_pct;
        switch (paquete->riesgo) {
        case PROTOCOLO_RIESGO_SEGURO:
            resultado.riesgo = RIESGO_SEGURO;
            break;
        case PROTOCOLO_RIESGO_PRECAUCION:
            resultado.riesgo = RIESGO_PRECAUCION;
            break;
        case PROTOCOLO_RIESGO_PELIGRO:
            resultado.riesgo = RIESGO_PELIGRO;
            break;
        default:
            return false;
        }
        break;
    case PROTOCOLO_MODO_DETENIDO:
        if (paquete->nivel_agua_pct > 100) return false;
        resultado.modo = MODO_DETENIDO;
        resultado.nivel_agua_pct = paquete->nivel_agua_pct;
        break;
    case PROTOCOLO_MODO_SIN_DATOS:
        resultado.modo = MODO_SIN_DATOS;
        break;
    default:
        return false;
    }

    *estado = resultado;
    return true;
}
