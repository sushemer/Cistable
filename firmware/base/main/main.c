#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "LTR_SYSTEM";

// =============================================================================
// 1. MACROS DE LA CISTERNA Y SENSOR DE PRESIÓN
// =============================================================================
#define TANK_LENGTH_M          5.00f   // Longitud total del tanque (m)
#define TANK_HEIGHT_M          1.80f   // Altura del tanque (m)
#define TANK_WIDTH_M           2.20f   // Ancho del tanque (m)
#define SENSOR_PRESSURE_KPA    11.0f   // Presión medida en el fondo (kPa)

// =============================================================================
// 2. PARÁMETROS FÍSICOS DEL VEHÍCULO Y LÍMITES
// =============================================================================
#define GRAVITY_MPS2           9.81f
#define WATER_DENSITY_KG_M3    1000.0f // Densidad del agua (kg/m3)
#define TRUCK_TRACK_M          2.20f   // Ancho de vía T (distancia entre ruedas) (m)
#define MASS_EMPTY_KG          8000.0f // Masa fija m1 (chasis + tanque vacío) (kg)
#define HEIGHT_CG1_M           1.20f   // Altura CG de la masa fija h_CG1 (m)
#define HEIGHT_CG2_M           1.80f   // Altura CG inicial del líquido h_CG2 (m)[cite: 1]
#define MAX_SLOSH_OFFSET_M     0.45f   // Desplazamiento máximo lateral por ola delta_x (m)[cite: 1]

// Umbrales de alerta LTR
#define LTR_WARNING_THRESHOLD  0.60f   // Límite de advertencia
#define LTR_CRITICAL_THRESHOLD 0.75f   // Límite crítico de vuelco[cite: 2]

#define DEG_TO_RAD             (3.14159265f / 180.0f)

// =============================================================================
// ESTRUCTURAS DE DATOS
// =============================================================================
typedef struct {
    float roll_rad;            // Ángulo de inclinación lateral (phi)[cite: 2]
    float pitch_rad;
    float accel_y_mps2;        // Aceleración lateral (ay)[cite: 1]
    bool is_valid;
} imu_sensor_data_t;

typedef struct {
    float speed_m_s;           // Velocidad v (m/s)[cite: 2]
    bool is_valid;
} gps_sensor_data_t;

typedef struct {
    float pressure_kpa;        // Presión leída
    float liquid_height_m;     // Altura de agua
    float fill_percentage;     // Porcentaje de llenado (eta) (%)[cite: 2]
    float volume_m3;           // Volumen actual
    float mass_kg;             // Masa del líquido m2 (kg)[cite: 1]
} water_tank_state_t;

typedef struct {
    float ltr_value;           // Load Transfer Ratio[cite: 1]
    float liquid_mass_kg;      // Masa m2[cite: 1]
    float delta_x_m;           // Desplazamiento lateral del CG del agua[cite: 1]
    float accel_y_effective;   // Aceleración lateral corregida por inclinación
    uint8_t risk_level;        // 0: Seguro, 1: Advertencia, 2: Crítico
} ltr_result_t;

// =============================================================================
// FUNCIONES DE LECTURA SIMULADA (PROTOTIPOS / MOCK READINGS)
// =============================================================================
imu_sensor_data_t mock_read_imu_cabin(void) {
    imu_sensor_data_t data = { .roll_rad = 3.5f * DEG_TO_RAD, .pitch_rad = 0.5f * DEG_TO_RAD, .accel_y_mps2 = 2.2f, .is_valid = true };
    return data;
}

imu_sensor_data_t mock_read_imu_rear_left(void) {
    imu_sensor_data_t data = { .roll_rad = 4.1f * DEG_TO_RAD, .pitch_rad = 0.4f * DEG_TO_RAD, .accel_y_mps2 = 2.4f, .is_valid = true };
    return data;
}

