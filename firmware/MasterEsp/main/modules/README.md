# Módulos del master

Cada módulo toma lecturas o las presenta; app_main coordina los dispositivos y el enlace:

- gps/: parser, UART y validación reutilizados de la prueba 02.
- display/: puerto ST7796S/LVGL basado en la prueba de pantalla y vista de telemetría.
- imu/: configura el BNO085 frontal y publica sus muestras tipadas desde el componente común.
- app_main.c: coordina GPS, pantalla y ESP-NOW; emite MASTERDATA cada segundo para el registrador futuro.

Las IMU se registran de forma independiente, sin fusión ni detección de volcadura. La pantalla de referencia se documentó con ESP-IDF 6.1, por lo que debe verificarse con la versión local antes de usarla.
