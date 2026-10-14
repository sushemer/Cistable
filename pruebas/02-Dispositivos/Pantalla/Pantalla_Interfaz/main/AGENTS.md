# Contexto del proyecto

Este proyecto forma parte de la materia Proyecto de Carrera.

Se está desarrollando un sistema para pipas/camiones cisterna de Bomberos de Tijuana.

## Objetivo del sistema

El sistema debe ayudar a prevenir volcaduras mediante:

- medición de variables del vehículo con sensores;
- cálculo de riesgo de volcadura;
- clasificación del riesgo como:
  - VERDE
  - AMARILLO
  - ROJO
- medición del porcentaje de llenado de la cisterna.

## Pantalla

Se utilizará una pantalla TFT de 3.5 pulgadas:

- controlador ST7796S;
- resolución nativa 320x480;
- comunicación SPI;
- se usará en orientación horizontal 480x320;
- no se requiere touch.

## Comportamiento de la interfaz

Cuando el camión esté en movimiento:

- mostrar únicamente el nivel de riesgo de volcadura;
- usar colores grandes y visibles;
- estados:
  - VERDE = SEGURO
  - AMARILLO = PRECAUCIÓN
  - ROJO = PELIGRO
- evitar información innecesaria que distraiga al conductor.

Cuando el camión esté detenido:

- mostrar únicamente el nivel de llenado de la cisterna;
- mostrar porcentaje grande;
- mostrar una barra de progreso;
- usar azul para representar agua.

También debe existir:

- pantalla de inicio;
- pantalla de error o "SIN DATOS".

## Plataforma

- ESP32 clásico
- ESP-IDF v6.1
- lenguaje C
- FreeRTOS
- VS Code con extensión ESP-IDF

## Arquitectura deseada

Separar el proyecto así:

main/
├── main.c
├── display/
│   ├── display.c
│   └── display.h
├── interfaz/
│   ├── interfaz.c
│   └── interfaz.h
└── CMakeLists.txt

Responsabilidades:

### main.c
- lógica general;
- simulación temporal;
- posteriormente integración de sensores.

### display.c
- inicialización SPI;
- inicialización ST7796S;
- inicialización esp_lcd;
- inicialización LVGL;
- manejo del display.

### interfaz.c
- pantalla de inicio;
- pantalla de riesgo;
- pantalla de nivel;
- pantalla de error;
- funciones gráficas.

## Librerías/componentes

Preferir componentes oficiales de ESP-IDF y Espressif.

Se desea utilizar:

- esp_lcd
- espressif/esp_lcd_st7796
- LVGL
- esp_lvgl_port si resulta conveniente

No usar Arduino.

## Estado actual

Se quiere partir de un ejemplo oficial de ESP-IDF, preferiblemente:

examples/peripherals/lcd/spi_lcd_touch

y adaptarlo.

El ejemplo debe compilar primero sin modificaciones antes de comenzar a cambiar el controlador.

Después:

1. agregar dependencia ST7796S;
2. eliminar funcionalidad touch;
3. cambiar controlador ST7789 por ST7796S;
4. configurar resolución 320x480;
5. rotar a 480x320;
6. reorganizar código;
7. implementar interfaz;
8. crear una simulación de estados.

## Simulación deseada

La simulación debe cambiar automáticamente:

INICIO
→ VERDE
→ AMARILLO
→ ROJO
→ CAMIÓN DETENIDO / NIVEL DE AGUA
→ ERROR
→ repetir

El porcentaje de agua puede simularse de 0 a 100%.

## Estilo de programación

- código claro y sencillo;
- adecuado para un estudiante que está aprendiendo ESP32;
- evitar abstracciones innecesarias;
- explicar cualquier cambio importante;
- no modificar varios subsistemas al mismo tiempo;
- compilar después de cada cambio importante;
- respetar la estructura de ESP-IDF;
- mantener CMakeLists.txt e idf_component.yml correctos.

## Importante

No asumir los GPIO definitivos de la pantalla hasta revisar el módulo físico.

Si falta información sobre hardware, preguntar antes de modificar conexiones o configuración de GPIO.

Antes de hacer cambios grandes:
1. inspeccionar el proyecto actual;
2. explicar qué archivos se modificarán;
3. hacer cambios pequeños;
4. ejecutar idf.py build;
5. corregir errores antes de continuar.