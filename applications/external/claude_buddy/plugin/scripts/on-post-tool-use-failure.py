#!/usr/bin/env python3
"""PostToolUseFailure hook: plays an error sound on the Flipper when a tool call fails."""

import json
import os
import socket
import sys

SOCKET_PATH = "/tmp/claude-flipper-bridge.sock"


def send_to_flipper(sound: str, text: str = "", subtext: str = "") -> None:
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(SOCKET_PATH)
    msg = json.dumps({"action": "notify", "sound": sound, "vibro": True, "text": text, "subtext": subtext})
    s.sendall(msg.encode())
    s.shutdown(socket.SHUT_WR)
    s.recv(4096)
    s.close()


def extract_subtext(hook_input: dict) -> str:
    """Extract a short, readable error summary for the Flipper display."""
    error = hook_input.get("error", "")
    tool_input = hook_input.get("tool_input", {})
    tool_name = hook_input.get("tool_name", "")

    # For Bash failures, show the first meaningful line of the error
    if tool_name == "Bash":
        for line in error.splitlines():
            line = line.strip()
            # Skip the generic "Exit code N" prefix
            if line.startswith("Exit code"):
                continue
            if line:
                return line[:21]

    # For file tools, show the basename
    if tool_name in ("Edit", "Write", "Read"):
        path = tool_input.get("file_path", "")
        if path:
            return os.path.basename(path)[:21]

    # Generic: first non-empty line of error
    for line in error.splitlines():
        line = line.strip()
        if line:
            return line[:21]

    return "Failed"


def main():
    if not os.path.exists(SOCKET_PATH):
        sys.exit(0)

    try:
        hook_input = json.loads(sys.stdin.read())
    except (json.JSONDecodeError, EOFError):
        sys.exit(0)

    tool_name = hook_input.get("tool_name", "")
    subtext = extract_subtext(hook_input)

    try:
        send_to_flipper("error", f"{tool_name} failed", subtext)
    except Exception:
        pass

    sys.exit(0)


if __name__ == "__main__":
    main()
