> Preparación vigente: [guía de prueba de vehículo](README-prueba-vehiculo.md).
> Los apartados de cableado/comandos anteriores se conservan como historial; ambos nodos actuales son ESP32-S3.

> Historial de revisión/desarrollo: las referencias a target ESP32 clásico,
> GPIO antiguos y sus binarios no son preparación vigente. Hardware actual: [ambos S3](hardware-esp32s3.md).

Revisión de implementación y preparación de prueba física — 2026-11-03

Rama inspeccionada: `Jesus/integracion-cabina`. Esta revisión no modifica
firmware, configuración, protocolo ni bloqueos. Sólo añade este documento.
Se ejecutó nuevamente `python pruebas/modelo-a/run-all.py`: ambas suites PASS.
No hubo push, commit, cambio de rama ni flasheo. No se repitieron compilaciones
porque no se cambió código; las compilaciones anteriores están documentadas.

**1. Parámetros y bloqueos reales**

Fuentes locales usadas en las tablas, relativas a raíz:

* C: `firmware/componentes/control_cabina/{include/control_cabina.h,control_cabina.c}`
  y `firmware/integracion/cabina/main/configuracion_modelo.c`.
* D: `firmware/componentes/modelo_a/{include/modelo_a.h,dinamica.c}`.
* G: `firmware/componentes/modelo_a/modelo_a.c`.
* H: `docs/modelo-a.md`, historial de medidas, estimaciones y discrepancias.
* I: `docs/integracion-cabina.md`, integración actual y procedencia exacta.
* A: `firmware/integracion/cabina/main/{app_main.c,seleccion.c,Kconfig.projbuild}`.
* P: `firmware/integracion/cabina/main/modules/gps/{gps_module.c,driver/gps_config.h,driver/gps_validation.c}`.
* R: `firmware/integracion/trasero/main/app_main.cpp` y `firmware/componentes/telemetria/`.

El valor efectivo real lo devuelve `ci_live_model_config()` ->
`cc_pending_config()`. La tabla de trazabilidad G no alimenta automáticamente
todos los campos C: tener una medida anotada no configura por sí solo el motor.
`NAN` significa número desconocido. Los campos `pendulum.*` están a cero por
inicialización, con estado `MA_MISSING`: esos ceros NO son parámetros medidos.

Tabla de números físicos y de identificación pendientes:

