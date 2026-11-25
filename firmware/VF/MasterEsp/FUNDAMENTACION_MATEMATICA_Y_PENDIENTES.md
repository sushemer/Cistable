# Fundamentación matemática, auditoría de código y pendientes

## Alcance y criterio de lectura

Este documento contrasta la literatura solicitada con el código que existe actualmente en
`MasterEsp` y `SlaveEsp`. Se revisaron los cuatro trabajos principales indicados:
Shin et al. (2021), Zheng et al. (2012), Li et al. (2013) y Dodge (2000). En la carpeta
`Articulos` también hay un PDF suplementario (`cdc_212703_DS1.pdf`); no es uno de los
cuatro trabajos base y no se usa para justificar ecuaciones.

La ausencia de una prueba de carretera impide afirmar validación experimental del vehículo
real. Sí permite afirmar que el firmware tiene una base analítica verificable, siempre que
sus parámetros se calibren y que las pruebas de banco y simulación que se proponen abajo
se ejecuten antes de usarlo como sistema de seguridad.

## 1. Resumen ejecutivo: suficiencia matemática sin pruebas en la cisterna

El LTR de Shin et al. se define directamente por las fuerzas verticales:

$$LTR = \frac{F_{zr}-F_{zl}}{F_{zl}+F_{zr}}, \qquad -1\le LTR\le 1.$$

`LTR = +/-1` representa pérdida de contacto de una rueda, no una probabilidad de
volcadura. Bajo las hipótesis de régimen cuasi-estacionario y aceleración lineal medida,
Shin et al., Ec. (3), obtiene una forma estimada proporcional a
`2 h_CG (a_y + g sin(phi_v))/(T g)`. El firmware conserva la forma física, pero usa la
proyección no lineal `a_y cos(phi) + g sin(phi)`, que es una generalización conservadora
para los ángulos pequeños del sensor.

La justificación previa a ensayos físicos es, por tanto, de **modelo**, no de certificación:

1. El término base procede del equilibrio de momentos alrededor de la huella de los
   neumáticos y el límite `|LTR|=1` tiene interpretación física.
2. `h_CG`, trocha y aceleración se mantienen explícitos. El firmware usa el peor roll
   disponible entre las dos IMU, acota LTR a `[-1,1]` y separa advertencia (`0.35`),
   riesgo longitudinal (`0.45`) y crítico (`0.80`) antes del límite físico.
3. La ventana dinámica de 10--75 % contiene el máximo de fuerza CFD cercano a 60 % que
   reporta Zheng et al. (2012, Sec. 4), y el peor intervalo de respuesta de Li et al.
   (2013, conclusiones) de aproximadamente 40--60 %.
4. La adquisición puede verificarse sin camión: IMU con mesa inclinable, tare, entradas
   sintéticas, integración del péndulo, pérdida de paquetes y latencia ESP-NOW.

Esto no demuestra que los valores genéricos sean los del camión. Tampoco demuestra que la
longitud fija de 0.50/1.00 m represente la cisterna. Esos son pendientes de identificación.

## 2. Matriz comparativa: teoría frente a firmware

