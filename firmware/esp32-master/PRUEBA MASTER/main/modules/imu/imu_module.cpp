#include "imu_module.h"

#include "esp_log.h"

// Pines ESP32 DevKit V1 alineados con la prueba 04.
#define FRONT_IMU_SPI_HOST SPI2_HOST
#define FRONT_IMU_MOSI GPIO_NUM_23
#define FRONT_IMU_MISO GPIO_NUM_19
#define FRONT_IMU_SCLK GPIO_NUM_18
#define FRONT_IMU_CS GPIO_NUM_5
#define FRONT_IMU_INT GPIO_NUM_4
#define FRONT_IMU_RESET GPIO_NUM_22
// BNO085: PS0 = 1 y PS1 = 1, ambos conectados a VCC, para seleccionar SPI.

static const char *TAG = "IMU_FRONT"; // Identifica mensajes de este módulo en UART0.
static imu_bno085_handle_t s_front_imu;

// Configura la IMU frontal y solicita los tres reportes usados en esta prueba funcional.
extern "C" bool imu_module_start(void){
    // Asignaciones explícitas compatibles con el estándar C++ configurado en ESP-IDF.
    imu_bno085_config_t config{};
    config.spi_host = FRONT_IMU_SPI_HOST;
    config.mosi_gpio = FRONT_IMU_MOSI;
    config.miso_gpio = FRONT_IMU_MISO;
    config.sclk_gpio = FRONT_IMU_SCLK;
    config.cs_gpio = FRONT_IMU_CS;
    config.int_gpio = FRONT_IMU_INT;
    config.reset_gpio = FRONT_IMU_RESET;
    config.spi_clock_hz = 3000000;
    config.report_period_us = 10000;
    config.service_period_ms = 2;
    config.install_isr_service = true;
    config.name = "imu_front";
    esp_err_t err = imu_bno085_create(&config, &s_front_imu);
    if (err == ESP_OK) err = imu_bno085_start(s_front_imu);
    if (err != ESP_OK) ESP_LOGE(TAG, "No se pudo iniciar el módulo BNO085: %s", esp_err_to_name(err));
    return err == ESP_OK;
}

// Copia la última muestra del driver común; no calcula variables de volcadura.
extern "C" bool imu_module_get_sample(imu_bno085_sample_t *sample){
    return imu_bno085_get_sample(s_front_imu, sample);
}
