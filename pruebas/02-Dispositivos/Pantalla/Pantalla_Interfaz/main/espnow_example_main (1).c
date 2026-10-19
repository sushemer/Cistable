#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_timer.h"

#define ESPNOW_CHANNEL 1

static const char *TAG = "ESPNOW_SENDER";
static uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

static void example_wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));
}

static void sender_task(void *pvParameters)
{
    (void)pvParameters;
    uint16_t secuencia = 0;
    const int64_t inicio_us = esp_timer_get_time();
    TickType_t ultima_activacion = xTaskGetTickCount();
    unsigned paso_anterior = 4;
    static const char *const nombres[] = {
        "SEGURO", "PRECAUCION", "PELIGRO", "DETENIDO 78%"
    };

    while (1) {
        unsigned paso = (unsigned)(((esp_timer_get_time() - inicio_us) / 5000000LL) % 4);
        uint8_t datos[6];
        datos[0] = 1;
        datos[1] = paso == 3 ? 2 : 1;
        datos[2] = paso == 3 ? 0 : (uint8_t)paso;
        datos[3] = 78;
        datos[4] = (uint8_t)(secuencia & 0xFF);
        datos[5] = (uint8_t)(secuencia >> 8);

        if (paso != paso_anterior) {
            ESP_LOGI(TAG, "Estado: %s", nombres[paso]);
            paso_anterior = paso;
        }
        esp_err_t result = esp_now_send(s_broadcast_mac, datos, 6);
        secuencia++;
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "Error al invocar envio: %s", esp_err_to_name(result));
        }
        vTaskDelayUntil(&ultima_activacion, pdMS_TO_TICKS(200));
    }
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    example_wifi_init();
    uint8_t mac_sta[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac_sta));
    ESP_LOGI(TAG, "MAC STA del emisor: " MACSTR, MAC2STR(mac_sta));
    ESP_LOGI(TAG, "Canal: %d", ESPNOW_CHANNEL);

    ESP_ERROR_CHECK(esp_now_init());
    esp_now_peer_info_t peer;
    memset(&peer, 0, sizeof(esp_now_peer_info_t));
    peer.channel = ESPNOW_CHANNEL;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    memcpy(peer.peer_addr, s_broadcast_mac, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    xTaskCreate(sender_task, "sender_task", 2048, NULL, 4, NULL);
}
