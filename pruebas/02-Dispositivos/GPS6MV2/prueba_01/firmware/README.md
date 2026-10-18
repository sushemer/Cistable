# Firmware de la prueba 01 — GPS por UART

Firmware ESP-IDF utilizado en la primera prueba del GPS6MV2. 

La información general está en el [README principal](../../README.md) y el resumen de la prueba en el [README de prueba 01](../README.md).

El programa está basado en `uart_echo`, recibe sentencias NMEA por UART y procesa las familias `GxRMC` y `GxGGA`.

## Conexión

| GPS | ESP32 |
|---|---|
| TX | GPIO16 (RX) |
| RX | GPIO17 (TX) |
| GND | GND |
| VCC | Alimentación indicada para el módulo |

Configuración inicial: `9600 8N1`. Debe confirmarse el baudrate del módulo  real antes de probarlo.

Para recibir únicamente datos, el cable `GPS RX → GPIO17` puede dejarse desconectado. Se mantiene disponible para enviar comandos de configuración en el futuro.

## Datos disponibles

| Campo | Descripción |
|---|---|
| `valid_fix` | Indica si la posición es válida. |
| `latitude_deg` | Latitud en grados decimales. |
| `longitude_deg` | Longitud en grados decimales. |
| `speed_kmh` | Velocidad en km/h. |
| `course_deg` | Rumbo en grados. |
| `satellites` | Satélites utilizados. |
| `hdop` | Indicador de precisión horizontal. |
| `age_ms` | Antigüedad del último RMC válido con coordenadas utilizables. |
| `sample_time_us` | Tiempo local de recepción del último RMC válido, en microsegundos desde el arranque del ESP32. No es la hora UTC del GPS. |

La velocidad se convierte de nudos a kilómetros por hora: km/h = nudos x 1.852

## Estados mostrados
El monitor muestra el estado del GPS aproximadamente cada segundo:
- `SIN_FIX`: no hay una posición válida.
- `FIX_VALIDO`: hay una posición válida y reciente.
- `DATO_OBSOLETO`: no se recibió una trama RMC dentro del tiempo permitido.

Ejemplo:
```text
GPS: FIX_VALIDO lat=32.514200 lon=-117.038500 vel=4.63 km/h rumbo=180.00 sat=8 HDOP=1.20 edad=250 ms
```
La salida incluye posición, velocidad, rumbo, satélites, HDOP y antigüedad del dato. La trama NMEA original se registra con nivel `DEBUG`.

## Uso

Probar el GPS en exteriores o cerca de una ventana. El primer fix es la
primera posición válida después de encender o reiniciar el módulo GPS, o después
de perder la señal. Puede tardar desde unos segundos hasta varios minutos.

Reiniciar o cargar nuevamente el programa del ESP32 no siempre obliga al GPS a
buscar desde cero. Si el módulo permaneció encendido, puede conservar sus datos
y continuar enviando posiciones.
Los resultados de este firmware corresponden a la [prueba 01](../README.md).
