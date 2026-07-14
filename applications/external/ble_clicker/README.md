# Flipper Zero BLE Clicker

BLE HID keyboard for voice-driven [Claude Code](https://docs.anthropic.com/en/docs/claude-code) workflow on iTerm2. Turns Flipper Zero into a wireless remote — dictate, submit, navigate panes and tabs without touching the keyboard.

Designed to be held **vertically**.

## Button Layout

```
        ┌─────────┐
        │  [Up]   │  → Escape
   [L]  │  [OK]   │  [R]
   tab  │         │  pane
        │  [Dn]   │  → new Claude
        └─────────┘
           [Back] → Exit

OK hold  — Opt+Space (start dictation)
OK tap   — Enter (submit)
Right    — next pane (Cmd+])
Left     — next tab (Cmd+Shift+])
Up       — Escape
Down     — new split pane + launch `diana claude --recent`
```

## Workflow

1. **Hold OK** — activates macOS dictation (Opt+Space)
2. **Release** — dictated text appears in terminal
3. **Tap OK** — sends Enter, submitting to Claude
4. **Right** — cycle to next iTerm2 split pane (another Claude instance)
5. **Left** — cycle to next tab
6. **Down** — split a new pane and launch a fresh Claude session

## Features

- Vertical screen orientation with adapted UI
- Vibro feedback on every key press
- Visual dictation indicator — screen inverts while Opt+Space is held
- Clean BLE teardown — Bluetooth returns to normal after exit
- BLE device name: **Diana**

## Build

Requires [ufbt](https://github.com/flipperdevices/flipperzero-ufbt):

```bash
pip install ufbt
ufbt
```

The compiled `.fap` will be in `dist/`.

## Install

Connect Flipper Zero via USB:

```bash
ufbt launch
```

## CI

GitHub Actions builds the FAP on every push. Download the latest artifact from the [Actions tab](../../actions).
