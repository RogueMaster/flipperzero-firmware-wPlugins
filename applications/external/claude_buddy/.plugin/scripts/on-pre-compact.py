#!/usr/bin/env python3
"""PreCompact hook: compaction LED + context-aware character animation."""

import json
import os
import socket
import sys

SOCKET_PATH = "/tmp/claude-flipper-bridge.sock"
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))


def send_to_flipper(sound: str, vibro: bool, text: str, subtext: str = "") -> None:
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(SOCKET_PATH)
    msg = json.dumps(
        {
            "action": "notify",
            "sound": sound,
            "vibro": vibro,
            "text": text,
            "subtext": subtext,
        }
    )
    s.sendall(msg.encode())
    s.shutdown(socket.SHUT_WR)
    s.recv(4096)
    s.close()


def main():
    if not os.path.exists(SOCKET_PATH):
        sys.exit(0)

    payload = sys.stdin.read()
    try:
        send_to_flipper("led_compact", False, "Compacting...")
        if payload.strip():
            proc = os.popen(f"python3 {SCRIPT_DIR}/context_usage.py pre_compact", "w")
            proc.write(payload)
            proc.close()
    except Exception:
        pass

    sys.exit(0)


if __name__ == "__main__":
    main()
