#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gps_data.h"

// Estado de calidad y vigencia asignado a la información GPS.
typedef enum {
    GPS_VALIDATION_NO_DATA,
    GPS_VALIDATION_SIN_FIX,
    GPS_VALIDATION_DEGRADADO,
    GPS_VALIDATION_VALIDO,
    GPS_VALIDATION_OBSOLETO,
} gps_validation_state_t;

// Criterios configurables usados para aceptar un fix en el análisis.
typedef struct {
    uint8_t min_satellites;
    float max_hdop;
    uint32_t max_age_ms;
    uint32_t max_epoch_skew_ms;
} gps_validation_config_t;

// Estado calculado, antigüedad de datos y motivo de aceptación o rechazo.
typedef struct {
    gps_validation_state_t state;
    bool usable_for_speed;
    uint32_t rmc_age_ms;
    uint32_t gga_age_ms;
    uint32_t epoch_skew_ms;
    const char *reason;
} gps_validation_result_t;

// Valida presencia, vigencia, coherencia temporal y calidad de RMC/GGA.
// usable_for_speed es una clasificación experimental, no una señal de seguridad.
gps_validation_result_t gps_validation_evaluate(const gps_sample_t *sample,  const gps_validation_config_t *config, int64_t now_us);

// Convierte el estado de validación a una etiqueta para los registros.
const char *gps_validation_state_name(gps_validation_state_t state);
