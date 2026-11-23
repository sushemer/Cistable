/**
 * @file main.cpp
 * @brief Lectura, Calibración (Tare) y Algoritmo LTR con una IMU GY-BNO085 (SPI / C++) en ESP32
 * @details Este programa inicializa una IMU BNO085 por SPI en un ESP32,
 *          obtiene el cuaternión (Game Rotation Vector -> Roll/Pitch) y la aceleración lineal (Linear Acceleration -> Ay),
 *          calcula el índice de riesgo de volcadura (Load Transfer Ratio - LTR) para un camión y permite ejecutar
 *          la calibración a cero (Tare) mediante el comando serie 't'.
 */

#include <stdio.h>
#include <cmath>
#include <algorithm>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/uart.h"
#include "BNO08x.hpp"

#include <cstring>
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/uart.h"
#include "BNO08x.hpp"

#define BNO_SPI_HOST SPI2_HOST
#define PIN_SPI_MOSI GPIO_NUM_18
#define PIN_SPI_MISO GPIO_NUM_16
#define PIN_SPI_SCK GPIO_NUM_15
#define SPI_CLOCK_SPEED_HZ 3000000UL
#define PIN_IMU1_CS GPIO_NUM_10
#define PIN_IMU1_HINT GPIO_NUM_11
#define PIN_IMU1_RST GPIO_NUM_12

#define RS485_UART UART_NUM_1
#define PIN_RS485_TX GPIO_NUM_17
#define PIN_RS485_RX GPIO_NUM_13
#define PIN_RS485_RTS GPIO_NUM_14
#define RS485_BAUD_RATE 9600
#define MODBUS_SLAVE_ADDRESS 1
#define MODBUS_LEVEL_REGISTER 0x0000
#define MODBUS_POLL_PERIOD_MS 200
#define REPORT_INTERVAL_US 10000UL
#define ESPNOW_CHANNEL 1

static const char* TAG = "TANQUE";
static const uint8_t CABIN_MAC[ESP_NOW_ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

typedef struct __attribute__((packed)) TankPayload {
    float roll_1;
    float pitch_1;
    float ay_1;
    float water_level;
    uint8_t sensor_status_flags;
} TankPayload;

static_assert(sizeof(TankPayload) == 17U, "TankPayload must be binary-compatible with the C cabin node");

enum : uint8_t {
    STATUS_IMU1_OK = 1U << 0,
    STATUS_RS485_OK = 1U << 1
};

struct IMUState {
    float roll = 0.0f;
    float pitch = 0.0f;
    float ay = 0.0f;
    bool initialized = false;
    bool valid = false;
    int64_t last_update_us = 0;
};

static BNO08x* imu1 = nullptr;
static IMUState imu_state;
static portMUX_TYPE state_mux = portMUX_INITIALIZER_UNLOCKED;
static float water_level = 0.0f;
static bool rs485_valid = false;

static void esp_now_send_callback(const esp_now_send_info_t*, esp_now_send_status_t status)
{
    static uint32_t sent_count = 0;
    ++sent_count;
    if (status == ESP_NOW_SEND_SUCCESS) {
        if ((sent_count % 20U) == 0U) {
            ESP_LOGI(TAG, "ESP-NOW: paquete transmitido por radio (%lu)", static_cast<unsigned long>(sent_count));
        }
    } else {
        ESP_LOGW(TAG, "ESP-NOW: fallo de transmision por radio; revisar canal y configuracion");
    }
}

static uint16_t modbus_crc16(const uint8_t* data, size_t length)
{
    uint16_t crc = 0xffff;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 1U) ? static_cast<uint16_t>((crc >> 1) ^ 0xa001U) : static_cast<uint16_t>(crc >> 1);
        }
    }
    return crc;
}