| Nombre y significado | Valor real actual | Fuente consultada | Dato propio recuperado | Qué falta exactamente | Salida bloqueada / motivo |
|---|---|---|---|---|---|
| `mounting` w,x,y,z: sensor trasero -> vehículo | cuatro NAN | C, D, H/I | Convención X delante/Y izquierda/Z arriba | Medir orientación de placa y obtener rotación unitaria, incluidos signos | Aceleración alineada, gravedad, agua nivelada y dinámica |
| `rear_r.x` | +0.64 m | C, G, I | Ya medido provisionalmente desde centro geométrico de tanque | Confirmar referencia/montaje; no volver a pedir el número | Brazo de palanca: además exige `position_accepted` |
| `rear_r.y` | −0.42 m | C, G, I | Medido desde centro del diferencial | Verificar alineación diferencial/centro cisterna | Evita atribuir un brazo equivocado al tanque |
| `rear_r.z` | −0.94 m | C, G, I | Ya medido provisionalmente | Confirmar cotas y cuerpo común | Corrección de posición |
| `latency_bound_us`: cota de retardo no contenida en edades transmitidas | 0, con `latency_bound_known=false` | C, A, R, I | Existe eco RTT, sin registro físico recuperado | Medir transporte y reconocer el retardo BNO/driver; RTT no es latencia unidireccional exacta | Todos los reportes se rechazan para cálculo mientras la cota esté pendiente |
| `raw_to_m`: profundidad por cuenta de registro | NAN m/cuenta | C, H/I, Twig agua2/informe PDF citado en I | 0x0004 siguió profundidad; no se documentó escala | Calibración contra profundidades conocidas/manual del sensor | Agua: no se pueden obtener profundidad, volumen ni masa reales |
| `sensor_height_m`: altura de sonda respecto del fondo interior | NAN m | C, H/I | No encontré medida propia | Medir ubicación de la toma sensible, no sólo carcasa | Profundidad absoluta del agua |
| `water_offset_m`: offset aditivo | NAN m | C, H/I | No encontré calibración propia | Establecer offset con referencia conocida; cero sólo si está comprobado | Profundidad y agua real |
| `dry_mass_kg`: masa equivalente sin agua | NAN kg | C, H, G | Cisterna ~924 kg estimados; diésel 209 L × 0.832 kg/L = 173.888 kg | Masa completa de chasis/cabina, tanque, plataforma, bomba, accesorios, ocupantes/equipo y combustible sin duplicaciones | Fuerzas secas y LTR; las dos cifras recuperadas no son el total |
| `dry_position.{x,y,z}`: CG seco respecto de referencia geométrica | tres NAN m | C, H | No hay CG completo; 1.20 m del viejo baseline era supuesto, no medición | Determinar CG agregado y transformar de altura sobre suelo a z relativo | Momento seco; x también participa si hay rotación general, y puede producir momento gravitatorio |
| `reference_ground_m`: altura de referencia geométrica | NAN m | C, G, H/I | Derivable provisional: 1.16 + 1.22/2 = **1.77 m**; anotado 1.73 m | Resolver discrepancia o aceptar explícitamente la derivación provisional en una configuración experimental documentada | Traslado de momento al apoyo y comprobación del centroide; no hace falta inventar otra cota |
| `track_m`: vía efectiva del modelo agregado | NAN m | C, H/G/I | Separaciones anotadas delantera 2.01 m y trasera 1.81 m | Confirmar referencia de esas medidas y justificar vía/modelo agregado, no promediarlas automáticamente | Denominador del LTR |
| `fixed_water_mass_kg`: agua equivalente no oscilante m_f | NAN kg | C, D, H/I | Masa total se obtiene de volumen válido; no hay fracción oscilante identificada | Identificar m_f junto con m_p para el nivel de llenado | Debe cumplirse m_f + m_p = masa de agua; LTR bloqueado |
| `fixed_water_position.{x,y,z}`: posición de m_f | tres NAN m | C, D | Centroide estático total del agua sí se calcula | Identificar posición de la parte fija; el centroide total no determina por sí solo esta posición | Fuerza/momento de agua fija y balance de CG |
| `pendulum.oscillating_mass_kg`: masa oscilante m_p | 0; estado ausente | C, D, H/I | No hay identificación propia | Identificar m_p>0, sin usar toda el agua por defecto | Dinámica y fuerzas del agua |
| `pendulum.semi_y_m`: semieje horizontal A de trayectoria de masa equivalente | 0 m; ausente | C, D | Ancho interior geométrico conocido aproximadamente; NO es A del péndulo | Identificar trayectoria equivalente | Dinámica: requiere A>0 |
| `pendulum.semi_z_m`: semieje vertical B de trayectoria | 0 m; ausente | C, D | Altura interior aproximada; NO es B del péndulo | Identificar trayectoria equivalente | Dinámica: requiere B>0 |
| `pendulum.center_z_m`: centro vertical z0 de trayectoria | 0 m; ausente | C, D | Centroide estático del agua calculable | Identificarlo coherente con A/B/m_p/m_f y referencia | Debe coincidir el CG conjunto en reposo con el geométrico |
| `pendulum.damping_nm_s`: amortiguamiento generalizado c | 0 N·m·s; ausente | C, D, H/I | Hay dos rompeolas; no hay amortiguamiento medido | Identificar o justificar valor experimental; dos rompeolas no determinan c | El estado pendiente bloquea; numéricamente c=0 es admisible sólo como modelo explícito sin amortiguamiento |
| `pendulum.max_angle_rad`: amplitud admisible | 0 rad | C, D | No hay dominio físico identificado | Elegir y justificar 0<límite<π/2 para el modelo identificado | Integración rechazada; no habilitar amplitudes grandes sin fundamento |
| `pendulum.max_dt_s`: intervalo máximo de entrada | 0 s | C, D | Adquisición/TX objetivo 0.01 s, no medida física de cumplimiento | Fijar política tras medir datos y comprobar estabilidad/convergencia | dt positivo y <= máximo, o dinámica inválida |
| `pendulum.max_step_s`: subpaso máximo RK4 | 0 s | C, D | Ensayos sintéticos usan subpasos explícitos; no son configuración real | Elegir según frecuencias identificadas y convergencia; <=max_dt_s | Dinámica rechazada; máximo 10000 subpasos por llamada |

Supuestos y estados pendientes, distintos de números faltantes:

| Campo / condición | Valor actual | Fuente | Recuperado / qué falta | Efecto |
|---|---|---|---|---|
| `configured` | false | C | Marca de activación de configuración real completa; no representa una medición | Sale de `cc_process` antes de convertir agua o excitar dinámica |
| `mounting_known` | false | C | Validar `mounting`, no poner true sólo para pasar la comprobación | Bloquea conversión espacial y aguas/dinámica en este pipeline |
| `position_accepted` | false | C, G/I | r ya está recuperado; falta aceptación razonada de sus referencias provisionales | Bloquea pipeline real |
| `rigid_assumed` | false | C, H/I | IMU está en chasis; verificar relación chasis/cisterna o declarar supuesto experimental de solidaridad | Sin cuerpo común no se puede usar brazo de palanca para excitación del tanque |
| `latency_bound_known` | false | C, R/I | Falta ensayo de latencia y límites de edad física | Bloquea frescura de reportes |
| `water_calibration_known` | false | C, H/I | Confirmar magnitud/escala del registro y altura/offset | Bloquea nueva estimación de agua |
| `mass_accounting_complete` | false | C, H | Incorporar todos los componentes una vez; GVWR=15876 kg no es masa vacía; estimación 924 kg excluye plataforma/bomba/accesorios | Bloquea fuerza/momento y LTR |
| `track_justified` | false | C, H/I | Medidas 2.01/1.81 m existen; falta modelo agregado por ejes/justificación | Bloquea LTR |
| `pendulum.status` | `MA_MISSING` (0) | C/D | Parámetros identificados o estimaciones con procedencia; para modo real se espera estado usable experimental | Bloquea paso dinámico, aunque otros campos se rellenen |
| `simulation` | false | C/A | Es selector de procedencia, no una autorización de validez. Para datos reales debe permanecer false | No es un bloqueo actual; true no debe usarse para disfrazar datos reales de demo |
| `armed` / estado inicial del péndulo | false / estado no establecido | C/A | Comando `a` supone expresamente θ=0 y θ̇=0, con reposo/agua/configuración; no es una medición inicial ni tare | No se integra sin armado; reinicios/huecos vuelven a exigirlo |
| `stationary_settled_confirmed` | false al arrancar | A/C | `s` sólo con GPS válido <=0.5 km/h; verificar visualmente que el líquido esté asentado | Bloquea actualización de agua y presentación de porcentaje; GPS cero por sí solo no basta |
| `no_filling_or_draining_confirmed` | false al arrancar | A/C | `f` confirma ausencia de llenado/descarga; `u` invalida volumen/desarma antes de cambiar agua | Bloquea uso dinámico de masa retenida |
| `have_water` y edad de última estimación | false, sin lectura geométrica válida | C | Debe adquirirse en reposo/nivel, con calibración y frescura | Bloquea dinámica y porcentaje real |
| Modelo seleccionado | A por defecto sin botón; B seleccionable con botón | A | Sólo hay configuración de A | B muestra SIN DATOS; no tiene parámetros matemáticos |

