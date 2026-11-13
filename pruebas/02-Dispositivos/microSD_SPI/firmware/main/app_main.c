#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <unistd.h>

#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdmmc_cmd.h"

// Prueba aislada: solamente microSD por SPI en un ESP32 DevKit V1.
// No se inicializan sensores, UART ni ESP-NOW.
#define SD_SPI_SCK_GPIO   GPIO_NUM_14
#define SD_SPI_MOSI_GPIO  GPIO_NUM_13
#define SD_SPI_MISO_GPIO  GPIO_NUM_27
#define SD_SPI_CS_GPIO    GPIO_NUM_26

#define SD_MOUNT_POINT    "/sdcard"
#define SD_FILE_PREFIX    "raw_"
#define SD_FILE_EXTENSION ".log"

static const char *TAG = "MICROSD_TEST";
static sdmmc_card_t *s_card;
static FILE *s_file;

// Busca el siguiente número para no sobrescribir pruebas anteriores.
static unsigned find_next_file_number(void)
{
    unsigned highest = 0;
    DIR *directory = opendir(SD_MOUNT_POINT);
    if (directory == NULL) return 1;

    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        unsigned number = 0;
        char suffix[8] = {0};
        const size_t prefix_length = strlen(SD_FILE_PREFIX);
        if (strncasecmp(entry->d_name, SD_FILE_PREFIX, prefix_length) != 0) continue;

        if (sscanf(entry->d_name + prefix_length, "%u%7s", &number, suffix) == 2 &&
            strcasecmp(suffix, SD_FILE_EXTENSION) == 0 && number > highest) {
            highest = number;
        }
    }
    closedir(directory);
    return highest + 1;
}

// Abre un único archivo crudo por ejecución.
static bool open_raw_file(void)
{
    const unsigned number = find_next_file_number();
    char path[64];
    snprintf(path, sizeof(path), SD_MOUNT_POINT "/%s%04u%s",
             SD_FILE_PREFIX, number, SD_FILE_EXTENSION);

    s_file = fopen(path, "w");
    if (s_file == NULL) {
        ESP_LOGE(TAG, "No se pudo crear %s: errno=%d (%s)",
                 path, errno, strerror(errno));
        return false;
    }

    fprintf(s_file, "registro;time_us;origen;linea_original\n");
    fflush(s_file);
    fsync(fileno(s_file));
    ESP_LOGI(TAG, "Archivo crudo activo: %s", path);
    return true;
}

// Monta la tarjeta con el pinout del DevKit V1.
static esp_err_t mount_sd_card(void)
{
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 4,
        .allocation_unit_size = 16 * 1024,
    };

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.max_freq_khz = 4000;

    spi_bus_config_t bus_config = {
        .mosi_io_num = SD_SPI_MOSI_GPIO,
        .miso_io_num = SD_SPI_MISO_GPIO,
        .sclk_io_num = SD_SPI_SCK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    esp_err_t error = spi_bus_initialize(host.slot, &bus_config, SDSPI_DEFAULT_DMA);
    if (error != ESP_OK && error != ESP_ERR_INVALID_STATE) return error;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_SPI_CS_GPIO;
    slot_config.host_id = host.slot;

    return esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot_config,
                                   &mount_config, &s_card);
}

// Escribe una línea con formato común para que después se separe en Excel.
static bool write_record(unsigned record, const char *origin, const char *line)
{
    const int64_t time_us = esp_timer_get_time();
    if (fprintf(s_file, "%u;%lld;%s;%s\n", record, (long long)time_us,
                origin, line) < 0) {
        return false;
    }

    // La sincronización permite retirar la tarjeta después de detener la prueba.
    return fflush(s_file) == 0 && fsync(fileno(s_file)) == 0;
}

// Genera una línea simulada de cada origen, sin afirmar que proviene de un sensor.
static void build_simulated_line(unsigned record, const char **origin, char *line, size_t size)
{
    switch (record % 5U) {
        case 1:
            *origin = "IMU1";
            snprintf(line, size,
                     "[DATA] IMU1,time_us=%lld,roll_deg=6.650,pitch_deg=1.250,yaw_deg=-2.900,"
                     "accel_x=0.010,accel_y=0.055,accel_z=0.020,gyro_x=0.0010,gyro_y=0.0020,"
                     "gyro_z=0.0030,ltr=0.18226,valid=1", (long long)esp_timer_get_time());
            break;
        case 2:
            *origin = "IMU2";
            snprintf(line, size,
                     "[DATA] IMU2,time_us=%lld,roll_deg=-76.330,pitch_deg=0.370,ay_m_s2=-0.012,"
                     "ltr=-0.85406,valid=1,status_flags=0x01", (long long)esp_timer_get_time());
            break;
        case 3:
            *origin = "IMU_A";
            snprintf(line, size,
                     "[DATA] IMU_A,time_us=%lld,roll_deg=-34.840,ay_m_s2=0.021,"
                     "ltr=-0.83523,sensors_used=2,valid=1", (long long)esp_timer_get_time());
            break;
        case 4:
            *origin = "GPS";
            snprintf(line, size,
                     "[DATA] GPS,time_us=%lld,lat_deg=32.5123000,lon_deg=-116.9639000,"
                     "speed_kmh=8.500,fix=1,satellites=8,hdop=1.20,ltr=-0.83523,"
                     "telemetry_roll_deg=-34.840", (long long)esp_timer_get_time());
            break;
        default:
            *origin = "EVENT";
            snprintf(line, size,
                     "[EVENT] origen=MICROSD event=SIMULACION detail=sin_sensores_ni_espnow");
            break;
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Prueba aislada: microSD por SPI, sin sensores ni ESP-NOW");
    ESP_LOGI(TAG, "SCK=%d MOSI=%d MISO=%d CS=%d", SD_SPI_SCK_GPIO,
             SD_SPI_MOSI_GPIO, SD_SPI_MISO_GPIO, SD_SPI_CS_GPIO);

    esp_err_t error = mount_sd_card();
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo montar la microSD: %s", esp_err_to_name(error));
        return;
    }

    sdmmc_card_print_info(stdout, s_card);
    if (!open_raw_file()) return;

    unsigned record = 0;
    while (true) {
        ++record;
        const char *origin = NULL;
        char line[512];
        build_simulated_line(record, &origin, line, sizeof(line));
        if (!write_record(record, origin, line)) {
            ESP_LOGE(TAG, "No se pudo sincronizar el registro %u: errno=%d (%s)",
                     record, errno, strerror(errno));
            break;
        }
        ESP_LOGI(TAG, "Registro crudo guardado: %u (%s)", record, origin);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
