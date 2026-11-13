#include <stdbool.h>

#include "tank_payload.h"
#include "imu_bno085.h"
#include "gps_module.h"

typedef struct {
    float roll_deg;
    float lateral_accel_m_s2;
    float ltr;
    float water_level_cm;
    float water_gallons;
    float water_percent;
    float speed_kmh;
    bool imu_local_ok;
    bool imu_remote_ok;
    bool water_ok;
    bool rear_online;
    bool static_risk;
} display_telemetry_t;

void display_module_start(void);
void display_module_update(const gps_module_snapshot_t *gps,
                           const display_telemetry_t *telemetry);
#pragma once
