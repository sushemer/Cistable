#include "gps_nmea.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define NMEA_LINE_SIZE 128U

// Extrae el identificador de sentencia (por ejemplo, GPRMC) para el diagnóstico.
static void identify_type(const char *line, char type[7]){
    size_t i = 0;
    if (line != NULL && line[0] == '$') {
        for (i = 0; i < 6 && line[i + 1] != '\0' && line[i + 1] != ',' && line[i + 1] != '*'; ++i) {
            type[i] = line[i + 1];
        }
    }
    type[i] = '\0';
}

// Comprueba el XOR NMEA entre '$' y '*' antes de procesar los campos.
static bool checksum_valid(const char *line){
    if (line == NULL || line[0] != '$') return false;

    const char *star = strchr(line, '*');
    if (star == NULL || star[1] == '\0' || star[2] == '\0' ||
        star[3] != '\0') {
        return false;
    }

    unsigned char calculated = 0;
    for (const char *cursor = line + 1; cursor < star; ++cursor) {
        calculated ^= (unsigned char)*cursor;
    }
    char *end = NULL;
    unsigned long received = strtoul(star + 1, &end, 16);
    return end == star + 3 && received <= 0xFF && calculated == received;
}

// Convierte un campo decimal y rechaza texto incompleto o valores no finitos.
static bool parse_float(const char *text, float *value){
    if (text == NULL || text[0] == '\0' || value == NULL) {
        return false;
    }
    char *end = NULL;
    float parsed = strtof(text, &end);
    if (end == text || *end != '\0' || !isfinite(parsed)) {
        return false;
    }
    *value = parsed;
    return true;
}

// Convierte un campo entero decimal y verifica que quepa en el tipo de salida.
static bool parse_uint(const char *text, unsigned *value){
    if (text == NULL || text[0] == '\0' || value == NULL) {
        return false;
    }
    char *end = NULL;
    unsigned long parsed = strtoul(text, &end, 10);
    if (end == text || *end != '\0' || parsed > UINT32_MAX) {
        return false;
    }
    *value = (unsigned)parsed;
    return true;
}

// Convierte la hora UTC hhmmss.sss de NMEA a milisegundos desde medianoche.
static bool parse_time_of_day(const char *text, uint32_t *time_ms){
    if (text == NULL || text[0] == '\0' || time_ms == NULL) {
        return false;
    }
    char *end = NULL;
    double value = strtod(text, &end);
    if (end == text || *end != '\0' || !isfinite(value) || value < 0.0) {
        return false;
    }
    unsigned hours = (unsigned)(value / 10000.0);
    double remainder = value - (double)hours * 10000.0;
    unsigned minutes = (unsigned)(remainder / 100.0);
    double seconds = remainder - (double)minutes * 100.0;
    if (hours > 23 || minutes > 59 || seconds < 0.0 || seconds >= 60.0) {
        return false;
    }
    uint32_t whole_seconds = hours * 3600U + minutes * 60U + (unsigned)seconds, millis = (uint32_t)lround((seconds - (double)(unsigned)seconds) * 1000.0);
    if (millis >= 1000U) {
        ++whole_seconds;
        millis = 0;
    }
    *time_ms = whole_seconds * 1000U + millis;
    return true;
}

// Convierte coordenadas NMEA grados/minutos a grados decimales con signo.
static bool coordinate_to_degrees(const char *value, const char *hemisphere, bool latitude, float *result){
    float raw = 0.0f;
    if (!parse_float(value, &raw) || hemisphere == NULL || result == NULL ||
        hemisphere[0] == '\0' || hemisphere[1] != '\0' || raw < 0.0f) {
        return false;
    }
    unsigned degrees = (unsigned)(raw / 100.0f);
    float minutes = raw - (float)degrees * 100.0f;
    unsigned max_degrees = latitude ? 90U : 180U;
    if (minutes < 0.0f || minutes >= 60.0f || degrees > max_degrees ||
        (degrees == max_degrees && minutes != 0.0f)) {
        return false;
    }

    float converted = (float)degrees + minutes / 60.0f;
    if ((latitude && hemisphere[0] == 'S') ||
        (!latitude && hemisphere[0] == 'W')) {
        converted = -converted;
    } else if (!((latitude && hemisphere[0] == 'N') ||
                 (!latitude && hemisphere[0] == 'E'))) {
        return false;
    }
    *result = converted;
    return true;
}

