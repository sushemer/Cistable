# Prueba 02 del GPS6MV2
Las primeras pruebas se hicieron en un automóvil normal, en el patio y en una ventana.

## Objetivo
Revisar la recepción NMEA y evaluar la calidad de posición y velocidad, incluidos posibles saltos o interrupciones. Se conservarán las lecturas originales para compararlas con los diagnósticos del firmware.



## Organización
| Ubicación | Contenido |
|---|---|
| `firmware/` | Firmware ESP-IDF para el GPS, sin IMU. |
| `pruebas/` | Registros generados y guardados en la laptop. |
| `analisis/` | Revisión de los registros y resultados. |

Los archivos se guardan por separado para cada recorrido o condición con el [script de apoyo](../apoyo/README.md). Se registrarán la NMEA original, velocidad, fix, satélites, HDOP, tiempos y diagnósticos de recepción. El tiempo desde el arranque del ESP32 no siempre equivale al tiempo de adquisición del GPS: puede haber permanecido encendido.

El consolidado `analisis/prueba_02_resultados.xlsx` conserva los registros importados, agrega cálculos con fórmulas de Excel y enlaza el resumen con las hojas de cada prueba. Los archivos que no se pudieron leer se anotan en `Fuentes`; al volverlos disponibles localmente, se puede regenerar el libro sin cambiar los originales.

## Referencias

- [README general del GPS6MV2](../README.md): descripción del dispositivo y referencias principales.
- [UART de ESP-IDF 5.5](https://docs.espressif.com/projects/esp-idf/en/v5.5.6/esp32/api-reference/peripherals/uart.html): recepción, buffers y errores UART.
- [Ejemplo NMEA de Espressif](https://github.com/espressif/esp-idf/tree/master/examples/peripherals/uart/nmea0183_parser): referencia de recepción UART y procesamiento NMEA.
