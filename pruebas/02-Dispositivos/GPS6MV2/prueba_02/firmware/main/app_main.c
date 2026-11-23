#include <inttypes.h>
#include <math.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"

#include "gps_config.h"
#include "gps_data.h"
#include "gps_device.h"
#include "gps_nmea.h"
#include "gps_speed.h"
#include "gps_validation.h"

static const char *TAG = "GPS";

// Informa errores de UART solo cuando cambian sus contadores.
static void report_uart_errors(void){
    static gps_device_stats_t reported = {0};
    gps_device_stats_t current = {0};
    gps_device_get_stats(&current);
    if (memcmp(&current, &reported, sizeof(current)) != 0) {
        ESP_LOGW(TAG, "GPS_EVENT|UART_ERRORS|overflow=%" PRIu32 "|buffer_full=%" PRIu32
            "|frame=%" PRIu32 "|parity=%" PRIu32 "|overlong=%" PRIu32, current.uart_overflow, current.uart_buffer_full,
            current.uart_frame_error, current.uart_parity_error, current.overlong_lines);
        reported = current;
    }
}

// Devuelve NAN para que el registro distinga datos inválidos de un cero real.
static float finite_or_nan(bool valid, float value){
    return valid ? value : NAN;
}

// Emite GPSDIAG con el orden de campos que espera el registrador de la laptop.
static void emit_diagnostic( const gps_sample_t *sample,const gps_nmea_result_t *nmea, const gps_validation_result_t *validation, const char *speed_status,
    bool speed_usable, float delta_kmh, float acceleration_kmh_s, float coordinate_speed_kmh, float coordinate_speed_delta_kmh, const char *raw){
    const int32_t gps_time_ms = nmea->has_time ? (int32_t)nmea->time_of_day_ms : -1;

    const int32_t epoch_skew_ms = validation->epoch_skew_ms == UINT32_MAX   ? -1 : (int32_t)validation->epoch_skew_ms;

    ESP_LOGI( TAG, "GPSDIAG|1|%" PRIu64 "|%s|%d|%d|%" PRId32 "|%d|%d|%" PRId32 "|%.6f|%.6f|%.3f|%.3f|%.2f|%u|%.2f|%" PRIu32 "|%" PRIu32 "|%s|%s|%d|%.3f|%.3f|%.3f|%.3f|%s",
        (uint64_t)esp_timer_get_time(), nmea->type, nmea->checksum_valid ? 1 : 0, nmea->parsed ? 1 : 0, gps_time_ms, sample->rmc_status_valid ? 1 : 0, sample->fix_quality, epoch_skew_ms,
        finite_or_nan(sample->rmc_data_valid, sample->latitude_deg), finite_or_nan(sample->rmc_data_valid, sample->longitude_deg), finite_or_nan(sample->rmc_data_valid, sample->speed_knots),
        finite_or_nan(sample->rmc_data_valid, sample->speed_kmh), finite_or_nan(sample->rmc_data_valid, sample->course_deg), (unsigned)sample->satellites, finite_or_nan(sample->gga_data_valid, sample->hdop),
        validation->rmc_age_ms, validation->gga_age_ms, gps_validation_state_name(validation->state), speed_status, speed_usable ? 1 : 0, isfinite(delta_kmh) ? delta_kmh : NAN,
        isfinite(acceleration_kmh_s) ? acceleration_kmh_s : NAN, isfinite(coordinate_speed_kmh) ? coordinate_speed_kmh : NAN, isfinite(coordinate_speed_delta_kmh) ? coordinate_speed_delta_kmh : NAN, raw);
}

// Emite el estado actual del GPS y de la velocidad a intervalos regulares.
static void emit_status( const gps_sample_t *sample, const gps_validation_result_t *validation, const char *speed_status, bool speed_usable){
    ESP_LOGI(TAG, "GPSSTATUS|1|%" PRIu64 "|%s|%" PRIu32 "|%" PRIu32 "|%.3f|%s|%d",
        (uint64_t)esp_timer_get_time(),gps_validation_state_name(validation->state), validation->rmc_age_ms, validation->gga_age_ms,
        finite_or_nan(sample->rmc_data_valid, sample->speed_kmh), speed_status, speed_usable ? 1 : 0);
}

