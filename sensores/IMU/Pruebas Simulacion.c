#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define SAMPLE_RATE_HZ 100
#define SAMPLE_PERIOD_MS (1000 / SAMPLE_RATE_HZ)
#define IMU_COUNT 3
#define LTR_RISK_LIMIT 0.6f
#define PI_F 3.14159265358979323846f

static const char *TAG = "imu_ltr";

/* Parámetros ficticios del camión para esta simulación. */
static const float GRAVITY_MS2 = 9.80665f;
static const float CG_HEIGHT_M = 1.8f;
static const float TRACK_WIDTH_M = 2.2f;

typedef enum {
	IMU_CABINA = 0,
	IMU_TANQUE_IZQ,
	IMU_TANQUE_DER
} imu_id_t;

typedef struct {
	float roll_deg;
	float lateral_accel_ms2;
} imu_reading_t;

typedef struct {
	uint32_t timestamp_ms;
	imu_reading_t imu[IMU_COUNT];
} imu_packet_t;

typedef struct {
	float estimate;
	float error_covariance;
	float process_noise;
	float measurement_noise;
	bool initialized;
} kalman_1d_t;

static QueueHandle_t imu_queue;

static float uniform_random(void)
{
	return ((float)esp_random() + 1.0f) / ((float)UINT32_MAX + 2.0f);
}

/* Box-Muller: convierte dos uniformes en una muestra gaussiana N(0, 1). */
static float gaussian_noise(float standard_deviation)
{
	const float radius = sqrtf(-2.0f * logf(uniform_random()));
	const float angle = 2.0f * PI_F * uniform_random();
	return standard_deviation * radius * cosf(angle);
}

static float kalman_update(kalman_1d_t *filter, float measurement)
{
	if (!filter->initialized) {
		filter->estimate = measurement;
		filter->initialized = true;
		return filter->estimate;
	}

	/*
	 * Predicción: la estimación permanece constante y su incertidumbre crece.
	 * Corrección: K pondera la medición según las incertidumbres del modelo y
	 * del sensor. Un ruido de medición alto hace que el filtro confíe más en
	 * la estimación anterior.
	 */
	filter->error_covariance += filter->process_noise;
	const float kalman_gain = filter->error_covariance /
							  (filter->error_covariance + filter->measurement_noise);
	filter->estimate += kalman_gain * (measurement - filter->estimate);
	filter->error_covariance *= (1.0f - kalman_gain);
	return filter->estimate;
}

static kalman_1d_t make_kalman(float process_noise, float measurement_noise)
{
	return (kalman_1d_t) {
		.estimate = 0.0f,
		.error_covariance = 1.0f,
		.process_noise = process_noise,
		.measurement_noise = measurement_noise,
		.initialized = false
	};
}

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

static float calculate_ltr(const imu_reading_t filtered[IMU_COUNT])
{
	float mean_roll_rad = 0.0f;
	float mean_lateral_accel = 0.0f;

	for (size_t index = 0; index < IMU_COUNT; ++index) {
		mean_roll_rad += filtered[index].roll_deg * (PI_F / 180.0f);
		mean_lateral_accel += filtered[index].lateral_accel_ms2;
	}
	mean_roll_rad /= IMU_COUNT;
	mean_lateral_accel /= IMU_COUNT;

	/*
	 * La inclinación modifica la aceleración lateral efectiva: la componente
	 * horizontal del sensor se proyecta con cos(roll) y la gravedad aporta
	 * g*sin(roll). Luego se aplica LTR = 2*h*a_y/(T*g).
	 */
	const float effective_lateral_accel = mean_lateral_accel * cosf(mean_roll_rad) +
										  GRAVITY_MS2 * sinf(mean_roll_rad);
	const float ltr = (2.0f * CG_HEIGHT_M * effective_lateral_accel) /
					  (TRACK_WIDTH_M * GRAVITY_MS2);
	return clamp_float(ltr, -1.0f, 1.0f);
}

static void simulate_imu_task(void *arg)
{
	(void)arg;
	TickType_t next_wake = xTaskGetTickCount();
	uint32_t sample_number = 0;

	while (true) {
		const float time_s = (float)sample_number / SAMPLE_RATE_HZ;
		/* La maniobra comienza en 3 s y llega gradualmente a una curva. */
		const float curve_progress = clamp_float((time_s - 3.0f) / 8.0f, 0.0f, 1.0f);
		const float curve_shape = curve_progress * curve_progress * (3.0f - 2.0f * curve_progress);
		const float nominal_roll = 1.0f + 8.0f * curve_shape;
		const float nominal_lateral_accel = 0.3f + 3.8f * curve_shape;

		imu_packet_t packet = {
			.timestamp_ms = (uint32_t)(time_s * 1000.0f),
			.imu = {
				[IMU_CABINA] = { nominal_roll, nominal_lateral_accel },
				[IMU_TANQUE_IZQ] = { nominal_roll * 1.05f, nominal_lateral_accel * 0.98f },
				[IMU_TANQUE_DER] = { nominal_roll * 0.95f, nominal_lateral_accel * 1.02f }
			}
		};

		for (size_t index = 0; index < IMU_COUNT; ++index) {
			packet.imu[index].roll_deg += gaussian_noise(0.8f);
			packet.imu[index].lateral_accel_ms2 += gaussian_noise(0.35f);
		}

		xQueueOverwrite(imu_queue, &packet);
		++sample_number;
		vTaskDelayUntil(&next_wake, pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
	}
}

static void filtering_and_ltr_task(void *arg)
{
	(void)arg;
	imu_packet_t packet;
	kalman_1d_t roll_filter[IMU_COUNT];
	kalman_1d_t accel_filter[IMU_COUNT];

	for (size_t index = 0; index < IMU_COUNT; ++index) {
		roll_filter[index] = make_kalman(0.015f, 0.64f);
		accel_filter[index] = make_kalman(0.02f, 0.1225f);
	}

	printf("Tiempo_ms,Roll_Cab_Raw,Roll_Cab_Filtrado,LTR_Calculado\n");
	while (xQueueReceive(imu_queue, &packet, portMAX_DELAY) == pdTRUE) {
		imu_reading_t filtered[IMU_COUNT];
		for (size_t index = 0; index < IMU_COUNT; ++index) {
			filtered[index].roll_deg = kalman_update(&roll_filter[index], packet.imu[index].roll_deg);
			filtered[index].lateral_accel_ms2 = kalman_update(&accel_filter[index],
															 packet.imu[index].lateral_accel_ms2);
		}

		const float ltr = calculate_ltr(filtered);
		printf("%lu,%.3f,%.3f,%.3f\n",
			   (unsigned long)packet.timestamp_ms,
			   packet.imu[IMU_CABINA].roll_deg,
			   filtered[IMU_CABINA].roll_deg,
			   ltr);
		if (fabsf(ltr) >= LTR_RISK_LIMIT) {
			ESP_LOGW(TAG, "RIESGO DE VOLCADURA: LTR=%.3f", ltr);
		}
	}
}

void app_main(void)
{
	imu_queue = xQueueCreate(1, sizeof(imu_packet_t));
	configASSERT(imu_queue != NULL);

	xTaskCreate(simulate_imu_task, "simulate_imus", 4096, NULL, 5, NULL);
	xTaskCreate(filtering_and_ltr_task, "filter_and_ltr", 4096, NULL, 5, NULL);
}
