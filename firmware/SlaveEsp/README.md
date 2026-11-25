# SlaveEsp para ESP32-S3

Firmware trasero: lee una BNO085 por SPI, consulta el sensor de agua por RS485
y transmite el paquete `TankPayload` por ESP-NOW.

## Pinout revisado

### IMU BNO085, SPI2

| BNO085 | ESP32-S3 |
|---|---:|
| MOSI | GPIO3 |
| MISO | GPIO16 |
| SCLK | GPIO15 |
| CS | GPIO10 |
| HINT/INT | GPIO11 |
| RESET | GPIO12 |
| PS0 y PS1 | VCC |
| GND | GND |

### Sensor de agua RS485, UART2

| RS485 | ESP32-S3 |
|---|---:|
| TX del transceptor | GPIO18 (RX de UART2) |
| RX del transceptor | GPIO17 (TX de UART2) |
| DE/RE | Según el transceptor y su control de dirección |
| GND | GND común |

UART2 trabaja a 9600 baudios. GPIO18 no se comparte con el SPI de la IMU en
esta configuración.

### Control del relé del sensor

```text
GPIO4 → control del relé
```

