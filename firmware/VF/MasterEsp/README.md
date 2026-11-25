# MasterEsp

Firmware del nodo **master** de un sistema de monitorización de estabilidad para un vehículo cisterna. Está desarrollado con **ESP-IDF 5.5.2** para un **ESP32-S3**.

El master:

- Lee la IMU BNO085 frontal por SPI.
- Recibe del nodo trasero, mediante ESP-NOW, la IMU trasera y el nivel de agua.
- Lee y valida la velocidad/posición de un GPS6MV2 por UART.
- Calcula el `Load Transfer Ratio` (LTR), riesgos estáticos y dinámicos, velocidad segura y efecto del movimiento del agua.
- Muestra el estado en una pantalla ST7796S usando LVGL.

> Este documento describe el estado actual del código. Antes de cambiar el cableado o el protocolo inalámbrico, hay que actualizar tanto este proyecto como el firmware del nodo trasero.

## 1. Arquitectura general

```text
                         ┌────────────────────────┐
 BNO085 frontal ────────►│                        │
 GPS6MV2 ───────────────►│        Master ESP32-S3  │────► LCD ST7796S
 Nodo trasero ─ ESP-NOW ►│                        │
                         └────────────────────────┘
```

El punto de entrada está en [`main/main.c`](main/main.c). `app_main()` crea los servicios y tareas, mientras que `sensor_task()` reúne las muestras y calcula la telemetría.

### Flujo de arranque

1. Se crea `s_data_mutex`, usado para proteger la telemetría compartida.
2. Se inicializa el bus SPI compartido para la pantalla.
3. Se inicia el lector GPS.
4. Se crean tareas independientes para la IMU frontal, pantalla, ESP-NOW, sensores y GUI.
5. IMU y ESP-NOW reintentan su inicialización cada cinco segundos si el hardware no está disponible.
6. La pantalla espera dos segundos antes de inicializar LVGL.

## 2. Organización del proyecto

| Ruta | Responsabilidad |
|---|---|
| [`main/main.c`](main/main.c) | Orquestación, calibración, cálculo de telemetría, tareas y logs. |
| [`main/tank_payload.h`](main/tank_payload.h) | Formato binario del paquete recibido del nodo trasero. |
| [`main/board_spi_pins.h`](main/board_spi_pins.h) | Bus SPI compartido y chip-select del LCD. |
| [`main/board_spi_pins.c`](main/board_spi_pins.c) | Inicialización del bus SPI común. |
| [`main/modules/imu/`](main/modules/imu/) | Configuración de la BNO085 frontal y acceso a su última muestra. |
| [`main/modules/gps/`](main/modules/gps/) | UART, parser NMEA, validación y cálculo de velocidad del GPS. |
| [`main/modules/ltr/`](main/modules/ltr/) | Cálculo del LTR y de la aceleración lateral segura. |
| [`main/modules/pendulum/`](main/modules/pendulum/) | Modelo de péndulo para simular el movimiento del agua. |
| [`main/modules/display/`](main/modules/display/) | Driver ST7796S, LVGL, interfaz de riesgo e imágenes de vehículos. |
| [`Apoyo/field_espnow/`](Apoyo/field_espnow/) | Inicialización de Wi-Fi/ESP-NOW y cola de recepción. |
| [`Apoyo/imu_bno085/`](Apoyo/imu_bno085/) | Adaptador C++ del driver externo de la BNO085. |
| [`CMakeLists.txt`](CMakeLists.txt) | Componentes externos y configuración del proyecto ESP-IDF. |
| [`main/CMakeLists.txt`](main/CMakeLists.txt) | Fuentes, includes y dependencias del componente principal. |

## 3. Hardware y pines

### SPI compartido: LCD

El bus compartido es `SPI2_HOST`:

| Señal | GPIO |
|---|---:|
| SCLK | 12 |
| MOSI | 11 |
| MISO | 13 |
| LCD CS | 10 |

Estos valores están en [`main/board_spi_pins.h`](main/board_spi_pins.h). Al cambiar un GPIO, revisar también que no exista conflicto con otro periférico y recompilar desde limpio.

### IMU frontal BNO085

La BNO085 usa un bus SPI separado, `SPI3_HOST`:

| Señal | GPIO |
|---|---:|
| MOSI | 6 |
| MISO | 7 |
| SCLK | 5 |
| CS | 4 |
| INT/HINT | 15 |
| RESET | 16 |

La configuración está en [`main/modules/imu/imu_module.cpp`](main/modules/imu/imu_module.cpp). El sensor se configura a 3 MHz, con reportes cada 20 ms y una tarea de servicio de 2 ms. PS0 y PS1 deben estar en nivel alto para seleccionar SPI.

### GPS6MV2

La configuración editable está en [`main/modules/gps/driver/gps_config.h`](main/modules/gps/driver/gps_config.h):

- UART2.
- 9600 baudios, 8N1.
- RX del ESP32 en GPIO18.
- TX sin usar (`UART_PIN_NO_CHANGE`).
- Se aceptan como utilizables, inicialmente, al menos 4 satélites y HDOP de hasta 4.0.

