#include "imu_device.h"

#include <inttypes.h>

#include "bno085.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"



static const char *TAG = "IMU";
static i2c_master_bus_handle_t bus_handle;
static bno085_handle_t imu_handle;

// Registra cada reporte con el tiempo monotónico de recepción y el timestamp SH-2.
static void sensor_callback( bno085_handle_t handle,const bno085_sensor_value_t *value,void *user_context){
    (void)handle;
    (void)user_context;
    if (value == NULL) {
        return;
    }

    const int64_t received_us = esp_timer_get_time(); // Tiempo monotónico local del ESP32.
    if (value->sensor_id == BNO085_SENSOR_LINEAR_ACCELERATION) {
        ESP_LOGI(TAG, "IMU|1|%" PRId64 "|%" PRIu64 "|LINEAR_ACCEL|%.5f|%.5f|%.5f|%u", received_us, value->timestamp_us, value->data.linear_acceleration.x, value->data.linear_acceleration.y, value->data.linear_acceleration.z, (unsigned)value->status);
    } else if (value->sensor_id == BNO085_SENSOR_GYROSCOPE_CALIBRATED) {
        ESP_LOGI(TAG, "IMU|1|%" PRId64 "|%" PRIu64 "|GYRO|%.5f|%.5f|%.5f|%u", received_us, value->timestamp_us, value->data.gyroscope.x, value->data.gyroscope.y, value->data.gyroscope.z, (unsigned)value->status);
    } else if (value->sensor_id == BNO085_SENSOR_ROTATION_VECTOR) {
        ESP_LOGI(TAG, "IMU|1|%" PRId64 "|%" PRIu64 "|ROTATION_QUAT|%.6f|%.6f|%.6f|%.6f|%u", received_us, value->timestamp_us, value->data.rotation_vector.i, value->data.rotation_vector.j, value->data.rotation_vector.k, value->data.rotation_vector.real, (unsigned)value->status);
    }
}

// Configura I2C, detecta la dirección del sensor y activa los tres reportes usados.
static esp_err_t imu_device_init(void){
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = IMU_I2C_PORT,
        .sda_io_num = IMU_SDA_GPIO,
        .scl_io_num = IMU_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&bus_config, &bus_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo crear el bus I2C: %s", esp_err_to_name(err));
        return err;
    }

    const uint8_t candidates[] = { IMU_I2C_ADDRESS, IMU_I2C_ALT_ADDR };
    uint8_t address = 0;
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        if (i2c_master_probe(bus_handle, candidates[i], pdMS_TO_TICKS(100)) == ESP_OK) {
            address = candidates[i];
            break;
        }
    }
    if (address == 0) {
        ESP_LOGE(TAG, "BNO085 no responde en 0x4A/0x4B; revise alimentación, I2C y modo de interfaz");
        return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGI(TAG, "BNO085 detectado en 0x%02X", address);

    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = IMU_I2C_HZ,
    };
    i2c_master_dev_handle_t device_handle;
    err = i2c_master_bus_add_device(bus_handle, &device_config, &device_handle);
    if (err != ESP_OK) {
        return err;
    }

    bno085_config_t config;
    bno085_config_default(&config);
    err = bno085_init(&config, device_handle, IMU_INT_GPIO, IMU_RST_GPIO, &imu_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo iniciar BNO085: %s", esp_err_to_name(err));
        return err;
    }
    err = bno085_register_sensor_callback(imu_handle, sensor_callback, NULL);
    if (err != ESP_OK) {
        return err;
    }

    const uint8_t reports[] = { BNO085_SENSOR_LINEAR_ACCELERATION, BNO085_SENSOR_GYROSCOPE_CALIBRATED, BNO085_SENSOR_ROTATION_VECTOR };
    for (size_t i = 0; i < sizeof(reports) / sizeof(reports[0]); ++i) {
        err = bno085_enable_sensor(imu_handle, reports[i], IMU_REPORT_PERIOD_US);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "No se pudo habilitar reporte IMU 0x%02X: %s",
                     reports[i], esp_err_to_name(err));
            return err;
        }
    }
    ESP_LOGI(TAG, "Lectura IMU habilitada; periodo solicitado=%u us", (unsigned)IMU_REPORT_PERIOD_US);
    return ESP_OK;
}

// Atiende periódicamente el componente; este servicio entrega los reportes al callback.
static void imu_task(void *argument){
    (void)argument;
    const esp_err_t err = imu_device_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Lectura IMU deshabilitada por error de inicialización");
        vTaskDelete(NULL);
        return;
    }
    while (true) {
        bno085_service(imu_handle);
        vTaskDelay(pdMS_TO_TICKS(IMU_SERVICE_PERIOD_MS));
    }
}

// Inicia la tarea de la IMU sin bloquear la tarea principal.
void imu_device_start(void){
    if (xTaskCreate(imu_task, "imu_reader", IMU_TASK_STACK_SIZE, NULL, IMU_TASK_PRIORITY, NULL) != pdPASS) {
        ESP_LOGE(TAG, "No se pudo crear la tarea de lectura IMU");
    }
}
