# Modelo A provisional — entrega modular

No es un modelo validado de alerta de volcadura. No produce estado verde ni
umbrales de riesgo. No se modificaron pantalla, drivers, paquetes ESP-NOW,
pines, particiones ni firmware funcional. No hubo commit, push, merge ni flasheo.

## Fuentes verificadas (referencias locales de lectura)

Se creó `Jesus/modelo-a-ltr` desde la rama **local** `Jesus/pantalla-espnow`.
Se conservaron los cambios preexistentes de pantalla. No se hizo fetch;
la inspección corresponde a estos snapshots, no se afirma actualidad remota:

| Fuente | Referencia y archivo |
|---|---|
| LTR | `origin/Twig` (`43b8de042226646cc1437fd9448c62ada610e32e`), `firmware/base/main/main.c` y `main/CMakeLists.txt` |
| Agua real | misma referencia, `sensores/agua/RS485-Agua/main/main.c` y `main/CMakeLists.txt` |
| IMU SPI | `origin/Pruebas-Gael` (`2bb43a8006bdd84b54c624ee022a46c042426a15`), `firmware/esp32-trasero/ESP-CISTERNA/main/main.cpp` y `main/CMakeLists.txt` |
| Prueba de dos IMU SPI | misma referencia, `firmware/esp32-trasero/TEST esp Cisterna en C++/main/main.cpp` |
| Prueba de dos IMU I2C | misma referencia, `firmware/esp32-trasero/TEST esp cisterna en C/main/main.c` |
| Semántica de reportes | misma referencia, `firmware/esp32-trasero/ESP-CISTERNA/components/esp32_BNO08x/include/report/BNO08xRptLinearAcceleration.hpp` y `source/report/BNO08xRptRVGeneric.cpp` |

Los nombres locales disponibles son referencias `origin/Twig` y
`origin/Pruebas-Gael`; no se crearon ni editaron ramas de compañeros.
El árbol `firmware/base` contiene un ejemplo LTR, no un driver de agua.
Su CMake registra solamente `main.c`; usa FreeRTOS y logs de ESP-IDF.

### Auditoría LTR

El baseline usa tres IMU simuladas (incluida cabina), presión simulada de
11 kPa, dimensiones 5 x 1.8 x 2.2 m, altura proporcional al volumen,
masa vacía 8000 kg, CG constantes 1.2/1.8 m, desplazamiento heurístico
0.45 * 4f(1-f) * ay_eff/g, valor absoluto y recorte a 1.
Clasifica con 0.60/0.75 y anuncia «vuelco inminente». Nada de eso se trasladó
al módulo nuevo. El original permanece intacto en su referencia de lectura.

La API `ma_truck_ltr` devuelve `MA_INCOMPLETE` mientras falten masa, CG,
contabilidad de componentes y una vía agregada justificada. Aun con esos
datos devuelve `MA_UNAVAILABLE`: no se conserva un cálculo heurístico.
Los campos firmado y magnitud quedan NaN; cualquier implementación futura
deberá conservar signo, magnitud separada y valor sin recorte, documentando
el dominio antes de pérdida de contacto. No se elige ni promedia 2.01/1.81 m.

### Adquisición IMU encontrada

Hay variantes incompatibles; la integración final no es inequívoca:

* `ESP-CISTERNA`: BNO085 por SPI2 a 3 MHz; dos CS/HINT/RST, pero
  `ENABLE_IMU2=0`. Game Rotation Vector y Linear Acceleration a 10000 us
  solicitados (100 Hz, no frecuencia física verificada). Servicio cada 5 ms.
  `get_euler(true)` entrega grados; aceleración lineal Y en m/s², sin gravedad.
  No se habilita reporte de giroscopio aunque el sensor tiene esa capacidad.
  La librería ofrece cuaterniones; la aplicación transmite solamente roll,
  pitch y ay. Timestamp local `esp_timer_get_time()` actualizado si llega
  cualquiera de los dos reportes; eso no garantiza frescura de ambos.
  Flag de validez antes de 500 ms. No exporta calidad de fusión/calibración.
