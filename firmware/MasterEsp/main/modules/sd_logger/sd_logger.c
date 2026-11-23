#include "sd_logger.h"
#include "sd_logger_config.h"

#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sdmmc_cmd.h"

#define SD_LOGGER_MOUNT_POINT    "/sdcard"
#define SD_LOGGER_RUN_PREFIX     "RAW_"
#define SD_LOGGER_RAW_EXTENSION  ".LOG"

#define SD_LOGGER_STARTUP_ATTEMPTS 3U
#define SD_LOGGER_STARTUP_RETRY_MS 1000U

static const char *TAG = "SD_LOGGER";
static FILE *s_raw_file;
static FILE *s_imu1_file;
static FILE *s_imu2_file;
static FILE *s_imu_a_file;
static FILE *s_gps_file;
static FILE *s_event_file;
static FILE *s_other_file;
static char s_run_path[64];
static unsigned s_record = 0;
#define SD_LOGGER_CSV_SEPARATOR ','
#define SD_LOGGER_STARTUP_COMPLETE BIT0

// La cola evita acceso simultáneo de sensor_task/gui_task a FAT y esperas de disco.
typedef struct {
    int64_t time_us;
    char line[SD_LOGGER_LINE_BYTES];
} sd_log_message_t;

static QueueHandle_t s_queue;
static EventGroupHandle_t s_events;
static sdmmc_card_t *s_card;
static volatile bool s_ready;
static unsigned s_dropped;
static portMUX_TYPE s_drop_mux = portMUX_INITIALIZER_UNLOCKED;

static const char *field_value(const char *line, const char *key,
                               char *buffer, size_t size)
{
    if (buffer == NULL || size == 0) return "";
    buffer[0] = '\0'; // Un campo ausente queda vacío, sin repetir el valor anterior.
    if (line == NULL || key == NULL) return buffer;
    const char *start = strstr(line, key);
    if (start == NULL) return "";
    start += strlen(key);
    const char *end = strchr(start, ',');
    if (end == NULL) end = start + strlen(start);
    size_t length = (size_t)(end - start);
    if (length >= size) length = size - 1;
    memcpy(buffer, start, length);
    buffer[length] = '\0';
    return buffer;
}

static void split_local_time(const char *local_time, char *date, size_t date_size,
                             char *clock, size_t clock_size)
{
    if (local_time == NULL || strlen(local_time) < 19) {
        if (date_size > 0) date[0] = '\0';
        if (clock_size > 0) clock[0] = '\0';
        return;
    }
    snprintf(date, date_size, "%.10s", local_time);
    snprintf(clock, clock_size, "%.8s", local_time + 11);
}

static void csv_text(FILE *file, const char *text)
{
    fputc('"', file);
    if (text != NULL) {
        for (const char *cursor = text; *cursor != '\0'; ++cursor) {
            if (*cursor == '"') fputc('"', file);
            fputc(*cursor, file);
        }
    }
    fputc('"', file);
}

