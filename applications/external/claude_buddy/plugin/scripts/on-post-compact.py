#!/usr/bin/env python3
"""PostCompact hook: stop cyan LED blink on Flipper after context compaction finishes."""

import json
import os
import socket
import sys

SOCKET_PATH = "/tmp/claude-flipper-bridge.sock"


def send_to_flipper(sound: str, vibro: bool, text: str, subtext: str = "") -> None:
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(SOCKET_PATH)
    msg = json.dumps({"action": "notify", "sound": sound, "vibro": vibro, "text": text, "subtext": subtext})
    s.sendall(msg.encode())
    s.shutdown(socket.SHUT_WR)
    s.recv(4096)
    s.close()


def main():
    if not os.path.exists(SOCKET_PATH):
        sys.exit(0)

    try:
        send_to_flipper("compact_done", False, "Compacted")
    except Exception:
        pass

    sys.exit(0)


if __name__ == "__main__":
    main()
