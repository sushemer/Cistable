# SlaveEsp - Nodo de tanque

Firmware para un **ESP32-S3** que mide la inclinación y aceleración lateral de
un vehículo mediante una IMU **BNO08x**, lee el nivel de un tanque por
**RS-485/Modbus RTU**, controla la alimentación del sensor de nivel mediante un
relé y envía los datos a otro nodo usando **ESP-NOW**.

El punto de entrada y la mayor parte de la lógica están en
[`main/main.cpp`](./main/main.cpp).

## Funciones principales

- Inicializa una IMU BNO08x por SPI.
- Obtiene:
  - `roll` y `pitch` mediante el reporte **Game Rotation Vector**.
  - `ay` mediante **Linear Acceleration**.
- Enciende el sensor de nivel durante 5 segundos, hace lecturas Modbus y lo
  apaga durante 60 segundos.
- Selecciona internamente el modelo de tanque:
  - `Solid` cuando el tanque está casi vacío o cerca de lleno.
  - `Pendulum` entre esos límites.
- Envía un paquete binario de 17 bytes al nodo de cabina cada 20 ms.
- Publica mensajes de diagnóstico mediante `ESP_LOGI`, `ESP_LOGW` y
  `ESP_LOGE`.

## Requisitos

- ESP32-S3.
- ESP-IDF 5.x configurado en el entorno.
- Sensor BNO08x conectado en modo SPI.
- Sensor de nivel compatible con la consulta Modbus utilizada por el firmware.
- Transceptor RS-485 y un relé compatible con el nivel lógico del GPIO.
- Un segundo nodo ESP-NOW configurado para escuchar en el canal 1.

El componente de la IMU está incluido localmente en
[`components/esp32_BNO08x`](./components/esp32_BNO08x). Solo soporta SPI en
esta versión.

## Conexiones

### IMU BNO08x

| Señal | GPIO |
|---|---:|
| SPI MOSI | 3 |
| SPI MISO | 16 |
| SPI SCK | 15 |
| CS | 10 |
| HINT | 11 |
| RST | 12 |

El bus SPI usa `SPI2_HOST` a 3 MHz. Los pines se definen al inicio de
[`main/main.cpp`](./main/main.cpp).

### RS-485 / Modbus

| Señal | GPIO/valor |
|---|---:|
| UART | `UART_NUM_2` |
| TX | 17 |
| RX | 18 |
| Baud rate | 9600 |
| Formato | 8N1 |
| Dirección Modbus | 1 |
| Registro leído | `0x0004` |

La lectura usa la función Modbus `0x03` (Read Holding Registers) y espera un
registro de 16 bits. El CRC es el CRC-16 estándar de Modbus, transmitido en
orden bajo-alto.

### Relé

| Señal | GPIO/valor |
|---|---:|
| Control del relé | 4 |
| Nivel activo | 1 |

El firmware configura el relé como salida y comienza apagándolo.

### ESP-NOW

- Canal: `1`.
- Interfaz: Wi-Fi en modo estación.
- MAC del receptor: `D4:05:92:79:3E:EC`.
- Cifrado: desactivado.

El canal y la MAC deben coincidir con la configuración del nodo receptor.

## Compilación, carga y monitor serie

Desde una terminal con el entorno de ESP-IDF habilitado:

```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p COMx flash
idf.py -p COMx monitor
```

También se pueden combinar la carga y el monitor:

```powershell
idf.py -p COMx flash monitor
```

Sustituye `COMx` por el puerto real de la placa. El proyecto se define en
[`CMakeLists.txt`](./CMakeLists.txt) y el componente principal en
[`main/CMakeLists.txt`](./main/CMakeLists.txt).

## Flujo de ejecución

`app_main()` realiza estas operaciones:

1. Inicializa NVS, UART2, la IMU, ESP-NOW y el GPIO del relé.
2. Crea tres tareas FreeRTOS:
   - `sensor_task`: consulta continuamente la IMU cada 5 ms.
   - `rs485_task`: controla el relé, consulta Modbus cada segundo durante la
     ventana activa y actualiza el nivel.
   - `esp_now_task`: toma una instantánea protegida por `state_mux` y envía el
     paquete cada 20 ms.
3. Mantiene el programa ejecutándose en las tareas; `app_main()` no contiene un
   bucle principal adicional.

El estado compartido se protege con `state_mux`. Si se añade una variable
compartida entre tareas, debe seguir el mismo patrón de sección crítica.

## Formato del paquete ESP-NOW

El receptor debe utilizar la misma estructura empaquetada:

```cpp
struct __attribute__((packed)) TankPayload {
    float roll_1;
    float pitch_1;
    float ay_1;
    float water_level;
    uint8_t sensor_status_flags;
};
```

El tamaño esperado es **17 bytes**. Los campos `float` están en las unidades
proporcionadas por la IMU o por el sensor de nivel:

| Campo | Descripción |
|---|---|
| `roll_1` | Roll en grados |
| `pitch_1` | Pitch en grados |
| `ay_1` | Aceleración lineal en Y, normalmente m/s² |
| `water_level` | Nivel leído del registro Modbus |
| `sensor_status_flags` | Estado de los sensores |

Bits de `sensor_status_flags`:

- Bit 0 (`0x01`): IMU válida y actualizada en los últimos 500 ms.
- Bit 1 (`0x02`): última lectura RS-485 válida.

