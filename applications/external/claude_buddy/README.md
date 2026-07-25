# Flipper CLI Agent Buddy

Turn your Flipper Zero into a physical companion for **Claude Code**, **OpenAI Codex**, and **Cursor** CLI agents. Get tactile feedback for every AI action, see which agent is connected at a glance, and control your terminal session with real buttons instead of typing.

> Supports macOS and Linux. Windows is not tested.

## What it does

**You feel what the agent is doing.**
Every significant event triggers a distinct sound and vibration pattern on your Flipper, so you always know status even when you are not looking at the screen.

**You see which agent is active.**
The status header shows **Claude**, **Codex**, or **Cursor** in the top-left corner when a host bridge is connected.

**You control the agent with physical buttons.**
Interrupt a runaway task, submit a prompt, trigger voice dictation, or open the command menu — all without touching the keyboard.

**The character reacts to context.**
Animations and optional Ctx/Lim meters reflect tool use, compaction, permission outcomes, and context pressure.

## Supported agents

| Agent | Package | Install doc | IPC socket |
|-------|---------|-------------|------------|
| **Claude Code** | `plugin/` | [Setup below](#claude-code) | `/tmp/claude-flipper-bridge.sock` |
| **OpenAI Codex** | `flipper-codex-buddy/` | [Codex README](flipper-codex-buddy/README.md) | `/tmp/codex-flipper-bridge.sock` |
| **Cursor** | `flipper-cursor-buddy/` | [Cursor README](flipper-cursor-buddy/README.md) | `/tmp/cursor-flipper-bridge.sock` |
| **Claude Desktop** | *(none — built into FAP)* | [Desktop mode](#claude-desktop-ble) | BLE only |

Each CLI agent ships its own hook scripts and Python host bridge. All bridges share the same Flipper application and serial protocol.

> **Important:** Only one bridge should use the Flipper USB serial port at a time. Stop or disable other bridges before switching agents.

## Flipper app modes

Claude Buddy runs in one of two transport modes, switchable from the on-device menu (long-press **Right → MENU**, then the top row):

- **Claude Code (USB/BLE)** (default) — pairs with a CLI agent through a host bridge over USB or BLE. Full keystroke forwarding: Enter, Esc, voice dictation, command menu, etc.
- **Claude Desktop (BLE)** — talks directly to the Claude Desktop app over Anthropic's [Hardware Buddy](https://github.com/anthropics/claude-desktop-buddy) protocol. No plugin, no host bridge. Live session status, token counts, transcript, and on-device permission prompts.

See [flipper-app/README.md](flipper-app/README.md) for the full button map and mode details.

## Buttons

| Button | Action (Bridge mode) |
|--------|----------------------|
| UP | Start / stop voice dictation |
| UP (hold) | Hold Space for voice input |
| LEFT | Interrupt (Esc) |
| LEFT (hold) | Send Ctrl+C |
| RIGHT | Open command menu |
| RIGHT (hold) | Open info menu |
| OK | Submit Enter |
| OK (hold) | Type "yes" and submit |
| DOWN | Send Down arrow |
| DOWN (hold) | Toggle mute |
| BACK | Send Backspace |
| BACK (hold) | Exit |

## Setup

### 1. Install the Flipper app

Download `claude_buddy.fap` from the [latest release](../../releases/latest) or build from source:

```bash
cd flipper-app && ufbt build
# Flash to a connected device (stop any running bridge first):
ufbt launch
```

Copy the `.fap` to `apps/USB/` on the SD card, or flash via `ufbt launch`.

Launch **Applications → USB → Claude Buddy** on the Flipper. You should hear the startup fanfare when the transport connects.

### 2. Pick your CLI agent

#### Claude Code

> **Requires Python 3.10+.**

```bash
claude plugin marketplace add jxw1102/flipper-claude-buddy
claude plugin install flipper-claude-buddy@flipper-claude-buddy
```

The plugin starts the host bridge automatically on each Claude Code session. Configure transport in the plugin settings (`auto`, `usb`, or `ble`).

#### OpenAI Codex

```bash
./flipper-codex-buddy/scripts/install-codex-plugin.sh
```

Then open `/hooks` in Codex, trust the plugin hooks, and start a new thread. See [flipper-codex-buddy/README.md](flipper-codex-buddy/README.md).

#### Cursor

```bash
chmod +x flipper-cursor-buddy/scripts/install-cursor-hooks.sh
./flipper-cursor-buddy/scripts/install-cursor-hooks.sh
```

Open **Settings → Hooks** in Cursor, confirm hooks are loaded, and start a new agent session. See [flipper-cursor-buddy/README.md](flipper-cursor-buddy/README.md).

### Claude Desktop (BLE)

1. On the Flipper, switch to **Claude Desktop (BLE)** in the info menu.
2. In Claude Desktop: **Help → Troubleshooting → Enable Developer Mode**.
3. **Developer → Open Hardware Buddy** and select your Flipper. Grant Bluetooth permission on first connect.

Once paired, Claude Desktop auto-reconnects when both sides are online.

## Connection

CLI bridges connect over USB (plug-and-play) or Bluetooth LE. USB is preferred when the cable is plugged in; bridges fall back to BLE automatically when configured for `auto`.

**macOS — Bluetooth pairing:** accept the pairing prompt on first BLE connect. If pairing fails after a firmware flash, remove the Flipper from System Settings → Bluetooth and re-pair.

**macOS — Accessibility (keystroke forwarding):** grant your terminal app Accessibility permission in System Settings → Privacy & Security → Accessibility. Without it, status updates work but button presses do nothing.

**Linux — USB:** Flipper appears as `/dev/ttyACM*`. Add your user to `dialout` if you get permission errors:

```bash
sudo usermod -aG dialout $USER   # log out and back in
export FLIPPER_TRANSPORT=usb     # recommended on Linux
```

**Linux — Keystroke forwarding:** requires `xdotool` on X11 (`sudo apt install xdotool`). Wayland is not yet supported.

**Linux — BLE:** experimental; USB is the reliable path today.

## Host identification protocol

Bridges report which agent is connected via the `host` field on `state` messages:

```json
{"v":1,"t":"state","d":{"claude":true,"host":"cursor"}}
```

Allowed values: `claude`, `codex`, `cursor`. The Flipper shows the label in the status header. Older bridges without `host` still work; Desktop BLE mode defaults to **Claude**.

## Troubleshooting

| Problem | Fix |
|---------|-----|
| Flipper not found over USB | Stop other bridges and apps using the serial port (qFlipper, another agent bridge). Set `FLIPPER_SERIAL_PORT` explicitly. |
| Wrong agent label / no label | Restart the bridge after flashing a new FAP. Ensure the bridge sends `host` in `state` messages. |
| No sound on task complete | Check bridge log: `tail -f /tmp/<agent>-flipper-bridge.log` |
| Buttons do nothing (macOS) | Grant Accessibility permission to your terminal app. |
| Buttons do nothing (Linux) | Install `xdotool`; focus the terminal window. |
| Parse errors / CLI banner in log | The Flipper is running the CLI app, not Claude Buddy — open the buddy FAP and restart the bridge. |

Bridge logs:

| Agent | Log file |
|-------|----------|
| Claude Code | `/tmp/claude-flipper-bridge.log` |
| Codex | `/tmp/codex-flipper-bridge.log` |
| Cursor | `/tmp/cursor-flipper-bridge.log` |

## Repository layout

```
flipper-app/              Flipper Zero FAP (C)
plugin/                   Claude Code plugin + host bridge
flipper-codex-buddy/      Codex plugin + host bridge
flipper-cursor-buddy/     Cursor hooks + host bridge
```

## Development

See [CLAUDE.md](CLAUDE.md) for build commands, architecture, protocol details, and release checklist.

## Support

If you find this useful, consider [buying the maintainer a coffee](https://ko-fi.com/jxw1102).

## License

MIT — see [LICENSE](LICENSE).
