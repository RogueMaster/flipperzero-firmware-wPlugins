#!/usr/bin/env python3
"""SubagentStart hook: show spawned agent type on Flipper."""

import json
import os
import socket
import sys

SOCKET_PATH = "/tmp/claude-flipper-bridge.sock"


def send_display(text: str, subtext: str = "") -> None:
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(SOCKET_PATH)
    msg = json.dumps({"action": "display", "text": text, "subtext": subtext})
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

    agent_type = hook_input.get("agent_type", "Agent")

    try:
        send_display(f"{agent_type} agent", "started")
    except Exception:
        pass

    sys.exit(0)


if __name__ == "__main__":
    main()
