# ESP32 Master

Proyecto ESP-IDF del master: lee GPS6MV2 y la BNO085 frontal, actualiza la pantalla ST7796S y recibe exclusivamente `TankPayload` de 29 bytes desde un emisor externo por ESP-NOW. No utiliza el protocolo heredado de 104 bytes ni ACK de aplicación.

El formato recibido está definido en `main/tank_payload.h` y debe coincidir byte por byte con el emisor: siete `float` y un byte de flags, con `packed`, para un total de 29 bytes. Esta misma estructura se usa para recepción, pantalla y registro; no existe una estructura paralela de telemetría.

Las muestras de IMU se leen y registran separadas; todavía no se fusionan con el GPS ni se detecta volcadura. 