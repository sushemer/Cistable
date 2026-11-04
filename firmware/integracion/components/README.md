# Componentes compartidos ESP-IDF

Adaptadores de hardware, radio y captura de ambos nodos S3. No sustituir por versiones incompatibles sin verificar.

## Archivos y responsabilidades

imu_bno085: reportes/normalización y rechazo de cuaternión; field_espnow: radio/callbacks/buzón; captura: CRC/escritor; esp32_BNO08x: instantánea de tercero.

## Entradas, salidas y dependencias

BNO enlaza SH2/ETL incluidos: preservar MIT de Myles Parfeniuk, MIT ETL/John Wellbelove y Apache-2.0 SH2/Hillcrest. No son managed_components. Captura CSTCAP01/CRC32 es distinta de CST2 y CRC16 Modbus.

## Uso y estado

CMake de ambos proyectos descubre estos componentes. Host: test_captura_completa.py y test_cableado_cabina.py desde pruebas/modelo-a.

Implementados: real (bloqueado si incompleto), PIPA_VIRTUAL_ENTRADAS_REALES y demo sintética diferenciados. Comprobado por software: matemáticas, continuidad, captura/configuración y seis builds S3. Pendiente físicamente: inscripción/memoria, alimentación, cableado, ejes/calibración, GPS, tasas y recorrido. No se flasheó en esta preparación.

[Guía completa de prueba](../../../docs/README-prueba-vehiculo.md) · [Hardware S3](../../../docs/hardware-esp32s3.md) · [Parámetros/fuentes/sensibilidad](../../../docs/pipa-virtual.md) · [Verificaciones](../../../docs/verificaciones-integracion.md) · [Procedencia de compañeros](../procedencia.json).

## Qué datos puedo cambiar y dónde

[Tabla completa de valores, componentes y signos](../../../docs/README-prueba-vehiculo.md#c-qué-datos-puedo-cambiar-y-dónde). Cabina: `Cistable integration (experimental)` en [Kconfig.projbuild](../cabina/main/Kconfig.projbuild); submenús `Parametros fisicos PIPA VIRTUAL` y `Montaje IMU trasero`. Las abreviaturas conservan el prefijo CI_VIRTUAL/CI_REAL. **Todos requieren recompilar/flashear manualmente** el nodo afectado.

| Parámetro / significado | Unidad / referencia | Menú o archivo exacto | Valor inicial | Perfil | Recompilar/flashear |
|---|---|---|---|---|---|
| Masa seca operacional | kg; sin agua | CI_VIRTUAL_CHASSIS/SHELL/PLATFORM/DIESEL/CREW_MASS_KG | 5000+924+600+173.888+200=6897.888 | Virtual | Sí |
| CG seco / vía | m, XYZ delante/izquierda/arriba; Z suelo | CI_VIRTUAL_*_CG_M, OVERRIDE_CG, CG_X_M/CG_Y_M/CG_Z_M; TRACK_M | (0,0,1.129996660); 1.81 | Virtual | Sí |
| Geometría W/H/L y fondo | m | CI_VIRTUAL_WIDTH_M/HEIGHT_M/LENGTH_M/BOTTOM_M | 2.210475/1.210475/3.030475; 1.1647625 | Virtual | Sí |
| Agua configurada por volumen | % | CI_VIRTUAL_FILL_PERCENT | 50; dominio 20..80 | Virtual | Sí |
| Rotación IMU | grados, Rz Ry Rx | CI_VIRTUAL_ROLL_DEG/PITCH_DEG/YAW_DEG; MOUNT_CONFIRMED | 0/0/0; n pendiente | Automóvil | Sí |
| Corrección/vector XYZ | mm, origen al IMU | CI_VIRTUAL_LEVER; R_X_MM/R_Y_MM/R_Z_MM | n; 0/0/0 sin efecto OFF | Automóvil | Sí |
| Brazo pipa real independiente | mm, centro cisterna al IMU | CI_REAL_LEVER; R_X_MM/R_Y_MM/R_Z_MM | y; +640/-420/-940 | Real bloqueado | Sí |
| Péndulo: amortiguamiento/masa/amplitud | 1 / escala / mrad | CI_VIRTUAL_ZETA/MASS_SCALE/MAX_ANGLE_MRAD | 0.05 / 1 / 200; frecuencia/masas/longitud derivados | Virtual | Sí |
| Cada reporte IMU / ESP-NOW | Hz solicitados / envíos/s máximo | preparar-captura.py --virtual --rate 100 o 50; CI_BENCH_50HZ ambos | 100 o 50; nuevos datos y callback | Ambos | Sí |
| Cálculo / pantalla | parejas nuevas / Hz | control_cabina.c / cabina/main/app_main.c: now-last_ui>=250000 | dt real / 4 | Cabina | Sí |
| Resumen consola | ms | CI_DIAG_PERIOD_MS; cabina/main/modules/gps/driver/gps_config.h: GPS_LOG_INTERVAL_MS | 1000 (1 Hz) | Cabina | Sí |

Montaje es del automóvil; masas/geometría son de la pipa virtual. Vector sin efecto con corrección OFF. Geometría/llenado recalculan dependencias e invalidan historia; exigen rearme. El CG se deriva por componentes salvo override. Reportes, radio, cálculo, pantalla y consola tienen frecuencias distintas: no cambiar sólo envío sin revisar adquisición/continuidad. Estimaciones y f/s/a mantienen clasificación experimental.