imu_sensor_data_t mock_read_imu_rear_right(void) {
    imu_sensor_data_t data = { .roll_rad = 3.9f * DEG_TO_RAD, .pitch_rad = 0.4f * DEG_TO_RAD, .accel_y_mps2 = 2.3f, .is_valid = true };
    return data;
}

gps_sensor_data_t mock_read_gps(void) {
    gps_sensor_data_t data = { .speed_m_s = 16.67f, .is_valid = true }; // ~60 km/h
    return data;
}

float mock_read_pressure_sensor_kpa(void) {
    return SENSOR_PRESSURE_KPA;
}

// =============================================================================
// 1. CÁLCULO DE NIVELES Y MASA A PARTIR DE PRESIÓN Y DIMENSIONES
// =============================================================================
water_tank_state_t calculate_water_from_pressure(void) {
    water_tank_state_t state = {0};

    state.pressure_kpa = mock_read_pressure_sensor_kpa();
    float pressure_pa = state.pressure_kpa * 1000.0f;

    // Altura de agua: h = P / (rho * g)
    state.liquid_height_m = pressure_pa / (WATER_DENSITY_KG_M3 * GRAVITY_MPS2);

    if (state.liquid_height_m > TANK_HEIGHT_M) {
        state.liquid_height_m = TANK_HEIGHT_M;
    } else if (state.liquid_height_m < 0.0f) {
        state.liquid_height_m = 0.0f;
    }

    // Porcentaje de llenado eta (%)[cite: 2]
    state.fill_percentage = (state.liquid_height_m / TANK_HEIGHT_M) * 100.0f;

    // Volumen (sección elíptica estándar)
    float max_volume_m3 = (3.14159265f / 4.0f) * TANK_WIDTH_M * TANK_HEIGHT_M * TANK_LENGTH_M;
    state.volume_m3 = max_volume_m3 * (state.fill_percentage / 100.0f);

    // Masa del líquido m2 (kg)[cite: 1]
    state.mass_kg = state.volume_m3 * WATER_DENSITY_KG_M3;

    return state;
}

// =============================================================================
// 2. CÁLCULO DEL RIESGO DE VUELCO (LTR) Y MOMENTO DE VUELCO
// =============================================================================
ltr_result_t calculate_rollover_risk(const imu_sensor_data_t *imu_cab,
                                     const imu_sensor_data_t *imu_rl,
                                     const imu_sensor_data_t *imu_rr,
                                     const gps_sensor_data_t *gps,
                                     const water_tank_state_t *water) 
{
    ltr_result_t result = {0};

    // 1. Promedio de inclinación (roll) y aceleración lateral (ay) de los 3 IMUs[cite: 1, 2]
    float avg_accel_y = (imu_cab->accel_y_mps2 + imu_rl->accel_y_mps2 + imu_rr->accel_y_mps2) / 3.0f;
    float avg_roll_rad = (imu_cab->roll_rad + imu_rl->roll_rad + imu_rr->roll_rad) / 3.0f;

    // 2. Aceleración lateral efectiva ay_eff = ay * cos(phi) + g * sin(phi)[cite: 1, 2]
    float accel_y_effective = avg_accel_y * cosf(avg_roll_rad) + GRAVITY_MPS2 * sinf(avg_roll_rad);

    // 3. Masa del líquido m2 obtenida del sensor de agua[cite: 1]
    float liquid_mass_m2 = water->mass_kg;
    float total_mass = MASS_EMPTY_KG + liquid_mass_m2;

    // 4. Desplazamiento del centro de gravedad del agua (delta_x) por sloshing[cite: 1, 2]
    // Efecto máximo al 50% de llenado eta[cite: 2]
    float fill_fraction = water->fill_percentage / 100.0f;
    float slosh_factor = 4.0f * fill_fraction * (1.0f - fill_fraction);
    float delta_x = MAX_SLOSH_OFFSET_M * slosh_factor * (accel_y_effective / GRAVITY_MPS2); //[cite: 1]

    // 5. Cálculo del Momento de Vuelco M_vuelco[cite: 1]
    // M_vuelco = (m1 * h_CG1 + m2 * h_CG2) * ay_eff + m2 * g * delta_x[cite: 1]
    float moment_fixed_mass = MASS_EMPTY_KG * HEIGHT_CG1_M * accel_y_effective;             //[cite: 1]
    float moment_liquid_mass = liquid_mass_m2 * (HEIGHT_CG2_M * accel_y_effective + GRAVITY_MPS2 * delta_x); //[cite: 1]
    float M_vuelco = moment_fixed_mass + moment_liquid_mass;                                 //[cite: 1]

    // 6. Cálculo del LTR[cite: 1]
    // LTR = (2 * M_vuelco) / (m_total * g * T)[cite: 1]
    float total_weight_force = total_mass * GRAVITY_MPS2;
    float ltr_calculated = (2.0f * fabsf(M_vuelco)) / (total_weight_force * TRUCK_TRACK_M);  //[cite: 1]

    if (ltr_calculated > 1.0f) ltr_calculated = 1.0f;

    // Guardar resultados
    result.ltr_value = ltr_calculated;
    result.liquid_mass_kg = liquid_mass_m2;
    result.delta_x_m = delta_x;
    result.accel_y_effective = accel_y_effective;

    // Clasificación de severidad[cite: 2]
    if (ltr_calculated >= LTR_CRITICAL_THRESHOLD) {
        result.risk_level = 2; // RIESGO CRÍTICO[cite: 2]
    } else if (ltr_calculated >= LTR_WARNING_THRESHOLD) {
        result.risk_level = 1; // ADVERTENCIA[cite: 2]
    } else {
        result.risk_level = 0; // SEGURO
    }

    return result;
}

