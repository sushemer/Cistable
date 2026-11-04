# Prueba en automóvil: pipa virtual con entradas reales

Guía vigente para **ambos ESP32-S3**. Preparación de software, sin pruebas físicas realizadas. Rama: `Jesus/integracion-cabina`. El cableado ESP32 clásico se conserva únicamente como historial.

El automóvil aporta movimiento al IMU trasero; el resultado es **LTR estimado de una PIPA VIRTUAL / EXPERIMENTAL**, no LTR del automóvil ni predicción validada de volcadura. El IMU de cabina es diagnóstico y no se promedia con el trasero. GPS real condiciona el controlador. Agua CONFIGURADA, inicialmente 50 % por volumen; sonda desconectada. Las estimaciones no habilitan el perfil real incompleto. La demo sintética es un tercer modo independiente.

Complementos: [hardware y alimentación](hardware-esp32s3.md), [ecuaciones, estimaciones y sensibilidad](pipa-virtual.md), [formato y capacidad de captura](captura-prueba-fisica.md), [verificaciones publicadas](verificaciones-integracion.md).

## A. Preparación y conexiones

Cabina: ESP32-S3 de Peralta con BNO085, GPS y ST7796S; se cablea de nuevo. Trasero: ESP32-S3 de Chris con BNO085 soldado; **conservar sus soldaduras y montaje**. La publicación dice WROOM-1-N16R8 (Flash 16 MB Quad, PSRAM 8 MB Octal); comprobar inscripción y memoria al arrancar. No acredita revisión de placa, regulador ni conector USB concreto.

| Placa / dispositivo | Señal | GPIO / bus | Situación |
|---|---|---|---|
| Cabina ST7796S | MOSI, SCK, CS | 11, 12, 10 / SPI2 | Nuevo cableado por comprobar |
| Cabina ST7796S | DC, RST | 9, 8 / GPIO | Nuevo; MISO sin conectar |
| Cabina BNO085 | MOSI, MISO, SCK | 6, 7, 5 / SPI3 | Nuevo cableado por comprobar |
| Cabina BNO085 | CS, INT, RST | 4, 15, 16 | Selección SPI del carrier por comprobar |
| Cabina GPS | TX GPS → RX ESP; TX ESP → RX GPS | 18; 17 / UART2, 9600 8N1 | Segunda dirección opcional; ambos GPIO reservados |
| Cabina botón opcional | Contacto a GND | 21 / pull-up interno | Deshabilitado; selección Modelo A |
| Trasero BNO085 Chris | MOSI, MISO, SCK | 18, 16, 15 / SPI2 | Conservar |
| Trasero BNO085 Chris | CS, INT, RST | 10, 11, 12 | Conservar |
| Trasero RS485/agua | TX, RX, DE/RE | 17, 13, 14 / UART1 | Conservados; UART/sonda deshabilitadas en ensayo |
| Ambas consolas | TX, RX | 43, 44 / UART0, 921600 8N1 | Identificar puente USB-UART y COM de cada nodo |
| Ambas USB nativo | D−, D+ | 19, 20 | Reservados; alternativa explícita, no estos perfiles UART |
| Cada placa/periféricos | GND | GND local común | Comprobar continuidad |

Pantalla sin touch ni SD; BL/VCC no tienen una tensión deducible del nombre ST7796S. Identificar breakout de pantalla, carrier BNO y modelo de GPS antes de conectar alimentación. El módulo WROOM usa 3.3 V y su lógica es 3.3 V; esto no determina la entrada VIN/5V de una placa de desarrollo. No aplicar lógica de 5 V a GPIO. Conservar alimentación previamente comprobada de Chris. [Tabla completa, restricciones de memoria/arranque y fuentes primarias](hardware-esp32s3.md).

Preparar placas/periféricos disponibles, cables de datos USB, computadora con dos puertos identificados, alimentación comprobada y sujeciones firmes. No hacen falta sensores adicionales. Identificar cada COM conectando una placa a la vez: `[System.IO.Ports.SerialPort]::GetPortNames()`. COM5 cabina y COM6 trasero son ejemplos: reemplazarlos. Confirmar qué conector va al puente UART0; no deducirlo por su posición en una foto.

## B. Perfiles y alcance

