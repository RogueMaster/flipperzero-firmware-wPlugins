#!/usr/bin/env python3
"""Extract context/session pressure from hook payloads and push to Flipper."""

from __future__ import annotations

import json
import os
import socket
import sys
from typing import Any

SOCKET_PATH = os.environ.get("FLIPPER_SOCKET", "/tmp/claude-flipper-bridge.sock")
STATE_PATH = os.environ.get("FLIPPER_CONTEXT_STATE", "/tmp/claude-flipper-context.json")


def _clamp_pct(value: Any) -> int | None:
    if value is None:
        return None
    try:
        pct = int(round(float(value)))
    except (TypeError, ValueError):
        return None
    return max(0, min(100, pct))


def _session_pct_from_rate_limits(rate_limits: dict[str, Any]) -> int | None:
    if not isinstance(rate_limits, dict):
        return None
    primary = rate_limits.get("primary")
    if isinstance(primary, dict) and primary.get("used_percent") is not None:
        return _clamp_pct(primary["used_percent"])
    secondary = rate_limits.get("secondary")
    if isinstance(secondary, dict) and secondary.get("used_percent") is not None:
        return _clamp_pct(secondary["used_percent"])
    return None


def _context_pct_from_tokens(tokens: Any, window: Any) -> int | None:
    try:
        tok = float(tokens)
        win = float(window)
    except (TypeError, ValueError):
        return None
    if win <= 0:
        return None
    return _clamp_pct(tok * 100.0 / win)


def extract_usage(hook_input: dict[str, Any]) -> dict[str, int | None]:
    out: dict[str, int | None] = {"context_pct": None, "session_pct": None}

    for key in (
        "usage_percent",
        "context_utilization_pct",
        "context_usage_percent",
        "context_percent",
    ):
        if key in hook_input and hook_input[key] is not None:
            out["context_pct"] = _clamp_pct(hook_input[key])
            break

    tokens = hook_input.get("token_count")
    if tokens is None:
        tokens = hook_input.get("context_tokens_before")
    window = hook_input.get("context_window_size")
    if window is None:
        window = hook_input.get("model_context_window")
    if out["context_pct"] is None:
        out["context_pct"] = _context_pct_from_tokens(tokens, window)

    info = hook_input.get("info")
    if isinstance(info, dict):
        if out["context_pct"] is None:
            total = (info.get("total_token_usage") or {}).get("total_tokens")
            out["context_pct"] = _context_pct_from_tokens(
                total, info.get("model_context_window")
            )
        if out["session_pct"] is None:
            out["session_pct"] = _session_pct_from_rate_limits(
                info.get("rate_limits") or {}
            )

    if out["session_pct"] is None:
        out["session_pct"] = _session_pct_from_rate_limits(
            hook_input.get("rate_limits") or {}
        )

    return out


def compact_level_for(context_pct: int | None, trigger: str = "auto") -> int:
    if context_pct is None:
        return 2 if trigger == "auto" else 1
    if context_pct >= 92:
        return 3
    if context_pct >= 78:
        return 2
    return 1


def load_state() -> dict[str, Any]:
    try:
        with open(STATE_PATH, encoding="utf-8") as handle:
            data = json.load(handle)
            return data if isinstance(data, dict) else {}
    except Exception:
        return {}


def save_state(state: dict[str, Any]) -> None:
    try:
        with open(STATE_PATH, "w", encoding="utf-8") as handle:
            json.dump(state, handle)
    except Exception:
        pass


def send_usage(
    context_pct: int | None = None,
    session_pct: int | None = None,
    compact_level: int | None = None,
) -> None:
    if not os.path.exists(SOCKET_PATH):
        return
    payload: dict[str, Any] = {"action": "usage"}
    if context_pct is not None:
        payload["context_pct"] = context_pct
    if session_pct is not None:
        payload["session_pct"] = session_pct
    if compact_level is not None:
        payload["compact_level"] = compact_level
    if len(payload) == 1:
        return
    client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    client.connect(SOCKET_PATH)
    client.sendall(json.dumps(payload).encode("utf-8"))
    client.shutdown(socket.SHUT_WR)
    client.recv(4096)
    client.close()


def sync_usage(
    hook_input: dict[str, Any] | None = None,
    *,
    compact_level: int | None = None,
    clear_compact: bool = False,
    context_override: int | None = None,
) -> dict[str, Any]:
    state = load_state()
    if hook_input:
        extracted = extract_usage(hook_input)
        if extracted["context_pct"] is not None:
            state["context_pct"] = extracted["context_pct"]
        if extracted["session_pct"] is not None:
            state["session_pct"] = extracted["session_pct"]
    if context_override is not None:
        state["context_pct"] = context_override
    if clear_compact:
        state["compact_level"] = 0
    elif compact_level is not None:
        state["compact_level"] = max(0, min(3, int(compact_level)))

    save_state(state)
    send_usage(
        context_pct=state.get("context_pct"),
        session_pct=state.get("session_pct"),
        compact_level=state.get("compact_level"),
    )
    return state


def main() -> int:
    mode = sys.argv[1] if len(sys.argv) > 1 else "sync"
    raw = sys.stdin.read()
    hook_input: dict[str, Any] = {}
    if raw.strip():
        try:
            hook_input = json.loads(raw)
        except json.JSONDecodeError:
            hook_input = {}

    if mode == "pre_compact":
        trigger = str(hook_input.get("trigger") or "auto")
        extracted = extract_usage(hook_input)
        ctx = extracted["context_pct"]
        sync_usage(
            hook_input,
            compact_level=compact_level_for(ctx, trigger),
        )
        return 0

    if mode == "post_compact":
        ctx = extract_usage(hook_input)["context_pct"]
        if ctx is None:
            ctx = 35
        else:
            ctx = max(25, min(55, int(ctx * 0.45)))
        sync_usage(hook_input, clear_compact=True, context_override=ctx)
        return 0

    sync_usage(hook_input)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
