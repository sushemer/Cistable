> **Historial ESP32 clásico**: no usar este cableado/build para S3. [Guía vigente](../README-prueba-vehiculo.md).

> Actualizaci?n posterior: [perfil PIPA VIRTUAL con entradas reales](../pipa-virtual.md).
> El registro siguiente conserva el estado anterior del perfil real incompleto.
> El usuario confirm? r=(+0.64,-0.42,-0.94) m, centro geom?trico cisterna -> IMU,
> X adelante/Y izquierda/Z arriba. Su rotaci?n de montaje sigue pendiente.
> El autom?vil tiene configuraci?n independiente y correcci?n de posici?n OFF.

# Próxima prueba: captura completa en la aplicación

El resumen de1 Hz anterior **no era captura completa**. Se conserva para
diagnóstico; ahora hay un flujo binario independiente con CRC32 e IDs de
captura, guardado sin convertirlo a texto. Se capturan entradas de radio,
decisiones del control, estados, tiempos y descartes. No significa que se
almacene cada conversión interna MEMS o cada reporte SH2 antes de que el driver
publique su última instantánea: esa pérdida previa también debe evaluarse con
los contadores de reporte disponibles.

Procedencia vigente: IMU trasera de lado en cara lateral del bastidor;
`mounting` sigue NAN y pendiente de comprobar. No se deduce su cuaternión de
fotos ni se resta90° al roll. Agua de registros antiguos: **simulada**. Fallo
GPS anterior: **conexión de hardware**. Esos campos no son mediciones reales
válidas ni calibración. Se conserva `configured=false`, montaje/calibraciones
pendientes y todos los bloqueos actuales del modelo.

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
para cambiar únicamente baud; GPIO −1 conserva los pines ya inicializados.
No se cambian pines de sensores ni se requiere hardware adicional.

Aceptar integridad sólo en el intervalo efectivamente guardado, con CRC válido,
sin huecos de capture_id, `capture_dropped_max=0`, sin pérdida de eventos y sin
cola creciente sostenida. Una cola llena produce contadores y huecos, nunca
espera del control. Un último registro cortado al parar se informa como cola
truncada; recortar análisis al último registro/intervalo íntegro y conservar
siempre el original. No atribuir un PROCESS faltante a sustitución del buzón
si hubo pérdida de captura: se etiqueta como causa desconocida por esa pérdida.

## Preparar y compilar sin tocar configuración normal

Desde raíz, con Python:

```
python pruebas/modelo-a/preparar-captura.py --rate 100
python pruebas/modelo-a/preparar-captura.py --rate 50
```

El script clona cada `sdkconfig` existente hacia `build/capture100/sdkconfig`
o `build/capture50/sdkconfig`, activa captura, configura baud921600 y selecciona
el periodo de reportes. Mantiene target, pines y banderas del modelo. Si la
copia ya existe, se conserva salvo `--refresh-generated`, que sólo regenera
esa copia de build. No sobrescribe `sdkconfig` normal. Ejecutar los comandos
`idf.py -B ... -D SDKCONFIG=... build` que imprime, desde cada proyecto.

El valor de `CONFIG_CI_FULL_CAPTURE=y` y baud921600 deben sobrevivir a Kconfig:
la opción UART default tiene baud fijo y no basta con cambiar el número en
un archivo. El generador usa UART custom número0, con los mismos pines.
No usar un binario con captura deshabilitada como prueba de captura completa.

Los perfiles reales mantienen DEMO apagada, GPS/IMU local/botón según la
configuración conservada. Activarlos sólo tras comprobar su cableado existente,
según [prueba-mesa.md](../prueba-mesa.md). La captura binaria no completa parámetros.

## Probar y guardar, con el firmware elegido ya cargado por el operador

1. Comprobar pantalla con demo marcada según instrucciones anteriores.
2. Para datos físicos, usar imagen de captura y DEMO apagada en ambas placas.
   Iniciar dos terminales de captura, una por puerto; no abrir otro monitor
   sobre el mismo puerto. Iniciar antes del reinicio manual para conservar BOOT.
