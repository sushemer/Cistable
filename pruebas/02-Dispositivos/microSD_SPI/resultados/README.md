# Evidencia de prueba 01: escritura en microSD

Se conservaron sin convertir ni editar para mantener la evidencia original de la prueba.

## Archivos recuperados

| Archivo | Registros | Último tiempo (ms) | Resultado |
|---|---:|---:|---|
| `SD_0001.CSV` | 20 | 19 350 | `OK` |
| `SD_0002.CSV` | 4 | 3 150 | `OK` |
| `SD_0003.CSV` | 14 | 13 250 | `OK` |
| `SD_0004.CSV` | 464 | 468 240 | `OK` |

Cada archivo incluye el encabezado:

```text
registro;tiempo_ms;mensaje;estado
```

Los registros indican que el ESP32 pudo montar la tarjeta y escribir líneas
periódicas con el mensaje `PRUEBA_MICROSD` y estado `OK`. `SD_0004.CSV` es la
prueba más larga de esta serie.



