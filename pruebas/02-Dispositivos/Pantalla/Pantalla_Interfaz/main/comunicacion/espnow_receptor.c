#include "espnow_receptor.h"
#include <string.h>
#include "freertos/queue.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_check.h"

static QueueHandle_t cola;
static const char *TAG = "espnow_receptor";

#if PRUEBA_RTT
static uint8_t mac_peer_rtt[6];

esp_err_t espnow_receptor_rtt_peer(const uint8_t mac[6])
{
    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = 1;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    esp_err_t ret = esp_now_add_peer(&peer);
    if (ret == ESP_OK) memcpy(mac_peer_rtt, mac, 6);
    return ret;
}

esp_err_t espnow_receptor_rtt_ack(uint16_t secuencia)
{
    const uint8_t ack[3] = {0xA1, (uint8_t)secuencia, (uint8_t)(secuencia >> 8)};
    return esp_now_send(mac_peer_rtt, ack, 3);
}
#endif

static void recepcion_cb(const esp_now_recv_info_t *info,
                         const uint8_t *datos, int longitud)
{
    if (info == NULL || info->src_addr == NULL || datos == NULL || longitud <= 0) return;
    espnow_recepcion_t recepcion = {0};
    memcpy(recepcion.mac_origen, info->src_addr, sizeof(recepcion.mac_origen));
    recepcion.longitud_original = longitud;
    size_t copiar = longitud < PROTOCOLO_ESTADO_LONGITUD ?
                    (size_t)longitud : PROTOCOLO_ESTADO_LONGITUD;
    memcpy(recepcion.datos, datos, copiar);
    if (info->rx_ctrl != NULL) {
        recepcion.rssi_valido = true;
        recepcion.rssi_dbm = info->rx_ctrl->rssi;
    }
    (void)xQueueSend(cola, &recepcion, 0);
}

esp_err_t espnow_receptor_init(uint8_t canal)
{
    if (cola != NULL) return ESP_ERR_INVALID_STATE;
    if (canal < 1 || canal > 14) return ESP_ERR_INVALID_ARG;

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // Recuperacion del ejemplo: borra la particion NVS completa.
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "Borrado NVS");
        ret = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "Inicializacion NVS");
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "esp_netif");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "Event loop");
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "WiFi init");
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "WiFi RAM");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "WiFi STA");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "WiFi start");
    ESP_RETURN_ON_ERROR(esp_wifi_set_channel(canal, WIFI_SECOND_CHAN_NONE), TAG, "Canal");
    ESP_RETURN_ON_ERROR(esp_now_init(), TAG, "ESP-NOW init");
    cola = xQueueCreate(8, sizeof(espnow_recepcion_t));
    if (cola == NULL) return ESP_ERR_NO_MEM;
    ret = esp_now_register_recv_cb(recepcion_cb);
    if (ret != ESP_OK) {
        vQueueDelete(cola);
        cola = NULL;
    }
    return ret;
}

bool espnow_receptor_recibir(espnow_recepcion_t *recepcion, TickType_t espera)
{
    return xQueueReceive(cola, recepcion, espera) == pdTRUE;
}
