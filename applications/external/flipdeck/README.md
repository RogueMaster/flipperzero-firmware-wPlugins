# FlipDeck

**Turn your Flipper Zero into a USB macro pad.**

FlipDeck transforms your Flipper Zero into a programmable USB macro pad with media controls, custom shortcuts, and an extensible plugin system. Media keys work **instantly** on any OS — no drivers, no host software. Custom keys trigger configurable actions through a lightweight host daemon.

```
┌──────────────────────────┐             ┌───────────────────────────┐
│  Flipper Zero             │   USB HID   │  Computer                 │
│                           │ ──────────▶ │                           │
│  ┌────┬────┬────┐        │  Media keys │  Play/Pause, Volume, etc. │
│  │ >  │ >> │ << │ Media  │  work nativ │  (no host needed)         │
│  ├────┼────┼────┤        │             │                           │
│  │ +  │ -  │ X  │        │  F13-F24    │  Shell, Python, HTTP,     │
│  └────┴────┴────┘        │  keys       │  Shortcuts, Paste, ...    │
│  ┌────┬────┬────┐        │             │  (via host daemon)        │
│  │ $  │ @  │ [] │ Custom │             │                           │
│  ├────┼────┼────┤        │             │  Web UI: localhost:7433   │
│  │ *  │ P  │ !  │        │             │  Config, actions, status  │
│  └────┴────┴────┘        │             │                           │
└──────────────────────────┘             └───────────────────────────┘
```

## Features

- **Multi-page grid** – up to 8 pages × 6 buttons, page names in header, swipe-to-switch
- **Native media keys** – Play/Pause, Next, Prev, Vol+/-, Mute, Brightness work without host
- **Custom F-keys (F13-F24)** – trigger any action via the host daemon
- **Modular action system** – shell, python, bash, keypress, open, paste, http, osascript, shortcut
- **Plugin architecture** – drop a `.py` file to add new action types
- **Background daemon** – install as macOS LaunchAgent or Linux systemd service
- **Web UI** – manage actions, edit config, upload to Flipper, monitor status
- **Cross-platform** – macOS + Linux (host side), any OS (media keys)
- **Flipper App Catalog ready** – proper manifest, icon, screenshots

## Quick Start

### 1. Build & install the Flipper app

```bash
# Prerequisites: ufbt (pip install ufbt)
cd flipper
ufbt build          # Compile
ufbt launch         # Deploy + launch on connected Flipper
```

### 2. Done!

Media buttons work immediately. Open Spotify/Music/YouTube and press Play.

### 3. (Optional) Install host for custom actions

```bash
cd host
pip install -e .    # Install the host package
flipdeck start --web  # Start daemon + web UI
```

Open **http://localhost:7433** to manage actions, or configure via `~/.config/flipdeck/actions.yaml`.

## Flipper Config

The Flipper reads button layout from `/ext/apps_data/flipdeck/config.txt`:

```
# page:Media
PLAY:Play:>
NEXT:Next:>>
PREV:Prev:<<
VOLUP:Vol+:+
VOLDN:Vol-:-
MUTE:Mute:X

# page:Custom
F13:Term:$
F14:Web:@
F15:Shot:[]
```

Format: `ACTION:Label:Symbol` — one per line, up to 6 per page.

### Available actions

| Key | Type | Description | Needs host? |
|---|---|---|---|
| `PLAY` | Media | Play/Pause | No |
| `NEXT` | Media | Next track | No |
| `PREV` | Media | Previous track | No |
| `VOLUP` | Media | Volume up | No |
| `VOLDN` | Media | Volume down | No |
| `MUTE` | Media | Toggle mute | No |
| `BRTUP` | Media | Brightness up | No |
| `BRTDN` | Media | Brightness down | No |
| `F13`-`F24` | Keyboard | Custom F-keys | Yes |

Upload via CLI: `flipdeck upload config/config.txt`
Upload via web: http://localhost:7433 → Config tab