3. En terminal con Python de ESP-IDF (incluye pyserial), desde raíz:

```
python pruebas/modelo-a/captura_completa.py record --port COM_CABINA --seconds 180 --output capturas/cabina100.raw --config firmware/integracion/cabina/build/capture100/sdkconfig --config firmware/integracion/cabina/main/configuracion_modelo.c
python pruebas/modelo-a/captura_completa.py record --port COM_TRASERO --seconds 180 --output capturas/trasero100.raw --config firmware/integracion/trasero/build/capture100/sdkconfig
```

Sustituir puertos; no ejecuta flash ni envía comandos por defecto. Usa baud921600,
8N1; fija DTR/RTS bajos antes de abrir, aunque el efecto físico del puente se
debe comprobar. Guarda bytes exactos, SHA256, duración PC y copias de configuración;
en Windows solicita256 KiB de buffer RX (~5 s del presupuesto nominal), registra
si la API aceptó la petición y no presupone que el driver garantice esa capacidad.
no sobrescribe archivos existentes. `--interactive` permite escribir f/s/a/u
seguidos de Enter; q termina captura local. En esta prueba incompleta `a` no
habilita el cálculo. Sin argumentos interactivos no se manda nada al equipo.

4. Observar en orden CST2 → mediciones traseras → GPS tras corregir conexión
   → IMU cabina → desconexiones y reinicios. La aceptación física de cada sensor
   sigue en `prueba-mesa.md`. No mover el camión para este ensayo de mesa.
5. Decodificar fuera de línea en carpetas nuevas:

```
python pruebas/modelo-a/captura_completa.py decode capturas/cabina100.raw --output capturas/cabina100
python pruebas/modelo-a/captura_completa.py decode capturas/trasero100.raw --output capturas/trasero100
```

Se producen `records.csv` (payload hexadecimal exacto), `measurements.csv`,
`process.csv`, `summary.json` y `console.txt` (resumen textual separado).
Los reinicios con BOOT se dividen por segmento antes de correlacionar rx_id.
Los archivos `.raw` y sus sidecars son la evidencia primaria; CSV son derivados.

6. Comparar tasas efectivas y pérdidas. Si100 Hz produce sobrescrituras,
   rupturas o desarmados, guardar ese resultado; repetir con **ambas placas**
   en perfil50, nombres de archivos50 y mismas exigencias de continuidad.
   Comparar deltas por segundo, no totales de ensayos de duración distinta.
   No se ha probado físicamente estabilidad a100 ni a50 Hz en esta entrega.

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

## Cierre verificado e instrucciones operativas

La reanudación comprobó la rama `Jesus/integracion-cabina`, el estado de Git,
los20 hashes del manifiesto anterior y los hashes de los ejecutables host.
Coinciden con los archivos actuales. Los resultados guardados son PASS, todos
los pasos tienen salida0 y sin timeout, y las entradas de pruebas no cambiaron
después de esa ejecución. Se reutilizan esas verificaciones; no se vuelve a
compilar ni ejecutar pruebas por editar estas instrucciones. No se modificó
firmware durante el cierre.

| Placa de esta prueba | Proyecto desde raíz | Target | Perfiles disponibles |
|---|---|---|---|
| Cabina: ESP32 básico de pruebas | `firmware/integracion/cabina` | esp32, flash4 MB | `build/capture100`, `build/capture50` |
| Trasero: ESP32-S3 | `firmware/integracion/trasero` | esp32s3, flash16 MB | `build/capture100`, `build/capture50` |

El binario de cabina corresponde al ESP32 básico declarado para las pruebas;
no corresponde al ESP32-S3 previsto para la versión final de cabina.
Ambas placas deben usar el mismo perfil.100 Hz solicita reportes cada10000 µs;
50 Hz cada20000 µs. La comunicación de captura permanece a921600 baud en ambos.

### Preparación y compilación por el operador

Abrir terminal con entorno ESP-IDF6.1. Desde raíz:

