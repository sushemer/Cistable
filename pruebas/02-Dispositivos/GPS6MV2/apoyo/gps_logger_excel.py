"""Programa principal para registrar datos NMEA del ESP32 en Excel.

Uso:
    py gps_logger_excel.py --device gps6mv2 --port COM7 --output registro.xlsx
"""

from __future__ import annotations

import argparse
from pathlib import Path

import serial
from serial.serialutil import SerialException

from dispositivos import obtener_dispositivo
from gps_excel import prepare_workbook
from gps_parser import COLUMNS, parse_serial_line

SAVE_EVERY_ROWS = 10


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--device", default="gps6mv2", help="Dispositivo configurado")
    parser.add_argument("--port", required=True, help="COM del ESP32, por ejemplo COM7")
    parser.add_argument("--output", required=True, help="Nombre del archivo Excel; debe indicarlo el usuario")
    args = parser.parse_args()

    try:
        device = obtener_dispositivo(args.device, args.port)
    except ValueError as error:
        parser.error(str(error))

    output = Path(args.output).resolve()
    book, sheet = prepare_workbook(output)
    book.save(output)
    pending = 0
    print(f"Registrando {device['nombre']} en {device['puerto']}; salida: {output}")
    print("Presione Ctrl+C para terminar.")

    try:
        with serial.Serial(device["puerto"], device["baudrate"], timeout=1) as port:
            while True:
                raw = port.readline()
                if not raw:
                    continue
                line = raw.decode("ascii", errors="replace").strip()
                record = parse_serial_line(line)
                if record is None:
                    continue

                sheet.append([record[column] for column in COLUMNS])
                pending += 1
                print(line)

                if pending >= SAVE_EVERY_ROWS:
                    book.save(output)
                    pending = 0
    except SerialException as error:
        print(f"No se pudo abrir {device['puerto']}: {error}")
        print("Cierre cualquier monitor que esté usando ese COM y vuelva a intentar.")
    except KeyboardInterrupt:
        print("Registro detenido por la persona.")
    finally:
        book.save(output)
        print(f"Registro guardado en {output}")


if __name__ == "__main__":
    main()