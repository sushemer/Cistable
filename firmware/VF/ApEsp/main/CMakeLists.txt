idf_component_register(SRCS "main.c" "ble_service.c"
                       INCLUDE_DIRS "." "../../shared"
                       REQUIRES esp_wifi esp_event esp_netif esp_http_server esp_timer
                                nvs_flash spiffs bt)

spiffs_create_partition_image(spiffs "${CMAKE_CURRENT_LIST_DIR}/../data" FLASH_IN_PROJECT)
