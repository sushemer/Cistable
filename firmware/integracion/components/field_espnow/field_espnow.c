#include "field_espnow.h"

#include <string.h>
#include "esp_timer.h"

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "freertos/queue.h"

#define RX_QUEUE_LENGTH 1

static const char *TAG = "FIELD_ESPNOW";
static QueueHandle_t s_rx_queue;
static portMUX_TYPE s_stats_mux=portMUX_INITIALIZER_UNLOCKED;
static field_espnow_stats s_stats;
static uint64_t s_last_delivered;
static field_rx_observer s_rx_observer;
static field_tx_observer s_tx_observer;
static uint8_t s_tx_data[ESP_NOW_MAX_DATA_LEN];
static size_t s_tx_size;
static const uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

// Conserva cualquier frame valido; la aplicacion decide como interpretar su formato.
static void receive_callback(const esp_now_recv_info_t *info, const uint8_t *data, int length){
    if (s_rx_queue == NULL || info == NULL || info->src_addr == NULL ||
        length < 0 || length > ESP_NOW_MAX_DATA_LEN || (length > 0 && data == NULL)) {
        portENTER_CRITICAL(&s_stats_mux);++s_stats.rx_rejected_input;portEXIT_CRITICAL(&s_stats_mux);
        return;
    }

    field_espnow_rx_t item = {0};
    memcpy(item.source_mac, info->src_addr, sizeof(item.source_mac));
    if (info->rx_ctrl != NULL) {
        item.rssi_dbm = (int8_t)info->rx_ctrl->rssi;
        item.rssi_valid = true;
    }
    item.received_us=esp_timer_get_time();
    portENTER_CRITICAL(&s_stats_mux);item.receive_id=++s_stats.rx_callbacks;portEXIT_CRITICAL(&s_stats_mux);
    item.data_len = (uint16_t)length;
    if (length > 0) {
        memcpy(item.raw_data, data, (size_t)length);
    }
    // El callback corre en la tarea Wi-Fi: copiar a cola y salir rapido.
    if(s_rx_observer) s_rx_observer(&item);
    (void)xQueueOverwrite(s_rx_queue, &item);
}
static void send_callback(const esp_now_send_info_t *info,esp_now_send_status_t status) {
    (void)info;
    /* Keep sending=true until observer has copied the completed frame. */
    if(s_tx_observer) s_tx_observer(s_tx_data,s_tx_size,(int)status);
    portENTER_CRITICAL(&s_stats_mux);
    ++s_stats.tx_completed;if(status!=ESP_NOW_SEND_SUCCESS) ++s_stats.tx_failed;
    s_stats.sending=false;portEXIT_CRITICAL(&s_stats_mux);
}
void field_espnow_observers(field_rx_observer rx,field_tx_observer tx) {
    s_rx_observer=rx;s_tx_observer=tx;
}
void field_espnow_get_stats(field_espnow_stats *out) {
    if(!out) return;
    portENTER_CRITICAL(&s_stats_mux);*out=s_stats;portEXIT_CRITICAL(&s_stats_mux);
}
esp_err_t field_espnow_try_send(const uint8_t *data,size_t size) {
    if(!data||size>sizeof(s_tx_data)||!size) return ESP_ERR_INVALID_ARG;
    portENTER_CRITICAL(&s_stats_mux);
    if(s_stats.sending) {++s_stats.tx_busy;portEXIT_CRITICAL(&s_stats_mux);return ESP_ERR_INVALID_STATE;}
    s_stats.sending=true;memcpy(s_tx_data,data,size);s_tx_size=size;
    portEXIT_CRITICAL(&s_stats_mux);
    esp_err_t result=esp_now_send(s_broadcast_mac,s_tx_data,size);
    portENTER_CRITICAL(&s_stats_mux);
    if(result==ESP_OK) ++s_stats.tx_submitted;
    else {s_stats.sending=false;++s_stats.tx_failed;}
    portEXIT_CRITICAL(&s_stats_mux);return result;
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
    ESP_RETURN_ON_ERROR(esp_now_register_send_cb(send_callback), TAG, "Registro TX");
    ESP_LOGI(TAG, "ESP-NOW listo en canal %u; broadcast habilitado para prueba", channel);
    return ESP_OK;
}

// Espera en la cola de recepción hasta el timeout solicitado por la aplicación.
bool field_espnow_receive(field_espnow_rx_t *received, TickType_t timeout){
    if(received==NULL||s_rx_queue==NULL||xQueueReceive(s_rx_queue,received,timeout)!=pdTRUE) return false;
    portENTER_CRITICAL(&s_stats_mux);
    ++s_stats.rx_delivered;s_stats.rx_skipped+=received->receive_id-s_last_delivered-1;
    s_last_delivered=received->receive_id;portEXIT_CRITICAL(&s_stats_mux);
    return true;
}

// Obtiene la MAC de la interfaz STA usada por ESP-NOW.
esp_err_t field_espnow_get_mac(uint8_t mac[FIELD_MAC_LENGTH]){
    if (mac == NULL) return ESP_ERR_INVALID_ARG;
    return esp_wifi_get_mac(WIFI_IF_STA, mac);
}
