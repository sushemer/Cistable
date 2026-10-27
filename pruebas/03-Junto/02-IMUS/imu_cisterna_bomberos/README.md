# Prueba de IMU en cisterna

Aquí se encuentra únicamente la prueba de la IMU.

El programa imprime `IMU_DATA` cada 100 ms. 

## Registro en Excel
En otra terminal:
```powershell
cd "..\Cistable\pruebas\03-Junto\02-IMUS\imu_cisterna_bomberos"
py -m pip install -r apoyo/requirements.txt
py apoyo/imu_cisterna_logger.py COM# registros\registro_atras.xlsx
```
Nota: Es importante que en ningún otro lado se esté "monitoreando"
Use un archivo diferente para `cabina`, `arriba`, `abajo` o cualquier otra ubicación. 