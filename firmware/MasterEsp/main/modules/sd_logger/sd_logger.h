#pragma once

#include <stdbool.h>

// Crea el escritor en segundo plano; reintenta el montaje sin parar los sensores.
bool sd_logger_start(void);

// Solo la tarea de pantalla espera el primer intento de SD para ordenar SPI2.
void sd_logger_wait_startup(void);

// Indica que el montaje y los encabezados ya fueron sincronizados en la tarjeta.
bool sd_logger_is_ready(void);

// Encola una copia y su tiempo ESP; devuelve false si falta SD o la cola está llena.
bool sd_logger_write_line(const char *line);

// Conserva la impresión original y manda la misma línea al escritor de la SD.
void sd_logger_logf(const char *tag, const char *format, ...)
    __attribute__((format(printf, 2, 3)));


