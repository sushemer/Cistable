#ifndef RECEPTOR_ESTADO_H
#define RECEPTOR_ESTADO_H

#include "protocolo_estado.h"

// API de un solo contexto: no incorpora sincronizacion entre tareas.
void receptor_estado_init(void);
bool receptor_estado_procesar(const paquete_espnow_t *paquete);

// Copia el estado vigente, incluido SIN_DATOS. Devuelve false solo con NULL.
// Comprueba tambien el timeout antes de entregar el estado.
bool receptor_estado_obtener(estado_sistema_t *estado);
void receptor_estado_actualizar_timeout(void);

#endif
