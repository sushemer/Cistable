idf_component_register(
    SRCS "main.c" "board_spi_pins.c" "modules/gps/gps_module.c" "modules/ltr/ltr_module.c"
         "modules/gps/driver/gps_data.c" "modules/gps/driver/gps_device.c"
         "modules/gps/driver/gps_nmea.c" "modules/gps/driver/gps_speed.c"
         "modules/gps/driver/gps_validation.c" "modules/display/display_module.c"
         "modules/display/display_port.c" "modules/display/interface.c"
         "modules/display/recursos/camion_pequeno.c"
         "modules/display/recursos/camion_grande.c"
         "modules/pendulum/pendulum_module.c"
         "modules/imu/imu_module.cpp"
         "modules/sd_logger/sd_logger.c"
    INCLUDE_DIRS "." "modules/gps" "modules/gps/driver" "modules/display"
                 "modules/ltr" "modules/pendulum" "modules/sd_logger"
    REQUIRES field_espnow imu_bno085 esp_timer esp_wifi driver fatfs sdmmc lvgl esp_lcd_st7796
)
