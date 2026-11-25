"""Configuración de los dispositivos que pueden registrarse."""

DISPOSITIVOS = {
    "gps6mv2": {
        "descripcion": "Receptor GPS 6M V2 conectado al ESP32",
        "baudrate": 115200,
    },
}


def obtener_dispositivo(nombre, puerto):
    """Devuelve la configuración seleccionada usando el puerto indicado."""
    if nombre not in DISPOSITIVOS:
        disponibles = ", ".join(sorted(DISPOSITIVOS))
        raise ValueError(f"Dispositivo desconocido: {nombre}. Disponibles: {disponibles}")

    configuracion = DISPOSITIVOS[nombre].copy()
    configuracion["nombre"] = nombre
    configuracion["puerto"] = puerto
    return configuracion
