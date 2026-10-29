#pragma once

#include <stdint.h>

typedef struct __attribute__((packed)) {
    float roll_1;                 // Bytes 0-3
    float pitch_1;                // Bytes 4-7
    float ay_1;                   // Bytes 8-11
    float water_level;            // Bytes 12-15
    uint8_t sensor_status_flags;  // Byte 16
} TankPayload;

#define STATUS_IMU1_OK  (1 << 0)
#define STATUS_RS485_OK (1 << 1)
