# Protocol: Android Keyboard Bridge

## Overview

| Layer | Role |
|-------|------|
| Android app | Landscape keyboard UI, builds HID events |
| BLE transport | Flipper Serial GATT service (hijacked by FAP) |
| Flipper FAP | Parses frames, applies USB HID to PC |
| USB HID | Key press/release to host |

## BLE GATT — Flipper Serial Service

The FAP hijacks the built-in Serial-over-BLE profile (same as the official mobile app RPC channel).

| Role | UUID |
|------|------|
| Service | `8fe5b3d5-2e7f-4a98-2a48-7acc60fe0000` |
| TX (phone → Flipper, write) | `19ed82ae-ed21-4c9d-4145-228e62fe0000` |
| RX (Flipper → phone, notify) | `19ed82ae-ed21-4c9d-4145-228e61fe0000` |
| Flow control (optional) | `19ed82ae-ed21-4c9d-4145-228e63fe0000` |

**Advertising name:** normal Flipper BLE name (Serial profile). Pair in system Bluetooth before connecting from the app.

### Connection flow

1. Pair phone with Flipper in system Bluetooth settings.
2. Save Flipper MAC in the Android app Settings.
3. Launch **Android KB Bridge** FAP on Flipper (BLE Serial + USB HID).
4. Tap the app’s BLE button; GATT discovers Serial service and becomes READY.
5. App writes key-event frames to the phone→Flipper write characteristic (`WRITE_NO_RESPONSE`), with an internal write queue so down/up are not dropped.

> Flow-control characteristic subscription is optional at keyboard rates. The FAP notifies buffer-empty from its main loop (not from the RX callback) to avoid a mutex deadlock with the Serial service.

## Frame format

All frames share the same header; the `event` byte selects keyboard or mouse.

| Offset | Size | Value | Description |
|--------|------|-------|-------------|
| 0 | 1 | `0xFB` | Magic byte 0 |
| 1 | 1 | `0x4B` | Magic byte 1 ("FBK") |
| 2 | 1 | `0x03` | Payload length (always 3) |
| 3 | 1 | `event` | See table below |
| 4–5 | 2 | — | Event-specific payload |

### Keyboard events

| `event` | Byte 4 | Byte 5 |
|---------|--------|--------|
| `0x01` key down | `mods` | `keycode` |
| `0x02` key up | `mods` | `keycode` |

```text
FB 4B 03 [01|02] [mods] [keycode]
```

Android sends `down`, then after a short hold (~60 ms) `up` for the same `(mods, keycode)`.

The Flipper FAP applies keyboard HID with a small gap between commands so the host sees the press.

### Mouse / touchpad events

| `event` | Byte 4 | Byte 5 |
|---------|--------|--------|
| `0x10` move | `dx` (int8) | `dy` (int8) |
| `0x11` button down | button mask | `0` |
| `0x12` button up | button mask | `0` |
| `0x13` scroll | delta (int8) | `0` |

```text
FB 4B 03 10 [dx] [dy]
FB 4B 03 11 [btn] 00
FB 4B 03 12 [btn] 00
FB 4B 03 13 [delta] 00
```

Button mask (USB HID mouse):

| Bit | Mask | Button |
|-----|------|--------|
| 0 | `0x01` | Left |
| 1 | `0x02` | Right |
| 2 | `0x04` | Middle / wheel |

Mouse moves are applied without the keyboard inter-event gap so the cursor stays responsive.

On BLE disconnect / FAP exit, Flipper releases all keys and mouse buttons.

### Modifier bitmask (`mods`)

| Bit | Mask | Key |
|-----|------|-----|
| 0 | `0x01` | Left Ctrl |
| 1 | `0x02` | Left Shift |
| 2 | `0x04` | Left Alt |
| 3 | `0x08` | Left GUI (Win/Cmd) |
| 4 | `0x10` | Right Ctrl |
| 5 | `0x20` | Right Shift |
| 6 | `0x40` | Right Alt |
| 7 | `0x80` | Right GUI |

### Key codes

USB HID Usage Page 0x07 (keyboard). Examples:

| Key | Code |
|-----|------|
| `a`–`z` | `0x04`–`0x1D` |
| `1`–`0` | `0x1E`–`0x27` |
| Enter | `0x28` |
| Escape | `0x29` |
| Backspace | `0x2A` |
| Tab | `0x2B` |
| Space | `0x2C` |

Full table: [USB HID Usage Tables](https://usb.org/sites/default/files/hut1_21.pdf).

## Status codes (future)

Optional status characteristic for bridge health:

| Value | Meaning |
|-------|---------|
| `0x00` | Idle |
| `0x01` | USB connected |
| `0x02` | BLE connected |
| `0x03` | Armed (both connected) |
| `0xFF` | Error / panic release |

## Security notes

- Requires prior BLE pairing (bonding).
- No encryption beyond standard BLE link layer in MVP.
- Flipper only accepts writes while FAP is running.