La velocidad no debe tratarse como válida hasta que `gps.validation.usable_for_speed` sea verdadero.

### LCD y botón

La pantalla ST7796S es de 320x480 y usa SPI2. Sus señales específicas se encuentran en [`main/modules/display/display_port.c`](main/modules/display/display_port.c):

- DC: GPIO9.
- RST: GPIO8.
- CS: GPIO10.

El botón de selección de vehículo está en GPIO1, con pull-up interno y activo en nivel bajo. Está definido en [`main/modules/display/interface.c`](main/modules/display/interface.c).

## 4. Protocolo del nodo trasero

Cada trama ESP-NOW válida debe tener exactamente `sizeof(TankPayload)` bytes: actualmente **17 bytes**, sin padding, porque la estructura está marcada como `packed`.

| Bytes | Campo | Unidad |
|---:|---|---|
| 0-3 | `roll_1` | grados |
| 4-7 | `pitch_1` | grados |
| 8-11 | `ay_1` | m/s² |
| 12-15 | `water_level` | escala del sensor, 0-120 |
| 16 | `sensor_status_flags` | bits de estado |

Flags definidos en [`main/tank_payload.h`](main/tank_payload.h):

- `STATUS_IMU2_OK` (`bit 0`): la IMU trasera está disponible.
- `STATUS_RS485_OK` (`bit 1`): la lectura del sensor de agua por RS485 es válida.

El canal ESP-NOW está fijado a `1` en `main.c` (`ESPNOW_CHANNEL`). El master registra su MAC al arrancar. El tiempo máximo aceptado sin recibir datos del esclavo es de tres segundos (`SENSOR_TIMEOUT_US`).

**Regla importante:** si se modifica `TankPayload`, modificar también el emisor y mantener idénticos el orden, tipos, unidades y tamaño de la estructura.

## 5. Cálculo de telemetría y riesgos

Los parámetros físicos y umbrales están agrupados al inicio de [`main/main.c`](main/main.c). Los más importantes son:

| Constante | Valor actual | Uso |
|---|---:|---|
| `VEHICLE_TRACK_M` | 1.60 m | Ancho de vía. |
| `VEHICLE_WHEELBASE_M` | 3.80 m | Distancia entre ejes. |
| `VEHICLE_GROSS_MASS_KG` | 5443 kg | Masa usada en el modelo del vehículo. |
| `CG_HEIGHT_EMPTY_M` / `CG_HEIGHT_FULL_M` | 1.20 / 1.50 m | Altura del centro de gravedad según llenado. |
| `STATIC_RISK_LTR_THRESHOLD` | 0.35 | Umbral de advertencia estática. |
| `PITCH_LTR_WARNING_THRESHOLD` | 0.45 | Umbral de advertencia longitudinal. |
| `CRITICAL_LTR_THRESHOLD` | 0.80 | Umbral crítico. |
| `STATIC_RISK_ROLL_THRESHOLD_DEG` | 8° | Umbral de inclinación lateral. |
| `REFERENCE_TURN_RADIUS_M` | 45 m | Radio usado para la predicción de giro. |

El módulo [`main/modules/ltr/ltr_module.h`](main/modules/ltr/ltr_module.h) usa:

```text
ay_efectiva = ay * cos(roll) + g * sin(roll)
LTR = 2 * h * ay_efectiva / (ancho_de_via * g)
```

La aceleración lineal de la IMU ya excluye la gravedad; no se debe restar `g` otra vez antes de llamar a `ltr_calculate()`.

### Nivel de agua y modelo de péndulo

`WATER_LEVEL_OVERRIDE` controla el origen del nivel:

- `-1`: usar la lectura recibida del nodo trasero.
- `0` a `120`: usar un valor fijo para pruebas.

El valor actual es `65`, por lo que **el firmware está usando un override fijo y no la lectura recibida**. Para una prueba real con el sensor, cambiarlo a `-1` y recompilar.

Entre 10% y 75% de llenado se activa el modelo de péndulo. Se usan longitudes de 0.50 m para movimiento lateral y 1.00 m para movimiento longitudinal. El volumen se estima con la geometría elíptica del tanque mediante `water_liters()`.

### Tare automático

Al arrancar se acumulan muestras estacionarias para compensar el montaje:

- IMU frontal: 30 muestras, ignorando muestras con giro mayor o igual a 0.05 rad/s.
- IMU trasera: 15 muestras válidas.

El tare no corrige el sensor físicamente; solo resta el promedio inicial. La función `telemetry_tare_sensors()` reinicia ambos procesos y puede conectarse a un botón o comando si se necesita recalibrar durante una prueba.

## 6. Tareas y concurrencia

