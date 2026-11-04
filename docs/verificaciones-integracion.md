# Verificaciones de la entrega

Alcance: software de `Jesus/integracion-cabina`, ambos nodos ESP32-S3. **No se flasheó ni se probaron físicamente memoria, alimentación, ejes, GPS o recorrido.** La [guía de vehículo](README-prueba-vehiculo.md) separa diagnóstico y confirmación física.

## Resultados conservados y reproducibilidad

El [resumen legible por máquina](../pruebas/modelo-a/verificaciones-entrega.json) publica resultados, hashes de las fuentes protegidas y de configuraciones/binarios verificados. Son huellas de los artefactos locales de referencia; no se requiere disponer de ellos para compilar. Una compilación nueva puede producir un hash de imagen distinto por ruta, fecha o revisión Git. Los resultados completos (stdout, stderr, ejecutables, manifests y fallos anteriores) siguen conservados localmente; no se suben copias de build ni dependencias descargadas.

| Comprobación | Resultado / alcance |
|---|---|
| Cabina S3 normal, virtual100, virtual50 | Compilación completa, salida 0 en los tres perfiles |
| Trasero S3 normal, capture100, capture50 | Compilación completa, salida 0 en los tres perfiles |
| Suite host previa, 16 pasos | Salida 0, sin timeout; geometría/dinámica, integración, continuidad, virtual, banco, sensibilidad, códec y analizadores |
| GPIO/controladores SPI | 132 casos de guard (44 × 3 configuraciones) |
| Variantes de configuración S3 | 7 compilaciones host, salida 0 |
| Decoder de captura | 6 pruebas, salida 0; incluye CRC/truncado/contadores |
| Agua desconectada | Fixture códec de ausencia; sin inventar lectura/timestamp |
| Generador desde copia sin sdkconfig | Regresión nueva: perfiles 100/50, S3/OPI/UART y preservación de montaje editado |
| Copia limpia: cabina virtual100 / trasero capture100 | Dos compilaciones completas nuevas, ambas salida 0, sin copiar sdkconfig/build/managed_components; flags efectivos comprobados |

Los hashes de las entradas actuales se cotejan con el manifiesto anterior para reutilizar pruebas sin repetir matemáticas no modificadas. En este cierre sólo cambian documentación, exclusiones, bloqueo de dependencias y dos herramientas offline: el generador admite una copia sin sdkconfig, y la conversión de imágenes exige rutas de entrada explícitas en lugar de una carpeta privada. No se cambian protocolo, ecuaciones ni firmware de adquisición.

ESP-IDF utilizado: 6.1.0, revisión registrada en el JSON. Prerrequisitos y comandos completos de build desde copia nueva: [guía](README-prueba-vehiculo.md#d-preparar-compilar-y-cargar-manualmente). El driver BNO/SH2/ETL se distribuye como fuente con licencias, LVGL/ST7796 mediante manifiesto y lock; no dependen de carpetas managed_components locales.

## Repetir comprobaciones por software

Desde raíz, Python y GCC disponibles:

```powershell
python pruebas/modelo-a/run-all.py
python pruebas/modelo-a/test_cableado_cabina.py
python pruebas/modelo-a/compile-variants.py
python pruebas/modelo-a/test_preparar_captura.py
git diff --check
```

`run-all.py` conserva stdout/stderr/código/timeout y hashes de ejecutables en `pruebas/modelo-a/build/logs/`. Para conservar series distintas, copiar la carpeta de resultados antes de repetir. Los fallos históricos no se reclasifican como PASS: [investigación de integration.exe](fallo-integration.md). También se conserva el fallo inicial del cierre por permisos de tuberías Windows (WinError 5), distinto de compilación/C y de hardware; sólo la ejecución posterior completa acredita compilación.

Los builds ESP32 clásico y las primeras expectativas erróneas de cabecera QIO permanecen como historial, no acreditan S3. QIO configurado con cabecera DIO es el comportamiento documentado del SDK, no una prueba de la memoria física.

## Qué queda fuera de Git y qué falta físicamente

Se conservan localmente build y sus variantes, sdkconfig efectivos/backups, managed_components, cachés, logs, binarios/ELF y capturas de ensayo. Se publican defaults, Kconfig, locks, generadores, fuentes, recursos/licencias y resumen pequeño. No se eliminan archivos locales ni se deja de rastrear material histórico de otras áreas. Las futuras capturas en `/capturas/` quedan ignoradas para evitar publicar datos GPS y grandes series por accidente.

`.gitattributes` limita las excepciones de whitespace a snapshots de terceros, licencia y archivos importados con espacios heredados; evita reformatearlos y perder su correspondencia con las evidencias. No desactiva la comprobación para el repositorio completo.

Pendiente: identificar placa/memoria y alimentación de carriers; cablear cabina S3; comprobar signos/rotación y calibración; obtener GPS utilizable; medir tasas reales, integridad de captura, parejas, pérdidas, desarmados/tiempo sin resultado y rearme en ensayo. Ni sensibilidad numérica ni semáforo verde validan predicción de volcadura.
