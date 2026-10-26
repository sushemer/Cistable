# Sensor de agua
El metodo que utilizamos fue el RS485 por que el adc del esp32 no es muy preciso y por que el sensor que compramos tiene salida rs485 en vez de una salida de corriente

Los resultados de los pruebas esta en un pdf adentro del folder rs485-agua

Las pruebas indico que el sensor detecta que tan profunda esta en el agua, regresando un valor en centimetros con un decimal y tiene un porcetaje de error de como un centimetro +- Por ejemplo si lo metes 5 centimetros de profundidad te va regresar un valor como 55 o 59 que significa 5.5 cm. Las pruebas comprueben estos resultados, las pruebas que se realizo fue moviendo el sensor poco a poco y midiendo su altura en el tanque y revision si el sensor ocupo ser encendido antes de meter al agua o si ya da resultados similares si se prende ya abajo del agua. Las pruebas concluyeron que no le afecta si el sensor se activa abajo del agua para su primer muestra.
