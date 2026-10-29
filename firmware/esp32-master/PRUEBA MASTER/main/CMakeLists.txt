idf_component_register(
    SRCS "app_main.c" "modules/gps/gps_module.c" "modules/imu/imu_module.cpp"
         "modules/gps/driver/gps_data.c" "modules/gps/driver/gps_device.c"
         "modules/gps/driver/gps_nmea.c" "modules/gps/driver/gps_speed.c"
         "modules/gps/driver/gps_validation.c"
    INCLUDE_DIRS "." "modules/gps" "modules/gps/driver"
    REQUIRES field_espnow imu_bno085 esp_timer esp_wifi driver
)