Políticas numéricas existentes que pueden rechazar datos. No son medidas
faltantes; requieren evaluación de funcionamiento/criterio de ensayo:

| Campo y significado | Actual | Fuente / dato propio | Qué falta comprobar | Salida afectada |
|---|---|---|---|---|
| `max_report_age_us` | 100000 us = 100 ms | C; política de software | Edad real, incluida cota de transporte | Frescura de sensores/resultados |
| `max_skew_us` | 20000 us = 20 ms | C | Desfase entre accel/gyro/orientación | Pares de reportes para cálculo |
| `water_hold_us` | 2000000 us = **2 s** | C | Política útil para maniobras y cambios de masa | Tras pasar 2 s sin actualización admisible, no hay LTR mostrado ni porcentaje retenido vigente |
| `alpha_min_dt_s` | 0.002 s | C/D | Frecuencia y jitter de giro; debe ser >0 y menor que máximo | Derivada angular |
| `alpha_max_dt_s` | 0.05 s | C/D | Huecos/jitter del flujo | Derivada angular y continuidad |
| `alpha_filter_tau_s` | 0.03 s | C/D | Ruido, retardo y respuesta del filtro; no está calibrado en vehículo | Excitación angular; tau<0/no finito invalida |
| `level_tolerance_rad` | 0.03 rad ≈1.72° | C | Pendiente residual y orientación; no se ha validado tolerancia hidrostática | Actualización del agua |
| `min_report_quality` | 2, calidad SH2 media | C, driver importado | Si cada reporte realmente alcanza ese estado; desconocida/>3 no se acepta | Corrección y dinámica; no equivale a precisión física validada |
| `stopped_kmh` / `moving_kmh` | 0.5 / 2 km/h | C | Criterios experimentales de velocidad | En la banda intermedia conserva estado previo; al arrancar ahí puede ser desconocido |
| `risk_warning` / `risk_critical` / `risk_hysteresis` | 0.60 / 0.75 / 0.03 | C; umbrales heredados de ensayo | Validación de clasificación; no es dato físico demostrado | Semáforo; configuración inválida bloquea salida. No determina por sí sola la corrección física del LTR |
| GPS: edad RMC/GGA | <=1800 ms | P, heredado de TACOS | Recepción física, parser y caducidad | Si falla: movimiento desconocido y SIN DATOS |
| GPS: skew de época, satélites, HDOP | <=200 ms; >=4; 0<HDOP<=4 | P | Fix válido y coherencia RMC/GGA | Aceptación de velocidad/reposo |

Los límites GPS de 180 km/h, 25 km/h/s y tolerancia 10 km/h están declarados
en `gps_config.h`, pero el camino actual de `gps_module` toma velocidad RMC
y `gps_validation`; no se deben presentar esos otros límites como filtros
efectivamente aplicados a esta salida sin seguir su uso.

Comprobaciones de funcionamiento necesarias, sin confundirlas con parámetros:

