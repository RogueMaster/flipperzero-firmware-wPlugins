# PC Health Remote (Flipper Zero app)

Shows your PC's temperatures and load on the Flipper and warns you with vibration, sound
and a red LED when something goes wrong (overheating, RAM/VRAM/disk full, low battery,
lost link) together with short "what to do" tips.

A small backend on the PC sends telemetry every second (see `../docs/PROTOCOL.md`); the
Flipper answers with a HELLO frame. Two transports: **Bluetooth LE** (default) and **USB CDC**.

## Screens
- **Waiting** - transport, device name and "Start backend on PC" hint.
- **Temps** - CPU/GPU temperature plus CPU/GPU/RAM/VRAM load bars.
- **Details** - RAM/VRAM size, CPU clock, fan, disk, battery, uptime, top processes.
- **Alert** - metric, value vs. limit and 2-3 tips. `OK` snoozes 10 min, `Back` dismisses.
- **PC lost** - overlay after 5 s without valid telemetry.
- **Settings** (`OK` on the dashboard) - transport and, per alert rule: on/off, threshold, signal.

Controls: `Left/Right` switch pages, `OK` opens settings, `Back` exits.

## Bluetooth notes
The app registers its own BLE profile: name `PCHealth <flipper name>`, a MAC different from
the stock Flipper, one-time numeric-comparison pairing with bonding (the Flipper shows "Verify code" - press OK;
the PC backend accepts it programmatically) and its own bond file in
`/ext/apps_data/pc_health_remote/.bt.keys`. Your phone's Flipper pairing is untouched; the
default profile is restored when the app exits.

## Build
```
pip install ufbt
cd flipper
ufbt            # builds dist/pc_health_remote.fap
ufbt launch     # run on a connected Flipper
make -C tests   # host-side unit tests (protocol + alert engine)
```

## Layout
`protocol.c` wire format and stream parser | `alerts.c` alert engine (no firmware deps) |
`advice.c` tips | `settings.c` persistence | `link.c` shared link state |
`transport_ble.c`, `transport_usb.c` | `signals.c` notification patterns |
`scenes/`, `views/` UI.
