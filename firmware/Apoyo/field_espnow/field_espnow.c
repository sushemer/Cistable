#include "field_espnow.h"

#include <string.h>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "freertos/queue.h"

#define RX_QUEUE_LENGTH 8

static const char *TAG = "FIELD_ESPNOW";
static QueueHandle_t s_rx_queue;
static const uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

// Conserva cualquier frame valido; la aplicacion decide como interpretar su formato.
static void receive_callback(const esp_now_recv_info_t *info, const uint8_t *data, int length){
    if (s_rx_queue == NULL || info == NULL || info->src_addr == NULL ||
        length < 0 || length > ESP_NOW_MAX_DATA_LEN || (length > 0 && data == NULL)) {
        return;
    }

    field_espnow_rx_t item = {0};
    memcpy(item.source_mac, info->src_addr, sizeof(item.source_mac));
    if (info->rx_ctrl != NULL) {
        item.rssi_dbm = (int8_t)info->rx_ctrl->rssi;
        item.rssi_valid = true;
    }
    item.data_len = (uint16_t)length;
    if (length > 0) {
        memcpy(item.raw_data, data, (size_t)length);
    }
    // El callback corre en la tarea Wi-Fi: copiar a cola y salir rapido.
    (void)xQueueSend(s_rx_queue, &item, 0);
}

// Inicia NVS, Wi-Fi STA y ESP-NOW; registra broadcast y la cola de recepción.
esp_err_t field_espnow_init(uint8_t channel){
    if (s_rx_queue != NULL) return ESP_ERR_INVALID_STATE;
    if (channel < 1 || channel > 14) return ESP_ERR_INVALID_ARG;
    
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(err, TAG, "NVS");

    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    const wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&wifi_config), TAG, "Wi-Fi init");
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "Wi-Fi storage");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "Wi-Fi mode");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "Wi-Fi start");
    ESP_RETURN_ON_ERROR(esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE), TAG, "Canal Wi-Fi");
    ESP_RETURN_ON_ERROR(esp_now_init(), TAG, "ESP-NOW init");

    s_rx_queue = xQueueCreate(RX_QUEUE_LENGTH, sizeof(field_espnow_rx_t));
    if (s_rx_queue == NULL) return ESP_ERR_NO_MEM;

    esp_now_peer_info_t broadcast_peer = {0};
    memcpy(broadcast_peer.peer_addr, s_broadcast_mac, sizeof(s_broadcast_mac));
    broadcast_peer.channel = channel;
    broadcast_peer.ifidx = WIFI_IF_STA;
    broadcast_peer.encrypt = false;
    err = esp_now_add_peer(&broadcast_peer);
    if (err != ESP_OK && err != ESP_ERR_ESPNOW_EXIST) return err;

    ESP_RETURN_ON_ERROR(esp_now_register_recv_cb(receive_callback), TAG, "Registro RX");
    ESP_LOGI(TAG, "ESP-NOW listo en canal %u; broadcast habilitado para prueba", channel);
    return ESP_OK;
}

// Espera en la cola de recepción hasta el timeout solicitado por la aplicación.
bool field_espnow_receive(field_espnow_rx_t *received, TickType_t timeout){
    return received != NULL && s_rx_queue != NULL &&
           xQueueReceive(s_rx_queue, received, timeout) == pdTRUE;
}

// Obtiene la MAC de la interfaz STA usada por ESP-NOW.
esp_err_t field_espnow_get_mac(uint8_t mac[FIELD_MAC_LENGTH]){
    if (mac == NULL) return ESP_ERR_INVALID_ARG;
    return esp_wifi_get_mac(WIFI_IF_STA, mac);
}