static void write_structured_row(FILE *file, unsigned record, int64_t time_us,
                                 const char *local_time, const char *origin,
                                 const char *line)
{
    if (file == NULL) return;

    char date[16], clock[16], value[64];
    split_local_time(local_time, date, sizeof(date), clock, sizeof(clock));
    fprintf(file, "%u%c%lld%c%s%c%s%c", record, SD_LOGGER_CSV_SEPARATOR,
            (long long)time_us, SD_LOGGER_CSV_SEPARATOR, date,
            SD_LOGGER_CSV_SEPARATOR, clock, SD_LOGGER_CSV_SEPARATOR);

    if (strcmp(origin, "IMU1") == 0) {
        field_value(line, "time_us=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "SIM=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "roll_deg=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "pitch_deg=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "yaw_deg=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "accel_x=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "accel_y=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "accel_z=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "gyro_x=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "gyro_y=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "gyro_z=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "ltr=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "valid=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
    } else if (strcmp(origin, "IMU2") == 0) {
        field_value(line, "time_us=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "SIM=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "roll_deg=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "pitch_deg=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "ay_m_s2=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "water_level_cm=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "status_flags=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "ltr=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "valid=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
    } else if (strcmp(origin, "IMU_A") == 0) {
        field_value(line, "time_us=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "SIM=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "roll_deg=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "ay_m_s2=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "ltr=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "sensors_used=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "max_safe_speed_kmh=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "min_turn_radius_m=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "predicted_ltr=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "speed_turn_risk=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "dynamic_risk=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "pitch_deg=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "pitch_ltr=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "predicted_pitch_ltr=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "pitch_risk=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
    } else if (strcmp(origin, "GPS") == 0) {
        field_value(line, "time_us=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "SIM=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "lat_deg=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "lon_deg=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "speed_kmh=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "speed_mph=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "fix=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "satellites=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "hdop=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "ltr=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "telemetry_roll_deg=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "gps_status=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "gps_reason=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "rmc_age_ms=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "gga_age_ms=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "epoch_skew_ms=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "speed_corrected_kmh=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "fusion_state=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "imu_accel_norm_m_s2=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "imu_stationary_ms=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "imu_fresh=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "speed_usable=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
    } else if (strcmp(origin, "EVENT") == 0) {
        field_value(line, "SIM=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "gps_status=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "gps_quality_state=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "gps_reason=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "fusion_state=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "imu_status=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "espnow_status=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "ltr=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "static_risk=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "max_safe_speed_kmh=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "min_turn_radius_m=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "predicted_ltr=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "speed_turn_risk=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "dynamic_risk=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "pitch_deg=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "pitch_ltr=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "predicted_pitch_ltr=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
        field_value(line, "pitch_risk=", value, sizeof(value)); fprintf(file, "%s%c", value, SD_LOGGER_CSV_SEPARATOR);
    }

    csv_text(file, line);
    fputc('\n', file);
}

// Obtiene la hora local configurada por el GPS; antes de sincronizar puede ser 1980.
static void local_time_string(char *buffer, size_t size)
{
    struct timeval now = {0};
    struct tm local = {0};
    gettimeofday(&now, NULL);
    // Antes de sincronizar con GPS, el RTC puede iniciar en 1970 o 1980.
    // Se deja vacío para no presentar una fecha falsa en Excel.
    if (now.tv_sec < 1577836800) {
        if (size > 0) buffer[0] = '\0';
        return;
    }
    localtime_r(&now.tv_sec, &local);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", &local);
}

// Identifica el origen para que el conversor pueda separar las hojas del Excel.
static const char *origin_from_line(const char *line)
{
    if (strstr(line, "[DATA] IMU1") == line) return "IMU1";
    // IMU2 RX es el mensaje crudo de depuración; solo la línea con time_us
    // tiene todas las columnas para IMU2.CSV. El mensaje RX queda en OTRO.CSV
    // y también se conserva en el archivo crudo.
    if (strstr(line, "[DATA] IMU2,") == line) return "IMU2";
    if (strstr(line, "[DATA] IMU_A") == line) return "IMU_A";
    if (strstr(line, "[DATA] GPS") == line) return "GPS";
    if (strstr(line, "[EVENT]") == line) return "EVENT";
    return "OTRO";
}

static unsigned find_next_run_number(void)
{
    unsigned highest = 0;
    DIR *directory = opendir(SD_LOGGER_MOUNT_POINT);
    if (directory == NULL) return 1;

    struct dirent *entry;
    const size_t prefix_length = strlen(SD_LOGGER_RUN_PREFIX);
    while ((entry = readdir(directory)) != NULL) {
        if (strncasecmp(entry->d_name, SD_LOGGER_RUN_PREFIX, prefix_length) != 0) {
            continue;
        }

        unsigned number = 0;
        if (sscanf(entry->d_name + prefix_length, "%u", &number) == 1 &&
            number > highest) {
            highest = number;
        }
    }
    closedir(directory);
    return highest + 1;
}

static const char *origin_file_name(const char *origin)
{
    if (strcmp(origin, "IMU1") == 0) return "IMU1.CSV";
    if (strcmp(origin, "IMU2") == 0) return "IMU2.CSV";
    if (strcmp(origin, "IMU_A") == 0) return "IMU_A.CSV";
    if (strcmp(origin, "GPS") == 0) return "GPS.CSV";
    if (strcmp(origin, "EVENT") == 0) return "EVENT.CSV";
    return "OTRO.CSV";
}

static FILE *file_for_origin(const char *origin)
{
    if (strcmp(origin, "IMU1") == 0) return s_imu1_file;
    if (strcmp(origin, "IMU2") == 0) return s_imu2_file;
    if (strcmp(origin, "IMU_A") == 0) return s_imu_a_file;
    if (strcmp(origin, "GPS") == 0) return s_gps_file;
    if (strcmp(origin, "EVENT") == 0) return s_event_file;
    return s_other_file;
}

// Cierra archivos parciales o fallidos antes de reintentar el montaje.
static void close_log_files(void)
{
    FILE **files[] = {&s_raw_file, &s_imu1_file, &s_imu2_file, &s_imu_a_file,
                      &s_gps_file, &s_event_file, &s_other_file};
    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); ++i) {
        if (*files[i] != NULL) fclose(*files[i]);
        *files[i] = NULL;
    }
}

static bool open_log_file(void)
{
    const unsigned run_number = find_next_run_number();
    snprintf(s_run_path, sizeof(s_run_path), SD_LOGGER_MOUNT_POINT "/%s%04u",
             SD_LOGGER_RUN_PREFIX, run_number);

    if (mkdir(s_run_path, 0777) != 0) {
        ESP_LOGE(TAG, "No se pudo crear la carpeta %s: errno=%d (%s)",
                 s_run_path, errno, strerror(errno));
        return false;
    }

    char path[96];
    snprintf(path, sizeof(path), "%s/%s%04u%s", s_run_path,
             SD_LOGGER_RUN_PREFIX, run_number, SD_LOGGER_RAW_EXTENSION);
    s_raw_file = fopen(path, "w");
    if (s_raw_file == NULL) {
        ESP_LOGE(TAG, "No se pudo crear %s: errno=%d (%s)",
                 path, errno, strerror(errno));
        return false;
    }

    FILE **files[] = {&s_imu1_file, &s_imu2_file, &s_imu_a_file, &s_gps_file,
                      &s_event_file, &s_other_file};
    const char *origins[] = {"IMU1", "IMU2", "IMU_A", "GPS", "EVENT", "OTRO"};
    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); ++i) {
        snprintf(path, sizeof(path), "%s/%s", s_run_path, origin_file_name(origins[i]));
        *files[i] = fopen(path, "w");
        if (*files[i] == NULL) {
            ESP_LOGE(TAG, "No se pudo crear %s: errno=%d (%s)",
                     path, errno, strerror(errno));
            return false;
        }
        // Excel reconoce el separador aunque Windows use otra configuración regional.
        fputs("\xEF\xBB\xBF" "sep=,\r\n", *files[i]);
    }

    fprintf(s_raw_file, "registro;time_us;fecha_hora_local;origen;linea_original\n");
    fprintf(s_imu1_file, "registro,time_us,fecha,hora,imu_time_us,sim,roll_deg,pitch_deg,yaw_deg,accel_x,accel_y,accel_z,gyro_x,gyro_y,gyro_z,ltr,valid,linea_original\n");
    fprintf(s_imu2_file, "registro,time_us,fecha,hora,imu_time_us,sim,roll_deg,pitch_deg,ay_m_s2,water_level_cm,status_flags,ltr,valid,linea_original\n");
    fprintf(s_imu_a_file, "registro,time_us,fecha,hora,imu_time_us,sim,roll_deg,ay_m_s2,ltr,sensors_used,max_safe_speed_kmh,min_turn_radius_m,predicted_ltr,speed_turn_risk,dynamic_risk,pitch_deg,pitch_ltr,predicted_pitch_ltr,pitch_risk,linea_original\n");
    fprintf(s_gps_file, "registro,time_us,fecha,hora,gps_time_us,sim,lat_deg,lon_deg,speed_kmh,speed_mph,fix,satellites,hdop,ltr,telemetry_roll_deg,gps_status,gps_reason,rmc_age_ms,gga_age_ms,epoch_skew_ms,speed_corrected_kmh,fusion_state,imu_accel_norm_m_s2,imu_stationary_ms,imu_fresh,speed_usable,linea_original\n");
    fprintf(s_event_file, "registro,time_us,fecha,hora,sim,gps_status,gps_quality_state,gps_reason,fusion_state,imu_status,espnow_status,ltr,static_risk,max_safe_speed_kmh,min_turn_radius_m,predicted_ltr,speed_turn_risk,dynamic_risk,pitch_deg,pitch_ltr,predicted_pitch_ltr,pitch_risk,linea_original\n");
    fprintf(s_other_file, "registro,time_us,fecha,hora,linea_original\n");

    FILE *all_files[] = {s_raw_file, s_imu1_file, s_imu2_file, s_imu_a_file,
                         s_gps_file, s_event_file, s_other_file};
    for (size_t i = 0; i < sizeof(all_files) / sizeof(all_files[0]); ++i) {
        if (fflush(all_files[i]) != 0 || fsync(fileno(all_files[i])) != 0) {
            ESP_LOGE(TAG, "No se pudo sincronizar el encabezado: errno=%d (%s)",
                     errno, strerror(errno));
            return false;
        }
    }

    ESP_LOGI(TAG, "Carpeta de registro activa: %s", s_run_path);
    ESP_LOGI(TAG, "Archivos: RAW_%04u.LOG, IMU1.CSV, IMU2.CSV, IMU_A.CSV, GPS.CSV, EVENT.CSV y OTRO.CSV",
             run_number);
    return true;
}

static bool mount_with_pins(int cs_gpio)
{
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 8,
        .allocation_unit_size = 16 * 1024,
    };

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SD_LOGGER_SPI_HOST;
    host.max_freq_khz = SD_LOGGER_MAX_FREQ_KHZ;

    esp_err_t error;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = cs_gpio;
    slot_config.host_id = host.slot;

    error = esp_vfs_fat_sdspi_mount(SD_LOGGER_MOUNT_POINT, &host, &slot_config,
                                    &mount_config, &s_card);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo montar la microSD: %s", esp_err_to_name(error));
        // SPI2 también pertenece al LCD: se mantiene disponible si la SD falla.
        return false;
    }

    sdmmc_card_print_info(stdout, s_card);
    if (open_log_file()) return true;
    close_log_files();
    esp_vfs_fat_sdcard_unmount(SD_LOGGER_MOUNT_POINT, s_card);
    s_card = NULL;
    return false;
}

