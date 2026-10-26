# Prueba de IMU en cisterna

Aquí se encuentra únicamente la prueba de la IMU. No incluye el firmware completo del vehículo, GPS, pantalla ni ESP-NOW.

## Firmware ESP-IDF

El proyecto está en:

```text
pruebas/imu_cisterna_bomberos/firmware
```

Desde una terminal ESP-IDF:

```powershell
cd "C:\Users\SUSHEMER\OneDrive\Documentos\GitHub\Bomb\Cistable\pruebas\imu_cisterna_bomberos\firmware"
idf.py set-target esp32
idf.py build
idf.py -p COM5 flash monitor
```

El programa imprime `IMU_DATA` cada 100 ms. Cierre el monitor con `Ctrl+]` antes de iniciar el registro.

## Registro en Excel

En otra terminal:

```powershell
cd "C:\Users\SUSHEMER\OneDrive\Documentos\GitHub\Bomb\Cistable\pruebas\imu_cisterna_bomberos"
py -m pip install -r tools/requirements.txt
py tools/imu_cisterna_logger.py COM5 registros\registro_atras.xlsx
```

Use un archivo diferente para `cabina`, `arriba`, `abajo` o cualquier otra ubicación. El logger conserva la línea original y separa los campos de la IMU cuando recibe `IMU_DATA`.
