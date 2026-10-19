# Herramienta de registro GPS

El script recibe por el puerto COM los diagnósticos del firmware ESP-IDF y los guarda en un archivo Excel en la laptop. La prueba 02 conserva una fila por sentencia recibida, incluyendo la NMEA cruda, además de estados periódicos y eventos del firmware.

## Uso

Desde esta carpeta, indique manualmente el puerto y el nombre del archivo:

```powershell
py gps_logger_excel.py --device gps6mv2 --port COM7 --output prueba_02_carro_01.xlsx
```

El script usa `115200` baudios para el puerto USB/serial del ESP32 y guarda cada 10 filas. Este enlace es independiente del UART del GPS, que inicia en `9600 8N1`. Cierre ESP-IDF Monitor, otros monitores seriales y Excel antes de iniciar el registro.

## Datos registrados

La hoja `GPS` conserva hora de recepción en la laptop, uptime del ESP32, tipo de registro y sentencia NMEA original. Cuando se recibe `GPSDIAG`, también guarda checksum, validez de parseo, estado RMC, calidad GGA, diferencia de época RMC-GGA, posición, velocidad en nudos y km/h, velocidad auxiliar calculada por coordenadas, rumbo, satélites, HDOP, antigüedad de RMC y GGA, estado GPS, decisión provisional de velocidad, delta y aceleración. La velocidad por coordenadas es solo una comparación: procede del mismo receptor y puede ser ruidosa, especialmente a baja velocidad. `GPSSTATUS` permite ver interrupciones aunque el receptor no esté enviando sentencias; `GPS_EVENT` registra inicio, primer fix, estabilidad y errores UART.

Se conserva la fila aunque el checksum, parseo, fix o velocidad no sean aceptables. La clasificación es para análisis y no constituye un límite operativo del vehículo.

## Dependencias

```powershell
py -m pip install pyserial openpyxl
```

Use un archivo distinto por recorrido, detenga el registro con `Ctrl+C` para guardar al finalizar y no abra el Excel mientras el programa siga activo.
