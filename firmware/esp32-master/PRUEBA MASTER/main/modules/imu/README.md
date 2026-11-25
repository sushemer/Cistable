# IMU frontal del master

`imu_module.cpp` implementa `imu_module_start()` para configurar e iniciar la BNO085 frontal y `imu_module_get_sample()` para copiar su muestra más reciente. El componente común `imu_bno085` inicializa el driver C++, habilita orientación, aceleración lineal y giro, y los publica desde una tarea de lectura. No calcula LTR, no hace tare ni fusiona datos con el GPS.

| Dato | Unidad |
|---|---|
| Roll, pitch, yaw (Game Rotation Vector) | grados |
| Aceleración lineal | m/s² |
| Giro calibrado | rad/s |
| sample_time_us | tiempo monotónico local de recepción |

Se solicita un reporte cada 10 ms (100 Hz). La lectura se marca fuera de línea si pasan más de 500 ms sin una muestra nueva.

Pines anteriores para DevKit V1: SPI3, MOSI 13, MISO 34, SCLK 14, CS 25, HINT 26 y RESET 27. Se conservan comentados en `imu_module.cpp` como referencia.

Pines activos del nuevo cableado del master: SPI3, MOSI 11, MISO 13, SCLK 12, CS 14, HINT 6 y RESET 5. El bus SPI3 se mantiene separado del SPI2 de la pantalla.

Configuración física obligatoria de la BNO085: conectar `PS0=1` y `PS1=1` a `VCC` para seleccionar el modo SPI. Verificar también que CS, HINT y RESET compartan tierra con el ESP32.
