# API de pantalla para integrar desde main

`pantalla.c/.h` encapsulan el display y la UI ya probados. No contienen
`app_main`, simulacion, sensores, calculo LTR, WiFi ni ESP-NOW.
No modifican la apariencia, GPIO, buffers o stack del display.

## Funciones

| Funcion | Uso |
|---|---|
| `pantalla_init()` | Inicializar una vez y mostrar INICIO. |
| `pantalla_mostrar_inicio()` | Volver a INICIO. |
| `pantalla_mostrar_estado(&estado)` | Mostrar riesgo, nivel o SIN DATOS. |

Las funciones de presentacion toman internamente el mutex LVGL. Llamarlas
desde una tarea normal, sin tener ese mutex tomado; no desde un callback
LVGL, WiFi o una interrupcion. El llamador debe mantener el estado estable
durante la llamada. La API no conserva su direccion.

La inicializacion debe terminar antes de arrancar otras tareas consumidoras.
Una segunda inicializacion devuelve ESP_ERR_INVALID_STATE. Se conserva el
manejo fatal de errores de hardware del display existente: no se promete
recuperacion de errores de SPI ni de asignacion de memoria.

## Ejemplo en el unico app_main del firmware integrado

```c
#include "pantalla.h"

void app_main(void)
{
    ESP_ERROR_CHECK(pantalla_init());
    estado_sistema_t estado = {
        .modo = MODO_DETENIDO,
        .riesgo = RIESGO_SEGURO,
        .nivel_agua_pct = 78
    };
    ESP_ERROR_CHECK(pantalla_mostrar_estado(&estado));
    // Iniciar aqui las tareas de GPS y recepcion del firmware integrado.
}
```

Movimiento usa MODO_EN_MOVIMIENTO y RIESGO_SEGURO, RIESGO_PRECAUCION o
RIESGO_PELIGRO. Detenido exige nivel 0..100. MODO_SIN_DATOS ignora riesgo y
nivel. Un estado invalido muestra SIN DATOS y devuelve ESP_ERR_INVALID_ARG;
en operacion normal gestionar ese retorno sin abortar por datos externos.
Los valores internos son los de estado_sistema.h: no son codigos del cable.

## Archivos necesarios y compilacion

Estos dos archivos NO son un driver autonomo. En el proyecto integrado
registrar pantalla.c y las fuentes existentes:

- `Pantalla_Interfaz/main/display/display.c` y su header.
- `Pantalla_Interfaz/main/interfaz/interfaz.c` y su header.
- `Pantalla_Interfaz/main/estado_sistema.h` (una sola definicion compartida).

Agregar a INCLUDE_DIRS esta carpeta, main del proyecto de pantalla,
main/display y main/interfaz. Conservar las dependencias y configuracion
LVGL del proyecto probado (incluidas sus fuentes tipograficas), driver
ST7796 y ajustes de hardware. No copiar solo estos dos archivos esperando
que LVGL y el driver se incorporen automaticamente.

No compilar el main.c de la demo junto al main integrado, ni llamar tambien
a display_init/interfaz_init: pantalla_init ya los llama. Las fuentes de
respaldo Interfaz.c y spi_lcd_touch_example_main.c tampoco deben incluirse.

El integrador sigue siendo responsable de validar recepcion, antiguedad,
modo y riesgo, y de llamar a pantalla_mostrar_estado cuando corresponda.
La pantalla no decide movimiento a partir del GPS ni calcula LTR.

Se entrega como capa optativa: la demo actual y su CMake permanecen intactos.
La compilacion completa y la prueba fisica del firmware integrado se deben
realizar cuando se registre esta capa en ese firmware.
