#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_now.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FIELD_MAC_LENGTH 6U

typedef struct {
    uint8_t source_mac[FIELD_MAC_LENGTH]; // MAC STA del emisor, para diagnóstico.
    int8_t rssi_dbm;                      // Nivel RSSI medido al recibir el paquete.
    bool rssi_valid;                      // Indica si el callback entregó metadatos RSSI.
    uint64_t received_us;
    uint64_t receive_id; /* monotonically assigned at callback, not printing */
    uint16_t data_len;                    // Longitud real del frame recibido.
    uint8_t raw_data[ESP_NOW_MAX_DATA_LEN]; // TankPayload recibido.
} field_espnow_rx_t;

// Inicia Wi-Fi en modo STA y ESP-NOW en el canal indicado. Esta primera
// prueba agrega la MAC broadcast para no depender de MAC configuradas a mano.
esp_err_t field_espnow_init(uint8_t channel);

// Recibe desde la cola copiada en el callback Wi-Fi; timeout puede ser cero.
bool field_espnow_receive(field_espnow_rx_t *received, TickType_t timeout);

// Lee la MAC STA local para identificar cada placa durante la configuración.
esp_err_t field_espnow_get_mac(uint8_t mac[FIELD_MAC_LENGTH]);
typedef struct {
    uint64_t rx_callbacks,rx_delivered,rx_skipped,tx_submitted,tx_completed,tx_failed,tx_busy;
    uint64_t rx_rejected_input;
    bool sending;
} field_espnow_stats;
typedef void (*field_rx_observer)(const field_espnow_rx_t *item);
typedef void (*field_tx_observer)(const uint8_t *data,size_t size,int status);
/* Register before starting Wi-Fi. Observers must copy without blocking. */
void field_espnow_observers(field_rx_observer rx,field_tx_observer tx);
esp_err_t field_espnow_try_send(const uint8_t *data,size_t size);
void field_espnow_get_stats(field_espnow_stats *out);

#ifdef __cplusplus
}
#endif
