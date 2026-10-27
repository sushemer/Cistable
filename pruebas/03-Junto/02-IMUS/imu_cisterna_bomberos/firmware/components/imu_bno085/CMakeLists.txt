# Adaptador C/C++ para leer una BNO085 mediante el driver compartido del proyecto.
idf_component_register(
    SRCS "imu_bno085.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp32_BNO08x esp_timer freertos driver
)
