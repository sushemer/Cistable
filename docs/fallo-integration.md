Investigación de integration.exe — 2026-11-03

**La ejecución que abortó es FAIL. No se reclasifica como PASS.** Las ejecuciones
posteriores con código corregido se registran por separado.

1. Ejecutable y comando

El ejecutable de nuestras pruebas es:
`C:\esp\Proyectos\Cistable\pruebas\modelo-a\build\integration.exe`.
Lo compila/ejecuta `python pruebas/modelo-a/run-all.py`. La llamada original
era `subprocess.run([str(build/'integration.exe')], check=True)` con el directorio
de trabajo heredado; durante aquella ejecución era la raíz de Cistable.
No fue una ejecución ni excepción del ESP32.

La captura identifica nombre/dirección, no hash ni ruta del binario. El binario
exacto que falló durante desarrollo ya había sido recompilado en el turno
anterior. No se afirma disponer de su copia original. Antes de investigar se
preservó el ejecutable actual y su SHA256 en:
`pruebas/modelo-a/build/fallo-integration/actual-previo/`.
SHA256 de esa copia actual inicial:
`4906377bfb50bf3a9841b5f60fe449194dda303271bbd2194560b762e567e3a5`.

2. Resultados separados

| Ejecución | stdout | stderr | Código / resultado |
|---|---|---|---|
| Binario actual antes de cambios de esta investigación | mensajes de suites completas | vacío | 0, no reproduce el fallo con el código corregido |
| Reconstrucción del rearme anterior, símbolos -g3/-O0 | vacío | aserción de resultado después del rearme | **3 / 0x00000003, FAIL**, sin timeout |
| Reconstrucción anterior después de reforzar regresión | vacío | aserción inmediata de historia no reiniciada | **3 / 0x00000003, FAIL**, sin timeout |
| Suites actuales recompiladas con símbolos y aserciones habilitadas | suites completas | vacío | 0 en geometría y 0 en integración |
| Actual bajo GDB, breakpoint en abort | salida normal | vacío | proceso inferior termina normalmente, no llega a abort |

Los stdout/stderr completos, comandos, cwd y returncodes están en archivos
separados `.stdout.txt`, `.stderr.txt` y `.json` bajo:

* `pruebas/modelo-a/build/fallo-integration/`: captura inicial, reconstrucción
  de la condición fallida y `traza-reconstruccion.stdout.txt` con bt full.
* `pruebas/modelo-a/build/fallo-integration-regresion/`: reproducción con el
  test reforzado que detecta antes la misma causa.
* `pruebas/modelo-a/build/logs/results.json`: resultados de las suites actuales;
  `run-integration.stdout.txt`, `run-integration.stderr.txt`, y `fixed-rearm.*`
  para la ejecución actual bajo GDB.

Esos artefactos están ignorados por Git; para compartir evidencia adjuntar
los archivos de texto/JSON o ejecutar el investigador con una carpeta nueva:

```
python pruebas/modelo-a/investigate_failure.py --output pruebas/modelo-a/build/nueva-investigacion
```

El investigador se niega a sobrescribir una carpeta existente. La copia de
`control_cabina` con comportamiento anterior y el ejecutable de reconstrucción
se crean sólo dentro de build. No se revierte ni se edita el firmware activo.
Se restituye exclusivamente el comportamiento antiguo de `cc_arm` que retenía
historia de secuencias/alpha. No se introduce una aserción falsa para provocar
el fallo: son las mismas expectativas funcionales de la suite.

3. Clasificación y traza

El código de excepción de la ventana, 0x40000015, está definido por Microsoft
como STATUS_FATAL_APP_EXIT. No es por sí solo un diagnóstico de corrupción
de memoria. En esta investigación GDB demuestra terminación por aserción:

```
#0 msvcrt!abort   C:\WINDOWS\System32\msvcrt.dll
#1 msvcrt!_assert C:\WINDOWS\System32\msvcrt.dll
#2 review_regressions() pruebas/modelo-a/integration_tests.c
#3 main()               pruebas/modelo-a/integration_tests.c
```

