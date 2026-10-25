#include "imu_device.h"

#include <inttypes.h>

#include "BNO08x.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "IMU";
static BNO08x *imu_handle = nullptr;

// Imprime un reporte IMU con el formato que entiende gps_imu_logger.py.
static void imprimir_reporte(const char *tipo, int64_t recibido_us,
                             float x, float y, float z, float w,
                             unsigned estado) {
    ESP_LOGI(TAG, "IMU|1|%" PRId64 "|0|%s|%.5f|%.5f|%.5f|%.5f|%u",
             recibido_us, tipo, x, y, z, w, estado);
}

// Inicializa una BNO085 en modo SPI y habilita orientación y aceleración lineal.
static bool imu_device_init() {
    const bno08x_config_t config(
        IMU_SPI_HOST, IMU_MOSI_GPIO, IMU_MISO_GPIO, IMU_SCLK_GPIO,
        IMU_CS_GPIO, IMU_INT_GPIO, IMU_RST_GPIO, IMU_SPI_CLOCK_HZ, true);

    imu_handle = new BNO08x(config);
    if (!imu_handle->initialize()) {
        ESP_LOGE(TAG, "BNO085 no responde por SPI; revise MOSI, MISO, SCK, CS, HINT y RST");
        delete imu_handle;
        imu_handle = nullptr;
        return false;
    }

    if (!imu_handle->rpt.rv_game.enable(IMU_REPORT_PERIOD_US) ||
        !imu_handle->rpt.linear_accelerometer.enable(IMU_REPORT_PERIOD_US)) {
        ESP_LOGE(TAG, "No se pudieron habilitar los reportes SPI de la BNO085");
        return false;
    }

    ESP_LOGI(TAG, "BNO085 inicializada por SPI; reportes solicitados a 5 Hz");
    return true;
}

// Atiende la IMU y conserva cada reporte recibido en la salida serial.
static void imu_task(void *argument) {
    (void)argument;
    if (!imu_device_init()) {
        ESP_LOGE(TAG, "Lectura IMU deshabilitada por error de inicialización");
        vTaskDelete(nullptr);
        return;
    }

    while (true) {
        if (imu_handle->data_available()) {
            const int64_t recibido_us = esp_timer_get_time();
            if (imu_handle->rpt.rv_game.has_new_data()) {
                const bno08x_euler_angle_t euler = imu_handle->rpt.rv_game.get_euler(true);
                imprimir_reporte("ROTATION_EULER", recibido_us, euler.x, euler.y, euler.z, 0.0f,
                                 static_cast<unsigned>(euler.accuracy));
            }
            if (imu_handle->rpt.linear_accelerometer.has_new_data()) {
                const bno08x_accel_t accel = imu_handle->rpt.linear_accelerometer.get();
                imprimir_reporte("LINEAR_ACCEL", recibido_us, accel.x, accel.y, accel.z, 0.0f,
                                 static_cast<unsigned>(accel.accuracy));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Inicia la tarea de lectura SPI sin bloquear la tarea principal.
extern "C" void imu_device_start(void) {
    if (xTaskCreate(imu_task, "imu_reader", IMU_TASK_STACK_SIZE, nullptr,
                    IMU_TASK_PRIORITY, nullptr) != pdPASS) {
        ESP_LOGE(TAG, "No se pudo crear la tarea de lectura SPI de la IMU");
    }
}