static bool mount_log(void)
{
    ESP_LOGI(TAG, "Probando SPI microSD principal: SCK=%d MOSI=%d MISO=%d CS=%d",
             SD_LOGGER_SCK_GPIO, SD_LOGGER_MOSI_GPIO,
             SD_LOGGER_MISO_GPIO, SD_LOGGER_CS_GPIO);
    if (mount_with_pins(SD_LOGGER_CS_GPIO)) {
        return true;
    }
    return false;
}

// Solo el escritor toca los archivos; conserva el instante de recepción en la cola.
static bool write_line(const sd_log_message_t *message)
{
    const char *line = message->line;
    if (s_raw_file == NULL) return false;

    ++s_record;
    char local_time[24];
    local_time_string(local_time, sizeof(local_time));
    const char *origin = origin_from_line(line);
    FILE *csv_file = file_for_origin(origin);
    if (csv_file == NULL) return false;
    const int64_t time_us = message->time_us;

    // El LOG conserva el mensaje sin comillas añadidas; el conversor existente
    // puede reconocer directamente [DATA] y [EVENT] en la última columna.
    fprintf(s_raw_file, "%u;%lld;%s;%s;%s\n", s_record,
            (long long)time_us, local_time, origin, line);
    write_structured_row(csv_file, s_record, time_us, local_time, origin, line);

    if (fflush(s_raw_file) != 0 || fflush(csv_file) != 0 ||
        fsync(fileno(s_raw_file)) != 0 || fsync(fileno(csv_file)) != 0) {
        ESP_LOGE(TAG, "No se pudo sincronizar la línea: errno=%d (%s)",
                 errno, strerror(errno));
        return false;
    }
    return true;
}

