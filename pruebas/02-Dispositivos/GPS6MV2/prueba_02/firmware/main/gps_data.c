#include "gps_data.h"

#include <string.h>

// Inicializa a cero la muestra acumulada antes de procesar sentencias NMEA.
void gps_sample_init(gps_sample_t *sample){
    if (sample != NULL) {
        memset(sample, 0, sizeof(*sample));
    }
}
