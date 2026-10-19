# Diseño y plan de la prueba 03

## 1. Propósito

Evaluar, con registros reproducibles, qué información aportan las tres IMU al GPS6MV2 durante el movimiento de un vehículo y si permiten reconocer temporalmente lecturas GPS sospechosas. La prueba se hará primero en un automóvil disponible; sus resultados describirán ese montaje, no certificarán el comportamiento ni los umbrales de un camión cisterna.

La prueba no debe ocultar ni borrar lecturas originales. GPS, cada IMU y los mensajes recibidos por ESP-NOW se conservarán para poder comparar después el dato crudo, la decisión del algoritmo y la referencia observada.

## 2. Qué significa “validar el GPS con IMU”

El GPS6MV2 entrega velocidad sobre el suelo estimada por el receptor. Una IMU mide aceleración y velocidad angular en el lugar donde está montada; no mide directamente la velocidad del vehículo. Si se integra aceleración en el tiempo, se obtiene un cambio de velocidad estimado, pero los pequeños errores de sesgo, orientación y ruido se acumulan como deriva. Por eso la IMU puede ayudar a detectar si un cambio rápido del GPS concuerda con el movimiento reciente, pero no convierte por sí sola una lectura en verdadera ni corrige la recepción satelital.

La comparación debe hacerse entre magnitudes en el mismo marco y en el mismo instante. Para contrastar velocidad longitudinal, se requiere conocer la orientación de los ejes de la IMU respecto al vehículo, separar la componente de gravedad y considerar la aceleración angular si el sensor está desplazado del punto de referencia. Sin estas correcciones, una curva, inclinación, vibración o montaje distinto puede parecer aceleración longitudinal.

## 3. Arquitectura por etapas

| Etapa | Qué se implementa | Por qué |
|---|---|---|
| 0. Cerrar prueba 02 | Revisar fix, edades, saltos, señal y tiempos de adquisición con GPS solo. | Establece la línea base; sin ella no se sabe si la integración mejora algo. |
| 1. Registro paralelo | Copiar el registro GPS de prueba 02 y añadir lectura cruda de cada IMU, identificador, contador de muestra y marca de tiempo de adquisición. Sin fusionar ni filtrar. | Permite medir frecuencia, ruido, sesgos, cortes y retrasos antes de diseñar un estimador. |
| 2. Alineación temporal y espacial | Medir retraso de GPS, lectura local y ESP-NOW; definir ejes del vehículo y orientación de cada sensor. | Datos llegados juntos no necesariamente fueron medidos juntos; el montaje cambia la interpretación de la aceleración. |
| 3. Contraste con una IMU | Comparar el cambio de velocidad Doppler con la aceleración longitudinal integrada solo durante intervalos cortos y con calidad conocida. Mantener siempre la velocidad GPS cruda. | Aísla errores y valor de una IMU antes de sumar complejidad y fuentes de retraso. |
| 4. Tres IMU | Añadir comparación entre nodos, detección de paquetes perdidos y análisis de diferencias entre posiciones. No promediar automáticamente. | Sensores separados observan vibraciones y rotaciones locales distintas; la discrepancia puede ser una señal útil, no ruido que deba ocultarse. |
| 5. Evaluación sin y con decisión | Reproducir los registros con reglas candidatas; comparar detección de picos, retraso, falsas alarmas y continuidad contra GPS solo. | La mejora debe demostrarse en datos separados de los usados para ajustar parámetros. |

La primera versión de prueba 03 será principalmente un registrador sincronizable, no un sistema de seguridad. Un filtro Kalman u otra fusión avanzada no se añade hasta contar con modelos, unidades, frecuencias, sincronización, calibración y datos suficientes para justificar sus estados y covarianzas.

## 4. Tiempos y comunicación

Cada nodo debe incluir en el paquete, como mínimo: identificador del sensor/nodo, contador creciente, tiempo local de adquisición, acelerómetro, giroscopio, estado de calibración disponible y estado de transmisión. El receptor agregará su propio tiempo de llegada. Se registrarán paquetes perdidos, duplicados, fuera de orden y latencia.

No se debe comparar la hora de llegada de ESP-NOW con la hora de recepción del GPS como si fueran marcas de adquisición equivalentes. Los relojes monotónicos de ESP32 distintos tampoco son directamente comparables sin estimar su desfase y deriva. La solución de sincronización —por ejemplo, sincronización periódica desde el nodo central y registro de su incertidumbre— se seleccionará después de conocer las placas, tasas de muestreo y topología reales. Si la incertidumbre temporal es grande respecto a los cambios que se quieren evaluar, esa muestra no se usará para validar picos.