static bool read_modbus_level(float& level)
{
    const uint8_t request[] = {MODBUS_SLAVE_ADDRESS, 0x03, static_cast<uint8_t>(MODBUS_LEVEL_REGISTER >> 8),
                                static_cast<uint8_t>(MODBUS_LEVEL_REGISTER), 0x00, 0x01};
    uint8_t request_frame[sizeof(request) + 2] = {};
    const uint16_t request_crc = modbus_crc16(request, sizeof(request));
    std::memcpy(request_frame, request, sizeof(request));
    request_frame[6] = static_cast<uint8_t>(request_crc);
    request_frame[7] = static_cast<uint8_t>(request_crc >> 8);

    uart_flush_input(RS485_UART);
    if (uart_write_bytes(RS485_UART, request_frame, sizeof(request_frame)) != sizeof(request_frame)) {
        return false;
    }

    uint8_t frame[8] = {};
    const int64_t deadline = esp_timer_get_time() + 120000;
    size_t received = 0;
    while (received < sizeof(frame) && esp_timer_get_time() < deadline) {
        if (uart_read_bytes(RS485_UART, &frame[received], 1, 0) == 1) {
            ++received;
        } else {
            vTaskDelay(pdMS_TO_TICKS(2));
        }
    }
    if (received != sizeof(frame) || frame[0] != MODBUS_SLAVE_ADDRESS || frame[1] != 0x03 || frame[2] != 0x02) {
        return false;
    }
    const uint16_t received_crc = static_cast<uint16_t>(frame[6]) | static_cast<uint16_t>(frame[7] << 8);
    if (modbus_crc16(frame, 6) != received_crc) {
        return false;
    }
    level = static_cast<float>((static_cast<uint16_t>(frame[3]) << 8) | frame[4]);
    return true;
}

static void update_imu(BNO08x* imu, IMUState& state)
{
    if (imu == nullptr || !state.initialized) {
        return;
    }
    bool got_data = false;
    if (imu->data_available()) {
        portENTER_CRITICAL(&state_mux);
        if (imu->rpt.rv_game.has_new_data()) {
            const bno08x_euler_angle_t euler = imu->rpt.rv_game.get_euler(true);
            state.roll = euler.x;
            state.pitch = euler.y;
            got_data = true;
        }
        if (imu->rpt.linear_accelerometer.has_new_data()) {
            state.ay = imu->rpt.linear_accelerometer.get().y;
            got_data = true;
        }
        if (got_data) {
            state.valid = true;
            state.last_update_us = esp_timer_get_time();
        }
        portEXIT_CRITICAL(&state_mux);
    }
}

static void sensor_task(void*)
{
    while (true) {
        update_imu(imu1, imu_state);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

static void rs485_task(void*)
{
    while (true) {
        float level = 0.0f;
        const bool ok = read_modbus_level(level);
        portENTER_CRITICAL(&state_mux);
        if (ok) {
            water_level = level;
        }
        rs485_valid = ok;
        portEXIT_CRITICAL(&state_mux);
        vTaskDelay(pdMS_TO_TICKS(MODBUS_POLL_PERIOD_MS));
    }
}

static void esp_now_task(void*)
{
    while (true) {
        TankPayload payload = {};
        portENTER_CRITICAL(&state_mux);
        payload.roll_1 = imu_state.roll;
        payload.pitch_1 = imu_state.pitch;
        payload.ay_1 = imu_state.ay;
        payload.water_level = water_level;
        const int64_t now_us = esp_timer_get_time();
        const bool imu1_ok = imu_state.valid && (now_us - imu_state.last_update_us < 500000);
        payload.sensor_status_flags = (imu1_ok ? STATUS_IMU1_OK : 0) |
                                      (rs485_valid ? STATUS_RS485_OK : 0);
        portEXIT_CRITICAL(&state_mux);
        ESP_LOGI(TAG,
                 "Enviando sensores: IMU[roll=%.3f, pitch=%.3f, ay=%.3f] "
                 "nivel=%.3f estado=0x%02X",
                 payload.roll_1, payload.pitch_1, payload.ay_1,
                 payload.water_level, payload.sensor_status_flags);
        const esp_err_t result = esp_now_send(CABIN_MAC, reinterpret_cast<const uint8_t*>(&payload), sizeof(payload));
        if (result != ESP_OK) {
            ESP_LOGW(TAG, "ESP-NOW no pudo encolar payload: %s", esp_err_to_name(result));
        }
        vTaskDelay(pdMS_TO_TICKS(250));
        //vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static bool initialize_imu(BNO08x*& imu, const bno08x_config_t& config, const char* name)
{
    imu = new BNO08x(config);
    if (!imu->initialize()) {
        ESP_LOGE(TAG, "%s no responde; se continua con su flag en 0", name);
        return false;
    }
    const bool rv_ok = imu->rpt.rv_game.enable(REPORT_INTERVAL_US);
    const bool accel_ok = imu->rpt.linear_accelerometer.enable(REPORT_INTERVAL_US);
    const bool initialized = rv_ok && accel_ok;
    ESP_LOGI(TAG, "%s inicializada: reportes=%s", name, initialized ? "OK" : "FAIL");
    return initialized;
}

static void initialize_esp_now()
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(esp_now_send_callback));

    uint8_t local_mac[ESP_NOW_ETH_ALEN] = {};
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, local_mac));
    ESP_LOGI(TAG, "MAC de estación=" MACSTR " canal=%u; destino ESP-NOW=transmisión general", MAC2STR(local_mac), ESPNOW_CHANNEL);

    esp_now_peer_info_t peer = {};
    std::memcpy(peer.peer_addr, CABIN_MAC, ESP_NOW_ETH_ALEN);
    peer.channel = ESPNOW_CHANNEL;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));
}

