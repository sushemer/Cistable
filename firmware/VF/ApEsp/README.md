# Sistema de telemetría LTR

Este repositorio contiene tres proyectos ESP-IDF para supervisar el riesgo de
volcadura de un vehículo cisterna:

- **ApEsp**: ESP32 central. Crea la red Wi-Fi, recibe telemetría y sirve la
  interfaz web.
- **MasterEsp**: nodo de cabina. Integra la IMU frontal, GPS, pantalla, microSD,
  cálculo LTR y recepción ESP-NOW del nodo cisterna.
- **SlaveEsp**: nodo de cisterna. Lee la IMU trasera y el nivel de agua, y
  transmite el paquete rápido por ESP-NOW al MasterEsp.

La lógica compartida entre los tres proyectos está en [`shared/`](../shared).
Los proyectos están pensados para **ESP-IDF 5.5.2**.

## 1. Arquitectura general

```text
                         Wi-Fi / UDP / HTTP
              +-------------------------------------+
              |                                     |
        +-----v-----+                         +-----v-----+
        | MasterEsp |                         | SlaveEsp  |
        | Cabina    |                         | Cisterna  |
        +-----+-----+                         +-----+-----+
              ^                                      |
              |                                      |
              +------------ ESP-NOW -----------------+
                             20 ms

                         +-----------+
                         |  ApEsp    |
                         | SoftAP    |
                         +-----------+
```

ApEsp crea la red:

| Parámetro | Valor por defecto |
|---|---|
| SSID | `Cisterna_LTR_AP` |
| Contraseña | `12345678` |
| Canal | `1` |
| Puerto UDP | `5000` |
| Puerto HTTP | `80` |

MasterEsp y SlaveEsp mantienen sus tareas de sensores y, cuando encuentran
el AP, envían reportes binarios por UDP a `192.168.4.1:5000`.

## 2. Estructura de carpetas

```text
V1/
├── shared/
│   ├── network_protocol.h     # Constantes, estructuras y CRC
│   └── network_client.h      # API del cliente de telemetría
├── ApEsp/
│   ├── CMakeLists.txt
│   ├── partitions.csv
│   ├── sdkconfig.defaults
│   ├── data/
│   │   ├── index.html         # Estructura de la página
│   │   ├── style.css          # Estilos
│   │   └── app.js             # Actualización y comandos
│   └── main/
│       ├── main.c             # SoftAP, UDP, HTTP, SPIFFS e historial
│       ├── ble_service.c      # Servicio BLE GATT
│       └── CMakeLists.txt
├── MasterEsp/
│   └── main/
│       └── network_client.c   # Búsqueda del AP y reportes del Master
└── SlaveEsp/
    └── main/
        └── network_client.c   # Búsqueda del AP y reportes del Slave
```

## 3. Compilar y grabar

Cada proyecto se compila desde su propia carpeta. Primero debe estar cargado
el entorno de ESP-IDF:

```text
cd ApEsp
idf.py set-target esp32
idf.py build
idf.py -p COMx flash monitor
```

Para MasterEsp y SlaveEsp:

```text
cd MasterEsp
idf.py set-target esp32s3
idf.py build
idf.py -p COMx flash monitor
```

Sustituye `COMx` por el puerto serie real. El `sdkconfig` existente de cada
proyecto puede conservarse; al cambiar de familia de chip conviene ejecutar
`idf.py fullclean` antes de recompilar.

ApEsp genera automáticamente la imagen SPIFFS mediante:

```cmake
spiffs_create_partition_image(spiffs "${CMAKE_CURRENT_LIST_DIR}/../data" FLASH_IN_PROJECT)
```

Por eso los cambios en `data/` se incorporan al ejecutar `idf.py build`.

## 4. Configuración central

Todas las constantes de red y tiempos están en
[`shared/network_protocol.h`](../shared/network_protocol.h).

### Red

```c
#define AP_WIFI_SSID         "Cisterna_LTR_AP"
#define AP_WIFI_PASS         "12345678"
#define AP_WIFI_CHANNEL      1
#define AP_SERVER_PORT       5000
#define AP_SEARCH_TIMEOUT_MS 10000
```

Si cambias el SSID, contraseña, canal o puerto, recompila los tres proyectos.
El canal debe ser el mismo para Wi-Fi y ESP-NOW.

### Intervalos de reporte

```c
#define REPORT_INTERVAL_FAST_MS 2000
#define REPORT_INTERVAL_MED_MS  5000
#define REPORT_INTERVAL_SLOW_MS 10000
#define DEFAULT_REPORT_INTERVAL_MS REPORT_INTERVAL_FAST_MS
```