// Monta/reintenta y vacía la cola. Una falla de almacenamiento no detiene sensores.
static void writer_task(void *arg)
{
    (void)arg;
    esp_log_level_set("sdmmc_cmd", ESP_LOG_DEBUG);
    bool mounted = false;
    for (unsigned attempt = 1; attempt <= SD_LOGGER_STARTUP_ATTEMPTS; ++attempt) {
        mounted = mount_log();
        if (mounted) break;
        if (attempt < SD_LOGGER_STARTUP_ATTEMPTS) {
            vTaskDelay(pdMS_TO_TICKS(SD_LOGGER_STARTUP_RETRY_MS));
        }
    }
    s_ready = mounted;
    xEventGroupSetBits(s_events, SD_LOGGER_STARTUP_COMPLETE);
    if (!mounted) {
        ESP_LOGW(TAG, "MicroSD no disponible tras %u intentos; se habilita la pantalla y se detienen los reintentos para compartir SPI",
                 (unsigned)SD_LOGGER_STARTUP_ATTEMPTS);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "MicroSD lista; escritura en segundo plano a %u kHz",
             (unsigned)SD_LOGGER_MAX_FREQ_KHZ);
    while (true) {
        if (!s_ready) {
            mounted = mount_log();
            s_ready = mounted;
            if (!mounted) {
                ESP_LOGW(TAG, "MicroSD desconectada; reintento en 5 segundos");
                vTaskDelay(pdMS_TO_TICKS(5000));
                continue;
            }
        }
        sd_log_message_t message;
        while (xQueueReceive(s_queue, &message, portMAX_DELAY) == pdTRUE) {
            if (!write_line(&message)) break;
            unsigned dropped;
            portENTER_CRITICAL(&s_drop_mux);
            dropped = s_dropped;
            s_dropped = 0;
            portEXIT_CRITICAL(&s_drop_mux);
            if (dropped > 0) ESP_LOGW(TAG, "Cola microSD llena: %u líneas omitidas", dropped);
        }
        s_ready = false;
        close_log_files();
        esp_vfs_fat_sdcard_unmount(SD_LOGGER_MOUNT_POINT, s_card);
        s_card = NULL;
        xQueueReset(s_queue);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

bool sd_logger_start(void)
{
    if (s_queue != NULL) return true;
    s_queue = xQueueCreate(16, sizeof(sd_log_message_t));
    s_events = xEventGroupCreate();
    if (s_queue != NULL && s_events != NULL &&
        xTaskCreate(writer_task, "sd_writer", 6144, NULL, 3, NULL) == pdPASS) return true;
    if (s_queue != NULL) vQueueDelete(s_queue);
    if (s_events != NULL) vEventGroupDelete(s_events);
    s_queue = NULL;
    s_events = NULL;
    ESP_LOGE(TAG, "No se pudo crear el escritor de la microSD");
    return false;
}

void sd_logger_wait_startup(void)
{
    if (s_events != NULL) {
        xEventGroupWaitBits(s_events, SD_LOGGER_STARTUP_COMPLETE, pdFALSE, pdTRUE, portMAX_DELAY);
    }
}

bool sd_logger_is_ready(void)
{
    return s_ready;
}

bool sd_logger_write_line(const char *line)
{
    if (!s_ready || line == NULL || strlen(line) >= SD_LOGGER_LINE_BYTES) return false;
    sd_log_message_t message = {.time_us = esp_timer_get_time()};
    memcpy(message.line, line, strlen(line) + 1);
    if (xQueueSend(s_queue, &message, 0) == pdTRUE) return true;
    portENTER_CRITICAL(&s_drop_mux);
    ++s_dropped;
    portEXIT_CRITICAL(&s_drop_mux);
    return false;
}

void sd_logger_logf(const char *tag, const char *format, ...)
{
    char line[SD_LOGGER_LINE_BYTES];
    va_list args;
    va_start(args, format);
    const int length = vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    ESP_LOGI(tag, "%s", line);
    if (length >= 0 && (size_t)length < sizeof(line)) (void)sd_logger_write_line(line);
}
