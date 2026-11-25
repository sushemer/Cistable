# Firmware

Proyecto ESP-IDF del master. El master lee el GPS6MV2 y la BNO085 frontal, muestra estado y velocidad y recibe telemetría ESP-NOW desde el ESP32 trasero.

Las IMU se muestrean y registran de manera independiente; no se fusionan ni se detecta volcadura. 
