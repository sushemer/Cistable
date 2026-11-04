> **Historial ESP32 clásico**: no usar este cableado/build para S3. [Guía vigente](../README-prueba-vehiculo.md).

# Entrega de mesa — diagnósticos, sin activar parámetros incompletos

Para la próxima prueba, la captura completa y los perfiles100/50 Hz se detallan
en [captura-prueba-fisica.md](../captura-prueba-fisica.md). El monitor textual y el
analizador de resumen descritos aquí conservan su utilidad de diagnóstico1 Hz;
no sustituyen la grabación binaria de entradas y decisiones de esa entrega.

Rama conservada: `Jesus/integracion-cabina`. Proyectos:
`firmware/integracion/cabina` (ESP32 básico, pantalla existente) y
`firmware/integracion/trasero` (ESP32-S3). No trasladar esos pines entre targets.
Se usan sólo los dos BNO085, GPS y sonda/RS485 existentes; no se requiere otra
compra. Parámetros/estimaciones y sensibilidad: [parametros-mesa.md](../parametros-mesa.md).
Continuidad vigente: [continuidad-control-cabina.md](../continuidad-control-cabina.md).

Esta entrega prepara firmware e instrucciones; **no se ejecutó la prueba física**.
Con configuración real actual `configured=false`, no habrá LTR ni semáforo
de riesgo calculado. Recibir sensores no elimina ese bloqueo. No enviar `a`
para intentar completarlo. La sonda temporal no integra ni arma un modelo ficticio.

## Configuración y captura

En cada proyecto, desde la terminal ESP-IDF 6.1:

```
idf.py reconfigure
idf.py menuconfig
idf.py build
```

Cabina: menú `Cistable integration (experimental)`;
`CI_DIAG_PERIOD_MS=1000` por defecto. Trasero: `Cistable rear bench diagnostics`,
`CI_REAR_DIAG_PERIOD_MS=1000`. Cero deshabilita captura; valores1..99 se acotan
a100 ms. Empezar con1000, aumentar el periodo si hay saturación; 100 ms no
garantiza ancho de banda de consola. No cambiar adquisición/TX, caducidades o
calidad para mantener cálculo. Buzones de instantánea de una posición y tareas
de impresión de baja prioridad evitan esperar por consola en adquisición;
el efecto sobre CPU/memoria/temporización aún debe medirse en equipo.

Una vez cargado por el operador el firmware elegido, abrir un monitor por
puerto existente, sin ejecutar `flash`:

```
idf.py -p COM_CABINA monitor
idf.py -p COM_TRASERO monitor
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

## Orden de prueba y aceptación

Usar el [cableado existente documentado](../revision-prueba-fisica.md), sección4;
pantalla en ESP32 clásico SCK18/MOSI19/DC21/RST22/CS4. GPS RX16/TX17,
IMU MOSI13/MISO26/SCK14/CS27/INT32/RST25 y botón23 son propuestas pendientes
de confirmar, con masa común y alimentación correspondiente al módulo existente.
No conectar los pines originales conflictivos de TACOS sobre esta pantalla.

| Orden | Configuración y observación | Aceptación del paso |
|---|---|---|
| 1. Pantalla | `CI_DEMO=y`; `CI_DEMO_CONTINUOUS=y`; periféricos opcionales apagados | Marca DEMO CONTINUA, porcentaje en reposo y semáforo sintético en movimiento, ciclos de12 s con3 s de reposo; >=75 s sin caducidad deliberada, sin reinicios/heap decreciente |
| 2. Recepción CST2 | `CI_DEMO=n`, GPS/IMU cabina/botón apagados; trasero integrado canal1 | BENCH have_frame=1, sesión/TX crecientes; V2 encode_ok=1 y resultado de encolado registrado. Sigue SIN DATOS por parámetros/GPS; LEGACY no habilita cálculo |
| 3. Mediciones traseras | Igual; observar1–3 min inmóvil y giros controlados de placa | Tiempos propios y secuencias avanzan; accel lineal cerca de cero en reposo, gyro responde en eje esperado, q normalizable/convención coherente; reportes presentes/calidad>=2 y edades/skew dentro de criterios. REAR_MB OK/CRC, raw responde a nivel conocido, errores se ven sin fabricar lectura nueva. Sin escala no aceptar metros/volumen. Registrar pérdidas/rupturas: cualquier ruptura impide declarar continuidad completa |
| 4. GPS | Confirmar cables; activar sólo `CI_ENABLE_GPS=y`, RX16/TX17 si corresponden al módulo; recompilar | GPS_DIAG con fix/RMC/GGA coherentes, <=1800 ms, skew<=200 ms, >=4sat y HDOP<=4 para usable_speed=1; perder fix debe volver a inutilizable. En interior sin fix: diagnóstico SIN FIX, no aprobar velocidad válida |
| 5. IMU cabina | Confirmar seis conexiones propuestas; `CI_ENABLE_CABIN_IMU=y`; recompilar | CABIN_IMU online y tiempos nuevos; comprobar reposo/signos por eje. No cambia el LTR ni se interpreta como movimiento del tanque |
| 6. Desconexiones/reinicios | Interrumpir por separado radio, IMU trasero, agua y GPS, luego reiniciar trasero; conservar logs | Reportes ausentes/viejos o Modbus error; no inventar cero como lectura válida; sesión trasera nueva incrementa restarts y no revive cálculo. Con modelo nunca armado: disarms=0 es esperado, sonda temporal registra rupturas. Ensayo integral futuro: cada pérdida que rompa continuidad desarma una vez, causa visible, duración sin resultado creciente y recuperación exige operador |

Anotar por fase duración, contadores iniciales/finales, causas, intervalos sin
resultado, acciones necesarias para recuperar y `event_drops`. Si fuentes reales
sobrescriben reportes a100 Hz, esta prueba debe mostrarlo; no bajar exigencias
para obtener verde. Repetir con diagnóstico menos frecuente permite evaluar
su interferencia; no demuestra por sí solo ausencia de pérdidas del BNO interno.

### Demo de caducidad separada

`CI_DEMO=y`, `CI_DEMO_EXPIRY=y`: sólo reposo inicial (~3 s), después marcha
continua. Con el hold sintético histórico de60 s termina alrededor de63 s de
tiempo **simulado** en SIN DATOS. Se registra un desarmado por volumen caducado.
La demo continua renueva agua únicamente en reposos sintéticos periódicos;
no prolonga el timeout real ni auto-rearma fallos. Ambas tienen marca visible,
vectores/parámetros sintéticos y no inicializan radio/GPS/IMU reales. Una
interrupción real de planificación/dominio puede detener también la demo;
su nombre no autoriza a ocultar esos fallos.

## Evidencia de desarrollo

Se conserva `build/continuidad-final/` y todas las capturas anteriores. Nuevo
host: suites geometría/integración/continuidad, `bench_tests.c`, analizador de
captura y escenarios geométricos. La prueba verifica caducidad con flujo
continuo, eventos/tiempos, dos demos a75 s y categoría experimental persistente.
Se conservó como FAIL la primera ejecución de bench por `tmpfile()` no
disponible en Windows (`build/mesa-fallo-tmpfile/`, exit3); el fixture usa ahora
un archivo en build, sin quitar la aserción. No fue un PASS del modelo.

Los resultados finales de host y comandos/códigos/hashes están en
`pruebas/modelo-a/build/logs/results.json`. Compilar y ejecutar host no verifica
pantalla, serial, frecuencias, orientación, escala o respuesta física del camión.
No hubo push, cambio de rama, commit ni flasheo.

Resultados de la versión final:

| Verificación | Resultado |
|---|---|
| `python pruebas/modelo-a/run-all.py` | PASS: geometría, integración, continuidad y mesa; analizador2 pruebas; escenarios6 casos. Todos los comandos exit0 |
| `python pruebas/modelo-a/compile-variants.py` | PASS: demo continua, demo caducidad, periféricos y botón; compilación de rutas condicionales, no ejecución física |
| ESP-IDF6.1 cabina ESP32, configuración real con periféricos opcionales apagados | Build exit0; binario0x14e600, app0x177000, 11% libre |
| ESP-IDF6.1 trasero ESP32-S3 | Reconfigure/build exit0; binario0xce270, app0x100000, 19% libre |
| `git diff --check` | exit0 |

Copia preservada de resultados host finales: `build/mesa-final/`. Se conservan
`mesa-previo/`, `continuidad-final/` y las demás evidencias. El primer build
trasero falló por Kconfig nuevo aún no regenerado; no fue PASS. La regeneración
y compilación posteriores sí terminaron con0. No se afirma que los builds
carezcan de avisos heredados del SDK.

Archivos de esta entrega para compartir conjuntamente:

* `firmware/componentes/control_cabina/control_cabina.c` y su header: contador,
  causas persistentes y revocación explícita de agua, sin relajar continuidad.
* `firmware/integracion/cabina/main/diagnostico.{c,h}` y
  `diagnostico_servicio.{c,h}`: diagnóstico puro comprobable y tarea de impresión.
* Cabina `app_main.c`, `CMakeLists.txt`, `Kconfig.projbuild`, `demo.{c,h}` y
  `configuracion_modelo.c` (sólo comentarios de estimaciones/requisitos).
* Trasero `main/app_main.cpp` y `main/Kconfig.projbuild`: estado Modbus y salida
  desacoplada de adquisición; CST2 no cambia.
* `pruebas/modelo-a/{bench_tests.c,analizar-mesa.py,test_analizar_mesa.py,
  parameter_scenarios.c,run-all.py,compile-variants.py}`.
* Este documento, `parametros-mesa.md` y enlace añadido en `integracion-cabina.md`.

Las pruebas `continuity_tests.c` e `integration_tests.c` anteriores y las
funciones matemáticas se conservan; no se copiaron constantes sintéticas a
la configuración del camión. No se alteraron pines, targets ni la política
real de2 s para volumen retenido.