// Lee NMEA, valida la calidad del fix y registra la velocidad original y sus comprobaciones.
void app_main(void){
    gps_sample_t sample, previous_accepted_sample = {0}, previous_position_sample = {0};
    char line[GPS_LINE_SIZE];
    int64_t boot_us = esp_timer_get_time(), last_speed_evaluated_us = 0, last_stability_rmc_us = 0,
         last_status_us = 0, last_usable_fix_us = 0;
    uint8_t consecutive_good_epochs = 0;
    bool have_accepted_speed = false, have_position_sample = false, have_pending_speed = false, have_speed_epoch = false, have_stability_epoch = false;
    uint32_t last_speed_epoch_ms = 0, last_stability_epoch_ms = 0;
    bool first_fix_reported = false, stable_fix_reported = false, last_speed_usable = false;
    float last_delta_kmh = NAN, last_acceleration_kmh_s = NAN, last_coordinate_speed_kmh = NAN, last_coordinate_speed_delta_kmh = NAN, pending_speed_kmh = NAN;
    const char *last_speed_status = "SIN_EVALUAR";

    const gps_validation_config_t validation_config = {
        .min_satellites = GPS_MIN_SATELLITES,
        .max_hdop = GPS_MAX_HDOP,
        .max_age_ms = GPS_MAX_AGE_MS,
        .max_epoch_skew_ms = GPS_MAX_EPOCH_SKEW_MS,
    };
    const gps_speed_config_t speed_config = {
        .max_acceleration_kmh_s = GPS_SPEED_MAX_ACCEL_KMH_S,
        .max_speed_kmh = GPS_SPEED_MAX_KMH,
    };

    gps_sample_init(&sample);
    gps_device_init();
    ESP_LOGI(TAG, "GPS_EVENT|BOOT|uptime_us=%" PRIu64 "|uart=%d|baud=%d", (uint64_t)boot_us, GPS_UART_PORT, GPS_UART_BAUD_RATE);

    while (true) {
        report_uart_errors();
        if (!gps_device_read_line(line, sizeof(line), pdMS_TO_TICKS(20))) {
            report_uart_errors();
            int64_t now_us = esp_timer_get_time();
            if (now_us - last_status_us >= GPS_LOG_INTERVAL_MS * 1000LL) {
                gps_validation_result_t current = gps_validation_evaluate(
                    &sample, &validation_config, now_us);
                if (!current.usable_for_speed && (current.state == GPS_VALIDATION_OBSOLETO ||current.state == GPS_VALIDATION_SIN_FIX)) {
                    have_accepted_speed = false;
                    have_pending_speed = false;
                    last_speed_usable = false;
                    last_speed_status = current.reason;
                    consecutive_good_epochs = 0;
                }
                if (last_usable_fix_us > 0 &&
                    now_us - last_usable_fix_us > GPS_MAX_AGE_MS * 1000LL) {
                    have_accepted_speed = false;
                    have_pending_speed = false;
                    last_speed_usable = false;
                }
                emit_status(&sample, &current, last_speed_status, current.usable_for_speed && last_speed_usable);
                last_status_us = now_us;
            }
            continue;
        }

        int64_t received_us = esp_timer_get_time();
        gps_nmea_result_t nmea = {0};
        (void) gps_nmea_process_line(&sample, line, received_us, &nmea);
        gps_validation_result_t validation = gps_validation_evaluate(&sample, &validation_config, received_us);

        if (validation.usable_for_speed) last_usable_fix_us = received_us;
        else if (last_usable_fix_us > 0 && received_us - last_usable_fix_us > GPS_MAX_AGE_MS * 1000LL) {
            have_accepted_speed = false;
            have_pending_speed = false;
            last_speed_usable = false;
        }

        // Evalúa velocidad una sola vez por época RMC, aunque lleguen otras sentencias NMEA.
        bool new_speed_epoch = !have_speed_epoch ||  sample.rmc_time_of_day_ms != last_speed_epoch_ms;
        if (sample.rmc_received_us > last_speed_evaluated_us && new_speed_epoch && validation.usable_for_speed) {
            last_speed_evaluated_us = sample.rmc_received_us;
            last_speed_epoch_ms = sample.rmc_time_of_day_ms;
            have_speed_epoch = true;
            // La velocidad por coordenadas se conserva solo como comparación secundaria.
            if (!have_position_sample || sample.rmc_time_of_day_ms != previous_position_sample.rmc_time_of_day_ms) {
                last_coordinate_speed_kmh = NAN;
                last_coordinate_speed_delta_kmh = NAN;
                if (have_position_sample) {
                    uint32_t previous_tod = previous_position_sample.rmc_time_of_day_ms;
                    uint32_t current_tod = sample.rmc_time_of_day_ms;
                    uint32_t elapsed_ms = current_tod >= previous_tod ? current_tod - previous_tod: 86400000U - previous_tod + current_tod;
                    if (elapsed_ms <= 2U * GPS_EXPECTED_PERIOD_MS) {
                        gps_coordinate_speed_t coordinate_speed = gps_speed_from_coordinates(&previous_position_sample,&sample, elapsed_ms);
                        if (coordinate_speed.valid) {
                            last_coordinate_speed_kmh = coordinate_speed.speed_kmh;
                            last_coordinate_speed_delta_kmh =coordinate_speed.speed_kmh - sample.speed_kmh;
                        }
                    }
                }
                previous_position_sample = sample;
                have_position_sample = true;
            }
            // Los límites clasifican muestras para el análisis; no controlan el vehículo.
            if (!have_accepted_speed) {
                if (sample.speed_kmh > GPS_SPEED_MAX_KMH) {
                    last_speed_usable = false;
                    last_speed_status = "velocidad_fuera_de_rango";
                }
                else {
                    previous_accepted_sample = sample;
                    have_accepted_speed = true;
                    have_pending_speed = false;
                    last_speed_usable = true;
                    last_speed_status = "PRIMERA_MUESTRA";
                    last_delta_kmh = 0.0f;
                    last_acceleration_kmh_s = NAN;
                }
            }
            else {
                uint64_t elapsed_us = (uint64_t)(sample.rmc_received_us - previous_accepted_sample.rmc_received_us);
                uint32_t elapsed_ms = (uint32_t)(elapsed_us / 1000ULL);
                gps_speed_check_t check = gps_speed_check(&previous_accepted_sample, &sample, elapsed_ms, &speed_config);
                last_delta_kmh = check.delta_kmh;
                last_acceleration_kmh_s = check.acceleration_kmh_s;
                last_speed_usable = check.valid;
                float pending_delta = sample.speed_kmh - pending_speed_kmh;
                if (pending_delta < 0.0f) pending_delta = -pending_delta;

                bool confirms_pending = have_pending_speed && pending_delta <= GPS_SPEED_CONFIRM_TOLERANCE_KMH;
                if (check.valid && confirms_pending) {
                    previous_accepted_sample = sample;
                    have_pending_speed = false;
                    last_speed_usable = true;
                    last_speed_status = "CONFIRMADA_2_MUESTRAS";
                } else if (!check.valid && confirms_pending) {
                    pending_speed_kmh = sample.speed_kmh;
                    last_speed_usable = false;
                    last_speed_status = "SALTO_PERSISTENTE_PENDIENTE";
                } else if (check.valid) {
                    previous_accepted_sample = sample;
                    have_pending_speed = false;
                    last_speed_status = "ACEPTADA_PROVISIONAL";
                } else {
                    have_pending_speed = true;
                    pending_speed_kmh = sample.speed_kmh;
                    last_speed_status = check.reason;
                }
            }
        }
        else if (!validation.usable_for_speed) {
            last_speed_usable = false;
            last_speed_status = validation.reason;
            if (strcmp(validation.reason, "epocas_rmc_gga_no_coinciden") != 0) consecutive_good_epochs = 0;

            if (validation.state == GPS_VALIDATION_OBSOLETO || validation.state == GPS_VALIDATION_SIN_FIX) {
                have_accepted_speed = false;
                have_pending_speed = false;
            }
        }

        // Registra el primer fix válido y cinco épocas válidas consecutivas como referencia.
        bool new_stability_epoch = !have_stability_epoch ||  sample.rmc_time_of_day_ms != last_stability_epoch_ms;
        if (sample.rmc_received_us > last_stability_rmc_us && new_stability_epoch && validation.usable_for_speed) {
            if (have_stability_epoch) {
                uint32_t elapsed_epoch_ms = sample.rmc_time_of_day_ms >= last_stability_epoch_ms
                    ? sample.rmc_time_of_day_ms - last_stability_epoch_ms : 86400000U - last_stability_epoch_ms + sample.rmc_time_of_day_ms;

                if (elapsed_epoch_ms > (GPS_EXPECTED_PERIOD_MS * 3U) / 2U) consecutive_good_epochs = 0;
            }
            last_stability_rmc_us = sample.rmc_received_us;
            last_stability_epoch_ms = sample.rmc_time_of_day_ms;
            have_stability_epoch = true;
            if (!first_fix_reported) {
                first_fix_reported = true;
                ESP_LOGI(TAG,"GPS_EVENT|FIRST_VALID_FIX|elapsed_since_esp_boot_ms=%" PRIu64, (uint64_t)(received_us - boot_us) / 1000ULL);
            }
            if (consecutive_good_epochs < UINT8_MAX)  ++consecutive_good_epochs;

            if (!stable_fix_reported && consecutive_good_epochs >= 5) {
                stable_fix_reported = true;
                ESP_LOGI(TAG,"GPS_EVENT|FIVE_CONSECUTIVE_VALID_EPOCHS|elapsed_since_esp_boot_ms=%" PRIu64, (uint64_t)(received_us - boot_us) / 1000ULL);
            }
        }

        emit_diagnostic(&sample, &nmea, &validation, last_speed_status, validation.usable_for_speed && last_speed_usable,last_delta_kmh,
                last_acceleration_kmh_s,last_coordinate_speed_kmh, last_coordinate_speed_delta_kmh, line);
        if (received_us - last_status_us >= GPS_LOG_INTERVAL_MS * 1000LL) {
            emit_status(&sample, &validation, last_speed_status, validation.usable_for_speed && last_speed_usable);
            last_status_us = received_us;
        }
    }
}
