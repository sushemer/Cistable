#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Registro normalizado que puede contener datos de GPS, IMU o ESP-NOW.
// Los campos que no correspondan al evento se escriben vacíos.
typedef struct {
    int64_t time_us;
    const char *evento;
    const char *dispositivo;
    float roll_deg;
    float pitch_deg;
    float yaw_deg;
    float accel_x;
    float accel_y;
    float accel_z;
    float gyro_x;
    float gyro_y;
    float gyro_z;
    float ay_m_s2;
    float ltr;
    float water_level_cm;
    int status_flags;
    int sensors_used;
    float lat_deg;
    float lon_deg;
    float speed_kmh;
    int gps_fix;
    int satellites;
    float hdop;
    int valid;
    int rssi_dbm;
    const char *linea_original;
} sd_logger_record_t;

// Abre o crea el CSV. La ruta debe pertenecer a un sistema FAT montado.
bool sd_logger_open(const char *path);

// Escribe el encabezado si el archivo es nuevo.
bool sd_logger_write_header(void);

// Agrega un registro y vacía el búfer para reducir pérdida ante un corte.
bool sd_logger_write_record(const sd_logger_record_t *record);

// Fuerza la escritura pendiente y cierra el archivo.
void sd_logger_close(void);

#ifdef __cplusplus
}
#endif