* `TEST esp Cisterna en C++`: dos instancias SPI, mismos reportes y paquete
  packed de 29 bytes: siete float (roll1,pitch1,ay1,roll2,pitch2,ay2,agua) y
  un byte de flags (bits 0/1 IMU, bit 2 RS485). Envío cada 50 ms en variante
  ESP-CISTERNA, canal 1, sin cifrado. No hay timestamps en esos 29 bytes.
* `TEST esp cisterna en C`: dos BNO085 I2C 0x4A/0x4B, 400 kHz, driver
  `rinku404/bno085` 1.0.2 (manifest `main/idf_component.yml`, IDF >=4.1)
  y copia de símbolos `bno085_2`/SH2 incluida en su CMake. Rotation Vector y Linear
  Acceleration a 100 Hz solicitados. Convierte cuaternión a roll/pitch;
  exporta grados, ay m/s² y LTR por sensor recortado. Paquete C natural:
  sequence y timestamp_ms (reloj del emisor), flags/reservas, ocho floats,
  uint16 agua/reserva. Envío cada 100 ms; frescura IMU 300 ms con timestamps
  por reporte. No transmite giroscopio, cuaternión completo ni calidad.
  La orden `t` resta la pendiente actual: no se reutiliza ese tare.

No hay montaje, orientación ni correspondencia física IMU1/izquierda
confirmados en esas aplicaciones. La nueva API exige identificar izquierda
y derecha explícitamente y mantener sus flujos separados. Cada reporte tiene
timestamp, dominio de reloj, estado y calidad (-1 desconocida). Giroscopio
en rad/s, aceleraciones en m/s², cuaterniones w,x,y,z. La orientación del
reporte conserva su marco de referencia original; se debe documentar el
marco mundo del BNO al integrarlo. `sensor_to_vehicle` es una rotación unitaria
medida/calibrada del sensor al vehículo: X delante, Y izquierda, Z arriba.
No se aplica tare ni corrección por brazo de palanca. Las posiciones siguen
pendientes. Comparación de aceleración lineal exige alineación, mismo reloj,
frescura y skew definidos por el integrador; devuelve diferencia diagnóstica,
no un indicador de oleaje, fallo o carga de rueda. No promedia ni usa fallback.

### Agua: evidencia y límites

Twig sí contiene un lector real UART/Modbus RTU: esclavo 1, función 03,
registro 0x0004, un registro uint16, datos high byte primero y CRC16
Modbus low byte primero. UART2, TX17/RX18, 9600 8N1, sin flujo, modo UART
normal; no configura DE/RE del MAX3485. Lee con espera de 300 ms y duerme
1000 ms después (periodo real incluye la lectura). Comprueba CRC, dirección
y función con longitud >=7; no valida byte count estrictamente ni distingue
excepción Modbus. Retorna INVALID_ARG, FAIL o TIMEOUT y registra errores.
No conserva timestamp ni convierte a magnitud física. **No confirma si el
registro es presión, profundidad u otra magnitud, ni escala ni signo físico.**

Pruebas-Gael consulta 0x0000 en lugar de 0x0004; no resuelve esa discrepancia.
La variante SPI espera 8 bytes aunque una respuesta estándar de un registro
son 7: requiere revisión antes de integrar. La variante C espera 7 y controla
DE, consulta cada 500 ms y marca agua antigua después de 620 ms. No se
copiaron esos drivers ni se inventaron registros o calibraciones.

`ma_estimate_water` rechaza registro crudo como configuración incompleta.
Acepta únicamente profundidad explícita en metros o presión manométrica
confirmada en kPa, con calibración explícita, altura sobre fondo y offset
aditivo en metros. Para presión: h=P*1000/(rho*g)+altura_sensor+offset.
No se conoce aún la altura/offset reales. El llamador debe marcar esos datos
pendientes. Política conservadora: actualizar sólo detenido, nivelado y
asentado, con muestra vigente en su mismo reloj. Se requiere definir y
verificar físicamente esos predicados y el máximo de edad; no se inventan
umbrales. Durante maniobras no se genera volumen nuevo ni se convierte toda
fluctuación de presión en cambio de masa. Si se conserva una última estimación,
su edad y estado deben exponerse; esta API devuelve NaN y motivo al bloquear.
Una entrada `MA_SIMULATED` conserva esa marca hasta el resultado.