| Concepto | Referencia | Estado | Ubicación | Evaluación |
|---|---|---|---|---|
| `LTR=(Fzr-Fzl)/(Fzl+Fzr)` y `|LTR|=1` como despegue | Shin et al. (2021), Sec. 2.1, Ec. (1) | Implementado como estimador | `MasterEsp/main/modules/ltr/ltr_module.c`, `ltr_calculate` | Correcto como índice; no sustituye validación de neumáticos. |
| `a_y^*=a_y cos(phi)+g sin(phi)` | Shin et al. (2021), Sec. 2.3, Ec. (3) | Implementado | `ltr_calculate` | La proyección conserva el término gravitacional; requiere separar peralte para no atribuirlo todo a suspensión. |
| `LTR=2 h a_y^*/(Tg)` | Shin et al. (2021), Ec. (3) | Implementado | `ltr_calculate` | Seguro si `h` y `T` son límites físicos; valores por defecto aún son de prueba. |
| Estimación `sin(phi_r)=(v_x gamma-a_y)/g` | Shin et al. (2021), Sec. 3, Ec. (3) | No implementado | GPS/IMU solo entregan velocidad y giroscopio | Sin peralte, un camino inclinado puede producir sesgo positivo o negativo. |
| Área de segmento elíptico y volumen | Geometría de tanque; no es una ecuación de los cuatro artículos | Implementado | `main.c`, `water_liters` | Válido para sección elíptica ideal; no modela mamparos ni volumen útil real. |
| Péndulo de Trammel elíptico, `a/b=a_p/b_p` | Zheng et al. (2012), Sec. 2.1, Ecs. (1)--(7) | No implementado | `pendulum_module.c` usa un péndulo simple | La longitud fija puede subestimar frecuencia, masa móvil y torque. |
| `omega=sqrt(g b_p/a_p^2)` | Zheng et al. (2012), Sec. 3.1, Ec. (3.13) | Parcial | `pendulum_period_from_inertia` | Existe periodo de péndulo simple, pero no `a_p,b_p` dependientes de llenado. |
| Polinomio `b_p/b` | Zheng et al. (2012), Sec. 4, Ec. (4.6) | No implementado | Pendiente en `pendulum_module` | Debe acotarse al dominio de ajuste de `Delta` y `Lambda`. |
| Polinomio `m_p/m`, `m_f=m-m_p` | Zheng et al. (2012), Sec. 3.2/4, Ecs. (3.18),(4.8) | No implementado | Pendiente | El código usa toda el agua como masa pendular. Esto puede sobreestimar o subestimar la fase, no solo el valor pico. |
| Péndulo no inercial y aceleración del bob | Li et al. (2013), Ecs. (17),(21),(24),(26)--(30) | Simplificado | `pendulum_simulate_vehicle_step` | Incluye excitación lateral y roll absoluto, pero no la geometría Trammel ni todos los términos de traslación/rotación. |
| Momento de roll/yaw del líquido | Li et al. (2013), Ec. (39) | No implementado | `calculate_telemetry` | `slosh_ay` se suma a `a_y`; no se suma un momento dinámico independiente al balance del vehículo. |
| Primer modo antisimétrico | Dodge (2000), Cap. 1, Sec. 1.2 | Parcial | Modelo de un grado de libertad | La elección de un modo es razonable en tiempo real; falta identificar frecuencia y masa modal. |
| Amortiguamiento viscoso/baffles | Dodge (2000), Cap. 2, Ecs. (2.16),(2.22) | Simplificado | `PENDULUM_DAMPING=0.60` | Es un coeficiente dimensional del integrador, no `2 gamma omega_n`; no representa baffles medidos. |
| Modos longitudinales y ondas viajeras | Dodge (2000), Sec. 1.6 | No implementado | Péndulo longitudinal fijo | Puede fallar ante impactos en los extremos si `L/(2a)` es grande. |
| Selección sólido/péndulo con histéresis | Zheng/Li, zona de llenado parcial | Implementado | `SlaveEsp:model_for_level` | Cubre 10--75 % y tiene histéresis solo en el extremo bajo; no reemplaza una parametrización continua. |
| 100 Hz IMU y 50 Hz ESP-NOW | Restricción de muestreo del sistema | Implementado | `SlaveEsp/main.cpp` | 100 Hz es ampliamente superior a 1--5 Hz esperables; 50 Hz debe verificarse frente a latencia y jitter. |
| Configuración remota `0x2D` | Requisito de operación, no una ecuación bibliográfica | Implementado parcialmente | `network_client.c`, `network_protocol.h`, ambos `network_apply_tank_config` | Master carga dimensiones; Slave solo usa `full_tank_value`. |

## 3. Análisis de lo aplicado

