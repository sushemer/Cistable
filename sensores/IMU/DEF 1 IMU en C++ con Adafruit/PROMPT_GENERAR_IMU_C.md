# Prompt para generar una version C funcional del proyecto BNO085

Genera una implementacion completa en **C**, no C++, para un proyecto ESP-IDF que replique el comportamiento del programa C++ descrito abajo. El resultado debe ser codigo compilable y ejecutable en hardware real. No entregues pseudocodigo, fragmentos incompletos ni funciones simuladas.

## Contexto del hardware

- MCU real: **ESP32 clasico**, no ESP32-S3.
- Target ESP-IDF obligatorio: `esp32`.
- ESP-IDF: `v5.5.2`.
- Flash detectada: 4 MB.
- Se prueba temporalmente con **una sola IMU BNO085/BNO08x**.
- La IMU utiliza **SPI**, no I2C ni UART.
- El BNO08x debe estar configurado fisicamente en modo SPI y compartir GND con el ESP32.

## Pinout que no debes cambiar

Usa exactamente estos GPIO:

```text
SPI host       : SPI2_HOST
MOSI / DI      : GPIO18
MISO / DO      : GPIO19
SCK            : GPIO15
CS             : GPIO22
HINT / INT     : GPIO23
RST            : GPIO5
SPI frequency  : 3000000 Hz
SPI mode       : MODE 3
```

No agregues IMU 2 ni IMU 3. No uses los pines definidos por defecto en menuconfig si contradicen este pinout.

## Driver y protocolo obligatorio

El BNO085 usa el protocolo SH-2 sobre paquetes SHTP. La implementacion C debe:

1. Inicializar GPIO de `CS` y `RST` como salidas.
2. Inicializar `HINT/INT` como entrada con interrupcion por flanco descendente.
3. Inicializar `SPI2_HOST` con MOSI=18, MISO=19 y SCK=15.
4. Configurar el dispositivo SPI en modo 3 y maximo 3 MHz.
5. Controlar `CS` manualmente durante las transacciones. No dependas de hardware CS si el driver usado tiene la limitacion del ESP32 para transacciones full-duplex.
6. Implementar el HAL requerido por SH-2:
   - `spi_open`
   - `spi_close`
   - `spi_read`
   - `spi_write`
   - `getTimeUs`
   - callback de eventos asincronos
   - callback de eventos de sensores
7. Ejecutar reset hardware mediante GPIO5 antes de abrir SH-2.
8. Registrar el HAL con `sh2_open()`.
9. Llamar a `sh2_getProdIds()` y comprobar su resultado.
10. Si `sh2_getProdIds()` falla, informar claramente que la IMU no responde por SPI, imprimir todos los pines usados y detener la aplicacion. No continuar habilitando reportes ni crear la tarea de procesamiento.
11. Procesar los paquetes recibidos cuando `HINT` indique que hay datos disponibles.
12. Proteger el acceso compartido al estado SH-2 con mutex o el mecanismo equivalente necesario para evitar carreras entre la tarea SPI y la tarea de aplicacion.

Si el repositorio ya contiene la libreria Hillcrest `sh2`, reutilizala. Si contiene el componente `esp32_BNO08x`, puedes reutilizar sus fuentes C compatibles, pero no incluyas directamente clases C++ desde `main.c`. Si necesitas adaptar funciones C++, crea un wrapper C con `extern "C"` solo en el limite, o implementa el HAL directamente en C.

## Reportes que deben habilitarse

Despues de una inicializacion exitosa, habilita solamente estos reportes:

- Game Rotation Vector, para obtener orientacion sin magnetometro.
- Linear Acceleration, para obtener aceleracion sin gravedad.
- Intervalo de ambos reportes: `10000` microsegundos, equivalente a 100 Hz.

Para el Game Rotation Vector, convierte el quaternion a Euler y conserva:

```text
roll  = euler.x en grados
pitch = euler.y en grados
```

Para Linear Acceleration conserva:

```text
ay = aceleracion_lineal.y en m/s^2
```

