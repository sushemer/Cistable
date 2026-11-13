# Prueba del lector microSD por SPI en ESP32 DevKit V1

Esta prueba verifica únicamente el lector microSD y escribe archivos CSV
directamente en la tarjeta. Se usan solamente pines del lado izquierdo del
ESP32 DevKit V1.

Cada registro se sincroniza con la tarjeta mediante `fflush()` y `fsync()`, por
lo que el tamaño del archivo debe actualizarse mientras la prueba continúa. Aun
así, detenga el ESP32 antes de retirar la tarjeta.

## Pinout

| Lector microSD | ESP32 DevKit V1, lado izquierdo |
|---|---:|
| SCK | GPIO14 |
| MOSI/DI | GPIO13 |
| MISO/DO | GPIO27 |
| CS/SS | GPIO26 |
| VCC | VIN/5V |
| GND | GND |

El lector MO0074/HCMODU0074 recibe 5 V y realiza la conversión de niveles SPI.
No conecte 5 V directamente a ningún GPIO del ESP32.

## Revisión de la tarjeta en Windows

1. Retire la microSD del lector.
2. Conéctela a la computadora con un lector USB.
3. Abra **Propiedades** de la unidad.
4. Revise **Sistema de archivos**.
5. Debe indicar `FAT32`.

También puede consultar las unidades sin modificarlas con:

```powershell
Get-Volume
```

Si aparece `exFAT`, respalde los archivos antes de formatear. Formatear borra
todo el contenido.

## Resultado esperado

Con la tarjeta insertada y el cableado correcto, el firmware debe mostrar:

```text
SCK=14 MOSI=13 MISO=27 CS=26
```

Después debe montar la tarjeta y crear un archivo como `sd_0001.csv`.

La numeración reconoce los nombres FAT aunque el sistema los entregue en
mayúsculas (`SD_0001.CSV`) o minúsculas, para evitar sobrescribir una ejecución
anterior.
Si aparece `ESP_ERR_TIMEOUT`, revise VCC, GND, `DI/DO`, CS y la continuidad de
los cables.

La prueba limita temporalmente la frecuencia SPI a 4 MHz para diagnosticar
errores `ESP_ERR_INVALID_CRC`. Si funciona a esta velocidad, el lector o su
conversor de niveles tiene poca tolerancia a velocidades mayores.

## Prueba aislada del formato del registro

El firmware actual de esta carpeta es una prueba independiente para el DevKit
V1. Solo usa la microSD. No inicializa IMUs, GPS, UART ni ESP-NOW. Para probar
el formato de almacenamiento genera líneas simuladas con los mismos grupos que
usará el Master: `IMU1`, `IMU2`, `IMU_A`, `GPS` y `EVENT`.

El resultado se guarda en un solo archivo por ejecución:

```text
raw_0001.log
```

La numeración toma el archivo mayor existente y crea el siguiente, incluso si
se borra un archivo intermedio. El formato es:

```text
registro;time_us;fecha_hora_local;origen;linea_original
```

Estas líneas son simuladas y sirven únicamente para comprobar la tarjeta, el
formato y el conversor. No deben interpretarse como mediciones reales.

## Conversión a un solo Excel con hojas

Después de retirar la tarjeta, copie el archivo `raw_XXXX.log` a una carpeta de
la computadora. Desde la carpeta `tools`, instale la dependencia una sola vez:

```powershell
py -m pip install -r requirements.txt
```

Ejecute:

```powershell
py raw_log_to_excel.py "..\resultados\prueba_01\raw_0001.log" "..\resultados\prueba_01\prueba_01_resultados.xlsx"
```

El libro resultante contiene las hojas `Resumen`, `IMU1_Frontal`,
`IMU2_Trasera`, `IMU_A_Combinada`, `GPS`, `Estados_Eventos` y `Crudo`.
`Resumen` usa fórmulas de Excel para contar registros, contar estados válidos y
calcular el LTR promedio. `Crudo` conserva cada línea original para auditoría.
