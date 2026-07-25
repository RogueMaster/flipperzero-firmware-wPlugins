# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run Commands

### Flipper App (C)
```bash
# Build (requires ufbt: pip3 install ufbt)
cd flipper-app && ufbt build

# Build and flash to connected Flipper (stop any running bridge first)
cd flipper-app && ufbt launch

# Or use the script
./scripts/build-flipper.sh           # build only
./scripts/build-flipper.sh --flash   # build + flash

# C protocol unit tests (desktop)
make -C flipper-app/tests
```

### Host Bridges (Python)

Three independent bridge packages share the same serial protocol:

| Agent | Package | Socket | Default `HOST_TYPE` |
|-------|---------|--------|---------------------|
| Claude Code | `plugin/host-bridge/` | `/tmp/claude-flipper-bridge.sock` | `claude` |
| Codex | `flipper-codex-buddy/host-bridge/` | `/tmp/codex-flipper-bridge.sock` | `codex` |
| Cursor | `flipper-cursor-buddy/host-bridge/` | `/tmp/cursor-flipper-bridge.sock` | `cursor` |

```bash
# Claude Code bridge (editable install)
cd plugin/host-bridge && pip3 install -e .
python3 -m bridge --transport usb

# Cursor bridge
cd flipper-cursor-buddy/host-bridge && pip3 install -e .
python3 -m bridge --transport usb

# Codex bridge
cd flipper-codex-buddy/host-bridge && pip3 install -e .
python3 -m bridge --transport usb
```

**Key environment overrides**

```bash
FLIPPER_TRANSPORT=usb              # auto, usb, or ble
FLIPPER_SERIAL_PORT=/dev/ttyACM0   # explicit USB port
FLIPPER_HOST_TYPE=claude           # claude, codex, or cursor (bridge identity)
FLIPPER_BT_NAME="Flipper"          # BLE device name prefix
FLIPPER_LOG_LEVEL=debug            # verbose logs
```

### Testing

```bash
# Python bridge tests
cd plugin/host-bridge && python3 -m pytest tests/
cd flipper-cursor-buddy/host-bridge && python3 -m pytest tests/
cd flipper-codex-buddy/host-bridge && python3 -m pytest tests/

# IPC smoke test (Claude socket)
echo '{"action":"notify","sound":"success","vibro":true,"text":"Test","subtext":""}' \
  | nc -U /tmp/claude-flipper-bridge.sock
```

## Architecture

```
Flipper Zero (flipper-app/, C)
  ↕ USB CDC serial  OR  BLE serial
Host Bridge (Python daemon, per agent)
  ↕ Unix socket  /tmp/<agent>-flipper-bridge.sock
Hook scripts (plugin/, flipper-codex-buddy/, flipper-cursor-buddy/)
```

**Components**

1. **`flipper-app/`** — Flipper Zero FAP (C). Button input, audio/haptic feedback, UI. Auto-selects USB or BLE at startup based on cable state and settings.

2. **`plugin/host-bridge/`** — Claude Code bridge. Asyncio daemon bridging serial ↔ Unix socket. Started by Claude Code `sessionStart` hook.

3. **`plugin/`** — Claude Code plugin (hooks, scripts, skills).

4. **`flipper-codex-buddy/`** — Codex plugin + bridge + slash commands (`/bridge-on`, etc.).

5. **`flipper-cursor-buddy/`** — Cursor hooks + bridge. Installed into `.cursor/hooks.json` via `install-cursor-hooks.sh`.

Only **one bridge** should own the Flipper serial port at a time.

## Threading Model — Critical

### Flipper App
- **BLE/serial RX callback** runs on a worker thread. It must NOT call any UI functions or `transport_send`.
- The callback queues parsed `ProtocolMessage` into a `FuriMessageQueue` and signals the GUI thread via `view_dispatcher_send_custom_event`.
- **GUI thread** (the Furi event loop) drains the queue and calls `transport_send` safely.
- Calling `ble_profile_serial_tx` from inside `bt_serial_event_cb` deadlocks on Momentum firmware — always defer TX to the GUI thread.

### Host Bridge
- Single asyncio event loop. All transport I/O, IPC, and ping tasks are async coroutines.
- `serial_conn.py` runs a reconnect loop that re-establishes the transport on disconnect.
- `transport_bt.py`: `readline()` must handle disconnect without blocking — checks `_closed` flag before and after `_rx_event.clear()`.

## Protocol

JSON lines (`\n`-terminated) over serial (USB or BLE):
```json
{"v": 1, "t": "<type>", "d": {...}}
```

