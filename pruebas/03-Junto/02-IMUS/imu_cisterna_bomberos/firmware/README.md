# Firmware ESP-IDF de una IMU en la cisterna

Proyecto mínimo para una BNO085 por SPI. No incluye GPS, pantalla, ESP-NOW ni sensores de agua.

## Cableado usado

| BNO085 | ESP32-S3 |
|---|---:|
| MOSI | GPIO 23 |
| MISO | GPIO 19 |
| SCLK | GPIO 18 |
| CS | GPIO 5 |
| INT / HINT | GPIO 4 |
| RESET | GPIO 22 |

PS0 y PS1 deben estar en VCC para seleccionar SPI, de acuerdo con el montaje usado anteriormente.

## Compilar y grabar

Desde una terminal ESP-IDF:

```powershell
cd "C:\Users\SUSHEMER\OneDrive\Documentos\GitHub\Bomb\Cistable\pruebas\imu_cisterna_bomberos\firmware"
idf.py set-target esp32
idf.py build
idf.py -p COM5 flash monitor
```

El firmware imprime líneas `IMU_DATA` cada 100 ms. Cierre el monitor con `Ctrl+]` antes de iniciar el logger de Excel.
