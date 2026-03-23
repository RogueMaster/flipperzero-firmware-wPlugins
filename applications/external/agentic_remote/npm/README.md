# Claupper

A Flipper Zero remote control for [Claude Code](https://docs.anthropic.com/en/docs/claude-code). One-handed coding with 5 buttons: approve (1/2/3), enter, and voice dictation.

## Install

```bash
npx claupper
```

This installs:
- **Claude Code skill** — makes Claude present every decision as 3 numbered choices
- **Flipper Zero apps** — BLE and USB `.fap` files ready to copy to your Flipper
- **Macro presets** — pre-built command sets (default, debugging, workflow, etc.)

## Usage

1. Open Claude Code (CLI or desktop)
2. Type `/claupper` to activate remote mode
3. Pick up your Flipper and start coding

Every Claude response becomes:
```
1. [What you most likely want]
2. [Show details first]
3. [Change direction]
```

## Flipper Setup

After running `npx claupper`, copy the right `.fap` to your Flipper's SD card:

| Firmware | File | SD Path |
|----------|------|---------|
| Momentum / Unleashed | `~/claupper/claude_remote_ble.fap` | `apps/Bluetooth/` |
| Stock | `~/claupper/claude_remote_usb.fap` | `apps/Tools/` |

### Macros (optional)

Copy a macro preset to your Flipper:
```
~/claupper/macros/default.txt -> SD/apps_data/claude_remote_ble/macros.txt
```

Available presets: `default`, `minimal`, `barebones`, `debugging`, `workflow`, `review`, `maximalist`

## Uninstall

```bash
npx claupper --uninstall
```

## Links

- [GitHub](https://github.com/Wet-wr-Labs/claupper)
- [Wetware Labs](https://WetwareOfficial.com)

Made by Kasen Sansonetti & the Wetware Labs team.
