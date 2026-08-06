"""Консольный режим — то же самое, что UI, но без окна.

    python -m tpms.console                       # слушать эфир
    python -m tpms.console --mode raw            # сырые тайминги
    python -m tpms.console --decode capture.sub  # разобрать файл
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
    print(f"Таймингов в файле: {len(capture.timings)}")
    if capture.frequency:
        print(f"Частота: {capture.frequency} Гц")
    if capture.preset:
        print(f"Пресет: {capture.preset}")

    frames = capture.decode()
    if not frames:
        print("Валидных кадров Renault не найдено.")
        return 1

    for frame in frames:
        print(
            f"ID {frame.id_hex}  {frame.pressure_kpa:.2f} kPa  "
            f"{frame.temperature_c} C  flags 0x{frame.flags:02x}  raw {frame.raw_hex}"
        )
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="tpms.console", description=__doc__)
    parser.add_argument("--port", help="последовательный порт Flipper (по умолчанию ищется сам)")
    parser.add_argument("--freq", type=int, default=DEFAULT_FREQUENCY, help="частота в Гц")
    parser.add_argument("--mode", choices=["json", "raw"], default="json")
    parser.add_argument(
        "--wake",
        action="store_true",
        help="периодически будить датчик полем 125 кГц (держать его у задней стороны Flipper)",
    )
    parser.add_argument("--csv", help="куда выгрузить показания при выходе")
    parser.add_argument("--decode", help="разобрать файл .sub или дамп raw-режима")
    args = parser.parse_args(argv)

    if args.decode:
        return _decode_file(args.decode)

    port = args.port
    if not port:
        ports = find_flipper_ports()
        if not ports:
            print("Flipper не найден. Подключите его по USB или укажите --port.", file=sys.stderr)
            return 2
        port = ports[0]

    print(
        f"Порт: {port}   частота: {args.freq} Гц   режим: {args.mode}"
        f"   пробуждение: {'вкл' if args.wake else 'выкл'}"
    )
    print("На Flipper должно быть запущено приложение TPMS Bridge. Ctrl+C — выход.\n")

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
                print(f"ошибка: {payload}", file=sys.stderr)
            elif kind in ("status", "line"):
                print(f"# {payload}")
    except KeyboardInterrupt:
        print("\nОстанавливаю...")
    finally:
        link.stop()

    print(f"\nВсего кадров: {model.total_frames}, датчиков: {len(model.sensors())}")
    for stats in model.sensors():
        print(
            f"  {stats.sensor_id}: кадров {stats.frames}, "
            f"давление {stats.min_pressure_kpa:.2f}..{stats.max_pressure_kpa:.2f} кПа, "
            f"температура {stats.min_temperature_c}..{stats.max_temperature_c} C"
        )

    if args.csv and model.total_frames:
        rows = model.export_csv(args.csv)
        print(f"Записано в {args.csv}: {rows} строк")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
