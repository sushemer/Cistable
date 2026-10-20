#include <stdbool.h>
#include <stdint.h>

#include "gps_data.h"
#include "gps_validation.h"

typedef struct {
    gps_sample_t sample;
    gps_validation_result_t validation;
    float displayed_speed_kmh;
    bool speed_valid;
    int64_t snapshot_us;
} gps_module_snapshot_t;

// Inicia UART/NMEA en tarea propia y publica la ultima muestra GPS.
void gps_module_start(void);
// Copia coherentemente la ultima muestra; false hasta que llegue NMEA.
bool gps_module_get_snapshot(gps_module_snapshot_t *snapshot);
#pragma once

