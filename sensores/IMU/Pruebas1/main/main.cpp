/**
 * @file main.cpp
 * @brief Lectura, Calibración (Tare) y Algoritmo LTR con 3 IMUs GY-BNO085 (SPI / C++) en ESP32-S3
 * @details Este programa inicializa 3 IMUs BNO085 compartiendo el bus SPI de un ESP32-S3,
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
#define PIN_SPI_MOSI            GPIO_NUM_11     ///< Pin MOSI (DI) compartido
#define PIN_SPI_MISO            GPIO_NUM_13     ///< Pin MISO (SDA) compartido
#define PIN_SPI_SCK             GPIO_NUM_12     ///< Pin SCK (SCL) compartido
#define SPI_CLOCK_SPEED_HZ      3000000UL       ///< Velocidad SCLK (3 MHz máx. para BNO08x)

// --- Sensor 1: IMU Cabina ---
#define PIN_IMU1_CS             GPIO_NUM_10     ///< Chip Select IMU 1 (Cabina)
#define PIN_IMU1_HINT           GPIO_NUM_4      ///< Host Interrupt IMU 1
#define PIN_IMU1_RST            GPIO_NUM_5      ///< Reset IMU 1

// --- Sensor 2: IMU Tanque Izquierdo ---
#define PIN_IMU2_CS             GPIO_NUM_9      ///< Chip Select IMU 2 (Tanque Izq)
#define PIN_IMU2_HINT           GPIO_NUM_6      ///< Host Interrupt IMU 2
#define PIN_IMU2_RST            GPIO_NUM_7      ///< Reset IMU 2

// --- Sensor 3: IMU Tanque Derecho ---
#define PIN_IMU3_CS             GPIO_NUM_14     ///< Chip Select IMU 3 (Tanque Der)
#define PIN_IMU3_HINT           GPIO_NUM_15     ///< Host Interrupt IMU 3
#define PIN_IMU3_RST            GPIO_NUM_16     ///< Reset IMU 3

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

// Instancias globales de los 3 sensores BNO08x
static BNO08x* imu1 = nullptr;
static BNO08x* imu2 = nullptr;
static BNO08x* imu3 = nullptr;

// =============================================================================
// FUNCIONES AUXILIARES Y ALGORITMO LTR
// =============================================================================

/**
 * @brief Calcula el índice de riesgo de volcadura (Load Transfer Ratio - LTR)
 * 
 * @param roll1 Roll IMU 1 (grados)
 * @param roll2 Roll IMU 2 (grados)
 * @param roll3 Roll IMU 3 (grados)
 * @param ay1 Aceleración lateral IMU 1 (m/s^2)
 * @param ay2 Aceleración lateral IMU 2 (m/s^2)
 * @param ay3 Aceleración lateral IMU 3 (m/s^2)
 * @param roll_avg_out Referencia de salida para el Roll promedio (grados)
 * @param ay_avg_out Referencia de salida para el Ay promedio (m/s^2)
 * @return float Valor de LTR acotado entre -1.0 y 1.0
 */