La dirección `0x00007FF948BFF9DD` de la captura coincide con `_assert` en la
traza obtenida en este entorno. La reproducción capturada, con el diálogo del
SO inhibido sólo para el proceso de pruebas, devuelve 3. Ese returncode NO se
confunde con el código de excepción de Windows ni con el código del depurador.
GDB puede terminar con 0 por ejecutar correctamente sus comandos mientras el
programa depurado está detenido en abort: eso no convierte la prueba en PASS.

No se observó un acceso inválido o una traza de corrupción de memoria: la
causa reproducida es lógica de rearme -> assert -> abort. Esto no pretende
demostrar ausencia de cualquier defecto de memoria en todo el proyecto.
Tampoco se atribuye a falta de hardware, ESP-IDF o DLL ausente. La ventana es
la forma en que Windows/CRT presentó la terminación fatal de la prueba host.

Fuentes primarias consultadas:
[NTSTATUS de Microsoft](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-erref/596a1078-e883-4972-9bbc-49e60bebca55),
[assert](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/assert-macro-assert-wassert),
[abort](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/abort).

4. Condición concreta

La primera reconstrucción falla en `review_regressions()` con:

```
o.status==MA_SIMULATED && o.mode==CC_MOVING && o.ltr.magnitude<1
```

El stderr capturado apunta a línea 106 de la versión entonces compilada.
Los números de línea posteriores cambiaron al añadir verificaciones; no
se altera esa expectativa. GDB muestra antes del primer paquete posterior
al rearme anterior:

```
s->armed = true
s->last_accel_sequence = 3
f->accel_sequence = 5
s->last_accel_us = 30000
f->accel_us = 50000
```

El límite anterior congeló la dinámica; al rearmar se conservaron esos datos.
La muestra 5 se interpreta entonces como salto desde la 3 y desarma otra vez
el motor. En la muestra 6 la salida sigue `MA_INCOMPLETE / CC_NO_DATA`, con
LTR de salida NaN, en lugar de la reanudación esperada. Es un caso totalmente
sintético, no una prueba física del camión.

5. Corrección y regresión

La corrección funcional **ya estaba presente al comenzar esta investigación**
en `firmware/componentes/control_cabina/control_cabina.c:cc_arm()`:
conserva el nuevo estado inicial explícito, reinicia alpha, tiempos y secuencias
y exige dos observaciones nuevas. No se repite ni se modifica esa corrección
innecesariamente. La comparación controlada verifica causalidad: quitar esos
resets sólo en una copia de build reproduce el fallo, conservarlos pasa.

Se reforzó `integration_tests.c` para comprobar inmediatamente después del
rearme: armado=true, alpha sin inicializar, tiempos/secuencias reiniciados;
primera observación pendiente de alpha sin desarmar; segunda observación
calculable. Con la copia defectuosa ahora falla antes:
`engine.armed && !engine.alpha.initialized` (línea 105 de esta captura).
No se eliminó ningún test ni se relajó ninguna expectativa.

En el código corregido GDB muestra después de `cc_arm`:

```
engine.armed = true
engine.last_accel_sequence = 0
engine.last_accel_us = 0
engine.alpha.initialized = false
[Inferior ... exited normally]
```

Se mejoró `run-all.py` para capturar cada compilación/ejecución en archivos,
registrar códigos, comandos y SHA256 del ejecutable y escribir estado global
PASS sólo al terminar ambas suites con código 0. Usa símbolos de depuración,
`-UNDEBUG`, timeout y supresión del diálogo de fallo del SO local al runner y
sus hijos. Eso no suprime la aserción ni cambia un fallo a éxito; evita que una
ventana deje integration.exe bloqueado y con el archivo ocupado para recompilar.
No cambia registro/configuración global de Windows ni el firmware ESP32.

Archivos modificados en esta investigación: `pruebas/modelo-a/integration_tests.c`,
`pruebas/modelo-a/run-all.py`; nuevo `pruebas/modelo-a/investigate_failure.py`,
este documento y nota de trazabilidad en `docs/integracion-cabina.md`.
No se modificaron los modelos, configuración física ni protocolos.
Las ejecuciones fallidas quedan FAIL; sólo las ejecuciones posteriores
identificadas con el código corregido tienen resultado satisfactorio.
