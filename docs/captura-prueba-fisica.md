# Captura vigente de prueba física: ambos ESP32-S3

Usar la [tabla, perfiles, memoria y comandos S3](hardware-esp32s3.md).
Cabina de ensayo `build/s3/virtual100`/`virtual50`, trasero
`build/s3/capture100`/`capture50`; ambos N16R8 esperados, por comprobar.
El IMU trasero excita la pipa virtual y la sonda está desconectada/ModbusOFF.
Agua50 % es CONFIGURADA, sin timestamp de adquisición ni respuesta Modbus.
No se reutilizan GPIO ni binarios del ESP32 clásico para certificar S3.

## Objetivos de ensayo, no capacidades verificadas

| Actividad | Perfil100 | Perfil50 |
|---|---|---|
| BNO: lineal m/s², gyro rad/s, Game Rotation Vector wxyz | Solicitar cada10000 µs | Solicitar cada20000 µs |
| ESP-NOW | Hasta100 envíos/s | Hasta50 envíos/s |
| Elegibilidad de envío | Cambió al menos una secuencia desde último envío aceptado, slot actual de10 ms sin intento y envío anterior terminado | Igual, slot20 ms |
| Control | Sólo parejas nuevas válidas, dt fuente real; sin repetir integración | Igual, dt nominal20 ms sin cambiar límites |
| Pantalla | 4 Hz en tarea separada, buzón de último estado | Igual |
| Resumen de consola | 1 Hz por defecto; en captura no se permite más de1 Hz | Igual |
| Captura UART | 921600 baud, 8N1, binario+resumen | Igual; menor carga de datos |

`field_espnow_try_send()` mantiene una sola transmisión pendiente; la libera
el callback de finalización. Una devolución de API correcta o callback de
broadcast correcto no demuestra entrega al receptor. No se forma una cola de
paquetes antiguos ni se compensa un retraso mediante ráfagas. Si se acumulan
reportes mientras la radio está ocupada, se toma la instantánea más nueva y
la pérdida/sobrescritura se hace visible; no se fabrican muestras intermedias.
Los slots se anclan al reloj local, evitando acumular deriva de un sleep tras
cada envío. No se recorren slots perdidos. La separación entre envíos reales
puede variar alrededor del periodo por disponibilidad de reportes y callback;
la cadencia objetivo no implica sincronía BNO ni100 muestras nuevas de cada tipo.

El control real consulta radio cada1 ms objetivo, sin bloquear en LVGL; sólo
procesa tramas recibidas nuevas y las consultas necesarias de vigencia. Su
buzón de radio sigue siendo de una posición. El buffer de **captura** conserva
historia para el PC y no alimenta después el modelo con datos antiguos.

## Capacidad calculada y límites

Formato binario: cabecera24 bytes + payload + CRC32 de4 bytes (IEEE,
polinomio0xedb88320; cubre cabecera+payload). Little endian explícito; no se
serializan estructuras C con padding. Magic `CSTCAP01`, tipo/u8, flags/u8,
payload_length/u16, capture_id/u32 y local_us/u64.

| Registro de cabina | Bytes por registro |
|---|---|
| RX: cabecera/CRC28 + rx_id8 + MAC6 + RSSI2 + CST2 original124 | 168 |
| PROCESS: cabecera/CRC28 + estado/base120 + motivo hasta96 | Hasta244 |
| Un paquete CST2 + decisión | Hasta412 |

100 ×412=41200 bytes/s. Reservando8192 bytes/s para texto1 Hz, metadatos,
estadísticas y eventos, el presupuesto es49392 bytes/s, ~54% de921600/10=
92160 bytes/s útiles en8N1. A50 Hz:28792 bytes/s, ~31%. A115200 sólo hay
11520 bytes/s: **no cabe**. Este presupuesto no autoriza texto de magnitudes
a100 Hz. En trasero, TX solicitado y finalizado suman312 bytes por paquete;
ACKs y resumen quedan dentro de la misma reserva de8 KiB/s en el ensayo de un
solo par. Tráfico ajeno o tasa superior a la prevista pueden desbordar la cola.

