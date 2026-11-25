# Análisis de resultados de la prueba 01

## Conclusión principal
Las lecturas anormales de `patio_02` no representan el movimiento real del dispositivo. El patrón es compatible con señal degradada al pasar entre zonas
abiertas y techadas, seguida de una reacquisición del GPS.

La evidencia principal es la combinación de calidad GGA `0`, HDOP de hasta `99.99`, pocos satélites y saltos de posición. Por ello, `fix_valido=True` en
RMC no es suficiente para considerar confiable una lectura.

Estos resultados son una línea base del funcionamiento del GPS, no una calibración definitiva de exactitud.

## Método
Se compararon las lecturas RMC y GGA de cada archivo. Para cada posición se revisaron:
- Tiempo entre lecturas.
- Calidad del fix, satélites y HDOP.
- Velocidad reportada por RMC.
- Distancia y velocidad calculadas entre coordenadas mediante Haversine.

La velocidad RMC es el dato principal. La velocidad calculada con coordenadas se utiliza solamente como comprobación, porque un salto de posición puede
producir una velocidad falsa.

Se consideró regular un intervalo entre `0.8` y `1.2` segundos.
## Comparación general
| Prueba | Lecturas válidas | Intervalos regulares | Resultado |
|---|---:|---:|---|
| Ventana 01 | 64 | 63 | Estable; velocidad implícita máxima de `0.78 km/h`. |
| Ventana 02 | 285 | 284 | Estable; velocidad implícita máxima de `0.64 km/h`. |
| Patio 02 | 87 | 76 | Saltos incompatibles mientras caminaba. |
| Carro 01 | 211 | 210 | Lecturas coherentes. |
| Carro 02 | 155 | 154 | Lecturas coherentes. |
| Carro 04 | 384 | 382 | Generalmente coherente. |
| Carro 05 | 1194 | 1136 | Huecos y saltos importantes. |
| Carro 06 | 1911 | 1371 | Intervalos irregulares y lecturas agrupadas. |

`prueba_01_carro_03.xlsx` y `prueba_01_patio_01.xlsx` están dañados.

## Patio 02
El archivo contiene 87 posiciones válidas en aproximadamente 111 segundos. La velocidad RMC máxima registrada fue de aproximadamente `57.1 km/h`. Al calcular la velocidad con las coordenadas apareció un valor implícito de aproximadamente `156.8 km/h`. Este último se debe a un salto de posición y no representa la velocidad real. 
También hubo una interrupción de aproximadamente 18 segundos y un desplazamiento calculado de `146.4 m`.

Al inicio se observó:
- Calidad GGA `0` durante aproximadamente 10 segundos.
- HDOP de `99.99` y valores cercanos a `68`.
- Posteriormente, HDOP entre aproximadamente `1.0` y `1.8`, con 6 a 10 satélites.

El pico de aproximadamente `57 km/h` debe considerarse una lectura falsa. La transición entre techo y cielo abierto probablemente provocó obstrucción,
reflexiones y reacquisición de la solución.

## Interpretación de las demás pruebas
- **Ventana:** sirve como referencia del ruido del GPS cuando está casi inmóvil.
- **Carro 01, 02 y 04:** son las pruebas más útiles para validar el registro en movimiento.
- **Carro 05 y 06:** requiere revisar los tramos con interrupciones y saltos.

## Archivos

- [Resultados consolidados](./prueba_01_resultados.xlsx)
- [Prueba patio 02](../pruebas/prueba_01_patio_02.xlsx)
- [Prueba ventana 01](../pruebas/prueba_01_ventana_01.xlsx)
- [Prueba carro 01](../pruebas/prueba_01_carro_01.xlsx)
- [Prueba carro 02](../pruebas/prueba_01_carro_02.xlsx)
- [Prueba carro 04](../pruebas/prueba_01_carro_04.xlsx)
- [Prueba carro 05](../pruebas/prueba_01_carro_05.xlsx)
- [Prueba carro 06](../pruebas/prueba_01_carro_06.xlsx)
