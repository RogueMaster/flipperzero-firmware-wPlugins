#!/usr/bin/env python3
"""Capture Flipper frames via USB RPC for Apps Catalog screenshots.

Produces 512x256 PNGs with only orange (254,138,44) and black (0,0,0).

Closes any locked loader app via CLI first, then opens Loteria Zero so
captures are not stolen by whatever was already running (e.g. CardScope).
"""

from __future__ import annotations

import glob
import hashlib
import time
from pathlib import Path

import serial
from PIL import Image

ORANGE = (254, 138, 44)
BLACK = (0, 0, 0)
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "screenshots"
APP = "/ext/apps/Games/loteria_zero.fap"

UP, DOWN, RIGHT, LEFT, OK, BACK = 0, 1, 2, 3, 4, 5
PRESS, RELEASE, SHORT = 0, 1, 2


def encode_varint(n: int) -> bytes:
    out = bytearray()
    while True:
        b = n & 0x7F
        n >>= 7
        out.append(b | (0x80 if n else 0))
        if not n:
            return bytes(out)


def decode_varint(buf: bytes, i: int):
    shift = 0
    n = 0
    while True:
        b = buf[i]
        i += 1
        n |= (b & 0x7F) << shift
        if not (b & 0x80):
            return n, i
        shift += 7


def wrap_msg(payload: bytes) -> bytes:
    return encode_varint(len(payload)) + payload


def cli_relaunch(port: str) -> None:
    """Close whatever owns the loader, then open Loteria Zero."""
    ser = serial.Serial(port, 115200, timeout=0.3)
    try:
        time.sleep(0.15)
        ser.reset_input_buffer()
        ser.write(b"\r\n")
        time.sleep(0.2)
        ser.write(b"loader close\r")
        time.sleep(1.0)
        ser.read(8192)
        ser.write(f"loader open {APP}\r".encode())
        time.sleep(2.0)
        out = ser.read(8192).decode("utf-8", "replace")
        ser.write(b"loader info\r")
        time.sleep(0.4)
        info = ser.read(4096).decode("utf-8", "replace")
        print(out)
        print(info)
        if "Loteria Zero" not in info:
            raise SystemExit("Loteria Zero is not running after loader open")
    finally:
        ser.close()


class FlipperRPC:
    def __init__(self, port: str):
        self.ser = serial.Serial(port, 115200, timeout=0.15)
        self.cmd = 1
        self.buf = bytearray()
        time.sleep(0.15)
        self.ser.reset_input_buffer()
        self.ser.write(b"\r\n")
        time.sleep(0.15)
        self.ser.write(b"start_rpc_session\r")
        time.sleep(0.5)
        self.ser.read(4096)
        self.buf.clear()
        self._send_start_stream()

    def _id(self) -> int:
        self.cmd += 1
        return self.cmd

    def write(self, data: bytes) -> None:
        self.ser.write(data)

    def _send_start_stream(self) -> None:
        body = bytes([0x08]) + encode_varint(self._id()) + bytes([0xA2, 0x01, 0x00])
        self.write(wrap_msg(body))

    def _send_stop_stream(self) -> None:
        body = bytes([0x08]) + encode_varint(self._id()) + bytes([0xAA, 0x01, 0x00])
        self.write(wrap_msg(body))

    def tap(self, key: int, pause: float = 0.45) -> None:
        for typ in (PRESS, SHORT, RELEASE):
            inner = bytes([0x08, key, 0x10, typ])
            body = bytes([0x08]) + encode_varint(self._id())
            body += bytes([0xBA, 0x01]) + encode_varint(len(inner)) + inner
            self.write(wrap_msg(body))
            time.sleep(0.05)
        time.sleep(pause)

    def next_frame(self, timeout: float = 3.0, distinct_from: bytes | None = None) -> bytes | None:
        deadline = time.time() + timeout
        while time.time() < deadline:
            chunk = self.ser.read(8192)
            if chunk:
                self.buf.extend(chunk)
            while True:
                if not self.buf:
                    break
                try:
                    size, i = decode_varint(self.buf, 0)
                except Exception:
                    del self.buf[:1]
                    continue
                if len(self.buf) < i + size:
                    break
                msg = bytes(self.buf[i : i + size])
                del self.buf[: i + size]
                idx = msg.find(b"\xb2\x01")
                if idx < 0:
                    continue
                j = idx + 2
                try:
                    nested_len, j = decode_varint(msg, j)
                except Exception:
                    continue
                nested = msg[j : j + nested_len]
                if not nested.startswith(b"\x0a"):
                    continue
                dlen, k = decode_varint(nested, 1)
                data = nested[k : k + dlen]
                if len(data) < 1024:
                    continue
                frame = data[:1024]
                if distinct_from is not None and frame == distinct_from:
                    continue
                return frame
        return None

    def close(self) -> None:
        try:
            self._send_stop_stream()
        finally:
            self.ser.close()


