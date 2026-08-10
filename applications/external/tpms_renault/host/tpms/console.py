"""Console mode: the same as the UI, only without a window.

    python -m tpms.console                       # listen on air
    python -m tpms.console --mode raw            # raw timings
    python -m tpms.console --decode capture.sub  # parse a file
"""

from __future__ import annotations

import argparse
import queue
import sys
import time

from .flipper_link import DEFAULT_FREQUENCY, FlipperLink, Reading, find_flipper_ports
from .model import SensorModel
from .sub_file import load


def _describe(reading: Reading) -> str:
    frame = reading.frame
    rssi = "" if reading.rssi_dbm is None else f"  rssi {reading.rssi_dbm:6.1f} dBm"
    return (
        f"[{time.strftime('%H:%M:%S')}] "
        f"ID {frame.id_hex}  "
        f"{frame.pressure_kpa:6.2f} kPa ({frame.pressure_bar:.3f} bar, {frame.pressure_psi:.1f} PSI)  "
        f"{frame.temperature_c:4d} C  "
        f"flags 0x{frame.flags:02x}  "
        f"raw {frame.raw_hex}{rssi}"
    )


def _decode_file(path: str) -> int:
    capture = load(path)
    print(f"Timings in the file: {len(capture.timings)}")
    if capture.frequency:
        print(f"Frequency: {capture.frequency} Hz")
    if capture.preset:
        print(f"Preset: {capture.preset}")

    frames = capture.decode()
    if not frames:
        print("No valid Renault frames found.")
        return 1

    for frame in frames:
        print(
            f"ID {frame.id_hex}  {frame.pressure_kpa:.2f} kPa  "
            f"{frame.temperature_c} C  flags 0x{frame.flags:02x}  raw {frame.raw_hex}"
        )
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="tpms.console", description=__doc__)
    parser.add_argument("--port", help="Flipper serial port (found automatically by default)")
    parser.add_argument("--freq", type=int, default=DEFAULT_FREQUENCY, help="frequency in Hz")
    parser.add_argument("--mode", choices=["json", "raw"], default="json")
    parser.add_argument(
        "--wake",
        action="store_true",
        help="periodically wake the sensor with a 125 kHz field (hold it against the back of the Flipper)",
    )
    parser.add_argument("--csv", help="where to export the readings on exit")
    parser.add_argument("--decode", help="parse a .sub file or a raw mode dump")
    args = parser.parse_args(argv)

    if args.decode:
        return _decode_file(args.decode)

    port = args.port
    if not port:
        ports = find_flipper_ports()
        if not ports:
            print("No Flipper found. Connect it over USB or pass --port.", file=sys.stderr)
            return 2
        port = ports[0]

    print(
        f"Port: {port}   frequency: {args.freq} Hz   mode: {args.mode}"
        f"   wake: {'on' if args.wake else 'off'}"
    )
    print("The TPMS Bridge app must be running on the Flipper. Ctrl+C to quit.\n")

    model = SensorModel()
    link = FlipperLink(port=port, frequency=args.freq, mode=args.mode, wake=args.wake)
    link.start()

    try:
        while link.running:
            try:
                kind, payload = link.events.get(timeout=0.5)
            except queue.Empty:
                continue

            if kind == "reading":
                model.update(payload)
                print(_describe(payload))
            elif kind == "raw":
                print(payload)
            elif kind == "error":
                print(f"error: {payload}", file=sys.stderr)
            elif kind in ("status", "line"):
                print(f"# {payload}")
    except KeyboardInterrupt:
        print("\nStopping...")
    finally:
        link.stop()

    print(f"\nFrames total: {model.total_frames}, sensors: {len(model.sensors())}")
    for stats in model.sensors():
        print(
            f"  {stats.sensor_id}: frames {stats.frames}, "
            f"pressure {stats.min_pressure_kpa:.2f}..{stats.max_pressure_kpa:.2f} kPa, "
            f"temperature {stats.min_temperature_c}..{stats.max_temperature_c} C"
        )

    if args.csv and model.total_frames:
        rows = model.export_csv(args.csv)
        print(f"Written to {args.csv}: {rows} rows")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
