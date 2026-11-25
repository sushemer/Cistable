#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static const char *TAG = "SENSOR_HIDROSTATICO";

// --- CONFIGURACIÓN ---
#define ADC_UNIT           ADC_UNIT_1
#define ADC_CHANNEL        ADC_CHANNEL_6     // En ESP32-S3, ADC1_CH6 es el GPIO7
#define ADC_ATTEN          ADC_ATTEN_DB_12   // Lee hasta ~3.1V

#define RESISTOR_OHMS      150.0f            // Valor de tu resistencia en Ohms
#define SENSOR_MAX_METERS  3.0f              // Ajusta a 2.0f o 3.0f según tu modelo
#define CURRENT_MIN_MA     4.0f              // 4 mA = 0 metros
#define CURRENT_MAX_MA     20.0f             // 20 mA = Rango Máximo

static adc_oneshot_unit_handle_t adc_handle;
static adc_cali_handle_t adc_cali_handle = NULL;
static bool is_calibrated = false;

// Inicialización de la calibración interna del ADC
static bool init_adc_calibration(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle) {
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if CONFIG_IDF_TARGET_ESP32
    #if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = unit,
        .atten = atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
    if (ret == ESP_OK) calibrated = true;
    #endif
#else
    #if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = unit,
        .chan = channel,
        .atten = atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
    if (ret == ESP_OK) calibrated = true;
    #endif
#endif

    *out_handle = handle;
    return calibrated;
}

void app_main(void) {
    // 1. Configurar la unidad ADC
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    // 2. Configurar el canal ADC
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &config));

    // 3. Inicializar calibración efuse si está disponible
    is_calibrated = init_adc_calibration(ADC_UNIT, ADC_CHANNEL, ADC_ATTEN, &adc_cali_handle);
    if (is_calibrated) {
        ESP_LOGI(TAG, "ADC Calibrado correctamente.");
    } else {
        ESP_LOGW(TAG, "Calibración eFuse no disponible, usando conversión lineal simple.");
    }

    while (1) {
        int raw_val = 0;
        int voltage_mv = 0;

        // Leer ADC
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL, &raw_val));

        // Obtener el voltaje en mV
        if (is_calibrated) {
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc_cali_handle, raw_val, &voltage_mv));
        } else {
            voltage_mv = (raw_val * 3300) / 4095;
        }

        // Calcular Corriente (mA): I = V / R
        float current_ma = (float)voltage_mv / RESISTOR_OHMS;

        // Calcular Nivel de Agua (m)
        float water_level_m = 0.0f;
        if (current_ma >= CURRENT_MIN_MA) {
            water_level_m = ((current_ma - CURRENT_MIN_MA) / (CURRENT_MAX_MA - CURRENT_MIN_MA)) * SENSOR_MAX_METERS;
        } else {
            // Si detecta menos de 4mA, el cable está desconectado o el sensor apagado
            water_level_m = 0.0f;
        }

        ESP_LOGI(TAG, "RAW: %4d | Voltaje: %4d mV | Corriente: %5.2f mA | Nivel: %4.2f m",
                 raw_val, voltage_mv, current_ma, water_level_m);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}