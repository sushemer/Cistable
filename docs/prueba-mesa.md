> Hardware vigente: **ambas placas ESP32-S3, N16R8 esperado**.
> Pinout, memorias, flags y comandos actuales: [preparación S3](hardware-esp32s3.md).
> Los resultados de ESP32 clásico se conservan como historial y no acreditan S3.

# Entrega de mesa — diagnósticos, sin activar parámetros incompletos

Para la próxima prueba, la captura completa y los perfiles100/50 Hz se detallan
en [captura-prueba-fisica.md](captura-prueba-fisica.md). El monitor textual y el
analizador de resumen descritos aquí conservan su utilidad de diagnóstico1 Hz;
no sustituyen la grabación binaria de entradas y decisiones de esa entrega.

Rama conservada: `Jesus/integracion-cabina`. Proyectos:
`firmware/integracion/cabina` y `firmware/integracion/trasero`, ambos ESP32-S3.
Usar sus mapas de placa independientes de la guía vigente.
Se usan sólo los dos BNO085, GPS y sonda/RS485 existentes; no se requiere otra
compra. Parámetros/estimaciones y sensibilidad: [parametros-mesa.md](parametros-mesa.md).
Continuidad vigente: [continuidad-control-cabina.md](continuidad-control-cabina.md).

Esta entrega prepara firmware e instrucciones; **no se ejecutó la prueba física**.
Con configuración real actual `configured=false`, no habrá LTR ni semáforo
de riesgo calculado. Recibir sensores no elimina ese bloqueo. No enviar `a`
para intentar completarlo. El diagnóstico temporal no integra ni arma un modelo ficticio. El perfil
PIPA_VIRTUAL sí dispone de estimaciones completas para cálculo experimental,
condicionado a montaje comprobado, entradas válidas y rearme explícito.

## Configuración y captura

En cada proyecto, desde la terminal ESP-IDF 6.1:

```
# Ejemplo cabina: mismo build S3 y SDKCONFIG en las tres acciones
idf.py -B build/s3/virtual100 -D SDKCONFIG=C:/esp/Proyectos/Cistable/firmware/integracion/cabina/build/s3/virtual100/sdkconfig reconfigure
idf.py -B build/s3/virtual100 -D SDKCONFIG=C:/esp/Proyectos/Cistable/firmware/integracion/cabina/build/s3/virtual100/sdkconfig menuconfig
idf.py -B build/s3/virtual100 -D SDKCONFIG=C:/esp/Proyectos/Cistable/firmware/integracion/cabina/build/s3/virtual100/sdkconfig build
```

Cabina: menú `Cistable integration (experimental)`;
`CI_DIAG_PERIOD_MS=1000` por defecto. Trasero: `Cistable rear bench diagnostics`,
`CI_REAR_DIAG_PERIOD_MS=1000`. Cero deshabilita sólo el resumen textual (no CI_FULL_CAPTURE); valores1..99 se acotan
a100 ms. Empezar con1000, aumentar el periodo si hay saturación; 100 ms no
garantiza ancho de banda de consola. No cambiar adquisición/TX, caducidades o
calidad para mantener cálculo. Buzones de instantánea de una posición y tareas
de impresión de baja prioridad evitan esperar por consola en adquisición;
el efecto sobre CPU/memoria/temporización aún debe medirse en equipo.

Una vez cargado por el operador el firmware elegido, abrir un monitor por
puerto existente, sin ejecutar `flash`:

```
idf.py -B build/s3/virtual100 -p COM_CABINA monitor
# En el proyecto trasero, con el build de ensayo correspondiente:
idf.py -B build/s3/capture100 -p COM_TRASERO monitor
```

Sustituir por puertos reales y ejecutar cada comando desde su proyecto.
En el monitor instalado, Ctrl-T seguido de Ctrl-L activa/desactiva archivo de
log; conservar el nombre que informa. Ctrl-] sale. Guardar configuración,
binarios/fecha, modelo de placa, cableado usado y ambos logs. No mezclar consolas
de dos placas en un archivo para el analizador de cabina.

Después, sin conectar ni escribir a dispositivos:

```
python pruebas/modelo-a/analizar-mesa.py cabina.txt --output resumen-mesa.json
```

El resumen conserva causas de cada `DISARM`, recuperaciones y sus duraciones,
contadores, intervalos acumulados sin resultado y advertencias por captura
incompleta. Separa reinicios de cabina detectados al reiniciarse su reloj de
evaluación; para máxima trazabilidad preferir un archivo por arranque. No
declara automáticamente PASS físico ni valida un modelo.

## Registros a observar

