idf_component_register(
    SRCS "app_main.c" "modules/gps/gps_module.c" "modules/imu/imu_module.cpp"
         "modules/gps/driver/gps_data.c" "modules/gps/driver/gps_device.c"
         "modules/gps/driver/gps_nmea.c" "modules/gps/driver/gps_speed.c"
         "modules/gps/driver/gps_validation.c" "modules/display/display_module.c"
         "modules/display/display_port.c" "modules/display/interface.c"
    INCLUDE_DIRS "." "modules/gps" "modules/gps/driver" "modules/display"
    REQUIRES field_espnow imu_bno085 esp_timer esp_wifi driver lvgl esp_lcd_st7796
)