Cola por placa:32 elementos de288 bytes (~9216 bytes más control FreeRTOS),
sin esperas de productores. A~200 registros/s absorbe ~160 ms de pausa, no una
interrupción indefinida. El escritor no calcula el modelo ni imprime vectores
a alta frecuencia. `STATS` incluye intentados, escritos, descartados, ocupación
observada máxima y ocupación actual. Un `fwrite` correcto no acredita que el
PC recibió el contenido: también se comprueban CRC e IDs en el archivo.

La capacidad UART calculada no verifica la capacidad del puente USB/UART,
driver de PC o disco. Se deben capturar los puertos correspondientes a UART0
ya configurada. Un puerto USB nativo secundario no queda caracterizado por
la cifra de baudios UART. Los perfiles seleccionan consola UART0 configurable
con GPIO43/44 y921600 en S3. La alternativa USB nativa conserva19/20,
con baud virtual sin significado de capacidad física.
El trasero mantiene su cableado soldado; cabina usa el nuevo mapa S3
de la guía vigente. No se requiere hardware adicional.

Aceptar integridad sólo en el intervalo efectivamente guardado, con CRC válido,
sin huecos de capture_id, `capture_dropped_max=0`, sin pérdida de eventos y sin
cola creciente sostenida. Una cola llena produce contadores y huecos, nunca
espera del control. Un último registro cortado al parar se informa como cola
truncada; recortar análisis al último registro/intervalo íntegro y conservar
siempre el original. No atribuir un PROCESS faltante a sustitución del buzón
si hubo pérdida de captura: se etiqueta como causa desconocida por esa pérdida.

## Preparar, cargar ustedes y guardar

El procedimiento completo se centraliza en [hardware S3](hardware-esp32s3.md):
`preparar-captura.py --normal`, después `--virtual --rate 100` o50 para ambos
nodos; compilar con `-B build/s3/...` y su SDKCONFIG efectivo. Por defecto,
UART0 de43/44; la opción `--console usb` genera carpetas con sufijo `-usb`.
Identificar el conector real antes de elegir transporte/puerto. No modificar
buffers/particiones para ocultar pérdidas. Las carpetas clásicas son historial.

Cerrar monitor IDF/Arduino y cualquier otra aplicación que posea el mismo COM.
Abrir dos capturadores desde la raíz, con nombres nuevos en cada ensayo:

```powershell
python pruebas/modelo-a/captura_completa.py record --transport uart --port COM5 --baud 921600 --seconds 180 --interactive --output capturas/cabina-s3-100.raw --config firmware/integracion/cabina/build/s3/virtual100/sdkconfig
python pruebas/modelo-a/captura_completa.py record --transport uart --port COM6 --baud 921600 --seconds 180 --output capturas/trasero-s3-100.raw --config firmware/integracion/trasero/build/s3/capture100/sdkconfig
python pruebas/modelo-a/captura_completa.py decode capturas/cabina-s3-100.raw --output capturas/cabina-s3-100-csv
python pruebas/modelo-a/captura_completa.py decode capturas/trasero-s3-100.raw --output capturas/trasero-s3-100-csv
```

COM5/6 son ejemplos; comprobar puertos y roles. Usar Python del entorno IDF
para `record` (incluye pyserial). El decoder offline no necesita puerto.
Abrir captura antes de RESET, comprobar BOOT, CONFIG, HW y WIRING. USB nativo
puede re-enumerar al reset: no ignorar un error/desconexión del capturador.
Con firmware USB usar `--transport usb`; el baud no mide throughput USB.
Las órdenes `f`, `s`, `a`, `u` se envían desde `--interactive`, con Enter;
`q` termina. No abrir un monitor adicional para enviarlas.

## Unidades, tiempos y decisiones conservados