| Tarea | Prioridad | Pila | Función |
|---|---:|---:|---|
| `imu_init` | 3 | 4096 | Inicializa/reintenta la BNO085. |
| `display_init` | 2 | 8192 | Espera el arranque de la pantalla e inicia LCD/LVGL. |
| `espnow_init` | 3 | 4096 | Inicializa/reintenta ESP-NOW. |
| `rollover_sensor` | 5 | 8192 | Recibe sensores, calcula telemetría y registra datos. |
| `rollover_gui` | 2 | 4096 | Obtiene GPS y actualiza la pantalla. |

La telemetría compartida (`s_telemetry`, IMU frontal y datos traseros) se protege con `s_data_mutex`. Si se añade una lectura o escritura a esas variables desde otra tarea, usar el mismo mutex. LVGL tiene además su propio bloqueo interno en el módulo de pantalla.

## 7. Compilar, grabar y monitorizar

Abrir una terminal con el entorno de ESP-IDF activo y ejecutar desde la raíz:

```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p COMx flash
idf.py -p COMx monitor
```

Sustituir `COMx` por el puerto real. Para recompilar después de cambiar `CMakeLists.txt`, dependencias o configuración de componentes:

```powershell
idf.py fullclean
idf.py build
```

Para cambiar opciones de ESP-IDF:

```powershell
idf.py menuconfig
```

El archivo [`sdkconfig`](sdkconfig) es generado por ESP-IDF; preferir `menuconfig` en vez de editarlo a mano.

## 8. Cómo modificar el sistema

### Cambiar un pin

1. Para SPI compartido, editar [`main/board_spi_pins.h`](main/board_spi_pins.h).
2. Para la IMU frontal, editar [`main/modules/imu/imu_module.cpp`](main/modules/imu/imu_module.cpp).
3. Para GPS, editar [`main/modules/gps/driver/gps_config.h`](main/modules/gps/driver/gps_config.h).
4. Para LCD o botón, editar [`main/modules/display/display_port.c`](main/modules/display/display_port.c) o [`main/modules/display/interface.c`](main/modules/display/interface.c).
5. Revisar conflictos de GPIO y hacer `fullclean` antes de volver a compilar.

### Cambiar un umbral o constante física

Editar las constantes al principio de [`main/main.c`](main/main.c), documentar la unidad y confirmar el efecto en los logs `[DATA] IMU_A`. No cambiar un umbral de seguridad basándose solo en la pantalla: validar también el LTR firmado, el LTR predicho y la calidad de los sensores.

### Cambiar la interfaz

- Layout, colores y textos: [`main/modules/display/interface.c`](main/modules/display/interface.c).
- Puente de telemetría a la pantalla: [`main/modules/display/display_module.c`](main/modules/display/display_module.c).
- Driver, resolución y pines LCD: [`main/modules/display/display_port.c`](main/modules/display/display_port.c).
- Imágenes de vehículos: [`main/modules/display/recursos/`](main/modules/display/recursos/).

### Añadir un sensor o módulo

1. Crear una interfaz `.h` pequeña con funciones de `start` y `get/snapshot`.
2. Ejecutar su tarea propia si la lectura es bloqueante.
3. Copiar la muestra bajo `s_data_mutex` si la consume `sensor_task()` o `gui_task()`.
4. Añadir el archivo fuente y las dependencias en [`main/CMakeLists.txt`](main/CMakeLists.txt).
5. Añadir una línea de log y, si corresponde, un campo en `display_telemetry_t`.

## 9. Diagnóstico rápido

| Síntoma | Revisar |
|---|---|
| `IMU no disponible` | Pines 5/6/7/4/15/16, PS0/PS1, alimentación y conflicto de SPI3. |
| `ESP-NOW no disponible` o `SIN_RX` | Canal 1, alimentación del esclavo, tamaño de `TankPayload` y MAC/paquetes recibidos. |
| `GPS SIN_FIX_UTIL` | GPIO18, 9600 baudios, antena, satélites, HDOP y antigüedad de la muestra. |
| Pantalla apagada | SPI2, CS/DC/RST, alimentación y dependencia ST7796S. |
| El nivel no cambia | Confirmar que `WATER_LEVEL_OVERRIDE` sea `-1`. |
| Tare incorrecto | Mantener el vehículo quieto durante el arranque y comprobar los mensajes de finalización. |

## 10. Dependencias y notas de mantenimiento

Las dependencias declaradas directamente están en [`main/idf_component.yml`](main/idf_component.yml): LVGL 9.3.0 y el driver ST7796. Los componentes propios adicionales se conectan desde [`CMakeLists.txt`](CMakeLists.txt).

El driver externo de BNO08x se documenta en [`Apoyo/README.md`](Apoyo/README.md). Si falta `Apoyo/components/esp32_BNO08x`, instalarlo según esas instrucciones antes de compilar.

No editar el contenido de `build/` ni de `managed_components/` para corregir el firmware. Son artefactos generados o dependencias administradas; los cambios permanentes deben hacerse en `main/`, `Apoyo/`, `CMakeLists.txt`, `main/CMakeLists.txt`, `main/idf_component.yml` o mediante `menuconfig`.
