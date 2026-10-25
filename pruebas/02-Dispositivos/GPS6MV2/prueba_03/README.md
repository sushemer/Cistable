# Prueba 03 — GPS e IMU

La prueba 03 evaluará si una IMU ayuda a identificar lecturas anómalas del GPS y a describir el movimiento del vehículo. En el repositorio de referencia se identifica el modelo BNO085/GY-BNO085. El firmware registra GPS e IMU con una sola IMU por SPI y el logger genera una muestra combinada cada 500 ms; no realiza fusión ni activa alarmas.

## Enfoque

Primero se registrarán GPS e IMU con sus datos originales y marcas de tiempo; después se analizará su alineación y comportamiento. El prototipo usa una BNO085 por SPI en el ESP32 DevKit V1. Esta prueba no amplía todavía a varias IMUs ni utiliza ESP-NOW.

La salida de las IMU no reemplazará la velocidad Doppler del GPS automáticamente: la aceleración integrada puede derivar y depende de orientación, sesgo y sincronización. La estrategia final se decidirá con los registros de la prueba 02 y ensayos controlados de la prueba 03.

## Preparación

- [Diseño y plan de pruebas](DISEÑO_Y_PLAN.md): fundamentos, etapas, datos necesarios y decisiones pendientes. Los dos proyectos `DEF 1 IMU` son las referencias principales seleccionadas; los demás quedan como material complementario.
- `firmware/`: proyecto ESP-IDF con registro GPS e IMU en paralelo; reutiliza temporalmente los módulos GPS de la prueba 02.
- `tools/`: script para guardar los registros GPS e IMU crudos y generar muestras combinadas cada 500 ms en un mismo Excel.
- `pruebas/`: registros originales obtenidos con el script de la laptop.
- `analisis/`: comparación de GPS, IMU y tiempos de comunicación.

El consolidado `analisis/prueba_03_resultados.xlsx` mantiene los datos de entrada en hojas separadas, agrega magnitudes derivadas con fórmulas y enlaza el resumen con las hojas GPS e IMU. Excel recalcula las fórmulas al abrir el libro; los registros originales en `pruebas/` no se modifican.

## Prueba estática

1. En una terminal ESP-IDF, desde `prueba_03/`, compile con `idf.py -C firmware build` y cargue con `idf.py -C firmware -p COM5 flash`. Sustituya `COM5` por el puerto del ESP32. La salida USB de depuración trabaja a 115200 baudios.
2. Conecte el GPS por UART2 (TX del GPS a GPIO16 del ESP32; RX del GPS a GPIO17, opcional si solo se recibe) y la BNO085 por SPI según el cableado de `firmware/README.md`.
3. Instale una vez las dependencias desde esta carpeta: `py -m pip install -r tools/requirements.txt`.
4. Cierre ESP-IDF Monitor y cualquier programa que use el puerto COM. Desde `prueba_03/`, inicie el registro con:

   ```powershell
   py tools/gps_imu_logger.py COM5 prueba_03_estatica_01.xlsx
   ```

   Cambie `COM5` por el puerto del ESP32 y escriba el nombre que quiera para el archivo. El script crea la carpeta de salida si aún no existe. No abra el mismo COM en el monitor y en el registrador a la vez.
5. Deje el equipo quieto en la ubicación de prueba; detenga con `Ctrl+C`. El libro se guarda cada cinco segundos y al salir, con hojas `GPS`, `IMU`, `Muestras_500ms` y `Eventos`. El GPS no se calibra al cambiarlo de lugar: necesita adquirir fix, lo cual puede tardar varios minutos según cuánto tiempo estuvo apagado y la vista al cielo. No detenga el registro solo porque aún no aparezca fix.

En `Muestras_500ms` se conserva la última lectura disponible de cada sensor para formar una fila común. `gps_actualizado` indica si llegó una nueva sentencia RMC desde la fila anterior y `gps_edad_ms` indica qué tan antigua es esa lectura GPS. Por eso pueden repetirse velocidad o posición durante medio segundo: no significa que el GPS esté enviando una lectura nueva a 2 Hz.

La primera prueba puede hacerse en ventana y patio. Mantenga anotado dónde estaba la antena y la IMU, si el cielo estaba despejado u obstruido, y las horas de inicio y fin. Este registro solo captura datos; no fusiona sensores ni determina todavía qué lecturas son correctas. La compilación de este proyecto debe verificarse en la instalación ESP-IDF disponible antes de dar por lista la carga al dispositivo.

La búsqueda bibliográfica ampliada y la selección de referencias específicas quedan pendientes de verificar; no se presentan aquí umbrales como si ya estuvieran validados.
