#!/usr/bin/env python3
"""Capture only structured IMU/GPS data records from the ESP32 serial port."""

import argparse
import re
import sys
from datetime import datetime

try:
    import serial
except ImportError:
    print("Missing dependency: pyserial", file=sys.stderr)
    print("Install it with: python -m pip install pyserial", file=sys.stderr)
    raise SystemExit(1)


DATA_PATTERN = re.compile(r"\[DATA\]\s+(?P<record>.+)")

TYPE_NAMES = {
    "IMU1": "IMU 1 (Cabin)",
    "IMU2": "IMU 2 (Slave ESP-NOW)",
    "IMU_A": "IMU A (Average)",
    "CABIN_IMU": "Cabin IMU",
    "SLAVE_IMU": "Slave IMU (ESP-NOW)",
    "IMU": "IMU 1 (Cabin, legacy format)",
    "GPS": "GPS",
}

FIELD_NAMES = {
    "time_us": "Time (us)",
    "roll_deg": "Roll (deg)",
    "pitch_deg": "Pitch (deg)",
    "yaw_deg": "Yaw (deg)",
    "accel_x": "Linear acceleration X (m/s^2)",
    "accel_y": "Linear acceleration Y (m/s^2)",
    "accel_z": "Linear acceleration Z (m/s^2)",
    "gyro_x": "Gyro X (rad/s)",
    "gyro_y": "Gyro Y (rad/s)",
    "gyro_z": "Gyro Z (rad/s)",
    "ay_m_s2": "Lateral acceleration Y (m/s^2)",
    "sensors_used": "Sensors used in average",
    "valid": "Valid",
    "water_level_cm": "Water level (cm)",
    "status_flags": "Status flags",
    "lat_deg": "Latitude (deg)",
    "lon_deg": "Longitude (deg)",
    "speed_kmh": "Speed (km/h)",
    "speed_mph": "Speed (mph)",
    "fix": "GPS fix quality",
    "satellites": "Satellites",
    "hdop": "HDOP",
    "ltr": "LTR",
    "telemetry_roll_deg": "Combined telemetry roll (deg)",
}


def readable_record(record: str) -> str:
    fields = record.split(",")
    record_type = fields[0]
    values = []
    for field in fields[1:]:
        if "=" not in field:
            values.append(field)
            continue
        key, value = field.split("=", 1)
        values.append(f"{FIELD_NAMES.get(key, key)}: {value}")
    return f"{TYPE_NAMES.get(record_type, record_type)} | " + " | ".join(values)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Save only [DATA] IMU/GPS records from an ESP32 serial monitor."
    )
    parser.add_argument("--port", required=True, help="Serial port, for example COM5")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--output", default="", help="Output text file")
    args = parser.parse_args()

    output_path = args.output or f"sensor_data_{datetime.now():%Y%m%d_%H%M%S}.txt"

    print(f"Reading {args.port} at {args.baud} baud")
    print(f"Saving only sensor records to {output_path}")
    print("Stop with Ctrl+C")

    try:
        with serial.Serial(args.port, args.baud, timeout=1) as port, open(
            output_path, "w", encoding="utf-8", newline="\n"
        ) as output:
            output.write("Named sensor records\n")
            while True:
                line = port.readline().decode("utf-8", errors="replace")
                match = DATA_PATTERN.search(line)
                if not match:
                    continue
                record = match.group("record").strip()
                readable = readable_record(record)
                print(readable, flush=True)
                output.write(readable + "\n")
                output.flush()
    except KeyboardInterrupt:
        print("\nCapture stopped.")
    except serial.SerialException as error:
        print(f"Serial error: {error}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
