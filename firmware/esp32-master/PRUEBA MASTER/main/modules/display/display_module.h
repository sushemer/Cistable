#include <stdbool.h>

#include "tank_payload.h"
#include "imu_bno085.h"
#include "gps_module.h"

// Inicializa la ST7796S y crea la vista de telemetria funcional (sin LTR).
void display_module_start(void);
// Actualiza la interfaz con los ultimos datos disponibles del master.
void display_module_update(const gps_module_snapshot_t *gps,
                           const imu_bno085_sample_t *front_imu,
                           const TankPayload *rear, bool rear_online);
#pragma once

