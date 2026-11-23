# Firmware de la prueba 04

Este proyecto reutiliza los módulos GPS de `prueba_02` y la lectura SPI de la
BNO085 usada en `prueba_03`. La diferencia es que aquí sí existe una capa de
decisión GPS+IMU en `main/gps_imu_fusion.c`.

La capa conserva el estado GPS original y agrega clasificación de reposo,
inicio, aceleración, frenado, movimiento constante, vibración/impacto,
pérdida de GPS y lecturas GPS sospechosas. La corrección a cero solo se aplica
a velocidades menores o iguales a 1 km/h cuando la aceleración lineal permanece
debajo de 0.20 m/s² durante 1 segundo.

El firmware emite:

```text
GPSDIAG|...|estado_fusion|velocidad_utilizable|...
GPSFUSION|1|tiempo|velocidad_gps|velocidad_corregida|cambio_velocidad|norma_accel|estado|reposo_ms|estado_gps|gps_valido|imu_fresca
```

`GPSDIAG` conserva el registro GPS original. `GPSFUSION` contiene la decisión
que el logger guarda en la hoja `FUSION`.

La IMU usa SPI con los pines de `main/imu_device.h`. El GPS usa UART2 en
GPIO16/17. La prueba no usa ESP-NOW.
