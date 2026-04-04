# FlipperTrack

USB HID activity simulator for Flipper Zero. Generates periodic mouse movement, keystroke bursts, and window-cycling inputs over USB HID — useful for keeping systems active during presentations, testing HID automation pipelines, or verifying endpoint software behavior in lab environments.

## Controls

| Button | Action |
|--------|--------|
| OK | Toggle Mac / Windows mode (changes Tab modifier) |
| Right | Toggle fake typing on / off |
| Hold Back | Exit and restore USB |

## Behavior

| Event | Interval |
|-------|----------|
| Mouse jitter | Every 2–5 s |
| Ctrl/Cmd+Tab | Every 45–90 s |
| Fake keystrokes | Every 8–20 s (when typing enabled) |

## Build

```
ufbt
```
