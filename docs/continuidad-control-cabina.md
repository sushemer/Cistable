# Revisión de continuidad — 2026-11-03

Se conservan `cc_arm()`, las pruebas previas y la captura del fallo histórico
de `integration.exe`. Esta revisión corrige rechazos durante funcionamiento,
partición/NaN, emparejamiento y requisitos indebidos para mostrar porcentaje.

## Política

`last_integrated_us` es el tiempo fuente de la última pareja confirmada; en
la primera pareja tras rearme es el anclaje del estado inicial declarado,
todavía sin integración. Sólo esa primera pareja o un paso completo exitoso
confirman alpha, tiempos y secuencias. Aceptar transporte actualiza `stream`,
pero eso **no** implica avanzar el modelo.

El paso usa copias de alpha y del siguiente estado del péndulo. Un fallo
posterior descarta esas copias. `invalidate_dynamic()` desarma, invalida alpha,
las categorías del péndulo/LTR y el vencimiento. Conserva números antiguos
sólo para diagnóstico. En movimiento entrega SIN DATOS con causa. En reposo
puede entregar un porcentaje admisible con LTR marcado incompleto/inválido.
La consola separa `dynamic_status`, `ltr_status` y estado de salida.

Recuperar un requisito no rearma. Sólo `cc_arm()` declara un nuevo estado
inicial explícito y reinicia historia/alpha/vencimiento, invalidando la
categoría del LTR anterior. La primera pareja nueva ancla el estado y prepara
alpha; la segunda integra el intervalo completo. No hay tare de sensores ni
promoción de experimental a validado.

Se admiten accel/gyro asíncronos: si sólo uno avanzó exactamente una secuencia,
se espera sin consumir ninguno. Cuando ambos avanzaron una secuencia se forma
una pareja, sujeta a calidad, tiempo, frescura y skew. `dt` usa el tiempo de
aceleración desde la pareja confirmada; alpha usa los tiempos propios de gyro.
Un paquete intermedio no acorta el intervalo que se integra.

La espera y el cálculo previo caducan en el menor de:

* frescura restante del reporte más antiguo, descontando cota de transporte;
* tiempo restante desde la aceleración confirmada hasta
  `min(alpha_max_dt_s, pendulum.max_dt_s)`, descontando edad y latencia.

Se comprueba el vencimiento aun si llega una pareja completa con `dt` fuente
pequeño: no revive continuidad caducada por reloj local. Los criterios actuales
son 100 ms de frescura y 50 ms de intervalo máximo; no se espera hasta 100 ms
para datos que ya no podrían integrarse.

La excitación se aproxima constante durante el paso y el skew está acotado
(20 ms por defecto, criterio de ensayo sin validar). No se reconstruye la
trayectoria entre muestras ni se interpolan reportes perdidos. RK4 subdivide
el paso numérico, no recupera información ausente.

## Auditoría de todas las salidas de `cc_process()`

Ruta: `firmware/componentes/control_cabina/control_cabina.c`. Pruebas nuevas
en `pruebas/modelo-a/continuity_tests.c`; suite anterior conservada en
`integration_tests.c`.