Actualmente los clientes usan `DEFAULT_REPORT_INTERVAL_MS`. Los comandos web
para cambiar el intervalo ya están definidos en el protocolo, pero el cliente
de nodo todavía debe implementar la aplicación de esos comandos para que el
cambio sea persistente.

### Modo de prueba del nivel de agua

```c
#define WATER_SENSOR_TEST_MODE  1
#define WATER_SENSOR_TEST_VALUE 65.0f
```

- `1`: SlaveEsp usa el valor fijo y marca
  `NETWORK_FLAG_WATER_SIMULATED`.
- `0`: SlaveEsp intenta leer el sensor Modbus RS485.

## 5. Protocolo binario

Todas las tramas siguen este formato:

```text
| Header | Comando | Longitud | Payload | CRC XOR |
| 1 byte | 1 byte  | 1 byte   | n bytes | 1 byte  |
```

El CRC es un XOR de todos los bytes desde `Header` hasta el último byte del
payload. Las funciones de codificación y validación están en
[`network_protocol.h`](../shared/network_protocol.h):

- `network_crc8(...)`
- `network_packet_encode(...)`
- `network_packet_validate(...)`

### Headers y comandos

| Constante | Valor | Uso |
|---|---:|---|
| `NETWORK_HEADER_AP` | `0x3A` | Trama originada por ApEsp |
| `NETWORK_HEADER_NODE` | `0x7A` | Trama originada por MasterEsp o SlaveEsp |
| `NETWORK_CMD_STATUS` | `0xB6` | Reporte de estado |
| `NETWORK_CMD_REPORT_RATE` | `0x18` | Cambiar frecuencia |
| `NETWORK_CMD_ENABLE` | `0x4C` | Habilitar/deshabilitar reportes |
| `NETWORK_CMD_SLEEP` | `0x9E` | Solicitar ahorro de energía |
| `NETWORK_CMD_TANK_CONFIG` | `0x2D` | Actualizar medidas del tanque |

### Payload de estado

`network_status_payload_t` está empaquetado sin padding:

| Campo | Tipo | Descripción |
|---|---|---|
| `node_id` | `uint8_t` | `1` para Master, `2` para Slave |
| `flags` | `uint8_t` | Estado de sensores y conexión |
| `roll_deg` | `float` | Roll |
| `pitch_deg` | `float` | Pitch |
| `lateral_accel_m_s2` | `float` | Aceleración lateral |
| `gps_speed_kmh` | `float` | Velocidad GPS |
| `water_level` | `float` | Nivel del agua |
| `ltr` | `float` | LTR calculado |

Cuando se modifique esta estructura, deben recompilarse ApEsp, MasterEsp y
SlaveEsp. También debe actualizarse cualquier código que compruebe
`sizeof(network_status_payload_t)`.

## 6. ApEsp por dentro

El punto de entrada está en [`ApEsp/main/main.c`](main/main.c).

### Inicio

`app_main()` realiza estas operaciones:

1. Inicializa NVS.
2. Inicializa la pila de red.
3. Configura el SoftAP.
4. Monta SPIFFS en `/spiffs`.
5. Crea el mutex del estado.
6. Inicia el servidor HTTP y la tarea UDP.
7. Inicia el servicio BLE.

### Tarea UDP

`udp_task()` escucha el puerto `AP_SERVER_PORT`, valida:

- longitud de la trama;
- CRC;
- header `NETWORK_HEADER_NODE`;
- comando `NETWORK_CMD_STATUS`;
- tamaño exacto del payload.

Los datos válidos se guardan en:

- `s_latest[3]`: último estado por nodo;
- `s_history[10]`: historial circular;
- `s_history_count` y `s_history_head`: control del historial.

El acceso a estas variables está protegido por `s_mutex`.

### API HTTP

| Ruta | Método | Función |
|---|---|---|
| `/` | GET | Página principal |
| `/style.css` | GET | Hoja de estilos |
| `/app.js` | GET | JavaScript del dashboard |
| `/api/status` | GET | Estado actual en JSON |
| `/api/history` | GET | Historial en JSON |
| `/api/command` | POST | Recibe una trama binaria de comando |

Para agregar una ruta:

1. Crea un handler `static esp_err_t`.
2. Añade una entrada al arreglo `routes` en `start_http()`.
3. Si sirve un archivo, colócalo en `ApEsp/data/`.
4. Recompila para regenerar SPIFFS.

## 7. Interfaz web

La interfaz no usa CDN ni dependencias externas.

- [`data/index.html`](data/index.html): tarjetas, tabla e inputs.
- [`data/style.css`](data/style.css): colores y diseño responsivo.
- [`data/app.js`](data/app.js): consultas periódicas y construcción de
  comandos binarios.

El navegador consulta `/api/status` cada dos segundos y `/api/history` en cada
actualización. Para añadir un dato:

