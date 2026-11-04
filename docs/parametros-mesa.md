> Preparación vigente: [guía de prueba de vehículo](README-prueba-vehiculo.md).
> Los apartados de cableado/comandos anteriores se conservan como historial; ambos nodos actuales son ESP32-S3.

> Actualizaci?n posterior: [perfil PIPA VIRTUAL con entradas reales](pipa-virtual.md).
> El registro siguiente conserva el estado anterior del perfil real incompleto.
> El usuario confirm? r=(+0.64,-0.42,-0.94) m, centro geom?trico cisterna -> IMU,
> X adelante/Y izquierda/Z arriba. Su rotaci?n de montaje sigue pendiente.
> El autom?vil tiene configuraci?n independiente y correcci?n de posici?n OFF.

# Registro de parámetros para mesa y configuración experimental

Actualización de procedencia para la próxima prueba: la IMU trasera estaba de
lado en la cara lateral del bastidor. Su rotación de montaje sigue sin comprobar
y permanece explícitamente NAN; no se obtiene de fotos ni restando90° al roll.
El usuario aclara que el agua de registros antiguos era simulada y que el GPS
falló por conexión de hardware. Esos campos no calibran la sonda ni acreditan
mediciones reales GPS/agua. El registro0x0004 es configuración candidata del
código, pendiente de comprobar con la sonda disponible en esta prueba.

Fecha: 2026-11-03. Fuentes recuperadas: tabla `ma_model_a_parameters` en
`firmware/componentes/modelo_a/modelo_a.c`, `docs/modelo-a.md` (histórico),
`docs/revision-prueba-fisica.md`, `docs/integracion-cabina.md` y los dos textos
originales adjuntos por el usuario. La confirmación posterior 2.22 × 1.22 m
prevalece sobre 2.20 × 1.24. No se volvieron a solicitar medidas documentadas.

Una estimación con ecuación, procedencia y limitaciones puede alimentar una
configuración **MA_EXPERIMENTAL** completa. No se exige medición directa de
cada parámetro. `configured`, `mass_accounting_complete` y `track_justified`
pueden describir una estimación completa y justificada; no significan validación
física. Hoy `ci_live_model_config()` sigue pendiente: este registro no activa
automáticamente el cálculo. Los números de `demo.c` no son estimaciones del camión.

«Incertidumbre no cuantificada» no significa cero: los datos originales no
aportan repetibilidad, tolerancias o distribución estadística. No se inventan
intervalos de confianza. Las alternativas documentadas se ensayan como
escenarios de sensibilidad, sin confundirlas con cotas físicas demostradas.

## A. Datos y estimaciones propias recuperados