float calculate_ltr(float roll1, float roll2, float roll3,
                  float ay1, float ay2, float ay3,
                  float &roll_avg_out, float &ay_avg_out)
{
    // 1. Promedio del ángulo Roll (en grados) y aceleración lateral Ay (en m/s^2)
    roll_avg_out = (roll1 + roll2 + roll3) / 3.0f;
    ay_avg_out   = (ay1 + ay2 + ay3) / 3.0f;

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
 * @brief Ejecuta el proceso de Tare (Calibración a Cero) en los 3 sensores físicos.
 * Estabelece la posición física actual de los sensores en el escritorio como el (0° Roll, 0° Pitch).
 */
void execute_tare_all()
{
    ESP_LOGI(TAG, "=== INICIANDO TARE (CALIBRACIÓN A CERO) EN LAS 3 IMUs ===");
    
    bool ok1 = imu1->rpt.rv_game.tare(true, true, true);
    bool ok2 = imu2->rpt.rv_game.tare(true, true, true);
    bool ok3 = imu3->rpt.rv_game.tare(true, true, true);

    if (ok1 && ok2 && ok3) {
        ESP_LOGI(TAG, ">>> Tare ejecutado exitosamente en IMU 1, IMU 2 e IMU 3 <<<");
    } else {
        ESP_LOGE(TAG, "Error durante la ejecución del Tare: IMU1=%s, IMU2=%s, IMU3=%s",
                 ok1 ? "OK" : "FAIL", ok2 ? "OK" : "FAIL", ok3 ? "OK" : "FAIL");
    }
}

// =============================================================================
// TAREAS FREERTOS
// =============================================================================

/**
 * @brief Tarea dedicada en FreeRTOS (Core 0) para adquisición de datos de IMUs,
 *        cálculo de LTR y emisión de datos CSV para Serial Plotter.
 * 
 * @param pvParameters Parámetro de tarea FreeRTOS (no utilizado)
 */
void imu_processing_task(void* pvParameters)
{
    IMUData data1 = {0}, data2 = {0}, data3 = {0};

    ESP_LOGI(TAG, "Tarea imu_processing_task iniciada correctamente en Core %d", xPortGetCoreID());

    // Encabezado para Serial Plotter / Consola CSV
    printf("Roll_1,Roll_2,Roll_3,Ay_Promedio,LTR_Calculado\n");

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

        // --- 2. Lectura Sensor 2 (Tanque Izquierdo) ---
        if (imu2->data_available()) {
            if (imu2->rpt.rv_game.has_new_data()) {
                bno08x_euler_angle_t euler2 = imu2->rpt.rv_game.get_euler(true);
                data2.roll_deg  = euler2.x;
                data2.pitch_deg = euler2.y;
            }
            if (imu2->rpt.linear_accelerometer.has_new_data()) {
                bno08x_accel_t lin_acc2 = imu2->rpt.linear_accelerometer.get();
                data2.ay = lin_acc2.y;
            }
        }

        // --- 3. Lectura Sensor 3 (Tanque Derecho) ---
        if (imu3->data_available()) {
            if (imu3->rpt.rv_game.has_new_data()) {
                bno08x_euler_angle_t euler3 = imu3->rpt.rv_game.get_euler(true);
                data3.roll_deg  = euler3.x;
                data3.pitch_deg = euler3.y;
            }
            if (imu3->rpt.linear_accelerometer.has_new_data()) {
                bno08x_accel_t lin_acc3 = imu3->rpt.linear_accelerometer.get();
                data3.ay = lin_acc3.y;
            }
        }

        // --- 4. Cálculo del Algoritmo LTR ---
        float roll_avg = 0.0f;
        float ay_avg   = 0.0f;
        float ltr      = calculate_ltr(data1.roll_deg, data2.roll_deg, data3.roll_deg,
                                      data1.ay, data2.ay, data3.ay,
                                      roll_avg, ay_avg);

        // --- 5. Salida Formateada para Serial Plotter (Formato CSV continuo) ---
        printf("%.2f,%.2f,%.2f,%.2f,%.4f\n",
               data1.roll_deg, data2.roll_deg, data3.roll_deg, ay_avg, ltr);

        // --- 6. Verificación no bloqueante de entrada por UART para la función Tare ('t') ---
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
    ESP_LOGI(TAG, " Inicializando Sistema LTR ESP32-S3 con 3 IMUs    ");
    ESP_LOGI(TAG, " Bus SPI Compartido: MOSI=%d, MISO=%d, SCK=%d    ",
             PIN_SPI_MOSI, PIN_SPI_MISO, PIN_SPI_SCK);
    ESP_LOGI(TAG, "==================================================");

    // --- Configuración de Instancias BNO08x ---
    // Nota: IMU 1 instala el servicio global de ISR para HINT (install_isr_service = true).
    // IMU 2 e IMU 3 reutilizan el servicio de ISR global ya instalado (install_isr_service = false).
    bno08x_config_t config_imu1(
        BNO_SPI_HOST, PIN_SPI_MOSI, PIN_SPI_MISO, PIN_SPI_SCK,
        PIN_IMU1_CS, PIN_IMU1_HINT, PIN_IMU1_RST, SPI_CLOCK_SPEED_HZ, true
    );

    bno08x_config_t config_imu2(
        BNO_SPI_HOST, PIN_SPI_MOSI, PIN_SPI_MISO, PIN_SPI_SCK,
        PIN_IMU2_CS, PIN_IMU2_HINT, PIN_IMU2_RST, SPI_CLOCK_SPEED_HZ, false
    );

    bno08x_config_t config_imu3(
        BNO_SPI_HOST, PIN_SPI_MOSI, PIN_SPI_MISO, PIN_SPI_SCK,
        PIN_IMU3_CS, PIN_IMU3_HINT, PIN_IMU3_RST, SPI_CLOCK_SPEED_HZ, false
    );

    imu1 = new BNO08x(config_imu1);
    imu2 = new BNO08x(config_imu2);
    imu3 = new BNO08x(config_imu3);

    // --- Inicialización del Bus SPI y Sensores ---
    ESP_LOGI(TAG, "Inicializando IMU 1 (Cabina)...");
    if (!imu1->initialize()) {
        ESP_LOGE(TAG, "¡Error al inicializar IMU 1!");
    } else {
        ESP_LOGI(TAG, "IMU 1 inicializada correctamente.");
    }

    ESP_LOGI(TAG, "Inicializando IMU 2 (Tanque Izquierdo)...");
    if (!imu2->initialize()) {
        ESP_LOGE(TAG, "¡Error al inicializar IMU 2!");
    } else {
        ESP_LOGI(TAG, "IMU 2 inicializada correctamente.");
    }

    ESP_LOGI(TAG, "Inicializando IMU 3 (Tanque Derecho)...");
    if (!imu3->initialize()) {
        ESP_LOGE(TAG, "¡Error al inicializar IMU 3!");
    } else {
        ESP_LOGI(TAG, "IMU 3 inicializada correctamente.");
    }

    // --- Habilitar Reportes requeridos en los 3 sensores a 10 ms (100 Hz) ---
    // 1. Game Rotation Vector (Roll/Pitch sin magnetómetro)
    // 2. Linear Acceleration (Aceleración lateral Ay excluyendo gravedad)
    ESP_LOGI(TAG, "Habilitando reportes Game Rotation Vector y Linear Acceleration (100 Hz)...");
    
    imu1->rpt.rv_game.enable(REPORT_INTERVAL_US);
    imu1->rpt.linear_accelerometer.enable(REPORT_INTERVAL_US);

    imu2->rpt.rv_game.enable(REPORT_INTERVAL_US);
    imu2->rpt.linear_accelerometer.enable(REPORT_INTERVAL_US);

    imu3->rpt.rv_game.enable(REPORT_INTERVAL_US);
    imu3->rpt.linear_accelerometer.enable(REPORT_INTERVAL_US);

    ESP_LOGI(TAG, "Reportes habilitados en los 3 sensores.");
    ESP_LOGI(TAG, "Presiona 't' en el Monitor Serie UART para ejecutar Tare en los 3 sensores.");

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
