# Primer flasheo de cabina S3: pila de app_main insuficiente

En el primer ensayo de carga en COM8 se identificó ESP32-S3 revisión v0.2,
Flash física de 16 MB Quad y PSRAM de 8 MB. El firmware `virtual100` compiló
y esptool verificó la escritura, pero **el arranque falló**. No equivale a una
prueba física aprobada del prototipo.

La PSRAM se detectó a 80 MHz y su test SRAM indicó OK. Después de
`Initialize SPI bus` / `Install panel IO` apareció `LoadProhibited`,
con `EXCVADDR=0xbcb46989`, y reinicios repetidos. No hubo registros de captura
de aplicación: el fallo precede a `cap_start()`.

La traza decodificada con el ELF exacto pasó por `find_desc_for_source`,
`esp_intr_alloc_info`, `spi_master_init_driver`, `esp_lcd_new_panel_io_spi`,
`display_init`, `pantalla_init` y `app_main`. La lista de descriptores de
interrupción ya estaba corrupta al llegar a la pantalla.

El desensamblado del binario fallido muestra estos marcos anidados de pila:

| Función | Bytes reservados por `entry a1` |
|---|---:|
| app_main | 1712 |
| ci_live_model_config | 928 |
| ci_virtual_derive | 800 |
| ma_depth_at_volume | 144 |
| ma_water_at_depth | 160 |
| **Subtotal de esta cadena** | **3744** |

Esto excede los **3584 bytes** configurados para la tarea principal, incluso
sin incluir wrapper, funciones matemáticas y otras llamadas. El desbordamiento
puede corromper memoria antes de que FreeRTOS compruebe el canario al cambiar
de tarea; el punto de caída posterior no identifica el origen de la escritura.

Corrección: `CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192` en defaults de **cabina** y
sus sdkconfig S3 locales. No cambia buffers de radio/captura, particiones,
ecuaciones, pinout ni pila del trasero. La regresión del generador comprueba
que una copia nueva conserva esta asignación. Los perfiles ya generados deben
recompilarse; no basta con cambiar defaults.

La comprobación de la corrección requiere reconstrucción, carga y observación
de arranque sin panic. El margen dinámico de pila bajo carga máxima y los
sensores/ejes siguen necesitando ensayo; ampliar la pila no valida el modelo.

## Resultado de la versión corregida en esta cabina

Se recompiló y cargó `virtual100` en COM8; esptool verificó el hash de escritura.
La observación de 20 segundos después de RESET tuvo una sola entrada a app_main,
sin panic ni reinicios. `HW_OBSERVED` registró Flash 16777216 bytes y PSRAM
8388608 bytes inicializada; el memtest PSRAM indicó OK.

`HW_STACK` registró 8192 bytes asignados y mínimo libre 4212: uso observado
3980 bytes en inicialización, superior a los 3584 anteriores. Este registro
se conserva en la captura. No establece el máximo de una futura prueba dinámica.

El BNO085 de cabina no respondió a `sh2_getProdIds()` y quedó offline; no llegaron
mediciones CST2 del trasero. Por tanto **arranque corregido no significa sensores
o integración completa aprobados**. Montaje permanece sin confirmar, agua 50 %
configurada, corrección de posición OFF y modelo sin armar. No se enviaron f/s/a.

Las evidencias completas se conservan localmente en
`pruebas/modelo-a/build/operacion-cabina/`: logs de build/flash/identificación,
raw de arranque y decoder. `fallo-arranque-original/` conserva binario, ELF,
sdkconfig y fuentes antes de corregir. No sobrescribir ni reclasificar la
captura fallida como PASS. La ejecución corregida se registra por separado.