| Perfil | Cabina | Trasero | Flags efectivos de ensayo |
|---|---|---|---|
| 100 Hz | `build/s3/virtual100` | `build/s3/capture100` | `CI_PIPA_VIRTUAL=y` en cabina, `CI_FULL_CAPTURE=y`, `CI_BENCH_50HZ=n` en ambos |
| 50 Hz, ensayo separado | `build/s3/virtual50` | `build/s3/capture50` | Iguales; `CI_BENCH_50HZ=y` en ambos |

Ambos target `esp32s3`. Cabina: GPS e IMU local habilitados, pantalla y ESP-NOW activos, botón deshabilitado, Modelo A. `CI_VIRTUAL_FILL_PERCENT=50`, `CI_VIRTUAL_LEVER=n`, `CI_VIRTUAL_MOUNT_CONFIRMED=n` al preparar. Trasero: `CI_REAR_WATER_ENABLED=n`; no se generan respuestas Modbus ni tiempos ficticios. El agua configurada no caduca como sensor; IMU/GPS conservan controles de frescura y continuidad.

Flash: opción QIO 16 MB/80 MHz; PSRAM Octal 80 MHz, inicialización y memtest solicitados, acceso por capacidades (malloc normal interno). Cabecera de imagen DIO intencional en este SDK, antes de habilitar Quad en el segundo bootloader. Particiones conservadas: app cabina 1500 KiB, trasero 1 MiB. Consola primaria UART0 43/44 a 921600, sin consola secundaria duplicada.

Dos cargas manuales: **diagnóstico con montaje no confirmado**, después **ejes comprobados, configuración confirmada, recompilación, nueva carga y rearme**. No editar `MOUNT_CONFIRMED` sólo para obtener LTR.

## C. Qué datos puedo cambiar y dónde

Todos los cambios siguientes requieren **recompilar y flashear manualmente**. `menuconfig` guarda el perfil seleccionado, no todos los perfiles. Menú raíz cabina: `Cistable integration (experimental)`, en [Kconfig.projbuild](../firmware/integracion/cabina/main/Kconfig.projbuild). Se omite el prefijo `CONFIG_` de sdkconfig.

