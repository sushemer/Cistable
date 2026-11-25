#include <stdio.h>
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

#define ESPNOW_CHANNEL 1

static const char *TAG = "ESPNOW_SENDER";
static uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

typedef struct {
    uint32_t counter;
    char message[32];
} espnow_message_t;

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

// --- CONFIRMACIÓN DE ENTREGA EN EL EMISOR (ACK) ---
static void example_espnow_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
{
    if (tx_info == NULL) return;

    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGI(TAG, "Envio a " MACSTR " [EXITOSO - ACK Recibido]", MAC2STR(tx_info->des_addr));
    } else {
        ESP_LOGW(TAG, "Envio a " MACSTR " [FALLIDO - Sin respuesta / Fuera de alcance]", MAC2STR(tx_info->des_addr));
    }
}

static void sender_task(void *pvParameters)
{
    espnow_message_t msg;
    uint32_t count = 0;

    while (1) {
        msg.counter = count++;
        snprintf(msg.message, sizeof(msg.message), "Mensaje ESP-NOW #%lu", (unsigned long)msg.counter);

        esp_err_t result = esp_now_send(s_broadcast_mac, (uint8_t *)&msg, sizeof(msg));
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "Error al invocar envio: %s", esp_err_to_name(result));
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
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

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(example_espnow_send_cb));

    esp_now_peer_info_t peer;
    memset(&peer, 0, sizeof(esp_now_peer_info_t));
    peer.channel = ESPNOW_CHANNEL;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    memcpy(peer.peer_addr, s_broadcast_mac, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    xTaskCreate(sender_task, "sender_task", 2048, NULL, 4, NULL);
}