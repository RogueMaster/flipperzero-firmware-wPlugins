# FlipperTrack

Proof-of-concept FAP for testing how productivity monitoring software responds to simulated HID activity.

Periodically generates mouse jitter, window-switch keystrokes, and fake typing bursts over USB HID — without touching the monitored host's software.

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

## Use case

Security research and authorized testing of endpoint monitoring tools. Run against systems you own or have explicit written permission to test.