Mantén el ultimo valor valido de roll y ay si en un ciclo no llega un reporte nuevo.

## Tare

Instala el driver RX de UART0 antes de usar `uart_read_bytes()`:

```c
uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0);
```

Acepta una `t` o `T` recibida por UART0 para ejecutar tare/calibracion a cero del Game Rotation Vector. El tare debe aplicarse solo a la unica IMU y debe informar si tuvo exito o fallo.

No llames a `uart_read_bytes()` si el driver UART no fue instalado correctamente.

## Algoritmo LTR

Implementa en C una funcion equivalente a:

```c
float calculate_ltr(float roll_deg, float ay,
                    float *roll_avg_out, float *ay_avg_out);
```

Con una sola IMU:

```text
roll_avg = roll_deg
ay_avg   = ay
```

Formula:

```text
roll_rad    = roll_avg * pi / 180
ay_effective = ay_avg * cos(roll_rad) + 9.81 * sin(roll_rad)
ltr_raw      = (2 * 1.8 * ay_effective) / (2.2 * 9.81)
ltr          = limitar(ltr_raw, -1.0, 1.0)
```

Si `abs(ltr) > 0.6`, registra una advertencia con `ESP_LOGW`.

## Tarea de procesamiento

Crea una tarea FreeRTOS equivalente a la del programa C++:

- Ejecutar en el core 0.
- Stack de 8192 bytes.
- Prioridad 5.
- Periodo aproximado de 10 ms usando `vTaskDelay(pdMS_TO_TICKS(10))`.
- No bloquear esperando indefinidamente por SPI ni UART.
- Leer datos nuevos de la IMU.
- Calcular LTR.
- Leer UART de forma no bloqueante.
- Imprimir continuamente esta cabecera una sola vez:

```text
Roll_1,Ay,LTR_Calculado
```

Despues imprimir una linea por ciclo con este formato:

```text
printf("%.2f,%.2f,%.4f\\n", roll_deg, ay, ltr);
```

## Arranque y manejo de errores

En `app_main`:

1. Registrar el inicio del sistema.
2. Instalar UART0.
3. Crear e inicializar una sola IMU.
4. Comprobar cada codigo de retorno.
5. Si la inicializacion de la IMU falla, imprimir:
   - el error de SH-2;
   - MOSI, MISO, SCK, CS, HINT y RST;
   - una indicacion para revisar modo SPI, alimentacion y GND;
   - y retornar sin crear la tarea.
6. Solo despues de inicializar correctamente habilitar reportes y crear la tarea.
7. No imprimir valores `0.00` como si fueran mediciones validas cuando la IMU no esta inicializada.

## Configuracion del proyecto

Genera o actualiza los archivos necesarios, incluyendo:

- `main/main.c`
- `main/CMakeLists.txt`
- cualquier wrapper o componente C necesario
- configuracion relevante de ESP-IDF

El target debe ser `esp32`, el tamaño de flash debe ser 4 MB y no debe quedar ninguna seleccion multiple de `ESPTOOLPY_FLASHSIZE`.

Conserva las dependencias existentes cuando sea posible. No reemplaces el protocolo SPI por I2C como solucion alternativa.

## Validacion obligatoria

Despues de generar el codigo:

1. Compila con ESP-IDF 5.5.2.
2. Corrige todos los errores de compilacion.
3. Explica cualquier advertencia que no sea bloqueante.
4. Comprueba que no existan referencias a IMU 2 o IMU 3.
5. Indica exactamente que binario debe grabarse.
6. Proporciona una lista breve de comprobacion del cableado.
7. Si la compilacion es correcta pero `sh2_getProdIds()` falla en hardware, no inventes que el sensor funciona: diagnostica la comunicacion SPI y muestra los pines reales usados.

La prioridad es que el programa C tenga el mismo comportamiento fiable que el programa C++ que ya funciona, especialmente en la secuencia de inicializacion SH-2, el modo SPI 3, el control manual de CS, el uso de HINT/RST y el manejo de errores.
