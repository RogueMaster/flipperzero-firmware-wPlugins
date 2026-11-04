"""Capture Flipper screens over the RPC screen stream, and drive the UI to reach them.

The firmware has no screenshot CLI; qFlipper does it over the protobuf RPC, which is what this
speaks. The protobuf is hand-rolled because the three messages involved are a handful of bytes
each and generating bindings from flipper.proto would be more moving parts than the task needs.

Wire format: `start_rpc_session` on the text CLI switches the serial link into RPC mode, after
which every message is a varint length followed by a PB_Main. Field numbers come from
assets/protobuf/flipper.proto:

    20 gui_start_screen_stream_request
    21 gui_stop_screen_stream_request
    22 gui_screen_frame        -> ScreenFrame { bytes data = 1 }
    23 gui_send_input_event_request -> { InputKey key = 1; InputType type = 2 }

The frame is the canvas buffer: 128x64 pixels, one bit each, in u8g2 page order - 8 pages of 128
columns, each byte holding 8 vertically stacked pixels. Output is 4x nearest-neighbour in the
palette qFlipper uses, so these sit beside hand-captured ones without looking different.
"""

import sys
import time

import serial
from PIL import Image

WIDTH, HEIGHT, SCALE = 128, 64, 4
BG, FG = (254, 138, 44), (0, 0, 0)

KEYS = {"UP": 0, "DOWN": 1, "RIGHT": 2, "LEFT": 3, "OK": 4, "BACK": 5}
TYPE_PRESS, TYPE_RELEASE, TYPE_SHORT = 0, 1, 2


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


def framed(payload: bytes) -> bytes:
    return varint(len(payload)) + payload


def msg_start_stream() -> bytes:
    return framed(tag(1, 0) + varint(1) + tag(20, 2) + varint(0))


def msg_stop_stream() -> bytes:
    return framed(tag(1, 0) + varint(2) + tag(21, 2) + varint(0))


def msg_input(key: str, kind: int = TYPE_SHORT) -> bytes:
    inner = tag(1, 0) + varint(KEYS[key]) + tag(2, 0) + varint(kind)
    return framed(tag(1, 0) + varint(3) + tag(23, 2) + varint(len(inner)) + inner)


def read_varint(buf: bytes, pos: int):
    value, shift = 0, 0
    while pos < len(buf):
        byte = buf[pos]
        pos += 1
        value |= (byte & 0x7F) << shift
        if not byte & 0x80:
            return value, pos
        shift += 7
    return None, pos


def find_frame(buf: bytes):
    """Pull the first 1024-byte canvas buffer out of whatever has arrived.

    Scans for a length-delimited field holding exactly a screen's worth of bytes rather than
    walking the whole PB_Main structure - the size is unambiguous here and the alternative is a
    full protobuf parser for one field.
    """
    want = WIDTH * HEIGHT // 8
    pos = 0
    while pos < len(buf):
        idx = buf.find(b"\x80\x08", pos)  # varint 1024, as a length prefix
        if idx < 0:
            return None
        start = idx + 2
        if start + want <= len(buf):
            return buf[start : start + want]
        pos = idx + 1
    return None


