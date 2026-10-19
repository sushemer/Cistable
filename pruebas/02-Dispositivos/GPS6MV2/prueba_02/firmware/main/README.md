# Módulos del firmware

| Archivo | Función |
|---|---|
| `app_main.c` | Coordina recepción, validación, diagnóstico de velocidad y eventos de arranque. |
| `gps_config.h` | Pines y valores predeterminados de la prueba. |
| `gps_device.c/.h` | UART, ensamblado de líneas NMEA y contadores de errores. |
| `gps_nmea.c/.h` | Checksum, RMC/GGA, hora UTC y validación estricta de campos. |
| `gps_data.c/.h` | Estructura compartida y función para inicializar los datos de recepción RMC/GGA. |
| `gps_validation.c/.h` | Fix, calidad, antigüedad y coincidencia temporal RMC-GGA. |
| `gps_speed.c/.h` | Comprobación provisional de rango/cambio y velocidad secundaria por coordenadas. |

Los parámetros se editan directamente en `gps_config.h`. El firmware emite `GPSDIAG`, `GPSSTATUS` y `GPS_EVENT` por la consola USB. El script de apoyo en la laptop los registra junto con la NMEA original; el ESP32 no genera ni guarda archivos.
