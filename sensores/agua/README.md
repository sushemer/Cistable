# Guía Básica: Sensor de Nivel QDY30A con ESP32-S3

Esta guía explica de forma sencilla cómo conectar y programar el sensor de nivel de agua sumergible **QDY30A** utilizando un microcontrolador **ESP32-S3** y un módulo adaptador **RS485 (MAX485)**.

---

## 1. Funcionamiento del Sensor y Resultados de Pruebas

### ¿Cómo mide la profundidad?
El sensor entrega el valor de la profundidad en milímetros o décimas de centímetro como un número entero. Para obtener la profundidad real en centímetros, simplemente debes dividir el valor entre 10 (o recorrer el punto un decimal).

* **Ejemplo 1:** Si el sensor envía el número `55`, significa que la profundidad es de $5.5cm.
* **Ejemplo 2:** Si el sensor envía el número `59`, significa que la profundidad es de $5.9cm.

### Resultados de las Pruebas Realizadas
1. **Margen de Error:** En las pruebas dentro del tanque de agua, moviendo el sensor gradualmente, se determinó que tiene una precisión constante con un margen de error de aproximadamente 1cm.
2. **Encendido en Agua:** Se comprobó si el sensor requería calibrarse o encenderse al aire libre antes de sumergirlo. La prueba concluyó que **no le afecta encenderse sumergido**: la primera muestra tomada da el valor correcto sin importar si el sensor se encendió dentro o fuera del agua.

---

## 2. Conexiones de Hardware (ESP32-S3)

Para conectar el sensor al ESP32-S3 necesitas un módulo transceptor **MAX485 (RS485 a TTL)**.

### Conexión del Módulo MAX485 al ESP32-S3:
* **VCC** del MAX485 -> Conectar a **5V** del ESP32-S3.
* **GND** del MAX485 -> Conectar a **GND** del ESP32-S3.
* **TXD** del MAX485 -> Conectar al **GPIO 16** del ESP32-S3 (Recepción RX).
* **RXD** del MAX485 -> Conectar al **GPIO 17** del ESP32-S3 (Transmisión TX).

### Conexión del Sensor QDY30A al Módulo MAX485:
* **Cable Rojo (+V):** Conectar a la fuente de alimentación del sensor (usualmente 12V a 24V DC).
* **Cable Negro (GND):** Conectar al negativo / GND de la fuente de alimentación.
* **Cable A (RS485+):** Conectar a la terminal **A** del módulo MAX485.
* **Cable B (RS485-):** Conectar a la terminal **B** del módulo MAX485.

*(Nota: Asegúrate de unir la tierra/GND de la fuente del sensor con la tierra/GND del ESP32-S3).*

---

## 3. Cómo Usar el Código

El código está escrito en C utilizando el framework **ESP-IDF**. 

### Pasos para ejecutarlo:
1. Crea un proyecto estándar de ESP-IDF.
2. Reemplaza el contenido de tu archivo principal `main.c` con el código provisto abajo.
3. Conecta tu ESP32-S3 a la computadora por USB.
4. Compila y carga el código con el comando:
   `idf.py build flash monitor`
5. Abre la consola de depuración: verás lecturas continuas cada segundo en el monitor serie con el valor entero que entrega el sensor (por ejemplo: `Read successful - Value: 55`).

---