| Comprobación | Configuración/evidencia actual | Qué falta / bloqueo |
|---|---|---|
| GPIO y alimentación | Perfil de mesa esp32; tabla de conexiones más abajo | Validar conexiones y niveles reales; no hay prueba física integrada |
| GPS/IMU local/botón | Tres flags deshabilitados | Activarlos explícitamente sólo después de confirmar cableado; IMU local no es entrada del LTR actual |
| Radio CST2 | 124 bytes, versión 2, canal 1, broadcast; ambos proyectos compilados | Comprobar llegada, sesión, secuencias y RTT; el paquete de 17 bytes no basta para corregir posición |
| Integridad de reportes | Flags accel/gyro/orientación y tiempo de observación por reporte | Los tres deben existir, ser finitos, recientes y coherentes; no son timestamps exactos de adquisición MEMS |
| Continuidad | Duplicados no avanzan; salto en secuencias accel/gyro desarma; huecos y reinicios invalidan | Medir pérdidas a 100 Hz objetivo y verificar recuperación mediante rearmado explícito |
| Cuaterniones | Norma de orientación admitida con error <=1e-4, normalización numérica; montaje requiere norma unitaria en tolerancia 1e-6 | Verificar convención y signos; normalizar no calibra orientación |
| Registro/agua | 0x0004, función 03, slave 1, uint16 big endian, CRC Modbus; 9600 8N1 | Confirmar respuesta real/escala. Vigencia de origen 500 ms no garantiza la política cabina de 100 ms |
| Cadencia agua vs política cabina | Trasero espera 200 ms después de transacción; cabina exige edad <=100 ms para actualizar | No todas las transmisiones contendrán agua bastante reciente; no significa por sí solo fallo UART |
| Coherencia de partición del agua | tolerancia masa 1e-6·max(1,m_agua); CG 1e-5 m | Necesita m_p/m_f y posiciones coherentes con nivel actual; no existe actualización automática de parámetros por llenado |
| Soporte y dominio | N_total>0; \|LTR\|<1 para resultado utilizable; amplitud y dt dentro de límites | Fuera de contacto/amplitud se muestra SIN DATOS y se desarma; no predice después de vuelco |
| Recursos de ejecución | Compilación cabe en flash; logs MEM disponibles | Medir heap/DMA/pila en equipo, no sólo tamaño ELF; no hay prueba física |

Datos recuperados que no deben volver a pedirse ni usarse indebidamente:
W/H/L exteriores 2.22/1.22/3.04 m; t provisional 0.0047625 m; base 1.16 m;
batalla 3.92 m frente a placa 5.30; separaciones 2.01/1.81 m; dos rompeolas,
tramos trasero→delantero 1.00+1.04+1.00 m, altura aproximada 80%; capacidad
informada 1200–1500 gal sin nominal exacto. Escenario 1300 gal US no es
capacidad total. Agua 1000 kg/m³, gravedad 9.81 m/s². GVWR 15876 kg no
rellena `dry_mass_kg`. La geometría estática funciona aunque falten masas.
No hay un campo real para aberturas/espesores de rompeolas: faltan para
identificar dinámica, pero no existe una comprobación numérica directa de ellos.

**2. Modelo implementado, unidades y alcance**

Código ejecutable en `dinamica.c`, acoplado en `control_cabina.c`.
No implementa las ecuaciones completas ni los ajustes de Zheng/Li/Shin.

Ejes de vehículo: X delante, Y izquierda, Z arriba. Referencia O: centro
geométrico del tanque, distinto de CG. La excitación usa sólo el IMU trasero.
Después de rotar aceleración lineal y giro del sensor a vehículo:

```
a_O = a_sensor - alpha × r - omega × (omega × r)
alpha_raw = (omega_n - omega_anterior) / dt
alpha_filtrada_n = alpha_filtrada_anterior
                  + dt/(tau+dt) * (alpha_raw - alpha_filtrada_anterior)
```

`a_*`: m/s²; `r`: m, O→sensor; `omega`: rad/s; `alpha`: rad/s²;
`dt` y `tau`: s. Primera muestra no da alpha válido; reloj distinto/sesión
nueva reinician historia; tiempos repetidos/invertidos o intervalos fuera
de política no se interpretan como aceleración angular cero.
El filtro inicia internamente en cero: su transitorio y retardo no están
caracterizados con ruido real.

Se usa **Linear Acceleration** del BNO, con gravedad ya excluida.
No se resta gravedad otra vez en la corrección. Para las ecuaciones mecánicas:

```
g_vehiculo = R_sensor_a_vehiculo · R_mundo_a_sensor · (0,0,-9.81)
```

El cuaternión recibido se interpreta sensor→mundo; su conjugado/inversa
lleva gravedad a sensor. Game RV no aporta una referencia absoluta estable
de rumbo como una alineación de montaje; no se trata el tare como nivelación.
La convención espacial de la librería y el montaje aún requieren ensayo.

Trayectoria de la única masa oscilante equivalente:

```
q(theta) = (0, A sin(theta), z0 - B cos(theta))
t(theta) = dq/dtheta = (0, A cos(theta), B sin(theta))
k(theta) = d²q/dtheta² = (0, -A sin(theta), B cos(theta))
qdot = t theta_dot
a0 = a_O + alpha×q + omega×(omega×q) + 2 omega×qdot + k theta_dot²
theta_ddot = [ t·(g_vehiculo - a0) - c theta_dot/m_p ] / (t·t)
a_p = a0 + t theta_ddot
```

| Variable | Significado / unidades |
|---|---|
| θ, θ̇, θ̈ | Coordenada angular de trayectoria, rad; rad/s; rad/s². θ positivo desplaza masa hacia +Y cerca de reposo; NO es roll del camión |
| A, B, z0 | Semiejes y centro vertical de trayectoria equivalente, m; no son automáticamente semiejes interiores de la cisterna |
| q, t, k | Posición relativa y derivadas respecto a θ, m usando radianes |
| m_p, m_f, m_d | Masa oscilante, agua fija equivalente y masa seca agregada, kg |
| c | Amortiguamiento generalizado, N·m·s; Qθ=−c θ̇ |
| a0, a_p | Aceleración absoluta expresada en ejes del vehículo, m/s²; a0 excluye únicamente t θ̈ |
| omega/alpha | Giro y aceleración angular de marco prescrito, rad/s y rad/s² |

