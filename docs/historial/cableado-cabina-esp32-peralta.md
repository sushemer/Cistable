> **Historial ESP32 clásico**: no usar este cableado/build para S3. [Guía vigente](../README-prueba-vehiculo.md).

﻿# Cabina Peralta: pantalla independiente, ESP32 DevKit V1

Asignación documental vigente, **pendiente de confirmar en el cableado físico**.
Targets conservados: cabina ESP32 clásico/DevKit V1, flash4 MB y sin PSRAM;
trasero de Chris ESP32-S3, flash16 MB. No se han flasheado placas.
Los números son **GPIO**, no posiciones numeradas del conector.

## Contraste con originales

Referencia local consultada: `origin/TACOS`, commit
`971ba82db8c6f342301b98b517dc52c17c01f32c`; no se cambió esa rama.

| Fuente en ese commit | Resultado de la consulta |
|---|---|
| `pruebas/02-Dispositivos/GPS6MV2/prueba_04/README.md`, Conexión | GPS TX→16; BNO MOSI23/MISO19/SCK18/CS5/INT4/RST22; coincide con tabla del usuario |
| `prueba_04/firmware/main/imu_device.h` en el mismo árbol | Confirma los seis GPIO anteriores; usa **SPI2_HOST**, reloj3 MHz, reportes solicitados5 Hz en esa prueba antigua |
| `prueba_04/firmware/main/CMakeLists.txt` | Reutiliza el driver GPS de `prueba_02` |
| `pruebas/02-Dispositivos/GPS6MV2/prueba_02/firmware/main/gps_config.h` | UART2, RX16/TX17, 9600 8N1; confirma RX16; TX17 es opcional para recepción NMEA |
| `firmware/esp32-master/PRUEBA MASTER/main/modules/imu/imu_module.cpp` | **Discrepa**: SPI3, MOSI11/MISO13/SCK12/CS14/INT6/RST5. GPIO6/11 son inadecuados para flash del DevKit clásico |
| `PRUEBA MASTER/main/modules/gps/driver/gps_config.h` | **Discrepa**: RX18/TX17; RX18 colisionaría con SCK del IMU documentado |

Por tanto, no se copian ciegamente los GPIO del master posterior. Se conserva
el cableado documentado de `prueba_04`, solicitado por el usuario. En nuestra
integración la IMU utiliza **SPI3/VSPI** (ya era el controlador de su módulo),
y la pantalla **SPI2/HSPI**. El cambio de controlador respecto a la prueba04
es software: no exige mover sus seis cables. GPIO matrix/IO mux permiten la
asignación; SCK18/MOSI23/MISO19 son la disposición VSPI habitual del ESP32.
Se mantienen los objetivos de reportes100/50 Hz de la integración actual,
no los5 Hz de aquella prueba antigua; tasas efectivas siguen por medir.

## Tabla final de cabina

«Conservar» significa conservar la referencia suministrada, **no conexión
físicamente certificada**. La pantalla anterior documentada usaba
SCK18/MOSI19/DC21/RST22/CS4 y entra en conflicto con el BNO de Peralta.

| Dispositivo / señal | GPIO final / bus | Acción respecto al montaje documentado |
|---|---|---|
| BNO085 DI/MOSI | 23, SPI3/VSPI | Conservar Peralta; no conectar botón aquí |
| BNO085 DO/MISO | 19, SPI3/VSPI | Conservar Peralta; quitar MOSI/SDO de pantalla de este GPIO |
| BNO085 SCK | 18, SPI3/VSPI, 3 MHz | Conservar Peralta; retirar SCK de pantalla de18 |
| BNO085 CS | 5, selección SPI3 | Conservar; excepción GPIO de strapping, comprobar arranque |
| BNO085 HINT/INT | 4, entrada de interrupción | Conservar; retirar CS de pantalla de4 |
| BNO085 RST | 22, salida | Conservar; retirar RST de pantalla de22 |
| BNO085 PS0/WAKE | 3.3 V, sin GPIO asignado | Conservar selección SPI documentada |
| BNO085 PS1 | 3.3 V, sin GPIO asignado | Conservar selección SPI documentada |
| GPS TX → RX ESP | 16, UART2 RX, 9600 8N1 | Conservar Peralta; **no RX18** del master posterior |
| ESP TX → GPS RX | 17, UART2 TX reservado | Conservar si ya existe; opcional, no hace falta añadirlo para recibir NMEA |
| ST7796S SCK | **14**, SPI2/HSPI, reloj20 MHz | Cambiar desde18 |
| ST7796S SDI/MOSI | **13**, SPI2/HSPI | Cambiar desde19 |
| ST7796S DC/RS | **21**, salida | Conservar conexión anterior de pantalla |
| ST7796S RST | **27**, salida | Cambiar desde22 |
| ST7796S CS | **25**, selección SPI2 por GPIO matrix | Cambiar desde4 |
| ST7796S SDO/MISO | **Sin conectar**, bus configurado MISO=-1 | Desconectar si estaba conectado; esta interfaz es sólo escritura |
| Touch / microSD de pantalla | Sin conectar, deshabilitados | No reutilizar CS15 del ejemplo; no hay asignación para estos dispositivos |
| Pantalla VCC / LED-BL | Sin GPIO de control | Conservar alimentación compatible ya utilizada; verificar módulo, no asumir tensión a partir del pinout SPI |
| Botón opcional, contacto a GND | **32**, entrada con pull-up interno | Si existe, mover desde23; dejarlo desconectado hasta cambiarlo. Deshabilitado por defecto |
| Consola USB-UART TX/RX | 1 / 3, UART0 nativa | Conservar; exclusivos de monitor/captura, nunca pantalla/GPS |
| GND de ESP, IMU, GPS y pantalla | GND común | Conservar y comprobar continuidad |
| Alimentación IMU/GPS | Compatible con cada módulo; lógica ESP32 3.3 V | Conservar suministro existente; comprobar etiqueta y nivel lógico, sin asumir VCC GPS |

