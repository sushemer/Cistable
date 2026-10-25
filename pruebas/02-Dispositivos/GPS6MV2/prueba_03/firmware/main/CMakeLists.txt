set(GPS2_MAIN_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../prueba_02/firmware/main")

idf_component_register(
    SRCS "app_main.c" "imu_device.cpp" "gps_bridge.c"
         "${GPS2_MAIN_DIR}/gps_device.c"
         "${GPS2_MAIN_DIR}/gps_nmea.c"
         "${GPS2_MAIN_DIR}/gps_data.c"
         "${GPS2_MAIN_DIR}/gps_validation.c"
    INCLUDE_DIRS "." "${GPS2_MAIN_DIR}"
    REQUIRES esp32_BNO08x driver esp_timer
)
