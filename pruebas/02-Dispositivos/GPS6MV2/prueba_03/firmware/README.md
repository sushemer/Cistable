# Firmware de la prueba 03 — lectura inicial de IMU

Proyecto ESP-IDF para registrar en paralelo el GPS y una BNO085 en un ESP32 DevKit V1. Reutiliza temporalmente los módulos de recepción, parser y validación del GPS de la prueba 02 y añade la lectura de aceleración lineal, giroscopio y cuaternión de orientación. No fusiona señales ni genera alarmas.

Los módulos GPS se compilan desde `prueba_02/firmware/main` mediante una ruta relativa en `main/CMakeLists.txt`. Esto evita duplicar código mientras la prueba 02 sigue en desarrollo; cuando se cierre esa prueba, convendrá congelar o copiar aquí la versión validada para conservar la reproducibilidad. Por esa misma razón, la prueba 03 usa los valores editables en `prueba_02/firmware/main/gps_config.h`; los ajustes de GPS afectan ambos firmwares.

## Qué se reutiliza y por qué

| Referencia | Se toma como base |
|---|---|
| Firmware de prueba 02 | UART GPS, ensamblado NMEA, parser, validación de RMC/GGA y formato GPSDIAG v1 reconocido por el script de apoyo. |
| DEF 1 IMU en C | Inicialización BNO085 por I²C, detección de las direcciones 0x4A/0x4B, callback, reportes y ciclo periódico de servicio.|
| Componente rinku404/bno085 1.0.2 | API fijada explícitamente como dependencia del proyecto; compatible con ESP-IDF 5.4. |

La tasa inicial solicitada de 10 Hz se eligió para que los tres flujos de texto IMU más el diagnóstico GPS quepan en la consola serial; es una tasa de registro inicial, no una tasa validada para detección rápida. El servicio del componente se atiende cada 10 ms según su ejemplo; eso no significa que cada reporte llegue a 100 Hz.

Se empieza con una IMU porque la versión 1.0.2 del componente documenta una sola instancia activa por ESP32 debido al estado global de su biblioteca SH-2. No se deben conectar tres BNO085 a este mismo firmware esperando que este componente las atienda como tres instancias. Si se eligen nodos ESP-NOW separados, cada ESP32 puede registrar su sensor local; si se elige un único ESP32-S3 para las tres, habrá que escoger o desarrollar una biblioteca multinstancia y resolver el direccionamiento/conmutación I²C o SPI antes de implementar esa arquitectura.

El código usa la API I²C master nueva de ESP-IDF, disponible desde la serie 5.2; por eso el manifiesto declara ese mínimo. No se usa Kconfig para los parámetros del GPS: prueba 03 comparte los valores de `gps_config.h` de prueba 02. Si se necesita cambiar GPS solo para esta prueba, primero habrá que separar su configuración para no alterar la prueba 02.

## Conexión inicial

| BNO085 | ESP32 DevKit V1 |
|---|---|
| SDA | GPIO21 |
| SCL | GPIO22 |
| INT | GPIO19 |
| RST | GPIO18 |
| GND | GND |
| VCC | Alimentación indicada por la placa |

Los pines son valores iniciales y se cambian en main/imu_device.c. Para I²C, el componente indica mantener PS0 y PS1 en GND; la selección de dirección depende del pin AD0 (0x4A/0x4B). Verificar también el voltaje y las resistencias pull-up de la placa. No conectar por estos pines a ciegas si el modelo exacto del breakout indica otra cosa.

## Datos emitidos

El firmware registra aceleración lineal (m/s²), velocidad angular del giroscopio (rad/s) y cuaternión de orientación, junto con el timestamp SH-2 y el tiempo monotónico del ESP32 al atender el callback. Son relojes distintos; se conservan para analizar su relación sin asumir sincronización perfecta. También se registra el estado de precisión del sensor. El script `../tools/gps_imu_logger.py` guarda las filas GPS, IMU y eventos en hojas separadas del mismo Excel. No usar estos datos para validar velocidad hasta contar con una captura completa y alineada.

El GPS emite `GPSDIAG` v1 con el mismo orden de columnas que la prueba 02. La velocidad se deja explícitamente como `SIN_FILTRO`: esta etapa conserva datos crudos y no aplica el filtro de saltos de velocidad de prueba 02.

## Compilación

En una terminal ESP-IDF, ejecutar: idf.py set-target esp32, idf.py build y luego idf.py flash monitor.

La dependencia rinku404/bno085 se descarga con el Component Manager desde main/idf_component.yml. Para la placa final ESP32-S3, seleccionar esp32s3 y revisar los GPIO antes de compilar y conectar el hardware.

## Referencias

- [Ejemplo DEF 1 IMU en C del proyecto](https://github.com/sushemer/Cistable/tree/Pruebas-Gael/sensores/IMU/DEF%201%20IMU%20en%20C%20con%20rinku404): base para inicialización I²C, detección de dirección, callback y reportes BNO085.
- [Componente BNO085 1.0.2](https://components.espressif.com/components/rinku404/bno085/versions/1.0.2/readme): API usada por este firmware.
- [Diseño y plan de la prueba 03](../DISEÑO_Y_PLAN.md): etapas, alcance y decisiones posteriores.
