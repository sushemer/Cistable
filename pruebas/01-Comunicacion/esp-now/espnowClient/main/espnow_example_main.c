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

static const char *TAG = "ESPNOW_RECEIVER";

typedef struct {
    uint32_t counter;
    char message[32];
} espnow_message_t;

static uint32_t s_last_counter = 0;
static bool s_first_packet = true;

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

// --- RECEPCIÓN + MEDICIÓN DE RSSI Y PÉRDIDAS ---
static void example_espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    if (recv_info == NULL || data == NULL || len <= 0) return;

    if (len == sizeof(espnow_message_t)) {
        espnow_message_t *msg = (espnow_message_t *)data;

        // 1. Lectura directa del RSSI desde el control del paquete
        int rssi = 0;
        if (recv_info->rx_ctrl) {
            rssi = recv_info->rx_ctrl->rssi;
        }

        // 2. Detección de paquetes omitidos
        if (!s_first_packet && msg->counter > s_last_counter + 1) {
            uint32_t lost = msg->counter - s_last_counter - 1;
            ESP_LOGW(TAG, "⚠️ ALERTA: Se perdieron %lu paquete(s) intermedio(s)", (unsigned long)lost);
        }
        s_first_packet = false;
        s_last_counter = msg->counter;

        ESP_LOGI(TAG, "De " MACSTR " | Seq: %lu | RSSI: %d dBm | Msg: %s",
                 MAC2STR(recv_info->src_addr),
                 (unsigned long)msg->counter,
                 rssi,
                 msg->message);
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
    ESP_ERROR_CHECK(esp_now_register_recv_cb(example_espnow_recv_cb));

    ESP_LOGI(TAG, "Receptor ESP-NOW listo y midiendo en Canal %d...", ESPNOW_CHANNEL);
}