La ecuación resulta de proyectar Newton en la tangente: una restricción ideal
no ejerce trabajo tangencial. Se integra RK4 con excitación constante durante
cada intervalo de entrada y subpasos; no interpola ni recupera muestras perdidas.
El roll no es otro estado calculado: entra a través de orientación/gravedad
y del giro prescrito. No se resuelve respuesta de suspensión/roll del camión.

Fuerzas del agua **sobre el tanque**, y fuerza equivalente de masa seca:

```
F_p = m_p (g_vehiculo - a_p)
F_f = m_f [g_vehiculo - a_O - alpha×q_f - omega×(omega×q_f)]
F_d = m_d [g_vehiculo - a_O - alpha×q_d - omega×(omega×q_d)]
M_Ox = (q × F_p).x + (q_f × F_f).x + (q_d × F_d).x
F_total = F_p + F_f + F_d
M_apoyo_x = M_Ox - h_O * F_total.y
N_total = -F_total.z
LTR_firmado = 2 M_apoyo_x / (N_total * T)
```

F en N, M en N·m, h_O=`reference_ground_m` y T=`track_m` en m.
LTR adimensional. Trasladar desde O usa vector (0,0,h_O) en el marco
idealizado; el plano de apoyo del modelo agregado no es una solución completa
de los contactos rueda/suelo durante roll de carrocería.
Convención LTR=(N_derecha−N_izquierda)/N_total; a_y positiva, CG centrado
y altura positiva producen momento positivo. Se guardan valor firmado,
magnitud y valor firmado limitado a [−1,1] por separado. |LTR|>=1 no es
una predicción de movimiento posterior a pérdida de contacto.

No hay doble conteo: `dry_mass_kg` excluye agua; sólo se suman m_p y m_f
para el agua. Se exige m_p+m_f=m_agua y:

```
[m_p (z0-B) + m_f q_f.z] / m_agua + h_O = CG_agua_estatico_sobre_suelo
```

No se suma además `m_agua*CG_estatico*a_y` ni el desplazamiento heurístico
del baseline. El CG estático sólo comprueba coherencia; no añade un segundo
momento del agua. El código no impone, por sí solo, que un valor declarado
como masa seca realmente excluya combustible duplicado o accesorios: eso
requiere contabilidad documentada y la bandera correspondiente.

Zheng/Li se consultaron como referencias de modelos equivalentes tipo
trammel; el acceso completo a ecuaciones/ajustes no se consiguió en la
implementación previa. Shin tampoco se reprodujo ni se contrastó ecuación
por ecuación. Por tanto no atribuyo una comparación formal de equivalencia
con ninguno: falta identificación por llenado, ajustes publicados, validación
contra sus resultados y acoplamiento completo de vehículo. Esta derivación
es una masa puntual en elipse con movimiento del marco impuesto, más un
balance agregado de apoyo. No son tres péndulos por los rompeolas ni CFD.
Los títulos/DOI y los intentos de acceso están en `integracion-cabina.md`.

Límites: geometría estática nivelada; extremos planos/interior aproximado;
sin desplazamiento de rompeolas; una masa oscilante identificada para un
nivel; montaje y rigidez supuestos; fuerzas secas agregadas, sin flexión de
cabina/chasis/tanque ni neumáticos/suspensión; amortiguamiento no identificado;
límites de amplitud elegidos explícitamente; sin alerta validada.

Pruebas que existen realmente:

| Parte | Referencia usada | Independencia y alcance |
|---|---|---|
| Vacío/lleno/media altura y centroide | Simetría de elipse; CG de media elipse = b−4b/(3π) sobre fondo | Referencia analítica independiente; prueba geométrica, no tanque real |
| Monotonía/inversión | 1001 alturas y retorno con mismo evaluador | Propiedad/consistencia; NO segunda implementación independiente de volumen |
| Transformación de ejes | Rotaciones conocidas 90° y 180° en Z | Resultado cartesiano analítico independiente |
| Brazo de palanca | alpha=(0,0,2), r conocido: alpha×r=(0.84,1.28,0); omega=(0,0,2): centrípeta=(−2.56,1.68,0) | Casos analíticos independientes: corrección debe devolver cero |
| alpha | omega_z sube 0.02 rad/s en 0.01 s -> 2 rad/s²; duplicado/hueco/sesión | Analítico para rampa constante; NO caracteriza ruido o transitorio de filtro |
| Péndulo en reposo | θ=0 y Fz=−m_p g | Analítico independiente |
| Equilibrio lateral | tan θ=−a_y A/(gB), Fy=−m_p a_y | Analítico independiente en marco no giratorio |
| Energía sin amortiguamiento | E=½m_p(A²cos²θ+B²sin²θ)θ̇² + m_p g B(1−cosθ) | Invariante analítico; conservación a tolerancia 1e−8 en caso sintético |
| Convergencia RK4 | pasos 0.04/0.02 s frente a solución con 0.001 s | Refinamiento numérico con el mismo integrador; NO solución exacta temporal independiente |
| Amortiguamiento | Energía final menor que ¼ inicial para c=2 sintético | Prueba de disipación; no prueba una curva analítica exacta ni amortiguamiento real |
| Fuerza/momento fijo | 100 kg a 1 m, ay=2: Fy=−200 N, Fz=−981 N, Mx=200 N·m | Analítico independiente |
| LTR firmado y límite | M,N,T sintéticos: LTR=1 y −2; salida limitada −1 | Álgebra independiente del paso dinámico; no reacciones medidas de ruedas |
| Partición/acoplamiento | Configuración demo positiva con masas/CG coherentes | Prueba funcional end-to-end; faltan casos negativos dedicados para todas las incoherencias de masa/CG |
| Protocolo y estados | bytes endian conocidos, NaN, duplicados, huecos, reinicio, GPS inválido | Comprueba software/CC_NO_DATA; no prueba visual de LVGL en hardware |
| Modbus | trama sintética, parcial, CRC alterado, excepción | No sustituye respuesta de sonda ni caracterización de timeout físico |