Los GPIO26 y33 quedan libres con esta asignación. El botón opcional reserva32.
Deshabilitar un driver **no desconecta sus cables**: el validador reserva el mapa
completo incluso en demo, impidiendo reutilizar para LCD un GPIO ya cableado.
Si se usan cables de pantalla anteriores, hay **cuatro cambios** obligatorios:
SCK18→14, MOSI19→13, RST22→27 y CS4→25. DC21 se conserva.

## Restricciones comprobadas en software

- SPI0/SPI1/flash: no se usan GPIO6..11. No se emplean GPIO0/2/12/15 de arranque
  para la pantalla, ni GPIO1/3 de consola, ni GPIO20/24/28..31 inexistentes/no
  expuestos en este perfil de DevKit. GPIO34..39 son sólo entrada y no tienen
  pull-up interno: no se proponen para LCD ni botón.
- GPIO5 de CS BNO se conserva por prioridad del montaje existente. Es un pin
  de strapping: verificar que CS esté inactivo y que se arranque/reinicie con
  el BNO conectado. La comprobación de software **no mide su nivel al reset**.
  GPIO4 es INT en este ESP32; no se debe interpretar con el mapa del S3.
- GPIO16/17 se reservan para GPS en **WROOM/DevKit sin PSRAM**. Comprobar el
  módulo físico: WROVER/variantes con PSRAM pueden ocuparlos. El firmware
  rechaza `CONFIG_SPIRAM=y` para esta asignación; no detecta por sí solo un
  módulo distinto cuya PSRAM se haya deshabilitado por software.
- UART0 queda en GPIO1/3. El guard rechaza UART de consola distinta de0 y
  GPIO UART0 custom distintos de nativos/-1. Captura mantiene921600; normal115200.
- GPIO13/14 se usan para LCD, por lo que no están disponibles simultáneamente
  para JTAG externo en esos pines. No hay conflicto con consola USB-UART normal.
- Pantalla y BNO inicializan controladores SPI distintos. Touch no asignado:
  habilitar la opción heredada de touch provoca error de compilación explícito.

