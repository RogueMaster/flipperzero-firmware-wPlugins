#!/usr/bin/env python3
"""
Push a file to a Flipper Zero over the serial CLI.

Why this exists rather than `ufbt launch` / `storage.py`: both of those open an
RPC session that conflicts with an already-active CLI session and hang. This
speaks the plain CLI protocol directly.

Two details that are easy to get wrong and cause silent corruption:

  1. Commands must be terminated with "\\r" ONLY. Sending "\\r\\n" leaves the
     "\\n" in the input stream, where `storage write_chunk` counts it as the
     first payload byte -- so every chunk lands shifted by one and the file is
     quietly wrong (correct size, wrong md5).

  2. After sending a chunk's payload, wait for the prompt before issuing the
     next command. Interleaving corrupts the stream.

The upload is verified with an on-device md5 before returning success.
"""

import hashlib
import os
import select
import sys
import termios
import time

PROMPT = b">: "
CHUNK = 1024


class FlipperCLI:
    def __init__(self, port):
        self.fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        attrs = termios.tcgetattr(self.fd)
        # Fully raw: no canonical mode, no signal chars, no flow control.
        attrs[0] = attrs[1] = attrs[3] = 0
        attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
        termios.tcsetattr(self.fd, termios.TCSANOW, attrs)
        termios.tcflush(self.fd, termios.TCIOFLUSH)
        self.buf = b""

    def close(self):
        os.close(self.fd)

    def write(self, data):
        n = 0
        while n < len(data):
            select.select([], [self.fd], [], 5)
            try:
                n += os.write(self.fd, data[n:])
            except BlockingIOError:
                time.sleep(0.005)

    def until(self, token, timeout=15.0):
        deadline = time.time() + timeout
        while time.time() < deadline:
            if token in self.buf:
                i = self.buf.index(token) + len(token)
                out, self.buf = self.buf[:i], self.buf[i:]
                return out
            if select.select([self.fd], [], [], 0.2)[0]:
                try:
                    chunk = os.read(self.fd, 4096)
                except BlockingIOError:
                    continue
                if chunk:
                    self.buf += chunk
                    deadline = time.time() + 1.0
        out, self.buf = self.buf, b""
        return out

    def sync(self):
        """Get to a known-good prompt, discarding any partial state."""
        time.sleep(0.5)
        for _ in range(3):
            self.write(b"\x03\r")
            time.sleep(0.3)
        termios.tcflush(self.fd, termios.TCIOFLUSH)
        self.buf = b""
        time.sleep(0.3)
        self.write(b"\r")
        self.until(PROMPT, 5)
        self.buf = b""

    def cmd(self, line, timeout=10.0):
        self.write(line.encode() + b"\r")  # \r ONLY -- see module docstring
        return self.until(PROMPT, timeout)


def push(port, src, dest):
    data = open(src, "rb").read()
    local_md5 = hashlib.md5(data).hexdigest()

    cli = FlipperCLI(port)
    try:
        cli.sync()
        cli.cmd(f'storage remove "{dest}"', timeout=8)

        sent = 0
        while sent < len(data):
            chunk = data[sent : sent + CHUNK]
            cli.write(f'storage write_chunk "{dest}" {len(chunk)}\r'.encode())
            resp = cli.until(b"Ready\r\n", 10)
            if b"Ready" not in resp:
                raise RuntimeError(f"device did not accept chunk: {resp[:200]!r}")
            cli.write(chunk)
            cli.until(PROMPT, 20)
            sent += len(chunk)
            pct = sent * 100 // len(data)
            print(f"\r    {pct:3d}%  {sent}/{len(data)} bytes", end="", flush=True)
        print()

        out = cli.cmd(f'storage md5 "{dest}"', timeout=10).decode("utf-8", "replace")
        hashes = [
            tok
            for tok in (l.strip() for l in out.splitlines())
            if len(tok) == 32 and all(c in "0123456789abcdef" for c in tok)
        ]
        if not hashes:
            raise RuntimeError(f"no md5 in response: {out!r}")
        if hashes[0] != local_md5:
            raise RuntimeError(f"md5 mismatch: device={hashes[0]} local={local_md5}")
        print(f"    verified md5 {local_md5}")
    finally:
        cli.close()


if __name__ == "__main__":
    if len(sys.argv) != 4:
        print(__doc__)
        print("usage: flipper_push.py <port> <local_file> <device_path>")
        sys.exit(2)
    push(sys.argv[1], sys.argv[2], sys.argv[3])
