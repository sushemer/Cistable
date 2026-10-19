#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"

// Contadores acumulados de errores y descartes durante la recepción UART.
typedef struct {
    uint32_t uart_overflow;
    uint32_t uart_buffer_full;
    uint32_t uart_frame_error;
    uint32_t uart_parity_error;
    uint32_t overlong_lines;
} gps_device_stats_t;

// Instala y configura la UART y la cola de eventos del GPS.
void gps_device_init(void);
// Lee una línea NMEA completa; false indica que aún no hay una línea lista.
bool gps_device_read_line(char *line, size_t line_size, TickType_t timeout);
// Copia los contadores actuales de errores de UART al destino indicado.
void gps_device_get_stats(gps_device_stats_t *stats);