| Parámetro / significado | Unidad; referencia y signo | Menú/archivo exacto | Valor inicial | Perfil |
|---|---|---|---|---|
| Masa chasis + cabina, sin cisterna/agua | kg, positiva | `Parametros fisicos PIPA VIRTUAL` → `CI_VIRTUAL_CHASSIS_MASS_KG` | 5000, escenario estimado | Virtual |
| Masa casco cisterna | kg | Mismo menú → `CI_VIRTUAL_SHELL_MASS_KG` | 924, estimación recuperada | Virtual |
| Plataforma y bomba | kg; no duplicar casco | `CI_VIRTUAL_PLATFORM_MASS_KG` | 600 = 350+250, supuesto | Virtual |
| Combustible | kg | `CI_VIRTUAL_DIESEL_MASS_KG` | 173.888 = 209 L × 0.832 kg/L, supuesto | Virtual |
| Tripulación/equipo | kg | `CI_VIRTUAL_CREW_MASS_KG` | 200 = 2×80+40, supuesto | Virtual |
| CG de componentes | m, Z desde suelo virtual; X delante, Y izquierda, Z arriba | Mismo menú → `CI_VIRTUAL_CHASSIS_CG_M`, `PLATFORM_CG_M`, `DIESEL_CG_M`, `CREW_CG_M` (todos con prefijo CI_VIRTUAL) | 1.0; 1.3; 0.8; 1.2; casco al centro geométrico | Virtual |
| CG seco agregado opcional | m XYZ; Z sobre suelo | `CI_VIRTUAL_OVERRIDE_CG`, `CI_VIRTUAL_CG_X_M/Y_M/Z_M` | Override n; 0/0/1.13 si se activa | Virtual |
| Vía de apoyo efectiva | m, distancia lateral agregada | `CI_VIRTUAL_TRACK_M` | 1.81, escenario | Virtual |
| Dimensiones interiores W/H/L | m; ancho Y, alto Z, longitud X | `CI_VIRTUAL_WIDTH_M/HEIGHT_M/LENGTH_M` | 2.210475 / 1.210475 / 3.030475 | Virtual |
| Fondo interior sobre suelo | m, +Z | `CI_VIRTUAL_BOTTOM_M` | 1.1647625 | Virtual |
| Agua configurada por volumen | %, no profundidad ni Modbus | `CI_VIRTUAL_FILL_PERCENT` | 50; permitido 20..80 | Virtual |
| Rotación IMU → vehículo | grados, matriz fija Rz(yaw) Ry(pitch) Rx(roll), mano derecha | `Montaje IMU trasero` → `CI_VIRTUAL_ROLL_DEG/PITCH_DEG/YAW_DEG`, `CI_VIRTUAL_MOUNT_CONFIRMED` | 0/0/0 **sin confirmar**; n | Automóvil |
| Corrección y vector r | mm; del origen de excitación al IMU, X delante/Y izquierda/Z arriba | Mismo menú → `CI_VIRTUAL_LEVER`, `CI_VIRTUAL_R_X_MM/Y_MM/Z_MM` | n; 0/0/0; vector sin efecto OFF | Automóvil |
| Montaje pipa real independiente | mm, centro geométrico cisterna → IMU | Mismo menú → `CI_REAL_LEVER`, `CI_REAL_R_X_MM/Y_MM/Z_MM`, `CI_REAL_*_DEG`, `CI_REAL_MOUNT_CONFIRMED` | y; +640/−420/−940; rotación pendiente | Real bloqueado |
| Amortiguamiento modal ζ | adimensional | Parámetros virtuales → `CI_VIRTUAL_ZETA` | 0.05 supuesto; explorar 0/0.10 | Virtual |
| Escala masa oscilante; amplitud máxima | adimensional; mrad | `CI_VIRTUAL_MASS_SCALE`, `CI_VIRTUAL_MAX_ANGLE_MRAD` | 1; 200; supuestos | Virtual |
| Frecuencia, longitud, masas y posiciones | rad/s, Hz, m, kg | **Derivados** por [pipa_virtual.c](../firmware/integracion/cabina/main/pipa_virtual.c) | ω=2.864785665; L=1.195321518; mp=2248.916271; mf=935.369404 | Virtual |
| Frecuencia solicitada de cada reporte BNO | Hz, reportes independientes | `preparar-captura.py --virtual --rate 100` o `50`; `Cistable physical capture` → `CI_BENCH_50HZ` en ambos nodos | 100; periodos 10000/20000 µs | Ambos |
| Objetivo ESP-NOW | envíos/s, no tasa garantizada | Mismo selector `CI_BENCH_50HZ`; slot trasero 10/20 ms | Hasta 100/50, datos nuevos/callback | Trasero |
| Cálculo | pasos/s y dt real | [control_cabina.c](../firmware/componentes/control_cabina/control_cabina.c), sin frecuencia artificial | Sólo parejas nuevas válidas | Cabina |
| Pantalla | Hz | [app_main.c](../firmware/integracion/cabina/main/app_main.c): `now-last_ui>=250000` | 4, tarea separada | Cabina |
| Resumen consola | ms / Hz | Menú raíz → `CI_DIAG_PERIOD_MS`; GPS `main/modules/gps/driver/gps_config.h` → `GPS_LOG_INTERVAL_MS` | 1000 ms / 1 Hz; captura limita a ≤1 Hz | Cabina |
| Datos pipa real incompletos | kg, m | `Parametros pipa real` → `CI_REAL_DRY_MASS_KG`, CG XYZ y vía | `nan` salvo geometría recuperada | Real bloqueado |

Masa seca operacional agregada **6897.888 kg**, incluye combustible/tripulación, excluye agua; CG calculado (0,0,1.129996660) m sobre suelo. GVWR no se usa como tara. [Tabla numérica completa, clasificación, fuentes e incertidumbre/sensibilidad](pipa-virtual.md). Estimaciones utilizables experimentalmente no son mediciones validadas.

Geometría/llenado recalculan profundidad por inversión del volumen elíptico, volumen/masa/centroide, rectángulo equivalente, frecuencia, longitud circular A=B, masas/posiciones y amortiguamiento. Se comprueban masa total y centroide. El identificador de configuración cambia y su adopción reinicia historia/dinámica y desarma; recompilar/flashear también arranca desarmado. La masa de casco introducida no se recalcula al cambiar dimensiones: revisar explícitamente esa estimación. Vacío/lleno no usan este modelo de oleaje. No copiar constantes de demo. El rectángulo equivalente aproxima la cisterna elíptica; no reproduce Zheng.