Prueba con el cisterna
- I (23074) QDY30A_RS485: Read successful - Value: 0
- I (24404) QDY30A_RS485: Read successful - Value: 0
- I (25734) QDY30A_RS485: Read successful - Value: 0
- I (27064) QDY30A_RS485: Read successful - Value: 0
- I (28394) QDY30A_RS485: Read successful - Value: 0
- I (29724) QDY30A_RS485: Read successful - Value: 97
- I (31054) QDY30A_RS485: Read successful - Value: 311
- I (32384) QDY30A_RS485: Read successful - Value: 447
- I (33714) QDY30A_RS485: Read successful - Value: 726
- I (35044) QDY30A_RS485: Read successful - Value: 964
- I (36374) QDY30A_RS485: Read successful - Value: 928
- I (37704) QDY30A_RS485: Read successful - Value: 964
- I (39024) QDY30A_RS485: Read successful - Value: 1040
- I (40344) QDY30A_RS485: Read successful - Value: 1114
- I (41664) QDY30A_RS485: Read successful - Value: 1086
- I (42994) QDY30A_RS485: Read successful - Value: 1117
- I (44324) QDY30A_RS485: Read successful - Value: 1116
- E (45624) QDY30A_RS485: Timeout or incomplete response (0 bytes received)
- E (45624) QDY30A_RS485: Sensor read error: ESP_ERR_TIMEOUT
- E (46924) QDY30A_RS485: Timeout or incomplete response (0 bytes received)
- E (46924) QDY30A_RS485: Sensor read error: ESP_ERR_TIMEOUT
- E (48224) QDY30A_RS485: Timeout or incomplete response (0 bytes received)
- E (48224) QDY30A_RS485: Sensor read error: ESP_ERR_TIMEOUT
- E (49524) QDY30A_RS485: Timeout or incomplete response (0 bytes received)
- E (49524) QDY30A_RS485: Sensor read error: ESP_ERR_TIMEOUT
- E (50824) QDY30A_RS485: Timeout or incomplete response (0 bytes received)
- E (50824) QDY30A_RS485: Sensor read error: ESP_ERR_TIMEOUT
- E (52124) QDY30A_RS485: Timeout or incomplete response (0 bytes received)
- E (52124) QDY30A_RS485: Sensor read error: ESP_ERR_TIMEOUT
- E (53424) QDY30A_RS485: Timeout or incomplete response (0 bytes received)
- E (53424) QDY30A_RS485: Sensor read error: ESP_ERR_TIMEOUT
- I (54754) QDY30A_RS485: Read successful - Value: 1114
- I (56384) QDY30A_RS485: Read successful - Value: 1114
- I (57714) QDY30A_RS485: Read successful - Value: 1114
- I (59034) QDY30A_RS485: Read successful - Value: 1113
- I (60354) QDY30A_RS485: Read successful - Value: 1113
- I (61674) QDY30A_RS485: Read successful - Value: 1114
- I (62994) QDY30A_RS485: Read successful - Value: 1114
- I (64314) QDY30A_RS485: Read successful - Value: 1114
- I (65634) QDY30A_RS485: Read successful - Value: 1114
- I (66964) QDY30A_RS485: Read successful - Value: 1115
- I (68294) QDY30A_RS485: Read successful - Value: 1114
- I (69624) QDY30A_RS485: Read successful - Value: 1111
- I (70954) QDY30A_RS485: Read successful - Value: 1109
- I (72284) QDY30A_RS485: Read successful - Value: 1109
- I (73614) QDY30A_RS485: Read successful - Value: 1110
- I (74944) QDY30A_RS485: Read successful - Value: 1109
- I (76274) QDY30A_RS485: Read successful - Value: 1110
- I (77604) QDY30A_RS485: Read successful - Value: 1110
- I (78934) QDY30A_RS485: Read successful - Value: 1109
- I (80254) QDY30A_RS485: Read successful - Value: 1110
- I (81574) QDY30A_RS485: Read successful - Value: 1109
- I (82894) QDY30A_RS485: Read successful - Value: 1110
- I (84214) QDY30A_RS485: Read successful - Value: 1050
- I (85534) QDY30A_RS485: Read successful - Value: 732
- I (86864) QDY30A_RS485: Read successful - Value: 704
- I (88194) QDY30A_RS485: Read successful - Value: 711
- I (89524) QDY30A_RS485: Read successful - Value: 668
- I (90854) QDY30A_RS485: Read successful - Value: 681
- I (92184) QDY30A_RS485: Read successful - Value: 645
- I (93514) QDY30A_RS485: Read successful - Value: 640
- I (94844) QDY30A_RS485: Read successful - Value: 700
- I (96174) QDY30A_RS485: Read successful - Value: 702
- I (97504) QDY30A_RS485: Read successful - Value: 713
- I (98834) QDY30A_RS485: Read successful - Value: 728
- I (100164) QDY30A_RS485: Read successful - Value: 803
- I (101484) QDY30A_RS485: Read successful - Value: 870
- I (102804) QDY30A_RS485: Read successful - Value: 915
- I (104124) QDY30A_RS485: Read successful - Value: 942
- I (105444) QDY30A_RS485: Read successful - Value: 986
- I (106764) QDY30A_RS485: Read successful - Value: 1042
- I (108094) QDY30A_RS485: Read successful - Value: 1104
- I (109424) QDY30A_RS485: Read successful - Value: 1109
- I (110754) QDY30A_RS485: Read successful - Value: 1108
- I (112084) QDY30A_RS485: Read successful - Value: 918
- I (113414) QDY30A_RS485: Read successful - Value: 763
- I (114744) QDY30A_RS485: Read successful - Value: 796
- I (116074) QDY30A_RS485: Read successful - Value: 806
- I (117404) QDY30A_RS485: Read successful - Value: 793
- I (118734) QDY30A_RS485: Read successful - Value: 813
- I (120064) QDY30A_RS485: Read successful - Value: 828
- I (121384) QDY30A_RS485: Read successful - Value: 814
- I (122704) QDY30A_RS485: Read successful - Value: 762
- I (124024) QDY30A_RS485: Read successful - Value: 723
- I (125344) QDY30A_RS485: Read successful - Value: 522
- I (126674) QDY30A_RS485: Read successful - Value: 278
- I (127994) QDY30A_RS485: Read successful - Value: 91
- I (129324) QDY30A_RS485: Read successful - Value: 0
- I (130654) QDY30A_RS485: Read successful - Value: 0
- I (131984) QDY30A_RS485: Read successful - Value: 0

