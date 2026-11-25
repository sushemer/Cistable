#include "driver/uart.h"

// Valores editables directamente aquí; no se configuran desde menuconfig.
#define GPS_UART_PORT                       UART_NUM_2 // UART usada para el GPS
#define GPS_UART_BAUD_RATE                  9600       // Velocidad de comunicación GPS
#define GPS_UART_RXD                        18         // GPIO que recibe TX del GPS en el nuevo cableado
#define GPS_UART_TXD                        17         // GPIO que transmite hacia RX del GPS

// Criterios iniciales para evaluar las lecturas; confirmar con los registros.
#define GPS_EXPECTED_PERIOD_MS              1000       // Intervalo esperado entre actualizaciones
#define GPS_MAX_AGE_MS                      1800       // Antigüedad máxima aceptada de los datos
#define GPS_MAX_EPOCH_SKEW_MS               200        // Diferencia máxima entre las horas RMC y GGA
#define GPS_MIN_SATELLITES                  4          // Satélites mínimos para considerar el fix
#define GPS_MAX_HDOP                        4.0f       // HDOP máximo; un valor menor suele indicar mejor geometría

// Límites provisionales para analizar las pruebas.
#define GPS_SPEED_MAX_KMH                   180.0f     // Velocidad máxima considerada en el análisis
#define GPS_SPEED_MAX_ACCEL_KMH_S           25.0f      // Cambio máximo de velocidad por segundo
#define GPS_SPEED_CONFIRM_TOLERANCE_KMH     10.0f      // Diferencia entre muestras sospechosas

#define GPS_RX_BUFFER_SIZE                  2048       // Espacio del búfer de recepción UART
#define GPS_LINE_SIZE                       128        // Tamaño máximo de una línea NMEA

#define GPS_LOG_INTERVAL_MS                 1000       // Intervalo entre mensajes periódicos de estado

#pragma once

