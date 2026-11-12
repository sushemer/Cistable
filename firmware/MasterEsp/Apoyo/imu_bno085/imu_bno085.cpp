#include "imu_bno085.h"

#include <inttypes.h>
#include <new>

#include "BNO08x.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

// Envuelve el objeto C++ de la biblioteca del compañero y conserva una muestra protegida.
struct imu_bno085 {
    explicit imu_bno085(const imu_bno085_config_t &cfg): config(cfg), tag(cfg.name ? cfg.name : "BNO085"),
          sensor(bno08x_config_t(cfg.spi_host, cfg.mosi_gpio, cfg.miso_gpio, cfg.sclk_gpio, cfg.cs_gpio, cfg.int_gpio, cfg.reset_gpio,
              cfg.spi_clock_hz, cfg.install_isr_service)),
          mutex(xSemaphoreCreateMutex()) {}

    imu_bno085_config_t config;
    const char *tag;
    BNO08x sensor;
    SemaphoreHandle_t mutex;
    imu_bno085_sample_t sample{};
    TaskHandle_t task{};
};

// Serializa el acceso y publica una instantánea completa para evitar lecturas parciales.
static void publish_sample(imu_bno085 *device, const imu_bno085_sample_t &sample){
    if (xSemaphoreTake(device->mutex, portMAX_DELAY) == pdTRUE) {
        device->sample = sample;
        xSemaphoreGive(device->mutex);
    }
}

// Atiende reportes habilitados y guarda Euler, aceleración lineal y giro sin fusionarlos.
static void imu_service_task(void *argument){
    auto *device = static_cast<imu_bno085 *>(argument);
    if (device == nullptr) {
        vTaskDelete(nullptr);
        return;
    }

    imu_bno085_sample_t sample{};
    sample.online = true;
    publish_sample(device, sample);
    while (true) {
        if (device->sensor.data_available()) {
            bool changed = false;
            if (device->sensor.rpt.rv_game.has_new_data()) {
                const bno08x_euler_angle_t euler = device->sensor.rpt.rv_game.get_euler(true);
                sample.roll_deg = euler.x;
                sample.pitch_deg = euler.y;
                sample.yaw_deg = euler.z;
                sample.has_orientation = true;
                changed = true;
            }
            if (device->sensor.rpt.linear_accelerometer.has_new_data()) {
                const bno08x_accel_t accel = device->sensor.rpt.linear_accelerometer.get();
                sample.linear_accel_m_s2[0] = accel.x;
                sample.linear_accel_m_s2[1] = accel.y;
                sample.linear_accel_m_s2[2] = accel.z;
                sample.has_linear_acceleration = true;
                changed = true;
            }
            if (device->sensor.rpt.cal_gyro.has_new_data()) {
                const bno08x_gyro_t gyro = device->sensor.rpt.cal_gyro.get();
                sample.gyro_rad_s[0] = gyro.x;
                sample.gyro_rad_s[1] = gyro.y;
                sample.gyro_rad_s[2] = gyro.z;
                sample.has_gyro = true;
                changed = true;
            }
            if (changed) {
                sample.sample_time_us = esp_timer_get_time();
                publish_sample(device, sample);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(device->config.service_period_ms));
    }
}

extern "C" esp_err_t imu_bno085_create(const imu_bno085_config_t *config, imu_bno085_handle_t *handle){
    // Valida recursos básicos antes de reservar el objeto del sensor.
    if (config == nullptr || handle == nullptr || config->report_period_us == 0 ||
        config->service_period_ms == 0 || config->spi_clock_hz == 0) return ESP_ERR_INVALID_ARG;
    auto *device = new (std::nothrow) imu_bno085(*config);
    if (device == nullptr || device->mutex == nullptr) {
        delete device;
        return ESP_ERR_NO_MEM;
    }
    *handle = device;
    return ESP_OK;
}

extern "C" esp_err_t imu_bno085_start(imu_bno085_handle_t handle){
    // Inicializa el hardware antes de crear la tarea, evitando carreras al instalar ISR.
    if (handle == nullptr || handle->task != nullptr) return ESP_ERR_INVALID_ARG;
    // Inicializar aquí hace que varios sensores instalen ISR en orden, sin carrera entre tareas.
    if (!handle->sensor.initialize()) {
        ESP_LOGE(handle->tag, "No se pudo inicializar la BNO085 por SPI");
        return ESP_FAIL;
    }
    // Se habilitan reportes de orientación, aceleración lineal y giro, sin lógica de LTR/tare.
    if (!handle->sensor.rpt.rv_game.enable(handle->config.report_period_us) ||
        !handle->sensor.rpt.linear_accelerometer.enable(handle->config.report_period_us) ||
        !handle->sensor.rpt.cal_gyro.enable(handle->config.report_period_us)) {
        ESP_LOGE(handle->tag, "No se pudieron habilitar los reportes de orientación, aceleración y giro");
        return ESP_FAIL;
    }
    ESP_LOGI(handle->tag, "BNO085 inicializada; periodo solicitado=%" PRIu32 " us", handle->config.report_period_us);
    const char *task_name = handle->config.name ? handle->config.name : "bno085_reader";
    return xTaskCreate(imu_service_task, task_name, 6144, handle, 5, &handle->task) == pdPASS
        ? ESP_OK : ESP_ERR_NO_MEM;
}

extern "C" bool imu_bno085_get_sample(imu_bno085_handle_t handle, imu_bno085_sample_t *sample){
    // Copia la muestra con timeout corto y reporta false si ya está vencida.
    if (handle == nullptr || sample == nullptr || handle->mutex == nullptr) return false;
    if (xSemaphoreTake(handle->mutex, pdMS_TO_TICKS(10)) != pdTRUE) return false;
    *sample = handle->sample;
    xSemaphoreGive(handle->mutex);
    // Evita que una lectura antigua parezca vigente si el sensor deja de responder.
    if (sample->sample_time_us > 0 &&
        esp_timer_get_time() - sample->sample_time_us > 500000) sample->online = false;
    return sample->online && sample->sample_time_us > 0;
}
