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
