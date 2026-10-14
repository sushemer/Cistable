#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/uart.h"

#define RS485_UART_PORT      UART_NUM_2
#define RS485_TX_PIN         17  // Conectar a TXD del módulo
#define RS485_RX_PIN         18  // Conectar a RXD del módulo

static const char *TAG = "QDY30A_RS485";

uint16_t modbus_crc16(const uint8_t *buf, uint16_t len) {
    uint16_t crc = 0xFFFF;
    for (int pos = 0; pos < len; pos++) {
        crc ^= (uint16_t)buf[pos];
        for (int i = 8; i != 0; i--) {
            if ((crc & 0x0001) != 0) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

void init_rs485(void) {
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    ESP_ERROR_CHECK(uart_param_config(RS485_UART_PORT, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(RS485_UART_PORT, RS485_TX_PIN, RS485_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(RS485_UART_PORT, 256, 256, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_set_mode(RS485_UART_PORT, UART_MODE_UART));
}

void read_sensor_task(void *pvParameters) {
    uint8_t rx_buffer[128];
    
    // Trama Modbus RTU para solicitar 10 registros desde el 0x0000:
    // [0x01]: ID | [0x03]: Leer | [0x00, 0x00]: Registro 0 | [0x00, 0x0A]: Pedir 10 registros
    uint8_t request[8] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00};
    
    uint16_t crc = modbus_crc16(request, 6);
    request[6] = crc & 0xFF;        // CRC Low
    request[7] = (crc >> 8) & 0xFF; // CRC High

    while (1) {
        uart_flush_input(RS485_UART_PORT);
        uart_write_bytes(RS485_UART_PORT, (const char *)request, sizeof(request));

        // Esperar respuesta (esperamos 3 bytes header + 20 bytes datos + 2 bytes CRC = 25 bytes)
        int len = uart_read_bytes(RS485_UART_PORT, rx_buffer, sizeof(rx_buffer), pdMS_TO_TICKS(300));

        if (len >= 25) {
            uint16_t rx_crc = (rx_buffer[len - 1] << 8) | rx_buffer[len - 2];
            uint16_t calc_crc = modbus_crc16(rx_buffer, len - 2);

            if (rx_crc == calc_crc && rx_buffer[0] == 0x01 && rx_buffer[1] == 0x03) {
                ESP_LOGI(TAG, "=== LECTURA DE REGISTROS ===");
                
                // Parsear los 10 registros recibidos (cada registro son 2 bytes)
                for (int i = 0; i < 10; i++) {
                    uint16_t reg_val = (rx_buffer[3 + (i * 2)] << 8) | rx_buffer[4 + (i * 2)];
                    ESP_LOGI(TAG, "Registro [0x%04X]: %d", i, reg_val);
                }
            } else {
                ESP_LOGW(TAG, "Error de suma CRC en la respuesta");
            }
        } else {
            ESP_LOGE(TAG, "Respuesta incompleta o sin respuesta (%d bytes)", len);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void) {
    init_rs485();
    xTaskCreate(read_sensor_task, "read_sensor_task", 4096, NULL, 5, NULL);
}