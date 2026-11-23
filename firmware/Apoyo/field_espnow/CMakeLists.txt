# Transporte ESP-NOW del master; recibe TankPayload de 29 bytes.
idf_component_register(SRCS "field_espnow.c" INCLUDE_DIRS "." REQUIRES esp_wifi nvs_flash esp_netif esp_event esp_common)
