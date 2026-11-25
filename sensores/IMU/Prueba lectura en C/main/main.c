#include <stdio.h>
#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

// Cabeceras oficiales de la librería CEVA SH2
#include "sh2.h"
#include "sh2_err.h"
#include "sh2_SensorValue.h"
#include "euler.h"

#define TAG "IMU_LTR_BNO085"

// ============================================================================
// CONFIGURACIÓN DE PINES DE HARDWARE (SPI & GPIO)
// ============================================================================
// Bus SPI2_HOST compartido
#define SPI_HOST_ID     SPI2_HOST
#define PIN_NUM_MISO    GPIO_NUM_13
#define PIN_NUM_MOSI    GPIO_NUM_11
#define PIN_NUM_CLK     GPIO_NUM_12

// Sensor 1: Cabina
#define PIN_NUM_CS1     GPIO_NUM_10
#define PIN_NUM_HINT1   GPIO_NUM_4
#define PIN_NUM_RST1    GPIO_NUM_5

// Sensor 2: Tanque Izquierdo
#define PIN_NUM_CS2     GPIO_NUM_9
#define PIN_NUM_HINT2   GPIO_NUM_6
#define PIN_NUM_RST2    GPIO_NUM_7

// Sensor 3: Tanque Derecho
#define PIN_NUM_CS3     GPIO_NUM_14
#define PIN_NUM_HINT3   GPIO_NUM_15
#define PIN_NUM_RST3    GPIO_NUM_16

// ============================================================================
// PARÁMETROS FÍSICOS DEL CAMIÓN Y ALGORITMO LTR
// ============================================================================
#define TRUCK_H             1.8f    // Altura del centro de gravedad (m)
#define TRUCK_T             2.2f    // Ancho de vía (m)
#define GRAVITY_G           9.81f   // Aceleración de la gravedad (m/s^2)
#define LTR_ALERT_THRESHOLD 0.60f   // Umbral de alerta por riesgo de volcadura

#define NUM_SENSORS         3

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// Estructura para almacenar los datos de cada sensor BNO085
typedef struct {
    uint8_t id;
    const char *name;
    gpio_num_t cs_pin;
    gpio_num_t hint_pin;
    gpio_num_t rst_pin;
    spi_device_handle_t spi_dev;
    SemaphoreHandle_t hint_sem;

    // Datos medidos del sensor
    float roll_rad;
    float roll_deg;
    float pitch_rad;
    float pitch_deg;
    float yaw_rad;
    float yaw_deg;
    float ay; // Aceleración lateral lineal (m/s^2)
} bno085_dev_t;

static bno085_dev_t g_sensors[NUM_SENSORS];

// Variables globales del multiplexor HAL SPI
static sh2_Hal_t g_hal;
static int g_current_rx_sensor_idx = 0; // Sensor que está entregando datos por SPI actualmente
static int g_active_tx_sensor_idx  = 0; // Sensor destino para transmisión de comandos
static int g_last_read_sensor      = 0; // Índice de rotación de lectura SPI

// ============================================================================
// IMPLEMENTACIÓN DE LA CAPA HAL MULTIPLEXADA (sh2_hal_t) PARA 3 SENSORES BNO085
// ============================================================================

