#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gps_data.h"

// Resultado de analizar una sentencia NMEA individual.
typedef struct {
    char type[7];
    bool checksum_valid;
    bool parsed;
    bool has_time;
    uint32_t time_of_day_ms;
} gps_nmea_result_t;

// Verifica checksum y procesa RMC/GGA válidos, actualizando la muestra acumulada.
// Devuelve true cuando la sentencia reconocida se pudo analizar correctamente.
bool gps_nmea_process_line(gps_sample_t *sample,const char *line, int64_t received_us, gps_nmea_result_t *result);
