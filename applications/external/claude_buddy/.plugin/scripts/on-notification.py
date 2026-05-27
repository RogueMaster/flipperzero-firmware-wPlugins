#!/usr/bin/env python3
"""Notification hook: forwards relevant Claude Code notifications to Flipper."""

import json
import os
import socket
import sys

SOCKET_PATH = "/tmp/claude-flipper-bridge.sock"

NOTIFY_MAP = {
    "idle_prompt":       ("alert", "Claude", "Waiting for input"),
}


def send_to_flipper(sound: str, text: str, subtext: str = "") -> None:
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(SOCKET_PATH)
    msg = json.dumps({"action": "notify", "sound": sound, "vibro": True, "text": text, "subtext": subtext})
    s.sendall(msg.encode())
    s.shutdown(socket.SHUT_WR)
    s.recv(4096)
    s.close()


def main():
    if not os.path.exists(SOCKET_PATH):
        sys.exit(0)

    try:
        hook_input = json.loads(sys.stdin.read())
    except (json.JSONDecodeError, EOFError):
        sys.exit(0)

    notification_type = hook_input.get("notification_type", "")
    entry = NOTIFY_MAP.get(notification_type)
    if not entry:
        sys.exit(0)

    sound, text, subtext = entry
    try:
        send_to_flipper(sound, text, subtext)
    except Exception:
        pass

    sys.exit(0)


if __name__ == "__main__":
    main()
