#include "gps_config.h"
#include "gps_device.h"

#include "driver/uart.h"
#include "esp_err.h"
#include "freertos/queue.h"

static QueueHandle_t uart_event_queue;
static gps_device_stats_t stats;
static size_t line_length;
static bool dropping_line;

// Configura UART del GPS y crea la cola donde ESP-IDF informa errores de recepción.
void gps_device_init(void){
    const uart_config_t uart_config = {
        .baud_rate = GPS_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install( GPS_UART_PORT, GPS_RX_BUFFER_SIZE, 0, 10, &uart_event_queue, 0));
    ESP_ERROR_CHECK(uart_param_config(GPS_UART_PORT, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(  GPS_UART_PORT, GPS_UART_TXD, GPS_UART_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}

// Atiende desbordamientos y errores UART; al desbordarse, descarta la trama incompleta.
static void poll_uart_events(void){
    uart_event_t event;
    while (uart_event_queue != NULL && xQueueReceive(uart_event_queue, &event, 0) == pdTRUE) {
        switch (event.type) {
            case UART_FIFO_OVF:
                ++stats.uart_overflow;
                uart_flush_input(GPS_UART_PORT);
                line_length = 0;
                dropping_line = false;
                xQueueReset(uart_event_queue);
                break;
            case UART_BUFFER_FULL:
                ++stats.uart_buffer_full;
                uart_flush_input(GPS_UART_PORT);
                line_length = 0;
                dropping_line = false;
                xQueueReset(uart_event_queue);
                break;
            case UART_FRAME_ERR:
                ++stats.uart_frame_error;
                break;
            case UART_PARITY_ERR:
                ++stats.uart_parity_error;
                break;
            default:
                break;
        }
    }
}

// Lee bytes hasta completar una línea NMEA terminada en LF; descarta líneas demasiado largas.
bool gps_device_read_line(char *line, size_t line_size, TickType_t timeout){
    uint8_t byte = 0;
    poll_uart_events();
    if (line == NULL || line_size < 2) return false;

    int received = uart_read_bytes(GPS_UART_PORT, &byte, 1, timeout);
    if (received != 1) return false;

    if (byte == '\n') {
        if (dropping_line) {
            dropping_line = false;
            line_length = 0;
            return false;
        }
        if (line_length == 0) return false;

        line[line_length] = '\0';
        line_length = 0;
        return true;
    }
    if (dropping_line || byte == '\r') return false;

    if (line_length >= line_size - 1) {
        ++stats.overlong_lines;
        line_length = 0;
        dropping_line = true;
        return false;
    }
    line[line_length++] = (char)byte;
    return false;
}

// Copia los contadores acumulados de errores para que la aplicación los registre.
void gps_device_get_stats(gps_device_stats_t *output){
    if (output != NULL)  *output = stats;
}
