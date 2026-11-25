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
| `prueba_03/` | Tercera prueba con IMU (monitoreo). |
| `prueba_04/` | Tercera prueba con IMU + validaciones de velocidad. |

Cada prueba tiene sus propios datos, análisis y firmware.

## Glosario básico

| Término | Significado |
|---|---|
| **GPS** | Sistema que calcula posición, hora y velocidad usando satélites. En este proyecto se usa un módulo GPS6MV2/NEO-6M. |
| **NMEA** | Formato de texto que utiliza el GPS para enviar información, por ejemplo `$GPRMC` y `$GPGGA`. |
| **RMC** | Sentencia NMEA que contiene datos de navegación, principalmente estado del fix, posición, velocidad y rumbo. |
| **GGA** | Sentencia NMEA que contiene posición, calidad del fix, número de satélites y HDOP. |
| **Fix** | Solución de posición calculada por el GPS. Un fix válido significa que el receptor tiene una posición utilizable. |
| **HDOP** | Indicador de la calidad geométrica de los satélites para calcular la posición horizontal. Un valor bajo suele ser mejor; `99.99` indica que no hay solución confiable. No representa directamente una velocidad ni un error exacto en metros. |
| **Satélites** | Número de satélites utilizados o reportados por el GPS para calcular la solución. Más satélites no siempre garantizan una buena posición, porque también importa su distribución. |
| **Rumbo** | Dirección del desplazamiento expresada en grados. `0°` apunta al norte, `90°` al este, `180°` al sur y `270°` al oeste. |
| **UART** | Comunicación serial utilizada para transmitir datos entre el GPS y el ESP32. En este proyecto el GPS está conectado al UART2 y la salida del ESP32 hacia la laptop usa UART0/USB. |
| **Baudios** | Velocidad de transmisión serial. El GPS utiliza normalmente `9600` baudios y el monitor USB del ESP32 `115200` baudios. |
| **IMU** | Unidad de medición inercial. Puede entregar aceleración, orientación y velocidad angular para describir el movimiento del dispositivo. |
| **Aceleración lineal** | Cambio de movimiento medido por la IMU, normalmente en m/s². Sirve para identificar arranques, frenados, vibraciones o quietud, pero no calcula por sí sola una velocidad constante confiable. |
| **Checksum** | Código incluido al final de una sentencia NMEA para comprobar que los caracteres recibidos no se alteraron durante la transmisión. |
| **Logger** | Programa de Python que lee el puerto COM y guarda los datos recibidos en un archivo Excel. |
| **Fix válido** | Registro que cumple las condiciones mínimas para considerar utilizable la posición GPS. Se analiza junto con velocidad, satélites, HDOP y antigüedad de la lectura. |

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

