"""Transport: the Flipper Zero CLI over USB-CDC.

The Flipper exposes its CLI on a virtual COM port. The tpms_bridge app,
running on the Flipper itself, registers the `tpms_rx` command there, which
prints received frames as line-delimited JSON.

This module knows nothing about the UI: events go into a queue and anyone
can pick them up.
"""

from __future__ import annotations

import json
import queue
import re
import threading
import time
from dataclasses import dataclass
from typing import Iterator

import serial
from serial.tools import list_ports

from .decoder import TpmsFrame, parse_frame

DEFAULT_FREQUENCY = 433_920_000
CLI_COMMAND = "tpms_rx"
CLI_PROMPT = b">: "
CTRL_C = b"\x03"

FLIPPER_VID = 0x0483
FLIPPER_PID = 0x5740

_ANSI_RE = re.compile(rb"\x1b\[[0-9;?]*[a-zA-Z]")


@dataclass(frozen=True)
class Reading:
    """One received frame together with the circumstances of reception."""

    host_time: float
    device_tick: int
    frame: RenaultFrame
    rssi_dbm: float | None

    @property
    def sensor_id(self) -> str:
        return self.frame.id_hex


def find_flipper_ports() -> list[str]:
    """Every serial port that looks like a Flipper."""
    ports: list[str] = []
    for port in list_ports.comports():
        name = (port.device or "") + " " + (port.description or "")
        looks_like_flipper = (
            (port.vid == FLIPPER_VID and port.pid == FLIPPER_PID)
            or "flip" in name.lower()
        )
        if looks_like_flipper:
            ports.append(port.device)
    return ports


def _clean(line: bytes) -> str:
    return _ANSI_RE.sub(b"", line).decode("utf-8", errors="replace").strip()


