#include "sd_logger.h"

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"

static const char *TAG = "SD_LOGGER";
static FILE *s_file;

// Convierte un valor flotante ausente en una celda vacía del CSV.
static void write_float(FILE *file, float value)
{
    if (isfinite(value)) {
        fprintf(file, "%.7g", (double)value);
    }
}

// Evita que una línea original con separadores rompa las columnas del CSV.
static void write_text(FILE *file, const char *text)
{
    fputc('"', file);
    if (text != NULL) {
        for (const char *p = text; *p != '\0'; ++p) {
            if (*p == '"') fputc('"', file);
            fputc(*p, file);
        }
    }
    fputc('"', file);
}

bool sd_logger_open(const char *path)
{
    if (path == NULL || path[0] == '\0') return false;
    sd_logger_close();
    s_file = fopen(path, "a+");
    if (s_file == NULL) {
        ESP_LOGE(TAG, "No se pudo abrir %s: errno=%d", path, errno);
        return false;
    }
    return true;
}

bool sd_logger_write_header(void)
{
    if (s_file == NULL) return false;
    if (fseek(s_file, 0, SEEK_END) != 0) return false;
    long size = ftell(s_file);
    if (size > 0) return true;
    fprintf(s_file,
            "registro;time_us;evento;dispositivo;roll_deg;pitch_deg;yaw_deg;"
            "accel_x;accel_y;accel_z;gyro_x;gyro_y;gyro_z;ay_m_s2;ltr;"
            "water_level_cm;status_flags;sensors_used;lat_deg;lon_deg;"
            "speed_kmh;gps_fix;satellites;hdop;valid;rssi_dbm;linea_original\n");
    return fflush(s_file) == 0;
}

bool sd_logger_write_record(const sd_logger_record_t *record)
{
    static uint32_t sequence;
    if (s_file == NULL || record == NULL) return false;
    fprintf(s_file, "%" PRIu32 ";%" PRId64 ";", ++sequence, record->time_us);
    write_text(s_file, record->evento);
    fputc(';', s_file);
    write_text(s_file, record->dispositivo);
    fputc(';', s_file);
    write_float(s_file, record->roll_deg); fputc(';', s_file);
    write_float(s_file, record->pitch_deg); fputc(';', s_file);
    write_float(s_file, record->yaw_deg); fputc(';', s_file);
    write_float(s_file, record->accel_x); fputc(';', s_file);
    write_float(s_file, record->accel_y); fputc(';', s_file);
    write_float(s_file, record->accel_z); fputc(';', s_file);
    write_float(s_file, record->gyro_x); fputc(';', s_file);
    write_float(s_file, record->gyro_y); fputc(';', s_file);
    write_float(s_file, record->gyro_z); fputc(';', s_file);
    write_float(s_file, record->ay_m_s2); fputc(';', s_file);
    write_float(s_file, record->ltr); fputc(';', s_file);
    write_float(s_file, record->water_level_cm); fputc(';', s_file);
    fprintf(s_file, "%d;%d;", record->status_flags, record->sensors_used);
    write_float(s_file, record->lat_deg); fputc(';', s_file);
    write_float(s_file, record->lon_deg); fputc(';', s_file);
    write_float(s_file, record->speed_kmh); fputc(';', s_file);
    fprintf(s_file, "%d;%d;", record->gps_fix, record->satellites);
    write_float(s_file, record->hdop); fputc(';', s_file);
    fprintf(s_file, "%d;%d;", record->valid, record->rssi_dbm);
    write_text(s_file, record->linea_original);
    fputc('\n', s_file);
    return fflush(s_file) == 0;
}

void sd_logger_close(void)
{
    if (s_file != NULL) {
        fflush(s_file);
        fclose(s_file);
        s_file = NULL;
    }
}
