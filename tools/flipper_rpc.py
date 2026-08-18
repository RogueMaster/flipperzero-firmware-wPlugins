#!/usr/bin/env python3
"""Minimal Flipper Zero RPC client: capture the screen and press buttons.

The Flipper CLI turns into a protobuf RPC channel after `start_rpc_session`.
Only three messages are needed here, so instead of pulling in a generated
protobuf runtime the few required fields are encoded by hand.

Wire format: each PB_Main message is prefixed with its length as a varint.
  Main.command_id                        = field 1, varint
  Main.gui_start_screen_stream_request   = field 20, embedded (empty)
  Main.gui_stop_screen_stream_request    = field 21, embedded (empty)
  Main.gui_screen_frame                  = field 22, embedded {data = field 1, bytes}
  Main.gui_send_input_event_request      = field 23, embedded {key = 1, type = 2}
  Main.stop_session                      = field 19, embedded (empty)

Framebuffer is 128x64, 1bpp, 8 pages of 128 bytes; each byte holds 8 vertical
pixels with the least significant bit at the top.

Usage:
  python flipper_rpc.py COM3 shot out.png
  python flipper_rpc.py COM3 press ok
  python flipper_rpc.py COM3 script "ok,down,ok" outdir
"""

import sys
import time

import serial
from PIL import Image

KEYS = {"up": 0, "down": 1, "right": 2, "left": 3, "ok": 4, "back": 5}
TYPE_PRESS = 0
TYPE_RELEASE = 1
TYPE_SHORT = 2
TYPE_LONG = 3

WIDTH, HEIGHT = 128, 64


def varint(value: int) -> bytes:
    out = bytearray()
    while True:
        byte = value & 0x7F
        value >>= 7
        out.append(byte | (0x80 if value else 0))
        if not value:
            return bytes(out)


def tag(field: int, wire: int) -> bytes:
    return varint((field << 3) | wire)


def embedded(field: int, payload: bytes) -> bytes:
    return tag(field, 2) + varint(len(payload)) + payload


def frame(payload: bytes) -> bytes:
    """Length-delimited PB_Main."""
    return varint(len(payload)) + payload


def read_varint(port) -> int:
    result, shift = 0, 0
    while True:
        chunk = port.read(1)
        if not chunk:
            raise TimeoutError("no data while reading varint")
        byte = chunk[0]
        result |= (byte & 0x7F) << shift
        if not byte & 0x80:
            return result
        shift += 7


def read_message(port, timeout_s: float = 5.0) -> bytes:
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        try:
            length = read_varint(port)
        except TimeoutError:
            continue
        if length == 0 or length > 8192:
            continue
        buf = b""
        while len(buf) < length and time.time() < deadline:
            buf += port.read(length - len(buf))
        if len(buf) == length:
            return buf
    raise TimeoutError("no complete RPC message received")


def _varint_at(buf: bytes, i: int):
    """Decode a varint at offset i. Returns (value, next_offset).

    Field tags above 15 need two bytes, so both tags and lengths must be read
    as full varints — reading a single byte silently misaligns everything that
    follows (ScreenFrame is field 22, tag 0xB2 0x01).
    """
    value, shift = 0, 0
    while i < len(buf):
        byte = buf[i]
        i += 1
        value |= (byte & 0x7F) << shift
        if not byte & 0x80:
            return value, i
        shift += 7
    raise ValueError("truncated varint")


def find_screen_frame(payload: bytes):
    """Return the 1024-byte framebuffer if this Main carries a ScreenFrame."""
    i = 0
    while i < len(payload):
        key, i = _varint_at(payload, i)
        field, wire = key >> 3, key & 0x07
        if wire == 0:
            _, i = _varint_at(payload, i)
        elif wire == 2:
            size, i = _varint_at(payload, i)
            body = payload[i : i + size]
            i += size
            if field == 22:  # Main.gui_screen_frame
                j = 0
                while j < len(body):
                    inner_key, j = _varint_at(body, j)
                    if inner_key & 0x07 != 2:
                        break
                    inner_size, j = _varint_at(body, j)
                    if inner_key >> 3 == 1:  # ScreenFrame.data
                        return body[j : j + inner_size]
                    j += inner_size
        else:
            return None
    return None


