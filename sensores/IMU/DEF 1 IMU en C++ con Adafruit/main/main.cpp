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
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/uart.h"
#include "BNO08x.hpp"

// =============================================================================
// BLOQUE DE MACROS Y DEFINICIONES (#define)
// =============================================================================

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