Faltan referencias independientes para respuesta transitoria general con
rotación combinada, pendiente/gravedad inclinada, movimiento del vehículo,
comparación CFD/papers y fuerzas reales. Las suites actuales pasaron otra vez;
su alcance no es validación física. Pruebas: `host_tests.c` e
`integration_tests.c`; ejecución reproducible: `run-all.py`.

**3. Función de cada IMU**

* Trasero, montado en chasis: transmite aceleración lineal XYZ, gyro XYZ y
  Game RV. Es la única excitación dinámica usada por `cc_process`. Para
  trasladarla al centro geométrico del tanque se exige montaje conocido,
  brazo aceptado y supuesto explícito chasis/cisterna solidarios.
* Cabina: adquiere sus propios reportes y registra `CABIN_IMU`; actualmente
  no excita el péndulo, no calcula una corrección para el chasis, no es un
  fallback y su ausencia no bloquea por sí sola el LTR del pipeline.

**No se interpreta la diferencia cabina/chasis como movimiento relativo
cisterna/chasis.** No hay IMU montado directamente en cisterna que mida ese
movimiento. La función matemática `ma_imu_accel_difference` permanece como
utilidad diagnóstica, exige mismo dominio de reloj/alineación y no se llama
desde `app_main` ni `cc_process`. No sincroniza las placas, no calcula
desplazamiento relativo y no valida rigidez del tanque.

**4. Tabla única de cableado de cabina**

Perfil de la tabla: ESP32 básico usado en tus pruebas. No es un pinout final
para S3. «Conservado» indica procedencia del proyecto de pantalla previamente
trabajado; no significa que yo haya hecho una prueba física integrada.

| Dispositivo / señal | Conexión configurada en cabina | Bus / sentido | Estado y evidencia | Diferencia respecto de TACOS |
|---|---|---|---|---|
| ST7796S SCK/CLK | GPIO18 | SPI2 salida | Conservada del display previo; sin prueba física nueva | GPS RX18 antiguo no puede compartirla |
| ST7796S MOSI/SDI | GPIO19 | SPI2 salida | Conservada | Sin cambio |
| ST7796S CS | GPIO4 | Salida | Conservada | Sin cambio |
| ST7796S DC | GPIO21 | Salida | Conservada | Sin cambio |
| ST7796S RST | GPIO22 | Salida | Conservada para ESP32 básico | No trasladar sin revisar placa S3 |
| ST7796S MISO/touch | Sin conexión usada; MISO=−1 | No utilizado | Display no necesita lectura/touch en esta configuración | La macro de touch CS15 no implica conexión activa |
| ST7796S VCC / BL / GND | Sin GPIO de alimentación/backlight definido por firmware; GND común | Según módulo físico | Alimentación/BL exactos no constan en código; verificar conexiones que ya usaste | No inventar tensión de alimentación del breakout |
| GPS TX -> ESP RX | GPIO16 | UART2 entrada, 9600 8N1 | **Propuesta no probada** | Antes RX18, en conflicto con SCK pantalla |
| GPS RX <- ESP TX | GPIO17, si se conecta | UART2 salida | Se conserva número de código TACOS; conjunto integrado no probado | Antes TX17 también |
| GPS VCC / GND | Según módulo, GND común | Alimentación | No encontré esquema de alimentación probado en documentación local | Confirmar módulo exacto, no inferir VIN de los pines UART |
| BNO085 DI/MOSI | GPIO13 | SPI3 salida, 3 MHz | **Propuesta no probada** | TACOS MOSI11 |
| BNO085 DO/MISO | GPIO26 | SPI3 entrada | **Propuesta no probada** | TACOS MISO13 |
| BNO085 CLK/SCK | GPIO14 | SPI3 salida | **Propuesta no probada** | TACOS SCK12 |
| BNO085 CS | GPIO27 | Salida | **Propuesta no probada** | TACOS CS14 |
| BNO085 HINT/INT | GPIO32 | Entrada | **Propuesta no probada** | TACOS INT6 |
| BNO085 RST | GPIO25 | Salida | **Propuesta no probada** | TACOS RST5 |
| BNO085 PS0/PS1 | Altos, modo SPI según configuración fuente | Pines del módulo | Código/documentación importados lo requieren; revisar resistencias y placa real | No es un GPIO de cabina adicional |
| BNO085 VCC / GND | Según breakout, lógica compatible con ESP32, GND común | Alimentación | No se verificó alimentación integrada | No confundir admisión de VIN del breakout con nivel lógico de señales |
| Botón | GPIO23 a GND; pull-up interno | Entrada activa baja | GPIO heredado de selección anterior, **prueba física no acreditada** | Sin pin alternativo configurable en menuconfig actualmente |

