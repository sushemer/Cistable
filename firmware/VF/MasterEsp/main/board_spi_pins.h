#pragma once

#include <stdbool.h>

#include "sdkconfig.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#if CONFIG_IDF_TARGET_ESP32S3
#define BOARD_SHARED_SPI_HOST SPI2_HOST
#define BOARD_SHARED_SPI_SCLK_GPIO GPIO_NUM_12
#define BOARD_SHARED_SPI_MOSI_GPIO GPIO_NUM_11
#define BOARD_SHARED_SPI_MISO_GPIO GPIO_NUM_13
#define BOARD_LCD_CS_GPIO GPIO_NUM_10
#else
#error "Este MasterEsp utiliza el cableado original de Twig para ESP32-S3."
#endif

esp_err_t board_shared_spi_init(void);
extern SemaphoreHandle_t s_spi2_mutex;
