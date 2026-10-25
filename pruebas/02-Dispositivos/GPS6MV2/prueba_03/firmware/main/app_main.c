#include "esp_log.h"

#include "gps_bridge.h"
#include "imu_device.h"

static const char *TAG = "PRUEBA03";

// Inicia GPS e IMU en tareas independientes para registrar ambas señales en paralelo.
void app_main(void){
    ESP_LOGI(TAG, "Inicio de prueba 03: GPS + IMU; muestras combinadas cada 500 ms; sin alarmas");
    gps_bridge_start();
    imu_device_start();
}
