#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>

#include "bno085.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BNO085_I2C_PORT          I2C_NUM_0
#define BNO085_SDA_GPIO         6
#define BNO085_SCL_GPIO         7
#define BNO085_INT_GPIO         5
#define BNO085_RST_GPIO         4
#define BNO085_I2C_ADDRESS      0x4A
#define BNO085_ALT_I2C_ADDRESS  0x4B
#define BNO085_I2C_FREQUENCY_HZ 400000
#define BNO085_REPORT_PERIOD_US 10000

#define TRUCK_HEIGHT_M          1.8f
#define TRUCK_TRACK_M           2.2f
#define GRAVITY_MPS2            9.81f
#define LTR_WARNING_LIMIT       0.6f

#define RAD_TO_DEG               (180.0f / (float)M_PI)
#define DEG_TO_RAD               ((float)M_PI / 180.0f)

static const char *TAG = "bno085_ltr";

typedef struct {
	float roll_rad;
	float pitch_rad;
	float lateral_acceleration;
	float roll_offset_rad;
	float pitch_offset_rad;
	bool tare_pending;
	bool tare_valid;
} ltr_state_t;

static ltr_state_t ltr_state;

static float clamp_float(float value, float minimum, float maximum)
{
	if (value < minimum) {
		return minimum;
	}
	if (value > maximum) {
		return maximum;
	}
	return value;
}

static float wrap_angle_rad(float angle)
{
	while (angle > (float)M_PI) {
		angle -= 2.0f * (float)M_PI;
	}
	while (angle < -(float)M_PI) {
		angle += 2.0f * (float)M_PI;
	}
	return angle;
}

static void quaternion_to_euler(float i, float j, float k, float real,
								float *roll_rad, float *pitch_rad)
{
	const float sin_roll = 2.0f * (real * i + j * k);
	const float cos_roll = 1.0f - 2.0f * (i * i + j * j);
	const float sin_pitch = clamp_float(2.0f * (real * j - k * i), -1.0f, 1.0f);

	*roll_rad = atan2f(sin_roll, cos_roll);
	*pitch_rad = asinf(sin_pitch);
}

static void sensor_callback(bno085_handle_t handle,
							const bno085_sensor_value_t *value,
							void *user_context)
{
	(void)handle;
	(void)user_context;

	if (value == NULL) {
		return;
	}

	if (value->sensor_id == BNO085_SENSOR_LINEAR_ACCELERATION) {
		ltr_state.lateral_acceleration = value->data.linear_acceleration.y;
		return;
	}

	if (value->sensor_id != BNO085_SENSOR_ROTATION_VECTOR) {
		return;
	}

	float raw_roll_rad;
	float raw_pitch_rad;
	quaternion_to_euler(value->data.rotation_vector.i,
						value->data.rotation_vector.j,
						value->data.rotation_vector.k,
						value->data.rotation_vector.real,
						&raw_roll_rad, &raw_pitch_rad);

	if (ltr_state.tare_pending) {
		ltr_state.roll_offset_rad = raw_roll_rad;
		ltr_state.pitch_offset_rad = raw_pitch_rad;
		ltr_state.tare_pending = false;
		ltr_state.tare_valid = true;
		ESP_LOGI(TAG, "Tare aplicado");
	}

	ltr_state.roll_rad = ltr_state.tare_valid
		? wrap_angle_rad(raw_roll_rad - ltr_state.roll_offset_rad)
		: raw_roll_rad;
	ltr_state.pitch_rad = ltr_state.tare_valid
		? wrap_angle_rad(raw_pitch_rad - ltr_state.pitch_offset_rad)
		: raw_pitch_rad;

	const float effective_lateral_acceleration =
		ltr_state.lateral_acceleration * cosf(ltr_state.roll_rad) +
		GRAVITY_MPS2 * sinf(ltr_state.roll_rad);
	const float ltr = clamp_float(
		(2.0f * TRUCK_HEIGHT_M * effective_lateral_acceleration) /
		(TRUCK_TRACK_M * GRAVITY_MPS2), -1.0f, 1.0f);

	printf("%.3f,%.3f,%.3f,%.3f\n",
		   ltr_state.roll_rad * RAD_TO_DEG,
		   ltr_state.pitch_rad * RAD_TO_DEG,
		   ltr_state.lateral_acceleration,
		   ltr);
	fflush(stdout);

	if (fabsf(ltr) > LTR_WARNING_LIMIT) {
		ESP_LOGW(TAG, "Riesgo de volcadura: LTR=%.3f", ltr);
	}
}