class FlipperLink:
    """Reader thread: holds the port, runs the command, emits events.

    Events in the queue are (kind, payload) tuples:
      ("reading", Reading)  decoded frame
      ("status", str)       informational message
      ("raw", str)          a line of raw timings (raw mode)
      ("line", str)         anything else that came from the device
      ("error", str)        failure, the thread has finished
      ("closed", None)      the thread stopped cleanly
    """

    def __init__(
        self,
        port: str,
        frequency: int = DEFAULT_FREQUENCY,
        mode: str = "json",
        wake: bool = False,
        events: queue.Queue | None = None,
    ) -> None:
        self.port = port
        self.frequency = frequency
        self.mode = mode
        self.wake = wake
        self.events: queue.Queue = events or queue.Queue()

        self._serial: serial.Serial | None = None
        self._thread: threading.Thread | None = None
        self._stop = threading.Event()

    # ------------------------------------------------------------------
    # Control
    # ------------------------------------------------------------------

    def start(self) -> None:
        if self._thread is not None:
            raise RuntimeError("already running")
        self._stop.clear()
        self._thread = threading.Thread(target=self._run, name="flipper-link", daemon=True)
        self._thread.start()

    def stop(self, timeout: float = 3.0) -> None:
        self._stop.set()
        thread = self._thread
        if thread is not None:
            thread.join(timeout)
        self._thread = None

    @property
    def running(self) -> bool:
        return self._thread is not None and self._thread.is_alive()

    # ------------------------------------------------------------------
    # Internals
    # ------------------------------------------------------------------

    def _emit(self, kind: str, payload: object = None) -> None:
        self.events.put((kind, payload))

    def _run(self) -> None:
        try:
            self._serial = serial.Serial(self.port, timeout=0.2, write_timeout=2.0)
        except serial.SerialException as exc:
            self._emit("error", f"cannot open {self.port}: {exc}")
            return

        try:
            self._drain_banner()
            options = self.mode + (" wake" if self.wake else "")
            command = f"{CLI_COMMAND} {self.frequency} {options}\r\n"
            self._serial.write(command.encode())
            self._emit("status", f"sent: {command.strip()}")
            self._read_loop()
        except serial.SerialException as exc:
            self._emit("error", f"port went away: {exc}")
        except Exception as exc:  # noqa: BLE001 — the thread must not die silently
            self._emit("error", f"read failure: {exc}")
        finally:
            self._shutdown()

    def _drain_banner(self) -> None:
        """Wake the CLI up and wait for its prompt."""
        assert self._serial is not None
        self._serial.write(b"\r\n")
        deadline = time.monotonic() + 2.0
        buffer = b""
        while time.monotonic() < deadline:
            buffer += self._serial.read(256)
            if CLI_PROMPT in buffer:
                break
        self._serial.reset_input_buffer()

    def _read_loop(self) -> None:
        assert self._serial is not None
        buffer = b""

        while not self._stop.is_set():
            chunk = self._serial.read(max(1, self._serial.in_waiting))
            if not chunk:
                continue
            buffer += chunk

            while b"\n" in buffer:
                line, buffer = buffer.split(b"\n", 1)
                self._handle_line(_clean(line))

    def _handle_line(self, line: str) -> None:
        if not line:
            return

        if line.startswith("{"):
            self._handle_json(line)
            return

        if line[0] in "+-" and self.mode == "raw":
            self._emit("raw", line)
            return

        # The command echo and the CLI prompt do not belong in the log.
        if line.startswith(CLI_COMMAND) or line.startswith(">:"):
            return

        self._emit("line", line)

    def _handle_json(self, line: str) -> None:
        try:
            message = json.loads(line)
        except json.JSONDecodeError:
            self._emit("line", line)
            return

        if "error" in message:
            self._emit("error", str(message["error"]))
            return

        if "event" in message:
            self._emit("status", f"{message['event']}: {message}")
            return

        raw_hex = message.get("raw")
        if not raw_hex:
            self._emit("line", line)
            return

        try:
            raw = bytes.fromhex(raw_hex)
        except ValueError:
            self._emit("line", line)
            return

        # The firmware knows all the protocols and has already checked
        # the frame, so the fields come from it; the raw bytes travel
        # along so that anything can be re-derived later. Renault frames
        # are still parsed here as well, which keeps the one protocol
        # this project was built around verified end to end.
        proto = str(message.get("proto", "renault"))
        pressure = message.get("pressure_kpa_x100")
        identifier = str(message.get("id", "0"))

        frame = TpmsFrame(
            raw=raw,
            sensor_id=int(identifier, 16),
            pressure_kpa=(pressure / 100.0) if isinstance(pressure, (int, float)) else None,
            temperature_c=message.get("temp_c"),
            flags=int(message.get("flags", 0)),
            proto=proto,
            battery_ok=message.get("battery_ok"),
            id_digits=max(6, len(identifier)),
        )

        if proto == "renault":
            checked = parse_frame(raw)
            if checked is None:
                self._emit("line", f"frame with a bad CRC: {raw_hex}")
                return
            frame = checked

        rssi_x10 = message.get("rssi_dbm_x10")
        self._emit(
            "reading",
            Reading(
                host_time=time.time(),
                device_tick=int(message.get("t", 0)),
                frame=frame,
                rssi_dbm=(rssi_x10 / 10.0) if isinstance(rssi_x10, (int, float)) else None,
            ),
        )

    def _shutdown(self) -> None:
        if self._serial is not None:
            try:
                # Ctrl+C stops the command so that the Flipper is not left
                # with its receiver running.
                self._serial.write(CTRL_C)
                self._serial.flush()
                time.sleep(0.1)
                self._serial.read(self._serial.in_waiting or 0)
            except Exception:  # noqa: BLE001 — the port may be gone already
                pass
            try:
                self._serial.close()
            except Exception:  # noqa: BLE001
                pass
            self._serial = None
        self._emit("closed")


def drain(events: queue.Queue) -> Iterator[tuple[str, object]]:
    """Take everything the queue has accumulated, without blocking."""
    while True:
        try:
            yield events.get_nowait()
        except queue.Empty:
            return
