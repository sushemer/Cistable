#include "board_spi_pins.h"

#include "esp_check.h"

#define BOARD_SHARED_SPI_MAX_TRANSFER_BYTES (320 * 80 * 2)
SemaphoreHandle_t s_spi2_mutex;

esp_err_t board_shared_spi_init(void)
{
    static bool initialized;
    if (initialized) return ESP_OK;
    if (s_spi2_mutex == NULL) s_spi2_mutex = xSemaphoreCreateMutex();

    ESP_RETURN_ON_ERROR(gpio_set_direction(BOARD_LCD_CS_GPIO, GPIO_MODE_OUTPUT),
                        "BOARD_SPI", "LCD CS");
    ESP_RETURN_ON_ERROR(gpio_set_level(BOARD_LCD_CS_GPIO, 1),
                        "BOARD_SPI", "LCD CS idle");

    const spi_bus_config_t bus_config = {
        .sclk_io_num = BOARD_SHARED_SPI_SCLK_GPIO,
        .mosi_io_num = BOARD_SHARED_SPI_MOSI_GPIO,
        .miso_io_num = BOARD_SHARED_SPI_MISO_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = BOARD_SHARED_SPI_MAX_TRANSFER_BYTES,
    };
    const esp_err_t error = spi_bus_initialize(BOARD_SHARED_SPI_HOST, &bus_config,
                                                SPI_DMA_CH_AUTO);
    if (error == ESP_OK) initialized = true;
    return error;
}
