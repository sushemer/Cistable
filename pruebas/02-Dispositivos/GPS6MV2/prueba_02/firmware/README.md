# Firmware de la prueba 02 — GPS sin IMU

Firmware ESP-IDF para evaluar el GPS6MV2 HW-248 conectado a un ESP32. El ESP32 envía diagnósticos por USB; el [script de apoyo](../../apoyo/README.md) guarda los registros en la laptop.

## Funcionamiento

- Recibe sentencias NMEA por UART2 y valida checksum y campos RMC/GGA.
- Compara las horas UTC de RMC y GGA y evalúa fix, satélites, HDOP y antigüedad.
- Conserva la velocidad RMC original en nudos y su conversión a km/h; usa la velocidad derivada de coordenadas solo como comprobación secundaria.
- Marca cambios de velocidad sospechosos con límites provisionales, sin eliminar las lecturas originales.
- Registra datos de arranque, estados, eventos y errores de recepción UART.

## Conexión inicial

| GPS | ESP32 |
|---|---|
| TX | GPIO16 (RX) |
| RX | GPIO17 (TX, opcional para esta prueba) |
| GND | GND común |
| VCC | Alimentación compatible con la placa |

Configuración inicial: UART2, `9600 8N1`. Para recibir datos basta conectar GPS TX, GND y alimentación. Confirmar el nivel lógico y la alimentación de la placa utilizada.

## Registro y tiempos

Conectar el ESP32 por USB y ejecutar el registrador de la laptop con el puerto y nombre de archivo deseados. Cerrar el monitor serial de ESP-IDF antes de abrir el puerto con el script. Guardar cada recorrido en un archivo distinto; no abrir el libro en Excel mientras se está registrando.

El firmware informa el tiempo desde el arranque del ESP32 hasta el primer fix y varias épocas válidas consecutivas. Esto no es necesariamente el tiempo de adquisición del GPS (TTFF): para medirlo, el receptor debe haber estado sin alimentación y debe registrarse cuándo vuelve a encenderse. Si permanece alimentado, el cronómetro del ESP32 no mide el TTFF.

## Configuración y alcance

Los parámetros se editan directamente en `main/gps_config.h` y se aplican al recompilar el firmware. Los límites de calidad y velocidad son valores iniciales para revisar el comportamiento en un automóvil normal; no son umbrales finales ni de seguridad para el camión cisterna. La prueba 02 no integra IMU.

## Referencias

- [README general del GPS6MV2](../../README.md).
- [UART de ESP-IDF 5.5](https://docs.espressif.com/projects/esp-idf/en/v5.5.6/esp32/api-reference/peripherals/uart.html): buffers, recepción y errores UART.
- [Ejemplo NMEA de Espressif](https://github.com/espressif/esp-idf/tree/master/examples/peripherals/uart/nmea0183_parser): ejemplo oficial de recepción y procesamiento NMEA. Es una referencia de estructura, no el origen de los filtros de este proyecto.
- [Hoja técnica u-blox NEO-6](https://content.u-blox.com/sites/default/files/products/documents/NEO-6_DataSheet_%28GPS.G6-HW-09005%29.pdf): consultar si el receptor montado en la placa HW-248 corresponde a la familia NEO-6; verificar la identificación del módulo.
