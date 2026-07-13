"""USB CDC serial transport to Flipper Zero."""

from __future__ import annotations

import time

import psutil
import serial
import serial.tools.list_ports

FLIPPER_VID = 0x0483
FLIPPER_PID = 0x5740
BAUD_RATE = 115200


def kill_stale_runfap() -> None:
    for proc in psutil.process_iter(["pid", "cmdline"]):
        try:
            cmdline = proc.info["cmdline"] or []
            if any("runfap.py" in arg for arg in cmdline):
                proc.kill()
                print(f"Killed stale ufbt runfap.py (pid {proc.pid})")
        except (psutil.NoSuchProcess, psutil.AccessDenied):
            pass


def find_flipper_port() -> str | None:
    for port in serial.tools.list_ports.comports():
        if port.vid == FLIPPER_VID and port.pid == FLIPPER_PID:
            return port.device
    return None


def _as_packet_list(result) -> list[bytes]:
    if isinstance(result, (bytes, bytearray)):
        return [bytes(result)]
    return list(result)


def run_usb_loop(packet_fn) -> None:
    """packet_fn() -> bytes | list[bytes]; reconnect forever."""
    kill_stale_runfap()
    print("JAYX agent · USB mode")

    while True:
        port_name = find_flipper_port()
        if not port_name:
            print("Flipper Zero not found. Retrying in 2s...")
            time.sleep(2)
            continue

        try:
            with serial.Serial(port_name, BAUD_RATE, timeout=1) as port:
                print(f"Connected to {port_name}")
                while True:
                    packets = _as_packet_list(packet_fn())
                    try:
                        for packet in packets:
                            port.write(packet)
                        port.flush()
                    except serial.SerialException as e:
                        print(f"Lost connection: {e}")
                        break
        except serial.SerialException as e:
            print(f"Failed to open {port_name}: {e}")
            time.sleep(2)
