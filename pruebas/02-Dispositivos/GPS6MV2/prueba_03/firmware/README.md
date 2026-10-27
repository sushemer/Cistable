# Firmware de la prueba 03 — lectura inicial de IMU

Proyecto ESP-IDF para registrar en paralelo el GPS y una BNO085 por SPI en un ESP32 DevKit V1. Reutiliza temporalmente los módulos de recepción, parser y validación del GPS de la prueba 02 y añade la lectura de aceleración lineal y orientación. No fusiona señales ni genera alarmas.

Los módulos GPS se compilan desde `prueba_02/firmware/main` mediante una ruta relativa en `main/CMakeLists.txt`. Esto evita duplicar código mientras la prueba 02 sigue en desarrollo; cuando se cierre esa prueba, convendrá congelar o copiar aquí la versión validada para conservar la reproducibilidad. Por esa misma razón, la prueba 03 usa los valores editables en `prueba_02/firmware/main/gps_config.h`; los ajustes de GPS afectan ambos firmwares.

## Qué se reutiliza y por qué

| Referencia | Se toma como base |
|---|---|
| Firmware de prueba 02 | UART GPS, ensamblado NMEA, parser, validación de RMC/GGA y formato GPSDIAG v1 reconocido por el script de apoyo. |
| DEF 1 IMU en C++ | Inicialización BNO085 por SPI, reportes de orientación y aceleración lineal.|
| Componente `esp32_BNO08x` | API SPI fijada como dependencia Git del proyecto. |

La tasa inicial solicitada de la IMU es de 5 Hz, una lectura cada 200 ms. Es una tasa de registro inicial, no una tasa validada para detección rápida. El servicio interno se atiende cada 10 ms para procesar el bus SPI; eso no significa que se imprima un reporte cada 10 ms.

Se empieza con una sola IMU para validar la lectura SPI, el montaje y la alineación temporal con el GPS. No se amplía todavía a varias BNO085 ni a ESP-NOW.

El código usa el componente `esp32_BNO08x` por SPI. No se usa Kconfig para los parámetros del GPS: prueba 03 comparte los valores de `gps_config.h` de prueba 02. Si se necesita cambiar GPS solo para esta prueba, primero habrá que separar su configuración para no alterar la prueba 02.

## Conexión inicial por SPI

| BNO085 | ESP32 DevKit V1 |
|---|---|
| DI/MOSI | GPIO23 |
| DO/MISO | GPIO19 |
| SCK | GPIO18 |
| CS | GPIO5 |
| HINT/INT | GPIO4 |
| RST | GPIO22 |
| PS1 | 3.3 V |
| PS0/WAKE | 3.3 V |
| GND | GND |
| VCC | Alimentación indicada por la placa |

Los pines se definen en `main/imu_device.h`. Para SPI, `PS1` y `PS0` se conectan a 3.3 V durante el reset. `PS0` funciona después como `WAKE`; dejarlo alto mantiene despierta la IMU en esta prueba. No se utilizan las direcciones I²C `0x4A/0x4B`. Verificar el voltaje y el pinout del breakout exacto.

## Datos emitidos

El firmware registra aceleración lineal (m/s²), velocidad angular del giroscopio (rad/s) y cuaternión de orientación, junto con el timestamp SH-2 y el tiempo monotónico del ESP32 al atender el callback. Son relojes distintos; se conservan para analizar su relación sin asumir sincronización perfecta. También se registra el estado de precisión del sensor. El script `../tools/gps_imu_logger.py` guarda las filas GPS, IMU y eventos en hojas separadas del mismo Excel. No usar estos datos para validar velocidad hasta contar con una captura completa y alineada.

El GPS emite `GPSDIAG` v1 con el mismo orden de columnas que la prueba 02. La velocidad se deja explícitamente como `SIN_FILTRO`: esta etapa conserva datos crudos y no aplica el filtro de saltos de velocidad de prueba 02. El logger también genera `Muestras_500ms`, usando la lectura más reciente de GPS e IMU para facilitar el análisis temporal; no es una fusión ni activa alarmas.

La dependencia `esp32_BNO08x` se declara en `main/idf_component.yml`. Esta versión está preparada para `esp32`/DevKit V1; revisar los GPIO antes de conectar el hardware.

## Referencias

- [Ejemplo DEF 1 IMU en C++ con Adafruit](https://github.com/sushemer/Cistable/tree/Pruebas-Gael/sensores/IMU/DEF%201%20IMU%20en%20C%2B%2B%20con%20Adafruit): referencia de inicialización SPI y lectura de una BNO085.
- [Componente `esp32_BNO08x`](https://github.com/myles-parfeniuk/esp32_BNO08x): API SPI usada por este firmware.
- [Diseño y plan de la prueba 03](../DISEÑO_Y_PLAN.md): etapas, alcance y decisiones posteriores.