GPS e IMU están deshabilitados para no arrancar con cableado TACOS conflictivo
en el ESP32 básico: GPIO6/11 se asocian normalmente a flash, y GPS RX18
colisiona con pantalla. El botón está deshabilitado porque no se confirmó su
conexión física; no tiene un conflicto con los pines propuestos de la tabla.
La comprobación `wiring_valid` es sólo una validación parcial de duplicados y
rango del perfil básico; no demuestra compatibilidad de todos los módulos.

Pasos exactos de habilitación, sin ejecutarlos aquí:

1. Con equipo sin alimentación, comparar conexiones reales con esta tabla;
   conservar pantalla probada y decidir/registrar las reasignaciones propuestas.
2. Activar entorno ESP-IDF 6.1 e ir a
   `C:\esp\Proyectos\Cistable\firmware\integracion\cabina`.
3. Ejecutar `idf.py menuconfig` -> **Cistable integration (experimental)**.
4. Desactivar demo para lectura real. Activar `CI_ENABLE_GPS` y comprobar
   `CI_GPS_RX=16`, `CI_GPS_TX=17` contra cableado; guardar.
5. Activar `CI_ENABLE_CABIN_IMU` y comprobar MOSI13/MISO26/SCLK14/CS27/INT32/RST25;
   activar `CI_ENABLE_BUTTON` sólo si existe el botón en GPIO23; guardar.
6. Ejecutar `idf.py build`. Comprobar en `sdkconfig` target `esp32` y flags
   `CONFIG_CI_ENABLE_...=y`; revisar que no aparezca mensaje de GPIO inválidos.
   Cada modificación requiere recompilar. Este documento no instala firmware.

Conviene habilitar primero GPS, después IMU y por último botón para aislar
fallos. Sus flags no rellenan parámetros matemáticos ni cambian `configured`.
En demo, GPS/IMU no arrancan aunque sus flags estén activos. El botón sí puede
operar en demo; para una primera demostración dejarlo deshabilitado facilita
ver el comienzo de la secuencia sin esperar selección.

**5. Tres pruebas distintas y resultados esperados**

Proyecto cabina único: `firmware/integracion/cabina`, target `esp32`,
flash configurada 4 MB, sin PSRAM, single app large. Emisor real:
`firmware/integracion/trasero`, target `esp32s3`, flash configurada 16 MB.
Son proyectos nuevos de integración; no usar el viejo `PRUEBA MASTER` ni el
ejemplo de `pruebas/modelo-a/esp-idf` como sustitutos del proyecto integrado.

| Prueba | Configuración exacta | Pantalla esperada | Consola que sí existe hoy |
|---|---|---|---|
| A: demostración visual | Cabina `CI_DEMO=y`; GPS/IMU/botón preferentemente n; `demo.c` configura todo sintético; sin emisor | Marca DEMOSTRACION; aproximadamente 50% por volumen durante primeros 3 s de simulación; luego estado de riesgo sintético de A | MASTERDATA status simulado, LTR sintético cuando se calcula, MEM. `cabin_online=0` y rx_count=0 son normales; source_session corresponde a DEMO, no a radio |
| B: radio real y diagnóstico disponible | Cabina `CI_DEMO=n`; parámetros reales pendientes como están; flags GPS/IMU según cableado confirmado; trasero CST2 del proyecto nuevo; ambos canal 1 | Marca EXPERIMENTAL; SIN DATOS para cálculo. Botón puede pedir selección A/B; B siempre SIN DATOS | Cabina MASTERDATA con RX/sesión/secuencia/estado/motivo y CABIN_IMU si local responde; trasero V2 con secuencias, resultado de encolado y último RTT; avisos Modbus en errores |
| C: cálculo experimental | Mismos proyectos/modo real que B; editar conscientemente `configuracion_modelo.c` con números/estados de tablas, `simulation=false`; compilar; datos recientes y sesión continua; `f`, luego `s`, luego `a` bajo condiciones verificadas | Reposo confirmado + agua válida: porcentaje A. Movimiento conocido + LTR utilizable: semáforo experimental. Caducidad/incompleto/fuera de dominio/GPS inválido: SIN DATOS | MASTERDATA con estado experimental y LTR firmado. `a` imprime supuesto θ=0/θ̇=0 y armado. No representa calibración de IMU ni validación de alerta |

En A, la fase inicial puede incluir SIN DATOS mientras se reúne la primera
pareja/estado. Los datos sintéticos avanzan desde el arranque, no desde la
confirmación del botón. La demo retiene agua 60 s desde la última actualización
en reposo (~3 s): **alrededor de 63 s de simulación pasará a SIN DATOS por
caducidad**, si no la bloquea antes otro límite. No es una demo cíclica infinita.