## Implementación y uso

* `firmware/componentes/modelo_a/include/modelo_a.h`: API pura C, estados,
  configuración trazable, dos flujos IMU, agua, LTR bloqueado y péndulo.
* `firmware/componentes/modelo_a/modelo_a.c`: geometría, inversión por
  bisección, centroide, controles de dominio, frescura y comparación alineada.
* `firmware/componentes/modelo_a/CMakeLists.txt`: componente ESP-IDF sin drivers.
* `pruebas/modelo-a/host_tests.c` y `run-host.ps1`: pruebas host con GCC.
* `pruebas/modelo-a/esp-idf/{CMakeLists.txt,main/CMakeLists.txt,main/main.c}`:
  ejemplo aislado que registra escenario simulado y estados de indisponibilidad.

La tabla de parámetros distingue medidos, informados, supuestos, derivados y
pendientes; ninguno se marca confirmado. Los desconocidos son NaN y tienen
origen pendiente. La configuración geométrica provisional y la función del
escenario de 1300 gal US están separadas. La estimación de cisterna de 924 kg
no se incorpora como masa medida al LTR.

La integral de tiras 2a sqrt(1-z²/b²) da el área indicada en la solicitud;
la integral de z por esa área da el primer momento -2ab²(1-u²)^(3/2)/3.
La implementación usa la forma equivalente theta-sin(theta)cos(theta), con
serie para theta<0.01 para evitar cancelación al vaciar. No acepta alturas o
volúmenes físicamente fuera del intervalo, incluso por poco; las tolerancias
de consistencia geométrica (32 epsilon) y norma de cuaternión (1e-6) son
numéricas, no calibraciones. Agua vacía tiene volumen/masa cero y CG ausente.

Sección constante, camión nivelado, tapas planas, W/H/L interiores restando
2t. La forma de extremos reales puede cambiar capacidad y CG. No se descuenta
material de rompeolas sin medirlo. Porcentaje de altura y volumen son campos
distintos. Sólo se calcula CG estático vertical del agua.

## Supuestos, discrepancias y pendientes

Exterior 2.20 x 1.24 x 3.04 m, t=0.0047625 m; base 1.16 m. Base más
media altura exterior da 1.78 m frente a 1.73 anotado; se conserva base 1.16
para la fórmula provisional. Batalla medida 3.92 frente a 5.30 m de placa.
No se usa ancho ambiguo 1.05 m. Separaciones 2.01/1.81 aún sin referencia
confirmada; no son automáticamente vías efectivas. Modificaciones sin confirmar.

Capacidad reportada 1200–1500 gal sin nominal exacto. Para simulación se
asume galón US (3.785411784 L). 1300 gal es un volumen inicial, nunca 100%.
Agua 1000 kg/m³ y g=9.81 m/s². Diésel 209 L a 0.832 kg/L -> 173.888 kg,
sin añadirlo dos veces si alguna masa medida ya lo incluye. GVWR 15876 kg
es capacidad original de placa, no masa vacía ni garantía del vehículo modificado.

924 kg sólo es estimación preliminar: acero 7850 kg/m³, espesor uniforme
3/16, pared elíptica, dos extremos planos y dos rompeolas al 80%, sin restar
orificios. Excluye plataforma, soportes, bomba, tuberías y accesorios.
Los ~160 kg de extremos son dos paredes completas; no sumar tapas de acceso
sin descontar huecos. Material y espesores no confirmados.

Faltan masas y CG de chasis/cabina, plataforma/soportes/accesorios,
ocupantes/equipo y combustible; contabilidad completa y modelo por ejes.
Faltan posiciones, orientación y sincronización IMU, calibración de agua,
forma real del tanque y geometría de rompeolas: posiciones, orificios,
espesores. Son dos transversales y tres zonas comunicadas al menos arriba;
no tres compartimentos herméticos ni tres péndulos independientes.
Montaje propuesto bajo tanque, lados simétricos, cerca de 1.52 m longitudinal;
medir y comprobar movimiento tanque/plataforma. No inferir que las IMU están
en el CG ni que sus aceleraciones deban ser iguales.