Fundamento primario: [datasheet ESP32-WROOM-32, strapping](https://documentation.espressif.com/esp32-wroom-32_datasheet_en.html),
[restricciones GPIO de ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/v5.5.6/esp32/api-reference/peripherals/gpio.html)
y [SPI master, selección de bus y GPIO matrix](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/spi_master.html).
La disponibilidad final de terminales y alimentación se comprueba sobre sus
placas reales, sin sustituirla por una fotografía ni por el nombre comercial.

## Periféricos habilitados por perfil

| Perfil cabina / build | Pantalla | GPS UART2 | BNO cabina SPI3 | Radio CST2 | Botón32 | Captura completa / UART0 |
|---|---|---|---|---|---|---|
| Real normal, `build` | Sí | Sí | Sí, sólo diagnóstico | Sí | No | No /115200 |
| Real captura100, `build/capture100` | Sí | Sí | Sí, objetivo100 Hz | Sí | No | Sí /921600 |
| Real captura50, `build/capture50` | Sí | Sí | Sí, objetivo50 Hz | Sí | No | Sí /921600 |
| Pipa virtual100, `build/virtual100` | Sí | Sí | Sí, sólo diagnóstico, objetivo100 Hz | Sí | No | Sí /921600 |
| Pipa virtual50, `build/virtual50` | Sí | Sí | Sí, sólo diagnóstico, objetivo50 Hz | Sí | No | Sí /921600 |
| Demo sintética continua/caducidad, selección `CI_DEMO` | Sí | **No se inicia** | **No se inicia** | **No se inicia** | No por defecto | Según configuración de captura seleccionada |

El botón puede habilitarse explícitamente con `CI_ENABLE_BUTTON`; su GPIO es
editable mediante `CI_BUTTON_GPIO`, inicial32. La demo omite los drivers reales
incluso si un sdkconfig heredado conserva sus flags activos. El estado efectivo
se imprime y captura en `WIRING_ENABLED`, además de `WIRING` con el mapa.
GPS/IMU habilitados en firmware no certifican ni cableado ni datos válidos:
la calidad, frescura y disponibilidad se comprueban en ejecución.

La IMU de cabina sigue siendo diagnóstico: no se promedia con la trasera ni
sustituye la excitación del modelo. Se conservan el modelo real bloqueado,
las estimaciones/hipótesis de pipa virtual, los montajes independientes,
las condiciones de rearme y el cálculo a partir del **IMU trasero**.

## Trasero de Chris: sin cambios

| Señal | GPIO / bus ESP32-S3 | Acción |
|---|---|---|
| BNO MOSI / MISO / SCK | 18 /16 /15, SPI2 | Conservar |
| BNO CS / HINT / RST | 10 /11 /12 | Conservar |
| Transceptor RS485 DI ← ESP TX | 17, UART1 | Conservar |
| Transceptor RS485 RO → ESP RX | 13, UART1 | Conservar |
| Transceptor DE/RE | 14, UART1 RTS half-duplex | Conservar |
| Sensor agua A/B | A/B del transceptor, no GPIO directo | Conservar |
| GND y alimentación | Montaje existente de Chris | Conservar; no aplicar restricciones de flash del ESP32 clásico al mapa S3 |

Los tres perfiles traseros existentes (normal, capture100 y capture50)
conservan BNO/RS485/radio activos y su configuración anterior. La captura
completa sólo está activa en capture100/50. No se editan ni recompilan como
parte de este cambio, porque sus fuentes/configuraciones no han cambiado;
se cotejan hashes con la evidencia previa.

## Configurar, compilar y comprobar en mesa

En cada proyecto de **cabina** usar el mismo build/sdkconfig que después se
flasheará. Pines LCD, IMU, GPS y botón: `Cistable integration (experimental)`;
LCD bajo `Pantalla ST7796S - SPI2 independiente de IMU SPI3`. Toda modificación
requiere recompilar y flashear. Peralta se aplica a normal y a las cuatro
configuraciones de captura existentes; no se modifican masas, llenado o ejes.

```powershell
cd C:/esp/Proyectos/Cistable/firmware/integracion/cabina
idf.py build
# Ejemplo virtual100; sustituir por virtual50/capture100/capture50 según caso:
idf.py -B build/virtual100 -D SDKCONFIG=C:/esp/Proyectos/Cistable/firmware/integracion/cabina/build/virtual100/sdkconfig menuconfig
idf.py -B build/virtual100 -D SDKCONFIG=C:/esp/Proyectos/Cistable/firmware/integracion/cabina/build/virtual100/sdkconfig build
# Sólo ustedes, después de confirmar GPIO/módulo/puerto:
idf.py -B build/virtual100 -D SDKCONFIG=C:/esp/Proyectos/Cistable/firmware/integracion/cabina/build/virtual100/sdkconfig -p COM5 flash
```

1. Sin alimentación, comprobar cada cable de la tabla. Retirar LCD de18/19/22/4
   antes de alimentar el montaje integrado; si hay botón en23, moverlo a32.
   No conectar SDO/touch/SD de pantalla. Confirmar placa WROOM y alimentación.
2. Encender/reiniciar con IMU y GPS conectados: arranque estable, sin bucle de
   reset ni `WIRING_INVALID`. El error de mapa detiene el arranque **antes** de
   inicializar pantalla, botón, GPS o BNO. No se prueba conectando pines al azar.
3. Revisar pantalla y `WIRING`/`WIRING_ENABLED`: buses y GPIO de esta tabla,
   LCD activo, GPS=1 y CABIN_IMU_DIAGNOSTIC=1 para perfiles reales/virtuales.
   La demo debe indicar que omite entradas reales.
4. Revisar `IMU_FRONT`, `CABIN` y GPS: inicio BNO sin conflicto de bus;
   aceleración/gyro/cuaternión con tiempos nuevos; GPS NMEA/fix válido cuando
   haya recepción. Desconectar/reconectar uno no debe convertirlo en dato válido
   ficticio. Conservar el diagnóstico de memoria/DMA y observar resets/errores.
5. Verificar CST2 trasero y hacer captura con los procedimientos de
   [captura física](../captura-prueba-fisica.md) y [pipa virtual](../pipa-virtual.md).
   Pantalla4 Hz, resumen1 Hz; objetivos de reportes/envíos100 o50, no garantía
   de tasas efectivas. Cerrar el monitor serie antes de abrir el capturador.

La compilación y el guard de GPIO verifican software. Quedan pendientes en
hardware: continuidad/correspondencia de cables, alimentación, nivel de CS5
al reset, exposición de GPIO del módulo, arranque simultáneo de ambos SPI,
memoria dinámica y estabilidad de adquisición/pantalla/captura. No se hicieron
pruebas físicas ni se declararon PASS de hardware.

Evidencias de esta actualización: `pruebas/modelo-a/build/cableado-peralta/`.
Las evidencias anteriores en `build/virtual-final` se conservan inmutables;
representan el cableado anterior, no los nuevos binarios de cabina.
