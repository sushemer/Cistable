# Pantalla del master

display_port.c parte del puerto ST7796S/LVGL usado en la prueba Pantalla_Interfaz. interface.c presenta velocidad GPS, estado del fix y datos recibidos; no incluye la interfaz ni cálculos de riesgo anteriores.

Los pines conservan los del ejemplo: SCLK 18, MOSI 19, CS 4, DC 21 y RST 22. Ajustarlos en display_port.c al cableado real y verificar que no choquen con el bus de la IMU. La referencia documenta ESP-IDF 6.1; esta integración requiere compilarse con la versión local antes de conectar.
