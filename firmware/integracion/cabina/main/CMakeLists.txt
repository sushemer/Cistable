idf_component_register(SRCS "app_main.c" "seleccion.c" "demo.c" "configuracion_modelo.c" "presentacion.c"
    "diagnostico.c" "diagnostico_servicio.c" "pipa_virtual.c"
    "pantalla/pantalla.c" "display/display.c" "interfaz/interfaz.c"
    "interfaz/recursos/camion_pequeno.c" "interfaz/recursos/camion_grande.c" "interfaz/recursos/fuente_enie.c"
    "modules/imu/imu_module.cpp" "modules/gps/gps_module.c"
    "modules/gps/driver/gps_data.c" "modules/gps/driver/gps_device.c"
    "modules/gps/driver/gps_nmea.c" "modules/gps/driver/gps_speed.c" "modules/gps/driver/gps_validation.c"
    INCLUDE_DIRS "." "display" "interfaz" "pantalla" "modules/gps" "modules/gps/driver"
    REQUIRES captura control_cabina field_espnow imu_bno085 esp_timer esp_wifi driver
    esp_driver_uart esp_driver_spi esp_driver_gpio heap esp_lcd lvgl esp_lcd_st7796)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wformat=2 -Werror=format)
