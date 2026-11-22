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

#ifndef PRUEBA_RTT
#define PRUEBA_RTT 1
#endif
#if PRUEBA_RTT
#include <inttypes.h>
#include <stdatomic.h>
#include "freertos/queue.h"
#define ACK_TIMEOUT_US 500000LL
static const uint8_t MAC_RECEPTOR_STA[6] = {0x8C, 0x94, 0xDF, 0x48, 0x36, 0x98};
static QueueHandle_t cola_ack;
static atomic_uint cola_saturada;
// Estructuras exclusivamente locales; nunca se transmiten.
typedef struct {
    uint8_t mac[6];
    uint8_t datos[3];
    int64_t llegada_us;
} ack_evento_t;
typedef struct {
    bool pendiente;
    uint16_t secuencia;
    int64_t envio_us;
} muestra_rtt_t;
static muestra_rtt_t muestras[8];
static uint64_t intentos, enviados, errores, validos, vencidos, tardios, saturaciones;
static uint64_t suma_us, minimo_us = UINT64_MAX, maximo_us;
static uint64_t siguiente_resumen = 100;

static void ack_cb(const esp_now_recv_info_t *info, const uint8_t *datos, int longitud)
{
    const int64_t llegada = esp_timer_get_time();
    if (!info || !info->src_addr || !datos || longitud != 3 || datos[0] != 0xA1 ||
        memcmp(info->src_addr, MAC_RECEPTOR_STA, 6) != 0) return;
    ack_evento_t evento = {.llegada_us = llegada};
    memcpy(evento.mac, info->src_addr, 6);
    memcpy(evento.datos, datos, 3);
    if (xQueueSend(cola_ack, &evento, 0) != pdTRUE) atomic_fetch_add(&cola_saturada, 1);
}

// Solo sender_task posee la tabla y los acumuladores.
static void procesar_acks(void)
{
    const int64_t ahora = esp_timer_get_time();
    ack_evento_t evento;
    while (xQueueReceive(cola_ack, &evento, 0) == pdTRUE) {
        uint16_t sec = (uint16_t)evento.datos[1] | ((uint16_t)evento.datos[2] << 8);
        bool encontrado = false;
        for (unsigned i = 0; i < 8; i++) {
            muestra_rtt_t *m = &muestras[i];
            if (!m->pendiente || m->secuencia != sec) continue;
            encontrado = true;
            int64_t rtt = evento.llegada_us - m->envio_us;
            if (rtt >= 0 && rtt < ACK_TIMEOUT_US) {
                validos++;
                suma_us += (uint64_t)rtt;
                if ((uint64_t)rtt < minimo_us) minimo_us = (uint64_t)rtt;
                if ((uint64_t)rtt > maximo_us) maximo_us = (uint64_t)rtt;
            } else {
                vencidos++;
                tardios++;
            }
            m->pendiente = false;
            break;
        }
        if (!encontrado) tardios++;
    }
    for (unsigned i = 0; i < 8; i++) {
        if (muestras[i].pendiente && ahora - muestras[i].envio_us >= ACK_TIMEOUT_US) {
            muestras[i].pendiente = false;
            vencidos++;
        }
    }
    if (validos >= siguiente_resumen) {
        ESP_LOGI(TAG, "RTT: intentos=%" PRIu64 " enviados=%" PRIu64 " errores=%" PRIu64
                 " ACK_validos=%" PRIu64 " vencidos=%" PRIu64 " tardios_duplicados=%" PRIu64
                 " tabla_saturada=%" PRIu64 " cola_saturada=%u",
                 intentos, enviados, errores, validos, vencidos, tardios, saturaciones,
                 atomic_load(&cola_saturada));
        ESP_LOGI(TAG, "RTT_us: min=%" PRIu64 " promedio=%" PRIu64 " max=%" PRIu64
                 " una_via_estimada=%" PRIu64,
                 minimo_us, suma_us / validos, maximo_us, suma_us / (2 * validos));
        siguiente_resumen = validos + 100;
    }
}
#endif

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
#if PRUEBA_RTT
        procesar_acks();
        muestra_rtt_t *muestra = NULL;
        for (unsigned i = 0; i < 8; i++) {
            if (!muestras[i].pendiente) { muestra = &muestras[i]; break; }
        }
        intentos++;
        if (muestra) {
            muestra->pendiente = true;
            muestra->secuencia = secuencia;
        } else {
            saturaciones++;
        }
        if (muestra) muestra->envio_us = esp_timer_get_time();
#endif
        esp_err_t result = esp_now_send(s_broadcast_mac, datos, 6);
#if PRUEBA_RTT
        if (result == ESP_OK) enviados++;
        else {
            errores++;
            if (muestra) muestra->pendiente = false;
        }
#endif
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
#if PRUEBA_RTT
    cola_ack = xQueueCreate(8, sizeof(ack_evento_t));
    if (!cola_ack) ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(esp_now_register_recv_cb(ack_cb));
#endif
    esp_now_peer_info_t peer;
    memset(&peer, 0, sizeof(esp_now_peer_info_t));
    peer.channel = ESPNOW_CHANNEL;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    memcpy(peer.peer_addr, s_broadcast_mac, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    xTaskCreate(sender_task, "sender_task", 2048, NULL, 4, NULL);
}

