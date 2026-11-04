# Hardware vigente: ambos nodos ESP32-S3, N16R8 esperado

Sustituye la asignación ESP32 clásico/DevKit V1 de cabina. Peralta desconectó
su circuito: el cableado de cabina es nuevo. Chris conserva su IMU ya soldado.
La publicación identifica **ESP32-S3-WROOM-1-N16R8** para ambas placas; se
prepara esa configuración, pero **inscripción, memorias y placa de desarrollo
no han sido verificadas físicamente**. No se deduce una revisión de PCB de la
publicación. No se hicieron flasheos ni pruebas de hardware.

## Fuente y límites de lo conocido

La tabla1-1 del [datasheet primario del módulo, v1.8](https://documentation.espressif.com/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf)
asigna a N16R8 Flash16 MB Quad y PSRAM8 MB Octal. La sección de pines restringe
35/36/37 para las variantes Octal. Se excluyen también las señales internas
de memoria26..34, GPIO22..25 inexistentes en S3, USB19/20 y strapping0/3/45/46.
La [guía oficial de DevKitC-1](https://documentation.espressif.com/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html)
muestra diferencias de LED38/48 entre revisiones; se evitan ambos como cautela,
sin afirmar que las placas compradas sean esa DevKit/revisión. Verificar sus
conectores, puentes USB-UART, reguladores, LED y GPIO realmente expuestos.

GPIO43/44 quedan exclusivos de UART0, GPIO19/20 exclusivos de USB nativo.
La comprobación usa el mapa completo aunque un driver esté deshabilitado:
apagar el driver no desconecta un cable. También comprueba controladores
SPI distintos (2 y3), válidos para periféricos generales, no SPI0/1 de memoria.
El error de mapa detiene el arranque antes de inicializar pantalla o sensores.

## Tabla única final de conexiones

Números **GPIO**, no números físicos de patilla/conector. Todas las conexiones
nuevas de cabina son propuestas por comprobar. Buses con el mismo nombre en
placas distintas son independientes; no conectar señales entre ESP por coincidencia
de número. GND común se refiere a cada ESP y sus periféricos conectados.

| Placa / dispositivo | Señal | GPIO / controlador | Acción |
|---|---|---|---|
| Cabina / ST7796S | SDI/MOSI | **11 / SPI2** | Nueva conexión |
| Cabina / ST7796S | SCK | **12 / SPI2**, reloj20 MHz | Nueva conexión |
| Cabina / ST7796S | CS | **10 / SPI2** | Nueva conexión |
| Cabina / ST7796S | DC/RS | **9**, salida | Nueva conexión |
| Cabina / ST7796S | RST | **8**, salida | Nueva conexión |
| Cabina / ST7796S | SDO/MISO | Sin conectar, MISO=-1 | Interfaz sólo escritura |
| Cabina / pantalla | Touch, SD, BL por GPIO | Sin asignación | Touch/SD deshabilitados; BL/alimentación requieren identificar breakout |
| Cabina / BNO085 | DI/MOSI | **6 / SPI3** | Nueva conexión |
| Cabina / BNO085 | DO/MISO | **7 / SPI3** | Nueva conexión |
| Cabina / BNO085 | SCK | **5 / SPI3**, reloj3 MHz | Nueva conexión |
| Cabina / BNO085 | CS | **4 / SPI3** | Nueva conexión |
| Cabina / BNO085 | HINT/INT | **15**, entrada IRQ | Nueva conexión |
| Cabina / BNO085 | RST | **16**, salida | Nueva conexión |
| Cabina / BNO085 | PS0/PS1 | Sin GPIO de control | Seleccionar SPI conforme a la placa BNO identificada; comprobar jumpers |
| Cabina / GPS | TX GPS → RX ESP | **18 / UART2**, 9600 8N1 | Nueva conexión |
| Cabina / GPS | TX ESP → RX GPS | **17 / UART2** | Opcional para recepción NMEA; GPIO reservado aunque no se conecte |
| Cabina / botón opcional | Contacto a GND | **21**, pull-up interno | Deshabilitado por defecto |
| Cabina / consola principal | TX / RX | **43 /44, UART0** | Usar puente USB-UART de la placa si existe; comprobar conector/COM |
| Cabina / USB nativo | D- / D+ | **19 /20, USB Serial/JTAG** | Reservados; consola alternativa, no conectar pantalla/IMU |
| Trasero Chris / BNO085 | MOSI / MISO / SCK | **18 /16 /15, SPI2** | Conservar soldaduras |
| Trasero Chris / BNO085 | CS / HINT / RST | **10 /11 /12** | Conservar soldaduras y montaje |
| Trasero Chris / BNO085 | Selección SPI y alimentación | Montaje existente | No cambiar jumpers ni alimentación comprobada |
| Trasero / RS485 | DI ← TX / RO → RX | **17 /13, UART1** | Asignación conservada; UART no se inicia en perfil de ensayo |
| Trasero / RS485 | DE/RE | **14, RTS UART1 half duplex** | Asignación conservada; sin actividad en ensayo |
| Trasero / sonda agua | A/B al transceptor | No GPIO directo | **Desconectada en prueba virtual** |
| Trasero / consola principal | TX / RX | **43 /44, UART0** | Configuración principal conservada, comprobar puente/puerto |
| Trasero / USB nativo | D- / D+ | **19 /20, USB Serial/JTAG** | Reservados para alternativa USB |
| Ambas / periféricos | GND | GND de la placa correspondiente | Comprobar continuidad antes de alimentar |

Centralización: GPIO y números de SPI en
`firmware/integracion/cabina/main/Kconfig.projbuild`, menú
`Cableado cabina ESP32-S3 N16R8`. `cableado_cabina.h` traduce números2/3 a
`SPI2_HOST`/`SPI3_HOST`; pantalla, IMU y guard consumen los mismos valores.
Cambiar cualquier asignación por menuconfig exige recompilar y flashear.
El mapa DevKit V1 (23/19/18/5/4/22, etc.) **no es plantilla para S3**.

## Alimentación: lo que aún hay que identificar

El datasheet especifica alimentación de3.3 V para el **módulo WROOM**, no la
función de un pin VIN/5V de una placa de desarrollo desconocida. No deducir
reguladores ni conectores de alimentación de una foto comercial. La lógica
ESP32 es3.3 V; no aplicar una salida lógica de5 V a sus entradas.

El modelo/placa concreta de GPS no está confirmado: el driver NMEA/9600 no
acredita su tensión de alimentación. ST7796S identifica al controlador, no la
alimentación de VCC/LED-BL del breakout. BNO085 identifica al circuito, no el
regulador del carrier. Antes de conectar VCC de cabina identificar etiquetas y
placas/manuales disponibles; no se inventa una tensión para esos módulos.
En Chris conservar sólo las conexiones y alimentación previamente comprobadas.
No se proponen compras ni sensores adicionales.

## Configuración efectiva preparada

| Concepto | Cabina S3 | Trasero S3 |
|---|---|---|
| Target | `esp32s3` | `esp32s3` |
| Flash |16 MB, opción QIO, 80 MHz |16 MB, opción QIO, 80 MHz |
| Cabecera de imagen / esptool |DIO, por diseño del SDK |DIO, por diseño del SDK |
| PSRAM |Octal, auto-detección, 80 MHz, init+memtest solicitados |Igual |
| Acceso PSRAM |`SPIRAM_USE_CAPS_ALLOC`, malloc normal permanece interno |Igual |
| Partición app conservada |`singleapp_large`:1500 KiB, offset0x10000 |`singleapp`:1 MiB, offset0x10000 |
| Consola normal |UART0,43/44,115200 |UART0,43/44,115200 |
| Consola de captura principal |UART0,43/44,921600,8N1 |Igual |

No se amplían buffers ni particiones por disponer de más memoria. Flash restante
no se convierte en una nueva partición ni en más colas. `SPIRAM_USE_CAPS_ALLOC`
no cambia malloc estándar a PSRAM; las asignaciones existentes no se aumentan.
No se habilitan memoria para instrucciones/rodata ni stacks externos.

**QIO frente a DIO:** `CONFIG_ESPTOOLPY_FLASHMODE_QIO=y` selecciona Quad; en
ESP-IDF6.1 `CONFIG_ESPTOOLPY_FLASHMODE="dio"` es la cabecera que utiliza el ROM
para cargar el segundo cargador, que habilita Quad. No editar esa cadena para
que diga QIO. Fundamento exacto: `components/esptool_py/Kconfig.projbuild`,
comentario y defaults76..98 del SDK local. `HW_CONFIG.flash_mode` imprime esa
cadena de cabecera; se contrasta con la opción QIO del sdkconfig efectivo.
Ni esa opción ni el éxito de compilación prueban la memoria instalada.

## Perfiles vigentes y flags

Todos los perfiles siguientes usan directorios **nuevos** `build/s3/...`.
Las carpetas antiguas `build`, `build/virtual100`, etc. son historial; no son
imágenes de cabina S3. Se respaldaron los sdkconfig antes de la migración.

| Nodo / perfil | Directorio relativo al proyecto | GPS / IMU cabina / LCD | Radio | Agua Modbus | Captura | Target IMU |
|---|---|---|---|---|---|---|
| Cabina real normal |`build/s3/normal` |Sí / sí diagnóstico / sí |Sí |No aplica local |No, consola115200 |100 Hz solicitado |
| Cabina pipa virtual100 |`build/s3/virtual100` |Sí / sí diagnóstico / sí |Sí |Ignorada; agua virtual50 %vol |Sí,921600 |100 Hz solicitado |
| Cabina pipa virtual50 |`build/s3/virtual50` |Sí / sí diagnóstico / sí |Sí |Ignorada; agua virtual50 %vol |Sí,921600 |50 Hz solicitado |
| Trasero real normal |`build/s3/normal` |No aplica / BNO trasero activo |Sí |Habilitada, cruda sin calibrar |No,115200 |100 Hz solicitado |
| Trasero ensayo100 |`build/s3/capture100` |BNO trasero activo |Sí |**Deshabilitada**, sonda desconectada |Sí,921600 |100 Hz solicitado |
| Trasero ensayo50 |`build/s3/capture50` |BNO trasero activo |Sí |**Deshabilitada**, sonda desconectada |Sí,921600 |50 Hz solicitado |

Botón `CI_ENABLE_BUTTON=n` en todos; GPIO21 queda reservado. En cabina de ensayo:
`CI_PIPA_VIRTUAL=y`, `CI_ENABLE_GPS=y`, `CI_ENABLE_CABIN_IMU=y`,
`CI_FULL_CAPTURE=y`, `CI_VIRTUAL_FILL_PERCENT=50`, `CI_VIRTUAL_LEVER=n`,
`CI_VIRTUAL_MOUNT_CONFIRMED=n`. El flag de montaje real también se deja sin
confirmar. No se fabrican calibraciones ni datos disponibles. `CI_BENCH_50HZ=y`
sólo en ambos perfiles50. La demo sintética sigue separada: no inicia los
periféricos/radio reales aunque haya flags heredados activos.

Trasero de ensayo: `CI_REAR_WATER_ENABLED=n` omite UART1 y tarea Modbus;
`CT_WATER=0`, aguaNaN, tiempo/secuencia de agua0 significan **sin muestra**.
`REAR_MB` indica `DISABLED_SENSOR_DISCONNECTED_NO_TRANSACTION`, no timeout ni
respuesta Modbus inventada. El registro candidato0x0004 sigue documentado,
pero no se consulta. El CST2 de124 bytes y sus offsets no cambian. La envoltura de captura
conserva CRC32; no se añade un CRC nuevo al CST2.

La excitación sigue siendo **IMU trasero**, el de cabina sólo diagnóstico.
El agua50 % es condición configurada del escenario, sin adquisición ni caducidad
de sensor. Se conservan frescura IMU/GPS, pérdidas, continuidad, rearme, captura
y ecuaciones. No es LTR del automóvil ni predicción validada de volcadura.
Parámetros y límites: [pipa virtual](pipa-virtual.md).

## Preparar y compilar sin reutilizar el build clásico

Terminal ESP-IDF6.1 configurada; desde raíz del repositorio:

```powershell
python pruebas/modelo-a/preparar-captura.py --normal
python pruebas/modelo-a/preparar-captura.py --virtual --rate 100
python pruebas/modelo-a/preparar-captura.py --virtual --rate 50
```

El preparador conserva los S3 ya generados; no copia GPIO DevKit V1. Recupera
parámetros de aplicación anteriores, selecciona memoria/target/perfil y crea
configs efectivos nuevos. `--refresh-generated` respalda antes de reemplazar,
restablece llenado50 %, leverOFF y montaje sin confirmar: no usarlo después de
una comprobación física sin revisar las opciones de nuevo.

```powershell
cd C:/esp/Proyectos/Cistable/firmware/integracion/cabina
idf.py -B build/s3/normal -D SDKCONFIG=C:/esp/Proyectos/Cistable/firmware/integracion/cabina/sdkconfig build
idf.py -B build/s3/virtual100 -D SDKCONFIG=C:/esp/Proyectos/Cistable/firmware/integracion/cabina/build/s3/virtual100/sdkconfig menuconfig
idf.py -B build/s3/virtual100 -D SDKCONFIG=C:/esp/Proyectos/Cistable/firmware/integracion/cabina/build/s3/virtual100/sdkconfig build
cd ../trasero
idf.py -B build/s3/capture100 -D SDKCONFIG=C:/esp/Proyectos/Cistable/firmware/integracion/trasero/build/s3/capture100/sdkconfig build
```

Para50 usar **virtual50 y capture50**, ambos nodos. Normal trasero utiliza
`-B build/s3/normal -D SDKCONFIG=.../trasero/sdkconfig`. Para cambiar modos/opciones
usar menuconfig con esos mismos `-B` y `SDKCONFIG`, luego compilar y flashear la
misma configuración. Los CMake rechazan targets distintos de S3.

Sólo ustedes, tras identificar placa, alimentación y puertos reales (COM5/6
son ejemplos):

```powershell
cd C:/esp/Proyectos/Cistable/firmware/integracion/cabina
idf.py -B build/s3/virtual100 -D SDKCONFIG=C:/esp/Proyectos/Cistable/firmware/integracion/cabina/build/s3/virtual100/sdkconfig -p COM5 flash
cd ../trasero
idf.py -B build/s3/capture100 -D SDKCONFIG=C:/esp/Proyectos/Cistable/firmware/integracion/trasero/build/s3/capture100/sdkconfig -p COM6 flash
```

No ejecutar `set-target` ni limpiar los builds antiguos: conservarlos como
historial. Tampoco cargar un `.bin` de ESP32 clásico a S3. `flash` usa los
binarios/offsets de `flasher_args.json` del build indicado; no copiar offsets
clásicos a mano.

## Consola y captura

Identificar si el conector de la placa va a un puente USB-UART (UART0,43/44)
o al USB nativo del S3 (19/20). No son intercambiables por elegir otro COM.
La configuración principal usa UART0. Si sólo está disponible el USB nativo:

```powershell
python pruebas/modelo-a/preparar-captura.py --virtual --rate 100 --console usb
```

Se preparan `build/s3/virtual100-usb` y `build/s3/capture100-usb`. Compilarlos
con los comandos impresos antes de flashearlos; el sufijo evita sobreescribir
las imágenes UART. El backend USB mantiene bytes binarios/LF y no aplica la
cota921600 de UART: el baud de un COM USB es virtual, no capacidad del enlace.
Hay que medir throughput y descartes; no se afirma que esté probado en placa.
No se presupone que la placa concreta exponga ese conector.

Cerrar **todos los monitores serie** antes del capturador (Ctrl+] en IDF).
Desde raíz, dos terminales para los dos puertos UART principales:

```powershell
C:/Espressif/tools/python/v6.1/venv/Scripts/python.exe pruebas/modelo-a/captura_completa.py record --transport uart --port COM5 --baud 921600 --seconds 180 --interactive --output capturas/cabina-s3-100.raw --config firmware/integracion/cabina/build/s3/virtual100/sdkconfig
C:/Espressif/tools/python/v6.1/venv/Scripts/python.exe pruebas/modelo-a/captura_completa.py record --transport uart --port COM6 --baud 921600 --seconds 180 --output capturas/trasero-s3-100.raw --config firmware/integracion/trasero/build/s3/capture100/sdkconfig
python pruebas/modelo-a/captura_completa.py decode capturas/cabina-s3-100.raw --output capturas/cabina-s3-100-csv
python pruebas/modelo-a/captura_completa.py decode capturas/trasero-s3-100.raw --output capturas/trasero-s3-100-csv
```

Abrir captura antes de RESET para guardar BOOT/configuración. Si al reset USB
re-enumera y se pierde el puerto, la captura puede terminar con error: comprobar
el COM, reabrir y verificar BOOT/CONFIG, sin declarar completa una captura sin
metadatos. Con USB nativo usar `--transport usb` y sus sdkconfig `-usb`; baud921600
es sólo parámetro del puerto virtual. No abrir otro monitor para enviar órdenes:
`--interactive` acepta `f`, `s`, `a`, `u` y Enter; `q` termina.

## Comprobar al arrancar y siguientes pasos

1. Fotografiar/leer la inscripción real de ambos módulos y la identificación de
   PCB/puentes/conectores. Confirmar alimentación de los periféricos identificados,
   GND, tabla de señales y que19/20 y35/36/37 no tengan periféricos añadidos.
2. En logs SDK comprobar PSRAM detectada de8 MB y resultado del memtest. En
   `HW_EXPECTED` hay requisitos solicitados, **no mediciones**. `HW_CONFIG` muestra
   opciones compiladas. `HW_OBSERVED` debe indicar chip S3 (`chip_model=9` en este SDK), Flash física16777216
   bytes (API exitosa), PSRAM inicializada y8388608 bytes; tamaños no prueban por
   sí solos el modelo/revisión. `HW_MEMORY` informa heap y DMA, no sugiere ampliar
   buffers. Si no llega a esas líneas, guardar el error de arranque íntegro.
3. Revisar `WIRING`/`WIRING_ENABLED`: GPIO, SPI2 distinto de SPI3, GPS e IMU local
   activos, consola correcta. Verificar pantalla sin resets ni errores DMA/bus;
   después recepción CST2 trasera, calidad/tasas/secuencias, GPS real y BNO cabina.
   Agua trasera debe permanecer ausente/deshabilitada; porcentaje virtual50 %
   debe verse como CONFIGURADO, no lectura de sonda.
4. Con vehículo quieto, comprobar signos de los tres ejes y rotación completa del
   **IMU trasero**: g alineada cerca de(0,0,-9.81), lineal/gyro cerca de cero;
   movimientos controlados con regla de mano derecha. No restar90° a roll ni
   deducir rotación de fotos. La orientación parte del Game Rotation Vector al
   arrancar; `a` no pone a cero el sensor, declara theta/rate iniciales supuestos.
5. Sólo después configurar `Montaje IMU trasero > MOUNT_CONFIRMED`, recompilar y
   flashear ese perfil. No cambia clasificación a validado. Repetir captura,
   GPS válido en reposo y confirmaciones `f`, `s`, `a`. Se exigen dos parejas
   nuevas; lever permaneceOFF en automóvil. El BNO de cabina no habilita ni
   sustituye la comprobación del trasero.
6. Hacer prueba dinámica suave del escenario virtual y ensayo de desconexión/
   reinicio. Registrar tasa efectiva por reporte y RX, parejas, skew, pérdidas,
   tiempo sin resultado, contador/causa de cada desarmado y necesidad de operador.
   Si100 Hz no es estable, repetir con ambos50, sin relajar continuidad.

Guardar ambos `.raw`, `.raw.json`, copias sdkconfig, carpetas CSV completas,
notas/fotos de identificación y signos, estado de conectores, puertos y órdenes.
Compartir también manifiesto/hash/binarios y `flasher_args.json` del build
realmente cargado. `configuration.json` conserva HW/WIRING/CONFIG por arranque.
`summary.json` distingue pérdidas de captura, sobrescritura de RX, errores CRC,
secuencias, desarmados e intervalos sin resultado. GPIO/header/compilación no
son un PASS físico. Ver [captura](captura-prueba-fisica.md).

Evidencias nuevas: `pruebas/modelo-a/build/s3-final/`. Las evidencias clásicas y
los antiguos builds traseros sin OPI quedan intactos como historial de su
configuración real; no acreditan los perfiles N16R8 nuevos.
