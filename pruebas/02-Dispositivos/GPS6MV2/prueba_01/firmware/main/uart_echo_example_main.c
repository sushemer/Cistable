/*
 * Lee sentencias NMEA del GPS y procesa:
 *   - RMC: posición, velocidad, rumbo y validez del fix
 *   - GGA: calidad del fix, satélites y HDOP
 *  Ejemplo de como se recibe: $GNRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*74\r\n
  
 * FLUJO:
 * 1. app_main() inicializa la muestra y crea gps_task().
 * 2. gps_task() configura el UART y recibe bytes del módulo GPS.
 * 3. Los bytes se agrupan hasta formar una línea NMEA completa.
 * 4. process_nmea_line() valida el checksum y separa los campos.
 * 5. RMC actualiza posición, velocidad y rumbo; GGA actualiza calidad, satélites y HDOP, y puede invalidar una posición deficiente.
 * 6. Cada segundo gps_log_sample() calcula la antigüedad y publica el estado.

 * IMPORTANTE: un RMC con estado 'A' no basta por sí solo. La calidad GGA, el HDOP y la antigüedad también deben considerarse antes de usar el dato.
 */
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Ajustar estos valores al módulo y a la placa utilizados.
#define GPS_UART_PORT       UART_NUM_2
#define GPS_UART_BAUD_RATE  9600//115200 //9600
#define GPS_UART_RXD        16
#define GPS_UART_TXD        17
#define GPS_UART_RTS        UART_PIN_NO_CHANGE
#define GPS_UART_CTS        UART_PIN_NO_CHANGE
// Tamaños de buffer y límites de edad para determinar si el fix es válido.
#define GPS_RX_BUFFER_SIZE  2048
#define GPS_LINE_SIZE       128
#define GPS_TASK_STACK      4096
#define GPS_MAX_AGE_MS      3000
static const char *TAG = "GPS";

typedef struct {
    // Ejemplo de datos extraídos de RMC y GGA: $GNRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*74
    bool valid_fix; 
    uint8_t satellites; //
    float latitude_deg; //
    float longitude_deg; //
    float speed_kmh; //
    float course_deg; // 
    float hdop;  //
    uint32_t age_ms; //
    uint64_t sample_time_us; //
} gps_sample_t;
static gps_sample_t gps_sample;
static uint64_t last_rmc_us;

// Devuelve true si la línea es una sentencia RMC, sin importar el proveedor GNSS.
static bool is_rmc(const char *id){
    return strcmp(id, "$GPRMC") == 0 || strcmp(id, "$GNRMC") == 0;
}

// Devuelve true si la línea es una sentencia GGA, sin importar el proveedor GNSS.
static bool is_gga(const char *id){
    return strcmp(id, "$GPGGA") == 0 || strcmp(id, "$GNGGA") == 0;
}

// Comprueba que la sentencia NMEA no haya sido alterada durante la transmisión.
static bool nmea_checksum_valid(const char *line){
    // La línea debe comenzar con '$' y contener un '*' seguido de dos dígitos hexadecimales.
    if (line == NULL || line[0] != '$') return false;

    // Busca el asterisco que precede al checksum.
    const char *star = strchr(line, '*');
    if (star == NULL || star[1] == '\0' || star[2] == '\0') return false;
    // Calcula el checksum XOR de todos los caracteres entre '$' y '*'.
    unsigned char checksum = 0;
    for (const char *p = line + 1; p < star; ++p) {
        /*
            Ejemplo con: $GNRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*74
            0x00 XOR 'G' = 0x47
            0x47 XOR 'N' = 0x09
            0x09 XOR 'R' = 0x5B
            ..
            0x0F XOR ',' = 0x23
            0x23 XOR 'W' = 0x74 Checksum calculado
        */
        checksum ^= (unsigned char)*p;
    }

    // Convierte los dos dígitos hexadecimales del checksum recibido a un valor numérico.
    unsigned int received = 0;
    // sscanf devuelve el número de elementos convertidos; debe ser 1 si la conversión fue exitosa.
    if (sscanf(star + 1, "%2x", &received) != 1) return false;

    return checksum == (unsigned char)received;
}

// Convierte coordenadas NMEA ddmm.mmmm/dddmm.mmmm a grados decimales.
static bool nmea_coordinate(const char *value, const char *hemisphere, float *degrees){
    if (value == NULL || hemisphere == NULL || degrees == NULL || value[0] == '\0') {
        return false;
    }

    char *end = NULL;
    double raw = strtod(value, &end);
    if (end == value || *end != '\0') {
        return false;
    }

    int whole_degrees = (int)(raw / 100.0);
    double minutes = raw - (whole_degrees * 100.0);
    if (minutes < 0.0 || minutes >= 60.0) {
        return false;
    }

    float result = (float)(whole_degrees + (minutes / 60.0));
    if (hemisphere[0] == 'S' || hemisphere[0] == 'W') {
        result = -result;
    } else if (hemisphere[0] != 'N' && hemisphere[0] != 'E') {
        return false;
    }

    *degrees = result;
    return true;
}