| Salida o condición | Política de continuidad | Cobertura |
|---|---|---|
| Argumentos inválidos | Desarma si existe estado; INVALID | Guardia directa sin desreferenciar nulos |
| Paquete duplicado | No consume; `cc_current()` controla vigencia | Duplicado caducado |
| Paquete inválido/invertido/sesión retirada | Desarma; INVALID | Inversión TX; codec y sesiones en suite anterior |
| Sesión nueva | Desarma, borra agua/historia | Reinicio durante marcha y suite anterior |
| Configuración/montaje/rigidez/brazo pendientes | Desarma; permite nivel independiente | Cambio armado y fixture sólo nivel |
| Agua/movimiento/no flujo/contabilidad/vía pendientes | Desarma | Agua inválida, GPS y revocación de flujo |
| Partición inválida/inconsistente | Desarma antes de consumir números | Masa/CG, estados, NaN, signos y cambio de llenado |
| Umbrales inválidos | Desarma | NaN de umbral |
| Reportes ausentes/antiguos, latencia desconocida | Desarma | Flags incompletos, edad y latencia |
| Calidad insuficiente/desconocida | Desarma | Calidad gyro insuficiente |
| Skew excesivo | Desarma | Orientación separada 30 ms |
| Transformación inválida | Desarma; no se oculta por fallo retenido | Montaje inválido y regresión previa de cuaternión |
| Límite/fallo ya retenido y desarmado | Conserva categoría tras validar entradas actuales | Suite anterior de retención/rearme |
| Desarmado y requisitos recuperados | Sigue desarmado | Restauración tras cada interrupción |
| Secuencia de reporte invertida/tiempo incoherente | Desarma; INVALID | Secuencia repetida con tiempo cambiado |
| Salto accel/gyro >1 | Cuenta pérdida/sobrescritura; desarma; STALE | Pareja perdida y dos gyro sin accel nuevo |
| Continuidad vencida | Desarma; STALE | Espera larga y pareja fuente reciente recibida tarde |
| Uno o ambos reportes sin avanzar | Espera acotada sin consumir ni renovar cálculo | Ambos órdenes asíncronos y repeticiones |
| `dt` fuera de límites | Desarma; STALE | Intervalo corto y hueco largo con secuencias adyacentes |
| Primera pareja sin alpha | Confirma sólo anclaje inicial; aún no integra | Rearme: primera MISSING, segunda calcula |
| Otro fallo de alpha/edad no representable | Desarma; descarta copia | Parámetros alpha inválidos; guardias de representación |
| Corrección de posición no EXPERIMENTAL/vector no finito | Desarma antes de usar referencia | `rear_r.x=NaN`, devuelve INVALID |
| Paso de péndulo no devuelve categoría esperada | Desarma; descarta estado siguiente | Suite previa de dominio/errores/mezcla de estados |
| Cualquiera de las dos fuerzas fijas falla | Desarma antes de sumar | Overflow de momento seco, NUMERICAL_ERROR |
| Suma de fuerza/momento no finita | Desarma; NUMERICAL_ERROR | Guardia explícita antes del LTR |
| LTR inválido/sin apoyo/error numérico | Desarma; no confirma paso | Suite previa de causas del LTR |
| LTR calculado con magnitud >=1 | Confirma paso, conserva valor como LIMITE EXP; desarma | Suite previa de transiciones en ambos signos |
| Paso completo admisible | Confirma todo una vez; SIMULATED/EXPERIMENTAL | Fixture y referencia de intervalo completo |

`cc_current()` también desarma por recepción/cálculo caducados, GPS desconocido,
revocación de no flujo o agua inválida. Se prueba esta ruta sin tramas nuevas.
Las guardias de representación y sumas extremas están auditadas; no se afirma
haber forzado todas las combinaciones extremas dentro del controlador.

## Emparejamiento y pérdidas

| Situación | Admisión y diagnóstico |
|---|---|
| Duplicado TX o mismos reportes en paquete nuevo | Espera sólo mientras vigente; no integra otra vez ni renueva vencimiento |
| Accel nuevo antes de gyro o viceversa | Espera una secuencia adyacente, dentro de límites |
| Salto TX con reportes accel/gyro adyacentes | Admite si tiempos válidos; aumenta `lost_packets`: puede perderse una transmisión que repetía reportes |
| Dos gyro antes de consumir una pareja | No admite: primer gyro sobrescrito sin integrar; cuenta y desarma |
| Pérdida de pareja completa | Cuenta huecos TX/accel/gyro detectados y desarma; no integra el hueco |
| Secuencias adyacentes con hueco largo | No admite: el contador no aporta trayectoria durante ese intervalo |
| Reinicio/inversión | No admite continuidad; exige estado inicial nuevo |

`lost_packets` acumula huecos TX aceptados, sin contar reinicios como pérdidas.
Los contadores de reportes acumulan huecos/sobrescrituras detectados al
emparejar durante el armado. No son un censo completo del BNO: antes del primer
anclaje, después del desarmado o al rechazar por otro motivo no existe historia
suficiente para tal censo. Se reinician con `cc_init()`, no con `cc_arm()`.
MASTERDATA registra los tres, `armed`, tiempo fuente confirmado y categorías.
Una pérdida de transporte admitida queda visible aun con resultado utilizable.

## Partición, incertidumbre y porcentaje

Antes de comparar se validan estados de agua/CG y parámetros, finitud, masas
(m_p>0, m_f>=0, seca>0), posiciones, semiejes, vía y límites. También se
comprueban suma y centroide resultantes. NaN no puede eludir la comparación.
Las devoluciones de `ma_correct_position()` y ambas `ma_fixed_mass_force()`
deben tener categoría esperada antes de consumir sus resultados.

Las tolerancias se conservan:

```
|m_f + m_p - m_agua| <= 1e-6 * max(1, m_agua)           [kg]
|h_O + [m_p*(z0-B)+m_f*q_f.z]/m_agua - CG| <= 1e-5     [m]
```

