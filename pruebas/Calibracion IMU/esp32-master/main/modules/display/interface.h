#include <stdbool.h>
#include "tank_payload.h"
#include "imu_bno085.h"
#include "gps_module.h"
#include "lvgl.h"

void interface_init(lv_display_t *display);
void interface_update(const gps_module_snapshot_t *gps,
                      const imu_bno085_sample_t *front_imu,
                      const TankPayload *rear, bool rear_online);
#pragma once

