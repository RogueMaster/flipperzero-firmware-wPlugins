# Claupper

A Flipper Zero remote control for [Claude Code](https://docs.anthropic.com/en/docs/claude-code). One-handed coding with 5 buttons: approve (1/2/3), enter, and voice dictation.

## Install

```bash
npx claupper
```

Local-only setup. Copies three things from the npm package to your home directory; does **not** touch your Flipper Zero:

| What | Where | Purpose |
|------|-------|---------|
| `SKILL.md` | `~/.claude/skills/claupper/` | Registers the `/claupper` Claude Code skill (3-option decision format) |
| `claude_remote_ble.fap` + `claude_remote_usb.fap` | `~/claupper/` | Pre-built Flipper apps; you copy one to your SD card |
| 7 macro preset `.txt` files | `~/claupper/macros/` | Editable command packs for the Macros menu |

After running this you still need to **manually copy the `.fap` to your Flipper SD card** (see below). Running `npx claupper` again is safe — it just overwrites the same files.

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
| Stock | `~/claupper/claude_remote_usb.fap` | `apps/USB/` |

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
