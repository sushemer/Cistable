#include "esp_err.h"

// Pines iniciales para DevKit V1
// Dirección I2C, tasas de adquisición y recursos de la tarea BNO085.
#define IMU_I2C_PORT          I2C_NUM_0      // Controlador I2C utilizado.
#define IMU_SDA_GPIO          GPIO_NUM_21    // GPIO de datos I2C para DevKit V1.
#define IMU_SCL_GPIO          GPIO_NUM_22    // GPIO de reloj I2C para DevKit V1.
#define IMU_INT_GPIO          GPIO_NUM_19    // GPIO conectado a INT de la BNO085.
#define IMU_RST_GPIO          GPIO_NUM_18    // GPIO conectado a RST de la BNO085.
#define IMU_I2C_ADDRESS       0x4A           // Dirección primaria de 7 bits.
#define IMU_I2C_ALT_ADDR      0x4B           // Dirección alternativa según AD0.
#define IMU_I2C_HZ            400000         // Frecuencia de bus I2C, en Hz.
#define IMU_REPORT_PERIOD_US  100000U        // Periodo solicitado para reportes: 10 Hz.
#define IMU_SERVICE_PERIOD_MS 10             // Periodo de atención del componente, en ms.
#define IMU_TASK_STACK_SIZE   6144           // Pila de la tarea IMU, en bytes.
#define IMU_TASK_PRIORITY     5              // Prioridad de la tarea IMU en FreeRTOS.

// Crea la tarea que inicializa la BNO085 y atiende sus reportes de sensores.
void imu_device_start(void);
