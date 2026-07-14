---
name: flipper-notify
description: Send a custom sound, vibration, and text notification to the Flipper Zero via the flipper-claude-buddy bridge daemon (requires the bridge to be running).
allowed-tools: Bash
---

# Flipper Zero Notify

Send a notification to the connected Flipper Zero via the host bridge daemon.

The bridge listens on a Unix socket at `/tmp/claude-flipper-bridge.sock`.

## Notify (sound + vibration + two-line text)

```bash
echo '{"action":"notify","sound":"SOUND","vibro":VIBRO,"text":"TITLE","subtext":"SUBTITLE"}' \
  | nc -U /tmp/claude-flipper-bridge.sock
```

- `SOUND` — one of the sound names below
- `VIBRO` — `true` to vibrate, `false` for silent
- `TITLE` / `SUBTITLE` — up to 20 characters each; omit or leave empty if not needed

## Sound reference

| Sound | Description |
|---|---|
| `success` | Ascending C5-E5-G5 chime + green flash |
| `error` | Low double beep G4-G4 + red flash |
| `alert` | Single E5 blip + cyan flash |
| `ready` | Ascending C5-E5-G5 + cyan flash |
| `connect` | Startup chord C5-E5-G5-C6 + green solid (persistent) |
| `disconnect` | Shutdown chord C6-G5-E5-C5 + red flash |
| `interrupt` | Descending E5-C5 + yellow solid (persistent) |
| `session_end` | Descending C6-G5-E5-C5 + yellow flash |
| `led_working` | Blue solid LED, no sound (persistent) |
| `led_off` | Turn LED off, no sound |
| `voice_start` | High ding A5 + red blink (persistent) |
| `voice_start_led` | Red blink only, no sound (persistent) |
| `voice_stop` | Low dong C5 + red flash then LED off |
| `voice_stop_quiet` | LED reset only, no sound |

## Examples

Task completed:
```bash
echo '{"action":"notify","sound":"success","vibro":true,"text":"Done","subtext":"Tests passed"}' \
  | nc -U /tmp/claude-flipper-bridge.sock
```

Error occurred:
```bash
echo '{"action":"notify","sound":"error","vibro":true,"text":"Build failed","subtext":""}' \
  | nc -U /tmp/claude-flipper-bridge.sock
```

Silent status update:
```bash
echo '{"action":"notify","sound":"alert","vibro":false,"text":"Thinking...","subtext":""}' \
  | nc -U /tmp/claude-flipper-bridge.sock
```

## Rules

- Keep text under 20 characters per line
- Use `success` for completions, `error` for failures, `alert` for information
- Use `vibro: true` for important notifications that need immediate attention
- Sounds marked "persistent" leave the LED on until the next notification resets it
- If the bridge is not running, `nc` will exit silently with no error shown to the user