## Flipper Controls

| Button | Action |
|---|---|
| D-Pad | Navigate grid |
| Left/Right at edge | Switch page |
| OK | Send HID key |
| Back (short) | Previous page |
| Back (long hold) | Exit app |

## Host Actions

Configure in `actions.yaml` or via the web UI:

```yaml
actions:
  f13:
    name: Open Terminal
    type: open
    target: ""
    app: Terminal
    enabled: true

  f14:
    name: Deploy Script
    type: bash
    script: ~/scripts/deploy.sh
    enabled: true

  f15:
    name: Screenshot
    type: keypress
    keys: cmd+shift+4
    enabled: true

  f16:
    name: API Trigger
    type: http
    url: https://api.example.com/webhook
    method: POST
    headers:
      Authorization: Bearer xxx
    body: '{"action": "deploy"}'
    enabled: true
```

### Built-in action types

| Type | Description | Fields |
|---|---|---|
| `shell` | Run shell command | `command`, `wait` |
| `python` | Run Python script | `script`, `args` |
| `bash` | Run bash script | `script` |
| `keypress` | Simulate key combo | `keys` (e.g. `cmd+shift+4`) |
| `open` | Open file/app/URL | `target`, `app` |
| `paste` | Type/paste string | `text` |
| `http` | HTTP request | `url`, `method`, `headers`, `body` |
| `osascript` | AppleScript (macOS) | `script` |
| `shortcut` | Shortcuts.app (macOS) | `name` |

### Custom plugins

Drop a `.py` file in `~/.config/flipdeck/plugins/`:

```python
ACTION_TYPE = "notify"

def execute(cfg):
    title = cfg.get("title", "FlipDeck")
    message = cfg.get("message", "Hello!")
    # your logic here
    return True
```

See `host/plugins_example/` for a full example.

## Background Service

Install as autostart service:

```bash
flipdeck install
```

- **macOS**: LaunchAgent (`~/Library/LaunchAgents/com.flipdeck.host.plist`)
- **Linux**: systemd user service (`~/.config/systemd/user/flipdeck.service`)

Logs: `~/Library/Logs/FlipDeck/` (macOS) or `journalctl --user -u flipdeck` (Linux)

## CLI Reference

```
flipdeck                    # Start daemon + web UI (default)
flipdeck start              # Start daemon (key listener only)
flipdeck start --web        # Start daemon + web UI
flipdeck web                # Web UI only (no key listener)
flipdeck upload config.txt  # Upload config to Flipper
flipdeck actions            # List configured actions
flipdeck install            # Install as background service
```

## Project Structure

```
flipdeck/
├── flipper/                    # Flipper FAP source
│   ├── application.fam         # App manifest
│   ├── flipdeck.c              # Multi-page HID macro pad
│   └── icon.png                # 10×10 app icon
├── host/                       # Host companion (Python)
│   ├── flipdeck_host/
│   │   ├── cli.py              # CLI entry point
│   │   ├── daemon.py           # Key listener daemon
│   │   ├── actions.py          # Modular action system
│   │   ├── web.py              # Flask web UI + API
│   │   ├── flipper.py          # Serial upload utility
│   │   ├── service.py          # Autostart installer
│   │   └── static/index.html   # Web dashboard
│   ├── plugins_example/
│   │   └── notify.py           # Example plugin
│   └── pyproject.toml          # pip-installable package
├── config/
│   └── config.txt              # Default Flipper config
├── Makefile                    # Build/install shortcuts
└── README.md
```

## Requirements

**Flipper side:**
- Flipper Zero with official firmware
- ufbt (`pip install ufbt`)

**Host side (optional, only for F13-F24 custom actions):**
- Python 3.9+
- macOS: Accessibility permission for Terminal/Python
- Linux: `xdotool` for keypress simulation, or run as root for evdev

## License

MIT