La interfaz de péndulo separa parámetros, estado, dt, entradas, fuerzas y
momento de salida. Siempre devuelve no disponible con NaN. Faltan masa
oscilante, longitud y amortiguamiento identificados. Su futura combinación
requiere derivación de fuerzas/momentos para evitar doble conteo.

## Integración pendiente

Listos para integrar como componente: geometría pura, contratos internos,
estados y políticas explícitas. No son contratos de bytes de radio.
Antes de conectar emisor/receptor hay que elegir entre las variantes de
paquetes existentes, verificar correspondencia física de sensores y ampliar
ambos extremos coordinadamente con versión, unidades, reportes completos,
timestamps por reporte, dominio/reinicio del reloj, secuencia y calidad.
No comparar reloj de cisterna con cabina sin sincronización demostrada.
El receptor debe conservar edad de adquisición además de recepción. No
reconstruir datos ausentes como ceros ni interpretar flags globales como
calidad de cada reporte. Pantalla debe mostrar ausencia/invalidez cuando se
integre, sin mapear esos estados a verde.

## Verificación

Ejecutar desde raíz con GCC disponible:

```powershell
New-Item -ItemType Directory -Force pruebas/modelo-a/build
gcc -std=c11 -Wall -Wextra -Werror -pedantic -Ifirmware/componentes/modelo_a/include firmware/componentes/modelo_a/modelo_a.c pruebas/modelo-a/host_tests.c -lm -o pruebas/modelo-a/build/modelo_a_tests.exe
./pruebas/modelo-a/build/modelo_a_tests.exe
```

También existe `pruebas/modelo-a/run-host.ps1`; la política local impidió
ejecutarlo directamente, por eso se ejecutaron los mismos comandos arriba.
Pruebas pasadas: vacío/lleno, media y cuarta altura, monotonía y retorno en
1001 puntos, centroide contenido, baja profundidad, entradas no finitas y
fuera de rango, configuración incompleta, LTR/péndulo no disponibles,
frescura, dominios, política hidrostática y separación de dos IMU.

Resultado calculado sin ajustar a referencias: capacidad 6415.221889442 L;
1300 gal US=4921.035319200 L; h=0.881962207 m;
CG=1.655453502 m sobre suelo; llenado por volumen 76.708731%.
No son mediciones ni capacidad certificada.

Para compilar el ejemplo: activar ESP-IDF 6.1, ir a
`pruebas/modelo-a/esp-idf` y ejecutar `idf.py build`.
La pantalla existente tiene target real `esp32`; el ejemplo usa el default
ESP-IDF `esp32`, sin cambiar el target existente. El hardware previsto es
ESP32-S3 N16R8 y deberá verificarse su configuración al integrar.
No se realizaron pruebas de hardware.

Compilación ejecutada: **ESP-IDF 6.1, target esp32, exit 0**, imagen
`pruebas/modelo-a/esp-idf/build/modelo_a_experimental.bin`, 0x25260 bytes;
85% de partición de aplicación libre. El primer intento restringido falló
con WinError 5 al crear subproceso; el reintento autorizado terminó.
El SDK emitió avisos de Kconfig, dependencias internas WiFi y generación de
gdbinit (ESP_ROM_ELF_DIR no definido); no impidieron generar ELF/binario.
Esto verifica compilación, no ejecución ni hardware S3.

Estado final: rama `Jesus/modelo-a-ltr`, mismo HEAD/base local
`39b9c5a38a8a57d2e2fc2defdbbd520ace015bc2`. Cambios nuevos sin stage:
este documento, `firmware/componentes/modelo_a/` y `pruebas/modelo-a/`.
Permanecen los cuatro archivos modificados preexistentes en
`pruebas/02-Dispositivos/Pantalla/Pantalla_Interfaz/main/`:
`CMakeLists.txt`, `main.c`, `interfaz/interfaz.c`, `interfaz/interfaz.h`,
y el directorio no rastreado `interfaz/recursos/`. `git diff --check`
señala whitespace preexistente en `interfaz/interfaz.c:237`, dejado intacto;
no se encontró whitespace final en los archivos nuevos.

Mensaje de commit propuesto (sin ejecutarlo):
`feat(modelo-a): add provisional geometry, two-IMU contracts and validity states`