| Parámetro, valor y unidades | Procedencia y método | Incertidumbre / uso experimental |
|---|---|---|
| W=2.22 m, H=1.22 m | Confirmación del usuario; geometría actual | Tolerancia no documentada; utilizables como geometría provisional |
| L=3.04 m | Medida exterior provisional original | Tolerancia no cuantificada; incluye simplificación de extremos planos |
| t=0.0047625 m (3/16 pulgada) | Espesor reportado; interior W−2t, H−2t, L−2t | Material/espesor real sin confirmar; estimación geométrica utilizable, no calibración |
| Base exterior=1.16 m; h_O derivada=1.77 m | Base + H/2 | Centro anotado 1.73 m discrepa 0.04 m; hipótesis preferida: base medida + dimensiones confirmadas; alternativa explícita para sensibilidad |
| r=(+0.64,−0.42,−0.94) m | Referencia geométrica→IMU chasis; Y desde diferencial | Utilizable provisionalmente tras configurar ejes y aceptar referencia; error no cuantificado, no es CG |
| Batalla=3.92 m; placa=5.30 m | Medición entre centros frente a placa | No intercambiarlas; no entra en el balance agregado implementado |
| Separaciones=2.01 / 1.81 m | Anotaciones delantera/trasera | Referencia de ruedas pendiente; no son automáticamente vías efectivas. Una vez establecida, un modelo de apoyo agregado puede evaluar ambos extremos, sin promedio automático |
| Dos rompeolas; zonas 1.00+1.04+1.00 m; altura ~80% | Datos originales | Comunican líquido; aberturas/espesores sin cuantificar. No equivalen a tres compartimentos aislados |
| ρ_agua=1000 kg/m³, g=9.81 m/s² | Constantes de trabajo documentadas | Estimaciones utilizables; sin corrección por temperatura/salinidad; no son parámetros identificados del oleaje |
| Cisterna ~924 kg | Estimación original: acero 7850 kg/m³, pared uniforme, dos extremos planos, dos rompeolas al 80% | Utilizable como contribución estimada, no como masa seca completa; omite orificios, plataforma/bomba/accesorios. No hay desviación cuantificada |
| Combustible 209 L ×0.832 kg/L=173.888 kg | Escenario documentado de llenado y densidad | Sólo si ese combustible existe en la configuración y no está incluido en otra masa; incertidumbre de cantidad no documentada |
| Capacidad informada 1200–1500 gal; escenario 1300 gal US | Notas originales | Tipo de galón/capacidad nominal sin confirmar; escenarios US explícitos, no lectura actual del agua |
| `dry_mass_kg` / `dry_position`=NAN kg / NAN m | No hay total ni CG completos en las fuentes | Se pueden estimar por suma m_i y Σm_i q_i/Σm_i, con registros del vehículo/materiales y ubicaciones existentes. Faltan masa de chasis/cabina, plataforma/accesorios, equipo y sus CG: hoy no hay fundamento numérico para completarlos |
| GVWR=15876 kg | Placa original | Es capacidad, no tara. No sustituye masa seca. Los antiguos 8000 kg y CG 1.20/1.80 m del baseline eran constantes sin fundamento recuperado para este vehículo |

Si se completa la contabilidad con estimaciones justificadas de componentes,
no hará falta convertirlas en mediciones directas para ensayar. Registrar qué
incluye cada masa y coordenada evita contar combustible, tanque o agua dos veces.
Cambiar parámetros exige invalidar/rearmar el estado previo, según la política
existente; no se arrastra sin justificación la energía de otro oscilador.

### Sensibilidad calculada con el código geométrico actual

`pruebas/modelo-a/parameter_scenarios.c` usa las API existentes, sin modificar
la configuración. Salida capturada: `build/logs/run-scenarios.stdout.txt`.
Capacidad geométrica=6.368571351 m³, omitiendo desplazamiento de rompeolas.

| Escenario explícito | V (m³) / masa a 1000 kg/m³ | h (m) | CG sobre suelo (m), base 1.16 | Porcentaje de volumen |
|---|---|---|---|---|
| 1200 gal US | 4.542494141 / 4542.494141 kg | 0.812092640 | 1.620543996 | 71.326737% |
| 1300 gal US | 4.921035319 / 4921.035319 kg | 0.873570694 | 1.650298478 | 77.270632% |
| 1500 gal US | 5.678117676 / 5678.117676 kg | 1.010179249 | 1.710864250 | 89.158421% |

Para respetar alternativamente centro 1.73, la base implícita sería 1.12 m:
todos esos CG bajan exactamente 0.04 m; h/V permanecen iguales. A 1300 gal,
la contribución proporcional a altura cambia 2.42% respecto de 1.6503 m.
No se cambia silenciosamente la base conocida. Si las separaciones resultaran
vías, sustituir 2.01 por 1.81 aumenta un LTR agregado, a igual momento/normal,
por factor 2.01/1.81=1.11050. Es sensibilidad condicional, no LTR del camión:
faltan masa/CG secos y no se ha justificado aún el apoyo agregado. No se evalúa
sensibilidad de m_p/A/B/c con valores arbitrarios: falta una estimación central.

## B. Montaje, ejes y calibraciones

