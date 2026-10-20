#pragma once

#include <stdint.h>

typedef struct __attribute__((packed)) {
    float roll_1;                 // Bytes 0-3
    float pitch_1;                // Bytes 4-7
    float ay_1;                   // Bytes 8-11
    float roll_2;                 // Bytes 12-15
    float pitch_2;                // Bytes 16-19
    float ay_2;                   // Bytes 20-23
    float water_level;            // Bytes 24-27
    uint8_t sensor_status_flags;  // Byte 28
} TankPayload;

#define STATUS_IMU1_OK  (1 << 0)
#define STATUS_IMU2_OK  (1 << 1)
#define STATUS_RS485_OK (1 << 2)
