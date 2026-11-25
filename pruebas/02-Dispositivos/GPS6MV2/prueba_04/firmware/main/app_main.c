#include "esp_log.h"

#include "gps_bridge.h"
#include "imu_device.h"

static const char *TAG = "PRUEBA04";

// Inicia GPS e IMU para corregir velocidades GPS pequeñas cuando la IMU confirma reposo.
void app_main(void){
    ESP_LOGI(TAG, "Inicio de prueba 04: GPS + IMU; fusion de reposo; IMU a 5 Hz");
    gps_bridge_start();
    imu_device_start();
}