Desactivar brazo sólo omite `a_O=a_IMU−α×r−ω×(ω×r)`. Permanecen transformación de ejes, gravedad y entradas angulares. En automóvil la aceleración alineada y sin gravedad se aplica al origen virtual **por hipótesis**, no reconstruye el CG del automóvil. Los vectores se convierten de mm a m; nunca copiar automáticamente el de pipa al automóvil.

## D. Preparar, compilar y cargar manualmente

Instalar **ESP-IDF 6.1.0**, herramientas esp32s3 y Git; abrir su PowerShell exportado. En la ruta de ejemplo: `& 'C:\esp\v6.1\esp-idf\export.ps1'`; si se instaló en otra ruta, usar su export.ps1. Comprobar `idf.py --version`, `python --version` y `python -c "import serial; print(serial.__version__)"` (pyserial del entorno IDF). GCC en PATH sólo es necesario para pruebas host.

```powershell
Set-Location 'C:\esp\Proyectos'
git clone --branch Jesus/integracion-cabina --single-branch https://github.com/sushemer/Cistable.git Cistable
Set-Location 'C:\esp\Proyectos\Cistable'
python pruebas/modelo-a/preparar-captura.py --virtual --rate 100
Set-Location 'C:\esp\Proyectos\Cistable\firmware\integracion\cabina'
idf.py -B build/s3/virtual100 -D SDKCONFIG=build/s3/virtual100/sdkconfig menuconfig
idf.py -B build/s3/virtual100 -D SDKCONFIG=build/s3/virtual100/sdkconfig build
Set-Location 'C:\esp\Proyectos\Cistable\firmware\integracion\trasero'
idf.py -B build/s3/capture100 -D SDKCONFIG=build/s3/capture100/sdkconfig build
```

Si el repositorio ya existe, omitir clone y conservar cambios antes de actualizar. El generador funciona sin sdkconfig previo, usa defaults/Kconfig y conserva perfiles S3 existentes, incluidas ediciones de montaje. **No usar `--refresh-generated` después de confirmar ejes sin revisar**: respalda y regenera restableciendo montaje pendiente/llenado inicial. BNO/SH2/ETL se incluyen con licencias; no hay que descargar submódulos. El gestor IDF obtiene LVGL 9.3.0/ST7796 1.5.0 según `cabina/dependencies.lock`; primera configuración necesita registro de componentes o caché compatible. No añadir managed_components a Git.

Antes de cargar: target esp32s3, perfil/puerto correctos, GPIO sin conflictos, consola UART0, memoria esperada y montaje no confirmado en primera etapa. No usar binarios/sdkconfig de otro target/perfil. Comandos **para ejecución manual por el equipo**, no ejecutados durante esta entrega:

```powershell
Set-Location 'C:\esp\Proyectos\Cistable\firmware\integracion\cabina'
idf.py -B build/s3/virtual100 -D SDKCONFIG=build/s3/virtual100/sdkconfig -p COM5 flash
Set-Location 'C:\esp\Proyectos\Cistable\firmware\integracion\trasero'
idf.py -B build/s3/capture100 -D SDKCONFIG=build/s3/capture100/sdkconfig -p COM6 flash
```

En BOOT comprobar ESP32-S3, Flash detectada 16 MB, PSRAM detectada 8 MB e init/memtest sin error. `HW_EXPECTED` es anuncio, `HW_CONFIG` opciones compiladas, **`HW_OBSERVED` lectura de hardware**, `HW_MEMORY` memoria disponible/DMA. Sólo ese futuro arranque verifica las placas. Si memoria no coincide, detenerse y revisar inscripción/configuración; no ignorar error de PSRAM.

## E. Diagnóstico, captura y rearme