Son consistencia numérica para **un punto identificado**, no incertidumbre
física ni banda de llenado. Otra lectura, incluso debida a ruido, puede
invalidar esa partición; se bloquea/desarma, sin ampliar tolerancias ni
recalcular m_f automáticamente. En marcha se retiene agua del último reposo
admisible durante `water_hold_us`, con confirmación de ausencia de flujo.

Con lo disponible se implementan V(h), porcentaje, masa/CG hidrostáticos,
validación de un punto y bloqueo si deja de coincidir. Faltan identificación
por llenado de m_p, A, B, z0, q_f, amortiguamiento y dominio, además de un
tratamiento independiente y documentado de incertidumbre. No se inventan
perfiles, ni se identifica aquí un CG horizontal completo. A/B representan
trayectoria de masa equivalente, no semiejes del tanque. `demo.c` conserva
valores sintéticos; no se copian a `configuracion_modelo.c`.

`update_water()` no exige `configured`, brazo aceptado, rigidez, masa seca,
partición, vía, accel/gyro ni armado para mostrar nivel. Sí exige geometría,
calibración, altura/offset y escala positiva si raw, montaje/orientación
conocidos, calidad SH2, nivelación, GPS en reposo, líquido asentado y frescura
con cota de transporte conocida. Caducar orientación impide mostrar porcentaje
aunque el agua retenida siga vigente. Esta separación no elimina los avisos de
modelo/límite ya retenidos.

## Evidencia y alcance

Se añadió `continuity_tests.c` antes de corregir: 25 comprobaciones fallidas,
exit 1, con suites anteriores pasando. Captura preservada:
`pruebas/modelo-a/build/continuidad-fallos/`. Copia anterior del controlador:
`build/control_cabina_antes_continuidad.c`; captura previa:
`build/continuidad-captura-previa/`. Se conservan también `fallo-integration/`
y la investigación histórica, sin reconstruir ni sobrescribir esa evidencia.

Las ampliaciones reprodujeron cuatro fallos de espera/registro en
`build/continuidad-espera-fallos/` y uno de categoría de LTR al mostrar nivel en
`build/continuidad-nivel-fallo/`, todos exit 1. Una regresión temporal de
prioridad de cuaternión fue detectada por el assert anterior y corregida sin
cambiar su expectativa. Ninguna de esas ejecuciones fue PASS.

Versión final: `python pruebas/modelo-a/run-all.py` debe compilar y ejecutar
geometría, integración anterior y continuidad, todos exit 0. El runner conserva
stdout/stderr/comando/returncode/SHA256 por ejecutable en `build/logs/`.
El chequeo asíncrono compara un estado inicial no nulo con un paso directo de
intervalo completo: verifica contabilidad temporal, **no** es una referencia
analítica independiente del integrador. Permanecen las referencias analíticas
previas de equilibrio, energía, fuerzas y brazo de palanca.

Se compilan con ESP-IDF 6.1 cabina ESP32 y trasero ESP32-S3, más variantes de
app demo/periféricos/botón. Host y compilación no verifican tasas/skew del BNO,
orientación, latencia, GPS, calibración hidrostática, montaje, pantalla o
respuesta física. Sigue pendiente hardware; no hubo flasheo, push ni commit.

Resultados finales ejecutados, con aserciones habilitadas:

| Comprobación | Resultado |
|---|---|
| `python pruebas/modelo-a/run-all.py` | PASS; compilación y ejecución de las tres suites exit 0; continuidad: 0 fallos |
| `python pruebas/modelo-a/compile-variants.py` | PASS demo, periféricos y botón |
| `idf.py build` en `firmware/integracion/cabina` | exit 0; ESP32, binario 0x14c590, 11% libre en app |
| `idf.py build` en `firmware/integracion/trasero` | exit 0; ESP32-S3, binario 0xcded0, 20% libre en app |
| `git diff --check` | exit 0 |

Copia preservada de la captura host final en
`pruebas/modelo-a/build/continuidad-final/`; el `results.json` original está en
`build/logs/`. Los builds completos corresponden al modo real actual, con
GPS/IMU de cabina/botón deshabilitados por configuración. Las variantes son
comprobaciones de compilación de las rutas condicionales, no imágenes
flasheadas ni ensayos físicos de periféricos.

Archivos de esta revisión: `control_cabina.c`, su header,
`cabina/main/configuracion_modelo.c` (comentarios, sin parámetros nuevos),
`cabina/main/app_main.c` (diagnóstico), `continuity_tests.c`, `run-all.py`,
`docs/integracion-cabina.md` y este documento. Compartir junto con las versiones
existentes de `modelo_a.h`, `modelo_a.c`, `dinamica.c`, `configuracion_modelo.h`
y el resto del proyecto integrado.
