#include "gps_validation.h"

#include <limits.h>
#include <stddef.h>

// Calcula la antigüedad de una trama; UINT32_MAX indica que no hay marca válida.
static uint32_t age_ms(int64_t now_us, int64_t received_us){
    if (received_us <= 0 || now_us < received_us) return UINT32_MAX;

    uint64_t age = (uint64_t)(now_us - received_us) / 1000ULL;
    return age > UINT32_MAX ? UINT32_MAX : (uint32_t)age;
}

// Mide la diferencia entre horas UTC considerando el cruce de medianoche.
static uint32_t epoch_skew_ms(uint32_t a, uint32_t b){
    const uint32_t day_ms = 86400000U;
    uint32_t direct = a > b ? a - b : b - a, wrapped = day_ms - direct;
    return direct < wrapped ? direct : wrapped;
}

// Construye un resultado uniforme para todos los estados de validación.
static gps_validation_result_t result( gps_validation_state_t state, bool usable, uint32_t rmc_age, uint32_t gga_age,uint32_t skew, const char *reason){
    return (gps_validation_result_t){
        .state = state,
        .usable_for_speed = usable,
        .rmc_age_ms = rmc_age,
        .gga_age_ms = gga_age,
        .epoch_skew_ms = skew,
        .reason = reason,
    };
}

// Acepta el fix para análisis de velocidad solo si RMC/GGA son recientes y cumplen los criterios de calidad.
gps_validation_result_t gps_validation_evaluate( const gps_sample_t *sample, const gps_validation_config_t *config, int64_t now_us){
    if (sample == NULL || config == NULL || now_us < 0) {
        return result(GPS_VALIDATION_NO_DATA, false, UINT32_MAX, UINT32_MAX, UINT32_MAX, "parametros_invalidos");
    }
    if (!sample->has_rmc && !sample->has_gga) {
        return result(GPS_VALIDATION_NO_DATA, false, UINT32_MAX, UINT32_MAX, UINT32_MAX, "sin_datos");
    }

    uint32_t rmc_age = age_ms(now_us, sample->rmc_received_us), gga_age = age_ms(now_us, sample->gga_received_us),
             skew = sample->has_rmc && sample->has_gga ? epoch_skew_ms(sample->rmc_time_of_day_ms, sample->gga_time_of_day_ms) : UINT32_MAX;

    if (!sample->has_rmc || !sample->has_gga) {
        bool stale = (sample->has_rmc && rmc_age > config->max_age_ms) || (sample->has_gga && gga_age > config->max_age_ms);
        if (stale) {
            return result(GPS_VALIDATION_OBSOLETO, false, rmc_age, gga_age,  skew, "trama_obsoleta");
        }
        return result(GPS_VALIDATION_DEGRADADO, false, rmc_age, gga_age, skew, "falta_rmc_o_gga");
    }
    if (rmc_age > config->max_age_ms || gga_age > config->max_age_ms) {
        return result(GPS_VALIDATION_OBSOLETO, false, rmc_age, gga_age, skew, "trama_obsoleta");
    }
    if (!sample->rmc_status_valid || !sample->rmc_data_valid ||  !sample->gga_data_valid || sample->fix_quality <= 0) {
        return result(GPS_VALIDATION_SIN_FIX, false, rmc_age, gga_age, skew, "fix_o_campos_invalidos");
    }
    if (skew > config->max_epoch_skew_ms) {
        return result(GPS_VALIDATION_DEGRADADO, false, rmc_age, gga_age, skew, "epocas_rmc_gga_no_coinciden");
    }
    if (sample->satellites < config->min_satellites) {
        return result(GPS_VALIDATION_DEGRADADO, false, rmc_age, gga_age, skew, "pocos_satellites");
    }
    if (sample->hdop <= 0.0f || sample->hdop > config->max_hdop) {
        return result(GPS_VALIDATION_DEGRADADO, false, rmc_age, gga_age, skew, "hdop_fuera_de_criterio");
    }
    return result(GPS_VALIDATION_VALIDO, true, rmc_age, gga_age, skew, "calidad_gps_aceptable");
}

// Devuelve el nombre legible que se escribe en los registros.
const char *gps_validation_state_name(gps_validation_state_t state){
    switch (state) {
        case GPS_VALIDATION_NO_DATA: return "SIN_DATOS";
        case GPS_VALIDATION_SIN_FIX: return "SIN_FIX";
        case GPS_VALIDATION_DEGRADADO: return "FIX_DEGRADADO";
        case GPS_VALIDATION_VALIDO: return "FIX_VALIDO";
        case GPS_VALIDATION_OBSOLETO: return "DATO_OBSOLETO";
        default: return "ESTADO_DESCONOCIDO";
    }
}

