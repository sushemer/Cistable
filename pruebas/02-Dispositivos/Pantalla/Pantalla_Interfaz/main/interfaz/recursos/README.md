# Recursos de seleccion

- `camion_pequeno`: silueta suministrada como `Camion 1 (Modelo A)`.
- `camion_grande`: silueta suministrada como `Camion 2 (Modelo B)`.
- Cada imagen mide 160x90, conserva transparencia y proporciones y se almacena
  en flash como ARGB8888 (bytes B, G, R, A). Los PNG permiten revisar el resultado;
  el firmware solo compila los archivos C, sin decodificador PNG.
- `fuente_enie.c` contiene solo U+00F1, obtenido de Montserrat Medium 24 px
  incluido en LVGL. Es un fallback exclusivo de los nombres de las tarjetas.
  Su licencia esta en `LICENCIA_Montserrat.txt`.

Para regenerar en Windows, ejecutar `generar_recursos.ps1` con los parametros
`-ModeloA` y `-ModeloB` apuntando a las imagenes originales. Usa System.Drawing
y la fuente presente en `managed_components/lvgl__lvgl/scripts/built_in_font/`.
No se ejecuta como parte de la compilacion de ESP-IDF.
