#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct imu_bno085 *imu_bno085_handle_t;

// Configuración de un sensor: bus SPI, pines, frecuencia de reportes y nombre de log.
typedef struct {
    spi_host_device_t spi_host; // Host SPI reservado para este sensor.
    gpio_num_t mosi_gpio;       // Datos hacia el BNO085.
    gpio_num_t miso_gpio;       // Datos desde el BNO085.
    gpio_num_t sclk_gpio;       // Reloj SPI.
    gpio_num_t cs_gpio;         // Selección del dispositivo SPI.
    gpio_num_t int_gpio;        // Señal HINT/interrupción del sensor.
    gpio_num_t reset_gpio;      // Reinicio de hardware del sensor.
    uint32_t spi_clock_hz;      // Frecuencia del reloj SPI.
    uint32_t report_period_us;  // Periodo solicitado a cada reporte del BNO085.
    uint32_t service_period_ms; // Periodo de sondeo de la tarea lectora.
    bool install_isr_service;   // true si este driver debe instalar el servicio GPIO ISR.
    const char *name;           // Nombre usado para identificar logs y tarea.
} imu_bno085_config_t;

// Instantánea en unidades del driver: Euler en grados, aceleración m/s² y giro rad/s.
typedef struct {
    bool online;                    // Hay una muestra reciente del sensor.
    bool has_orientation;           // roll/pitch/yaw contienen datos recibidos.
    bool has_linear_acceleration;   // El vector de aceleración está disponible.
    bool has_gyro;                  // El vector de giro calibrado está disponible.
    float roll_deg;                 // Inclinación alrededor del eje X, grados.
    float pitch_deg;                // Inclinación alrededor del eje Y, grados.
    float yaw_deg;                  // Orientación alrededor del eje Z, grados.
    float linear_accel_m_s2[3];     // Aceleración lineal X/Y/Z, m/s².
    float gyro_rad_s[3];            // Velocidad angular X/Y/Z, rad/s.
    int64_t sample_time_us;         // Tiempo local de recepción desde el arranque, µs.
} imu_bno085_sample_t;

// NOTA: el driver C++ subyacente mantiene estado SH-2 global; no usar dos handles
// activos simultáneamente hasta cambiar o adaptar esa dependencia.
// Reserva el controlador y valida la configuración; todavía no activa el hardware.
esp_err_t imu_bno085_create(const imu_bno085_config_t *config, imu_bno085_handle_t *handle);
// Inicializa SPI/BNO085, habilita reportes y crea la tarea que publica muestras.
esp_err_t imu_bno085_start(imu_bno085_handle_t handle);
// Copia la última muestra de forma segura; false indica que no hay dato reciente.
bool imu_bno085_get_sample(imu_bno085_handle_t handle, imu_bno085_sample_t *sample);

#ifdef __cplusplus
}
#endif
