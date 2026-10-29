# Registro serial de IMU y telemetría

El script `apoyo/imu_cisterna_logger.py` es un registrador serial genérico para
compartir con el compañero. Sirve si el ESP32 envía solo la IMU, la IMU junto
con el sensor de agua o mensajes adicionales. No necesita que el sensor de
agua esté conectado ni que su formato esté definido de antemano.

Guarda cada línea completa y separa los formatos conocidos de IMU y GPS. Si
encuentra una línea con campos separados por `|` o comas cuyo formato aún no
conoce, conserva los campos por posición en `campos_separados`; la línea
original también queda en `linea_serial` y `mensaje_crudo`.

Con el firmware de una sola IMU, los datos quedan identificados como
`IMU_DATA`. Con el master, `IMU_DATA|1` es la IMU frontal y `IMU_DATA|2` la
trasera; el GPS, RSSI y agua se registran aparte cuando aparecen. No abra otro
monitor en el mismo puerto COM mientras el logger esté conectado.

## Ejecutar el logger

Desde PowerShell, cambie a esta carpeta del repositorio y ejecute:

```powershell
cd "C:\Users\SUSHEMER\OneDrive\Documentos\GitHub\Bomb\Cistable\pruebas\03-Junto\02-IMUS\imu_cisterna_bomberos"
py -m pip install -r apoyo\requirements.txt
py apoyo\imu_cisterna_logger.py COM5 registros\registro_imu.xlsx
```


El libro se guarda periódicamente y al detener con `Ctrl+C`. No sobrescribe un
Excel existente; use un nombre nuevo para cada registro.