```powershell
Set-Location C:\esp\Proyectos\Cistable
python pruebas/modelo-a/preparar-captura.py --rate 100
```

Los perfiles ya están generados: este comando los conserva y muestra las rutas.
Para generar el alternativo, usar `--rate 50`. No usar `--refresh-generated`
salvo que se quiera reemplazar conscientemente la copia generada del perfil.

Para cabina:

```powershell
Set-Location C:\esp\Proyectos\Cistable\firmware\integracion\cabina
idf.py -B build/capture100 -D SDKCONFIG=C:/esp/Proyectos/Cistable/firmware/integracion/cabina/build/capture100/sdkconfig build
```

Para trasero, en otra terminal:

```powershell
Set-Location C:\esp\Proyectos\Cistable\firmware\integracion\trasero
idf.py -B build/capture100 -D SDKCONFIG=C:/esp/Proyectos/Cistable/firmware/integracion/trasero/build/capture100/sdkconfig build
```

Para50 Hz sustituir **todas** las apariciones de `capture100` por `capture50`
en ambas placas. No cambiar límites de frescura, skew o continuidad.
Las dos imágenes de captura por placa y las dos configuraciones normales
tenían builds completados. Sus tamaños de app:

| Imagen | Cabina | Trasero |
|---|---|---|
| Captura100 / captura50 | 0x14fa10 bytes cada una | 0xcf160 bytes cada una |
| Normal sin captura | 0x14f000 bytes | 0xce8f0 bytes |

Los tamaños iguales entre perfiles no indican archivos idénticos: sus SHA256
son distintos y se guardan individualmente. Una recompilación posterior puede
cambiar el hash por metadatos; guardar los binarios/configuraciones realmente
usados y su nuevo hash. El cierre conserva las copias anteriores verificadas.

### Puertos, flasheo y exclusión del monitor

Identificar puertos sin abrirlos desde una terminal ESP-IDF:

```powershell
python -m serial.tools.list_ports -v
```

Anotar qué COM corresponde a cada placa, usando identificación del módulo o
conectando una por vez. `COM_CABINA` y `COM_TRASERO` son marcadores: reemplazarlos
por los puertos observados. No se ha asignado un número COM por suposición.

**Cerrar el monitor serie antes de flashear y mantenerlo cerrado durante la
captura.** En `idf.py monitor`, salir con Ctrl-]. Cerrar también monitor serie
de VS Code, Arduino, PuTTY u otro capturador que tenga ese mismo COM. Dos
capturadores en puertos distintos sí pueden ejecutarse simultáneamente.

Los siguientes comandos son para que los ejecute el operador; no se ejecutaron
en esta entrega. Desde el proyecto de cabina:

```powershell
idf.py -B build/capture100 -D SDKCONFIG=C:/esp/Proyectos/Cistable/firmware/integracion/cabina/build/capture100/sdkconfig -p COM_CABINA flash
```

Desde el proyecto de trasero:

```powershell
idf.py -B build/capture100 -D SDKCONFIG=C:/esp/Proyectos/Cistable/firmware/integracion/trasero/build/capture100/sdkconfig -p COM_TRASERO flash
```

No añadir `monitor` a esos comandos. `flash` usa los bootloader, particiones,
offsets y app del directorio elegido: cabina bootloader0x1000/app0x10000;
trasero bootloader0x0/app0x10000. No intercambiar imágenes entre targets.
El baud921600 corresponde a la captura; el baud de flasheo lo gestiona IDF.

### Captura y conversión

Al terminar el flasheo y quedar libre el COM, iniciar desde raíz una terminal
por placa. Estas órdenes no flashean ni envían confirmaciones de operador:

```powershell
python pruebas/modelo-a/captura_completa.py record --port COM_CABINA --baud 921600 --seconds 180 --output capturas/ensayo01/cabina100.raw --config firmware/integracion/cabina/build/capture100/sdkconfig --config firmware/integracion/cabina/main/configuracion_modelo.c --config pruebas/modelo-a/build/captura-final/cierre/manifiesto.json
python pruebas/modelo-a/captura_completa.py record --port COM_TRASERO --baud 921600 --seconds 180 --output capturas/ensayo01/trasero100.raw --config firmware/integracion/trasero/build/capture100/sdkconfig --config pruebas/modelo-a/build/captura-final/cierre/manifiesto.json
```

