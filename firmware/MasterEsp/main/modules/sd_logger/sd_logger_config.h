#pragma once

#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

// El Master conserva el cableado S3 de Twig; se agregan MISO y CS para la SD.
#define SD_LOGGER_SPI_HOST SPI2_HOST
#if CONFIG_IDF_TARGET_ESP32S3
#define SD_LOGGER_SCK_GPIO GPIO_NUM_12
#define SD_LOGGER_MOSI_GPIO GPIO_NUM_11
#define SD_LOGGER_MISO_GPIO GPIO_NUM_13
#define SD_LOGGER_CS_GPIO GPIO_NUM_14
#define SD_LOGGER_LCD_CS_GPIO GPIO_NUM_10
#else
#error "Este MasterEsp utiliza el cableado original de Twig para ESP32-S3."
#endif

// Cada dispositivo mantiene su frecuencia; el LCD original sigue usando 20 MHz.
#define SD_LOGGER_MAX_FREQ_KHZ 4000
#define SD_LOGGER_MAX_TRANSFER_BYTES (320 * 80 * 2)
#define SD_LOGGER_LINE_BYTES 384
