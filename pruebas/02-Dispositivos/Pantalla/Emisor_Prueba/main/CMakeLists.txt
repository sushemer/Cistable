idf_component_register(
    SRCS "espnow_example_main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES nvs_flash esp_netif esp_event esp_wifi esp_timer
)
