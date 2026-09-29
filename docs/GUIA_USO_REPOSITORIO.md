# Guía de uso del repositorio

## Objetivo
Esta guía define la forma de trabajo del equipo para mantener el repositorio organizado, evitar conflictos y asegurar que todos los cambios sean revisados antes de integrarse al proyecto.

## Ramas principales

### `main`
La rama `main` contiene únicamente versiones estables y validadas.
- No se debe trabajar directamente sobre `main`.
- No se permiten commits directos.
- No se permiten pushes directos.
- Los cambios deben llegar mediante un Pull Request.
- Todo Pull Request debe ser revisado y aprobado antes de integrarse.

### `testing`
La rama `testing` se utiliza para integrar y probar cambios antes de llevarlos a `main`.

- Los Pull Requests de las ramas personales deben dirigirse primero a `testing`.
- No se permiten cambios directos sin revisión.
- Los cambios deben probarse en conjunto.
- Otra persona debe validar el funcionamiento.
- Si se encuentran errores, el responsable debe corregirlos en su propia rama.
- Cuando `testing` sea estable, se podrá crear un Pull Request hacia `main`.
- No se permiten pushes directos a `testing`.
- Todos los cambios deben llegar mediante Pull Request.

## Ramas personales
Cada integrante trabajará en su propia rama. Nadie deberá modificar directamente la rama de otra persona.

Cada integrante es responsable de:
- Trabajar únicamente en su rama.
- Mantener actualizada su rama.
- Documentar sus cambios.
- Probar su código antes de crear un Pull Request.
- Resolver los comentarios recibidos durante la revisión.
- Informar si su cambio afecta a otros módulos.

## Flujo de trabajo
1. Crear o utilizar la rama personal correspondiente.
2. Realizar los cambios únicamente en esa rama.
3. Probar localmente el código.
4. Actualizar el README de la carpeta si el cambio lo requiere.
5. Registrar cualquier decisión técnica importante.
6. Crear un Pull Request hacia `testing`.
7. Solicitar la revisión de otra persona.
8. Corregir los problemas encontrados.
9. Validar el cambio dentro de `testing`.
10. Cuando el conjunto sea estable, crear un Pull Request de `testing` hacia `main`.

## Control de versiones
Se utilizará el formato: MAJOR.MINOR.PATCH o vx.y.z

Cada número representa lo siguiente:
- `MAJOR`: cambios importantes que pueden romper la compatibilidad.
- `MINOR`: nuevas funciones compatibles con la versión anterior.
- `PATCH`: correcciones, mejoras pequeñas o ajustes que no cambian la funcionalidad principal.

Ejemplos:
- v0.1.0
- v0.2.1
- v1.0.0


Mientras el proyecto siga en desarrollo, se puede utilizar `0` como versión principal: v0.y.z

Ejemplo en `CHANGELOG.md`:
```md
## [0.2.0] - 2026-09-28
### descripción breve
### Agregado
- Lectura inicial del sensor de agua.
- Responsable: Nombre de la persona.
- Rama: `nombre/sensor-agua`.
```

## Pull Requests
Todo Pull Request debe incluir:
- Descripción clara del cambio.
- Motivo del cambio.
- Archivos o módulos afectados.
- Pruebas realizadas.
- Resultado esperado.
- Problemas conocidos o pendientes.
- Nombre de la persona responsable.
- Nombre de la persona que realizará la revisión.

Los Pull Requests de ramas personales deben dirigirse primero a `testing`.

## Revisión obligatoria
Ningún cambio se considerará terminado hasta que otra persona lo haya revisado.

La persona revisora debe comprobar:
- Que el código sea entendible.
- Que no se hayan roto otros módulos.
- Que las pruebas sean suficientes.
- Que no existan credenciales o configuraciones privadas.
- Que el README esté actualizado.
- Que el cambio funcione en `testing`.
- Que el cambio corresponda con la descripción del Pull Request.

La persona que desarrolló el cambio no debe ser la única responsable de aprobarlo.

## Documentación
Todo módulo nuevo debe tener un `README.md` cuando sea necesario para explicar:
- Qué hace.
- Quién es responsable.
- Qué hardware utiliza.
- Cómo se conecta.
- Qué librerías requiere.
- Cómo se ejecuta o prueba.
- Qué funciones ofrece.
- Qué problemas conocidos tiene.
- Cómo se integra con el resto del sistema.

Esto aplica especialmente a:
- Sensores.
- Protocolos de comunicación.
- Firmware de cada ESP32.
- Scripts.
- Pruebas específicas.
- Herramientas auxiliares.

La documentación debe actualizarse junto con el código.

## Commits
Los commits deben describir claramente qué se modificó.

Ejemplos:
- test(imu): agrega prueba de calibracion
- fix(esp-now): corrige reconexion entre nodos

## Regla de seguridad
No se deben subir al repositorio:
- Contraseñas WiFi.
- Claves privadas.
- Tokens.
- Archivos de configuración personal.
- Archivos temporales.
- Datos sensibles.

Para estos casos se deben utilizar archivos `.example` y la carpeta `configuracion-local/`.


## Regla principal
Ningún cambio debe llegar directamente a `main` ni a `testing`.
El flujo obligatorio será: Rama personal → Pull Request → testing → validación → Pull Request → main