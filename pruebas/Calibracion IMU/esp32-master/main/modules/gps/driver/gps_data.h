#pragma once

#include <stdbool.h>
#include <stdint.h>

// Conserva los datos más recientes de RMC (posición/velocidad) y GGA (calidad del fix).
// Las horas de época son UTC en ms; received_us es el tiempo monotónico de recepción.
typedef struct {
    bool has_rmc;
    bool rmc_status_valid;
    bool rmc_data_valid;
    uint32_t rmc_time_of_day_ms;
    int64_t rmc_received_us;
    float latitude_deg;
    float longitude_deg;
    float speed_knots;
    float speed_kmh;
    float course_deg;

    bool has_gga;
    bool gga_data_valid;
    uint32_t gga_time_of_day_ms;
    int64_t gga_received_us;
    int fix_quality;
    uint8_t satellites;
    float hdop;
} gps_sample_t;

// Inicializa a cero la muestra acumulada antes de procesar NMEA.
void gps_sample_init(gps_sample_t *sample);

