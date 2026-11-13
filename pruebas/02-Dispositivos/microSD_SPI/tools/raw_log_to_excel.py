"""Convierte un raw_XXXX.log del ESP32 en un solo libro Excel por hojas."""

from __future__ import annotations

import argparse
import re
from pathlib import Path
from typing import Dict, Iterable, List

from openpyxl import Workbook
from openpyxl.styles import Font, PatternFill
from openpyxl.utils import get_column_letter


KEY_VALUE_RE = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)=([^,\s]+)")

SHEETS = {
    "IMU1": ("IMU1_Frontal", [
        "registro", "time_us", "roll_deg", "pitch_deg", "yaw_deg",
        "accel_x", "accel_y", "accel_z", "gyro_x", "gyro_y", "gyro_z",
        "ltr", "ltr_valido",
    ]),
    "IMU2": ("IMU2_Trasera", [
        "registro", "time_us", "roll_deg", "pitch_deg", "ay_m_s2",
        "ltr", "ltr_valido", "status_flags",
    ]),
    "IMU_A": ("IMU_A_Combinada", [
        "registro", "time_us", "roll_deg", "ay_m_s2", "ltr",
        "sensores_usados", "calculo_valido",
    ]),
    "GPS": ("GPS", [
        "registro", "time_us", "lat_deg", "lon_deg", "speed_kmh",
        "fix", "satellites", "hdop", "ltr", "telemetry_roll_deg",
    ]),
}


def parse_value(value: str):
    """Convierte números decimales y hexadecimales; conserva texto restante."""
    try:
        if value.lower().startswith("0x"):
            return int(value, 16)
        if any(c in value for c in ".eE"):
            return float(value)
        return int(value)
    except ValueError:
        return value


def parse_line(line: str) -> Dict[str, object]:
    """Extrae el tipo de mensaje y sus pares clave=valor."""
    values = {key: parse_value(value) for key, value in KEY_VALUE_RE.findall(line)}
    if "IMU1" in line[:32]:
        kind = "IMU1"
    elif "IMU2" in line[:32]:
        kind = "IMU2"
    elif "IMU_A" in line[:32]:
        kind = "IMU_A"
    elif "GPS" in line[:32]:
        kind = "GPS"
    elif line.startswith("[EVENT]"):
        kind = "EVENT"
    else:
        kind = "OTRO"
    values["tipo"] = kind
    return values


def read_raw(path: Path):
    """Lee el formato registro;time_us;origen;linea_original."""
    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if not raw or raw.startswith("registro;"):
            continue
        parts = raw.split(";", 3)
        if len(parts) != 4:
            yield {"tipo": "OTRO", "linea_original": raw}
            continue
        record, time_us, origin, original = parts
        parsed = parse_line(original)
        parsed.update({
            "registro": parse_value(record),
            "time_us": parse_value(time_us),
            "origen": origin,
            "linea_original": original,
        })
        yield parsed


def value(row: Dict[str, object], key: str):
    """Devuelve vacío para campos ausentes y 1/0 para estados booleanos."""
    if key in ("ltr_valido", "calculo_valido"):
        return row.get("valid", "")
    return row.get(key, "")


def style_sheet(ws, headers: List[str]):
    """Aplica encabezado, autofiltro y anchos legibles."""
    header_fill = PatternFill("solid", fgColor="1F4E78")
    for cell in ws[1]:
        cell.font = Font(color="FFFFFF", bold=True)
        cell.fill = header_fill
    ws.freeze_panes = "A2"
    ws.auto_filter.ref = ws.dimensions
    for index, header in enumerate(headers, start=1):
        ws.column_dimensions[get_column_letter(index)].width = max(12, min(24, len(header) + 3))


def write_data_sheet(wb: Workbook, name: str, headers: List[str], rows: Iterable[Dict[str, object]]):
    ws = wb.create_sheet(name)
    ws.append(headers)
    for row in rows:
        ws.append([value(row, header) for header in headers])
    style_sheet(ws, headers)
    return ws


def create_workbook(raw_path: Path, output_path: Path):
    rows = list(read_raw(raw_path))
    wb = Workbook()
    summary = wb.active
    summary.title = "Resumen"
    summary.append(["Conversión de registro crudo", ""])
    summary.append(["Archivo de origen", raw_path.name])
    summary.append(["Nota", "Los datos se separaron desde un único archivo del ESP32."])
    summary.append([])
    summary.append(["Hoja", "Registros", "Estados válidos", "LTR promedio"])

    for kind, (sheet_name, headers) in SHEETS.items():
        write_data_sheet(wb, sheet_name, headers, (r for r in rows if r.get("tipo") == kind))
        summary.append([
            sheet_name,
            f'=COUNTA(\'{sheet_name}\'!A:A)-1',
            f'=COUNTIF(\'{sheet_name}\'!L:L,1)' if kind == "IMU1" else
            f'=COUNTIF(\'{sheet_name}\'!G:G,1)' if kind in ("IMU2", "IMU_A") else
            f'=COUNTIF(\'{sheet_name}\'!F:F,1)',
            f'=IFERROR(AVERAGE(\'{sheet_name}\'!L:L),"")' if kind == "IMU1" else
            f'=IFERROR(AVERAGE(\'{sheet_name}\'!F:F),"")' if kind == "IMU2" else
            f'=IFERROR(AVERAGE(\'{sheet_name}\'!E:E),"")' if kind == "IMU_A" else
            f'=IFERROR(AVERAGE(\'{sheet_name}\'!I:I),"")',
        ])

    event_ws = wb.create_sheet("Estados_Eventos")
    event_headers = ["registro", "time_us", "origen", "evento", "detalle", "linea_original"]
    event_ws.append(event_headers)
    for row in rows:
        if row.get("tipo") == "EVENT":
            event_ws.append([
                row.get("registro", ""), row.get("time_us", ""), row.get("origen", ""),
                row.get("event", ""), row.get("detail", ""), row.get("linea_original", ""),
            ])
    style_sheet(event_ws, event_headers)

    raw_ws = wb.create_sheet("Crudo")
    raw_headers = ["registro", "time_us", "origen", "linea_original"]
    raw_ws.append(raw_headers)
    for row in rows:
        raw_ws.append([row.get(header, "") for header in raw_headers])
    style_sheet(raw_ws, raw_headers)

    summary["A1"].font = Font(bold=True, size=14, color="FFFFFF")
    summary["A1"].fill = PatternFill("solid", fgColor="1F4E78")
    for cell in summary[5]:
        cell.font = Font(color="FFFFFF", bold=True)
        cell.fill = PatternFill("solid", fgColor="5B9BD5")
    summary.freeze_panes = "A6"
    summary.column_dimensions["A"].width = 24
    summary.column_dimensions["B"].width = 18
    summary.column_dimensions["C"].width = 14
    summary.column_dimensions["D"].width = 16
    wb.calculation.fullCalcOnLoad = True
    wb.calculation.forceFullCalc = True
    output_path.parent.mkdir(parents=True, exist_ok=True)
    wb.save(output_path)


def main():
    parser = argparse.ArgumentParser(description="Separa un raw_XXXX.log en un libro Excel por hojas.")
    parser.add_argument("raw_log", type=Path, help="Archivo raw_XXXX.log extraído de la microSD")
    parser.add_argument("output", type=Path, help="Libro .xlsx de salida")
    args = parser.parse_args()
    if args.output.suffix.lower() != ".xlsx":
        parser.error("El archivo de salida debe terminar en .xlsx")
    create_workbook(args.raw_log, args.output)
    print(f"Libro creado: {args.output}")


if __name__ == "__main__":
    main()