| Registro | Interpretación |
|---|---|
| `DIAG_PRINT` | Hora local real de impresión, hora de evaluación y edad de cola; ninguna es hora MEMS |
| `BENCH` | Sesión/TX y `source_sent_us`, recepción local, totales de paquetes/huecos/duplicados/reinicios/rechazos y wire inválido; indicador de simulación y cota de latencia |
| `REAR_REPORT` | Por accel/gyro/orientation/water: tiempo fuente de observación, secuencia, presencia, edad parcial conocida, categoría de frescura, repeticiones/huecos/inversiones |
| `REAR_VALUES` | Accel lineal XYZ en m/s², gyro XYZ en rad/s, q en orden wxyz, calidad SH2 y agua/unidad/registro; ejes todavía del sensor |
| `REAR_MB` (consola trasera) | Resultado real de última transacción: OK, EXCEPTION y código, TIMEOUT_OR_PARTIAL o error CRC/dirección/longitud/UART agrupado por driver; cuentas crudas, última lectura buena y contadores |
| `V2` (trasero) | Secuencia/sesión y tiempos de emisión, resultado de encolado, encode_ok y RTT más reciente; encolado correcto no demuestra entrega |
| `MODEL_HEALTH` | Motivo de salida y motivo dinámico separados, armado, total de desarmados y última causa, tiempos sin dinámica/pantalla, volumen retenido/edad/vigencia y banderas pendientes |
| `MODEL_EVENT` | Cada transición de armado a desarmado, id/causa/sesión/secuencia/hora local de detección. RECOVER registra intervalo sin cálculo y si hubo necesidad de intervención |
| `TIMING_PROBE` | Parejas, esperas y rupturas **estructurales de tiempos fuente**. No comprueba montaje/cota desconocidos, no calcula LTR ni reemplaza la política completa |
| `MASTERDATA`, `GPS_DIAG`, `CABIN_IMU`, `MEM` | Diagnóstico previo de continuidad, estado/frescura GPS, IMU local sólo diagnóstico, recursos |

Los timestamps BNO son observación local del driver al recibir el reporte,
no adquisición exacta del MEMS. El cuaternión del adaptador es SH2 Game Rotation Vector, wxyz,
interpretado sensor→referencia; para gravedad se usa su conjugado y luego la
rotación fija de montaje. Esta convención debe comprobarse físicamente antes
de activar cálculos; yaw no tiene referencia magnética absoluta y la norma
unitaria no comprueba ejes/signos. No se restan
cabina y chasis para inferir cisterna/chasis, ni se fusionan ambas IMU.

La edad parcial es `(sent_source−report_source)+(evaluation_local−rx_local)`.
No incluye retardo de radio ni todo el retardo interno BNO. Sin cota conocida,
`fresh_status` puede ser INCOMPLETE aun con edad parcial pequeña; los valores
siguen visibles para diagnóstico. RTT usa ida y vuelta en un solo reloj, no
es latencia unidireccional ni se configura automáticamente como RTT/2.
CST2 conserva sus124 bytes: no transmite estado detallado Modbus; cabina imprime
`NOT_TRANSMITTED_CST2_SEE_REAR_MB`. Flags y detalle trasero son cosas distintas.

`no_dynamic_total_us` incluye arranque y todo intervalo sin LTR dinámico
vigente; `no_screen_total_us` cuenta SIN DATOS, separando un porcentaje útil
de ausencia de LTR. `current_missing_us` es el intervalo en curso; RECOVER
informa su duración cerrada. Resolución: iteración de control, no exactitud
microsegundo física. `disarms` cuenta transiciones reales, no rechazos repetidos
de un modelo ya desarmado. Se conserva entre rearmes y vuelve a cero al reiniciar
cabina. El evento usa causa original incluso si pantalla muestra porcentaje.

Eventos: cola32, no bloqueo del control. Si `event_drops>0`, la captura **no es
completa** respecto de todas las causas; repetir con consola/carga corregidas.
Las instantáneas pueden sustituirse, pero estadísticas se actualizan por trama
y salud por iteración. No interpretar 1 Hz de impresión como 1 Hz de adquisición.
Con parámetros incompletos habrá0 desarmados si nunca se armó: no implica que
la continuidad ya esté probada. Evaluar rupturas de la sonda y pérdidas; el
ensayo integral del péndulo real sigue pendiente de configuración fundamentada.

## Agua: tres edades y acciones

| Magnitud | Política conservada |
|---|---|
| Comunicación | Recepción local reciente; paquetes repetidos no renuevan estado integrado |
| Lectura de sonda | Timestamp propio y flag; trasero permite500 ms para flag, pero cabina exige100 ms con cota para actualizar estimación. Son filtros distintos |
| Volumen estimado | Última estimación hidrostática admisible; `water_hold_us=2000000` en real. Tramas/lecturas nuevas en marcha no la renuevan |

Actualizar sólo con escala/altura/offset y montaje conocidos, orientación
reciente/nivelación, GPS en reposo, líquido asentado confirmado y lectura
admisible. Con oleaje no convertir fluctuación de presión en masa nueva.
Conservar el volumen anterior durante su vigencia sólo bajo hipótesis de
ausencia de flujo para dinámica. Al llenarse/descargarse: `u` antes del cambio,
invalida volumen y desarma; esperar nuevo reposo asentado y nueva estimación.
El sistema no detecta físicamente por sí solo toda carga/descarga: `f` es
declaración del operador, no un sensor de flujo.

