#ifndef ESPNOW_RECEPTOR_H
#define ESPNOW_RECEPTOR_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "protocolo_estado.h"

#ifndef PRUEBA_RTT
#define PRUEBA_RTT 1
#endif
#if PRUEBA_RTT
esp_err_t espnow_receptor_rtt_peer(const uint8_t mac[6]);
esp_err_t espnow_receptor_rtt_ack(uint16_t secuencia);
#endif

typedef struct {
    uint8_t mac_origen[6];
    int32_t longitud_original;
    uint8_t datos[PROTOCOLO_ESTADO_LONGITUD];
    bool rssi_valido;
    int8_t rssi_dbm;
} espnow_recepcion_t;

// Inicializar una sola vez antes de consumir la cola.
esp_err_t espnow_receptor_init(uint8_t canal);
bool espnow_receptor_recibir(espnow_recepcion_t *recepcion, TickType_t espera);

#endif