// Separa los campos modificando las comas de la copia local de la sentencia.
static size_t split_fields(char *text, char *fields[], size_t capacity){
    size_t count = 0;
    char *cursor = text;
    while (count < capacity) {
        fields[count++] = cursor;
        char *comma = strchr(cursor, ',');
        if (comma == NULL) {
            break;
        }
        *comma = '\0';
        cursor = comma + 1;
    }
    return count;
}

// Verifica checksum y actualiza los campos RMC/GGA reconocidos; otras sentencias no actualizan la muestra.
bool gps_nmea_process_line( gps_sample_t *sample, const char *line, int64_t received_us,gps_nmea_result_t *result){
    if (result != NULL) {
        *result = (gps_nmea_result_t){0};
        identify_type(line, result->type);
    }
    if (sample == NULL || line == NULL || result == NULL) {
        return false;
    }
    result->checksum_valid = checksum_valid(line);
    if (!result->checksum_valid) {
        return false;
    }

    size_t length = strlen(line);
    char buffer[NMEA_LINE_SIZE];
    if (length == 0 || length >= sizeof(buffer)) {
        return false;
    }
    memcpy(buffer, line, length + 1);
    char *star = strchr(buffer, '*');
    if (star == NULL) {
        return false;
    }
    *star = '\0';

    char *fields[20] = {0};
    size_t count = split_fields(buffer, fields, 20);
    if (count < 2) {
        return false;
    }

    if ((strcmp(fields[0], "$GPRMC") == 0 || strcmp(fields[0], "$GNRMC") == 0) && count > 8) {
        float lat = 0.0f, lon = 0.0f, speed_knots = 0.0f, course = 0.0f;
        uint32_t tod_ms = 0;
        bool time_ok = parse_time_of_day(fields[1], &tod_ms), position_ok = coordinate_to_degrees(fields[3], fields[4], true, &lat) &&
                        coordinate_to_degrees(fields[5], fields[6], false, &lon), speed_ok = parse_float(fields[7], &speed_knots) && speed_knots >= 0.0f,
                        course_ok = fields[8][0] == '\0' || (parse_float(fields[8], &course) && course >= 0.0f && course <= 360.0f);

        sample->has_rmc = time_ok;
        sample->rmc_status_valid = fields[2][0] == 'A' && fields[2][1] == '\0';
        sample->rmc_data_valid = time_ok && position_ok && speed_ok && course_ok;
        sample->rmc_received_us = received_us;
        if (time_ok) {
            sample->rmc_time_of_day_ms = tod_ms;
            result->has_time = true;
            result->time_of_day_ms = tod_ms;
        }
        if (position_ok) {
            sample->latitude_deg = lat;
            sample->longitude_deg = lon;
        }
        if (speed_ok) {
            sample->speed_knots = speed_knots;
            sample->speed_kmh = speed_knots * 1.852f;
        }
        sample->course_deg = fields[8][0] != '\0' ? course : NAN;
        result->parsed = sample->rmc_data_valid;
        return result->parsed;
    }

    if ((strcmp(fields[0], "$GPGGA") == 0 || strcmp(fields[0], "$GNGGA") == 0) && count > 8) {
        uint32_t tod_ms = 0;
        unsigned quality = 0, satellites = 0;
        float hdop = 0.0f;
        bool time_ok = parse_time_of_day(fields[1], &tod_ms), quality_ok = parse_uint(fields[6], &quality) && quality <= INT32_MAX,
             satellites_ok = parse_uint(fields[7], &satellites) && satellites <= UINT8_MAX, hdop_ok = parse_float(fields[8], &hdop) && hdop >= 0.0f;

        sample->has_gga = time_ok;
        sample->gga_data_valid = quality_ok && satellites_ok && hdop_ok;
        sample->gga_received_us = received_us;
        if (time_ok) {
            sample->gga_time_of_day_ms = tod_ms;
            result->has_time = true;
            result->time_of_day_ms = tod_ms;
        }
        if (quality_ok) sample->fix_quality = (int)quality;
        if (satellites_ok) sample->satellites = (uint8_t)satellites;
        if (hdop_ok) sample->hdop = hdop;
        result->parsed = time_ok && sample->gga_data_valid;
        return result->parsed;
    }
    return false;
}