// Valida, separa y procesa una sentencia NMEA completa (RMC o GGA).
static void process_nmea_line(const char *line){
    if (!nmea_checksum_valid(line)) {
        ESP_LOGW(TAG, "Checksum NMEA invalido: %s", line);
        return;
    }

    // Copia la línea para poder modificarla y separar los campos.
    printf("%s\n", line);
    char sentence[GPS_LINE_SIZE];
    strlcpy(sentence, line, sizeof(sentence));
    char *star = strchr(sentence, '*');
    if (star != NULL) {
        *star = '\0';
    }
    // Separa la línea en campos usando la coma como delimitador.
    char *fields[20] = {0};
    size_t field_count = 0;
    fields[field_count++] = sentence;
    for (char *p = sentence; *p != '\0' && field_count < 20; ++p) {
        if (*p == ',') {
            *p = '\0';
            fields[field_count++] = p + 1;
        }
    }
    if (field_count == 0)  return;

    // Procesa RMC y GGA según corresponda.
    if (is_rmc(fields[0]) && field_count > 8) {
        // RMC puede indicar 'A' durante una transición; por eso GGA es importante.
        bool valid = fields[2][0] == 'A';

        gps_sample.valid_fix = false;

        float latitude = 0.0f, longitude = 0.0f;
        bool position_ok = nmea_coordinate(fields[3], fields[4], &latitude) &&  nmea_coordinate(fields[5], fields[6], &longitude);

        if (position_ok) {
            gps_sample.latitude_deg = latitude;
            gps_sample.longitude_deg = longitude;
        }

        gps_sample.valid_fix = valid && position_ok;
        if (gps_sample.valid_fix) {
            last_rmc_us = esp_timer_get_time();
            gps_sample.sample_time_us = last_rmc_us;
        }
        gps_sample.speed_kmh = strtof(fields[7], NULL) * 1.852f;
        gps_sample.course_deg = strtof(fields[8], NULL);
        ESP_LOGI(TAG, "RMC procesado: estado=%s", valid ? "A (valido)" : "V (sin fix)");
    } 
    else if (is_gga(fields[0]) && field_count > 8) {
        int fix_quality = atoi(fields[6]);
        gps_sample.satellites = (uint8_t)atoi(fields[7]);
        gps_sample.hdop = strtof(fields[8], NULL);

        // GGA confirma la calidad del fix y puede corregir el estado de RMC.
        if (fix_quality == 0) gps_sample.valid_fix = false;
        ESP_LOGI(TAG, "GGA procesado: fix=%d sat=%u HDOP=%.2f", fix_quality, gps_sample.satellites, gps_sample.hdop);
    }
}

// Calcula cuánto tiempo ha pasado desde la última sentencia RMC válida recibida.
static void gps_update_age(void){
    if (last_rmc_us == 0) {
        gps_sample.age_ms = UINT32_MAX;
        return;
    }

    uint64_t age_us = (uint64_t)esp_timer_get_time() - last_rmc_us;
    gps_sample.age_ms = (uint32_t)(age_us / 1000ULL);
    if (gps_sample.age_ms > GPS_MAX_AGE_MS) gps_sample.valid_fix = false;
}

// Publica cada segundo el estado actual y los datos GPS más recientes.
static void gps_log_sample(void){
    gps_update_age(); 
    const char *state = gps_sample.age_ms > GPS_MAX_AGE_MS ? "DATO_OBSOLETO" : (gps_sample.valid_fix ? "FIX_VALIDO" : "SIN_FIX");
    ESP_LOGI(TAG,"%s lat=%.6f lon=%.6f vel=%.2f km/h rumbo=%.2f sat=%u HDOP=%.2f edad=%" PRIu32 " ms",
             state, gps_sample.latitude_deg, gps_sample.longitude_deg, gps_sample.speed_kmh, gps_sample.course_deg, gps_sample.satellites, gps_sample.hdop, gps_sample.age_ms);
}

// Tarea principal: configura UART, arma líneas NMEA y dispara el reporte periódico.
static void gps_task(void *arg){
    const uart_config_t uart_config = {
        .baud_rate = GPS_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(GPS_UART_PORT, GPS_RX_BUFFER_SIZE, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(GPS_UART_PORT, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(GPS_UART_PORT, GPS_UART_TXD, GPS_UART_RXD, GPS_UART_RTS, GPS_UART_CTS));

    uint8_t buffer[128];
    char line[GPS_LINE_SIZE];
    size_t line_length = 0;
    TickType_t last_log = xTaskGetTickCount();

    ESP_LOGI(TAG, "GPS iniciado: UART%d, %d baudios, RX=%d", GPS_UART_PORT, GPS_UART_BAUD_RATE, GPS_UART_RXD);

    while (true) {
        int length = uart_read_bytes(GPS_UART_PORT, buffer, sizeof(buffer),  pdMS_TO_TICKS(60000));
        if (length > 0)  ESP_LOGI(TAG, "Bytes recibidos: %d", length);
        else  ESP_LOGI(TAG, "No llegaron Bytes");
        // Agrupa los bytes recibidos hasta formar una línea NMEA completa.
        for (int i = 0; i < length; ++i) {
            char c = (char)buffer[i];
            if (c == '\n') {
                if (line_length > 0) {
                    line[line_length] = '\0';
                    if (line[line_length - 1] == '\r') line[line_length - 1] = '\0';
                    ESP_LOGD(TAG, "NMEA: %s", line);
                    process_nmea_line(line);
                }
                line_length = 0;
            } 
            else if (line_length < sizeof(line) - 1 && c != '\r') {
                line[line_length++] = c;
            }
            else if (line_length >= sizeof(line) - 1) {
                ESP_LOGW(TAG, "Sentencia NMEA demasiado larga; se descarta");
                line_length = 0;
            }
        }

        if (xTaskGetTickCount() - last_log >= pdMS_TO_TICKS(1000)) {
            gps_log_sample();
            last_log = xTaskGetTickCount();
        }
    }
}

// Punto de entrada: limpia el estado y arranca la tarea del GPS.
void app_main(void){
    memset(&gps_sample, 0, sizeof(gps_sample));
    gps_sample.age_ms = UINT32_MAX;
    xTaskCreate(gps_task, "gps_uart_task", GPS_TASK_STACK, NULL, 10, NULL);
}
