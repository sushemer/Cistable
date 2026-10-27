# Análisis de la prueba 3

Este directorio contiene el análisis exclusivo de la prueba 3, que registra
GPS e IMU en paralelo.

## Archivo principal

`prueba_03_analisis.xlsx` compara las situaciones:
- Carro
- Ventana 02

La hoja `Resumen` calcula con fórmulas los registros RMC, fix válidos, porcentaje de fix, velocidad promedio, mínima y máxima, satélites, HDOP, duración y recorrido aproximado. También calcula la aceleración lineal promedio y máxima de la IMU y contiene gráficas comparativas.

En la parte inferior de `Resumen` se incluyen gráficas del recorrido GPS del carro y de ventana. 

También se incluyen gráficas de velocidad GPS respecto al tiempo y gráficas de aceleración lineal de la IMU respecto al tiempo. Usan directamente las tablas principales `GPS_Carro`, `GPS_Ventana02`, `IMU_Carro` e `IMU_Ventana02`, sin duplicar los registros en hojas adicionales. Esto permite comparar el cambio de velocidad con los periodos en que la IMU detectó actividad, frenado o quietud, y deja una estructura reutilizable para la prueba 4. Las gráficas de velocidad utilizan tiempo transcurrido desde el inicio de cada situación, conmarcas aproximadas cada minuto.

Las hojas `GPS_*` e `IMU_*` conservan los datos de cada situación. Las columnas auxiliares muestran cómo se obtienen las métricas sin modificar los registros originales.

## Interpretación
Los resultados de la prueba 3 fueron los siguientes:

- `Carro`: se obtuvieron 426 registros RMC válidos de 577. La velocidad promedio de las lecturas válidas fue aproximadamente 10.67 km/h, con mínima de 0.23 km/h y máxima de 20.57 km/h. La IMU registró una aceleración lineal promedio cercana a 0.59 m/s² y picos de aproximadamente 3.75 m/s², lo que confirma actividad del vehículo.
- `Ventana 02`: el GPS reportó una velocidad promedio aproximada de 0.40 km/h y máxima de 3.09 km/h. Sin embargo, la IMU registró una aceleración promedio de aproximadamente 0.014 m/s² y una máxima de 0.063 m/s². Esto indica que la ventana estaba quieta y que las velocidades pequeñas correspondían al ruido o desplazamiento aparente del GPS.

La velocidad promedio del carro fue prácticamente igual a la de la prueba 2.
La diferencia principal fue la calidad de recepción: la prueba 3 tuvo menosregistros GPS válidos y más variación en HDOP. La IMU aportó información útil para distinguir la quietud de la ventana, pero no debe reemplazar la velocidad del GPS.

## Qué aportó la IMU

La IMU permitió observar el movimiento físico del equipo mientras el GPS reportaba velocidad. En el carro, la aceleración lineal tuvo un promedio de aproximadamente `0.59 m/s²`, un máximo cercano a `3.75 m/s²` y alrededor del 65 % de las lecturas superó `0.35 m/s²`. Esto es consistente con un vehículo en movimiento y con cambios de aceleración, frenado o vibración.

En `Ventana 02`, la aceleración lineal promedio fue aproximadamente `0.014 m/s²` y la máxima `0.063 m/s²`. Las lecturas permanecieron por debajo de `0.10 m/s²`, mientras el GPS llegó a reportar hasta `3.09 km/h`. La evidencia de la IMU indica que la ventana estaba quieta y que esas velocidades fueron principalmente ruido o desplazamiento aparente del GPS.

La aceleración de la IMU se calculó como la magnitud de sus tres ejes:
```text
aceleración = √(Ax² + Ay² + Az²)
```

Esto permite analizar la actividad general sin depender de la orientación de
un solo eje.

## Hallazgos principales
- El GPS midió correctamente el orden de magnitud de la velocidad del carro:
  aproximadamente `10.67 km/h` en la prueba 3.
- El carro tuvo 426 registros RMC válidos de 577, aproximadamente `73.8 %`.Por lo tanto, sí hubo velocidad registrada, pero una parte de las sentencias no cumplió las condiciones de validación.
- La IMU confirmó movimiento en el carro y quietud en la ventana.
- La IMU no puede demostrar por sí sola una velocidad constante. Si el carro avanza sin acelerar, la aceleración puede acercarse a cero aunque el vehículo siga moviéndose.
- La prueba 3 todavía no fusiona ni corrige la velocidad. Conserva GPS e IMU para analizar su relación y preparar la lógica de la prueba 4.

## Conclusiones 

La prueba 3 fue útil para comprobar que la IMU puede complementar al GPS. Su mejor uso es detectar quietud, arranque, frenado, aceleraciones y vibraciones, además de señalar cuando una velocidad GPS pequeña no coincide con el estadofísico del equipo.