def framebuffer_to_image(data: bytes, scale: int = 1) -> Image.Image:
    img = Image.new("1", (WIDTH, HEIGHT), 1)
    px = img.load()
    for page in range(HEIGHT // 8):
        for x in range(WIDTH):
            byte = data[page * WIDTH + x]
            for bit in range(8):
                if byte & (1 << bit):
                    px[x, page * 8 + bit] = 0
    if scale > 1:
        img = img.resize((WIDTH * scale, HEIGHT * scale), Image.NEAREST)
    return img


class FlipperRPC:
    def __init__(self, port_name: str):
        self.port = serial.Serial(port_name, 230400, timeout=0.4, write_timeout=5)
        time.sleep(0.4)

        # Tear down anything a previous run left behind before saying hello. A
        # run that dies mid-frame leaves the screen stream running, and the
        # firmware then keeps pushing frames into a CDC nobody is reading: the
        # app's GUI thread blocks inside the frame callback, input stops being
        # processed, and even the loader cannot close it. It looks exactly like
        # an application hang and it is not one. If a session is still live
        # these two messages end it; if it is not, they are noise to the shell,
        # which the wake-up below clears.
        self.command_id = 1
        for content in (embedded(21, b""), embedded(19, b"")):
            try:
                self._send(content)
            except serial.SerialException:
                break
        time.sleep(0.3)

        self.port.reset_input_buffer()
        self.port.write(b"\r\n")
        time.sleep(0.4)
        self.port.read(4096)
        # Terminate with a bare CR. With "\r\n" the CLI consumes only the CR,
        # and the leftover LF (0x0A) is then read by the RPC layer as a varint
        # length of 10 — every following message is misframed, the session
        # answers ERROR_DECODE once and drops back to the shell.
        self.port.write(b"start_rpc_session\r")
        deadline = time.time() + 2.0
        while time.time() < deadline:
            if self.port.read(4096):
                deadline = time.time() + 0.6
        self.command_id = 1
        self.streaming = False

    def _send(self, content: bytes) -> None:
        payload = tag(1, 0) + varint(self.command_id) + content
        self.command_id += 1
        self.port.write(frame(payload))

    def start_stream(self) -> None:
        self._send(embedded(20, b""))
        self.streaming = True
        time.sleep(0.3)

    def stop_stream(self) -> None:
        if self.streaming:
            self._send(embedded(21, b""))
            self.streaming = False
            time.sleep(0.2)

    def screenshot(self, path: str, scale: int = 1, settle_s: float = 0.6) -> str:
        """Grab the freshest frame. The device streams continuously, so drain
        whatever is queued and keep the last complete frame."""
        if not self.streaming:
            self.start_stream()
        time.sleep(settle_s)

        # Discard the backlog: frames queued before now show the previous
        # screen. Dropping them costs framing alignment, so restart the stream
        # to get a clean message boundary, then take the first frame that
        # arrives.
        self.port.reset_input_buffer()
        self._send(embedded(21, b""))  # stop
        time.sleep(0.15)
        self.port.reset_input_buffer()
        self._send(embedded(20, b""))  # start again

        data = None
        deadline = time.time() + 5.0
        while time.time() < deadline:
            try:
                msg = read_message(self.port, timeout_s=2.0)
            except (TimeoutError, ValueError):
                break
            try:
                found = find_screen_frame(msg)
            except ValueError:
                continue
            if found and len(found) >= WIDTH * HEIGHT // 8:
                data = found
                break
        if data is None:
            raise RuntimeError("no screen frame captured")
        framebuffer_to_image(data, scale).save(path)
        return path

    def _input_event(self, code: int, event_type: int) -> None:
        inner = b""
        if code:  # proto3 omits zero-valued fields; UP == 0
            inner += tag(1, 0) + varint(code)
        if event_type:  # PRESS == 0
            inner += tag(2, 0) + varint(event_type)
        self._send(embedded(23, inner))
        time.sleep(0.06)

    def press(self, key: str, long: bool = False) -> None:
        """Emulate a real button tap.

        A lone SHORT event is accepted by the RPC layer but ignored by the
        GUI — hardware emits PRESS, then SHORT (or LONG) on release, then
        RELEASE, and the input service expects that whole sequence.
        """
        code = KEYS[key.lower()]
        self._input_event(code, TYPE_PRESS)
        self._input_event(code, TYPE_LONG if long else TYPE_SHORT)
        self._input_event(code, TYPE_RELEASE)
        time.sleep(0.3)

    def close(self) -> None:
        # Each half is guarded separately: if stopping the stream fails because
        # the port has already wedged, the session teardown is still worth
        # attempting, and the port must close either way.
        for step in (self.stop_stream, lambda: self._send(embedded(19, b""))):
            try:
                step()
            except serial.SerialException:
                pass
        time.sleep(0.2)
        self.port.close()


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    port_name, action = sys.argv[1], sys.argv[2]
    rpc = FlipperRPC(port_name)
    try:
        if action == "shot":
            out = sys.argv[3] if len(sys.argv) > 3 else "screenshot.png"
            scale = int(sys.argv[4]) if len(sys.argv) > 4 else 1
            print(rpc.screenshot(out, scale))
        elif action == "press":
            for key in sys.argv[3].split(","):
                rpc.press(key.strip())
            print("ok")
        elif action == "script":
            steps = [s.strip() for s in sys.argv[3].split(",") if s.strip()]
            outdir = sys.argv[4] if len(sys.argv) > 4 else "."
            scale = int(sys.argv[5]) if len(sys.argv) > 5 else 1
            rpc.start_stream()
            for index, step in enumerate(steps):
                if step.startswith("wait"):
                    time.sleep(float(step[4:] or 1))
                elif step == "shot":
                    print(rpc.screenshot(f"{outdir}/step{index:02d}.png", scale))
                else:
                    rpc.press(step)
        else:
            print(f"unknown action: {action}")
            return 1
    finally:
        rpc.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
