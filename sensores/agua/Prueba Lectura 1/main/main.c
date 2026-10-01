#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "QDY30A_RS485";

// Configuracion de Hardware (Pines ESP32-S3 -> MAX3485)
#define PAT_UART_PORT      UART_NUM_2
#define TXD_PIN            GPIO_NUM_17 // Conectado a DI del MAX3485
#define RXD_PIN            GPIO_NUM_16 // Conectado a RO del MAX3485
#define RTS_PIN            GPIO_NUM_18 // Conectado a los pines DE y RE unidos del MAX3485
#define CTS_PIN            UART_PIN_NO_CHANGE

#define BAUDRATE           9600
#define BUF_SIZE           256

/**
 * @brief Inicializa el puerto UART en modo RS485 Half-Duplex.
 */
static void init_rs485(void)
{
    const uart_config_t uart_config = {
        .baud_rate = BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // 1. Instalar el driver de UART (Buffer RX, Buffer TX, Event Queue, etc.)
    ESP_ERROR_CHECK(uart_driver_install(PAT_UART_PORT, BUF_SIZE * 2, 0, 0, NULL, 0));

    // 2. Configurar los parámetros de comunicación UART
    ESP_ERROR_CHECK(uart_param_config(PAT_UART_PORT, &uart_config));

    // 3. Asignar los pines GPIO para TX, RX y RTS (DE/RE)
    ESP_ERROR_CHECK(uart_set_pin(PAT_UART_PORT, TXD_PIN, RXD_PIN, RTS_PIN, CTS_PIN));

    // 4. Activar el modo hardware RS485 Half-Duplex
    // En este modo, el periférico UART del ESP32 conmuta automáticamente el pin RTS (DE/RE)
    // poniéndolo en HIGH durante la transmisión y en LOW durante la recepción.
    ESP_ERROR_CHECK(uart_set_mode(PAT_UART_PORT, UART_MODE_RS485_HALF_DUPLEX));

    // 5. Configurar timeout de recepción RX (ayuda a detectar fin de trama)
    ESP_ERROR_CHECK(uart_set_rx_timeout(PAT_UART_PORT, 3));

    ESP_LOGI(TAG, "UART2 configurado exitosamente en modo RS485 Half-Duplex (9600 Baudios)");
}

/**
 * @brief Tarea de FreeRTOS que realiza la lectura del sensor QDY30A-B cada 1000 ms.
 */
static void qdy30a_read_task(void *pvParameters)
{
    // Trama de solicitud Modbus RTU:
    // [0] Dirección del esclavo: 0x01
    // [1] Código de función: 0x03 (Read Holding Registers)
    // [2,3] Dirección inicial del registro: 0x0000
    // [4,5] Cantidad de registros a leer: 0x0001
    // [6,7] CRC16 Modbus (Low Byte, High Byte): 0x84, 0x0A
    const uint8_t request_frame[] = { 0x01, 0x03, 0x00, 0x00, 0x00, 0x01, 0x84, 0x0A };
    uint8_t rx_buffer[BUF_SIZE];

    while (1) {
        // Limpiar el buffer de entrada para descartar datos obsoletos o ruido eléctrico
        uart_flush_input(PAT_UART_PORT);

        // Enviar trama de solicitud Modbus RTU por RS485
        int bytes_sent = uart_write_bytes(PAT_UART_PORT, (const char *)request_frame, sizeof(request_frame));
        ESP_LOGI(TAG, "Solicitud de lectura enviada (%d bytes)", bytes_sent);

        // Esperar la respuesta del sensor (Timeout de 1000 ms)
        int rx_bytes = uart_read_bytes(PAT_UART_PORT, rx_buffer, BUF_SIZE - 1, pdMS_TO_TICKS(1000));

        if (rx_bytes > 0) {
            // Imprimir la trama recibida en formato Hexadecimal para depuración física y lógica
            char hex_str[BUF_SIZE * 3 + 1] = {0};
            for (int i = 0; i < rx_bytes; i++) {
                sprintf(hex_str + strlen(hex_str), "%02X ", rx_buffer[i]);
            }
            ESP_LOGI(TAG, "RX (%d bytes): %s", rx_bytes, hex_str);

            // Respuesta Modbus RTU esperada para 1 registro (típicamente 7 bytes):
            // [0] Esclavo (0x01)
            // [1] Función (0x03)
            // [2] Conteo de bytes de datos (0x02)
            // [3] Data High
            // [4] Data Low
            // [5] CRC Low
            // [6] CRC High
            if (rx_bytes >= 5 && rx_buffer[0] == 0x01 && rx_buffer[1] == 0x03) {
                // Extraer Data High (índice 3) y Data Low (índice 4) y combinarlos en 16 bits
                uint16_t raw_level = (rx_buffer[3] << 8) | rx_buffer[4];
                ESP_LOGI(TAG, "Valor crudo del sensor (16-bit): %u (0x%04X)", raw_level, raw_level);

                /*
                 * =========================================================================
                 * GUÍA DE INTERPRETACIÓN DEL VALOR CRUDO (Sensor QDY30A-B)
                 * =========================================================================
                 * El transductor QDY30A-B entrega la medición de nivel como un entero sin
                 * signo de 16 bits, representando el nivel de agua en unidades físicas según
                 * la calibración de fábrica.
                 * 
                 * Ejemplos comunes de conversión a metros de agua (mH2O):
                 * 
                 * 1. Sensor con escala de resolución en milímetros (mm) [Rango 0 - 5 Metros]:
                 *    - Rango devuelto por el sensor: 0 a 5000 (0x0000 a 0x1388).
                 *    - Fórmula: Nivel_metros = (float)raw_level / 1000.0f;
                 *    - Ejemplo: raw_level = 2450  => 2.45 metros de profundidad.
                 * 
                 * 2. Sensor con escala de resolución en centímetros (cm) o con 1 decimal:
                 *    - Rango devuelto por el sensor: 0 a 500 (0x0000 a 0x01F4).
                 *    - Fórmula: Nivel_metros = (float)raw_level / 100.0f;
                 *    - Ejemplo: raw_level = 245   => 2.45 metros de profundidad.
                 * 
                 * 3. Sensores con rango nominal diferente (ej. 0-1m, 0-10m):
                 *    - Se mantiene el factor de división (generalmente 1000 o 100) según
                 *      el datasheet del fabricante especifico.
                 * =========================================================================
                 */
            } else {
                ESP_LOGW(TAG, "Trama de respuesta invalida o incompleta (Esperado Esclavo=0x01, Func=0x03)");
            }
        } else {
            ESP_LOGE(TAG, "Timeout: No se recibio respuesta del sensor QDY30A-B via RS485");
        }

        // Frecuencia de muestreo: 1 segundo (1000 ms)
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "--- Iniciando prueba física de lectura RS485 QDY30A-B ---");

    // Inicializar el puerto RS485 con control de flujo automático DE/RE por RTS
    init_rs485();

    // Crear la tarea FreeRTOS de lectura
    xTaskCreate(qdy30a_read_task, "qdy30a_read_task", 4096, NULL, 5, NULL);
}