En B hay una limitación de observabilidad: **no se imprimen en consola los
vectores, calidad, cuaternión ni water_raw del paquete CST2 recibido**.
El emisor tampoco imprime el valor raw en cada lectura exitosa. El paquete
los lleva, pero los logs V2/MASTERDATA actuales no permiten certificar esos
valores numéricos. El log LEGACY sí muestra valores del antiguo paquete de
17 bytes, que no habilita el modelo. Tampoco hay actualmente un log periódico
detallado del fix GPS en este `app_main`; el inicio de UART no prueba un fix.
Para B puede verificarse transporte, reinicios, tasas de secuencia y flujo
local de cabina; una prueba completa de magnitudes traseras/agua/fix requerirá
inspección con depurador o una mejora de diagnóstico posterior, no realizada
en esta revisión. No debe marcarse esa parte como probada por recibir paquetes.

En C `f` declara que no se llena/descarga. Con fix válido y velocidad <=0.5,
`s` confirma líquido asentado; se pierde esa confirmación al moverse o perder
GPS. El cálculo también comprueba nivelación. `a` requiere configuración,
agua presente y las confirmaciones; además los datos/partición/dominio deben
seguir pasando comprobaciones. `u` invalida masa retenida y desarma antes de
llenar/descargar. Hay que repetir `s` al volver a reposo; un hueco/reinicio
exige un nuevo armado. Escribir comandos en consola del equipo sólo durante
una prueba autorizada por el operador; aquí no se enviaron comandos físicos.

No confundir tener todos los números con tener una prueba dinámica útil:
la retención real actual de agua es de 2 s después del último reposo válido.
Sin una nueva estimación admisible, C sólo permite una ventana corta de
salida utilizable. No se amplió para forzar un resultado. La política de
retención es una decisión pendiente antes de una prueba sostenida del vehículo.

Secuencia física concreta que sí se puede preparar con lo existente:

1. Guardar capturas de consola de cada placa y su configuración; ejecutar
   primero `python pruebas/modelo-a/run-all.py` (PASS hoy).
2. Prueba A: observar marca sintética, fase de porcentaje y fase dinámica;
   esperar caducidad prevista, sin confundirlo con datos reales.
3. Prueba B con sólo radio/pantalla: verificar que RX y secuencias cambian y
   que hay RTT; cortar/reiniciar trasero y comprobar nueva sesión/SIN DATOS.
4. Habilitar periféricos locales uno a uno tras revisar cableado: IMU produce
   CABIN_IMU, GPS inicia UART; no declarar fix válido sólo por ese log.
5. Dejar constancia de que faltan valores traseros/agua en los logs actuales.
   No calibrar `raw_to_m` a partir de secuencias o de un valor raw no observado.
6. Posponer C hasta disponer de las magnitudes/identificación y comprobaciones
   de las tablas. Ninguna prueba de mesa equivale a validar vuelco.

**6. Los seis archivos rastreados modificados y los proyectos nuevos**

Comparación respecto de HEAD `c396f4354b44a8b60ce08cbf9f9e6b311cd30bad`:

| Archivo rastreado | Por qué cambió |
|---|---|
| `docs/modelo-a.md` | Sólo se añadió nota inicial de actualización: arquitectura cabina/trasero, dimensiones confirmadas y enlace a integración. El cuerpo conserva historia anterior y contiene afirmaciones ya superadas; no usarlo como configuración vigente |
| `firmware/componentes/modelo_a/CMakeLists.txt` | Añade `dinamica.c` a fuentes del componente |
| `firmware/componentes/modelo_a/include/modelo_a.h` | Roles trasera/cabina, extern C, estados experimental/fuera de dominio, parámetros/salidas completas de dinámica y LTR, API de rotación/brazo/alpha/fuerzas |
| `firmware/componentes/modelo_a/modelo_a.c` | Dimensiones confirmadas, r/tramos de rompeolas y estados; adapta utilidad diagnóstica a roles; retira stubs trasladados a implementación en `dinamica.c`; conserva fórmulas geométricas |
| `pruebas/modelo-a/host_tests.c` | Adapta nombres de IMU y cambia expectativa de LTR completo de no disponible a experimental; mantiene geometría y validaciones |
| `pruebas/modelo-a/run-host.ps1` | Enlaza también `dinamica.c` para la suite original |

Nuevos y aún no rastreados: `dinamica.c`; componentes `control_cabina/` y
`telemetria/`; proyectos `firmware/integracion/{cabina,trasero}/`; dependencias
compartidas `firmware/integracion/components/`; procedencia/manifiestos/scripts;
tests `integration_tests.c`, runners `run-all.py` y `compile-variants.py`;
`docs/integracion-cabina.md` y este documento de revisión. El proyecto de
pantalla original no fue sobrescrito. Los 486 archivos importados incluyen
ETL/BNO/SH2 y recursos; los 11 editados después de importación están enumerados
en `firmware/integracion/modificaciones-importadas.json`.

La revisión añadió únicamente este archivo Markdown. Conserva los seis
modificados anteriores y todos los nuevos existentes. No se quitaron bloqueos,
no se habilitaron GPIO, no se rellenaron masas y no se instaló firmware.