Con los capturadores abiertos, reiniciar manualmente las placas para incluir
BOOT desde el comienzo. Al finalizar, el capturador cierra el puerto. Sólo
entonces se puede volver a abrir un monitor; no superponerlo con la captura.
Guardar como `ensayo02` los cambios de cableado o perfil; los archivos no se
sobrescriben. Para50 Hz, cambiar perfiles y nombres100 por50 en ambas órdenes.

Conversión posterior, incluso con placas desconectadas:

```powershell
python pruebas/modelo-a/captura_completa.py decode capturas/ensayo01/cabina100.raw --output capturas/ensayo01/cabina100
python pruebas/modelo-a/captura_completa.py decode capturas/ensayo01/trasero100.raw --output capturas/ensayo01/trasero100
```

### Qué revisar y qué compartir

| Evidencia | Identificación |
|---|---|
| Pérdida en captura/PC o desborde del escritor | `summary.json`: `capture_id_gaps`, `capture_dropped_max`, `crc_or_header_errors`, `truncated_tail_bytes` y `warnings`; revisar por segmento si hubo reinicios |
| Buzón de control sustituye un paquete | `measurements.csv`: `not_processed_latest_slot`, sólo atribuible con captura íntegra; `unknown_due_to_capture_loss` impide afirmar esa causa |
| Reportes repetidos, huecos e inversiones | `summary.json`, por sesión/reporte: `repeats`, `gaps`, `reverse`, `observed_rate_hz` y `driver_counter_rate_hz`; contadores de radio en `last_radio_counters` |
| Muestra inválida o no utilizable | `wire_syntax_valid=false`, presencia/calidad, unidades y tiempos en `measurements.csv`; `invalid_wire` y motivo/estado en `process.csv`. Sintaxis válida no equivale a calibración, frescura ni montaje válido |
| Parejas realmente usadas | `process.csv`: `decision=2`, `dt_source_us`, separación gyro/accel y skew; `summary.json`: `committed_steps`, tasas y máximos admitidos |
| Ruptura de funcionamiento | `disarm_events`, `missing_intervals`, motivos del modelo y banderas del operador; el resumen textual conserva también `MODEL_EVENT`/`MODEL_HEALTH` |

Con configuración incompleta, cero pasos integrados y cero desarmados no
demuestran funcionamiento del modelo: puede que nunca se haya armado.
GPS/agua históricos siguen excluidos como mediciones reales. El montaje de
lado requiere verificar ejes y configurar una rotación completa, sin offset
aislado de roll. Ningún perfil de captura elimina estos bloqueos.

Compartir **la carpeta completa `capturas/ensayo01`**: ambos `.raw`, ambos
`.raw.json`, todas las copias `.sdkconfig`, `configuracion_modelo.c` y manifiesto
que el capturador añadió al nombre `.raw`, y ambas carpetas decodificadas con
`records.csv`, `measurements.csv`, `process.csv`, `summary.json`, `console.txt`.
Añadir una nota con puertos, placas/targets, perfil, duración, conexiones,
reinicios/desconexiones provocados y resultado observado. Esas notas distinguen
un corte deliberado de una pérdida espontánea; no modifican el archivo original.

La carpeta `pruebas/modelo-a/build/captura-final/cierre` conserva manifiesto,
copias de binarios de flasheo/configuraciones y evidencias de software de esta
versión. Compartirla junto con la captura, especialmente si se recompila o se
usa otra computadora. Los fallos previos permanecen en sus directorios como
FAIL; no se reinterpretan como PASS.

Quedan pendientes las pruebas físicas: capacidad real USB/UART y PC, tasas
BNO/ESP-NOW, montaje/ejes, agua real/calibración y GPS tras corregir la conexión.
La compilación y los tests host acreditan software, no esas condiciones físicas.
