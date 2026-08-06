"""Транспорт: CLI Flipper Zero поверх USB-CDC.

Flipper отдаёт CLI на виртуальном COM-порту. Приложение tpms_bridge,
запущенное на самом Flipper, регистрирует там команду `tpms_rx`, которая
построчно печатает NDJSON с принятыми кадрами.

Модуль ничего не знает про UI: события складываются в очередь, забирает
их кто угодно.
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

from .decoder import RenaultFrame, parse_frame

DEFAULT_FREQUENCY = 433_920_000
CLI_COMMAND = "tpms_rx"
CLI_PROMPT = b">: "
CTRL_C = b"\x03"

FLIPPER_VID = 0x0483
FLIPPER_PID = 0x5740

_ANSI_RE = re.compile(rb"\x1b\[[0-9;?]*[a-zA-Z]")


@dataclass(frozen=True)
class Reading:
    """Один принятый кадр вместе с обстоятельствами приёма."""

    host_time: float
    device_tick: int
    frame: RenaultFrame
    rssi_dbm: float | None

    @property
    def sensor_id(self) -> str:
        return self.frame.id_hex


def find_flipper_ports() -> list[str]:
    """Все похожие на Flipper последовательные порты."""
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
    """Читающий поток: держит порт, гоняет команду, отдаёт события.

    События в очереди — кортежи (kind, payload):
      ("reading", Reading)  — декодированный кадр
      ("status", str)       — служебное сообщение
      ("raw", str)          — строка сырых таймингов (режим raw)
      ("line", str)         — всё прочее, что пришло с устройства
      ("error", str)        — сбой, поток завершился
      ("closed", None)      — поток корректно остановлен
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
    # Управление
    # ------------------------------------------------------------------

    def start(self) -> None:
        if self._thread is not None:
            raise RuntimeError("уже запущено")
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
    # Внутреннее
    # ------------------------------------------------------------------

    def _emit(self, kind: str, payload: object = None) -> None:
        self.events.put((kind, payload))

    def _run(self) -> None:
        try:
            self._serial = serial.Serial(self.port, timeout=0.2, write_timeout=2.0)
        except serial.SerialException as exc:
            self._emit("error", f"не удалось открыть {self.port}: {exc}")
            return

        try:
            self._drain_banner()
            options = self.mode + (" wake" if self.wake else "")
            command = f"{CLI_COMMAND} {self.frequency} {options}\r\n"
            self._serial.write(command.encode())
            self._emit("status", f"отправлено: {command.strip()}")
            self._read_loop()
        except serial.SerialException as exc:
            self._emit("error", f"порт отвалился: {exc}")
        except Exception as exc:  # noqa: BLE001 — поток не должен падать молча
            self._emit("error", f"сбой чтения: {exc}")
        finally:
            self._shutdown()

    def _drain_banner(self) -> None:
        """Разбудить CLI и дождаться приглашения."""
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

        # Эхо команды и приглашение CLI не засоряют лог.
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

        # Поля пересчитываем на хосте: decoder.py — единственный источник
        # правды по протоколу, прошивка отдаёт сырые байты.
        frame = parse_frame(raw)
        if frame is None:
            self._emit("line", f"кадр с битой CRC: {raw_hex}")
            return

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
                # Ctrl+C останавливает команду, чтобы Flipper не остался
                # с включённым приёмником.
                self._serial.write(CTRL_C)
                self._serial.flush()
                time.sleep(0.1)
                self._serial.read(self._serial.in_waiting or 0)
            except Exception:  # noqa: BLE001 — порт мог уже исчезнуть
                pass
            try:
                self._serial.close()
            except Exception:  # noqa: BLE001
                pass
            self._serial = None
        self._emit("closed")


def drain(events: queue.Queue) -> Iterator[tuple[str, object]]:
    """Забрать всё, что накопилось в очереди, не блокируясь."""
    while True:
        try:
            yield events.get_nowait()
        except queue.Empty:
            return
