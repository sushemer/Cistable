> Preparación vigente: [guía de prueba de vehículo](README-prueba-vehiculo.md).
> Los apartados de cableado/comandos anteriores se conservan como historial; ambos nodos actuales son ESP32-S3.

> Hardware vigente: **ambas placas ESP32-S3, N16R8 esperado**.
> Pinout, memorias, flags y comandos actuales: [preparación S3](hardware-esp32s3.md).
> Los resultados de ESP32 clásico se conservan como historial y no acreditan S3.

> Perfil nuevo y separado: [PIPA VIRTUAL con entradas reales](pipa-virtual.md).
> Mantiene las condiciones del perfil real y registra estimaciones/configuraci?n
> propias para el autom?vil. El vector real confirmado es (+0.64,-0.42,-0.94) m;
> la rotaci?n lateral debe configurarse y comprobarse.

# Integración experimental de cabina — 2026-11-03

Trabajo en `Jesus/integracion-cabina`, creada desde `Jesus/modelo-a-ltr`,
HEAD de partida `c396f4354b44a8b60ce08cbf9f9e6b311cd30bad`. El repositorio
estaba limpio. Se ejecutó `git fetch origin` correctamente después de autorizar
la escritura de FETCH_HEAD. En esa etapa inicial no se hicieron pull, merge, commits, push, PR ni flasheo.
La entrega posterior autoriza commits/push sólo a la rama personal; véase la guía vigente.

La entrega permite probar el núcleo en host y el firmware en mesa. No es una
alerta validada para circulación. La dinámica y LTR están implementados; los
parámetros reales siguen incompletos y bloquean su utilización con el camión.

## Procedencia y selección

Ramas locales inspeccionadas: `Jesus/modelo-a-ltr`, `Jesus/pantalla-espnow`,
`main`, `testing`. Referencias remotas actualizadas: `origin/Jesus/modelo-a-ltr`,
`origin/Jesus/pantalla-espnow`, `origin/Pruebas-Gael`, `origin/TACOS`,
`origin/Twig`, `origin/main`, `origin/testing` y `origin/HEAD`.

| Componente | Revisión exacta | Rutas fuente |
|---|---|---|
| Matemáticas locales | `c396f4354b44a8b60ce08cbf9f9e6b311cd30bad` | `firmware/componentes/modelo_a/`, `pruebas/modelo-a/` |
| Pantalla completa elegida por usuario | `origin/Jesus/pantalla-espnow`, `1fc348ebc3679738e4bbd301f4290b13227ef14c` | `pruebas/02-Dispositivos/Pantalla/Pantalla_Interfaz/main/{display,interfaz,estado_sistema.h}`, `pruebas/02-Dispositivos/Pantalla/Integracion/pantalla.{c,h}` |
| GPS, IMU cabina, transporte | `origin/TACOS`, `971ba82db8c6f342301b98b517dc52c17c01f32c` | `firmware/esp32-master/PRUEBA MASTER/` y `firmware/Apoyo/{field_espnow,imu_bno085}` |
| IMU trasero y cableado | `origin/Pruebas-Gael`, `9b0f0d40c4b67b82a5bc6b909ea8e916f2d30b0f` | `firmware/esp32-trasero/PRUEBA SLAVE/main/main.cpp`, `main/CMakeLists.txt`, `components/esp32_BNO08x/` |
| LTR histórico y agua | `origin/Twig`, `dcd3cde9a22e0430bacc6aeae21558c1f9d914bf` | `firmware/base/`, `sensores/agua/{RS485-Agua,agua2}/main/main.c` |

Se compararon la pantalla local y las ramas antes de importar: la copia actual
es la interfaz sencilla; la rama de pantalla incluye selección A/B, recursos,
botón y preparación visual. El usuario eligió esa variante. Se conserva
intacto el proyecto original; sólo se incorporan archivos necesarios en
`firmware/integracion/cabina/`. No se copiaron aplicaciones enteras ni se
integraron ramas. `procedencia.json` registra cada archivo importado, ruta,
referencia, commit y blob de origen. `modificaciones-importadas.json` enumera
los archivos importados que se editaron; 486 archivos incluyen la dependencia
ETL del proveedor. `importar_fuentes.py` exporta fuentes con `git show`,
rechaza destinos existentes; no debe ejecutarse sobre esta integración editada.

La dependencia TACOS `firmware/Apoyo/esp32_BNO08x` es un gitlink a
`fdb99db225fc28f17de0d430a486c182aef927a2`, no una biblioteca completa en esa
rama. Se usa expresamente la biblioteca vendorizada completa de Pruebas-Gael
indicada arriba, incluidos licencia MIT y SH2. No se afirma equivalencia de
revisión con ese gitlink. Se excluyeron documentación generada y tests del
proveedor. Sus fuentes y CMake/Kconfig se compilan con el adaptador TACOS.