Si se cambia el orden, tipo o empaquetado de los campos, también debe cambiarse
el receptor. El `static_assert` de `main.cpp` ayuda a detectar cambios de
tamaño durante la compilación.

## Parámetros que se modifican con frecuencia

Las constantes de hardware y temporización están al principio de
[`main/main.cpp`](./main/main.cpp):

| Constante | Valor actual | Uso |
|---|---:|---|
| `SPI_CLOCK_SPEED_HZ` | 3 MHz | Velocidad del bus SPI |
| `RS485_BAUD_RATE` | 9600 | Velocidad del sensor Modbus |
| `SENSOR_ON_TIME_MS` | 5000 | Tiempo encendido del sensor |
| `SENSOR_OFF_TIME_MS` | 60000 | Tiempo apagado del sensor |
| `MODBUS_POLL_PERIOD_MS` | 1000 | Periodo de consulta Modbus |
| `FULL_TANK_VALUE` | 120 | Valor de tanque lleno |
| `PENDULUM_UPPER_VALUE` | 75% de lleno | Límite superior del modelo péndulo |
| `EMPTY_TANK_VALUE` | 10% de lleno | Límite inferior del modelo péndulo |
| `LEVEL_HYSTERESIS` | 5 | Histéresis para evitar cambios rápidos |
| `REPORT_INTERVAL_US` | 10000 | Reportes de IMU a 100 Hz |
| `ESPNOW_SEND_INTERVAL_MS` | 20 | Periodo de transmisión |

Al cambiar pines, actualiza tanto las constantes como el cableado físico.
Después de cambiar el protocolo o el formato del paquete, actualiza ambos
nodos antes de probar la comunicación.

## Cómo extender el firmware

### Añadir una nueva lectura

1. Añade el estado necesario junto a `IMUState` y `water_level`.
2. Actualízalo en la tarea responsable de la interfaz física.
3. Protege la escritura y la lectura con `state_mux`.
4. Añade el campo al `TankPayload` solo si el receptor también se modificará.
5. Actualiza el `static_assert` y esta documentación.

### Cambiar la lectura Modbus

Modifica `read_modbus_level()` para cambiar la dirección, función, registro,
longitud de respuesta o escala del sensor. Conserva la validación de:

- cantidad de bytes recibidos;
- dirección y función de respuesta;
- código de bytes;
- CRC.

Una lectura inválida debe dejar `rs485_valid` en `false`; no debe presentarse
como una lectura correcta.

### Cambiar el modelo de tanque

La selección se concentra en `model_for_level()`. Si se cambian los umbrales,
considera también la histéresis para evitar oscilaciones cuando el nivel esté
cerca de un límite.

### Cambiar el receptor ESP-NOW

Actualiza `CABIN_MAC` y, si es necesario, `ESPNOW_CHANNEL`. El receptor debe
usar el mismo canal, la misma interfaz y exactamente el mismo formato binario.

## Diagnóstico

Los mensajes se identifican con la etiqueta `TANQUE`. Algunos mensajes
esperados son:

- `IMU inicializada: reportes=OK`
- `No se pudo leer el sensor de agua`
- `ESP-NOW: fallo de transmision por radio`
- `Nodo Tanque operativo; payload ESP-NOW=17 bytes`

### La IMU no responde

- Comprueba alimentación y tierra común.
- Confirma que el BNO08x esté en modo SPI.
- Revisa MOSI, MISO, SCK, CS, HINT y RST.
- Verifica que ningún otro dispositivo esté usando el mismo CS.

### No hay lecturas de nivel

- Comprueba TX/RX y el transceptor RS-485.
- Confirma 9600 baudios, 8N1 y dirección Modbus 1.
- Verifica que el registro `0x0004` y la escala sean correctos para el sensor.
- Revisa la terminación y polaridad del bus RS-485.
- Observa si el relé realmente alimenta el sensor durante la ventana activa.

### ESP-NOW no transmite

- Confirma que el receptor use el canal 1.
- Comprueba la MAC configurada en `CABIN_MAC`.
- Asegúrate de que el receptor esté iniciado antes de probar.
- Compara el tamaño y el orden de `TankPayload` en ambos proyectos.

## Código legado dentro de `main.cpp`

Al final de [`main/main.cpp`](./main/main.cpp) existe un bloque grande bajo
`#if 0`. Ese bloque contiene una versión anterior del firmware enfocada en
calcular y mostrar el índice LTR por UART. **No se compila actualmente**.

La implementación activa es la que aparece antes de ese bloque y termina en el
`app_main()` que crea `sensor_task`, `rs485_task` y `esp_now_task`. Si se
reactiva el bloque antiguo, habrá que eliminar o renombrar el `app_main()` y
revisar las definiciones duplicadas antes de compilar.

## Recomendaciones de mantenimiento

- Mantén la configuración de hardware agrupada al inicio del archivo.
- No accedas directamente a estado compartido desde varias tareas sin
  protección.
- Mantén el `static_assert` del tamaño del paquete.
- Registra los errores de periféricos y no conviertas una lectura inválida en
  un valor válido.
- Tras modificar tiempos, pines o el protocolo, prueba primero cada periférico
  por separado y luego la transmisión ESP-NOW completa.
