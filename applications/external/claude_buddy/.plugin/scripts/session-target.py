#!/usr/bin/env python3
"""Detect and register the current Claude runner target with the bridge."""

from __future__ import annotations

import hashlib
import json
import os
import socket
import subprocess
import sys


TERM_PROGRAM_APP_NAMES = {
    # macOS terminals
    "Apple_Terminal": "Terminal",
    "Terminal": "Terminal",
    "Terminal.app": "Terminal",
    "iTerm.app": "iTerm",
    "iTerm2": "iTerm",
    # cross-platform
    "vscode": "Visual Studio Code",
    "WarpTerminal": "Warp",
    "WezTerm": "WezTerm",
    "Hyper": "Hyper",
    "Ghostty": "Ghostty",
    # Linux terminals
    "gnome-terminal": "GNOME Terminal",
    "konsole": "Konsole",
    "xterm": "XTerm",
    "alacritty": "Alacritty",
    "kitty": "kitty",
    "tilix": "Tilix",
    "xfce4-terminal": "Xfce Terminal",
}


def _normalize_tty(value: str) -> str:
    value = (value or "").strip()
    if not value or value == "??":
        return ""
    if value.startswith("/dev/"):
        return value
    return f"/dev/{value}"


def detect_tty() -> str:
    for fd in (0, 1, 2):
        try:
            return _normalize_tty(os.ttyname(fd))
        except OSError:
            pass

    pid = os.getpid()
    seen: set[int] = set()
    while pid > 1 and pid not in seen:
        seen.add(pid)
        try:
            ppid = subprocess.check_output(
                ["ps", "-o", "ppid=", "-p", str(pid)],
                stderr=subprocess.DEVNULL,
                text=True,
            ).strip()
            tty = subprocess.check_output(
                ["ps", "-o", "tty=", "-p", str(pid)],
                stderr=subprocess.DEVNULL,
                text=True,
            ).strip()
        except Exception:
            break

        normalized_tty = _normalize_tty(tty)
        if normalized_tty:
            return normalized_tty

        try:
            pid = int(ppid)
        except ValueError:
            break

    return ""


def build_target() -> dict[str, str]:
    term_program = (os.environ.get("TERM_PROGRAM") or "").strip()
    target = {
        "app_name": TERM_PROGRAM_APP_NAMES.get(term_program, term_program),
        "term_program": term_program,
        "term_session_id": (os.environ.get("TERM_SESSION_ID") or "").strip(),
        "iterm_session_id": (os.environ.get("ITERM_SESSION_ID") or "").strip(),
        "tty": detect_tty(),
        # X11 window ID — set by VTE-based terminals (gnome-terminal, kitty, etc.)
        # Used by XdotoolInputBackend on Linux to focus the correct window.
        "window_id": (os.environ.get("WINDOWID") or "").strip(),
    }
    material = json.dumps(target, sort_keys=True, separators=(",", ":")).encode()
    target["session_key"] = hashlib.sha1(material).hexdigest()[:16]
    return target


def send_action(socket_path: str, action: str, target: dict[str, str]) -> int:
    payload = {"action": action, **target}
    try:
        client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        client.connect(socket_path)
        client.sendall(json.dumps(payload).encode())
        client.shutdown(socket.SHUT_WR)
        client.recv(65536)
        client.close()
    except Exception:
        return 1
    return 0


def main(argv: list[str]) -> int:
    if len(argv) != 3 or argv[1] not in {"register_target", "release_target"}:
        print(
            "usage: session-target.py <register_target|release_target> <socket>",
            file=sys.stderr,
        )
        return 2

    socket_path = argv[2]
    if not os.path.exists(socket_path):
        return 0

    target = build_target()
    if not any(
        (
            target["app_name"],
            target["tty"],
            target["term_session_id"],
            target["iterm_session_id"],
        )
    ):
        return 0

    return send_action(socket_path, argv[1], target)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
