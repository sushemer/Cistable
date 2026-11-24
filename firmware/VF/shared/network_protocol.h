#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Configuracion central de la red Wi-Fi y del servidor AP. */
#define AP_WIFI_SSID                "Cisterna_LTR_AP"
#define AP_WIFI_PASS                "12345678"
#define AP_WIFI_CHANNEL             1
#define AP_SERVER_PORT              5000
#define AP_SEARCH_TIMEOUT_MS        10000

/* Intervalos de reporte y duraciones de ahorro de energia. */
#define REPORT_INTERVAL_FAST_MS     2000
#define REPORT_INTERVAL_MED_MS      5000
#define REPORT_INTERVAL_SLOW_MS     10000
#define DEFAULT_REPORT_INTERVAL_MS  REPORT_INTERVAL_FAST_MS

#define SLEEP_DURATION_SHORT_MS     10000
#define SLEEP_DURATION_MED_MS       15000
#define SLEEP_DURATION_LONG_MS      20000

/* Simulacion configurable del sensor de agua (desactivada por defecto).
 * Los perfiles Kconfig deben incluir CISTERNA_TEST_MODE / CISTERNA_TEST_VALUE
 * en MasterEsp y SlaveEsp. */
#ifndef CISTERNA_TEST_MODE
#define CISTERNA_TEST_MODE 0
#endif
#ifndef CISTERNA_TEST_VALUE
#define CISTERNA_TEST_VALUE 65.0f
#endif

#define NETWORK_HEADER_AP           0x3A
#define NETWORK_HEADER_NODE         0x7A
#define NETWORK_CMD_STATUS          0xB6
#define NETWORK_CMD_REPORT_RATE     0x18
#define NETWORK_CMD_ENABLE          0x4C
#define NETWORK_CMD_SLEEP           0x9E
#define NETWORK_CMD_TANK_CONFIG     0x2D
#define NETWORK_CMD_TANK_CONFIG_ACK 0x2E

#define NETWORK_PACKET_MAX_PAYLOAD 80U

#define NETWORK_NODE_MASTER         1
#define NETWORK_NODE_SLAVE          2

#define NETWORK_RISK_SAFE           0
#define NETWORK_RISK_CAUTION        1
#define NETWORK_RISK_CRITICAL       2

/* Banderas individuales de salud y estado de red. */
#define NETWORK_FLAG_AP_ONLINE       (1U << 0)
#define NETWORK_FLAG_IMU_OK          (1U << 1)
#define NETWORK_FLAG_GPS_OK          (1U << 2)
#define NETWORK_FLAG_SD_OK           (1U << 3)
#define NETWORK_FLAG_WATER_OK        (1U << 4)
#define NETWORK_FLAG_WATER_SIMULATED (1U << 5)
#define NETWORK_FLAG_REPORTING_ENABLED (1U << 6)
#define NETWORK_FLAG_CONFIG_VALID    (1U << 7)
#define NETWORK_FLAG_LTR_DEGRADED    (1U << 8)
#define NETWORK_FLAG_LONG_WAVE_RISK  (1U << 9)

#define NETWORK_TANK_CONFIG_VERSION  1U

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __attribute__((packed)) {
    uint8_t node_id;
    uint16_t flags;
    float roll_deg;
    float pitch_deg;
    union {
        float ay_ms2;
        float lateral_accel_m_s2;
    };
    union {
        float speed_kmh;
        float gps_speed_kmh;
    };
    float water_level;
    float ltr;
    float ltr_static;
    float ltr_dynamic;
    float bank_angle_deg;
    float suspension_roll_deg;
    uint8_t risk_level;
    uint16_t report_interval_ms;
} network_status_payload_t;

typedef struct __attribute__((packed)) {
    uint32_t config_version;
    float vehicle_track_m;
    float semi_axis_a_m;
    float semi_axis_b_m;
    float tank_length_m;
    float cg_height_empty_m;
    float cg_height_full_m;
    float full_tank_value;
    float liquid_density_kg_m3;
    float damping_ratio_lat;
    float damping_ratio_long;
    float slosh_enable_pct;
    float slosh_disable_low_pct;
    float slosh_disable_high_pct;
} network_tank_config_payload_t;

typedef struct __attribute__((packed)) {
    uint32_t config_version;
    uint8_t accepted;
    uint8_t source_node;
    uint16_t flags;
} network_tank_config_ack_payload_t;

typedef struct __attribute__((packed)) {
    uint8_t header;
    uint8_t command;
    uint8_t payload_length;
    uint8_t payload[sizeof(network_status_payload_t) > 64 ? sizeof(network_status_payload_t) : 64];
    uint8_t crc;
} network_packet_t;

static inline uint8_t network_crc8(const uint8_t *data, size_t length)
{
    uint8_t crc = 0;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
    }
    return crc;
}

static inline bool network_packet_expected_payload_length(uint8_t command,
                                                          uint8_t payload_length)
{
    size_t expected;
    switch (command) {
        case NETWORK_CMD_STATUS:
            expected = sizeof(network_status_payload_t);
            break;
        case NETWORK_CMD_TANK_CONFIG:
            expected = sizeof(network_tank_config_payload_t);
            break;
        case NETWORK_CMD_TANK_CONFIG_ACK:
            expected = sizeof(network_tank_config_ack_payload_t);
            break;
        case NETWORK_CMD_REPORT_RATE:
            expected = 5U;
            break;
        case NETWORK_CMD_ENABLE:
        case NETWORK_CMD_SLEEP:
            expected = 2U;
            break;
        default:
            return payload_length <= 64U;
    }
    return payload_length == expected;
}

static inline bool network_packet_encode(uint8_t header, uint8_t command,
                                         const void *payload, uint8_t payload_length,
                                         uint8_t *out, size_t out_size, size_t *written)
{
    const size_t total = (size_t)payload_length + 4U;
    if (out == NULL || written == NULL || (header != NETWORK_HEADER_AP &&
                                           header != NETWORK_HEADER_NODE) ||
        total > out_size || payload_length > 64U ||
        !network_packet_expected_payload_length(command, payload_length) ||
        (payload_length != 0U && payload == NULL)) {
        return false;
    }
    out[0] = header;
    out[1] = command;
    out[2] = payload_length;
    if (payload_length != 0U && payload != NULL) {
        memcpy(&out[3], payload, payload_length);
    }
    out[3U + payload_length] = network_crc8(out, 3U + payload_length);
    *written = total;
    return true;
}

static inline bool network_packet_validate(const uint8_t *packet, size_t length)
{
    if (packet == NULL || length < 4U ||
        (packet[0] != NETWORK_HEADER_AP && packet[0] != NETWORK_HEADER_NODE) ||
        packet[2] > 64U ||
        !network_packet_expected_payload_length(packet[1], packet[2]) ||
        length != (size_t)packet[2] + 4U) {
        return false;
    }
    return network_crc8(packet, length - 1U) == packet[length - 1U];
}

#ifdef __cplusplus
}
#endif
