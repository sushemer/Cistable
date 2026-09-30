# Sistema de Monitoreo de Estabilidad Vehicular (LTR) con ESP32-S3 y 3x BNO085

Este proyecto implementa el firmware en **ESP-IDF (v5.x)** en lenguaje **C++** para un **ESP32-S3**. Su propósito es leer simultáneamente **3 sensores IMU GY-BNO085** a través de un bus **SPI** compartido, calcular el índice de riesgo de volcadura **LTR (Load Transfer Ratio)** en tiempo real y permitir la calibración dinámica a cero (Tare).

---

## 📌 Características Principales

- **Gestión Multi-IMU por SPI:** Lectura sincrónica de 3 sensores BNO085 (Cabina, Tanque Izquierdo y Tanque Derecho) utilizando líneas independientes de Chip Select (CS), Interrupción (HINT) y Reset (RST).
- **Procesamiento de Datos:**
  - *Game Rotation Vector:* Obtención de ángulos Euler (*Roll* y *Pitch*) sin interferencia magnética.
  - *Linear Acceleration:* Extracción de la aceleración lateral ($a_y$) filtrada mecánicamente por la IMU.
- **Algoritmo LTR (Load Transfer Ratio):** Cálculo continuo de transferencia de carga para detectar riesgo inminente de volcadura ($\vert{}LTR\vert{} > 0.6$).
- **Calibración Dinámica (Tare):** Re-orientación del "punto cero" enviando el comando `'t'` por terminal UART.

---

## 🛠️ Instalación e Integración de la Librería (`esp32_BNO08x`)

### ⚠️ ¿Por qué no debes subir la carpeta `components/` a GitHub?
Si intentas subir la carpeta `components/esp32_BNO08x` directamente por la interfaz web de GitHub, fallará porque contiene **más de 100 archivos y submódulos anidados** (como la librería oficial SH2 de CEVA). 

Para solucionar esto y mantener tu repositorio limpio, utiliza **Submódulos de Git**.

### Pasos para Configurar la Librería en tu Proyecto Local:

1. **Ignorar archivos generados en Git:**
   Asegúrate de tener un archivo `.gitignore` en la raíz de tu proyecto con la siguiente configuración:
   ```gitignore
   build/
   managed_components/
   sdkconfig
Añadir la librería como Submódulo de Git:
Abre la terminal en la raíz de tu proyecto y ejecuta:

Bash
git submodule add [https://github.com/myles-parfeniuk/esp32_BNO08x.git](https://github.com/myles-parfeniuk/esp32_BNO08x.git) components/esp32_BNO08x
git submodule update --init --recursive
Esto conectará la librería directamente al repositorio original sin saturar tu historial de cambios en GitHub.

Si alguien más clona tu proyecto de GitHub, debe ejecutar:

Bash
git clone --recursive <URL_DE_TU_REPOSITORIO>
⚙️ Configuración del Entorno ESP-IDF (C++)
Dado que la librería está escrita en C++, debes asegurarte de que tu proyecto esté configurado para C++:

Renombra el archivo en la carpeta main/: de main.c a main.cpp.

Actualiza main/CMakeLists.txt para que registre el archivo .cpp:

CMake
idf_component_register(SRCS "main.cpp"
                       INCLUDE_DIRS ".")
Asegúrate de incluir extern "C" en la función principal dentro de main.cpp:

C++
extern "C" void app_main(void) {
    // Punto de entrada
}
🔌 Conexión de Hardware (Pinout SPI)
Todos los sensores comparten el bus SPI principal, pero requieren pines individuales de control:

Señal	Pin ESP32-S3	IMU 1 (Cabina)	IMU 2 (Tanque Izq)	IMU 3 (Tanque Der)
SCK	GPIO 12	SCL	SCL	SCL
MOSI	GPIO 11	SDA / DI	SDA / DI	SDA / DI
MISO	GPIO 13	SDO / DO	SDO / DO	SDO / DO
CS	Independiente	GPIO 10	GPIO 14	GPIO 17
HINT (INT)	Independiente	GPIO 9	GPIO 21	GPIO 18
RST	Independiente	GPIO 3	GPIO 4	GPIO 5
VCC / GND	3.3V / GND	3.3V / GND	3.3V / GND	3.3V / GND
🧮 Algoritmo LTR (Load Transfer Ratio)
El índice LTR evalúa el desequilibrio de carga entre las ruedas izquierdas y derechas del vehículo. Se define mediante la fórmula:

LTR= 
T⋅g
2⋅h⋅a 
y_efectiva
​
 
​
 
Donde:

h: Altura del centro de gravedad (1.8 m).

T: Ancho de vía o chasis (2.2 m).

g: Aceleración de la gravedad (9.81 m/s 
2
 ).

a 
y_efectiva
​
 : Aceleración lateral ajustada por la inclinación (Roll):

a 
y_efectiva
​
 =a 
y_promedio
​
 ⋅cos(ϕ)+g⋅sin(ϕ)
Interpretación de Valores LTR:
LTR=0.0: Vehículo totalmente equilibrado.

LTR=±0.6: Umbral de Alerta. Transferencia de carga severa.

LTR=±1.0: Volcadura inminente. Las ruedas de un lado se han despegado del suelo.

🚀 Compilación y Ejecución
Compilar el proyecto:

Bash
idf.py build
Flashear al ESP32-S3 y abrir el monitor serie:

Bash
idf.py -p PORT flash monitor
📊 Formato de Salida (Serial Plotter)
La salida por consola se emite en formato CSV a 100 Hz, lista para usarse con herramientas como el Serial Plotter de Arduino o PlotJuggler:

Fragmento de código
Roll_Cabina, Roll_Tanque_Izq, Roll_Tanque_Der, Ay_Promedio, LTR_Calculado
0.12, 0.15, 0.10, 0.02, 0.003
🎯 Calibración a Cero (Tare)
Con el vehículo en una superficie plana, envía el carácter 't' por la terminal UART del monitor serie. El sistema enviará una orden de Tare a los 3 sensores, redefiniendo la orientación actual como 0.00 
∘
  en todos sus ejes.