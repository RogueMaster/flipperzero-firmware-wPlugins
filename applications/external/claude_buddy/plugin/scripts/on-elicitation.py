#!/usr/bin/env python3
"""Elicitation hook: notifies Flipper when Claude asks the user for input."""

import json
import os
import socket
import sys

SOCKET_PATH = "/tmp/claude-flipper-bridge.sock"


def send_to_flipper(sound: str, text: str, subtext: str) -> None:
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(SOCKET_PATH)
    msg = json.dumps({"action": "notify", "sound": sound, "vibro": True, "text": text, "subtext": subtext})
    s.sendall(msg.encode())
    s.shutdown(socket.SHUT_WR)
    s.recv(4096)
    s.close()


def extract_subtext(hook_input: dict) -> str:
    """Build a short subtext from the elicitation payload."""
    # Prefer MCP server name if present
    mcp = hook_input.get("mcp_server_name", "")
    if mcp:
        return mcp[:21]
    # Fall back to first line of message
    message = hook_input.get("message", "")
    first_line = message.split("\n", 1)[0].strip()
    return first_line[:21] if first_line else "Input needed"


def main():
    if not os.path.exists(SOCKET_PATH):
        sys.exit(0)

    try:
        hook_input = json.loads(sys.stdin.read())
    except (json.JSONDecodeError, EOFError):
        sys.exit(0)

    subtext = extract_subtext(hook_input)

    try:
        send_to_flipper("alert", "Elicitation", subtext)
    except Exception:
        pass

    sys.exit(0)


if __name__ == "__main__":
    main()