El nodo frontal puede llegar antes por estar conectado localmente, pero rapidez de llegada no significa mayor exactitud. Para estimar traslación, el peso dependerá de calidad, calibración, orientación, rigidez del montaje y marca temporal; no de que un sensor esté “más cerca” de la pantalla. La cercanía al centro de rotación del vehículo puede reducir ciertos efectos de palanca, pero primero se debe conocer la posición física y el objetivo de cada IMU.

## 5. Calibración y arranque

No se propondrá una espera universal de 2–10 minutos. Hay que distinguir:

- **Adquisición del GPS:** tiempo hasta que el receptor obtiene datos de navegación utilizables; se medirá desde una condición de alimentación definida y se registrarán fix y calidad. Reiniciar el ESP32 no necesariamente reinicia el GPS si el módulo sigue alimentado.
- **Preparación de la IMU:** lectura del estado de calibración que soporte el modelo exacto, estimación de sesgo estacionario cuando corresponda y verificación de orientación/montaje. Esto no equivale a “calibrar la IMU” con un cronómetro.
- **Referencia vehículo-sensor:** conversión de ejes de la placa a ejes longitudinal, lateral y vertical del vehículo. Si se mueve o gira el sensor, esa referencia puede dejar de ser válida aunque el componente no haya cambiado.

Al encender, se registrarán por separado el inicio del ESP32, inicio o continuidad de alimentación del GPS, primer dato GPS válido, calidad de fix, inicio de muestras de cada IMU y estado de calibración reportado. Durante la inicialización, las señales no se declararán confiables por el solo hecho de haber transcurrido cierto tiempo.

## 6. Plan de registro

Mantener el script de laptop como almacenamiento de la prueba. El firmware debe emitir datos estructurados y sin pérdida silenciosa; la laptop agrega su hora de recepción y conserva el archivo por ejecución. Registrar una ficha manual por archivo:

- fecha, nombre de archivo, vehículo y trayecto;
- ubicación y orientación de GPS y de cada IMU, con foto o croquis;
- modelo exacto de cada IMU, placa/nodo y conexión;
- inicio de alimentación del GPS y reinicios del ESP32;
- estado del cielo/obstrucción (por ejemplo, exterior despejado, ventana/techo), sin mezclar condiciones en una sola etiqueta;
- actividad del vehículo: estacionado, arranque, velocidad estable, aceleración, frenado, giro o detención;
- versión de firmware, configuración, frecuencia configurada y observaciones del operador.

Conservar NMEA cruda, velocidad Doppler en nudos y km/h, estado de RMC/GGA, satélites, HDOP, antigüedad, errores UART, valores crudos de acelerómetro y giroscopio por eje, tiempo de adquisición y recepción, estado de calibración, contadores/secuencia ESP-NOW y datos de pérdida/latencia. No guardar solo el resultado filtrado.

## 7. Secuencia de ensayos

1. **Caracterización estacionaria:** vehículo inmóvil; registrar GPS e IMU para observar dispersión aparente de velocidad, cambios de posición, ruido, sesgo y estabilidad temporal. Repetir con montaje y condición de cielo descritos.
2. **Movimiento simple:** ruta segura y legal con tramos de aceleración moderada, velocidad aproximadamente estable, frenado y giros. El conductor no opera la laptop ni el equipo durante la marcha.
3. **Comparación de nodos:** repetir con los tres nodos activos y luego revisar cada flujo por separado para identificar diferencias de tasa, ruido, orientación o enlace.
4. **Recepción degradada controlada:** comparar condiciones documentadas de visibilidad sin fabricar una obstrucción peligrosa ni inferir que una prueba doméstica representa el vehículo final.
5. **Reproducción offline:** calcular métricas primero en registros de diseño y confirmar en recorridos independientes. Separar los datos usados para ajustar umbrales de aquellos usados para evaluar.

Para verificar velocidad de referencia se buscará una fuente independiente apropiada (por ejemplo, equipo de medición/registro del vehículo con precisión conocida o una ruta repetible y segura). El GPS del mismo módulo y la velocidad derivada de sus coordenadas no son una verdad-terreno independiente.

## 8. Evaluación y criterios de decisión

Comparar GPS solo con GPS más contraste IMU en los mismos recorridos y revisar:

- picos falsos por hora o por recorrido y proporción de eventos reales conservados;
- retraso para reconocer un cambio real de velocidad;
- error respecto a la referencia independiente, si se dispone de ella;
- duración y frecuencia de estados degradados, pérdida de muestras y recuperación;
- efecto de desalineación temporal, movimiento del montaje y pérdida de cada nodo;
- resultados por condición, no solo un promedio global.

No se establecerá un umbral numérico hasta revisar la distribución de los datos de prueba 02 y caracterizar el vehículo/sensores. Un límite de aceleración basado en el automóvil actual no se transferirá automáticamente al camión cisterna. Si la IMU y el GPS discrepan, el primer resultado será un estado de **desacuerdo/degradación** con datos crudos preservados; no se asumirá siempre que la IMU tiene razón.

