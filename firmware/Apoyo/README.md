# Componentes de apoyo del firmware

Esta carpeta contiene los componentes propios que necesita el proyecto ESP-IDF del master. El emisor ESP-NOW y el nodo trasero son proyectos externos.

## Dependencia externa de la BNO085

`esp32_BNO08x/` no es código propio de Cistable. La referencia está documentada en la rama [Pruebas-Gael/sensores/IMU](https://github.com/sushemer/Cistable/tree/Pruebas-Gael/sensores/IMU), que indica obtener esta dependencia como submódulo desde el repositorio externo. Es la biblioteca utilizada por `imu_bno085` para comunicarse con la BNO085.

Para obtenerla localmente, desde `firmware/Apoyo/` se debe clonar:

```powershell
git clone https://github.com/myles-parfeniuk/esp32_BNO08x.git esp32_BNO08x
```

## Relación de archivos y procedencia

| Ruta | Función | Procedencia o referencia |
|---|---|---|
| `field_espnow/field_espnow.c` | Inicializa Wi-Fi/ESP-NOW y entrega al master frames RAW. | Integración propia basada en la especificación ESP-NOW del compañero. |
| `imu_bno085/imu_bno085.cpp` | Adaptador C/C++ que configura la BNO085 y publica sus muestras. | Adaptación propia; utiliza la API de `esp32_BNO08x`. |
| `esp32_BNO08x/` | Driver externo de la BNO080/BNO085. | Repositorio externo; se instala según `Pruebas-Gael/sensores/IMU`, no se copia como archivos normales. |
| `../esp32-master/main/tank_payload.h` | Formato ESP-NOW de 29 bytes. | Especificación recibida del compañero; debe coincidir byte por byte. |
| `../esp32-master/main/modules/gps/` | Lectura y validación del GPS6MV2. | Reutilización de la prueba GPS 02, adaptada al master. |
| `../esp32-master/main/modules/display/` | Pantalla ST7796S/LVGL. | Basada en la prueba de pantalla y adaptada a la telemetría actual. |
| `../esp32-master/main/modules/imu/` | Pines y arranque de la BNO085 frontal. | Integración propia; conserva los pines DevKit V1 comentados y los pines nuevos activos. |
| `water/README.md` | Referencia del sensor QDY30A por RS485/Modbus. | Revisión de la rama `Twig/sensores/agua`; aún no está activado en el master. |