Cambios respecto de fuentes: adaptador IMU con tiempos/secuencias/calidad por
reporte y cuaternión; transporte con tiempo local RX y buzón de una posición;
extern C en interfaces utilizadas desde C++; configuración de GPIO separada;
main de integración nuevo, sin el MASTERDATA defectuoso. Se corrigió el
comentario del proveedor que decía «including gravity» en el getter lineal:
el reporte habilitado es Linear Acceleration/SH2_LINEAR_ACCELERATION.

## Arquitectura ejecutable

Un IMU en cabina y un IMU trasero. El trasero está en el **chasis**; no se
presupone solidario con la cisterna. El de cabina conserva su flujo diagnóstico;
no se sustituye ni promedia con el trasero, ni se interpreta como sensor de carga.
Roles públicos `IMU_TRASERA`/`IMU_CABINA` sustituyen izquierda/derecha.

La cabina inicializa ST7796S/LVGL mediante `pantalla_init`, conserva selección
A/B y recursos. Sólo A tiene configuración matemática; B muestra SIN DATOS.
Botón opcional GPIO21: liberación estable, debounce 30 ms, corta cambia,
mantener 1 s confirma. Sin botón habilitado se selecciona A explícitamente.
Se conserva la pantalla de preparación en recursos, pero no se ejecuta tare ni
se presenta un temporizador como calibración física.

Un solo `field_espnow_init(1)` y un único callback. El callback valida tamaño
máximo, copia metadatos/datos y hora de recepción a un buzón de una posición;
no calcula, no llama LVGL y no genera logs. La tarea de aplicación procesa
fuera del callback a 100 Hz objetivo. Reemplazar el elemento viejo evita una
cola creciente. Los saltos de reportes accel/gyro desarman la dinámica;
un salto de paquete TX se contabiliza por separado y sólo se admite si no
se perdió ningún reporte necesario y se cumplen los límites temporales.
No se afirma que todas las muestras físicas se conserven bajo carga.
Pantalla a 4 Hz, logs resumidos a 1 Hz. Servicio IMU cada 2 ms, reportes
solicitados cada 10 ms, transmisión trasera cada 10 ms; medir frecuencias reales.

MASTERDATA versión 3 distingue `log_local_us`, `rx_local_us`, conteo de
paquetes consumidos y secuencia/sesión del emisor. No atribuye hora de impresión
a la muestra. Incluye pérdidas de paquetes y reportes, `armed`,
`model_source_us`, `dynamic_status` y `ltr_status` para distinguir nivel
utilizable de dinámica invalidada. Advertencias `-Wformat=2 -Werror=format` activadas en ambos main.
El timeout antiguo de enlace de 3 s no se usa como permiso para cálculo:
la política experimental por defecto exige 100 ms de edad por reporte.

## Protocolo CST2

Ambos proyectos usan `firmware/componentes/telemetria/include/telemetria.h`
y serialización explícita little endian, float IEEE754 binary32; no envían
`sizeof(struct)` ni dependen de padding. Paquete de **124 bytes**, magic CST2,
versión 2. Cambiar ambos nodos juntos para adquirir los datos completos.

| Bytes | Contenido |
|---|---|
| 0–7 | CST2, versión, reservado=0, longitud uint16=124 |
| 8–15 | sesión aleatoria uint32 por arranque, secuencia TX uint32 |
| 16–55 | sent_us, accel_us, gyro_us, orientation_us, water_us (uint64) |
| 56–67 | secuencias de reportes accel/gyro/orientation |
| 68–91 | aceleración lineal XYZ m/s², velocidad angular XYZ rad/s |
| 92–107 | cuaternión w,x,y,z, sensor→mundo del Game RV |
| 108–115 | agua float, registro uint16, magnitud 0=raw/1=m/2=kPa manométricos, flags |
| 116–123 | calidad SH2 accel/gyro/orientación, reservado=0, secuencia agua |

Flags 0/1/2/3 indican accel/gyro/orientación/agua vigentes en origen.
Calidad 0–3 es la precisión SH2; valores >3 se consideran desconocidos,
no se convierten en calidad buena. El criterio de ensayo exige mínimo 2.
Los timestamps son de observación de **cada reporte en el ESP emisor**,
no se presentan como la hora exacta del muestreo MEMS. Hace falta medir también
retardo del BNO/driver y pérdidas para acotar edad física y fidelidad de alpha.
No se comparan directamente los relojes de las dos placas.

Edad parcial = (sent_us - report_us) + (now_cabina - rx_cabina).
La corrección completa requiere además un límite de transporte explícitamente
configurado y medido; por defecto está pendiente. El receptor devuelve ACK2 de
20 bytes cada diez paquetes: sesión, secuencia y sent_us original. El trasero
calcula RTT sólo con su propio reloj y registra `last_rtt_us` (0 significa aún
sin medición). Incluye ambas radios, colas y planificación; no es una medida
de latencia unidireccional ni habilita el modelo automáticamente.