1. Añádelo al JSON generado por `status_handler()`.
2. Léelo en `refresh()` dentro de `app.js`.
3. Añade el elemento HTML correspondiente.

La función `command(command, payload)` construye el paquete, calcula el XOR y
lo envía a `/api/command`.

## 8. MasterEsp y SlaveEsp

Los clientes están en:

- [`MasterEsp/main/network_client.c`](../MasterEsp/main/network_client.c)
- [`SlaveEsp/main/network_client.c`](../SlaveEsp/main/network_client.c)

Cada cliente:

1. Configura el modo estación.
2. Fija el canal Wi-Fi.
3. Escanea el SSID del AP.
4. Se conecta si lo encuentra.
5. Crea un socket UDP.
6. Solicita una muestra mediante un callback.
7. Codifica y envía `NETWORK_CMD_STATUS`.

El callback de Master toma los valores de la telemetría ya calculada por el
nodo. El callback de Slave toma la IMU, el nivel de agua y sus flags.

Para añadir un campo al reporte:

1. Modifica `network_status_payload_t`.
2. Completa el campo en `provide_network_status()` de ambos nodos.
3. Actualiza el JSON del ApEsp.
4. Actualiza la interfaz web si corresponde.

No dupliques el cálculo LTR en el cliente de red: el cliente debe transportar
la telemetría que ya producen las tareas de sensores.

## 9. BLE

El servicio BLE está en
[`ApEsp/main/ble_service.c`](main/ble_service.c).

Publica un servicio GATT con una característica de lectura/notificación. El
valor actual es una cadena mínima (`LTR_STATUS`). Para publicar el LTR real:

1. Define una fuente de datos segura y protegida por mutex.
2. Reemplaza el contenido de `alert_access()`.
3. Si se requiere actualización automática, conserva el handle
   `s_value_handle` y llama a la notificación cuando cambie el estado.

## 10. Hardware y código existente

La integración de red no debe cambiar los pines físicos existentes. Los
módulos principales de MasterEsp se encuentran en
[`MasterEsp/main/modules/`](../MasterEsp/main/modules/):

- `imu/`: lectura BNO085 frontal;
- `gps/`: GPS y velocidad;
- `ltr/`: cálculo del índice LTR;
- `pendulum/`: modelo dinámico del líquido;
- `display/`: LCD ST7796S y LVGL;
- `sd_logger/`: registro en microSD.

SlaveEsp concentra su implementación en
[`SlaveEsp/main/main.cpp`](../SlaveEsp/main/main.cpp), incluyendo IMU, Modbus
RS485, relé del sensor y transmisión ESP-NOW.

## 11. Diagnóstico

### No aparece la red Wi-Fi

- Comprueba que ApEsp arranque correctamente.
- Verifica que `AP_WIFI_CHANNEL` sea `1` en los tres nodos.
- Confirma que la contraseña tenga al menos ocho caracteres.
- Revisa el monitor serie de ApEsp.

### ApEsp no recibe telemetría

- Comprueba que el nodo tenga IP asignada por el SoftAP.
- Verifica que el nodo se haya conectado antes de iniciar los reportes.
- Revisa que el payload tenga el tamaño esperado.
- Revisa los mensajes de CRC descartado.

### El dashboard muestra `OFFLINE`

El dashboard considera online un nodo cuando su flag contiene
`NETWORK_FLAG_AP_ONLINE`. Confirma que el callback del nodo se esté ejecutando
y que los reportes lleguen al puerto UDP 5000.

### El nivel de agua no cambia

Comprueba `WATER_SENSOR_TEST_MODE`. Con valor `1`, el valor es fijo por diseño.

## 12. Limitaciones actuales y próximos pasos

La base está preparada para ampliar la integración, pero estas funciones
requieren trabajo adicional antes de considerarse completas en una instalación
real:

- Los comandos recibidos por ApEsp se validan y se retransmiten por broadcast,
  pero MasterEsp y SlaveEsp todavía deben implementar el parser y aplicar
  `0x18`, `0x4C`, `0x9E` y `0x2D`.
- `AP_SEARCH_TIMEOUT_MS` está definido como configuración compartida; la tarea
  de cliente debe usar un temporizador explícito para garantizar exactamente
  ese límite y después abandonar el modo de búsqueda.
- El callback BLE publica un valor de demostración; debe conectarse al estado
  LTR protegido por mutex.
- Antes de desplegar, debe probarse la compilación con ESP-IDF 5.5.2 y validar
  los tamaños binarios en hardware.
- Deben añadirse pruebas de CRC, tramas truncadas, comandos inválidos y pérdida
  de conexión.

Estas limitaciones están documentadas para que una modificación futura no
confunda una interfaz preparada con una función ya aplicada al hardware.
