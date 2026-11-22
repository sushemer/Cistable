# MasterEsp para ESP32-S3 con registro microSD

Base: proyecto `firmware/MasterEsp` del compañero De la Peña en la rama `Twig`,

El Master lee la IMU frontal, recibe la IMU trasera y el agua por ESP-NOW,
lee GPS6MV2 y actualiza la pantalla ST7796S. Las líneas `[DATA]` y los eventos
se guardan en la microSD desde una tarea de escritura independiente.

## Conexiones del ESP32-S3

Los números de las tablas son **GPIO del ESP32-S3**, no posiciones físicas
del conector de la placa. El mapa corresponde a las definiciones activas del
firmware.

| Interfaz | Dispositivos | Configuración |
|---|---|---|
| SPI2 compartido | Pantalla ST7796S y lector microSD | SCK=12, MOSI=11 y MISO=13; cada dispositivo tiene su propio CS. |
| SPI3 | IMU frontal BNO085 | SCK=5, MOSI=6 y MISO=7; reloj de 3 MHz. |
| UART2 | GPS6MV2 | RX del ESP32-S3=18; 9600 baudios. |

### 1. Pantalla ST7796S — SPI2

| Pin de la pantalla | Conectar en el ESP32-S3 | Función |
|---|---|---|
| SCK / CLK | GPIO12 | Reloj compartido con la microSD. |
| MOSI / SDI / DIN | GPIO11 | Datos del ESP32-S3 hacia la pantalla. |
| CS | GPIO10 | Selección exclusiva de la pantalla. |
| DC / RS | GPIO9 | Selección entre datos y comandos. |
| RESET / RST / RES | GPIO8 | Reinicio de la pantalla. |
| GND | GND común | Tierra del sistema. |

La pantalla funciona a **20 MHz**. El MISO del bus se configura en GPIO13
para recibir datos de la microSD; la tabla de la pantalla conserva las
conexiones de escritura del montaje actual.

El touch permanece deshabilitado. **GPIO15 está asignado a HINT/INT de la
IMU frontal**.

Definición de pines: [display_port.c](main/modules/display/display_port.c).

### 2. Lector microSD y SPI2 compartido con la pantalla

| Pin del lector microSD | Conectar en el ESP32-S3 | Función |
|---|---|---|
| SCK / CLK | GPIO12 | Mismo reloj que la pantalla. |
| MOSI / DI / DIN | GPIO11 | Misma salida de datos que la pantalla. |
| MISO / DO / DOUT | GPIO13 | Datos de la tarjeta hacia el ESP32-S3. |
| CS / SS | GPIO14 | Selección exclusiva de la microSD. |
| GND | GND común | Tierra del sistema. |

La microSD funciona hasta **4 MHz**. La conexión compartida queda así:

```text
GPIO12 ── SCK de pantalla y SCK de microSD
GPIO11 ── MOSI de pantalla y DI de microSD
GPIO13 ── DO de microSD
GPIO10 ── CS de pantalla
GPIO14 ── CS de microSD
```

El controlador SPI aplica a cada dispositivo su frecuencia y su selección CS.

Definición de pines:
[sd_logger_config.h](main/modules/sd_logger/sd_logger_config.h).

### 3. IMU frontal BNO085 — SPI3

| Pin de la IMU | Conectar en el ESP32-S3 | Función |
|---|---|---|
| SCK / SCLK | GPIO5 | Reloj SPI de la IMU. |
| MOSI / DI | GPIO6 | Datos del ESP32-S3 hacia la IMU. |
| MISO / DO | GPIO7 | Datos de la IMU hacia el ESP32-S3. |
| CS | GPIO4 | Selección de la IMU. |
| HINT / INT | GPIO15 | Interrupción de la IMU hacia el ESP32-S3. |
| RESET / RST | GPIO16 | Reinicio de la IMU. |
| PS0 | VCC de la IMU | Nivel alto para seleccionar SPI. |
| PS1 | VCC de la IMU | Nivel alto para seleccionar SPI. |
| GND | GND común | Tierra del sistema. |

Este bus es independiente del SPI2 de pantalla y microSD. La configuración
actual de la IMU usa un reloj SPI de **3 MHz**.

Definición de pines: [imu_module.cpp](main/modules/imu/imu_module.cpp).

### 4. GPS6MV2 — UART2

| Pin del GPS | Conexión | Función |
|---|---|---|
| TX | GPIO18, RX de UART2 del ESP32-S3 | Salida NMEA del GPS hacia el Master. |
| RX | Sin conexión de transmisión en este montaje | El firmware utiliza la recepción del GPS. |
| GND | GND común | Tierra del sistema. |

La comunicación está configurada a **9600 baudios**. La conexión de datos
es **TX del GPS → GPIO18 del ESP32-S3**.

Definición de pines: [gps_config.h](main/modules/gps/driver/gps_config.h).


### Alimentación y conexión con el Slave

Todos los dispositivos comparten **GND**. Las señales GPIO del ESP32-S3
son de **3.3 V**. El lector microSD utilizado en la prueba anterior recibió
**5 V en VCC**; ese valor corresponde a ese módulo lector. La alimentación
de la pantalla, el GPS y la BNO085 depende de la placa concreta y se conserva
según el montaje del compañero.

La IMU trasera y el sensor de agua se conectan al **Slave**. El Master recibe
sus datos por **ESP-NOW**, sin un cable de datos entre ambas placas.

## Archivos en la tarjeta

Cada arranque crea una carpeta nueva tomando el número mayor existente y
sumando uno. Si falla el almacenamiento y se recupera, se crea otra carpeta.

```text
RAW_0020/
├── RAW_0020.LOG
├── IMU1.CSV
├── IMU2.CSV
├── IMU_A.CSV
├── GPS.CSV
├── EVENT.CSV
└── OTRO.CSV
```

- `IMU1`: IMU frontal local, aceleración, giro y LTR individual.
- `IMU2`: IMU trasera recibida por ESP-NOW, agua, banderas y LTR individual.
- `IMU_A`: cálculo agregado de las IMUs realizado por el Master.
- `GPS`: coordenadas, velocidad, fix, satélites, HDOP y LTR asociado.
- `EVENT`: disponibilidad de GPS, IMU y recepción remota, LTR y estado de riesgo.
- `OTRO`: líneas recibidas por el logger que no tienen un formato reconocido.
- `.LOG`: copia de todas las líneas entregadas al logger, con tiempo y origen.

Los CSV tienen títulos y una indicación `sep=,` para que Excel separe las
columnas al abrirlos. El tiempo del ESP se guarda en microsegundos; fecha y
hora local de Tijuana se completan después de una RMC válida del GPS.
Antes de sincronizar, las columnas de calendario quedan vacías.