Paquetes duplicados no avanzan el estado ni renuevan la vigencia del resultado.
Saltos de reportes, tiempos invertidos, huecos largos y reinicios desarman;
requieren armado explícito. Se rechazan sesiones retiradas (hasta cuatro;
después exige reiniciar/rearmar el receptor en vez de olvidar sesiones).
La sesión aleatoria no es autenticación. Radio de mesa: canal 1, broadcast,
sin cifrado; usar un solo par en ensayo. No es configuración de despliegue.

Se conserva lectura del antiguo paquete **17 bytes** (roll/pitch en grados,
ay lineal m/s², water raw y flags bit0 IMU/bit1 agua) para log LEGACY.
Su tiempo de origen es UNKNOWN. No habilita brazo de palanca ni péndulo ni
se inventan XYZ, giroscopio o cuaternión. Con emisor antiguo, la pantalla queda
SIN DATOS. Los receptores originales de la pantalla no se enlazan al proyecto.

## Agua y Modbus

Twig RS485-Agua y agua2 consultan esclavo 1, función 03, registro 0x0004,
uint16 big endian, CRC low/high, 9600 8N1. agua2 configura half duplex y DE/RE;
no documenta la escala ni magnitud física. PRUEBA SLAVE consultaba 0x0000 y
esperaba 8 bytes; no se reutiliza ese error. El nuevo emisor conserva sus
GPIO UART1 TX17/RX13/RTS14 y usa registro 0x0004 según el código de prueba
encontrado. Sigue siendo necesario contrastarlo con la sonda instalada.

Se extrajo también el texto de las 13 páginas del informe de Twig
`sensores/agua/RS485-Agua/Prueba sensor de agua..pdf`. En página 1 el autor
reporta que 0x0004 sigue los cambios de profundidad al subir/bajar la sonda
en un depósito de aproximadamente un metro. Es evidencia experimental del
registro, no una calibración: no especifica unidad ni escala. Los registros
crudos del informe incluyen 0x0006=2000, pero no se interpreta ese número como
escala sin documentación. La extracción textual no sustituye revisión visual
de posibles figuras. Se instaló pypdf únicamente en build/pdfdeps para leerlo.

Se acumulan respuestas parciales hasta 7 bytes; CRC en 5/6 sobre los primeros
5, byte count=2, ID y función validados. Excepción 0x83 ocupa 5 bytes y se
registra su código; CRC/cabecera inválidos o timeout 120 ms invalidan el dato.
No se conserva como actual el último valor al fallar. Lectura en tarea
independiente, espera 200 ms después de cada transacción; nunca bloquea la
transmisión IMU durante el timeout.

El emisor transmite **raw**, registro y tiempo; no metros ni porcentaje.
La configuración cabina exige confirmar escala `raw_to_m`, altura respecto
al fondo y offset. No están inventados. Sólo se actualiza volumen con GPS
válido en reposo, confirmación del operador de líquido asentado, orientación
nivelada dentro de tolerancia configurable y lectura reciente. Agua en movimiento
se retiene como estimación de último reposo, con edad máxima y condición explícita
de ausencia de llenado/descarga. Comando `u` invalida volumen y desarma antes
de llenar o descargar; no se puede detectar todo cambio de masa sin más datos.
GPS inválido significa movimiento desconocido, nunca velocidad cero.

## Núcleo y ecuaciones implementadas

Geometría existente conservada y probada. El usuario confirmó ancho 2.22 y
altura 1.22 m el 2026-11-03; se registran como confirmados. Antes eran
2.20/1.24. Longitud reportada 3.04 m, espesor provisional 0.0047625 m,
base exterior provisional 1.16 m. Interior aproximado restando 2t;
sección constante, tapas planas, camión nivelado. No se descuenta rompeolas.

Con esos datos: 6368.571351257 L geométricos; escenario 1300 gal US =
4921.035319200 L, h=0.873570694 m, CG agua=1.650298478 m sobre suelo,
77.270632% por volumen. No es capacidad nominal ni medición de masa.
La discrepancia de altura central anotada 1.73 m persiste: base + H/2
ahora da 1.77 m. Batalla 3.92 frente a placa 5.30 m; separaciones 2.01/1.81
sin referencia confirmada. No se promedian para inventar vía agregada.
GVWR no es masa vacía. Estimación de cisterna 924 kg sigue sin ser masa medida.

Corrección de posición, tras alinear ejes X delante/Y izquierda/Z arriba:

```
r = referencia geométrica de cisterna → sensor de chasis
a_ref = a_sensor - alpha × r - omega × (omega × r)
```

r provisional (+0.64,-0.42,-0.94) m; Y se midió desde diferencial y su
alineación está pendiente. Se exige aceptar expresamente esa provisionalidad,
configurar cuaternión de montaje y asumir cuerpo rígido común chasis/cisterna.
No se confunde centro geométrico y CG. El contrato recibe aceleración lineal
sin gravedad; no se resta ni suma gravedad en esta corrección. Gravedad para
la dinámica se obtiene girando (0,0,-9.81) con inversa de orientación y
transformación de montaje. La normalización acotada de cuaternión corrige
sólo redondeo binary32, no montaje ni pendiente.

