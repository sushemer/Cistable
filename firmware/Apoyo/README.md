# Componentes comunes del firmware

Esta carpeta contiene los componentes compartidos por los proyectos ESP-IDF del repositorio.

| Componente | Función |
|---|---|
| `field_espnow/` | Comunicación ESP-NOW y entrega de telemetría al master. |
| `imu_bno085/` | Adaptador del proyecto para iniciar la BNO085 y obtener sus muestras. |
| `esp32_BNO08x/` | Biblioteca externa para el sensor BNO080/BNO085; se fija mediante un submódulo Git. |

## Preparar la biblioteca BNO08x

Al clonar Cistable por primera vez, se debe descargar los submódulos con:

```powershell
git submodule update --init --recursive
```

También se puede clonar todo de una vez:

```powershell
git clone --recurse-submodules https://github.com/sushemer/Cistable.git
```

Si ya se clonó el repositorio sin submódulos, basta con ejecutar el primer comando desde la raíz de Cistable. La biblioteca queda disponible en `firmware/Apoyo/esp32_BNO08x/`; no es necesario copiarla manualmente ni instalarla por separado.

El repositorio principal fija la revisión exacta de la biblioteca. Para actualizarla intencionalmente, desde la raíz:

```powershell
git submodule update --remote firmware/Apoyo/esp32_BNO08x
```

Después se debe revisar y guardar el nuevo puntero del submódulo en un commit del repositorio principal.