### 3.1 LTR, compensación gravitacional y umbrales

En [`ltr_module.c`](C:/Users/gaela/OneDrive/Documentos/Embebidos%202/Proyecto/Version%20Fundamentada/MasterEsp/main/modules/ltr/ltr_module.c)
la función `ltr_calculate` convierte roll a radianes y calcula:

```text
ay_efectiva = ay*cos(roll) + g*sin(roll)
LTR_raw     = 2*h*ay_efectiva/(track*g)
```

La aceleración lineal del BNO085 excluye gravedad, por lo que el comentario y la llamada
del sistema no vuelven a restar `g`. `ltr_max_safe_lateral_accel` despeja el margen hasta
un LTR objetivo y sirve para velocidad en una curva de radio `45 m`; esa velocidad es una
envolvente estática, no una predicción de maniobra completa.

En [`main.c`](C:/Users/gaela/OneDrive/Documentos/Embebidos%202/Proyecto/Version%20Fundamentada/MasterEsp/main/main.c)
las constantes por defecto son: trocha `1.60 m`, batalla `3.80 m`, masa `5443 kg`,
`h_CG=1.20--1.50 m`, advertencia estática `0.35`, advertencia longitudinal `0.45`,
crítico `0.80`, radio `45 m`. La altura se interpola con el porcentaje de volumen
elíptico, pero se aplica una relación lineal de altura de CG que aún debe identificarse.
El roll de mayor magnitud entre IMU frontal y trasera se usa para el riesgo; esto evita
ocultar una discrepancia, aunque también puede ser conservador frente a ruido.

### 3.2 Tare y fusión

`telemetry_tare_sensors` reinicia offsets; el tare automático promedia 30 muestras
frontales y 15 traseras, rechazando movimiento si el módulo frontal reporta gyro mayor o
igual a `0.05 rad/s`. La función `sensor_task` se ejecuta cada 10 ms, recibe hasta ocho
paquetes pendientes, descarta tamaños incorrectos, comprueba timeout de 3 s y fusiona
ambas IMU. El tare reduce error de montaje, pero no separa por sí mismo el peralte de
carretera del roll de suspensión.

### 3.3 Volumen y modo de tanque

`water_liters` calcula el área de un segmento de elipse y la multiplica por la longitud.
Luego se obtiene `water_percent` a partir del volumen, no de la altura directamente. Esto
es una mejora frente a una escala lineal para una sección elíptica ideal.

En el Slave, `model_for_level` selecciona `Pendulum` estrictamente entre 10 % y 75 %,
con histéresis de 5 unidades en el límite inferior. La literatura respalda vigilar esa
zona, pero no prueba que los mismos límites sean óptimos para esta geometría y sus
mamparos.

### 3.4 Integración y muestreo

`pendulum_simulate_vehicle_step` integra con Euler semiimplícito:

```text
theta_ddot = -(m*d/I)*(g*sin(theta+phi) + ay*cos(theta+phi))
             - damping*theta_dot
theta_dot += theta_ddot*dt
theta     += theta_dot*dt
```

El paso se limita a `1--20 ms`. La IMU del Slave pide reportes cada 10 ms (100 Hz) y
envía ESP-NOW cada 20 ms (50 Hz). El documento de Zheng muestra frecuencias de sloshing
del orden de varios rad/s para sus geometrías de ajuste; por ello el muestreo es
suficiente para un modo de aproximadamente `0.3--1 Hz`, pero Nyquist por sí solo no
garantiza estabilidad numérica: debe comprobarse `dt*omega_n` y el jitter real.

## 4. Brechas y plan de implementación

### 4.1 Péndulo de Trammel paramétrico

**Importancia.** Zheng et al. (2012), Ecs. (4.6), (4.8) y (4.9), ajusta por CFD:

