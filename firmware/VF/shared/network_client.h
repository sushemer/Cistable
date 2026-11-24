#pragma once
#include "network_protocol.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*network_status_provider_t)(network_status_payload_t *status);
esp_err_t network_client_start(uint8_t node_id, network_status_provider_t provider);
void network_apply_tank_config(const network_tank_config_payload_t *config);
extern SemaphoreHandle_t s_ap_send_sem;

#ifdef __cplusplus
}
#endif
