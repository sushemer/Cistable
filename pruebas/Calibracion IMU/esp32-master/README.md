# Cistable Master ESP32-S3

Firmware del nodo **master** de Cistable. El master reúne las mediciones del
vehículo y del tanque en una única salida de telemetría:

- recibe por **ESP-NOW** el estado del nodo trasero (`TankPayload`);
- lee la **BNO085 frontal** mediante SPI;
- recibe y valida tramas NMEA de un **GPS6MV2** mediante UART;
- publica registros periódicos en el monitor serie para su captura por un
  registrador externo o por herramientas de análisis.

El proyecto está construido con ESP-IDF y está configurado para
`esp32s3`. No implementa todavía la fusión GPS/IMU, la detección de
volcadura ni el filtrado de velocidad de la prueba 02.

## Arquitectura

`main/app_main.c` coordina el sistema y ejecuta el ciclo principal:

1. inicia el GPS y la IMU frontal;
2. inicializa ESP-NOW en el canal 1;
3. acepta únicamente paquetes del tamaño de `TankPayload`;
4. conserva la última muestra recibida del nodo trasero;
5. lee la última muestra de la BNO085;
6. emite el estado legible y un registro `MASTERDATA` cada segundo.

Los módulos están organizados así:

| Ruta | Responsabilidad |
| --- | --- |
| `main/app_main.c` | Coordinación, recepción ESP-NOW y salida de telemetría |
| `main/tank_payload.h` | Contrato binario del nodo trasero |
| `main/modules/gps/` | UART, parser NMEA, validación y velocidad del GPS |
| `main/modules/imu/` | Configuración de la BNO085 frontal |
| `main/modules/display/` | Puerto ST7796S/LVGL y vista de telemetría |
| `../Apoyo/components/` | Componentes compartidos `field_espnow` e `imu_bno085` |

El módulo de pantalla forma parte de la compilación, pero el `app_main`
actual no llama a `display_module_start()` ni a `display_module_update()`.
Por tanto, el comportamiento operativo actual se basa en los logs serie.

## Requisitos

- ESP-IDF compatible con el proyecto y `idf.py` disponible en el entorno.
- Placa ESP32-S3.
- Dependencia del directorio compartido `Apoyo`, esperado por
  `CMakeLists.txt` en `../Apoyo`.
- GPS6MV2.
- BNO085 configurada en modo SPI.
- Pantalla ST7796S opcional para la integración LVGL.

Las dependencias gestionadas por ESP-IDF se declaran en
[`main/idf_component.yml`](main/idf_component.yml): LVGL 9.3.0 y
`esp_lcd_st7796`.

## Compilación y carga

Desde este directorio, con el entorno de ESP-IDF activado:

```text
idf.py set-target esp32s3
idf.py build
idf.py -p COMx flash monitor
```

Sustituye `COMx` por el puerto serie de la placa. Para salir del monitor,
utiliza `Ctrl-]`.

Si el proyecto no encuentra `field_espnow` o `imu_bno085`, comprueba primero
que exista el directorio compartido en la ubicación esperada por
[`CMakeLists.txt`](CMakeLists.txt). Los pines y criterios del GPS se editan
directamente en
[`gps_config.h`](main/modules/gps/driver/gps_config.h); no se configuran
mediante `menuconfig`.

## Conexiones de hardware

### BNO085 frontal

Configuración activa en
[`imu_module.cpp`](main/modules/imu/imu_module.cpp):

| Señal | GPIO |
| --- | ---: |
| SPI host | `SPI3_HOST` |
| MOSI | 11 |
| MISO | 13 |
| SCLK | 12 |
| CS | 14 |
| HINT/INT | 6 |
| RESET | 5 |

Conecta `PS0=1` y `PS1=1` a `VCC` para seleccionar el modo SPI. La IMU
solicita orientación, aceleración lineal y giro a 100 Hz. Se considera fuera
de línea después de 500 ms sin una muestra nueva, según el componente común.

### GPS

La configuración inicial está en
[`gps_config.h`](main/modules/gps/driver/gps_config.h):

