#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/uart.h"

#define RS485_UART_PORT   UART_NUM_2
#define RS485_TX_PIN      17
#define RS485_RX_PIN      16
#define RS485_RTS_PIN     4   // Connect to DE/RE pins of MAX485 board

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
    
    // Assign TX, RX, and RTS (used for DE/RE driver enable)
    ESP_ERROR_CHECK(uart_set_pin(RS485_UART_PORT, RS485_TX_PIN, RS485_RX_PIN, RS485_RTS_PIN, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(RS485_UART_PORT, 256, 256, 0, NULL, 0));
    
    // Configure hardware half-duplex RS485 mode
    ESP_ERROR_CHECK(uart_set_mode(RS485_UART_PORT, UART_MODE_RS485_HALF_DUPLEX));
}

esp_err_t read_sensor(uint16_t *value) {
    if (value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t rx_buffer[64];
    uint8_t request[8] = {0x01, 0x03, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00};
    
    uint16_t crc = modbus_crc16(request, 6);
    request[6] = crc & 0xFF;
    request[7] = (crc >> 8) & 0xFF;

    uart_flush_input(RS485_UART_PORT);
    uart_write_bytes(RS485_UART_PORT, (const char *)request, sizeof(request));

    int len = uart_read_bytes(RS485_UART_PORT, rx_buffer, sizeof(rx_buffer), pdMS_TO_TICKS(300));

    if (len >= 7) {
        uint16_t rx_crc = (rx_buffer[6] << 8) | rx_buffer[5];
        uint16_t calc_crc = modbus_crc16(rx_buffer, 5);

        if (rx_crc == calc_crc && rx_buffer[0] == 0x01 && rx_buffer[1] == 0x03) {
            *value = (rx_buffer[3] << 8) | rx_buffer[4];
            return ESP_OK;
        } else {
            ESP_LOGW(TAG, "CRC error or unexpected Modbus response header");
            return ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "Timeout or incomplete response (%d bytes received)", len);
        return ESP_ERR_TIMEOUT;
    }
}

void sensor_task(void *pvParameters) {
    uint16_t valor = 0;

    while (1) {
        esp_err_t ret = read_sensor(&valor);
        
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Read successful - Value: %d", valor);
        } else {
            ESP_LOGE(TAG, "Sensor read error: %s", esp_err_to_name(ret));
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void) {
    init_rs485();
    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);
}