// =============================================================================
// TAREA MONITORA (FreeRTOS Task)
// =============================================================================
void monitoring_task(void *pvParameters) {
    while (1) {
        // Lecturas simuladas de sensores
        imu_sensor_data_t imu_cab = mock_read_imu_cabin();
        imu_sensor_data_t imu_rl  = mock_read_imu_rear_left();
        imu_sensor_data_t imu_rr  = mock_read_imu_rear_right();
        gps_sensor_data_t gps     = mock_read_gps();

        // 1. Calcular estado del agua a partir de los macros y la presión del sensor
        water_tank_state_t water = calculate_water_from_pressure();

        // 2. Calcular riesgo de vuelco e indicador LTR[cite: 1, 2]
        ltr_result_t risk = calculate_rollover_risk(&imu_cab, &imu_rl, &imu_rr, &gps, &water);

        // Imprimir telemetría
        ESP_LOGI(TAG, "Agua: Altura=%.2fm | Llenado=%.1f%% | Masa Liq=%.0f kg",
                 water.liquid_height_m, water.fill_percentage, risk.liquid_mass_kg);
        ESP_LOGI(TAG, "Dinámica: Ay_eff=%.2f m/s2 | Slosh Offset (delta_x)=%.3f m",
                 risk.accel_y_effective, risk.delta_x_m);

        if (risk.risk_level == 2) {
            ESP_LOGE(TAG, "!!! ALERTA ROJA: RIESGO DE VUELCO INMINENTE !!! LTR = %.3f", risk.ltr_value); //[cite: 1, 2]
        } else if (risk.risk_level == 1) {
            ESP_LOGW(TAG, "¡PRECAUCIÓN!: Riesgo elevado de volcadura. LTR = %.3f", risk.ltr_value);     //[cite: 1, 2]
        } else {
            ESP_LOGI(TAG, "Estado Normal. LTR = %.3f", risk.ltr_value);                                 //[cite: 1]
        }

        vTaskDelay(pdMS_TO_TICKS(200)); // Bucle de ejecución cada 200 ms (5 Hz)
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Iniciando Sistema de Predicción de Vuelco LTR...");
    xTaskCreate(monitoring_task, "monitoring_task", 4096, NULL, 5, NULL);
}