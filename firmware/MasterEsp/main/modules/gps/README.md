# GPS del master

Este módulo reutiliza el parser NMEA, la UART y la validación de la prueba 02. gps_module ejecuta la recepción en una tarea y publica la última muestra para la pantalla y el coordinador. Los pines y criterios se editan en driver/gps_config.h.

La velocidad mostrada es la reportada por RMC, convertida a km/h. En esta integración inicial se conserva como dato del receptor: todavía no se ejecuta el filtro de velocidad de la prueba 02 ni se fusiona con la IMU. La salida MASTERDATA conserva fix, posición, velocidad y HDOP para el futuro registrador.