| Campo actual | Método con lo disponible | Incertidumbre / condición |
|---|---|---|
| `mounting`=NAN wxyz; `mounting_known=false` | Identificar ejes de placa y rotación fija sensor→vehículo X delante, Y izquierda, Z arriba; comprobar signos mediante giros de la placa y orientación en reposo | Alineación física sin cuantificar; normalizar cuaternión sólo corrige redondeo, no calibra montaje |
| `position_accepted=false`, `rigid_assumed=false` | Aceptar explícitamente la referencia provisional de r y el supuesto de cuerpo común chasis/cisterna | Supuesto experimental; la IMU trasera está en chasis. Diferencia cabina/chasis no mide movimiento cisterna/chasis |
| `raw_to_m`, `sensor_height_m`, `water_offset_m`=NAN | Recuperado: QDY30A-B de rango reportado 2 m, registro0x0004, datos uint16 crudos. Calibrar con profundidades conocidas disponibles: h=k·raw+b; con dos puntos k=(h₂−h₁)/(raw₂−raw₁), repetir para residuales | Rango nominal no da por sí solo cuentas/metro. Separar altura de toma y offset de lectura; no identificables individualmente sólo con una recta si no se conoce ubicación. Sin escala/unidad no hay volumen real |
| `latency_bound_known=false`, valor almacenado 0 µs no utilizable | RTT ACK2 en reloj trasero + edades observadas y planificación; documentar una cota empírica conservadora y sus límites, incluido retardo BNO/driver | No RTT/2 automático ni diferencia entre relojes. Un máximo observado no demuestra un límite absoluto; permanece experimental y no se configura antes del ensayo |
| `water_calibration_known=false` | Registrar escala, offset, ubicación, unidades y residuales, usando sonda y referencias existentes | Una confirmación de operador no crea esa calibración |

## C. Péndulo equivalente: fundamento y lo que puede derivarse

Fuente verificable de estas ecuaciones: `dinamica.c:rhs()` y la derivación de
Newton documentada en `integracion-cabina.md`. No se atribuyen coeficientes a
Zheng/Li/Shin sin recuperar su parametrización completa y aplicabilidad.
Dimensiones del tanque a=W_int/2, b=H_int/2 no son A/B de la trayectoria
q=(0,A sinθ,z0−B cosθ) de la masa equivalente.

Linealizando alrededor de θ=0, sin rotación impuesta y con excitación lateral:

```
I θ̈ + c θ̇ + K θ = −m_p A a_y
I=m_p A²; K=m_p g B
ω₀²=g B/A²; ζ=c/(2 m_p A² ω₀)
m_f=M_agua−m_p
h_O + [m_p(z0−B)+m_f q_f.z]/M_agua = CG_agua
```