```text
bp/b = 1.089 + 0.726D - 0.1379L - 0.953D^2 - 1.216LD
       + 0.05141L^2 - 0.06107D^3 + 0.5739LD^2 + 0.1632L^2D
mp/m = 0.7844 - 1.729D + 0.3351L + 1.156D^2 + 0.7256LD
       - 0.1254L^2 - 0.3219D^3 - 0.9152LD^2 + 0.08043L^2D
ap = bp*L, mf = m-mp
```

`D` es fracción de llenado y `L=a/b` es relación de aspecto. Implementar en
`pendulum_module.c/.h` una función que reciba `D`, `a`, `b`, masa total y devuelva
`bp`, `ap`, `mp`, `mf`, `bf`, validando el dominio y acotando resultados no físicos.
Usar `omega_n=sqrt(g*bp/(ap*ap))` (Zheng, Ec. 3.13) y construir
`I_p=mp*ap*ap` solo como primera aproximación; la inercia exacta debe seguir la
geometría del Trammel.

### 4.2 Acoplamiento de fuerza y torque al LTR

**Importancia.** Li et al. (2013), Ecs. (17), (21), (24), (26)--(30) y (39), acopla
aceleración del bob, ángulo absoluto y momentos de roll/yaw. El código calcula
`slosh_ay` y lo suma al `ay` total, pero no calcula `M_slosh`.

Añadir a `pendulum_state_t` posición, velocidad y aceleración del centro de masa
pendular; después de integrar:

```text
theta_abs = phi + theta
r_y       = ap*sin(theta_abs)
r_z       = -bp*cos(theta_abs)
F_y       = mp*a_y_bob
M_roll    = r_z*F_y - r_y*F_z
M_total   = M_chassis + M_roll
LTR_dyn   = (2*M_total)/(m_total*g*track)
```

Las expresiones exactas deben conservar los términos de Li para la geometría no inercial
(aceleración de traslación, `phi_dot` y `phi_ddot`) y deben integrarse con `dt` medido,
no con un periodo ideal. La interfaz puede devolver `moment_roll_Nm` y `moment_pitch_Nm`
al `calculate_telemetry`; el LTR estático debe seguir disponible como fallback explícito.

### 4.3 Amortiguamiento identificado

**Importancia.** Dodge (2000), Cap. 2, Ecs. (2.16) y (2.22), muestra que el modelo sin
disipación produce picos resonantes no realistas; los baffles perforados modifican la
disipación. `PENDULUM_DAMPING=0.60` no es el término modal normalizado.

Cambiar el modelo a:

```text
theta_ddot + 2*gamma*omega_n*theta_dot + omega_n^2*theta = forcing
```

y guardar `gamma` como parámetro de configuración por tanque. El intervalo `0.05--0.10`
solo debe ser un rango inicial de ensayo, no una constante certificada. Identificarlo con
un escalón de aceleración en banco, con y sin baffles, y comprobar que la energía decae.

### 4.4 Peralte de carretera

Shin et al. (2021), Sec. 3, obtiene:

```text
phi_r = asin((v_x*gamma - a_y)/g)
```

bajo velocidad lateral casi estacionaria. Añadir un estimador con `speed_valid`,
`usable_for_speed`, gyro yaw y filtro de baja frecuencia; rechazarlo en maniobras
transitorias o con velocidad baja. Alternativamente, usar la diferencia entre las dos
IMU para separar roll de cabina, roll de cisterna y torsión. La compensación no debe
eliminar el término gravitacional antes de aplicar LTR: debe separar el peralte y luego
recalcular el ángulo de suspensión.

### 4.5 Modelo longitudinal y ondas viajeras

Dodge (2000), Sec. 1.6, advierte que en un cilindro horizontal largo (`L/(2a) >> 1`)
los modos longitudinales pueden transformarse en ondas viajeras y producir impactos
impulsivos en los extremos. El `1.00 m` actual es un péndulo genérico y no puede
representar ese fenómeno.

