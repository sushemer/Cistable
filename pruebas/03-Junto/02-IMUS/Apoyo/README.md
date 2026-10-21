# Logger de una IMU

Este directorio contiene el registrador para una sola IMU. Lee las líneas CSV
que imprime el ESP32 por UART0/USB y las guarda en un libro Excel. No utiliza
ESP-NOW.

## Flujo

```text
IMU → ESP32 → UART0/USB → puerto COM → imu_logger_1.py → Excel
```

El firmware de referencia imprime:

```text
Roll_1,Ay,LTR_Calculado
12.35,0.42,0.1834
```

El logger ignora mensajes `ESP_LOG`, mensajes de arranque y cualquier línea que
no contenga exactamente tres valores numéricos separados por comas.

## Columnas del Excel

| Columna | Significado |
|---|---|
| `numero_lectura` | Número consecutivo de cada lectura de la misma IMU. |
| `roll_1_deg` | Roll de la IMU en grados. |
| `ay_m_s2` | Aceleración lateral Ay en m/s². |
| `ltr_calculado` | LTR calculado por el firmware. |
| `linea_original` | Línea CSV recibida, conservando sus valores numéricos. |

No se agrega fecha ni hora. `numero_lectura` conserva el orden de llegada.

## Requisitos

Desde esta carpeta, instalar las dependencias una sola vez:

```powershell
py -m pip install -r requirements.txt
```

## Forma recomendada de ejecutarlo

1. Cargue el firmware de una IMU al ESP32.
2. Confirme el puerto COM asignado al ESP32.
3. Cierre ESP-IDF Monitor y cualquier otro programa que use ese COM.
4. Abra PowerShell en esta carpeta:

```powershell
cd "C:\Users\SUSHEMER\OneDrive\Documentos\GitHub\Bomb\Cistable\pruebas\03-Junto\02-IMUS\Apoyo"
```

5. Ejecute el logger:

```powershell
py imu_logger_1.py --port COM5 --output "..\registros\imu_prueba_01.xlsx"
```

Cambie `COM5` por el puerto real del ESP32.

## Ejecución desde la carpeta `02-IMUS`

También puede ejecutarlo desde la carpeta padre:

```powershell
cd "C:\Users\SUSHEMER\OneDrive\Documentos\GitHub\Bomb\Cistable\pruebas\03-Junto\02-IMUS"
py .\Apoyo\imu_logger_1.py --port COM5 --output ".\registros\imu_prueba_01.xlsx"
```

La velocidad predeterminada es `115200` baudios. Si el firmware usa otra,
puede indicarla explícitamente:

```powershell
py .\Apoyo\imu_logger_1.py --port COM5 --baud 115200 --output ".\registros\imu_prueba_01.xlsx"
```

El archivo Excel se guarda automáticamente cada 10 lecturas. Esta cantidad es
fija y no se configura desde el comando.

Detenga la prueba con `Ctrl+C`. El programa guardará las últimas lecturas antes
de terminar.

## Archivo de salida

El archivo se guarda en:

```text
02-IMUS\registros\imu_prueba_01.xlsx
```

El programa crea la carpeta de salida si no existe, pero no sobrescribe un
archivo existente. Use un nombre nuevo para cada prueba.

## Tare

El `tare` pone a cero la orientación de la IMU en la posición física actual.
Debe hacerse con el vehículo detenido y la IMU colocada en la posición que se
considerará nivelada. Este logger solamente lee y guarda; no envía la letra `t`
al ESP32.

## Limitaciones

- Está diseñado para una sola IMU.
- No utiliza ESP-NOW.
- No calcula Roll, Ay ni LTR; esos valores deben venir del firmware.
- No agrega fecha ni hora.
- Para dos IMUs se creará después otro logger o una versión modular.
