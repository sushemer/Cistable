"""Registra líneas serie y separa los campos importantes de IMU_DATA y MASTERDATA."""

from __future__ import annotations

import argparse
import re
from datetime import datetime
from pathlib import Path
from time import monotonic

import serial
from openpyxl import Workbook
from openpyxl.styles import Alignment, Font, PatternFill
from openpyxl.utils import get_column_letter

SAVE_SECONDS = 5.0
ANSI = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")
LOG = re.compile(r"\b([DIWE])\s+\((\d+)\)\s+([^:]+):\s?(.*)")
MASTER_FIELDS = [
    "master_uptime_us", "gps_state", "latitude_deg", "longitude_deg", "gps_speed_kmh",
    "gps_satellites", "gps_hdop", "gps_rmc_age_ms", "front_imu_online", "front_roll_deg",
    "front_pitch_deg", "front_ax_m_s2", "front_ay_m_s2", "front_az_m_s2", "rear_online",
    "rear_sequence", "valid_mask", "imu1_roll_deg", "imu1_pitch_deg", "imu1_ay_m_s2",
    "imu2_roll_deg", "imu2_pitch_deg", "imu2_ay_m_s2", "water_level",
]
DIRECT_IMU_FIELDS = [
    "imu_id", "imu_sample_time_us", "imu_online", "imu_roll_deg", "imu_pitch_deg", "imu_yaw_deg",
    "imu_ax_m_s2", "imu_ay_m_s2", "imu_az_m_s2", "imu_gx_rad_s", "imu_gy_rad_s", "imu_gz_rad_s",
]
RAW_COLUMNS = ["hora_pc_local", "nivel", "etiqueta", "tipo_registro", *DIRECT_IMU_FIELDS, *MASTER_FIELDS, "mensaje_crudo", "linea_serial"]


def now_local() -> str:
    return datetime.now().astimezone().isoformat(timespec="milliseconds")


def value(text: str):
    if text.lower() in {"nan", "inf", "+inf", "-inf", "infinity", "+infinity"}:
        return None
    if re.fullmatch(r"[+-]?\d+", text):
        return int(text)
    if re.fullmatch(r"[+-]?(?:\d+\.\d*|\.\d+|\d+)(?:[eE][+-]?\d+)?", text):
        return float(text)
    return text


def make_sheet(book: Workbook):
    ws = book.create_sheet("Datos_Crudos")
    ws.append(RAW_COLUMNS)
    ws.freeze_panes = "A2"
    ws.auto_filter.ref = f"A1:{get_column_letter(len(RAW_COLUMNS))}1"
    for cell in ws[1]:
        cell.font = Font(bold=True, color="FFFFFF")
        cell.fill = PatternFill("solid", fgColor="1F4E78")
        cell.alignment = Alignment(wrap_text=True, vertical="center")
    ws.sheet_view.showGridLines = False
    return ws


def fit(ws):
    for column in ws.columns:
        width = min(42, max(12, max(len(str(cell.value or "")) for cell in column[:200]) + 2))
        ws.column_dimensions[column[0].column_letter].width = width
    ws.column_dimensions[get_column_letter(RAW_COLUMNS.index("mensaje_crudo") + 1)].width = 55
    ws.column_dimensions[get_column_letter(RAW_COLUMNS.index("linea_serial") + 1)].width = 55


def create_book():
    book = Workbook()
    book.remove(book.active)
    ws = make_sheet(book)
    fit(ws)
    return book, ws


def parse_masterdata(message: str):
    if "MASTERDATA|" not in message:
        return None
    parts = message[message.index("MASTERDATA|"):].split("|")
    if len(parts) < 3 or parts[1] != "2" or len(parts[2:]) != len(MASTER_FIELDS):
        return None
    return dict(zip(MASTER_FIELDS, [value(item) for item in parts[2:]]))


def parse_direct_imu(message: str):
    if "IMU_DATA|" not in message:
        return None
    parts = message[message.index("IMU_DATA|"):].split("|")
    if len(parts) != 13:
        return None
    return dict(zip(DIRECT_IMU_FIELDS, [value(item) for item in parts[1:]]))


def append_row(ws, level: str, tag: str, message: str, line: str):
    direct = parse_direct_imu(message)
    master = parse_masterdata(message)
    direct_values = [direct.get(field) if direct else None for field in DIRECT_IMU_FIELDS]
    master_values = [master.get(field) if master else None for field in MASTER_FIELDS]
    kind = "IMU_DATA" if direct else "MASTERDATA" if master else "OTRA_LINEA"
    ws.append([now_local(), level, tag, kind, *direct_values, *master_values, message, line])


def main():
    parser = argparse.ArgumentParser(description="Guarda la línea serie completa y separa IMU_DATA y MASTERDATA en columnas.")
    parser.add_argument("puerto", help="COM del master, por ejemplo COM5")
    parser.add_argument("archivo", help="Libro .xlsx nuevo, por ejemplo registro_atras.xlsx")
    args = parser.parse_args()
    output = Path(args.archivo).expanduser().resolve()
    if output.suffix.lower() != ".xlsx":
        parser.error("El archivo de salida debe terminar en .xlsx")
    if output.exists():
        parser.error(f"No se sobrescribirá el archivo existente: {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    book, ws = create_book()
    book.save(output)
    print(f"Registro: {args.puerto} -> {output}")
    print("Cada fila conserva la línea completa y separa IMU_DATA o MASTERDATA en columnas.")
    last_save = monotonic()
    try:
        with serial.Serial(args.puerto, 115200, timeout=1) as port:
            while True:
                raw_bytes = port.readline()
                if not raw_bytes:
                    if monotonic() - last_save >= SAVE_SECONDS:
                        book.save(output)
                        last_save = monotonic()
                    continue
                line = ANSI.sub("", raw_bytes.decode("utf-8", errors="replace").rstrip("\r\n"))
                match = LOG.search(line)
                level = match.group(1) if match else ""
                tag = match.group(3).strip() if match else ""
                message = match.group(4) if match else line
                append_row(ws, level, tag, message, line)
                print(line)
                if monotonic() - last_save >= SAVE_SECONDS:
                    book.save(output)
                    last_save = monotonic()
    except KeyboardInterrupt:
        print("\nRegistro detenido.")
    except serial.SerialException as error:
        print(f"No se pudo abrir {args.puerto}: {error}")
    finally:
        fit(ws)
        book.save(output)
        print(f"Libro guardado: {output}")


if __name__ == "__main__":
    main()
