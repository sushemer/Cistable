# PRUEBA SLAVE

Firmware de prueba para la caja de cisterna.

Esta versión lee una sola BNO085, consulta el nivel de agua por RS485 y envía
por ESP-NOW el paquete de 17 bytes que espera `PRUEBA MASTER`.

La dependencia `esp32_BNO08x` se mantiene externa y debe estar disponible en
`components/esp32_BNO08x/` antes de compilar este proyecto.
