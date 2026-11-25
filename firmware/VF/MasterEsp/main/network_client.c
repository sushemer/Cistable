#include "../../shared/network_client.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include <string.h>
#include "esp_timer.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_sleep.h"

static const char *TAG = "MASTER_NET";
static uint8_t s_node_id;
static network_status_provider_t s_provider;
static bool s_connected;
static bool s_reporting_enabled = true;
static uint32_t s_report_interval_ms = DEFAULT_REPORT_INTERVAL_MS;
static int64_t s_sleep_until_us = 0;
static bool s_got_ip = false;
static uint32_t s_udp_sent_count = 0;
static volatile bool s_scan_done;
static volatile bool s_scan_running;

static void network_event_handler(void *arg, esp_event_base_t base,
                                  int32_t event_id, void *event_data)
{
    (void)arg;
    if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "WiFi asociado al AP en canal %d", AP_WIFI_CHANNEL);
    } else if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = (const ip_event_got_ip_t *)event_data;
        s_got_ip = true;
        ESP_LOGI(TAG, "Conectado al AP: IP=" IPSTR, IP2STR(&event->ip_info.ip));
    } else if (base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        s_scan_done = true;
        s_scan_running = false;
    }
}

static void apply_command(const uint8_t *packet, size_t length)
{
    if (packet == NULL || length < 4U || packet[2] > 64U) return;
    switch (packet[1]) {
        case NETWORK_CMD_REPORT_RATE:
            if (packet[2] >= 5U && length >= 8U && packet[7] == NETWORK_NODE_MASTER) {
                s_report_interval_ms = (uint32_t)packet[3] |
                                       ((uint32_t)packet[4] << 8) |
                                       ((uint32_t)packet[5] << 16) |
                                       ((uint32_t)packet[6] << 24);
                ESP_LOGI(TAG, "Intervalo actualizado a %lu ms",
                         (unsigned long)s_report_interval_ms);
            }
            break;
        case NETWORK_CMD_ENABLE:
            if (packet[2] >= 2U && packet[3] == NETWORK_NODE_MASTER) {
                s_reporting_enabled = packet[4] != 0U;
                ESP_LOGI(TAG, "Reportes %s", s_reporting_enabled ? "habilitados" : "deshabilitados");
            }
            break;
        case NETWORK_CMD_SLEEP:
            if (packet[2] >= 2U && packet[3] == NETWORK_NODE_MASTER) {
                uint32_t duration_ms = 0;
                const uint8_t duration_code = packet[4];
                if (duration_code == 10U) duration_ms = SLEEP_DURATION_SHORT_MS;
                else if (duration_code == 15U) duration_ms = SLEEP_DURATION_MED_MS;
                else if (duration_code == 20U) duration_ms = SLEEP_DURATION_LONG_MS;
                if (duration_ms == 0) break;
                ESP_LOGI(TAG, "Comando Ahorro recibido para Cabina: %lu ms",
                         (unsigned long)duration_ms);
                s_reporting_enabled = false;
                ESP_LOGI(TAG, "Entrando en deep sleep real durante %lu ms",
                         (unsigned long)duration_ms);
                ESP_LOGI(TAG, "El nodo reiniciara al despertar");
                esp_sleep_enable_timer_wakeup((uint64_t)duration_ms * 1000ULL);
                esp_wifi_stop();
                vTaskDelay(pdMS_TO_TICKS(100));
                esp_deep_sleep_start();
            }
            break;
        case NETWORK_CMD_TANK_CONFIG:
            if (packet[2] == sizeof(network_tank_config_payload_t) &&
                length >= (size_t)packet[2] + 4U) {
                network_tank_config_payload_t config;
                memcpy(&config, &packet[3], sizeof(config));
                network_apply_tank_config(&config);
            }
            break;
        default:
            break;
    }
}