alpha se calcula con diferencias de giroscopio en ejes comunes y mismo reloj,
filtro de primer orden gain=dt/(tau+dt), min/max dt configurables. Primera
muestra, reinicio, dominio distinto y hueco no producen un alpha cero válido.
No se recupera silenciosamente tras pérdidas. Los parámetros de filtro son
criterios de ensayo que requieren revisar ruido y retardo en logs.

### Modelo reducido independiente

Las referencias solicitadas se buscaron por DOI/editorial. El texto completo
y sus ecuaciones no fueron accesibles: Wiley/Hindawi devolvieron errores de
acceso/timeout, MDPI 429 y la copia PDF de CiteSeer 403. Los resultados de
búsqueda verifican la familia trammel y la necesidad de identificar parámetros,
pero no permiten certificar una reproducción de sus ecuaciones.

Referencias para contrastar antes de atribuir equivalencia:
[Zheng et al. 2012](https://doi.org/10.1155/2012/162825),
[Li et al. 2013](https://doi.org/10.1155/2013/659873),
[Shin et al. 2021](https://doi.org/10.3390/electronics10212605).
No se llama modelo de Zheng/Li/Shin al código implementado ni se copian sus
coeficientes de ajuste. Se entrega esta derivación verificable por Newton:

```
q(theta) = (0, A sin(theta), z0 - B cos(theta))
t = dq/dtheta; k = d²q/dtheta²; qdot = t theta_dot
a0 = a_ref + alpha×q + omega×(omega×q) + 2 omega×qdot + k theta_dot²
theta_ddot = [t·(g_body-a0) - c theta_dot / m_p] / (t·t)
a_mass = a0 + t theta_ddot
F_liquid_on_tank = m_p (g_body - a_mass)
Mx_reference = (q × F_liquid_on_tank).x
```

Es una masa puntual en trayectoria elíptica prescrita, con amortiguamiento
generalizado c en N m s, excitación traslacional y rotacional prescrita;
no es el modelo completo acoplado de vehículo, suspensión, neumáticos y fluido.
Se integra RK4 con subpasos acotados y validación de dt, finitud y amplitud.
A/B/z0/masa oscilante/c/amplitud admisible necesitan identificación. No se
deducen automáticamente de dimensiones exteriores ni del número de rompeolas.
Sólo hay dos rompeolas con tramos reportados 1.00+1.04+1.00 m de atrás hacia
delante; no tres péndulos ni compartimentos hidráulicamente aislados.

### Fuerzas y LTR

El agua se divide explícitamente m_p + m_f = masa de agua geométrica,
y su centroide en reposo debe coincidir con el geométrico. El resto del
vehículo se representa por masa seca equivalente y CG configurados, excluyendo
agua. Fijos: F=m(g_body-a_ref-alpha×q-omega×(omega×q)). Se suman fuerzas y
momentos de cada masa **una sola vez**, incluida la masa oscilante dinámica.
No se añade un desplazamiento heurístico al momento estático del agua.

```
F_total = F_dry + F_fixed_water + F_oscillating
Mx_support = sum(Mx_reference) - reference_ground_m * F_total.y
N_total = -F_total.z
LTR_signed = 2 Mx_support / (N_total * aggregate_track_m)
```

Balance agregado cuasiestático respecto del plano de apoyo idealizado,
sin dinámica de suspensión ni distribución por ejes. LTR positivo significa
mayor reacción derecha que izquierda en la convención adoptada. Para ay positiva
hacia izquierda, fuerza inercial hacia derecha produce momento positivo.
Orientación y omega/alpha son entradas prescritas; no se resuelve roll del
vehículo ni se identifica cabina como cuerpo rígido con chasis. Su contribución
como parte de la masa seca agregada es otra aproximación del modelo reducido.
Se exige justificar vía efectiva y contabilidad de masa. Sin N positivo no
hay resultado. Se conservan valor firmado y magnitud sin recorte y una salida
limitada separada. |LTR|>=1 registra LIMITE EXPERIMENTAL, conserva el valor y
exige rearme; no constituye una predicción física posterior al límite de contacto.

## Configuración real, sintética y estados

`cabina/main/configuracion_modelo.c` es el punto explícito para los parámetros
reales; devuelve configuración pendiente. Completar `configured`, montaje,
rigidez, aceptación de r, cota de latencia, calibración de agua, masas/CG,
partición de agua, parámetros elípticos, amortiguamiento, vía y criterios.
Agua en reposo puede habilitarse sin `configured`, masas dinámicas,
`position_accepted` ni `rigid_assumed`: montaje/orientación, calibración,
geometría, GPS en reposo, líquido asentado, calidad y frescura siguen siendo
necesarios. Las banderas de contabilidad/vía siguen bloqueando LTR. Revisar esos valores con evidencias,
no copiar los números de demo. La partición del agua actualmente representa un
solo nivel identificado: si cambia masa/centroide se invalida hasta reconfigurar
o proporcionar una función de identificación por llenado.

Comandos de operador en modo real: `f` confirma que no hay llenado/descarga;
`s` confirma líquido asentado sólo con GPS válido bajo umbral de reposo;
se borra al perder GPS o moverse. `a` arma suponiendo theta=0, velocidad=0
sólo con configuración habilitada, agua presente y confirmaciones anteriores.
No pone a cero el IMU. `u` invalida agua y desarma. El montaje y modelo deben
estar completos antes de usar `a`; el motor vuelve a comprobar datos y dominio.

En `CONFIG_CI_DEMO=y` no se inicializa radio ni GPS/IMU de cabina. Se generan
vectores sintéticos con tiempos de simulación a 10 ms, datos de alta calidad
ficticia y parámetros mecánicos **sintéticos**, una fase en reposo de 3 s y
excitación sinusoidal. Marca persistente DEMOSTRACION en pantalla.
No se mezclan muestras reales/simuladas. El modo normal tiene marca
EXPERIMENTAL - NO VALIDADO y no convierte configuración pendiente en verde.

Estados disponibles: ausentes, antiguos, incompletos, inválidos, simulados,
experimentales, no disponibles y fuera de dominio. Una API puede devolver un
resultado experimental calculado; no significa validación física.
GPS desconocido o modelo inválido -> SIN DATOS. Reposo confirmado + agua
vigente -> porcentaje. Movimiento conocido + LTR utilizable -> semáforo.
Umbrales de ensayo configurables: reposo <=0.5 km/h, movimiento >=2 km/h,
histéresis entre ambos; riesgo 0.60/0.75, histéresis 0.03. No son umbrales
validados y no se anuncia «vuelco inminente».

## Hardware y prueba vigentes

Ambos nodos son ESP32-S3 con N16R8 esperado, aún por identificar físicamente.
La cabina se recablea; el IMU soldado de Chris conserva sus señales. Tabla
única, memoria/consola, perfiles y comandos en [hardware S3](hardware-esp32s3.md).
Usar `build/s3/normal`, `build/s3/virtual100` o `build/s3/virtual50` de cabina,
y `build/s3/capture100`/`capture50` de trasero para ensayo sin sensor de agua.
GPS e IMU local activos en perfiles reales/virtuales; botón opcional GPIO21
apagado por defecto. Montaje trasero sigue sin confirmar. No se ejecutaron
flasheos ni se validaron físicamente memorias/ejes. Ver [prueba de mesa](prueba-mesa.md).

## Cambios mínimos a coordinar con el compañero trasero

Adoptar el proyecto de integración trasero o trasladar: cabecera/codec CST2,
adaptador de IMU con Game RV/cuaternión + Linear Acceleration XYZ + Cal Gyro
XYZ a 100 Hz, tiempos/secuencias/calidad por reporte, sesión nueva por reinicio,
emisión 100 Hz objetivo, agua raw con registro/magnitud explícita, parser
Modbus de 7 bytes/excepción de 5 y tarea UART independiente. Mantener un IMU.
Los archivos concretos están en `firmware/integracion/trasero/`,
`firmware/integracion/components/{imu_bno085,field_espnow,esp32_BNO08x}/` y
`firmware/componentes/telemetria/`. Instalarlo junto con cabina CST2; la
compatibilidad antigua en cabina es sólo diagnóstica. ACK2 es opcional para
el cálculo, obligatorio para la prueba de RTT descrita. No se cambiaron ramas
de compañeros ni se flasheó ninguna placa.

## Historial de implementación, archivos y verificaciones anteriores

Verificación final realizada:

| Verificación | Resultado |
|---|---|
| `python pruebas/modelo-a/run-all.py` | PASS, GCC C11 con Wall/Wextra/Werror/pedantic |
| Geometría anterior | PASS: vacío/lleno, centroide, 1001 retornos, monotonía y dominios |
| Dinámica | PASS: brazo de palanca analítico, ejes/signos, alpha/intervalos, equilibrio lateral, energía, disipación y convergencia RK4 |
| Integración host | PASS: pipeline sintético completo hasta estados de pantalla; masa/centroide del agua compatibles |
| Protocolo/errores | PASS: endian/binary32, ACK2, antigüedad, NaN, duplicados, huecos, reinicios, Modbus parcial/CRC/excepción y configuración incompleta |
| Cabina normal | ESP-IDF 6.1, target esp32, build exit 0 |
| Trasero CST2 | ESP-IDF 6.1, target esp32s3, build exit 0 |
| Rutas condicionales cabina | `compile-variants.py`: PASS para app demo, app con periféricos, selección con botón, usando flags reales del compilador ESP32 |
| `git diff --check` | sin errores; sólo avisos de conversión LF/CRLF |

La comprobación de variantes compila objetos separados, no instala ni ejecuta
firmware demo. El binario normal sigue siendo el modo real con configuración
incompleta y periféricos conflictivos deshabilitados por defecto.

Cabina: `firmware/integracion/cabina/build/cistable_cabina_integrada.bin`,
1,354,688 bytes (0x14abc0), partición app 1,536,000 bytes (0x177000), 12% libre.
Flash configurada 4 MB, esquema single app large heredado. DRAM estática
data+bss 105,111 bytes; IRAM vectors+text 98,151 bytes según ELF.
Trasero: `firmware/integracion/trasero/build/cistable_trasero_v2.bin`,
843,472 bytes (0xcded0), app 1,048,576 bytes, 20% libre; flash configurada
16 MB, sin PSRAM requerida. DRAM data+bss 41,279 bytes e IRAM vectors+text
94,715 bytes (además del relleno de final de sección del enlazador).
No confundir memoria estática con heap libre de ejecución. Los logs MEM de
cabina muestran heap interno y mayor bloque DMA para medir en mesa después
de iniciar pantalla/radio/periféricos. No se ha verificado margen de RAM
dinámica, pila ni alimentación en hardware.

Se corrigieron durante compilación la dependencia ETL omitida en la primera
exportación, dependencia directa esp_driver_uart requerida por IDF6 y la
inicialización UART C++ ante un campo añadido en IDF6. El SDK emite avisos
propios de Kconfig/WiFi/gdbinit (ESP_ROM_ELF_DIR no exportado), sin impedir
generar ambos binarios. Las fuentes originales de compañeros siguen intactas.

Estado final de Git: rama `Jesus/integracion-cabina`, HEAD sin nuevos commits
`c396f4354b44a8b60ce08cbf9f9e6b311cd30bad`. Seis archivos rastreados modificados
(nota de docs, header/código/CMake de modelo, tests y runner original); nuevos
documento, dinámica, componentes control/protocolo, directorio de integración
y pruebas/runner/compilación de variantes sin stage. No se cambió la pantalla
original ni las ramas locales/remotas fuente. Builds/managed_components,
sdkconfig locales y dependencia PDF son artefactos ignorados.

* Núcleo: `firmware/componentes/modelo_a/{include/modelo_a.h,modelo_a.c,dinamica.c,CMakeLists.txt}`;
  conserva geometría, cambia roles/configuración e implementa dinámica y LTR.
* Protocolo/control: `firmware/componentes/{telemetria,control_cabina}/`.
* Proyectos, fuentes importadas y procedencia: `firmware/integracion/`.
* Pruebas: `pruebas/modelo-a/{host_tests.c,integration_tests.c,run-host.ps1,run-all.py}`.
* Documentación: este archivo y nota de actualización en `docs/modelo-a.md`.

Propuesta sin ejecutar commits:

1. `feat(modelo-a): implement elliptical dynamics, lever arm and signed force-based LTR`
2. `feat(telemetry): add versioned CST2 codec and compatible rear sender`
3. `feat(cabina): integrate existing display, GPS, IMU and guarded model pipeline`
4. `test(cistable): cover dynamics, protocol and failure states; document bench setup`

## Historial de correcciones matemáticas: causas de límite, frescura y cuaterniones

Esta sección sustituye las descripciones anteriores que agrupaban cualquier
salida fuera de dominio con la desconexión. No modifica parámetros físicos,
política de agua, cableado ni CST2. Los enums añadidos son internos; no se
envía un `ma_status` en el paquete CST2.

| Estado | Causa | Valor matemático y presentación |
|---|---|---|
| `MA_LTR_LIMIT` | Se calculó un LTR finito con N>0 y \|LTR\|>=1 | Conserva valor firmado, magnitud y versión limitada; `CC_LIMIT` -> rojo `LIMITE EXP.`. Se desarma dinámica; es un registro experimental retenido, no predicción física actual ni posterior al vuelco |
| `MA_OUT_OF_DOMAIN` | Amplitud inicial o integrada supera `max_angle_rad` | Salidas de fuerzas/estado siguiente permanecen NaN; SIN DATOS con aviso `MODELO INVALIDO: AMPLITUD` |
| `MA_NUMERICAL_ERROR` | Desbordamiento/no finitud durante integración, fuerza, momento o cociente LTR | No hay un LTR/predicción utilizable; SIN DATOS con causa numérica |
| `MA_NO_SUPPORT` | Reacción normal total calculada <=0 | No se divide por ella ni se fabrica LTR; SIN DATOS con causa `SIN APOYO` |
| `MA_INVALID` | Entrada no finita, dt/configuración numérica inválidos, transformación inválida | Error de entrada, distinto del cálculo finito que alcanza límite |
| `MA_INCOMPLETE`, `MA_MISSING`, `MA_STALE` | Configuración pendiente, datos ausentes o antiguos | Conservan sus significados; no son un límite LTR calculado |

El límite se conserva al recibir reportes válidos posteriores, sin avanzar
el péndulo ni declararlo una predicción después de pérdida de contacto. La
leyenda dice `LIMITE REGISTRADO; SIN PREDICCION ACTUAL`. En API/MASTERDATA
se mantiene el LTR firmado registrado. Si el enlace caduca o el movimiento
es desconocido, prevalece esa falta de datos; el estado del motor conserva
el registro. Los fallos matemáticos de amplitud/número/apoyo también se
conservan ante reportes posteriores válidos hasta un rearme explícito.
Datos inválidos de sensores/configuración pueden prevalecer como otro motivo
de invalidez. No hay retorno automático a verde desde un límite registrado.
`cc_arm()` establece nuevo estado inicial explícito y reinicia historia de
alpha/secuencias; hacen falta dos observaciones nuevas antes de integrar.

La función pura `ma_truck_ltr()` no tiene memoria: puede devolver experimental
para 0.999999, límite para 1 y 1.000001 y experimental si una nueva evaluación
aislada vuelve debajo de 1. El controlador añade la retención/rearme; las
pruebas cubren ambas capas y ambos signos. Un resultado límite sintético
sigue identificado por la marca DEMO y la configuración de simulación; el
estado límite nunca significa modelo validado.

`ma_freshness()` ahora trata `MA_VALID`, `MA_EXPERIMENTAL` y `MA_SIMULATED`
como estados de datos que deben superar idénticas comprobaciones: reloj
conocido e igual al del llamador, timestamp no futuro y edad <= máximo.
Una muestra reciente conserva exactamente su categoría, no se promociona
experimental a validado. Reloj desconocido/distinto -> INCOMPLETE; futuro ->
INVALID; demasiado antigua -> STALE. Se revisaron sus consumidores
`ma_estimate_water()` y `ma_imu_accel_difference()` para aceptar realmente
experimental sólo después de ese control; mezclar real y simulado sigue
rechazado y mezclar válido/experimental conserva experimental.
`ma_alpha_update()` comprueba continuidad/dt del reloj fuente, pero no tiene
un argumento de hora actual: su llamador debe verificar frescura antes.
En cabina eso lo hace `report_fresh()` con edades CST2 sin restar relojes de
placas distintas. Agua y giro reales del controlador se marcan experimental.

El adaptador BNO conserva el cuaternión recibido sin normalizar. El codec
CST2 rechaza norma cuadrada alejada de 1 más de 1e-4 cuando orientación está
presente, además de cero/NaN/infinito. La normalización para usar el dato real
ocurre en `cc_process()` después de comprobar paquete, calidad y tiempos:
`ma_quat_normalize_checked()` se aplica a la orientación (inversa para gravedad)
y al montaje. Sólo corrige redondeo con |norm²−1|<=1e-4, devuelve norma unitaria
y rechaza escala arbitraria/entradas inválidas. No corrige orientación física,
no hace tare y no habilita un montaje desconocido. `ma_rotate_vector()` y la
utilidad `ma_imu_accel_difference()` siguen exigiendo norma prácticamente
unitaria (~1e-6); quien llame directamente a la utilidad diagnóstica debe
entregar transformaciones ya normalizadas. La utilidad no se usa para fusionar
cabina/chasis ni para inferir movimiento cisterna/chasis.

Rutas y funciones responsables:

| Responsabilidad | Ruta y función |
|---|---|
| Fuerza/momento de masa oscilante | `firmware/componentes/modelo_a/dinamica.c`: `rhs()`, `ma_pendulum_step()` |
| Fuerza/momento de masa fija | misma ruta: `ma_fixed_mass_force()` |
| Sumar contribuciones secas y ambas partes de agua | `firmware/componentes/control_cabina/control_cabina.c`: `cc_process()` |
| Trasladar Mx al apoyo | misma función: `dry_moment + water_moment + out.roll_moment_nm - reference_ground_m*(dry.y+water.y+out.force_n.y)` |
| Comprobar m_f+m_p y CG estático vertical | misma ruta: `partition_status()`, valida estados/finitud/signos antes de comparar; tolerancias numéricas 1e-6 relativas en masa / 1e-5 m en CG |
| Balance LTR firmado y categoría límite | `firmware/componentes/modelo_a/dinamica.c`: `ma_truck_ltr()` |
| Estados de cálculo, retención y modos de salida | `firmware/componentes/control_cabina/control_cabina.c`: `cc_process()`, `cc_current()`, `cc_arm()` |
| Política de pantalla pura y comprobable en host | `firmware/integracion/cabina/main/presentacion.c`: `ci_display_state()`, `ci_model_notice()` |
| Llamada desde tarea | `firmware/integracion/cabina/main/app_main.c`: `display_state()` delega en política pura; `app_main()` llama pantalla fuera de callback |
| API LVGL y color/texto | `firmware/integracion/cabina/main/pantalla/pantalla.c`: `estado_valido()`, `pantalla_mostrar_estado()`; `interfaz/interfaz.c`: `interfaz_mostrar_riesgo()` |

### Parametrización por llenado, sin copiar parámetros sintéticos

Actualmente la configuración contiene **un punto identificado** del modelo,
no una tabla ni interpolación automática. Para diferentes llenados se deberá
identificar y documentar una familia en función de f=V/V_geométrico: m_p(f),
m_f(f), A(f), B(f), z0(f), q_f(f), c(f) y dominio de amplitud/frecuencia.
La geometría del tanque proporciona V(h), m_agua(f) y CG estático; no proporciona
por sí sola la fracción oscilante, trayectoria ni amortiguamiento.

Los semiejes interiores del tanque son a=W_int/2 y b=H_int/2. Los campos
`semi_y_m`/`semi_z_m` son A/B de la trayectoria de la **masa equivalente**:
no deben igualarse automáticamente a a/b. No se usan los números de `demo.c`
como parámetros del camión, ni se infiere amortiguamiento de dos rompeolas.

En cada punto identificado deben cumplirse m_f=m_agua−m_p>=0 y el CG estático
conjunto. Si se conocen los otros parámetros y m_p>0, la consistencia vertical
permite derivar:

```
z0 = B + [m_agua*(CG_agua_suelo - h_O) - m_f*q_f.z] / m_p
```

Esta relación no identifica por sí sola los demás parámetros. La posición
horizontal de la masa fija también requiere coherencia física; el control
actual comprueba el CG vertical, no una identificación horizontal completa.
Una futura tabla/ajuste deberá tener procedencia, rangos y comprobación por
punto; no extrapolar a llenados no identificados. Cambiar configuración exige
invalidar estado anterior y rearmar con estado inicial justificado, no arrastrar
sin derivación la energía/posición de un oscilador con parámetros distintos.
Hoy, si cambia llenado y las comprobaciones de masa/CG dejan de coincidir,
el cálculo se bloquea. Vacío/lleno requieren tratamiento estático apropiado:
el paso dinámico actual exige m_p>0 y no tiene una rama especial para masa
oscilante cero; no se inventa una masa pequeña para evadirlo.

Pruebas añadidas en `pruebas/modelo-a/integration_tests.c`: alrededor de ±1,
retención/rearme y dos muestras nuevas, causas de amplitud inicial/integrada,
desbordamiento numérico, apoyo nulo/negativo, dt inválido, estados experimentales
recientes/caducados/reloj desconocido/futuros (también en consumidores),
normalización acotada de orientación/montaje y rechazo de cero/escala/NaN/Inf,
rechazo de cuaterniones por codec y mapeo real a pantalla sin verde por fallos.
`run-all.py` compila también `presentacion.c`, sin LVGL, para probar esa política.

Para compartir esta corrección deben actualizarse conjuntamente:

* `firmware/componentes/modelo_a/{modelo_a.c,dinamica.c,include/modelo_a.h}`.
* `firmware/componentes/control_cabina/{control_cabina.c,include/control_cabina.h}`.
* `firmware/integracion/cabina/main/{app_main.c,CMakeLists.txt,estado_sistema.h,presentacion.c,presentacion.h}`.
* `firmware/integracion/cabina/main/pantalla/pantalla.c` y `interfaz/interfaz.c`.
* `pruebas/modelo-a/{integration_tests.c,run-all.py}`.
* Este documento, nota histórica en `docs/revision-prueba-fisica.md` y
  `firmware/integracion/modificaciones-importadas.json` (14 adaptados).

`configuracion_modelo.c` se conserva sin rellenar ni alterar; incluirlo junto
con su header y los anteriores al compartir el proyecto. No se cambiaron drivers
BNO, calibraciones, parámetros de demo, pines ni formato CST2 en esta corrección.

Verificación de esta corrección: `python pruebas/modelo-a/run-all.py` PASS;
`python pruebas/modelo-a/compile-variants.py` PASS (demo/periféricos/botón).
ESP-IDF 6.1 cabina `esp32`: build exit 0, binario 0x14b130 bytes, 12% libre
en app 0x177000. Trasero `esp32s3`: build exit 0, 0xcded0 bytes, 20% libre
en app 0x100000. `git diff --check` sin errores de whitespace. No hubo
ejecución ni verificación visual en hardware, push, commit ni flasheo.

Auditoría posterior de la ventana `integration.exe / 0x40000015`:
la ejecución abortada durante el desarrollo **fue FAIL, no PASS**. Las suites
exitosas documentadas arriba corresponden a ejecuciones posteriores corregidas.
La reconstrucción del rearme anterior falla por assert y devuelve 3; GDB
muestra `_assert -> abort`. La corrección de reinicio de historia en `cc_arm`
ya estaba en el código actual. Se reforzó la regresión y se incorporó captura
de stdout/stderr/returncode en el runner. Evidencias y límites de la
reproducción: [fallo-integration.md](fallo-integration.md).

Revisión adicional de salidas anticipadas, partición, porcentaje independiente
y reportes asíncronos: [continuidad-control-cabina.md](continuidad-control-cabina.md).
Incluye la política de invalidación/rearme, auditoría de cada salida y
capturas FAIL previas a las correcciones; las evidencias históricas se conservan.

Entrega posterior de diagnóstico y mesa: [prueba-mesa.md](prueba-mesa.md) y
[parametros-mesa.md](parametros-mesa.md) son las instrucciones vigentes para
captura, dos escenarios de demo, volumen retenido y registro de estimaciones.
No modifican las condiciones reales para mantener el cálculo activo.
