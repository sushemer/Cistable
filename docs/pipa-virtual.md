> Hardware vigente: **ambas placas ESP32-S3, N16R8 esperado**.
> Pinout, memorias, flags y comandos actuales: [preparación S3](hardware-esp32s3.md).
> Los resultados de ESP32 clásico se conservan como historial y no acreditan S3.

# Pipa virtual con entradas reales: entrega experimental

Estado: implementación experimental; consultar los resultados y hashes de esta
versión en `pruebas/modelo-a/build/virtual-final/`. No es LTR del automóvil ni
predicción validada de volcadura. El automóvil aporta aceleración y giro; la
masa, geometría, agua y apoyo pertenecen a una pipa virtual. No mezclar con el
perfil real incompleto ni con la demostración sintética.

## Selección y configuración editable

En cabina: `Cistable integration (experimental) > Perfil de calculo Cistable`:
`Pipa real`, `PIPA_VIRTUAL_ENTRADAS_REALES` o `Synthetic demonstration`.
Todos los cambios de menuconfig requieren **recompilar y flashear**. No hay
edición de parámetros físicos durante la marcha. La función pura
`ci_virtual_derive()` recalcula todas las dependencias desde sus entradas;
`cc_apply_config()` invalida el péndulo/alpha/historia y exige rearme con dos
parejas nuevas si se cambia una configuración en una futura interfaz. El ID
semántico de configuración cambia con geometría, llenado, masas o montaje.
Un reinicio tras flashear también comienza desarmado. No se copian parámetros
de `demo.c`.

Archivos centrales: `firmware/integracion/cabina/main/Kconfig.projbuild`
(interfaz), `configuracion_modelo.c/.h` (adaptación SDK y registro),
`pipa_virtual.c/.h` (entradas agrupadas, derivación independiente del SDK).
Las ecuaciones del integrador existente permanecen en
`firmware/componentes/modelo_a/dinamica.c`.

