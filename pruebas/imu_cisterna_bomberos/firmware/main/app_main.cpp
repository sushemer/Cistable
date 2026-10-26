#include "imu_bno085.h"

#include <inttypes.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "IMU_CISTERNA";

extern "C" void app_main(void)
{
    imu_bno085_config_t config{};
    config.spi_host = SPI2_HOST;
    config.mosi_gpio = GPIO_NUM_23;
    config.miso_gpio = GPIO_NUM_19;
    config.sclk_gpio = GPIO_NUM_18;
    config.cs_gpio = GPIO_NUM_5;
    config.int_gpio = GPIO_NUM_4;
    config.reset_gpio = GPIO_NUM_22;
    config.spi_clock_hz = 3000000;
    config.report_period_us = 10000;
    config.service_period_ms = 2;
    config.install_isr_service = true;
    config.name = "imu_cisterna";

    imu_bno085_handle_t imu = nullptr;
    ESP_ERROR_CHECK(imu_bno085_create(&config, &imu));
    ESP_ERROR_CHECK(imu_bno085_start(imu));
    ESP_LOGI(TAG, "IMU lista; SPI2 MOSI=23 MISO=19 SCLK=18 CS=5 INT=4 RESET=22");

    while (true) {
        imu_bno085_sample_t sample{};
        const bool valid = imu_bno085_get_sample(imu, &sample);
        ESP_LOGI(TAG,
                 "IMU_DATA|1|%" PRId64 "|%d|%.3f|%.3f|%.3f|%.3f|%.3f|%.3f|%.3f|%.3f|%.3f",
                 sample.sample_time_us,
                 valid ? 1 : 0,
                 sample.roll_deg,
                 sample.pitch_deg,
                 sample.yaw_deg,
                 sample.linear_accel_m_s2[0],
                 sample.linear_accel_m_s2[1],
                 sample.linear_accel_m_s2[2],
                 sample.gyro_rad_s[0],
                 sample.gyro_rad_s[1],
                 sample.gyro_rad_s[2]);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