En marcha continua, aun con radio y sonda funcionando, a los2 s desde última
estimación válida se pierde el permiso dinámico y se exige rearme. No hay
retención indefinida ni se amplía timeout en esta entrega. Volver a reposo
puede recuperar porcentaje; no recupera solo la dinámica. Si ruido/nuevo
llenado deja inconsistente la partición fija del agua, la dinámica también
se bloquea. Las tolerancias numéricas no son bandas físicas de sensor.
La prueba `bench_tests.c:water_expiry()` reproduce ese escenario con cadencia
continua y verifica causa, contador único, frescura cruda y recuperación explícita.

## Qué autorizan f, s, a y u

| Comando | Efecto exacto / límites |
|---|---|
| `f` | Declara ausencia de llenado/descarga hasta `u` o reinicio. Habilita esa condición para usar agua retenida; no calibra, no actualiza volumen ni arma |
| `s` | Sólo con GPS válido <=umbral de reposo, confirma líquido asentado. Se borra al perder GPS o superar umbral de reposo; no configura montaje, presión, masas ni dominio |
| `a` | Sólo si configured, reposo asentado, agua presente y no flujo: solicita `cc_arm()` con θ=0 rad, θ̇=0 rad/s EXPERIMENTAL. Dos parejas nuevas requeridas; el motor vuelve a validar todos los requisitos. No es tare ni validación |
| `u` | Revoca no flujo, invalida volumen y desarma con causa de operador. Usarlo antes de llenar/descargar; volver a confirmar/calcular/rearmar cuando proceda |

`OPERATOR` informa comando y banderas resultantes, incluso si `s`/`a` no
se aceptan por falta de requisitos. Ningún comando rellena NAN o promueve una
estimación a validada. En esta prueba, con configuración pendiente, `a` no
habilita cálculo real. Una configuración futura completa de estimaciones
justificadas sí puede ensayarse como experimental, sin exigir que todas sean
mediciones directas y conservando bloqueos por invalidez/unidades/montaje.

## Orden vigente de prueba y aceptación (S3)

Tabla de GPIO y comandos: [hardware S3](hardware-esp32s3.md). No utilizar la
antigua cabina ESP32 clásico ni interpretar sus builds como pruebas S3.

| Paso | Qué observar | Aceptación |
|---|---|---|
| 1. Identificación/memoria | Inscripción, PCB, puente/USB, `HW_EXPECTED`, `HW_CONFIG`, `HW_OBSERVED` | Chip S3, Flash física16 MB y PSRAM8 MB observadas, memtest sin error; no darlo por probado por la publicación |
| 2. Pantalla | GPIO nuevos LCD SPI2, no touch/SD, interfaz/datos marcados | Sin conflicto de bus/DMA/reset; demo claramente sintética si se selecciona |
| 3. Recepción CST2 | Trasero de Chris con GPIO intactos, canal1 | Sesión/secuencias y tiempos propios; LEGACY no habilita cálculo |
| 4. Mediciones traseras | Calidad/edades por reporte, gyro/lineal/q y tasa efectiva | Reportes nuevos, calidad>=2, sin asumir sincronía; documentar skew/huecos. Agua `DISABLED`, flag0, sin adquisiciones Modbus |
| 5. GPS | RX18/TX17 de cabina S3; fix/RMC/GGA | Edad<=1800 ms, skew<=200 ms, >=4 sat, HDOP<=4 para usable; sin fix no aprobar velocidad |
| 6. IMU cabina | SPI3 GPIO6/7/5/4/15/16 | Tiempos nuevos, reposo/signos; sólo diagnóstico, no excitación ni promedio |
| 7. Ejes/rearme | Rotación del IMU trasero comprobada y declarada, recompilar; `f`, `s`, `a` | Dos parejas nuevas; estado experimental, volumen configurado50 %, leverOFF |
| 8. Desconexión/reinicio | Interrumpir radio, reportes, GPS; reiniciar trasero | Invalidación y causa precisas; contar desarmados/tiempo sin resultado/rearme de operador; no recuperar estado dinámico antiguo |

Agua desconectada: no se solicita una prueba física de sonda en este ensayo.
Si100 Hz no permite continuidad, guardar resultados y repetir ambos50 Hz;
no modificar límites sólo para mantener el cálculo. Captura y monitor serie
son mutuamente excluyentes sobre un puerto. Guardar ambos raw/JSON/configs,
CSV completos, HW/WIRING, comandos/maniobras y evidencias del build cargado.

El historial íntegro de preparación anterior queda en
[mesa ESP32 clásica](historial/prueba-mesa-esp32.md), sin vigencia como pinout S3.

Nota vigente: la política de agua sensada/retención sólo se aplica al perfil real.
En el ensayo virtual50 % es condición configurada sin adquisición ni caducidad
de sonda; trasero ModbusOFF conserva agua ausente, sin inventar transacciones.