| Entrada editable / nombre | Unidad y referencia/signo | Valor inicial | Dónde cambiar |
|---|---|---|---|
| `CI_VIRTUAL_CHASSIS_MASS_KG`, `CHASSIS_CG_M` | kg; Z sobre suelo virtual | 5000; 1.0 | Parámetros físicos PIPA VIRTUAL |
| `CI_VIRTUAL_SHELL_MASS_KG` | kg cisterna vacía, incluye estimación de rompeolas | 924 | mismo menú; CG sigue centro geométrico |
| `CI_VIRTUAL_PLATFORM_MASS_KG`, `PLATFORM_CG_M` | kg; Z sobre suelo | 600; 1.3 | mismo menú |
| `CI_VIRTUAL_DIESEL_MASS_KG`, `DIESEL_CG_M` | kg; Z sobre suelo | 173.888; 0.8 | mismo menú |
| `CI_VIRTUAL_CREW_MASS_KG`, `CREW_CG_M` | kg; Z sobre suelo | 200; 1.2 | mismo menú |
| `CI_VIRTUAL_OVERRIDE_CG`, `CG_X_M`, `CG_Y_M`, `CG_Z_M` | CG seco total: X adelante, Y izquierda desde centro; Z **sobre suelo** | no; (0,0,1.13) inactivo | mismo menú; sin override, promedio por componentes |
| `CI_VIRTUAL_WIDTH_M`, `HEIGHT_M`, `LENGTH_M` | dimensiones interiores completas, m, positivas | 2.210475; 1.210475; 3.030475 | mismo menú |
| `CI_VIRTUAL_BOTTOM_M` | fondo interior sobre suelo, m | 1.1647625 | mismo menú |
| `CI_VIRTUAL_FILL_PERCENT` | % **por volumen**, configurado; no % de profundidad | 50, rango 20..80 | mismo menú |
| `CI_VIRTUAL_TRACK_M` | distancia entre apoyos laterales agregados, m | 1.81 | mismo menú |
| `CI_VIRTUAL_ZETA` | razón de amortiguamiento adimensional, 0 <= zeta < 1 | 0.05 | mismo menú |
| `CI_VIRTUAL_MASS_SCALE` | multiplicador de masa modal, positivo y partición no negativa | 1 | mismo menú, sensibilidad |
| `CI_VIRTUAL_MAX_ANGLE_MRAD` | amplitud del péndulo, mrad, respecto equilibrio | 200 | mismo menú, 50..300; nunca ángulo del automóvil |
| `CI_VIRTUAL_LATENCY_MS` | presupuesto supuesto de observación/transporte, ms | 20 | mismo menú, 1..80; no latencia medida |
| `CI_VIRTUAL_LEVER` | habilitar traslado entre puntos del automóvil | desactivado | **Montaje IMU trasero** |
| `CI_VIRTUAL_R_X/Y/Z_MM` | mm, origen elegido del automóvil -> IMU; X adelante,Y izquierda,Z arriba | (0,0,0), no utilizado | mismo menú; conversión interna /1000 a m |
| `CI_VIRTUAL_ROLL/PITCH/YAW_DEG` | grados; rotación fija sensor -> vehículo, Rz(yaw) Ry(pitch) Rx(roll) | (0,0,0), **pendiente de comprobar** | mismo menú; no son offsets restados a Euler |
| `CI_VIRTUAL_MOUNT_CONFIRMED` | confirma comprobación física de ejes | no | mismo menú; bloquea dinámica hasta configurar |
| `CI_VIRTUAL_RIGID` | supuesto de cuerpo común para traslado del automóvil | no | mismo menú; necesario sólo si se activa lever |
| `CI_REAL_LEVER`, `CI_REAL_R_X/Y/Z_MM` | mapeo independiente de la pipa real | sí; (+640,-420,-940) mm | Montaje IMU trasero al seleccionar real |
| `CI_REAL_ROLL/PITCH/YAW_DEG`, `MOUNT_CONFIRMED`, `RIGID` | convención anterior, montaje lateral del bastidor | identidad pendiente; no; no | mismo menú en real |
| `CI_REAL_WIDTH/HEIGHT/LENGTH/BOTTOM_M` | m, interior / fondo sobre suelo | geometría anterior | Parámetros pipa real |
| `CI_REAL_DRY_MASS_KG`, `CG_X/Y/Z_M`, `TRACK_M` | kg/m; CG Z sobre suelo | nan, pendientes | mismo menú; no completan por sí solos el modelo real |

Los XYZ de cada componente se agrupan en `ci_virtual_parameters.dry[]`;
X=Y=0 son supuestos de simetría, editables en `pipa_virtual.c` (recompilar).
El override XYZ del CG total está disponible directamente en menuconfig.
Densidad=1000 kg/m³ y g=9.81 m/s² son constantes de trabajo explícitas en
`ci_virtual_defaults()`; cambiarlas exige recompilar. El perfil real conserva
agua sin calibrar, partición y dinámica pendientes: **no se habilita con estas
estimaciones virtuales**. Desactivar lever real bloquea su LTR.

## Procedencia y tabla numérica nominal, 50 % por volumen