static int hal_open(sh2_Hal_t *self)
{
    (void)self;
    // Reset por hardware de los 3 sensores BNO085
    for (int i = 0; i < NUM_SENSORS; i++) {
        gpio_set_level(g_sensors[i].rst_pin, 0);
    }
    vTaskDelay(pdMS_TO_TICKS(20));

    for (int i = 0; i < NUM_SENSORS; i++) {
        gpio_set_level(g_sensors[i].rst_pin, 1);
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    return SH2_OK;
}

static void hal_close(sh2_Hal_t *self)
{
    (void)self;
    for (int i = 0; i < NUM_SENSORS; i++) {
        gpio_set_level(g_sensors[i].rst_pin, 0); // Poner los 3 sensores en reset
    }
}

static int hal_read(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len, uint32_t *t_us)
{
    (void)self;
    if (!pBuffer) return 0;

    // Polleo Round-Robin de los 3 sensores buscando uno con HINT en bajo (datos listos)
    for (int i = 0; i < NUM_SENSORS; i++) {
        int sensor_idx = (g_last_read_sensor + 1 + i) % NUM_SENSORS;

        if (gpio_get_level(g_sensors[sensor_idx].hint_pin) == 0) {
            g_last_read_sensor = sensor_idx;
            g_current_rx_sensor_idx = sensor_idx; // Registrar sensor origen

            // Paso 1: Leer encabezado SHTP de 4 bytes
            uint8_t header[4] = {0};
            spi_transaction_t t_hdr;
            memset(&t_hdr, 0, sizeof(t_hdr));
            t_hdr.length = 32; // 4 bytes * 8 bits
            t_hdr.tx_buffer = NULL;
            t_hdr.rx_buffer = header;

            gpio_set_level(g_sensors[sensor_idx].cs_pin, 0);
            esp_err_t ret = spi_device_polling_transmit(g_sensors[sensor_idx].spi_dev, &t_hdr);

            if (ret != ESP_OK) {
                gpio_set_level(g_sensors[sensor_idx].cs_pin, 1);
                return 0;
            }

            // Longitud de paquete SHTP de 16 bits (limpiando bit de continuación 0x8000)
            uint16_t packet_len = (uint16_t)(header[0] | (header[1] << 8)) & ~0x8000;

            if (packet_len < 4 || packet_len > len || packet_len > SH2_HAL_MAX_TRANSFER_IN) {
                gpio_set_level(g_sensors[sensor_idx].cs_pin, 1);
                return 0;
            }

            memcpy(pBuffer, header, 4);

            // Paso 2: Leer payload remanente si packet_len > 4
            if (packet_len > 4) {
                spi_transaction_t t_body;
                memset(&t_body, 0, sizeof(t_body));
                t_body.length = (packet_len - 4) * 8;
                t_body.tx_buffer = NULL;
                t_body.rx_buffer = pBuffer + 4;

                ret = spi_device_polling_transmit(g_sensors[sensor_idx].spi_dev, &t_body);
            }

            gpio_set_level(g_sensors[sensor_idx].cs_pin, 1);

            if (ret != ESP_OK) {
                return 0;
            }

            if (t_us) {
                *t_us = (uint32_t)esp_timer_get_time();
            }

            return packet_len;
        }
    }

    return 0; // Ningún sensor tiene datos listos
}

static int hal_write(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len)
{
    (void)self;
    if (len == 0 || !pBuffer) return 0;

    int sensor_idx = g_active_tx_sensor_idx;

    // Esperar a que el pin HINT del sensor destino baje (listo para recibir)
    int retries = 50;
    while (gpio_get_level(g_sensors[sensor_idx].hint_pin) != 0 && retries > 0) {
        vTaskDelay(pdMS_TO_TICKS(1));
        retries--;
    }

    if (gpio_get_level(g_sensors[sensor_idx].hint_pin) != 0) {
        return 0; // Sensor no listo para recibir comando
    }

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = len * 8;
    t.tx_buffer = pBuffer;
    t.rx_buffer = NULL;

    gpio_set_level(g_sensors[sensor_idx].cs_pin, 0);
    esp_err_t ret = spi_device_polling_transmit(g_sensors[sensor_idx].spi_dev, &t);
    gpio_set_level(g_sensors[sensor_idx].cs_pin, 1);

    if (ret != ESP_OK) {
        return 0;
    }

    return len;
}

static uint32_t hal_get_time_us(sh2_Hal_t *self)
{
    (void)self;
    return (uint32_t)esp_timer_get_time();
}

// ISR para interrupciones en el pin HINT
static void IRAM_ATTR gpio_hint_isr_handler(void *arg)
{
    bno085_dev_t *dev = (bno085_dev_t *)arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (dev && dev->hint_sem) {
        xSemaphoreGiveFromISR(dev->hint_sem, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

// ============================================================================
// CALLBACKS SH2
// ============================================================================

static void sh2_async_event_cb(void *cookie, sh2_AsyncEvent_t *pEvent)
{
    (void)cookie;
    if (pEvent && pEvent->eventId == SH2_RESET) {
        ESP_LOGI(TAG, "Notificación de RESET recibida en SH2.");
    }
}

static void sensor_callback(void *cookie, sh2_SensorEvent_t *pEvent)
{
    (void)cookie;
    if (!pEvent) return;

    // Asignar el evento recibido al sensor que transmitió el paquete SPI
    int sensor_idx = g_current_rx_sensor_idx;
    if (sensor_idx < 0 || sensor_idx >= NUM_SENSORS) return;

    bno085_dev_t *dev = &g_sensors[sensor_idx];

    sh2_SensorValue_t value;
    int rc = sh2_decodeSensorEvent(&value, pEvent);
    if (rc != SH2_OK) return;

    switch (value.sensorId) {
        case SH2_GAME_ROTATION_VECTOR: {
            float r = value.un.gameRotationVector.real;
            float i = value.un.gameRotationVector.i;
            float j = value.un.gameRotationVector.j;
            float k = value.un.gameRotationVector.k;

            float roll_rad, pitch_rad, yaw_rad;
            q_to_ypr(r, i, j, k, &roll_rad, &pitch_rad, &yaw_rad);

            dev->roll_rad  = roll_rad;
            dev->pitch_rad = pitch_rad;
            dev->yaw_rad   = yaw_rad;

            dev->roll_deg  = roll_rad  * (180.0f / M_PI);
            dev->pitch_deg = pitch_rad * (180.0f / M_PI);
            dev->yaw_deg   = yaw_rad   * (180.0f / M_PI);
            break;
        }
        case SH2_LINEAR_ACCELERATION: {
            // Extraer componente de aceleración lateral (m/s^2)
            dev->ay = value.un.linearAcceleration.y;
            break;
        }
        default:
            break;
    }
}

// ============================================================================
// INICIALIZACIÓN DE PERIFÉRICOS SPI Y SENSORES
// ============================================================================

static esp_err_t init_spi_bus(void)
{
    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_NUM_MISO,
        .mosi_io_num = PIN_NUM_MOSI,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = SH2_HAL_MAX_TRANSFER_IN,
    };

    return spi_bus_initialize(SPI_HOST_ID, &buscfg, SPI_DMA_CH_AUTO);
}

static esp_err_t init_sensor_device(bno085_dev_t *dev)
{
    // 1. Configurar GPIO de Chip Select (CS) y Reset (RST)
    gpio_config_t io_conf_out = {
        .pin_bit_mask = (1ULL << dev->cs_pin) | (1ULL << dev->rst_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf_out);
    gpio_set_level(dev->cs_pin, 1);
    gpio_set_level(dev->rst_pin, 1);

    // 2. Configurar GPIO de Interrupción (HINT)
    dev->hint_sem = xSemaphoreCreateBinary();
    gpio_config_t io_conf_in = {
        .pin_bit_mask = (1ULL << dev->hint_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&io_conf_in);
    gpio_isr_handler_add(dev->hint_pin, gpio_hint_isr_handler, (void *)dev);

    // 3. Añadir dispositivo al bus SPI maestro
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 3 * 1000 * 1000, // 3 MHz SPI Clock para BNO085
        .mode = 3,                          // Modo SPI 3 (CPOL=1, CPHA=1)
        .spics_io_num = -1,                 // Manejado manualmente mediante CS pin GPIO
        .queue_size = 7,
    };

    return spi_bus_add_device(SPI_HOST_ID, &devcfg, &dev->spi_dev);
}

static void setup_sensors_config(void)
{
    g_sensors[0] = (bno085_dev_t){
        .id = 0, .name = "Cabina",
        .cs_pin = PIN_NUM_CS1, .hint_pin = PIN_NUM_HINT1, .rst_pin = PIN_NUM_RST1
    };
    g_sensors[1] = (bno085_dev_t){
        .id = 1, .name = "Tanque Izq",
        .cs_pin = PIN_NUM_CS2, .hint_pin = PIN_NUM_HINT2, .rst_pin = PIN_NUM_RST2
    };
    g_sensors[2] = (bno085_dev_t){
        .id = 2, .name = "Tanque Der",
        .cs_pin = PIN_NUM_CS3, .hint_pin = PIN_NUM_HINT3, .rst_pin = PIN_NUM_RST3
    };

    gpio_install_isr_service(0);

    for (int i = 0; i < NUM_SENSORS; i++) {
        ESP_LOGI(TAG, "Inicializando Hardware SPI/GPIO para Sensor %d (%s)...", i + 1, g_sensors[i].name);
        ESP_ERROR_CHECK(init_sensor_device(&g_sensors[i]));
    }

    // Configurar HAL Multiplexada
    g_hal.open = hal_open;
    g_hal.close = hal_close;
    g_hal.read = hal_read;
    g_hal.write = hal_write;
    g_hal.getTimeUs = hal_get_time_us;

    // Abrir sesión SH2 con la HAL multiplexada
    int status = sh2_open(&g_hal, sh2_async_event_cb, NULL);
    if (status != SH2_OK) {
        ESP_LOGE(TAG, "Error abriendo sesión SH2: %d", status);
        return;
    }

    sh2_setSensorCallback(sensor_callback, NULL);

    // Configuración de reportes a 100 Hz (10,000 microsegundos)
    sh2_SensorConfig_t config = {
        .changeSensitivityEnabled = false,
        .changeSensitivityRelative = false,
        .wakeupEnabled = false,
        .alwaysOnEnabled = false,
        .changeSensitivity = 0,
        .reportInterval_us = 10000, // 100 Hz
        .batchInterval_us = 0,
        .sensorSpecific = 0
    };

    // Configurar reportes para cada sensor individualmente seleccionando g_active_tx_sensor_idx
    for (int i = 0; i < NUM_SENSORS; i++) {
        g_active_tx_sensor_idx = i;
        sh2_setSensorConfig(SH2_GAME_ROTATION_VECTOR, &config);
        sh2_setSensorConfig(SH2_LINEAR_ACCELERATION, &config);
        ESP_LOGI(TAG, "Sensor %d (%s) configurado a 100 Hz (Game Rotation Vector & Linear Accel).", i + 1, g_sensors[i].name);
    }
}

// ============================================================================
// TAREAS FREERTOS: LTR CALCULATOR & UART TARE LISTENER
// ============================================================================

// Tarea principal para el cálculo continuo del algoritmo LTR
void ltr_processing_task(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(10); // Ejecución a 100 Hz

    ESP_LOGI(TAG, "Iniciando procesador de eventos LTR a 100 Hz...");

    // Encabezado CSV
    printf("Roll_Cabina,Roll_TanqueIzq,Roll_TanqueDer,Ay_Promedio,LTR_Calculado\n");

    while (1) {
        // Atender servicios de la sesión SH2 (procesa lecturas SPI de los 3 sensores)
        sh2_service();

        // Obtener ángulos Roll (en radianes) y Aceleración Lateral Ay (m/s^2)
        float roll1 = g_sensors[0].roll_rad;
        float roll2 = g_sensors[1].roll_rad;
        float roll3 = g_sensors[2].roll_rad;

        float ay1 = g_sensors[0].ay;
        float ay2 = g_sensors[1].ay;
        float ay3 = g_sensors[2].ay;

        // Promedios
        float roll_avg = (roll1 + roll2 + roll3) / 3.0f;
        float ay_avg   = (ay1 + ay2 + ay3) / 3.0f;

        // Cálculo de Aceleración Lateral Efectiva
        float ay_efectiva = ay_avg * cosf(roll_avg) + GRAVITY_G * sinf(roll_avg);

        // Cálculo del Índice LTR (Load Transfer Ratio)
        float ltr = (2.0f * TRUCK_H * ay_efectiva) / (TRUCK_T * GRAVITY_G);

        // Limitar (Clamp) LTR entre -1.0 y 1.0
        if (ltr > 1.0f) ltr = 1.0f;
        if (ltr < -1.0f) ltr = -1.0f;

        // Salida formateada en CSV continuo
        printf("%.2f, %.2f, %.2f, %.3f, %.3f\n",
               g_sensors[0].roll_deg,
               g_sensors[1].roll_deg,
               g_sensors[2].roll_deg,
               ay_avg,
               ltr);

        // Alerta preventiva en consola si |LTR| > 0.60
        if (fabsf(ltr) > LTR_ALERT_THRESHOLD) {
            ESP_LOGW(TAG, "¡ALERTA DE RIESGO DE VOLCADURA! LTR = %.3f (Excede el límite |0.60|)", ltr);
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// Tarea para calibración a cero (Tare Now) al recibir 't' por UART
void uart_tare_task(void *pvParameters)
{
    (void)pvParameters;
    int c;
    while (1) {
        c = getchar();
        if (c == 't' || c == 'T') {
            ESP_LOGI(TAG, "Comando 't' detectado. Ejecutando Tare Now (Calibración a Cero) en 3 IMUs...");
            for (int i = 0; i < NUM_SENSORS; i++) {
                g_active_tx_sensor_idx = i;
                int res = sh2_setTareNow(SH2_TARE_X | SH2_TARE_Y | SH2_TARE_Z, SH2_TARE_BASIS_GAMING_ROTATION_VECTOR);
                if (res == SH2_OK) {
                    ESP_LOGI(TAG, "Tare Now completado exitosamente en Sensor %d (%s)", i + 1, g_sensors[i].name);
                } else {
                    ESP_LOGE(TAG, "Error ejecutando Tare Now en Sensor %d (%s), código: %d", i + 1, g_sensors[i].name, res);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ============================================================================
// PUNTO DE ENTRADA PRINCIPAL (app_main)
// ============================================================================
void app_main(void)
{
    ESP_LOGI(TAG, "Inicializando Sistema de Monitoreo LTR con 3 IMUs BNO085 (SPI / ESP-IDF)...");

    // 1. Inicializar Bus SPI Maestro
    ESP_ERROR_CHECK(init_spi_bus());

    // 2. Configurar e Inicializar Sensores BNO085 y Capa HAL Multiplexada
    setup_sensors_config();

    // 3. Crear Tareas FreeRTOS
    xTaskCreatePinnedToCore(ltr_processing_task, "ltr_task", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(uart_tare_task, "tare_task", 3072, NULL, 2, NULL, 0);
}
