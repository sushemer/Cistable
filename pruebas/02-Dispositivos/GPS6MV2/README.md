# GPS6MV2
Documentación, firmware, herramientas y resultados del receptor GPS 6M V2.

## Propósito
Esta carpeta contiene el trabajo experimental del GPS para el proyecto. El objetivo es definir una estrategia para detectar lecturas falsas, especialmente
saltos de velocidad ocasionados por señal obstruida, reflexiones o pérdida temporal del fix.

El GPS entrega la velocidad de la sentencia RMC en nudos. El firmware la convierte a kilómetros por hora usando: km/h = nudos x 1.852

La posición, latitud y longitud se conservan para análisis, pero la velocidad Doppler reportada por el receptor es el dato principal. La velocidad calculada
a partir de diferencias de coordenadas se utilizará únicamente como comprobación secundaria.

## Organización
| Ubicación | Contenido |
|---|---|
| `apoyo/` | Script de Python para registrar NMEA en Excel. |
| `prueba_01/` | Primera prueba del GPS. |
| `prueba_02/` | Segunda prueba del GPS. |
| `prueba_03/` | Tercera prueba con IMU. |

Cada prueba tiene sus propios datos, análisis y firmware.

## Referencias técnicas
1. **Parser NMEA oficial de Espressif para ESP-IDF**
   - [Ejemplo `nmea0183_parser`](https://github.com/espressif/esp-idf/tree/master/examples/peripherals/uart/nmea0183_parser)
   - Referencia para recepción UART, buffers y procesamiento de sentencias
     NMEA en ESP-IDF.

2. **Documentación oficial del receptor u-blox NEO-6**
   - [Hoja técnica NEO-6](https://content.u-blox.com/sites/default/files/products/documents/NEO-6_DataSheet_%28GPS.G6-HW-09005%29.pdf)
   - Referencia para mensajes NMEA, velocidad de actualización, configuración
     UART, modos de operación y características del NEO-6M.

3. **Ejemplo práctico de u-blox con UART2 en ESP32**
   - [ESP32 u-blox GPS OLED](https://github.com/giorgioinfo13/esp32-ublox-gps-oled)
   - Referencia práctica para conexión por UART2, lectura de velocidad,
     coordenadas, satélites y HDOP.