Fundamento del supuesto de chasis: como referencia de orden de magnitud,
el cat?logo primario [Hino 500, 1627 FC LWB, PDF p?gina 3 (?ndice cero)](https://hino-trucks.co.za/wp-content/uploads/2025/03/hino-500-specs.pdf)
distingue tara de chasis 5183 kg y GVM16000 kg. Se adopta **5000 kg redondeados
como escenario an?logo**, no se identifica nuestra pipa como Hino ni se traslada
su tara exacta. El estado de combustible de esa tara no se usa para hacer una
resta ficticia: nuestro presupuesto de5000 excluye expresamente diesel,
ocupantes y carrocer?a a?adida. Esa exclusi?n es supuesto del escenario, no
reconstrucci?n del veh?culo del cat?logo. Explorar4000..6000 kg no es intervalo
estad?stico. Altura de CG1.0 m: supuesto bajo el fondo de cisterna1.165 m,
compatible con concentraci?n de motor/chasis, pendiente de contraste.
Plataforma/bomba600 kg es presupuesto de escenario (350 kg estructura y250 kg
bomba/accesorios), no lista medida de piezas; CG1.3 m supone instalaci?n cerca
del fondo. Personas/equipo200 kg representa2x80 kg +40 kg de equipo; CG1.2 m
es supuesto. Diesel se coloca a0.8 m como escenario bajo la cisterna. Todas
estas ubicaciones son supuestos expl?citos, no mediciones recuperadas.

Dimensiones exteriores confirmadas por el usuario: 2.22 x 1.22 m; largo
reportado 3.04 m. Espesor reportado 0.0047625 m y base 1.16 m. Restamos 2t en
las tres dimensiones y sumamos t al fondo. Extremos planos y desplazamiento
volumétrico de rompeolas omitido. Tolerancias/repetibilidad desconocidas.
No se utiliza GVWR=15876 kg como tara. El antiguo CG anotado 1.73 m discrepa
del centro geométrico 1.77 m: se conserva base+geometría como escenario nominal.

| Parámetro | Valor | Unidad | Procedencia / clasificación | Incertidumbre o exploración |
|---|---:|---|---|---|
| Chasis y cabina, excluye todos los renglones siguientes | 5000 | kg | supuesto explícito de vehículo virtual medio; **no tara recuperada del camión** | explorar 4000..6000; no intervalo de confianza |
| CG chasis/cabina | (0,0,1.0) | m sobre suelo | supuesto de escenario | altura no identificada |
| Cisterna vacía / CG | 924 / (0,0,1.77) | kg / m | estimación recuperada, acero 7850 kg/m³, pared uniforme, extremos y dos rompeolas al 80 %; CG geométrico supuesto | orificios/accesorios no identificados |
| Plataforma, bomba y accesorios / CG | 600 / (0,0,1.3) | kg / m | supuesto independiente; excluye cisterna, combustible y agua | no medido |
| Diesel / CG | 173.888 / (0,0,0.8) | kg / m | 209 L x 0.832 kg/L recuperados; ubicación supuesta | cantidad real desconocida |
| Personas/equipo / CG | 200 / (0,0,1.2) | kg / m | supuesto de escenario | no medido |
| Masa seca total | 6897.888 | kg | suma calculada; incluye los cinco renglones una vez | no equivale a tara pesada |
| CG seco total | (0,0,1.129996660) | m sobre suelo | promedio ponderado calculado | explorar Z +/-0.20 m |
| Vía agregada | 1.81 | m | hipótesis de apoyo basada en separación trasera anotada; referencia de rueda por verificar | alternativa anotada 2.01 m; LTR a momento fijo se multiplica por 1.81/2.01 |
| Origen O, centro geométrico | 1.770000 | m sobre suelo | calculado | discrepancia histórica 0.04 m |
| Capacidad geométrica interior | 6.368571351 | m³ | calculada, elipse extruida | no capacidad calibrada |
| Llenado | 50 | % volumen | **CONFIGURADO/SIMULADO**, arbitrario inicial | cambiar 20..80 recalcula todas las dependencias |
| Profundidad / volumen | 0.605237500 / 3.184285676 | m / m³ | inversión de volumen elíptico / calculado | no Modbus |
| Masa / CG del agua | 3184.285676 / 1.513129227 | kg / m sobre suelo | rho=1000; hidrostática calculada | no medición del líquido |
| Ancho superficie libre B / profundidad rectangular equivalente h_e | 2.210475 / 0.475352421 | m | equivalencia propuesta abajo | no solución espectral elíptica |
| Primer modo omega / f | 2.864785665 / 0.455944799 | rad/s / Hz | fórmula rectangular, calculado | aproximación modal |
| Longitud circular L=A=B_trayectoria | 1.195321518 | m | g/omega² | **no semiejes de la cisterna** |
| Masa oscilante / fija | 2248.916271 / 935.369404 | kg | primer modo / complemento | explorar masa modal x0.8..1.2, conservando masa/CG |
| Z oscilante en reposo / fija | 1.292744086 / 2.043003016 | m sobre suelo | equivalentes calculados, conserva CG estático | puntos equivalentes, no ubicaciones de porciones físicas de agua |
| Z pivote | 2.488065604 | m sobre suelo | Z oscilante + L | puede estar fuera del tanque; no soporte físico |
| zeta / c generalizado | 0.05 / 920.523536 | 1 / N m s | supuesto / 2 zeta m_p L² omega | explorar zeta=0,0.05,0.10; no atribuido a rompeolas |
| Dominio angular | +/-0.20 | rad | supuesto de pequeña amplitud (11.46 grados) | excederlo invalida el modelo, no se fuerza resultado |
| Frescura / skew / dt máximo | 100 / 20 / 50 | ms | política previa conservada | son criterios software, no capacidades físicas verificadas |
| Presupuesto observación/transporte | 20 | ms | supuesto experimental explícito | verificar con captura; no resta de relojes no sincronizados ni RTT/2 |

Aqu? ?masa seca? es el conjunto ajeno al agua transportada e incluye el diesel
y las personas/equipo explicitados; no es el peso seco de f?brica ni una tara
pesada.

CG seco interno relativo al origen O: Z=-0.640003340 m. Agua fija:
Z=+0.273003016 m. Pivote relativo: Z=+0.718065604 m. Las fuerzas de cada masa
se contabilizan una vez; no se añade de nuevo la masa total de agua.

## Ecuaciones y fuente primaria

Fuente consultada: Franklin T. Dodge (SwRI), *The New Dynamic Behavior of
Liquids in Moving Containers* (2000), tabla 3.1, página impresa 47 / PDF 52
(índice cero), capítulo 3; solución de pequeña amplitud del capítulo 1.
[Documento del autor, copia accesible](https://www.vibrationdata.com/tutorials/SwRI_SLOSH_Update.pdf).
El servidor original de SwRI no respondió durante la consulta. No se afirma
reproducir Zheng, Li o Shin. La tabla es para recipiente rectangular rígido,
líquido homogéneo e incompresible, superficie libre, pequeñas oscilaciones,
sin rompeolas; no es fórmula exacta para nuestra sección elíptica.

Construcción propia de la equivalencia: sea a=W_i/2, b=H_i/2, u=(h-b)/b.
El volumen elíptico V(h) se obtiene integrando franjas
`2 a sqrt(1-z²/b²)`; se invierte por bisección para V=f V_total. El centroide
estático se integra con la misma sección elíptica. Luego:

```
B = 2 a sqrt(1-u²)                       [m, ancho de superficie libre]
h_e = V / (L_i B)                        [m, profundidad del rectángulo]
k = pi / B                              [1/m]
omega² = g k tanh(k h_e)                  [1/s²]
m_p/M = 8 B tanh(k h_e) / (pi³ h_e)      [adimensional, primer modo]
L = g / omega²                          [m]
m_f = M - m_p                           [kg]
z_p_reposo = CG_agua + h_e/2 - 2 tanh(k h_e/2)/k
z_pivote = z_p_reposo + L
z_f = (M CG_agua - m_p z_p_reposo)/m_f
c = 2 zeta m_p L² omega                  [N m s]
```

La expresión de reposo procede de restar la longitud al pivote de tabla 3.1,
y trasladar verticalmente el CG del rectángulo al CG elíptico real calculado.
Se conserva volumen, masa, longitud longitudinal, ancho y área de superficie
libre, y CG estático. Se aproxima forma de paredes, profundidad efectiva,
espectro, fuerzas y momentos dinámicos. Se absorben modos superiores en la
masa fija; se omite su inercia rotacional distribuida residual I0. No se
modelan mamparos, oleaje violento, impacto, suspensión, neumáticos o reparto
por ejes. El modelo agregado de apoyo no resuelve cada rueda. Por ello tampoco
se afirma exactitud del momento ante rotaciones del vehículo.

El integrador existente usa `r=(0,L sin(theta),z_pivote-L cos(theta))`:

```
t = dr/dtheta; q = d²r/dtheta²
b = a_O + alpha x r + omega_body x (omega_body x r)
    + 2 omega_body x (t theta_dot) + q theta_dot²
theta_ddot = [t . (g_body-b) - c theta_dot/m_p] / (t . t)
F_p = m_p [g_body - b - t theta_ddot]
F_f/seca = m [g_body-a_O-alpha x r-omega_body x(omega_body x r)]
M_O = suma (r x F)_X
M_apoyo = M_O - h_O suma F_Y
N = -suma F_Z; LTR = 2 M_apoyo/(N via)
```

Aceleración **lineal** SH2 en m/s² ya sin gravedad; no se resta g nuevamente.
La gravedad se reconstruye desde el cuaternión conjugado sensor->inercial y
se rota al marco del vehículo. Gyro rad/s y alpha rad/s² permanecen activos
aunque lever esté apagado. El traslado opcional es
`a_O=a_IMU-alpha x r-omega x(omega x r)`. El bypass virtual aplica la
aceleración alineada directamente a O, **hipótesis de excitación**, sin afirmar
que es la aceleración reconstruida del CG del automóvil.

Validaciones: finitud, signos, norma de cuaternión, masa positiva, masa fija
positiva, suma de masas y centroide; omega/L positivas, unidades SI. Se rechazan
configuraciones inválidas, no se amplían tolerancias para aceptarlas. El dominio
dinámico se limita a 20..80 % de volumen; 0/100 % sólo tienen hidrostática y
no péndulo aplicable. La API los distingue, menuconfig impide seleccionarlos
para esta prueba dinámica. La escala de masa recalcula masa fija, posición y
amortiguamiento; no es identificación física.

## Montaje, orientación inicial y operación

Pipa real: r=(+0.64,-0.42,-0.94) m desde **centro geométrico de cisterna a
IMU**, confirmado por el usuario; diferencial centrado lateralmente. Su
rotación lateral sigue pendiente. Automóvil: r=(0,0,0) independiente, lever OFF.
Si se habilita en automóvil, se debe medir/definir su propio origen/r y aceptar
cuerpo rígido. Nunca copiar el vector de la pipa.

La orientación inicial viene del Game Rotation Vector del BNO al arrancar;
es referencia de fusión local, no rumbo norte GPS. No se calcula una rotación
de montaje a partir de una foto. Seleccionar Rz Ry Rx fija correspondiente al
montaje físico: transformar los ejes de placa, aceleración, gyro y gravedad.
Con vehículo inmóvil y placa nivelada, verificar g_body aproximadamente
(0,0,-9.81), aceleración lineal cercana a cero y gyro cercano a cero. Girar
controladamente cada eje positivo según mano derecha y comprobar gyro; acelerar
hacia delante debe producir +X, hacia izquierda +Y, arriba +Z. Una inclinación
estática permite comprobar el signo de gravedad. Verificar calidad y tiempos;
no activar `MOUNT_CONFIRMED` hasta haber comprobado la matriz. Guardar notas
sobre posición de placa/ejes y valores observados. No restar 90 grados a roll.
El IMU de cabina sigue siendo diagnóstico, no se promedia ni se interpreta
cabina-chasis como movimiento cisterna-chasis.

Rearme: con GPS válido indicando <=0.5 km/h, detenido/asentado, ejes comprobados
y orientación reciente nivelada (tolerancia 0.03 rad), enviar `f`, `s`, `a`.
`f`: confirmar escenario sin cambio de volumen; no crea una lectura de agua.
`s`: confirmar asentamiento en reposo; GPS solo no lo demuestra. `a`: declarar
estado inicial supuesto theta=0, velocidad=0; no identifica el oleaje inicial.
Se requieren dos parejas nuevas posteriores. Ninguna confirmación convierte
estimaciones en validadas. `u`: invalida estado/confirmación; volver a reposo y
rearmar. En virtual la definición del volumen persiste, porque es configuración;
para cambiarla editar porcentaje y reconstruir/flashear. En real se conserva
la caducidad de agua retenida de dos segundos, no alterada por la demo/virtual.

Si hay hueco, pérdida de reporte, reinicio, dato inválido, GPS caducado o tiempo
fuera de dominio, se desarma; no reanuda estado antiguo ni inventa intervalos.
Para recuperar, detenerse y repetir comprobación de reposo y rearme. Muestra
el motivo exacto y conserva contador/causa de desarmados y tiempo sin resultado.
No relajar continuidad para que parezca funcionar a 100 Hz: si las parejas
reales no lo permiten, registrar el resultado y repetir con ambos ESP a 50 Hz.

## Preparación física y captura vigentes

Ambos ESP son **ESP32-S3 N16R8 esperado**. Usar exclusivamente la tabla y
los comandos de [hardware S3](hardware-esp32s3.md); la cabina DevKit V1
anterior dejó de ser preparación vigente. Directorios cabina:
`build/s3/virtual100`/`virtual50`; trasero: `build/s3/capture100`/`capture50`.
El preparador `--virtual --rate 100` o50 prepara ambos nodos, con GPS e IMU
local habilitados, `MOUNT_CONFIRMED=n`, leverOFF, escenario50 % por volumen,
Modbus trasero deshabilitado y consola UART0 de43/44 a921600. Ver alternativa
USB nativo en la misma guía. No abrir monitor y capturador sobre el mismo puerto.

La pantalla conserva PIPA VIRTUAL/EXPERIMENTAL, LTR numérico cuando sea válido,
agua CONFIGURADA y motivo de falta de resultado. Umbrales experimentales:
magnitud LTR0.60/0.75, histéresis0.03; verde no garantiza seguridad. Límite>=1
se retiene para diagnóstico con rearme, fuera del dominio no se predice.
Captura conserva CONFIG/HW/WIRING, entradas CST2 y decisiones, CRC, pérdidas,
desarmados e intervalos sin resultado. GPS sigue como resumen de1 Hz, no una
captura completa de NMEA. Guardar raw, metadata, sdkconfig, CSV y manifiestos.

Los pasos `f`, `s`, `a`, controles de orientación/frescura y ecuaciones de las
secciones anteriores no cambian. Trasero, no cabina, aporta la excitación.
La tabla física/matemática anterior sigue siendo una configuración experimental,
no medición del camión ni del automóvil. No se amplían buffers ni particiones.

## Verificación y explicación al profesor

`virtual_tests.c`: equilibrio, simetría, suma de masas/CG, inversión elíptica,
periodo numérico2.193247958 s frente a referencia analítica lineal2.193247957 s,
energía decreciente con damping; cambio de llenado, vacío/lleno separados,
NAN/cuaternión inválido, entradas reales sin agua, caducidad, pérdida y rearme.
Prueba específica de bypass: cambiar r no cambia LTR con lever OFF, pero gyro
y alpha siguen influyendo. Las regresiones integration/continuity/bench
conservan los ensayos anteriores de límite y asincronía. Captura verifica CRC,
metadatos tipo8 y decoder. Esto es verificación software, no validación física.

Exploración determinista: a_Y=0.15 sin(0.8 omega_nom t) m/s² durante10 s,
orientación/gyro nulos; cada fila varía sólo lo indicado y mantiene masa/CG.
No son intervalos de confianza ni prueba representativa de conducción.

| Variación | Pico |LTR| |
|---|---:|
| CG seco Z -0.20 / nominal / +0.20 m | 0.048479 / 0.050742 / 0.053009 |
| Masa modal x0.8 / x1 / x1.2 | 0.044738 / 0.050742 / 0.056761 |
| zeta 0 / 0.05 / 0.10 | 0.062206 / 0.050742 / 0.043475 |

Demostración breve: explicar primero que la pipa es virtual; mostrar escenario
50 % y parámetros estimados, comprobar reposo y signos con las placas; armar,
mostrar LTR y estados ante excitación medida suave, luego desconectar/reiniciar
trasero para mostrar SIN RESULTADO y rearme necesario. Conservar captura para
medir si el enlace/reportes permiten continuidad real. No usar la pantalla
como criterio para decidir maniobras. Si aún no se puede comprobar montaje o
GPS, mostrar diagnóstico real y la demo sintética explícitamente separada,
sin afirmar que ya se ejecutó el cálculo con entradas reales.