def save_png(frame: bytes, path: Path) -> None:
    img = Image.new("RGB", (128, 64), BLACK)
    px = img.load()
    for y in range(64):
        for x in range(128):
            byte = frame[(y // 8) * 128 + x]
            px[x, y] = ORANGE if (byte & (1 << (y % 8))) else BLACK
    out = img.resize((512, 256), Image.NEAREST)
    cols = {c[1] for c in (out.getcolors(512 * 256) or [])}
    if cols - {ORANGE, BLACK}:
        raise RuntimeError(f"bad colors {cols}")
    out.save(path)
    digest = hashlib.sha1(frame).hexdigest()[:8]
    print(f"saved {path.name} sha1={digest} colors={sorted(cols)}")


def grab(rpc: FlipperRPC, prev: bytes | None, timeout: float = 3.0) -> bytes | None:
    fr = rpc.next_frame(timeout=timeout, distinct_from=prev)
    if fr is None:
        fr = rpc.next_frame(timeout=1.5)
    return fr


def main() -> None:
    ports = glob.glob("/dev/cu.usbmodem*")
    if not ports:
        raise SystemExit("Flipper not connected")
    port = ports[0]
    OUT.mkdir(exist_ok=True)

    # Remove prior catalog captures only (keep README)
    for p in OUT.glob("*.png"):
        p.unlink()

    cli_relaunch(port)

    rpc = FlipperRPC(port)
    try:
        time.sleep(0.4)
        menu = grab(rpc, None, 4.0)
        if not menu:
            raise SystemExit("no menu frame")
        save_png(menu, OUT / "ss0.png")
        prev = menu

        # New game -> vertical card view
        rpc.tap(OK, 0.9)
        game = grab(rpc, prev, 3.0)
        if game:
            save_png(game, OUT / "ss1.png")
            prev = game
            rpc.tap(OK, 0.7)
            game2 = grab(rpc, prev, 2.5)
            if game2:
                save_png(game2, OUT / "ss2.png")
                prev = game2

        # Back to menu
        rpc.tap(BACK, 0.8)
        menu2 = grab(rpc, prev, 3.0) or prev
        if menu2:
            save_png(menu2, OUT / "ss_menu_return.png")
            prev = menu2

        # Menu order with save present:
        # New game, Continue, All cards, History, Statistics, Our story, Settings, About
        # Without Continue: New game, All cards, History, ...
        # From top: Down x2 -> History if Continue exists, else Down x1
        # Probe: go to History (prefer Continue path: Down x3)
        for _ in range(3):
            rpc.tap(DOWN, 0.28)
        rpc.tap(OK, 0.8)
        hist = grab(rpc, prev, 3.0)
        if hist:
            save_png(hist, OUT / "ss3.png")
            prev = hist

        rpc.tap(BACK, 0.6)
        # From History selection, Down to Statistics, Our story, Settings
        for _ in range(3):
            rpc.tap(DOWN, 0.28)
        rpc.tap(OK, 0.8)
        settings = grab(rpc, prev, 3.0)
        if settings:
            save_png(settings, OUT / "ss4.png")
            prev = settings

        rpc.tap(BACK, 0.6)
        rpc.tap(DOWN, 0.28)
        rpc.tap(OK, 0.8)
        about = grab(rpc, prev, 3.0)
        if about:
            save_png(about, OUT / "ss5.png")
    finally:
        rpc.close()


if __name__ == "__main__":
    main()