static void poll_tare_command(void)
{
	int character = getchar();
	if (character == 't' || character == 'T') {
		ltr_state.tare_pending = true;
		ESP_LOGI(TAG, "Tare solicitado; esperando el siguiente reporte");
	}
}

void app_main(void)
{
	i2c_master_bus_config_t bus_config = {
		.i2c_port = BNO085_I2C_PORT,
		.sda_io_num = BNO085_SDA_GPIO,
		.scl_io_num = BNO085_SCL_GPIO,
		.clk_source = I2C_CLK_SRC_DEFAULT,
		.glitch_ignore_cnt = 7,
		.flags.enable_internal_pullup = true,
	};
	i2c_master_bus_handle_t bus_handle;
	ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));

	uint8_t detected_address = 0;
	const uint8_t candidate_addresses[] = {
		BNO085_I2C_ADDRESS,
		BNO085_ALT_I2C_ADDRESS,
	};
	for (size_t index = 0; index < sizeof(candidate_addresses); ++index) {
		if (i2c_master_probe(bus_handle, candidate_addresses[index],
							 pdMS_TO_TICKS(100)) == ESP_OK) {
			detected_address = candidate_addresses[index];
			break;
		}
	}
	if (detected_address == 0) {
		ESP_LOGE(TAG, "No hay respuesta I2C en 0x4A ni 0x4B. Revise VCC/GND, pull-ups, PS0/PS1, AD0 y SDA/SCL (GPIO %d/%d)",
				 BNO085_SDA_GPIO, BNO085_SCL_GPIO);
		return;
	}
	ESP_LOGI(TAG, "Dispositivo I2C detectado en 0x%02X", detected_address);

	i2c_device_config_t device_config = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = detected_address,
		.scl_speed_hz = BNO085_I2C_FREQUENCY_HZ,
	};
	i2c_master_dev_handle_t device_handle;
	ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &device_config,
											  &device_handle));

	bno085_config_t bno085_config;
	bno085_config_default(&bno085_config);

	bno085_handle_t sensor_handle;
	esp_err_t init_err = bno085_init(&bno085_config, device_handle,
									 BNO085_INT_GPIO, BNO085_RST_GPIO,
									 &sensor_handle);
	if (init_err != ESP_OK) {
		ESP_LOGE(TAG, "No se pudo inicializar BNO085: %s", esp_err_to_name(init_err));
		ESP_LOGE(TAG, "RST GPIO %d queda en nivel %d; debe subir a 1 cuando el sensor no esta en reset",
				 BNO085_RST_GPIO, gpio_get_level(BNO085_RST_GPIO));
		return;
	}
	ESP_ERROR_CHECK(bno085_register_sensor_callback(sensor_handle,
													sensor_callback, NULL));
	ESP_ERROR_CHECK(bno085_enable_sensor(sensor_handle,
										 BNO085_SENSOR_ROTATION_VECTOR,
										 BNO085_REPORT_PERIOD_US));
	ESP_ERROR_CHECK(bno085_enable_sensor(sensor_handle,
										 BNO085_SENSOR_LINEAR_ACCELERATION,
										 BNO085_REPORT_PERIOD_US));

	const int stdin_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
	if (stdin_flags >= 0) {
		(void)fcntl(STDIN_FILENO, F_SETFL, stdin_flags | O_NONBLOCK);
	} else {
		ESP_LOGW(TAG, "No se pudo configurar UART para lectura no bloqueante");
	}

	printf("Roll, Pitch, Ay, LTR_Calculado\n");
	ESP_LOGI(TAG, "BNO085 listo; presione 't' para tare");

	while (true) {
		bno085_service(sensor_handle);
		poll_tare_command();
		if (errno != EAGAIN && errno != EWOULDBLOCK) {
			clearerr(stdin);
		}
		vTaskDelay(pdMS_TO_TICKS(10));
	}
}
