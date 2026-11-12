#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_now.h"
#include "freertos/FreeRTOS.h"

#define FIELD_MAC_LENGTH 6U

typedef struct {
    uint8_t source_mac[FIELD_MAC_LENGTH]; // MAC STA del emisor, para diagnóstico.
    int8_t rssi_dbm;                      // Nivel RSSI medido al recibir el paquete.
    bool rssi_valid;                      // Indica si el callback entregó metadatos RSSI.
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