extern "C" void app_main(void)
{
    esp_err_t nvs_result = nvs_flash_init();
    if (nvs_result == ESP_ERR_NVS_NO_FREE_PAGES || nvs_result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_result);

    uart_config_t uart_config = {};
    uart_config.baud_rate = RS485_BAUD_RATE;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_DEFAULT;
    ESP_ERROR_CHECK(uart_driver_install(RS485_UART, 512, 0, 0, nullptr, 0));
    ESP_ERROR_CHECK(uart_param_config(RS485_UART, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(RS485_UART, PIN_RS485_TX, PIN_RS485_RX, PIN_RS485_RTS, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_set_mode(RS485_UART, UART_MODE_RS485_HALF_DUPLEX));

    bno08x_config_t config1(BNO_SPI_HOST, PIN_SPI_MOSI, PIN_SPI_MISO, PIN_SPI_SCK,
                            PIN_IMU1_CS, PIN_IMU1_HINT, PIN_IMU1_RST, SPI_CLOCK_SPEED_HZ, true);
    imu_state.initialized = initialize_imu(imu1, config1, "IMU");
    initialize_esp_now();

    xTaskCreate(sensor_task, "sensor_task", 8192, nullptr, 5, nullptr);
    xTaskCreate(rs485_task, "rs485_task", 4096, nullptr, 5, nullptr);
    xTaskCreate(esp_now_task, "esp_now_task", 4096, nullptr, 4, nullptr);
    ESP_LOGI(TAG, "Nodo Tanque operativo; payload ESP-NOW=%u bytes", static_cast<unsigned>(sizeof(TankPayload)));
}

// =============================================================================
// BLOQUE DE MACROS Y DEFINICIONES (#define)
// =============================================================================

#if 0

// --- Configuración del Bus SPI Compartido ---
#define BNO_SPI_HOST            SPI2_HOST       ///< Periférico SPI a utilizar en ESP32-S3
#define PIN_SPI_MOSI            GPIO_NUM_18     ///< Pin MOSI (DI) compartido
#define PIN_SPI_MISO            GPIO_NUM_19     ///< Pin MISO (SDA) compartido
#define PIN_SPI_SCK             GPIO_NUM_15     ///< Pin SCK (SCL) compartido
#define SPI_CLOCK_SPEED_HZ      3000000UL       ///< Velocidad SCLK (3 MHz máx. para BNO08x)

// --- Sensor 1: IMU Cabina ---
#define PIN_IMU1_CS             GPIO_NUM_22     ///< Chip Select IMU 1 (Cabina)
#define PIN_IMU1_HINT           GPIO_NUM_23     ///< Host Interrupt IMU 1
#define PIN_IMU1_RST            GPIO_NUM_5      ///< Reset IMU 1

// --- Constantes del Vehículo (Camión) y Físicas ---
#define TRUCK_H_CG              1.8f            ///< Altura del Centro de Gravedad (h = 1.8 m)
#define TRUCK_TRACK_W           2.2f            ///< Ancho de Vía / Vía del eje (T = 2.2 m)
#define GRAVITY_G               9.81f           ///< Aceleración de la gravedad (g = 9.81 m/s^2)
#define LTR_WARNING_THRESHOLD   0.6f            ///< Umbral para advertencia visual de volcadura (|LTR| > 0.6)

// --- Configuración de Reportes ---
#define REPORT_INTERVAL_US      10000UL         ///< Intervalo de reporte: 10 ms (10,000 us = 100 Hz)
#define DEG_TO_RAD_FACTOR       (M_PI / 180.0f) ///< Factor de conversión de grados a radianes

static const char* TAG = "LTR_SYSTEM";

// =============================================================================
// ESTRUCTURAS Y VARIABLES GLOBALES
// =============================================================================

/// @brief Estructura para almacenar las lecturas procesadas de una IMU
struct IMUData {
    float roll_deg;  ///< Ángulo Roll en grados
    float pitch_deg; ///< Ángulo Pitch en grados
    float ay;        ///< Aceleración lineal lateral en Y (m/s^2)
};

// Instancia global del sensor BNO08x
static BNO08x* imu1 = nullptr;

// =============================================================================
// FUNCIONES AUXILIARES Y ALGORITMO LTR
// =============================================================================

/**
 * @brief Calcula el índice de riesgo de volcadura (Load Transfer Ratio - LTR)
 * 
 * @param roll Roll de la IMU (grados)
 * @param ay Aceleración lateral de la IMU (m/s^2)
 * @param roll_avg_out Referencia de salida para el Roll promedio (grados)
 * @param ay_avg_out Referencia de salida para el Ay promedio (m/s^2)
 * @return float Valor de LTR acotado entre -1.0 y 1.0
 */
float calculate_ltr(float roll, float ay,
                  float &roll_avg_out, float &ay_avg_out)
{
    // Con una sola IMU, el promedio coincide con la lectura del sensor.
    roll_avg_out = roll;
    ay_avg_out   = ay;

    // 2. Convertir el Roll promedio a radianes para funciones trigonométricas
    float roll_avg_rad = roll_avg_out * DEG_TO_RAD_FACTOR;

    // 3. Aceleración lateral efectiva proyectada por el ángulo de inclinación (Roll):
    //    ay_efectiva = ay_promedio * cos(roll_promedio) + g * sin(roll_promedio)
    float ay_effective = (ay_avg_out * cosf(roll_avg_rad)) + (GRAVITY_G * sinf(roll_avg_rad));

    // 4. Fórmula del LTR (Load Transfer Ratio):
    //    LTR = (2 * h * ay_efectiva) / (T * g)
    float ltr_raw = (2.0f * TRUCK_H_CG * ay_effective) / (TRUCK_TRACK_W * GRAVITY_G);

    // 5. Limitar (clamp) el valor final entre -1.0 y 1.0
    float ltr_clamped = std::clamp(ltr_raw, -1.0f, 1.0f);

    // 6. Advertencia visual en consola si el valor absoluto supera el umbral crítico (0.6)
    if (std::abs(ltr_clamped) > LTR_WARNING_THRESHOLD) {
        ESP_LOGW(TAG, "¡¡¡ ADVERTENCIA DE RIESGO DE VOLCADURA !!! LTR: %.3f (Umbral > %.1f)",
                 ltr_clamped, LTR_WARNING_THRESHOLD);
    }

    return ltr_clamped;
}

/**
 * @brief Ejecuta el proceso de Tare (Calibración a Cero) en el sensor físico.
 * Estabelece la posición física actual de los sensores en el escritorio como el (0° Roll, 0° Pitch).
 */
void execute_tare_all()
{
    ESP_LOGI(TAG, "=== INICIANDO TARE (CALIBRACION A CERO) ===");
    
    bool ok1 = imu1->rpt.rv_game.tare(true, true, true);

    if (ok1) {
        ESP_LOGI(TAG, ">>> Tare ejecutado exitosamente en IMU 1 <<<");
    } else {
        ESP_LOGE(TAG, "Error durante la ejecucion del Tare: IMU1=FAIL");
    }
}

// =============================================================================
// TAREAS FREERTOS
// =============================================================================

/**
 * @brief Tarea dedicada en FreeRTOS (Core 0) para adquisición de datos de la IMU,
 *        cálculo de LTR y emisión de datos CSV para Serial Plotter.
 * 
 * @param pvParameters Parámetro de tarea FreeRTOS (no utilizado)
 */
void imu_processing_task(void* pvParameters)
{
    IMUData data1 = {0};

    ESP_LOGI(TAG, "Tarea imu_processing_task iniciada correctamente en Core %d", xPortGetCoreID());

    // Encabezado para Serial Plotter / Consola CSV
    printf("Roll_1,Ay,LTR_Calculado\n");

    while (true)
    {
        // --- 1. Lectura Sensor 1 (Cabina) ---
        if (imu1->data_available()) {
            if (imu1->rpt.rv_game.has_new_data()) {
                bno08x_euler_angle_t euler1 = imu1->rpt.rv_game.get_euler(true); // true = grados
                data1.roll_deg  = euler1.x; // Roll alrededor del eje X
                data1.pitch_deg = euler1.y; // Pitch alrededor del eje Y
            }
            if (imu1->rpt.linear_accelerometer.has_new_data()) {
                bno08x_accel_t lin_acc1 = imu1->rpt.linear_accelerometer.get();
                data1.ay = lin_acc1.y; // Aceleración lateral eje Y (m/s^2)
            }
        }

        // --- 2. Cálculo del Algoritmo LTR ---
        float roll_avg = 0.0f;
        float ay_avg   = 0.0f;
        float ltr      = calculate_ltr(data1.roll_deg, data1.ay,
                           roll_avg, ay_avg);

        // --- 3. Salida Formateada para Serial Plotter (Formato CSV continuo) ---
        printf("%.2f,%.2f,%.4f\n", data1.roll_deg, ay_avg, ltr);

        // --- 4. Verificación no bloqueante de entrada por UART para la función Tare ('t') ---
        uint8_t rx_char = 0;
        if (uart_read_bytes(UART_NUM_0, &rx_char, 1, 0) > 0) {
            if (rx_char == 't' || rx_char == 'T') {
                execute_tare_all();
            }
        }

        // Pequeño retardo para ceder CPU y mantener ciclo constante (~10 ms)
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// =============================================================================
// PUNTO DE ENTRADA PRINCIPAL (app_main)
// =============================================================================

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");

    esp_err_t uart_init_result = uart_driver_install(UART_NUM_0, 256, 0, 0, nullptr, 0);
    if (uart_init_result != ESP_OK && uart_init_result != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "No se pudo instalar el driver UART0: %s", esp_err_to_name(uart_init_result));
    }
    ESP_LOGI(TAG, " Inicializando Sistema LTR ESP32-S3 con 1 IMU     ");
    ESP_LOGI(TAG, " Bus SPI Compartido: MOSI=%d, MISO=%d, SCK=%d    ",
             PIN_SPI_MOSI, PIN_SPI_MISO, PIN_SPI_SCK);
    ESP_LOGI(TAG, "==================================================");

    // --- Configuración de Instancias BNO08x ---
    // Nota: IMU 1 instala el servicio global de ISR para HINT (install_isr_service = true).
    // El servicio global de ISR se instala junto con la unica IMU.
    bno08x_config_t config_imu1(
        BNO_SPI_HOST, PIN_SPI_MOSI, PIN_SPI_MISO, PIN_SPI_SCK,
        PIN_IMU1_CS, PIN_IMU1_HINT, PIN_IMU1_RST, SPI_CLOCK_SPEED_HZ, true
    );

    imu1 = new BNO08x(config_imu1);

    // --- Inicialización del Bus SPI y Sensores ---
    ESP_LOGI(TAG, "Inicializando IMU 1 (Cabina)...");
    if (!imu1->initialize()) {
        ESP_LOGE(TAG, "Error al inicializar IMU 1: no responde por SPI.");
        ESP_LOGE(TAG, "Revisa MOSI/DI=%d, MISO/DO=%d, SCK=%d, CS=%d, HINT=%d y RST=%d.",
                 PIN_SPI_MOSI, PIN_SPI_MISO, PIN_SPI_SCK,
                 PIN_IMU1_CS, PIN_IMU1_HINT, PIN_IMU1_RST);
        ESP_LOGE(TAG, "Verifica tambien que el BNO08x este configurado en modo SPI y tenga GND comun.");
        return;
    }
    ESP_LOGI(TAG, "IMU 1 inicializada correctamente.");

    // --- Habilitar Reportes requeridos en la IMU a 10 ms (100 Hz) ---
    // 1. Game Rotation Vector (Roll/Pitch sin magnetómetro)
    // 2. Linear Acceleration (Aceleración lateral Ay excluyendo gravedad)
    ESP_LOGI(TAG, "Habilitando reportes Game Rotation Vector y Linear Acceleration (100 Hz)...");
    
    imu1->rpt.rv_game.enable(REPORT_INTERVAL_US);
    imu1->rpt.linear_accelerometer.enable(REPORT_INTERVAL_US);

    ESP_LOGI(TAG, "Reportes habilitados en la IMU.");
    ESP_LOGI(TAG, "Presiona 't' en el Monitor Serie UART para ejecutar Tare.");

    // --- Creación de la Tarea en FreeRTOS en el Núcleo 0 (Core 0) ---
    xTaskCreatePinnedToCore(
        imu_processing_task,    // Función de la tarea
        "imu_processing_task",  // Nombre de la tarea
        8192,                   // Tamaño del stack (bytes)
        NULL,                   // Parámetros
        5,                      // Prioridad
        NULL,                   // Handle de la tarea
        0                       // Núcleo 0 (PRO_CPU_NUM)
    );
}
#endif
