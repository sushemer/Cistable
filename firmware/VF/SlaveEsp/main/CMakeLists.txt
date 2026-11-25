idf_component_register(SRCS "network_client.c" "main.cpp"
                    INCLUDE_DIRS "." "../../shared"
                    REQUIRES esp32_BNO08x driver esp_timer freertos esp_wifi esp_event esp_netif lwip nvs_flash esp_hw_support)
