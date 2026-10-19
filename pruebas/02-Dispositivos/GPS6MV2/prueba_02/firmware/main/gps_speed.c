#include "gps_speed.h"

#include <math.h>
#include <stddef.h>

// Agrupa el resultado y la causa de aceptación o rechazo de una muestra.
static gps_speed_check_t check_result(bool valid, bool jump, float delta, float acceleration, const char *reason){
    return (gps_speed_check_t){
        .valid = valid,
        .jump_detected = jump,
        .delta_kmh = delta,
        .acceleration_kmh_s = acceleration,
        .reason = reason,
    };
}

// Marca velocidades fuera de rango o cambios incompatibles con el límite provisional.
gps_speed_check_t gps_speed_check( const gps_sample_t *previous, const gps_sample_t *current, uint32_t elapsed_ms, const gps_speed_config_t *config){
    if (previous == NULL || current == NULL || config == NULL || elapsed_ms == 0) {
        return check_result(false, false, 0.0f, 0.0f, "parametros_invalidos");
    }

    if (current->speed_kmh < 0.0f || current->speed_kmh > config->max_speed_kmh) {
        return check_result(false, true, 0.0f, 0.0f, "velocidad_fuera_de_rango");
    }

    float delta = current->speed_kmh - previous->speed_kmh, absolute_delta = delta < 0.0f ? -delta : delta;
    float elapsed_seconds = (float)elapsed_ms / 1000.0f, acceleration = absolute_delta / elapsed_seconds;

    if (acceleration > config->max_acceleration_kmh_s) {
        return check_result(false, true, delta, acceleration, "cambio_demasiado_rapido");
    }

    return check_result(true, false, delta, acceleration, "cambio_aceptable");
}

// Calcula una velocidad aproximada entre dos posiciones; se usa como contraste, no como dato principal.
gps_coordinate_speed_t gps_speed_from_coordinates(const gps_sample_t *previous,const gps_sample_t *current, uint32_t elapsed_ms){
    if (previous == NULL || current == NULL || elapsed_ms == 0 ||
        !previous->rmc_data_valid || !current->rmc_data_valid) {
        return (gps_coordinate_speed_t){ .valid = false, .speed_kmh = NAN };
    }

    const float deg_to_rad = 0.01745329251994329577f, earth_radius_m = 6371000.0f;
    float lat1 = previous->latitude_deg * deg_to_rad, lat2 = current->latitude_deg * deg_to_rad;
    float dlat = (current->latitude_deg - previous->latitude_deg) * deg_to_rad, dlon = (current->longitude_deg - previous->longitude_deg) * deg_to_rad;
    float sin_lat = sinf(dlat * 0.5f), sin_lon = sinf(dlon * 0.5f), a = sin_lat * sin_lat + cosf(lat1) * cosf(lat2) * sin_lon * sin_lon;
    if (a < 0.0f) a = 0.0f;
    if (a > 1.0f) a = 1.0f;

    float distance_m = 2.0f * earth_radius_m * atan2f(sqrtf(a), sqrtf(1.0f - a)), elapsed_s = (float)elapsed_ms / 1000.0f;
    float speed_kmh = (distance_m / elapsed_s) * 3.6f;
    return (gps_coordinate_speed_t){
        .valid = isfinite(speed_kmh),
        .speed_kmh = speed_kmh,
    };
}
