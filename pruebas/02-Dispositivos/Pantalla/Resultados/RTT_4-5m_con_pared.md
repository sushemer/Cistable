- I (5885) ESPNOW_SENDER: Estado: PRECAUCION
- I (10885) ESPNOW_SENDER: Estado: PELIGRO
- I (15885) ESPNOW_SENDER: Estado: DETENIDO 78%
- I (20885) ESPNOW_SENDER: Estado: SEGURO
- I (25885) ESPNOW_SENDER: Estado: PRECAUCION
- I (30885) ESPNOW_SENDER: Estado: PELIGRO
- I (35885) ESPNOW_SENDER: Estado: DETENIDO 78%

- I (39485) ESPNOW_SENDER: RTT: intentos=194 enviados=194 errores=0 ACK_validos=100 vencidos=94 tardios_duplicados=0 tabla_saturada=0 cola_saturada=0
- I (39485) ESPNOW_SENDER: RTT_us: min=2375 promedio=4940 max=20033 una_via_estimada=2470

- I (40885) ESPNOW_SENDER: Estado: SEGURO
- I (45885) ESPNOW_SENDER: Estado: PRECAUCION
- I (50885) ESPNOW_SENDER: Estado: PELIGRO
- I (55885) ESPNOW_SENDER: Estado: DETENIDO 78%
- I (60885) ESPNOW_SENDER: Estado: SEGURO
- I (65885) ESPNOW_SENDER: Estado: PRECAUCION

- I (70085) ESPNOW_SENDER: RTT: intentos=347 enviados=347 errores=0 ACK_validos=200 vencidos=147 tardios_duplicados=0 tabla_saturada=0 cola_saturada=0
- I (70085) ESPNOW_SENDER: RTT_us: min=2375 promedio=5031 max=20033 una_via_estimada=2515

- I (70885) ESPNOW_SENDER: Estado: PELIGRO
- I (75885) ESPNOW_SENDER: Estado: DETENIDO 78%
- I (80885) ESPNOW_SENDER: Estado: SEGURO
- I (85885) ESPNOW_SENDER: Estado: PRECAUCION
- I (90885) ESPNOW_SENDER: Estado: PELIGRO

- I (91685) ESPNOW_SENDER: RTT: intentos=455 enviados=455 errores=0 ACK_validos=300 vencidos=155 tardios_duplicados=0 tabla_saturada=0 cola_saturada=0
- I (91685) ESPNOW_SENDER: RTT_us: min=2375 promedio=4632 max=20033 una_via_estimada=2316

- I (95885) ESPNOW_SENDER: Estado: DETENIDO 78%
- I (100885) ESPNOW_SENDER: Estado: SEGURO
- I (105885) ESPNOW_SENDER: Estado: PRECAUCION
- I (110885) ESPNOW_SENDER: Estado: PELIGRO
- I (115885) ESPNOW_SENDER: Estado: DETENIDO 78%
- I (120885) ESPNOW_SENDER: Estado: SEGURO

- I (121285) ESPNOW_SENDER: RTT: intentos=603 enviados=603 errores=0 ACK_validos=400 vencidos=203 tardios_duplicados=0 tabla_saturada=0 cola_saturada=0
- I (121285) ESPNOW_SENDER: RTT_us: min=2375 promedio=4541 max=20033 una_via_estimada=2270

- I (125885) ESPNOW_SENDER: Estado: PRECAUCION
- I (130885) ESPNOW_SENDER: Estado: PELIGRO
- I (135885) ESPNOW_SENDER: Estado: DETENIDO 78%
- I (140885) ESPNOW_SENDER: Estado: SEGURO
- I (145885) ESPNOW_SENDER: Estado: PRECAUCION
- I (150885) ESPNOW_SENDER: Estado: PELIGRO

- I (152885) ESPNOW_SENDER: RTT: intentos=761 enviados=761 errores=0 ACK_validos=500 vencidos=261 tardios_duplicados=0 tabla_saturada=0 cola_saturada=0
- I (152885) ESPNOW_SENDER: RTT_us: min=2375 promedio=4693 max=24188 una_via_estimada=2346

- I (155885) ESPNOW_SENDER: Estado: DETENIDO 78%
- I (160885) ESPNOW_SENDER: Estado: SEGURO


Con 500 ACK válidos tenemos:
- RTT mínimo: 2.375 ms
- RTT promedio: 4.693 ms
- RTT máximo: 24.188 ms
- latencia unidireccional estimada: 2.346 ms

Así que, para los paquetes que sí completaron ida + ACK, ESP-NOW está respondiendo rápido. El problema no es la latencia; es la cantidad de ACK que no regresaron dentro de los 500 ms.