## 9. Decisiones pendientes antes del firmware

1. Revisión exacta de las placas GY-BNO085 y confirmación de que las tres unidades usan realmente BNO085; confirmar rango, tasas y modo configurado en las placas.
2. Resolver la arquitectura: tres sensores por SPI a un ESP32-S3, como en un ejemplo del repositorio, o IMU frontal local y dos nodos traseros por ESP-NOW, como se había descrito para el montaje. Confirmar el modelo de ESP32 de cada nodo.
3. Ubicación mecánica, rigidez, orientación de ejes y si el montaje cambiará entre sesiones.
4. Frecuencia de muestreo requerida y presupuesto de latencia, derivados de las pruebas y del objetivo de detección.
5. Método de sincronización y forma de medir su error.
6. Referencia independiente para velocidad/aceleración y protocolo seguro de ensayo.
7. Resultados de prueba 02: comportamiento estacionario, primer fix, velocidad GPS, falsos picos, calidad de señal y límites del registrador.

Hasta resolver estos puntos, no se deben copiar parámetros de un ejemplo de otra IMU ni fijar calibración, pesos o umbrales en el firmware.

## 10. Referencias principales del repositorio

Se priorizarán los dos proyectos `DEF 1 IMU` indicados por el usuario. La carpeta [sensores/IMU del repositorio](https://github.com/sushemer/Cistable/tree/Pruebas-Gael/sensores/IMU) contiene también ejemplos secundarios, que no se tomarán como base principal. Los dos `DEF` aportan piezas distintas y sus configuraciones no se deben mezclar sin revisar hardware, interfaz y propósito:

| Ejemplo | Qué aporta como referencia principal | Qué se debe verificar antes de reutilizar |
|---|---|---|
| [DEF 1 IMU en C con rinku404](https://github.com/sushemer/Cistable/tree/Pruebas-Gael/sensores/IMU/DEF%201%20IMU%20en%20C%20con%20rinku404) | Base preferida para estudiar la lectura de una BNO085 en C con ESP-IDF: I²C, detección entre `0x4A`/`0x4B`, reportes y dependencia `rinku404/bno085` `1.0.2`. | El ejemplo usa GPIO 6/7 y una IMU. Hay que confirmar que esos pines sirven en la placa real y que la dependencia es compatible con el ESP-IDF del firmware GPS. |
| [DEF 1 IMU en C++ con Adafruit](https://github.com/sushemer/Cistable/tree/Pruebas-Gael/sensores/IMU/DEF%201%20IMU%20en%20C%2B%2B%20con%20Adafruit) | Referencia complementaria para reportes BNO08x, SPI, orientación, aceleración lineal y el concepto de tare/LTR. Su README describe tres sensores, mientras `main.cpp` y `PROMPT_GENERAR_IMU_C.md` describen una configuración de un sensor. | No copiar como una configuración única: cantidad de sensores, modelo ESP32/ESP32-S3 y pines varían dentro de esa carpeta. Si el firmware final será C, reutilizar el comportamiento/documentación, no incluir clases C++ en `main.c`. |

Los ejemplos secundarios [Prueba lectura en C](https://github.com/sushemer/Cistable/tree/Pruebas-Gael/sensores/IMU/Prueba%20lectura%20en%20C) y [Pruebas1](https://github.com/sushemer/Cistable/tree/Pruebas-Gael/sensores/IMU/Pruebas1) se consultarán únicamente para contrastar ideas de tres IMU por SPI; no son la base elegida ni implementan la arquitectura de nodos traseros por ESP-NOW.

Los ejemplos configuran reportes de orientación y aceleración lineal a `100 Hz` en algunos casos. Es solo un valor de referencia, no una frecuencia decidida para este proyecto. Debe verificarse en la documentación del fabricante y medirse junto con la carga de comunicación, las pérdidas y el tiempo real de cada muestra.

El “tare” de los ejemplos vuelve a definir una referencia de orientación; no sustituye la identificación de ejes IMU–vehículo ni corrige por sí solo sesgo, escala o sincronización. Asimismo, calcular un promedio simple de tres aceleraciones/ángulos y convertirlo en una alerta no constituye evidencia suficiente para adoptar esa regla en el sistema.

## 11. Referencias externas

La bibliografía específica de integración GNSS–IMU, documentación primaria del BNO085 y ejemplos verificados de sincronización todavía requiere búsqueda. La búsqueda profunda se intentó, pero el servicio Exa informó que la cuenta agotó sus créditos. Esta sección se completará con documentos del fabricante, documentación ESP-IDF y publicaciones académicas antes de definir el algoritmo; no se usarán referencias de blogs genéricos como sustituto.
