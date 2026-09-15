from __future__ import annotations

import os
from dataclasses import dataclass


@dataclass(frozen=True)
class SerialCandidate:
    device: str
    description: str
    likely_flipper: bool


def serial_candidates() -> list[SerialCandidate]:
    try:
        from serial.tools import list_ports
    except ImportError as error:
        raise RuntimeError(
            "pyserial is required; install this package with pip"
        ) from error

    candidates: list[SerialCandidate] = []
    for port in list_ports.comports():
        description = " ".join(
            value
            for value in (port.description, port.manufacturer, port.product)
            if value
        )
        searchable = f"{port.device} {description}".lower()
        likely = "flipper" in searchable or "usbmodemflip" in searchable
        candidates.append(
            SerialCandidate(port.device, description or "serial device", likely)
        )
    return sorted(candidates, key=lambda item: (not item.likely_flipper, item.device))


def open_serial(device: str, timeout: float = 0.1):
    try:
        import serial
    except ImportError as error:
        raise RuntimeError(
            "pyserial is required; install this package with pip"
        ) from error
    options = {
        "port": device,
        "baudrate": 115200,
        "timeout": timeout,
        "write_timeout": 2.0,
    }
    if os.name == "posix":
        options["exclusive"] = True
    connection = serial.Serial(**options)
    # PTYs used by the simulator do not implement modem-control ioctls. Real
    # CDC ACM devices do, and DTR tells the Flipper that the host is ready.
    try:
        connection.dtr = True
        connection.rts = False
    except OSError:
        pass
    return connection