| Parámetro | Valor |
| --- | --- |
| UART | `UART2` |
| Baud rate | 9600 |
| RX del ESP32-S3 | GPIO 18 |
| TX del ESP32-S3 | GPIO 17 |
| Actualización esperada | 1 s |

El módulo valida RMC/GGA usando, entre otros criterios, un mínimo de
4 satélites, HDOP máximo de 4.0 y una antigüedad máxima de 1.8 s.

## Protocolo ESP-NOW

El master escucha en el canal `1` y acepta exclusivamente un paquete de
**17 bytes**, definido como una estructura empaquetada:

| Bytes | Campo | Tipo | Unidad |
| ---: | --- | --- | --- |
| 0-3 | `roll_1` | `float` | grados |
| 4-7 | `pitch_1` | `float` | grados |
| 8-11 | `ay_1` | `float` | m/s² |
| 12-15 | `water_level` | `float` | según el sensor RS-485 |
| 16 | `sensor_status_flags` | `uint8_t` | bits de estado |

Los bits definidos son:

- `STATUS_IMU1_OK` (`0x01`): datos de la IMU trasera válidos.
- `STATUS_RS485_OK` (`0x02`): lectura del sensor de nivel válida.

El emisor y el master deben compilar la misma definición de
[`TankPayload`](main/tank_payload.h), incluido el atributo `packed`. El master
no usa el protocolo anterior de 104 bytes ni ACK de aplicación. Si recibe
otro tamaño, descarta el paquete y lo informa en el log.

La conexión se considera activa durante 3 segundos desde el último paquete
válido. Después de ese tiempo, los datos del nodo trasero se publican como
no disponibles (`NaN`) y se registra una advertencia.

## Salida serie

El firmware escribe tres líneas de diagnóstico y una línea de datos cada
segundo:

```text
GPS|estado=...|lat=...|lon=...|velocidad_kmh=...|sat=...|hdop=...
IMU|online=...|roll=...|pitch=...|ay=...
ESPNOW|online=...|imu_roll=...|imu_pitch=...|imu_ay=...|agua=...|flags=...|seq=...
MASTERDATA|2|timestamp_us|gps_state|lat|lon|speed_kmh|satellites|hdop|rmc_age_ms|front_online|front_roll|front_pitch|front_ax|front_ay|front_az|rear_online|sequence|flags|rear_roll|rear_pitch|rear_ay|water_level
```

`MASTERDATA` es un formato delimitado por `|`. `timestamp_us` es tiempo
monotónico local desde el arranque, `sequence` incrementa con cada
`TankPayload` válido y `front_online`/`rear_online` indican disponibilidad.
Los valores que no tienen una muestra válida se imprimen como `nan`.

## Puntos de extensión y limitaciones

- La lógica de fusión de sensores y la detección de volcadura aún no están
  implementadas.
- La velocidad del GPS es la reportada por RMC y convertida a km/h; no se
  aplica el filtro de velocidad de la prueba 02.
- `TankPayload` no incluye un contador del emisor; `sequence` identifica
  recepciones válidas en el master, no paquetes únicos del nodo trasero.
- La pantalla ST7796S/LVGL está preparada como módulo, pero debe integrarse
  explícitamente en `app_main` si se requiere salida gráfica.
- Los pines y umbrales de GPS están codificados en archivos de configuración
  del módulo; cualquier cambio de hardware debe quedar documentado junto con
  el cableado.

## Diagnóstico rápido

| Síntoma | Qué revisar |
| --- | --- |
| `No se pudo iniciar el módulo BNO085` | SPI, alimentación, `PS0/PS1`, CS, HINT y RESET |
| `Sin TankPayload validos durante 3 s` | Canal ESP-NOW, tamaño de 17 bytes y flags del emisor |
| `GPS|estado=SIN_DATOS` | UART, TX/RX cruzados, baud rate y alimentación del GPS |
| Error al configurar CMake | Presencia de `../Apoyo` y componentes compartidos |
| No aparecen registros `MASTERDATA` | Monitor serie, velocidad configurada y arranque de la aplicación |
