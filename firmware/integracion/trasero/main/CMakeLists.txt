idf_component_register(SRCS "app_main.cpp" INCLUDE_DIRS "."
    REQUIRES captura imu_bno085 telemetria field_espnow esp_timer esp_wifi esp_driver_uart esp_hw_support)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wformat=2 -Werror=format)