Medir `L`, `2a` y nivel real; calcular la razón geométrica y seleccionar: (a) Trammel
longitudinal para `L/(2a)` moderado, o (b) modelo de evento impulsivo con saturación y
contador de confianza para geometría larga. No reutilizar silenciosamente el LTR lateral
como sustituto del impacto de frenado.

### 4.6 Parametrización operativa

`NETWORK_CMD_TANK_CONFIG (0x2D)` ya se valida y llega al Master mediante
`network_client.c`. El Master actualiza trocha, alturas de CG, escala del sensor y
dimensiones; el Slave solo actualiza `full_tank_value` y sus umbrales 10/75 %. Además,
`WATER_LEVEL_OVERRIDE=65` y `WATER_SENSOR_TEST_MODE=1` mantienen el sistema en modo de
prueba.

Antes de operación:

1. Desactivar ambos overrides mediante configuración de compilación o comando explícito.
2. Extender `network_tank_config_payload_t` con `a`, `b`, longitud útil, posición de CG,
   densidad, baffles y parámetros `gamma`; versionar el payload.
3. Aplicar los mismos valores en Master y Slave, confirmar CRC y responder con un estado
   de configuración aceptada.
4. Rechazar configuraciones fuera de dominio, conservar la última válida y marcar
   `NETWORK_FLAG_WATER_SIMULATED` si se usa una simulación.

## Validación recomendada antes de campo

- **Unitarias:** `ltr_calculate` en roll cero, peralte puro y `|LTR|=1`; volumen a 0 y
  100 %; polinomios dentro y fuera de dominio; integración con `dt=1, 10, 20 ms`.
- **Banco IMU:** inclinación conocida, aceleración lateral sintética, tare repetido y
  comparación de IMU frontal/trasera.
- **Red:** medir periodo, jitter, pérdida, timeout de 3 s y aplicación de `0x2D`.
- **Modelo:** barridos de frecuencia y llenado 10--75 %, incluyendo 40--60 %, comparando
  masa modal, frecuencia y momento con una referencia offline.
- **Criterio de liberación:** ningún NaN/Inf, saturación documentada, error de tiempo
  acotado y señal de confianza separada del valor LTR.

## 5. Referencias bibliográficas

1. Shin, D., Woo, S., & Park, M. (2021). *Rollover Index for Rollover Mitigation
   Function of Intelligent Commercial Vehicle's Electronic Stability Control*.
   **Electronics**, 10(21), 2605. Secs. 2.1, 2.3, 3; Ecs. (1)--(3).
2. Zheng, X.-L., Li, X.-S., & Ren, Y.-Y. (2012). *Equivalent Mechanical Model for
   Lateral Liquid Sloshing in Partially Filled Tank Vehicles*. **Mathematical Problems
   in Engineering**, Article 162825. Secs. 2--4; Ecs. (2.7), (3.13), (3.18), (4.6),
   (4.8), (4.9).
3. Li, X.-S., Zheng, X.-L., Ren, Y.-Y., Wang, Y.-N., & Cheng, Z.-Q. (2013). *Study on
   Driving Stability of Tank Trucks Based on Equivalent Trammel Pendulum for Liquid
   Sloshing*. **Discrete Dynamics in Nature and Society**, Article 659873. Secs. 2--4;
   Ecs. (17), (21), (24), (26)--(30), (39).
4. Dodge, F. T. (2000). *The New “Dynamic Behavior of Liquids in Moving Containers”*.
   Southwest Research Institute, actualización de NASA SP-106. Caps. 1--3, en particular
   Sec. 1.2, Sec. 1.6 y Ecs. (2.16), (2.22), (3.15).
5. Referencias históricas citadas por los trabajos: Abramson (1966), *The Dynamic
   Behavior of Liquids in Moving Containers*, NASA SP-106; Salem, *Rollover Stability of
   Partially Filled Heavy-Duty Elliptical Tankers Using Trammel Pendulums*; y trabajos
   CFD/FLUENT citados por Zheng et al.

