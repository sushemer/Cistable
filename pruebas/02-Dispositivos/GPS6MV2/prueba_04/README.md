# Prueba 04 — GPS + IMU para corregir movimiento aparente
Esta prueba usa una GPS 6MV2 y una BNO085 por SPI en un ESP32 DevKit V1. Su
objetivo es evitar que el GPS reporte como movimiento las velocidades pequeñas
cuando el equipo permanece quieto en ventana o patio.

## Funcionamiento
1. La IMU calcula la magnitud de la aceleración lineal.
2. Si permanece por debajo de `0.20 m/s²` durante al menos `1 segundo`, se confirma reposo.
3. Si el GPS reporta `1 km/h` o menos y la IMU confirma reposo, la velocidad corregida se guarda como `0 km/h` con estado `ESTACIONARIO_IMU`.
4. Si el GPS reporta más de `1 km/h`, se conserva su velocidad aunque la IMU tenga aceleración cercana a cero, porque un vehículo puede avanzar a velocidad constante sin aceleración lineal grande.
5. Si el GPS reporta velocidad baja pero la IMU detecta movimiento, se conserva la lectura y se marca `BAJA_VEL_IMU_MOVIMIENTO`.

Además, la capa clasifica:
- `INICIO_MOVIMIENTO`
- `ACELERANDO`
- `FRENANDO`
- `MOVIMIENTO_CONSTANTE`
- `VIBRACION_IMPACTO`
- `GPS_SOSPECHOSO_IMU_QUIETA`
- `GPS_SIN_DATOS_IMU_MOVIMIENTO`
- `GPS_SIN_DATOS_IMU_QUIETA`.

Estos umbrales son iniciales y deberán revisarse con los registros reales. La aceleración lineal no permite saber por sí sola si un vehículo que avanza a velocidad constante está moviéndose; por eso se combina con la velocidad GPS y su cambio entre lecturas. No es todavía una navegación inercial completa.

## Conexión
GPS por UART2: TX del GPS a GPIO16 del ESP32. BNO085 por SPI:

| BNO085 | ESP32 DevKit V1 |
|---|---|
| DI/MOSI | GPIO23 |
| DO/MISO | GPIO19 |
| SCK | GPIO18 |
| CS | GPIO5 |
| HINT/INT | GPIO4 |
| RST | GPIO22 |
| PS1 | 3.3 V |
| PS0/WAKE | 3.3 V |
| GND | GND |

## Registrar en Excel

Desde PowerShell, ejecute los comandos desde la carpeta `prueba_04`:

```powershell
cd "C:\Users\SUSHEMER\OneDrive\Documentos\GitHub\Bomb\Cistable\pruebas\02-Dispositivos\GPS6MV2\prueba_04"
py -m pip install -r apoyo\requirements.txt
py apoyo\gps_imu_logger.py COM5 pruebas\prueba_04_ventana_01.xlsx
```

Cambie `COM5` por el puerto del ESP32 y el nombre del archivo si desea otro.
El script crea la carpeta de salida si aún no existe.

Detenga con `Ctrl+C`. El archivo se guarda cada cinco segundos y al finalizar.

## Hojas del Excel
- `GPS`: sentencias GPSDIAG originales.
- `IMU`: orientación y aceleración lineal.
- `FUSION`: velocidad GPS cruda, velocidad corregida, aceleración y estado.
- `Muestras_500ms`: vista temporal combinada.
- `Eventos`: mensajes de diagnóstico.

Las columnas principales para el análisis son `velocidad_corregida_kmh`, `cambio_velocidad_kmh`, `aceleracion_imu_norm_m_s2` y `estado_fusion` de `FUSION`. Compare también `gps_velocidad_cruda_kmh` para comprobar cuándo se eliminó una lectura pequeña por reposo confirmado.
