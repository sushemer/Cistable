#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "bno085.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/uart.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#define I2C_PORT I2C_NUM_0
#define SDA_GPIO 6
#define SCL_GPIO 7
#define IMU1_INT 5
#define IMU1_RST 4
#define IMU2_INT 15
#define IMU2_RST 16
#define IMU1_ADDR 0x4A
#define IMU2_ADDR 0x4B
#define REPORT_US 10000
#define LEVEL_UART UART_NUM_1
#define LEVEL_TX 17
#define LEVEL_RX 18
#define LEVEL_DE 8
#define LEVEL_BAUD 9600
#define LEVEL_ID 0x01
#define LEVEL_POLL_MS 500
#define LEVEL_STALE_MS (LEVEL_POLL_MS + 120)
#define STALE_MS 300
#define TX_PERIOD_MS 100
#define HEIGHT_M 1.8f
#define TRACK_M 2.2f
#define GRAVITY 9.81f
#define RAD_TO_DEG (180.0f / (float)M_PI)
#define FLAG_IMU1 (1U << 0)
#define FLAG_IMU2 (1U << 1)
#define FLAG_LEVEL (1U << 2)

static const char *TAG = "tank_node";
static const uint8_t cabin_mac[ESP_NOW_ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

typedef struct {
    float roll, pitch, ay, roll_zero, pitch_zero;
    bool tare_pending, tare_valid;
    int64_t last_orientation_us, last_acceleration_us;
} imu_state_t;

typedef struct {
    uint32_t sequence, timestamp_ms;
    uint8_t sensor_status_flags;
    uint8_t reserved[3];
    float imu1_roll_deg, imu1_pitch_deg, imu1_ay_mps2, imu1_ltr;
    float imu2_roll_deg, imu2_pitch_deg, imu2_ay_mps2, imu2_ltr;
    uint16_t water_level_raw, reserved_level;
} tank_payload_t;

static imu_state_t imu[2];
extern esp_err_t bno085_2_init(const bno085_config_t *, i2c_master_dev_handle_t, gpio_num_t, gpio_num_t, bno085_handle_t *);
extern void bno085_2_service(bno085_handle_t);
extern esp_err_t bno085_2_register_sensor_callback(bno085_handle_t, bno085_sensor_callback_t, void *);
extern esp_err_t bno085_2_enable_sensor(bno085_handle_t, uint8_t, uint32_t);

static float clampf_local(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
static float wrap_angle(float x) {
    while (x > (float)M_PI) x -= 2.0f * (float)M_PI;
    while (x < -(float)M_PI) x += 2.0f * (float)M_PI;
    return x;
}
static void quat_to_euler(float i, float j, float k, float w, float *roll, float *pitch) {
    *roll = atan2f(2.0f * (w * i + j * k), 1.0f - 2.0f * (i * i + j * j));
    *pitch = asinf(clampf_local(2.0f * (w * j - k * i), -1.0f, 1.0f));
}
static void imu_callback(bno085_handle_t handle, const bno085_sensor_value_t *value, void *context) {
    (void)handle;
    imu_state_t *state = context;
    if (!state || !value) return;
    int64_t now = esp_timer_get_time();
    if (value->sensor_id == BNO085_SENSOR_LINEAR_ACCELERATION) {
        state->ay = value->data.linear_acceleration.y;
        state->last_acceleration_us = now;
    } else if (value->sensor_id == BNO085_SENSOR_ROTATION_VECTOR) {
        float roll, pitch;
        quat_to_euler(value->data.rotation_vector.i, value->data.rotation_vector.j,
                      value->data.rotation_vector.k, value->data.rotation_vector.real, &roll, &pitch);
        if (state->tare_pending) {
            state->roll_zero = roll;
            state->pitch_zero = pitch;
            state->tare_pending = false;
            state->tare_valid = true;
        }
        state->roll = state->tare_valid ? wrap_angle(roll - state->roll_zero) : roll;
        state->pitch = state->tare_valid ? wrap_angle(pitch - state->pitch_zero) : pitch;
        state->last_orientation_us = now;
    }
}
static float ltr(const imu_state_t *state) {
    float effective_ay = state->ay * cosf(state->roll) + GRAVITY * sinf(state->roll);
    return clampf_local(2.0f * HEIGHT_M * effective_ay / (TRACK_M * GRAVITY), -1.0f, 1.0f);
}
static uint16_t crc16(const uint8_t *data, size_t length) {
    uint16_t crc = 0xffff;
    for (size_t n = 0; n < length; n++) {
        crc ^= data[n];
        for (uint8_t bit = 0; bit < 8; bit++) crc = (crc & 1) ? (crc >> 1) ^ 0xa001 : crc >> 1;
    }
    return crc;
}
static esp_err_t level_init(void) {
    const uart_config_t config = {.baud_rate = LEVEL_BAUD, .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE, .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, .source_clk = UART_SCLK_DEFAULT};
    esp_err_t err = uart_param_config(LEVEL_UART, &config);
    if (err != ESP_OK) return err;
    err = uart_set_pin(LEVEL_UART, LEVEL_TX, LEVEL_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) return err;
    err = uart_driver_install(LEVEL_UART, 256, 0, 0, NULL, 0);
    return err == ESP_ERR_INVALID_STATE ? ESP_OK : err;
}
static void level_request(void) {
    uint8_t request[] = {LEVEL_ID, 0x03, 0x00, 0x00, 0x00, 0x01, 0, 0};
    uint16_t crc = crc16(request, 6);
    request[6] = crc & 0xff;
    request[7] = crc >> 8;
    uart_flush_input(LEVEL_UART);
    gpio_set_level(LEVEL_DE, 1);
    (void)uart_write_bytes(LEVEL_UART, request, sizeof(request));
    (void)uart_wait_tx_done(LEVEL_UART, pdMS_TO_TICKS(20));
    gpio_set_level(LEVEL_DE, 0);
}
static bool level_read(uint16_t *value) {
    static uint8_t frame[7];
    static size_t used;
    uint8_t data[32];
    int count = uart_read_bytes(LEVEL_UART, data, sizeof(data), 0);
    for (int n = 0; n < count; n++) {
        if (used < sizeof(frame)) frame[used++] = data[n];
        if (used == sizeof(frame)) {
            uint16_t crc = crc16(frame, 5);
            bool ok = frame[0] == LEVEL_ID && frame[1] == 3 && frame[2] == 2 &&
                      frame[5] == (crc & 0xff) && frame[6] == (crc >> 8);
            if (ok) *value = ((uint16_t)frame[3] << 8) | frame[4];
            used = 0;
            if (ok) return true;
        }
    }
    return false;
}
static void radio_init(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        (void)nvs_flash_erase();
        (void)nvs_flash_init();
    }
    (void)esp_netif_init();
    (void)esp_event_loop_create_default();
    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&wifi_config) != ESP_OK) return;
    (void)esp_wifi_set_mode(WIFI_MODE_STA);
    (void)esp_wifi_start();
    if (esp_now_init() != ESP_OK) return;
    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, cabin_mac, ESP_NOW_ETH_ALEN);
    peer.ifidx = WIFI_IF_STA;
    (void)esp_now_add_peer(&peer);
}
static void poll_tare(void) {
    int c = getchar();
    if (c == 't' || c == 'T') imu[0].tare_pending = imu[1].tare_pending = true;
    if (errno != EAGAIN && errno != EWOULDBLOCK) clearerr(stdin);
}
void app_main(void) {
    gpio_config_t de = {.pin_bit_mask = 1ULL << LEVEL_DE, .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE, .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    (void)gpio_config(&de);
    if (level_init() != ESP_OK) ESP_LOGE(TAG, "UART RS485 no disponible");
    radio_init();
    i2c_master_bus_config_t bus_config = {.i2c_port = I2C_PORT, .sda_io_num = SDA_GPIO,
        .scl_io_num = SCL_GPIO, .clk_source = I2C_CLK_SRC_DEFAULT, .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true};
    i2c_master_bus_handle_t bus = NULL;
    if (i2c_new_master_bus(&bus_config, &bus) != ESP_OK) ESP_LOGE(TAG, "Bus I2C no disponible");
    bno085_handle_t handles[2] = {NULL, NULL};
    const uint8_t addresses[2] = {IMU1_ADDR, IMU2_ADDR};
    const gpio_num_t ints[2] = {IMU1_INT, IMU2_INT};
    const gpio_num_t resets[2] = {IMU1_RST, IMU2_RST};
    for (size_t n = 0; bus && n < 2; n++) {
        if (i2c_master_probe(bus, addresses[n], pdMS_TO_TICKS(20)) != ESP_OK) {
            ESP_LOGW(TAG, "IMU %u ausente", (unsigned)(n + 1));
            continue;
        }
        i2c_device_config_t device_config = {.dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addresses[n], .scl_speed_hz = 400000};
        i2c_master_dev_handle_t device = NULL;
        if (i2c_master_bus_add_device(bus, &device_config, &device) != ESP_OK) continue;
        bno085_config_t config;
        bno085_config_default(&config);
        config.i2c_timeout_ms = 20;
        esp_err_t err = n == 0 ? bno085_init(&config, device, ints[n], resets[n], &handles[n])
                               : bno085_2_init(&config, device, ints[n], resets[n], &handles[n]);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "IMU %u no inicializada: %s", (unsigned)(n + 1), esp_err_to_name(err));
            continue;
        }
        if (n == 0) {
            (void)bno085_register_sensor_callback(handles[n], imu_callback, &imu[n]);
            (void)bno085_enable_sensor(handles[n], BNO085_SENSOR_ROTATION_VECTOR, REPORT_US);
            (void)bno085_enable_sensor(handles[n], BNO085_SENSOR_LINEAR_ACCELERATION, REPORT_US);
        } else {
            (void)bno085_2_register_sensor_callback(handles[n], imu_callback, &imu[n]);
            (void)bno085_2_enable_sensor(handles[n], BNO085_SENSOR_ROTATION_VECTOR, REPORT_US);
            (void)bno085_2_enable_sensor(handles[n], BNO085_SENSOR_LINEAR_ACCELERATION, REPORT_US);
        }
    }
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags >= 0) (void)fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    uint32_t sequence = 0;
    uint16_t water_level = 0;
    int64_t next_request = 0, next_tx = 0, last_level_ok = 0;
    while (true) {
        int64_t now = esp_timer_get_time();
        if (handles[0]) bno085_service(handles[0]);
        if (handles[1]) bno085_2_service(handles[1]);
        if (now >= next_request) { level_request(); next_request = now + LEVEL_POLL_MS * 1000LL; }
        uint16_t received;
        if (level_read(&received)) { water_level = received; last_level_ok = now; }
        poll_tare();
        if (now >= next_tx) {
            tank_payload_t payload = {0};
            payload.sequence = sequence++;
            payload.timestamp_ms = (uint32_t)(now / 1000);
            bool ok1 = imu[0].last_orientation_us > 0 && now - imu[0].last_orientation_us < STALE_MS * 1000LL &&
                       now - imu[0].last_acceleration_us < STALE_MS * 1000LL;
            bool ok2 = imu[1].last_orientation_us > 0 && now - imu[1].last_orientation_us < STALE_MS * 1000LL &&
                       now - imu[1].last_acceleration_us < STALE_MS * 1000LL;
            if (ok1) payload.sensor_status_flags |= FLAG_IMU1;
            if (ok2) payload.sensor_status_flags |= FLAG_IMU2;
            if (last_level_ok > 0 && now - last_level_ok < LEVEL_STALE_MS * 1000LL) payload.sensor_status_flags |= FLAG_LEVEL;
            payload.imu1_roll_deg = imu[0].roll * RAD_TO_DEG;
            payload.imu1_pitch_deg = imu[0].pitch * RAD_TO_DEG;
            payload.imu1_ay_mps2 = imu[0].ay;
            payload.imu1_ltr = ltr(&imu[0]);
            payload.imu2_roll_deg = imu[1].roll * RAD_TO_DEG;
            payload.imu2_pitch_deg = imu[1].pitch * RAD_TO_DEG;
            payload.imu2_ay_mps2 = imu[1].ay;
            payload.imu2_ltr = ltr(&imu[1]);
            payload.water_level_raw = water_level;
            (void)esp_now_send(cabin_mac, (const uint8_t *)&payload, sizeof(payload));
            next_tx = now + TX_PERIOD_MS * 1000LL;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
