# PRUEBA MASTER

Firmware de prueba para la caja de cabina/master.

Esta versión está enfocada en la prueba de campo y deja la pantalla fuera del
flujo para no bloquear la lectura de la IMU. El master:

- Lee la IMU local de la cabina.
- Lee GPS.
- Recibe por ESP-NOW una `TankPayload` de 17 bytes.
- Imprime por serial los datos locales y los recibidos.

El paquete de 17 bytes contiene una IMU remota, nivel de agua y banderas:

```text
roll_1       float  0..3
pitch_1      float  4..7
ay_1         float  8..11
water_level  float  12..15
flags        uint8  16
```

Para compilar se necesita ESP-IDF y la dependencia externa `esp32_BNO08x`
dentro de `firmware/Apoyo/esp32_BNO08x/`, de acuerdo con las instrucciones del
README de `firmware/Apoyo`.