def to_image(frame: bytes) -> Image.Image:
    img = Image.new("RGB", (WIDTH, HEIGHT), BG)
    px = img.load()
    for page in range(HEIGHT // 8):
        for x in range(WIDTH):
            bits = frame[page * WIDTH + x]
            for bit in range(8):
                if bits & (1 << bit):
                    px[x, page * 8 + bit] = FG
    return img.resize((WIDTH * SCALE, HEIGHT * SCALE), Image.NEAREST)


def msg_ping() -> bytes:
    return framed(tag(1, 0) + varint(9) + tag(5, 2) + varint(0))


def msg_stop_session() -> bytes:
    return framed(tag(1, 0) + varint(8) + tag(19, 2) + varint(0))


class Flipper:
    def __init__(self, port: str):
        self.port = serial.Serial(port, timeout=1)
        self.port.baudrate = 230400
        self.buf = b""
        self._open_session()
        # Requested once for the whole run: registering primes a first frame, and every screen
        # change sends another.
        self.port.write(msg_start_stream())
        time.sleep(0.8)

    def _open_session(self):
        """Get into RPC mode from whatever state the device was left in.

        The device stays in RPC mode after a client goes away, so a run that died leaves it
        there - and the text `start_rpc_session` sent into a live session is malformed protobuf,
        which wedges it further. Closing with stop_session is what frees it, so opening with one
        makes the handshake work whether or not the last run got that far. It is harmless at a
        CLI prompt, where it reads as a stray line.
        """
        self.port.reset_input_buffer()
        self.port.write(msg_stop_session())
        time.sleep(0.6)
        self.port.read(self.port.in_waiting or 1)

        self.port.write(b"\r")
        time.sleep(0.5)
        self.port.read(self.port.in_waiting or 1)
        self.port.write(
            b"start_rpc_session\r"
        )  # bare CR; a trailing LF kills the session
        time.sleep(0.8)
        self.port.read(self.port.in_waiting or 1)

        self.port.write(msg_ping())
        time.sleep(0.6)
        if not self.port.read(self.port.in_waiting or 1):
            raise RuntimeError(
                "RPC session did not answer a ping after start_rpc_session"
            )

    def grab(self) -> Image.Image:
        """Take the freshest frame, discarding anything queued from before the last input.

        Re-asks for the stream between attempts. The device only sends frames while a stream is
        registered, and anything that disturbs the session in between - another tool using the
        port, a reconnect - leaves the session answering pings but sending nothing, which is
        indistinguishable from a quiet screen until you ask again.
        """
        # Drain whatever the last input produced and keep the newest complete frame. The device
        # emits on screen changes, so a keypress yields one or more frames and a settled screen
        # yields none - which is why nothing here flushes the buffer or re-asks for the stream.
        # Re-asking while a stream is already registered gets an acknowledgement and no frame.
        latest = None
        deadline = time.time() + 4
        while time.time() < deadline:
            self.buf += self.port.read(self.port.in_waiting or 1)
            while True:
                frame = self._take_frame()
                if not frame:
                    break
                latest = frame
            if latest and not self.port.in_waiting:
                return to_image(latest)
        if latest:
            return to_image(latest)
        raise TimeoutError("no screen frame arrived")

    def _take_frame(self):
        """Pull the oldest complete frame out of the running buffer, consuming it."""
        want = WIDTH * HEIGHT // 8
        idx = self.buf.find(b"\x0a\x80\x08")  # ScreenFrame.data, length 1024
        if idx < 0 or idx + 3 + want > len(self.buf):
            return None
        frame = self.buf[idx + 3 : idx + 3 + want]
        self.buf = self.buf[idx + 3 + want :]
        return frame

    def press(self, key: str, settle: float = 0.6):
        """Replay a full button cycle, not just the short-press event.

        The firmware's input service emits press, then short, then release, and the view
        dispatcher acts on the sequence - a lone SHORT arrives without the press that should
        have preceded it and is ignored.
        """
        for kind in (TYPE_PRESS, TYPE_SHORT, TYPE_RELEASE):
            self.port.write(msg_input(key, kind))
            time.sleep(0.08)
        time.sleep(settle)

    def close(self):
        """Hand the device back to its CLI.

        Leaving the session open is what makes the next run's handshake fail, so this is not
        merely tidiness.
        """
        try:
            self.port.write(msg_stop_stream())
            time.sleep(0.3)
            self.port.write(msg_stop_session())
            time.sleep(0.3)
        finally:
            self.port.close()


def main() -> int:
    port, out_dir = sys.argv[1], sys.argv[2]
    script = sys.argv[3:]  # name[:key,key,...] steps

    flipper = Flipper(port)
    try:
        for step in script:
            name, _, keys = step.partition(":")
            for key in filter(None, keys.split(",")):
                flipper.press(key)
            path = f"{out_dir}/{name}.png"
            flipper.grab().save(path)
            print("captured", path, flush=True)
    finally:
        flipper.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