| Campo / valor actual, unidades | Obtención o estimación explícita | ¿Se puede calcular ahora? / incertidumbre |
|---|---|---|
| `oscillating_mass_kg`=0 ausente, m_p kg | Modelo modal de fluido: ajustar participación de masa a fuerza/momento del modo, con normalización definida; o ajuste conjunto de respuesta a una excitación conocida | No hay coeficiente modal ni respuesta registrada que determine m_p. 0<m_p<=M sólo es restricción, no estimación del 50% |
| `fixed_water_mass_kg`=NAN, m_f kg | M−m_p, masa total de geometría/calibración | Derivable cuando exista m_p; incertidumbre combina masa total y reparto, no la tolerancia numérica 1e-6 |
| `semi_y_m` / `semi_z_m`=0 ausentes, A/B m | Trayectoria/modalidad equivalente justificada: ω₀²=gB/A² permite B=ω₀²A²/g si se conoce A y frecuencia natural | Una frecuencia sola no separa A y B ni determina m_p. La geometría permite un modelo potencial lineal idealizado sin rompeolas, pero no hay solución modal/normalización recuperada para esta elipse y sus rompeolas; no se copian semiejes del tanque |
| `center_z_m`=0 ausente, z0 m | z0=B+[M(CG−h_O)−m_f q_f.z]/m_p | Derivable con B/reparto/q_f conocidos; no identifica éstos. Error de referencia de 4 cm se propaga aquí |
| `fixed_water_position`=NAN, q_f m | Coherencia de centroide: q_f.z=[M(CG−h_O)−m_p(z0−B)]/m_f si m_f>0; simetría puede justificar x=y=0 en referencia centrada ideal | Sólo z es derivable condicionalmente; no se divide por cero. Simetría es supuesto, no CG medido de camión |
| `damping_nm_s`=0 con estado ausente, c N·m·s | Decaimiento modal: δ=ln(amplitud_n/amplitud_n+1), ζ=δ/sqrt(4π²+δ²), ω₀=ω_d/sqrt(1−ζ²), c=2ζm_pA²ω₀ | Sin señal modal/amplitud identificable no hay c numérico. La sonda existente puede registrar transitorios, pero presión local con oleaje no es automáticamente θ ni amplitud modal. c=0 es hipótesis ideal inviscida posible para un análisis, no amortiguamiento identificado ni sustituto del resto |
| `max_angle_rad`=0 ausente, rad | Elegir dominio pequeño con error de aproximación declarado y comprobar respuesta; sinθ/θ difiere ~θ²/6 (0.1 rad:0.167%, 0.2:0.665%) | Criterio matemático estimable; no prueba validez hidrodinámica a esas amplitudes. No hay amplitud física aceptada para este tanque |
| `max_dt_s` / `max_step_s`=0 ausentes, s | Política según frecuencia modal y flujo real; convergencia RK4, respetar <=max_dt y máximo10000 subpasos | Se pueden escoger por análisis numérico y registros, no necesitan medición física directa; faltan frecuencia/modalidad y aceptación del flujo real. No se amplían por pérdidas |
| `pendulum.status`=MA_MISSING | Pasar a MA_EXPERIMENTAL con parámetros completos y justificados | No exige mediciones directas de todos; hoy faltan fundamentos numéricos, no sólo una bandera |
| Estado inicial θ,θ̇ no establecido | Equilibrio explícito tras reposo asentado; `a` declara 0 rad /0 rad/s | Estimación experimental de estado, no observación directa ni reinicio de calibración |

Con los sensores disponibles se puede registrar excitación y presión para
contrastar un modelo modal candidato. No se supone que una presión local,
ni las dos IMU montadas en cuerpos distintos, identifiquen por sí solas masa,
trayectoria y amortiguamiento del fluido. No se requiere ni propone otra IMU
o compra. La implementación se conserva; esta entrega no añade un solucionador
hidrodinámico ni inventa su normalización para completar la tabla.

**Cálculo más sencillo disponible:** estimación hidrostática de nivel, volumen,
masa y centroide vertical, con geometría provisional y sonda calibrada, líquido
asentado y nivelado. Ya existe y puede habilitarse sin péndulo, r, rigidez,
masa seca ni vía. Antes de calibrar, sólo se prueban cuentas crudas, geometría
por escenarios y diagnóstico cinemático de los IMU. No es un LTR ni el modelo
dinámico completo, y no se muestra semáforo a partir de él.

## D. Comprobaciones físicas y criterios de software

Pendientes: cableado/alimentación existentes, UART/RS485 y CRC, signos de ejes,
calidad SH2, reportes verdaderamente nuevos, pérdidas ESP-NOW, RTT/cota de edad,
calibración de agua, coherencia GPS, heap/DMA y efecto del registro en consola.
Procedimiento y aceptación: [prueba-mesa.md](prueba-mesa.md).

Se conservan políticas actuales: reportes100 ms, skew20 ms, volumen2 s,
alpha min/max0.002/0.05 s y tau0.03 s, nivelación0.03 rad, SH2>=2,
reposo<=0.5/movimiento>=2 km/h y riesgo0.60/0.75 con histéresis0.03.
Son elecciones de ensayo, no incertidumbre de medición ni parámetros físicos
validados. GPS exige RMC/GGA vigentes<=1800 ms, skew de época<=200 ms,
>=4 satélites y 0<HDOP<=4. No se relajan para que el resultado permanezca activo.
