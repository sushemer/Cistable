# Prueba de IMU en cisterna

Aquí se encuentra únicamente la prueba de la IMU.

El programa imprime `IMU_DATA` cada 100 ms. 

## Registro en Excel
En otra terminal:
```powershell
cd "C:\Users\SUSHEMER\OneDrive\Documentos\GitHub\Bomb\Cistable\pruebas\03-Junto\02-IMUS\imu_cisterna_bomberos"
py -m pip install -r apoyo\requirements.txt
py apoyo/imu_cisterna_logger.py COM# registros\registro_atras.xlsx
```
Cambie `COM#` por el puerto del ESP32 y use un archivo diferente para `cabina`, `arriba`, `abajo` o cualquier otra ubicación.
Detenga el registro con `Ctrl+C`. Es importante que ningún otro programa esté monitoreando el mismo puerto serial.
