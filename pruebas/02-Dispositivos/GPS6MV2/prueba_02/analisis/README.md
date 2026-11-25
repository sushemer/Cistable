# Análisis de la prueba 2

## Archivo principal
`prueba_02_analisis.xlsx` compara las situaciones:
- Carro
- Patio
- Ventana 01
- Ventana 02

La hoja `Resumen` contiene fórmulas para calcular registros RMC, fix válidos, porcentaje de fix, velocidad promedio, mínima y máxima, satélites, HDOP, duración y recorrido aproximado. 

En la parte inferior de `Resumen` se incluyen gráficas de recorrido para el carro, `Ventana 01` y `Ventana 02`. Cada gráfica usa longitud contra latitud directamente desde las columnas auxiliares de las hojas `GPS_*`. El patio no tiene una gráfica de recorrido porque no obtuvo coordenadas válidas.

También se incluyen gráficas de velocidad GPS respecto al tiempo para el carro, `Ventana 01` y `Ventana 02`. Las gráficas usan directamente las tablas principales `GPS_*` y sus columnas auxiliares, usando únicamente registros RMC con fix y velocidad válidos. Se conserva el formato visual de la prueba 2: puntos grandes y sin línea de conexión.

Las hojas `GPS_*` conservan los registros crudos de cada situación y agregan columnas auxiliares con fórmulas. No se deben editar manualmente los datos crudos.

## Interpretación
Los resultados de la prueba 2 fueron los siguientes:

- `Carro`: 525 registros RMC válidos, velocidad promedio de aproximadamente 10.64 km/h, mínima de 0.03 km/h y máxima de 20.07 km/h. El GPS mantuvo en promedio 8.17 satélites y un HDOP aproximado de 1.34. Es la situación más útil para evaluar la velocidad del vehículo.
- `Patio`: no obtuvo fix válido. Sus lecturas no permiten calcular una velocidad confiable; deben interpretarse como ausencia de solución GPS.
- `Ventana 01`: velocidad promedio aproximada de 0.12 km/h y máxima de 1.15 km/h. La lectura corresponde principalmente al ruido normal del GPS mientras el receptor permanecía quieto.
- `Ventana 02`: velocidad promedio aproximada de 0.53 km/h y máxima de 2.21 km/h. También debe interpretarse como ruido o desplazamiento aparente, no como movimiento real.

En conjunto, la prueba 2 muestra que el GPS puede entregar velocidades pequeñas aunque el receptor esté quieto. Por eso una velocidad baja no debe aceptarse automáticamente.

## Hallazgos principales

- El carro registró una velocidad promedio cercana a 10.64 km/h, con un máximo aproximado de 20.07 km/h. Sus 525 registros RMC fueron válidos y la calidad GPS fue adecuada para analizar el recorrido.
- El patio no tuvo fix válido, por lo que no se puede obtener una velocidad ni un recorrido confiables. Por ese motivo no aparece una trayectoria en la  gráfica de recorrido.
- `Ventana 01` y `Ventana 02` registraron velocidades pequeñas aunque estaban quietas. Estas lecturas deben considerarse ruido o desplazamiento aparente del GPS, no movimiento real.
- Las gráficas de velocidad contra tiempo permiten observar cuándo aparecieron los picos y no solo el promedio general. Esto será útil para comparar laprueba 4, donde la IMU ayudará a validar esos cambios.