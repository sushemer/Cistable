# Herramientas de registro del GPS

Esta carpeta contiene el programa auxiliar para recibir sentencias NMEA del ESP32 por UART y guardarlas en un archivo `.xlsx`.

La descripción general del dispositivo y la organización de las pruebas se encuentran en el [README principal del GPS6MV2](../README.md).

## Cómo funciona
1. Abre el puerto COM indicado.
2. Lee una línea NMEA completa del ESP32.
3. Ignora las líneas que no comienzan con `$`.
4. Identifica sentencias `GPRMC`, `GNRMC`, `GPGGA` o `GNGGA`.
5. Convierte latitud y longitud del formato NMEA a grados decimales.
6. Convierte la velocidad de nudos a km/h.
7. Extrae rumbo, satélites, HDOP, calidad del fix y hora GPS.
8. Conserva también la sentencia NMEA original en `nmea_crudo`.
9. Agrega cada sentencia recibida como una fila en la hoja `GPS`.
10. Guarda el libro periódicamente y vuelve a guardarlo al terminar con `Ctrl+C`.

## Uso básico
Desde esta carpeta, ejecutar:
```powershell
py gps_logger_excel.py --port COM7 --output prueba_02_carro_01.xlsx
```
`--port` indica el puerto COM del ESP32 y `--output` indica el nombre del
archivo Excel. El script utiliza internamente `115200` baudios y guarda cada
10 filas.

## Dependencias
El entorno de Python debe tener instalados:
```powershell
py -m pip install pyserial openpyxl
```

También se necesita que el ESP32 esté enviando datos por el puerto COM seleccionado.

## Columnas generadas

La hoja `GPS` contiene:

| Columna | Descripción |
|---|---|
| `hora_pc` | Hora local de la computadora al recibir la línea. |
| `hora_gps` | Hora incluida en la sentencia GPS. |
| `tipo` | Tipo de sentencia, por ejemplo `GNRMC` o `GNGGA`. |
| `fix_valido` | `True` cuando RMC indica estado activo `A`. |
| `latitud_deg` / `longitud_deg` | Coordenadas en grados decimales. |
| `velocidad_kmh` | Velocidad RMC convertida de nudos a km/h. |
| `rumbo_deg` | Rumbo indicado por RMC en grados. |
| `satelites` | Satélites usados, tomado de GGA. |
| `hdop` | Dilución horizontal de precisión, tomada de GGA. |
| `calidad_fix` | Calidad de solución GGA. |
| `nmea_crudo` | Sentencia original recibida. |

## Consideraciones importantes
- El programa registra cada sentencia por separado. Una fila RMC contiene principalmente posición, velocidad y rumbo; una fila GGA contiene principalmente satélites, HDOP y calidad del fix.
- Por esa razón, algunas columnas quedan vacías en determinadas filas. El análisis posterior debe relacionar RMC y GGA por hora GPS si se necesita una fila consolidada.
- La versión actual no valida el checksum NMEA antes de guardar la línea. Para una futura versión se puede agregar esa validación y una columna `checksum_valido`.
- El programa conserva la sentencia cruda, lo cual permite revisar o reprocesar los datos posteriormente.
- No se debe abrir el archivo de salida en Excel mientras el registrador intenta guardarlo. Excel, OneDrive o la vista previa pueden bloquear el archivo.
- Conviene usar un archivo distinto para cada prueba, por ejemplo `prueba_02_patio_01.xlsx`, para no mezclar recorridos.

## Flujo recomendado para una nueva prueba
1. Confirmar el puerto COM.
2. Cerrar ESP-IDF Monitor, Arduino Serial Monitor, PuTTY y Excel si están usando el mismo puerto o archivo.
3. Conectar el ESP32 y esperar a que el GPS transmita.
4. Ejecutar el comando de registro.
5. Realizar la prueba sin mover ni desconectar el dispositivo de forma inesperada.
6. Detener con `Ctrl+C` para forzar el guardado final.
7. Abrir el Excel y revisar.


## Pruebas
La herramienta puede utilizarse para registrar los datos de cada prueba. Cada prueba debe conservar sus archivos de salida por separado.
