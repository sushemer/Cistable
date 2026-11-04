# Modelo A: geometría y dinámica reducida

Biblioteca C pura: geometría elíptica, fuerzas, momentos, péndulo equivalente y LTR. Derivación reducida experimental, no reproducción verificada de Zheng.

## Archivos y responsabilidades

modelo_a.c: volumen/centroide, orientación, traslado, masas/LTR; dinamica.c: integrador/fuerza del péndulo; include/modelo_a.h: estados y contratos SI.

## Entradas, salidas y dependencias

Entradas SI finitas/estados/frescura; salidas ma_status, geometría, dinámica y LTR. MA_LTR_LIMIT conserva valor, distinto de amplitud fuera de dominio, error numérico o sin apoyo. Controlador decide presentación/rearme.

## Uso y estado

Desde raíz: `python pruebas/modelo-a/run-all.py`, o run-host.ps1 para núcleo geométrico/dinámico. Requiere Python y GCC, sin hardware.

Implementados: real (bloqueado si incompleto), PIPA_VIRTUAL_ENTRADAS_REALES y demo sintética diferenciados. Comprobado por software: matemáticas, continuidad, captura/configuración y seis builds S3. Pendiente físicamente: inscripción/memoria, alimentación, cableado, ejes/calibración, GPS, tasas y recorrido. No se flasheó en esta preparación.

[Guía completa de prueba](../../../docs/README-prueba-vehiculo.md) · [Hardware S3](../../../docs/hardware-esp32s3.md) · [Parámetros/fuentes/sensibilidad](../../../docs/pipa-virtual.md) · [Verificaciones](../../../docs/verificaciones-integracion.md) · [Procedencia de compañeros](../../integracion/procedencia.json).

## Qué datos puedo cambiar y dónde

[Tabla completa de valores, componentes y signos](../../../docs/README-prueba-vehiculo.md#c-qué-datos-puedo-cambiar-y-dónde). Cabina: `Cistable integration (experimental)` en [Kconfig.projbuild](../../integracion/cabina/main/Kconfig.projbuild); submenús `Parametros fisicos PIPA VIRTUAL` y `Montaje IMU trasero`. Las abreviaturas conservan el prefijo CI_VIRTUAL/CI_REAL. **Todos requieren recompilar/flashear manualmente** el nodo afectado.

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
