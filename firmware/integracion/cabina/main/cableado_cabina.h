#pragma once
#include "sdkconfig.h"
#include "driver/spi_master.h"
/* Controller numbers in menuconfig are hardware SPI2/SPI3, not enum ordinals.
 * All GPIOs live in the same Kconfig board section. S3 N16R8 expected only. */
#define CI_LCD_HOST (CONFIG_CI_LCD_SPI_HOST==2?SPI2_HOST:SPI3_HOST)
#define CI_CABIN_IMU_HOST (CONFIG_CI_IMU_SPI_HOST==2?SPI2_HOST:SPI3_HOST)
