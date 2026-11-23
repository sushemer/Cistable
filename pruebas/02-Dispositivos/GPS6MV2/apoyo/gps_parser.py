"""Conversión de NMEA y registros de diagnóstico del firmware a filas Excel."""

from datetime import datetime
from math import isfinite

COLUMNS = [
    "hora_pc", "hora_gps", "tipo", "fix_valido", "latitud_deg",
    "longitud_deg", "velocidad_kmh", "rumbo_deg", "satelites", "hdop",
    "calidad_fix", "nmea_crudo", "registro", "uptime_esp_us",
    "checksum_valido", "parse_valido", "hora_gps_ms", "rmc_valido",
    "desfase_rmc_gga_ms", "edad_rmc_ms", "edad_gga_ms", "estado_gps",
    "estado_velocidad", "velocidad_utilizable", "delta_velocidad_kmh",
    "aceleracion_kmh_s", "evento", "velocidad_nudos",
    "velocidad_coordenadas_kmh", "diferencia_rmc_coordenadas_kmh",
]


def nmea_to_degrees(value, hemisphere):
    """Convierte grados/minutos NMEA a grados decimales."""
    if not value or not hemisphere:
        return None
    try:
        raw = float(value)
        degrees = int(raw / 100)
        minutes = raw - degrees * 100
        if minutes < 0 or minutes >= 60:
            return None
        result = degrees + minutes / 60
        if hemisphere in ("S", "W"):
            result = -result
        elif hemisphere not in ("N", "E"):
            return None
        return result
    except (TypeError, ValueError):
        return None


def _blank_record():
    record = {column: None for column in COLUMNS}
    record["hora_pc"] = datetime.now().astimezone().isoformat(timespec="milliseconds")
    return record


def _number(value, cast=float):
    if value in (None, "", "nan", "NaN"):
        return None
    try:
        parsed = cast(value)
        if cast is float and not isfinite(parsed):
            return None
        return parsed
    except (TypeError, ValueError, OverflowError):
        return None


def parse_nmea(line):
    """Interpreta una sentencia NMEA cruda (compatibilidad con registros anteriores)."""
    record = _blank_record()
    record.update(registro="NMEA", nmea_crudo=line)
    fields = line.split(",")
    if not fields or not fields[0].startswith("$"):
        return record

    record["tipo"] = fields[0][1:]
    record["hora_gps"] = fields[1] if len(fields) > 1 else None
    if fields and "*" in fields[-1]:
        fields[-1] = fields[-1].split("*", 1)[0]

    if fields[0] in ("$GPRMC", "$GNRMC") and len(fields) >= 9:
        speed_knots = _number(fields[7])
        record.update(
            fix_valido=fields[2] == "A",
            latitud_deg=nmea_to_degrees(fields[3], fields[4]),
            longitud_deg=nmea_to_degrees(fields[5], fields[6]),
            velocidad_nudos=speed_knots,
            velocidad_kmh=speed_knots * 1.852 if speed_knots is not None else None,
            rumbo_deg=_number(fields[8]),
        )
    elif fields[0] in ("$GPGGA", "$GNGGA") and len(fields) >= 9:
        record.update(
            calidad_fix=_number(fields[6], int),
            satelites=_number(fields[7], int),
            hdop=_number(fields[8]),
        )
    return record


def parse_diagnostic(payload):
    """Interpreta GPSDIAG|1 emitido por el firmware de prueba 02."""
    parts = payload.split("|", 26)
    if len(parts) != 27 or parts[0] != "GPSDIAG" or parts[1] != "1":
        return None
    record = _blank_record()
    (_marker, _version, uptime, sentence_type, checksum, parsed, gps_ms,
     rmc_valid, quality, epoch_skew, latitude, longitude, speed_knots,
     speed_kmh, course, satellites, hdop, rmc_age, gga_age, gps_state,
     speed_state, speed_usable, delta, acceleration, coordinate_speed,
     coordinate_delta, raw) = parts
    record.update(
        registro="GPSDIAG",
        uptime_esp_us=_number(uptime, int),
        tipo=sentence_type or None,
        hora_gps_ms=_number(gps_ms, int),
        fix_valido=(_number(rmc_valid, int) == 1) if rmc_valid else None,
        rmc_valido=(_number(rmc_valid, int) == 1) if rmc_valid else None,
        calidad_fix=_number(quality, int),
        desfase_rmc_gga_ms=_number(epoch_skew, int),
        latitud_deg=_number(latitude),
        longitud_deg=_number(longitude),
        velocidad_kmh=_number(speed_kmh),
        velocidad_nudos=_number(speed_knots),
        rumbo_deg=_number(course),
        satelites=_number(satellites, int),
        hdop=_number(hdop),
        edad_rmc_ms=_number(rmc_age, int),
        edad_gga_ms=_number(gga_age, int),
        estado_gps=gps_state or None,
        estado_velocidad=speed_state or None,
        velocidad_utilizable=(_number(speed_usable, int) == 1) if speed_usable else None,
        delta_velocidad_kmh=_number(delta),
        aceleracion_kmh_s=_number(acceleration),
        velocidad_coordenadas_kmh=_number(coordinate_speed),
        diferencia_rmc_coordenadas_kmh=_number(coordinate_delta),
        checksum_valido=_number(checksum, int) == 1,
        parse_valido=_number(parsed, int) == 1,
        nmea_crudo=raw,
    )
    if sentence_type in ("GPRMC", "GNRMC") and raw.startswith("$"):
        nmea_record = parse_nmea(raw)
        record["hora_gps"] = nmea_record["hora_gps"]
        record["velocidad_kmh"] = _number(speed_kmh)
    elif sentence_type in ("GPGGA", "GNGGA") and raw.startswith("$"):
        record["hora_gps"] = raw.split(",", 2)[1] if raw.count(",") >= 1 else None
    return record


def parse_serial_line(line):
    """Extrae un registro desde la consola ESP-IDF o desde NMEA directa."""
    if line.startswith("$"):
        return parse_nmea(line)
    marker = line.find("GPSDIAG|")
    if marker >= 0:
        return parse_diagnostic(line[marker:])
    status_marker = line.find("GPSSTATUS|")
    if status_marker >= 0:
        parts = line[status_marker:].split("|", 8)
        if len(parts) == 9 and parts[1] == "1":
            record = _blank_record()
            record.update(
                registro="STATUS",
                uptime_esp_us=_number(parts[2], int),
                estado_gps=parts[3],
                edad_rmc_ms=_number(parts[4], int),
                edad_gga_ms=_number(parts[5], int),
                velocidad_kmh=_number(parts[6]),
                estado_velocidad=parts[7],
                velocidad_utilizable=_number(parts[8], int) == 1,
                evento="Estado periódico; permite identificar interrupciones de recepción.",
                nmea_crudo=line,
            )
            return record
    event_marker = line.find("GPS_EVENT|")
    if event_marker >= 0:
        record = _blank_record()
        record.update(registro="EVENTO", evento=line[event_marker:].strip(), nmea_crudo=line)
        return record
    return None