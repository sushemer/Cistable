#include "imu_module.h"

#include "esp_log.h"
#include "sdkconfig.h"
#include "perfil_mesa.h"
#include "cableado_cabina.h"

// New ESP32-S3 board map; the old DevKit V1 pinout is historical only.
#define FRONT_IMU_SPI_HOST CI_CABIN_IMU_HOST
#define FRONT_IMU_MOSI static_cast<gpio_num_t>(CONFIG_CI_IMU_MOSI)
#define FRONT_IMU_MISO static_cast<gpio_num_t>(CONFIG_CI_IMU_MISO)
#define FRONT_IMU_SCLK static_cast<gpio_num_t>(CONFIG_CI_IMU_SCLK)
#define FRONT_IMU_CS static_cast<gpio_num_t>(CONFIG_CI_IMU_CS)
#define FRONT_IMU_INT static_cast<gpio_num_t>(CONFIG_CI_IMU_INT)
#define FRONT_IMU_RESET static_cast<gpio_num_t>(CONFIG_CI_IMU_RST)
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
    config.report_period_us = CI_REPORT_PERIOD_US;
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
