idf_component_register(
    SRCS "main.c" "modules/gps/gps_module.c" "modules/ltr/ltr_module.c"
         "modules/gps/driver/gps_data.c" "modules/gps/driver/gps_device.c"
         "modules/gps/driver/gps_nmea.c" "modules/gps/driver/gps_speed.c"
         "modules/gps/driver/gps_validation.c" "modules/display/display_module.c"
         "modules/display/display_port.c" "modules/display/interface.c"
         "modules/display/recursos/camion_pequeno.c"
         "modules/display/recursos/camion_grande.c"
         "modules/pendulum/pendulum_module.c"
         "modules/imu/imu_module.cpp"
    INCLUDE_DIRS "." "modules/gps" "modules/gps/driver" "modules/display"
                 "modules/ltr" "modules/pendulum"
    REQUIRES field_espnow imu_bno085 esp_timer esp_wifi driver lvgl esp_lcd_st7796
)