**Host → Flipper:** `ping`, `notify`, `state`, `status`, `menu`, `perm`, `usage` (context pressure)
**Flipper → Host:** `hello`, `pong`, `enter`, `esc`, `voice`, `down`, `cmd`, `perm_resp`

**`state` message** — session + host identity:
```json
{"t":"state","d":{"claude":true,"host":"cursor"}}
```
`host` is optional for backward compatibility (`claude`, `codex`, `cursor`).

The Flipper sends `hello` on the first received `ping` (from the GUI thread), not at BLE connect time. This is because the host's CCCD write (enabling notifications) hasn't happened yet when the connection status callback fires.

## BLE Transport Details

- RX (Flipper→host, notify): `19ed82ae-ed21-4c9d-4145-228e61fe0000`
- TX (host→Flipper, write): `19ed82ae-ed21-4c9d-4145-228e62fe0000`
- Host writes with `response=False` (write-without-response), chunk size capped to `negotiated_mtu - 3`
- `BT_WRITE_CHUNK = 128` in `config.py` (runtime cap applies)

## Key Files

| File | Purpose |
|------|---------|
| `flipper-app/claude_buddy.c` | App entry point, GUI event loop, message dispatch |
| `flipper-app/ui.c` | Display rendering, animations, host label, button handlers |
| `flipper-app/protocol.c` | JSON parse/build for all message types |
| `flipper-app/nus_state.c` | Claude Desktop BLE state machine |
| `plugin/host-bridge/bridge/daemon.py` | Claude bridge event loop |
| `flipper-cursor-buddy/host-bridge/bridge/daemon.py` | Cursor bridge (`cursor_connect` IPC) |
| `flipper-codex-buddy/host-bridge/bridge/daemon.py` | Codex bridge |
| `plugin/scripts/context_usage.py` | Context/session pressure → `usage` IPC messages |

## Runtime Files

| Agent | Socket | PID | Log |
|-------|--------|-----|-----|
| Claude | `/tmp/claude-flipper-bridge.sock` | `/tmp/claude-flipper-bridge.pid` | `/tmp/claude-flipper-bridge.log` |
| Codex | `/tmp/codex-flipper-bridge.sock` | `/tmp/codex-flipper-bridge.pid` | `/tmp/codex-flipper-bridge.log` |
| Cursor | `/tmp/cursor-flipper-bridge.sock` | `/tmp/cursor-flipper-bridge.pid` | `/tmp/cursor-flipper-bridge.log` |

Additional Claude-only files:
- Session refcount: `/tmp/claude-flipper-bridge.refcount`
- Turn stats: `/tmp/claude-flipper-turn-stats.json`
- Skip-stop flag: `/tmp/claude-flipper-skip-stop.flag`
- BT name cache: `$PLUGIN_DATA/bt_name`

Inspect bridge activity: `tail -f /tmp/<agent>-flipper-bridge.log`

## Platform Notes

| Feature | macOS | Linux |
|---------|-------|-------|
| USB transport | `/dev/cu.usbmodem*` | `/dev/ttyACM*` (auto-detected) |
| BLE transport | ✓ | experimental |
| Keystroke forwarding | AppleScript (`osascript`) | `xdotool` (X11 only) |
| Wayland keystroke | ✗ | not yet supported (`ydotool` needed) |
| Dictation | macOS native | disabled by default; `FLIPPER_DICTATION_BACKEND=custom` |

On Linux, `WINDOWID` (VTE terminals like gnome-terminal and kitty) is used by `xdotool` to focus the correct window.

## Command Menu System

The Flipper command menu is populated from optional shortcut files (project overrides user):

**Claude Code**
1. `~/.claude/flipper-commands.txt`
2. `$PROJECT_DIR/.claude/flipper-commands.txt`
3. Auto-discovered from `.claude/commands/`

**Cursor:** `~/.cursor/flipper-commands.txt`, `$PROJECT_DIR/.cursor/flipper-commands.txt`

Commands are sent as a pipe-delimited `menu` message; the bridge stores abbreviations in `_cmd_map`.

## Releasing a New Version

1. **Commit any uncommitted changes first** — the version bump should be its own clean commit.
2. **`flipper-app/CHANGELOG.md`** — add a new `## vX.Y` section at the top.
3. **`flipper-app/application.fam`** — update `fap_version`
4. **`flipper-app/ui.c`** — update version string on the About page
5. **`plugin/.claude-plugin/plugin.json`** — update `version`
6. **`plugin/host-bridge/pyproject.toml`** — update `version`
7. Commit, push, then tag:
   ```bash
   git tag X.Y
   git push origin X.Y
   ```
   CI (`.github/workflows/build-fap.yml`) creates the GitHub release and attaches the built `.fap`.
