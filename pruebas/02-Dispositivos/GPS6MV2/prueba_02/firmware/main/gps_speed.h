#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gps_data.h"

// Límites provisionales usados para clasificar cambios de velocidad en la prueba.
typedef struct {
    float max_acceleration_kmh_s;
    float max_speed_kmh;
} gps_speed_config_t;

// Resultado de comparar dos velocidades consecutivas.
typedef struct {
    bool valid;
    bool jump_detected;
    float delta_kmh;
    float acceleration_kmh_s;
    const char *reason;
} gps_speed_check_t;

// Velocidad calculada entre posiciones; sirve solo como comprobación secundaria.
typedef struct {
    bool valid;
    float speed_kmh;
} gps_coordinate_speed_t;

// Evalúa rango y aceleración entre muestras; no es un límite de seguridad.
gps_speed_check_t gps_speed_check(const gps_sample_t *previous,const gps_sample_t *current,uint32_t elapsed_ms,const gps_speed_config_t *config);

// Calcula la velocidad media entre dos posiciones válidas usando su intervalo.
gps_coordinate_speed_t gps_speed_from_coordinates(const gps_sample_t *previous,  const gps_sample_t *current, uint32_t elapsed_ms);
