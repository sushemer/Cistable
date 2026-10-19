idf_component_register(
    SRCS "gps_device.c" "app_main.c" "gps_validation.c" "gps_nmea.c" "gps_data.c"
         "gps_speed.c"
    INCLUDE_DIRS "."
    REQUIRES esp_timer driver
)
