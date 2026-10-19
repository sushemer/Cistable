set(GPS2_MAIN_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../prueba_02/firmware/main")

idf_component_register(
    SRCS "app_main.c" "imu_device.c" "gps_bridge.c"
         "${GPS2_MAIN_DIR}/gps_device.c"
         "${GPS2_MAIN_DIR}/gps_nmea.c"
         "${GPS2_MAIN_DIR}/gps_data.c"
         "${GPS2_MAIN_DIR}/gps_validation.c"
    INCLUDE_DIRS "." "${GPS2_MAIN_DIR}"
    REQUIRES bno085 driver esp_timer
)

# BNO085 1.0.2 uses %u for uint32_t in its own log messages. With the
# installed Xtensa toolchain that emits a format warning promoted to an error;
# downgrade only that warning for this third-party component, not for our code.
idf_component_get_property(BNO085_COMPONENT_LIB rinku404__bno085 COMPONENT_LIB)
if(BNO085_COMPONENT_LIB)
    target_compile_options(${BNO085_COMPONENT_LIB} PRIVATE -Wno-error=format)
endif()
