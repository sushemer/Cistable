# Prueba 01 del GPS 6M V2
Primera prueba del GPS de manera controlada hasta cierto punto. 

## Resultados principales
- Las pruebas de ventana fueron las más estables.
- En patio y carro aparecieron saltos de posición, interrupciones y lecturas con tiempos irregulares.
- Patio 02 presentó HDOP de hasta `99.99`, calidad GGA `0` y un pico de velocidad de aproximadamente `57 km/h`.
- `prueba_01_carro_03.xlsx` y `prueba_01_patio_01.xlsx` están dañados y deben repetirse.
- El análisis debe realizarse con los archivos individuales; el resumen solo sirve como referencia general.

## Archivos

```text
prueba_01/
├── pruebas/       # Archivos Excel de cada prueba
├── analisis/      # Análisis y resultados consolidados
├── firmware/      # Proyecto ESP-IDF de esta prueba
└── README.md
```

