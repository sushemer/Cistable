idf_component_register(SRCS "captura.c" "captura_runtime.c" INCLUDE_DIRS "include"
    REQUIRES field_espnow control_cabina esp_timer esp_driver_uart vfs esp_driver_usb_serial_jtag esp_hw_support spi_flash esp_psram)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wformat=2 -Werror=format)