static void network_task(void *arg)
{
    (void)arg;
    wifi_scan_config_t scan = {0};
    s_scan_done = false;
    s_scan_running = false;
    bool ap_found = false;
    const int64_t search_deadline_us = esp_timer_get_time() +
                                       (int64_t)AP_SEARCH_TIMEOUT_MS * 1000LL;
    if (esp_wifi_scan_start(&scan, false) == ESP_OK) s_scan_running = true;
    else ESP_LOGW(TAG, "No se pudo iniciar el escaneo Wi-Fi");

    while (esp_timer_get_time() < search_deadline_us && !s_got_ip) {
        if (s_scan_done) {
            s_scan_done = false;
            uint16_t count = 0;
            esp_wifi_scan_get_ap_num(&count);
            wifi_ap_record_t records[16];
            if (count > 16) count = 16;
            esp_wifi_scan_get_ap_records(&count, records);
            for (uint16_t i = 0; i < count; ++i) {
                if (strcmp((char *)records[i].ssid, AP_WIFI_SSID) == 0) {
                    ap_found = true;
                    wifi_config_t sta_config = {0};
                    snprintf((char *)sta_config.sta.ssid, sizeof(sta_config.sta.ssid), "%s", AP_WIFI_SSID);
                    snprintf((char *)sta_config.sta.password, sizeof(sta_config.sta.password), "%s", AP_WIFI_PASS);
                    esp_wifi_set_config(WIFI_IF_STA, &sta_config);
                    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
                    if (sta_netif != NULL) {
                        esp_err_t dhcp_result = esp_netif_dhcpc_start(sta_netif);
                        if (dhcp_result != ESP_OK && dhcp_result != ESP_ERR_INVALID_STATE) {
                            ESP_LOGW(TAG, "DHCP no iniciado: %s", esp_err_to_name(dhcp_result));
                        }
                    }
                    if (esp_wifi_connect() == ESP_OK) s_connected = true;
                    break;
                }
            }
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    if (esp_timer_get_time() >= search_deadline_us) {
        if (s_scan_running) esp_wifi_scan_stop();
        s_scan_running = false;
    }
    if (!s_got_ip && !ap_found) {
        esp_wifi_set_channel(AP_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
        ESP_LOGW(TAG, "Timeout de 10 s buscando %s; ESP-NOW continua en canal %d",
                 AP_WIFI_SSID, AP_WIFI_CHANNEL);
    } else if (!s_got_ip) {
        ESP_LOGI(TAG, "AP encontrado; esperando asignacion de IP");
    }
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (fd < 0) { vTaskDelete(NULL); return; }
    struct sockaddr_in destination = {.sin_family=AF_INET, .sin_port=htons(AP_SERVER_PORT),
                                      .sin_addr.s_addr=inet_addr("192.168.4.1")};
    int rx_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (rx_fd >= 0) {
        int reuse = 1;
        setsockopt(rx_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        struct sockaddr_in local = {.sin_family=AF_INET, .sin_port=htons(AP_SERVER_PORT),
                                    .sin_addr.s_addr=htonl(INADDR_ANY)};
        if (bind(rx_fd, (struct sockaddr *)&local, sizeof(local)) < 0) {
            close(rx_fd);
            rx_fd = -1;
        }
    }
    while (true) {
        if (rx_fd >= 0) {
            uint8_t command_packet[80];
            int received = recv(rx_fd, command_packet, sizeof(command_packet), MSG_DONTWAIT);
            if (received >= 4 && network_packet_validate(command_packet, (size_t)received) &&
                command_packet[0] == NETWORK_HEADER_AP) {
                apply_command(command_packet, (size_t)received);
            }
        }
        (void)xSemaphoreTake(s_ap_send_sem, pdMS_TO_TICKS(50));
        static int64_t s_last_report_us;
        const int64_t now_us = esp_timer_get_time();
        if (s_connected && s_got_ip && s_reporting_enabled &&
            (s_sleep_until_us == 0 || now_us >= s_sleep_until_us) &&
            now_us - s_last_report_us >= (int64_t)s_report_interval_ms * 1000LL) {
            network_status_payload_t status = {.node_id=s_node_id, .flags=NETWORK_FLAG_AP_ONLINE};
            s_provider(&status);
            if (s_reporting_enabled) status.flags |= NETWORK_FLAG_REPORTING_ENABLED;
            status.report_interval_ms = (uint16_t)s_report_interval_ms;
            if (status.ltr >= 0.8f || status.ltr <= -0.8f) {
                status.risk_level = NETWORK_RISK_CRITICAL;
            } else if (status.ltr >= 0.35f || status.ltr <= -0.35f) {
                status.risk_level = NETWORK_RISK_CAUTION;
            } else {
                status.risk_level = NETWORK_RISK_SAFE;
            }
            uint8_t packet[80]; size_t length;
            if (network_packet_encode(NETWORK_HEADER_NODE, NETWORK_CMD_STATUS, &status,
                                      sizeof(status), packet, sizeof(packet), &length)) {
                const int sent = sendto(fd, packet, length, 0,
                                        (struct sockaddr *)&destination, sizeof(destination));
                if (sent == (int)length) {
                    ++s_udp_sent_count;
                    s_last_report_us = now_us;
                    if (s_udp_sent_count == 1U) {
                        ESP_LOGI(TAG, "Primer reporte UDP enviado al AP");
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

esp_err_t network_client_start(uint8_t node_id, network_status_provider_t provider)
{
    if (!provider) return ESP_ERR_INVALID_ARG;
    s_node_id = node_id; s_provider = provider;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                         WIFI_EVENT_STA_CONNECTED,
                                                         network_event_handler,
                                                         NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                         WIFI_EVENT_SCAN_DONE,
                                                         network_event_handler,
                                                         NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                         IP_EVENT_STA_GOT_IP,
                                                         network_event_handler,
                                                         NULL, NULL));
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_channel(AP_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
    BaseType_t result = xTaskCreate(network_task, "ap_report", 4096, NULL, 3, NULL);
    return result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