1. **Pantalla:** imagen/Modelo A, PIPA VIRTUAL / EXPERIMENTAL, agua 50 % CONFIGURADA y motivo sin resultado. SIN DATOS es esperado con montaje pendiente.
2. **CST2:** sesiones/secuencias nuevas; ambos firmwares compatibles de esta entrega. Trama de 124 bytes, reportes XYZ/gyro/cuaternión/tiempos/calidad. Legacy 17 bytes sólo diagnóstico.
3. **IMU trasero:** tasas/calidad de lineal, gyro y Game Rotation Vector independientes. Cuaternión wxyz sensor→referencia de fusión local, no norte GPS. El BNO establece orientación inicial al arrancar; comprobarla inmóvil/nivelada. El dato real se normaliza si finito/no nulo antes del cálculo; entradas inválidas se rechazan.
4. **Ejes:** verificar lineal≈0 m/s², gyro≈0 rad/s y gravedad transformada≈(0,0,−9.81) en reposo nivelado. Comprobar giros positivos por mano derecha y aceleración +X delante/+Y izquierda/+Z arriba. Configurar matriz Rz Ry Rx, sin offset de roll ni deducirla de fotos. Guardar posición/ejes/valores observados. Ajustar `CI_VIRTUAL_*_DEG`, confirmar `CI_VIRTUAL_MOUNT_CONFIRMED=y` sólo tras comprobar, recompilar y cargar cabina manualmente. IMU local sigue como diagnóstico; diferencia cabina/chasis no es movimiento cisterna/chasis.
5. **GPS:** exterior, RMC válido y GGA coherente, ≥4 satélites, HDOP≤4.0, edad≤1800 ms, separación de épocas≤200 ms y filtro `usable_for_speed`. Rearme con velocidad≤0.5 km/h. Los GPS antiguos fallidos y agua antigua simulada no son mediciones válidas.
6. **Captura:** cerrar IDF monitor/Arduino/terminal que posea el mismo COM. **Monitor y capturador no pueden abrir simultáneamente un puerto**. Dos PowerShell exportados, desde raíz, con nombres nuevos:

```powershell
Set-Location 'C:\esp\Proyectos\Cistable'
python pruebas/modelo-a/captura_completa.py record --transport uart --port COM5 --baud 921600 --seconds 180 --interactive --output capturas/ensayo01-100/cabina.raw --config firmware/integracion/cabina/build/s3/virtual100/sdkconfig
```

```powershell
Set-Location 'C:\esp\Proyectos\Cistable'
python pruebas/modelo-a/captura_completa.py record --transport uart --port COM6 --baud 921600 --seconds 180 --output capturas/ensayo01-100/trasero.raw --config firmware/integracion/trasero/build/s3/capture100/sdkconfig
```

Ajustar duración al recorrido; archivos nuevos exclusivamente, sin sobrescribir. `--config` copia sdkconfig junto al raw. Abrir antes de RESET para guardar BOOT/CONFIG/HW/WIRING. Ante desconexión/reenumeración de puerto, registrar incidencia y repetir captura íntegra. UART0 921600 8N1. USB nativo requiere generar/compilar `--console usb`, identificar puerto y usar `--transport usb`; no mezclar esos binarios con perfiles UART ni interpretar baud virtual como throughput USB.

7. **Rearme:** detenido, escenario asentado y referencia nivelada reciente (tolerancia 0.03 rad), enviar desde capturador interactivo de cabina `f` Enter, `s` Enter, esperar nuevos datos, `a` Enter. `f`: confirma sin llenado/descarga; `s`: confirma asentamiento, sólo con GPS válido detenido; `a`: supone θ=0 y θ̇=0 y arma si configuración/montaje/agua/referencia reciente lo permiten. No validan parámetros ni crean medidas. `u` invalida estado/confirmación; exige repetir procedimiento. `q` termina. No abrir otro monitor para enviar órdenes.
8. Revisar OPERATOR/COMMAND, `ARM_REJECT` si falla, `armed=1` y después **parejas nuevas válidas/pasos integrados** con dt real. Armado, parejas candidatas y pasos comprometidos son distintos. Cero desarmados por nunca haberse armado no acredita el modelo.

## F. Recorrido y fallos controlados

Guardar reposo inicial, recorrido normal y reposo final; equipos sujetos, operación por copiloto. No buscar maniobras de riesgo para cambiar semáforo. Anotar ensayo/perfil, montaje/matriz, puertos, versiones, comandos, maniobras, desarmados/incidentes. Las horas humanas dan contexto, no adquisición.

Pantalla: LTR experimental, agua configurada, semáforo |LTR| 0.60/0.75 e histéresis 0.03. Verde no garantiza seguridad. |LTR|≥1 conserva valor/límite y exige rearme; amplitud fuera de dominio o error numérico invalida predicción. SIN DATOS debe tener motivo.

Si se desarma, registrar causa/duración sin resultado; no rearmar en movimiento ni relajar límites. Detenerse en lugar adecuado, resolver GPS/montaje/reportes/configuración y repetir reposo/f/s/a. Desconexión/reinicio: ensayo separado y detenido; comprobar sesión nueva, pérdidas/frescura, invalidación y rearme. No integrar intervalos sin datos.

## G. Guardar, convertir e interpretar

