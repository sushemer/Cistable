#include <stdbool.h>
#include "gps_module.h"
#include "display_module.h"
#include "lvgl.h"

void interface_init(lv_display_t *display);
void interface_poll_truck_button(void);
void interface_update(const gps_module_snapshot_t *gps,
                      const display_telemetry_t *telemetry);
#pragma once
