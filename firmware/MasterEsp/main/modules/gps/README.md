# GPS del master

Este módulo reutiliza el parser NMEA, la UART y la validación de la prueba 02. gps_module ejecuta la recepción en una tarea y publica la última muestra para la pantalla y el coordinador. Los pines y criterios se editan en driver/gps_config.h.

La velocidad GPS solo se marca utilizable cuando RMC y GGA son recientes, válidas y coinciden en época; además, GGA debe reportar fix, al menos cuatro satélites y HDOP dentro del límite de `gps_config.h`. `gps_validation_state_name()` expone `SIN_DATOS`, `SIN_FIX`, `FIX_DEGRADADO`, `FIX_VALIDO` o `DATO_OBSOLETO`, junto con el motivo del resultado.

Cada sentencia NMEA recibida también se conserva completa en `OTRO.CSV`, con tipo de sentencia, resultado del checksum y resultado del parseo. Así se puede distinguir una interrupción UART de una sentencia recibida con checksum inválido.

La integración del Master combina una velocidad GPS utilizable con la aceleración lineal reciente de la BNO085 frontal mediante `gps_imu_fusion.c`. Solo corrige a cero una velocidad GPS de hasta 1 km/h después de confirmar aceleración lineal menor o igual a 0.20 m/s² durante 1 segundo. Si la IMU permanece quieta y el GPS supera 1 km/h, el estado marca la discrepancia como `GPS_SOSPECHOSO_IMU_QUIETA`, pero conserva la velocidad GPS porque aceleración baja no demuestra que el vehículo esté detenido. La velocidad cruda, la corregida, la calidad GPS y el estado de fusión se registran en `GPS.CSV` y `EVENT.CSV`.

Los umbrales son experimentales y deben validarse con las capturas de patio y ventana; no equivalen a una navegación inercial ni sustituyen una validación con referencia externa.