```powershell
Set-Location 'C:\esp\Proyectos\Cistable'
python pruebas/modelo-a/captura_completa.py decode capturas/ensayo01-100/cabina.raw --output capturas/ensayo01-100/cabina-csv
python pruebas/modelo-a/captura_completa.py decode capturas/ensayo01-100/trasero.raw --output capturas/ensayo01-100/trasero-csv
```

Cada carpeta nueva contiene `summary.json`, `configuration.json`, `console.txt`, `measurements.csv`, `process.csv`, `records.csv`. Guardar **ambos raw originales**, `.raw.json`, copias sdkconfig de `--config`, ambas carpetas CSV y notas/fotos del montaje. Por nodo conservar app `.bin`, bootloader/partition-table `.bin`, `flasher_args.json`, lock aplicable, versión Git y SHA256 de los archivos cargados (`Get-FileHash ruta -Algorithm SHA256`). Compartir ensayo completo, no sólo Excel/pantalla/resumen. Capturas pueden incluir ubicación GPS; compartir por canal acordado, no añadir automáticamente a Git.

Tres pérdidas diferentes: **captura** (CRC32, capture_id, cola/desbordes), **paquetes ESP-NOW** (sesión/secuencia TX frente RX; callback no garantiza entrega), **reportes BNO** (secuencias independientes, duplicados/huecos/tasas). Buzón usa última muestra y expone sustituciones, no acumula antiguos. Captura guarda lo observado por firmware, no todo MEMS interno ni NMEA completo; GPS se resume a 1 Hz. Texto 1 Hz no sustituye captura.

En summary revisar tasas por reporte y RX/TX, edad parcial, separación de parejas, dt admitido, calidad/pérdidas/reinicios, CRC y `capture_dropped_max`, número/causa de desarmados, intervalos sin resultado y recuperación con operador. Exigir integridad de captura en el intervalo analizado; conservar truncados/errores y delimitar intervalo útil. Con pérdida de captura no atribuir con certeza PROCESS ausente al buzón. Timestamps µs monotónicos pertenecen a cada nodo; impresión no es adquisición ni observación de driver es instante MEMS exacto. **No restar relojes no sincronizados para inferir latencia unidireccional**.

Frecuencias distintas: BNO solicita cada reporte a 100 Hz, ESP-NOW hasta 100 envíos/s con datos nuevos/callback, modelo integra sólo parejas admitidas, pantalla 4 Hz, consola 1 Hz. Perfil 50 cambia reportes/slots en ambos nodos conservando límites de continuidad. Si 100 no es estable, conservar ensayo y repetir:

```powershell
Set-Location 'C:\esp\Proyectos\Cistable'
python pruebas/modelo-a/preparar-captura.py --virtual --rate 50
Set-Location 'C:\esp\Proyectos\Cistable\firmware\integracion\cabina'
idf.py -B build/s3/virtual50 -D SDKCONFIG=build/s3/virtual50/sdkconfig menuconfig
idf.py -B build/s3/virtual50 -D SDKCONFIG=build/s3/virtual50/sdkconfig build
Set-Location 'C:\esp\Proyectos\Cistable\firmware\integracion\trasero'
idf.py -B build/s3/capture50 -D SDKCONFIG=build/s3/capture50/sdkconfig build
```

Transferir al perfil 50 la transformación **comprobada** y confirmación de montaje; revisar flags y cargar manualmente sustituyendo los perfiles en sección D. Capturar en `capturas/ensayo02-50` con sdkconfig correspondientes. Comparar tasas, huecos, desarmados y tiempo sin resultado; no declarar capacidad verificada hasta medirla.

Ante SIN DATOS leer motivo: falta GPS → reparar recepción/fix sin fingir velocidad; montaje pendiente → comprobar matriz; fuera de dominio → resultado inválido, delimitar análisis; pérdida de continuidad → guardar causa, detenerse/rearmar. La prueba demuestra integración, fallos y viabilidad temporal. No identifica amortiguamiento real ni valida predicción de volcadura.

Para el profesor: «El automóvil excita una pipa virtual de geometría/masas documentadas. Agua al 50 % es escenario; IMU/GPS son reales. Registramos LTR experimental, entradas, calidad, pérdidas y desarmados. Comprobamos signos/reposo y observamos conducción normal. Esto prueba integración y continuidad, no certifica seguridad ni predice volcadura del automóvil».