RX registra cada paquete observado por callback, incluso los sustituidos antes
del control: rx_id, MAC/RSSI, hora de recepción local y bytes originales CST2.
TX y TX_DONE registran el paquete original y resultado de API/callback en
reloj local trasero. Los IDs de secuencia de CST2 cuentan envíos aceptados;
las secuencias por reporte cuentan observaciones del driver, sin presumir
sincronía entre accel/gyro/orientación.

PROCESS vincula rx_id con sesión/TX, hora de evaluación, fuente previa/actual,
tiempo gyro, expiración, estado/armado/desarmados, θ/rate/LTR y alpha filtrado.
Decisión1=anclaje inicial sin integración;2=paso integrado;3=duplicado de
paquete;4=wire inválido;5=transición de consulta;0=rechazo o espera (con motivo).
Se conserva la causa original de desarmado aunque otra condición posterior
tenga prioridad en pantalla. COMMAND registra la confirmación de operador,
armado resultante y banderas: velocidad válida/bit0, asentado/bit1,
sin flujo/bit2, sintético/bit3. No son mediciones de estado físico.

CSV: accel lineal XYZ m/s², gyro XYZ rad/s, q(w,x,y,z) del Game Rotation Vector
sensor→referencia (convención aún por comprobar), agua cruda en cuentas para
enum0, profundidad m para1 y presión manométrica kPa para2. La sonda real
actual transmite enum0; unidad/escala física sigue pendiente. θ rad,
θ̇ rad/s, alpha rad/s². Estados numéricos corresponden a `ma_status` del
`modelo_a.h` que acompaña el firmware. Conservar esa versión y configuración
para reproducir un cálculo futuro.

Timestamps en µs monotónicos del nodo indicado; son observación del driver,
emisión solicitada, recepción o evaluación, **no** hora de impresión ni
adquisición MEMS exacta. El decoder calcula tasas por reloj fuente, recepción
por reloj cabina, separación accel/gyro y span de orientación por fuente,
edad parcial `(sent−sample)+(process−rx)` y dt de pasos realmente integrados.
No resta relojes de placas distintas ni presenta RTT/2 como latencia.

Resumen: reportes nuevos observados, repeticiones, huecos e inversiones;
tasa observada y tasa inferida de contador de driver separadas; recepción y
TX admitidos; parejas integradas, dt/skew admitido, causas y duración sin
resultado, desarmados y necesidad de intervención. Un rx_id sin PROCESS entre
otros procesados se marca como sustituido del buzón sólo si captura íntegra;
en la cola final es `unprocessed_tail`. La sonda temporal previa sigue disponible
cuando el modelo está bloqueado: parejas estructurales no son pasos de péndulo.

Con parámetros incompletos, `committed_steps=0` y desarmados0 pueden ser
correctos porque nunca se armó. No declarar PASS del modelo por esos ceros.
Evaluar flujo y sensores con sus registros, mantener bloqueos y documentar
que el cálculo experimental completo sigue pendiente.

## Evidencias y alcance

`configuration.json` conserva los registros CONFIG/HW/WIRING por arranque.
`HW_EXPECTED`/`HW_CONFIG` son requisitos/opciones; `HW_OBSERVED` es dato de
API en la placa cuando se ejecute. No se han obtenido aún observaciones físicas
por esta adaptación. Confirmar chip, memoria, memtest, periféricos y signos.
Guardar raw, raw.json, copias sdkconfig y CSV íntegros, notas de montaje/puertos,
maniobras/comandos, binarios y flasher_args/manifiesto del build cargado.

En `summary.json` revisar tasas efectivas, parejas y separación temporal,
pérdidas/duplicados/reinicios, CRC32 de captura, desbordes, desarmados con causa,
intervalos sin resultado y operador requerido. No declarar captura completa
cuando falten datos/metadatos o existan pérdidas sin delimitar. El resumen1 Hz
no sustituye la captura binaria ni acredita todas las muestras internas del MEMS.

La preparación y cierre anteriores se conservan en
[historial ESP32](historial/captura-prueba-fisica-esp32.md). No acreditan target,
PSRAM o cableado S3. Evidencias nuevas: `pruebas/modelo-a/build/s3-final/`.
