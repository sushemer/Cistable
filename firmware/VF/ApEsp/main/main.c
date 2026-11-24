#include "network_protocol.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_spiffs.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <string.h>

#define HISTORY_LENGTH 10
void ble_service_start(void);
static const char *TAG = "APESP";
static network_status_payload_t s_latest[3], s_history[HISTORY_LENGTH];
static size_t s_history_count, s_history_head;
static SemaphoreHandle_t s_mutex;
static uint32_t s_udp_report_count;

static void record_status(const network_status_payload_t *status)
{
    if (status == NULL || status->node_id > 2U) return;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_latest[status->node_id] = *status;
    s_history[s_history_head] = *status;
    s_history_head = (s_history_head + 1U) % HISTORY_LENGTH;
    if (s_history_count < HISTORY_LENGTH) ++s_history_count;
    xSemaphoreGive(s_mutex);
}

static void udp_task(void *arg)
{
    (void)arg;
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (fd < 0) { ESP_LOGE(TAG, "No se pudo crear socket UDP"); vTaskDelete(NULL); return; }
    struct sockaddr_in address = {.sin_family = AF_INET, .sin_port = htons(AP_SERVER_PORT),
                                  .sin_addr.s_addr = htonl(INADDR_ANY)};
    if (bind(fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        ESP_LOGE(TAG, "No se pudo enlazar UDP"); close(fd); vTaskDelete(NULL); return;
    }
    uint8_t buffer[128];
    while (true) {
        struct sockaddr_in source;
        socklen_t source_length = sizeof(source);
        int received = recvfrom(fd, buffer, sizeof(buffer), 0,
                                (struct sockaddr *)&source, &source_length);
        if (received >= 4 && network_packet_validate(buffer, (size_t)received) &&
            buffer[0] == NETWORK_HEADER_NODE && buffer[1] == NETWORK_CMD_STATUS &&
            buffer[2] == sizeof(network_status_payload_t)) {
            network_status_payload_t status;
            memcpy(&status, &buffer[3], sizeof(status));
            record_status(&status);
            ++s_udp_report_count;
            if (status.node_id == NETWORK_NODE_MASTER || status.node_id == NETWORK_NODE_SLAVE) {
                uint32_t source_ip = ntohl(source.sin_addr.s_addr);
                ESP_LOGI(TAG, "Reporte nodo %u recibido desde %lu.%lu.%lu.%lu",
                         status.node_id,
                         (unsigned long)((source_ip >> 24U) & 0xffU),
                         (unsigned long)((source_ip >> 16U) & 0xffU),
                         (unsigned long)((source_ip >> 8U) & 0xffU),
                         (unsigned long)(source_ip & 0xffU));
            } else if ((s_udp_report_count % 10U) == 1U) {
                ESP_LOGI(TAG, "Reporte UDP recibido: nodo=%u total=%lu",
                         status.node_id, (unsigned long)s_udp_report_count);
            }
        } else if (received >= 0) {
            ESP_LOGW(TAG, "Paquete UDP descartado: CRC o formato invalido");
        }
    }
}

static esp_err_t file_handler(httpd_req_t *req, const char *path, const char *type)
{
    FILE *file = fopen(path, "r");
    if (!file) return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found");
    httpd_resp_set_type(req, type);
    char buffer[1024]; size_t count;
    while ((count = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (httpd_resp_send_chunk(req, buffer, count) != ESP_OK) { fclose(file); return ESP_FAIL; }
    }
    fclose(file);
    return httpd_resp_send_chunk(req, NULL, 0);
}
static esp_err_t index_handler(httpd_req_t *r) { return file_handler(r, "/spiffs/index.html", "text/html"); }
static esp_err_t css_handler(httpd_req_t *r) { return file_handler(r, "/spiffs/style.css", "text/css"); }
static esp_err_t js_handler(httpd_req_t *r) { return file_handler(r, "/spiffs/app.js", "application/javascript"); }

static esp_err_t status_handler(httpd_req_t *req)
{
    network_status_payload_t v[3];
    xSemaphoreTake(s_mutex, portMAX_DELAY); memcpy(v, s_latest, sizeof(v)); xSemaphoreGive(s_mutex);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    char json[768];
    int length = snprintf(json, sizeof(json),
        "{\"master\":{\"online\":%s,\"roll\":%.2f,\"pitch\":%.2f,\"ay\":%.2f,\"speed\":%.2f,\"ltr\":%.3f,\"risk\":%u,\"interval\":%u,\"flags\":%u},"
        "\"slave\":{\"online\":%s,\"roll\":%.2f,\"pitch\":%.2f,\"ay\":%.2f,\"water\":%.2f,\"ltr\":%.3f,\"risk\":%u,\"interval\":%u,\"flags\":%u}}",
        (v[1].flags & NETWORK_FLAG_AP_ONLINE) ? "true":"false", v[1].roll_deg, v[1].pitch_deg,
        v[1].lateral_accel_m_s2, v[1].gps_speed_kmh, v[1].ltr,
        (unsigned)v[1].risk_level, (unsigned)v[1].report_interval_ms, (unsigned)v[1].flags,
        (v[2].flags & NETWORK_FLAG_AP_ONLINE) ? "true":"false", v[2].roll_deg, v[2].pitch_deg,
        v[2].lateral_accel_m_s2, v[2].water_level, v[2].ltr,
        (unsigned)v[2].risk_level, (unsigned)v[2].report_interval_ms, (unsigned)v[2].flags);
    return httpd_resp_send(req, json, length);
}

static esp_err_t history_handler(httpd_req_t *req)
{
    network_status_payload_t h[HISTORY_LENGTH]; size_t count;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    count = s_history_count;
    for (size_t i = 0; i < count; ++i) h[i] = s_history[(s_history_head + HISTORY_LENGTH - count + i) % HISTORY_LENGTH];
    xSemaphoreGive(s_mutex);
    char json[2048]; size_t used = (size_t)snprintf(json, sizeof(json), "[");
    for (size_t i = 0; i < count && used < sizeof(json); ++i)
        used += (size_t)snprintf(json + used, sizeof(json) - used,
            "%s{\"node\":%u,\"ltr\":%.3f,\"water\":%.2f,\"speed\":%.2f}",
            i ? "," : "", h[i].node_id, h[i].ltr, h[i].water_level, h[i].gps_speed_kmh);
    if (used < sizeof(json)) snprintf(json + used, sizeof(json) - used, "]");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t command_handler(httpd_req_t *req)
{
    uint8_t packet[80];
    size_t received = 0;
    while (received < req->content_len && received < sizeof(packet)) {
        int chunk = httpd_req_recv(req, (char *)&packet[received], req->content_len - received);
        if (chunk <= 0) break;
        received += (size_t)chunk;
    }
    if (received < 4 || !network_packet_validate(packet, received) ||
        packet[0] != NETWORK_HEADER_AP) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid packet");
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (fd >= 0) {
        int broadcast = 1;
        setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
        struct sockaddr_in destination = {.sin_family=AF_INET, .sin_port=htons(AP_SERVER_PORT),
                                          .sin_addr.s_addr=inet_addr("192.168.4.255")};
        sendto(fd, packet, received, 0, (struct sockaddr *)&destination, sizeof(destination));
        close(fd);
    }
    ESP_LOGI(TAG, "Comando 0x%02X recibido (%d bytes)", packet[1], received);
    return httpd_resp_sendstr(req, "OK");
}

static void start_http(void)
{
    httpd_handle_t server; httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 16384;
    config.max_uri_handlers = 16;
    ESP_ERROR_CHECK(httpd_start(&server, &config));
    const httpd_uri_t routes[] = {
        {"/", HTTP_GET, index_handler, NULL}, {"/style.css", HTTP_GET, css_handler, NULL},
        {"/app.js", HTTP_GET, js_handler, NULL}, {"/api/status", HTTP_GET, status_handler, NULL},
        {"/api/history", HTTP_GET, history_handler, NULL}, {"/api/command", HTTP_POST, command_handler, NULL}};
    for (size_t i = 0; i < sizeof(routes)/sizeof(routes[0]); ++i) ESP_ERROR_CHECK(httpd_register_uri_handler(server, &routes[i]));
}

void app_main(void)
{
    esp_err_t nvs = nvs_flash_init();
    if (nvs == ESP_ERR_NVS_NO_FREE_PAGES || nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase()); ESP_ERROR_CHECK(nvs_flash_init());
    } else ESP_ERROR_CHECK(nvs);
    ESP_ERROR_CHECK(esp_netif_init()); ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap(); wifi_init_config_t wifi = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi));
    wifi_config_t ap = {.ap = {.ssid = AP_WIFI_SSID, .ssid_len = sizeof(AP_WIFI_SSID)-1U,
        .channel = AP_WIFI_CHANNEL, .password = AP_WIFI_PASS, .max_connection = 4,
        .authmode = WIFI_AUTH_WPA2_PSK}};
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP)); ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&(esp_vfs_spiffs_conf_t){
        .base_path="/spiffs", .partition_label=NULL, .max_files=5, .format_if_mount_failed=true}));
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) { ESP_LOGE(TAG, "No se pudo crear mutex"); return; }
    start_http(); xTaskCreate(udp_task, "status_udp", 4096, NULL, 5, NULL);
    ble_service_start();
    ESP_LOGI(TAG, "AP listo en canal %d, puerto %d", AP_WIFI_CHANNEL, AP_SERVER_PORT);
}
