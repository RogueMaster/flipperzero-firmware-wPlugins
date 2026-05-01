# Hardware & wiring

🇫🇷 French version: [HARDWARE.md](HARDWARE.md)

## Supported modules

- **NEO-6M V2 GPS** (u-blox NEO-6) — NMEA 0183 output, 9600 bauds, 1 Hz
- Bundled ceramic antenna, or external active antenna via U.FL

> Other NMEA modules should also work (PA1010D, BN-180, NEO-M8N, etc.). The **baud rate is configurable** via Settings → `GPS baud` (4800 / 9600 / 19200 / 38400 / 57600 / 115200). If you test another module successfully, open a PR to update this list.

## Wiring on Flipper Zero

The Flipper Zero exposes **USART1** on GPIO pins 13 (TX) and 14 (RX). The app uses `FuriHalSerialIdUsart` = USART1.

### Diagram (3 wires)

```
  Flipper Zero GPIO                          NEO-6M GPS
 ┌────┬──────────┐                          ┌──────────┐
 │  9 │ 3V3      ├──────── 🔴 red ────────▶ │ VCC      │
 │ 10 │ SWC      │                          │          │
 │ 11 │ GND      ├──────── ⚫ black ──────▶ │ GND      │
 │ 12 │ SIO      │                          │          │
 │ 13 │ TX       │                          │ RX ◁ ── unused (see note)
 │ 14 │ RX       │◀─────── 🟢 green ─────── │ TX       │
 │ 15 │ C1       │                          └──────────┘
 │ 16 │ C0       │
 │ 17 │ 1W       │
 │ 18 │ GND      │
 └────┴──────────┘
```

### Summary table

| NEO-6M | Wire | Flipper GPIO    | Role                                |
|--------|------|-----------------|-------------------------------------|
| VCC    | 🔴   | pin 9 (3V3)     | 3.3 V power                         |
| GND    | ⚫   | pin 11 (or 18)  | Ground                              |
| **TX** | 🟢   | **pin 14** (RX) | NMEA GPS → Flipper                  |
| RX     | —    | —               | **Unused**: we only read from GPS   |

### Why only 3 wires?

The app only **reads** NMEA sentences emitted by the GPS (1 Hz). It never sends configuration commands (e.g. `$PMTK...` for ublox) — the module runs with its factory defaults. So the GPS's RX pin can be left floating.

If you ever want to configure the module (switch to 10 Hz, disable some sentences, etc.), connect the 4th wire: **Flipper pin 13 (TX) → GPS RX**.

**⚠️ 3.3 V only.** The NEO-6M accepts 3.3–5 V on power, but its TX output may go to 5 V if powered at 5 V → possibly fries the Flipper UART. Power it at 3.3 V only.

> Full Flipper pinout: https://docs.flipper.net/gpio-and-modules

## First fix

A cold GPS can take **30 s to 5 min** to lock its first fix. To speed it up:
- Outdoors, wide open sky (not under trees, not behind double-glazing, not in a building's shadow)
- Antenna facing the sky
- Keep the module powered continuously — ephemeris data is lost when it's turned off

After the first outdoor fix, the module keeps a warm-start capability for a few minutes.

## Diagnostic: no NMEA data

1. **Swapped wires** — symptom #1. Verify: GPS TX → Flipper pin 14.
2. **No power** — the NEO-6M's red LED should blink at 1 Hz when it has a fix, and stay lit without fix.
3. **UART conflict** — on Momentum and other firmwares, the *Expansion* service intercepts the UART. The app auto-disables it, but if you still see `ExpansionSrvWorker` spam in the logs, please open an issue.
4. **qFlipper is running** — it holds the serial port during `ufbt launch`. Quit it with Cmd+Q.
5. **Baud rate mismatch** — if your module isn't at 9600, adjust Settings → `GPS baud`.

### Watching NMEA frames arrive

Fastest check: go to **Menu → GPS status** and look at the `NMEA: X B / Y lines` line. The counter ticks up in real time as long as the UART is receiving. Three cases:

- **`0 B / 0 lines`** not moving → no bytes received (wire, power, module disconnected)
- **`X B / 0 lines`** X goes up but Y stays 0 → bytes received but no full sentence → wrong baud rate
- **X and Y both go up** → the UART is fine, just a bad fix quality (go outside, wait)

### Detailed logs via CLI

```bash
ufbt cli
> log debug
```

The app logs under the `OSM` tag:
- `[I][OSM] GPS fix acquired` on each no-fix → fix transition (cooldown 10 s)
- `[D][OSM] save: ...`, `[D][OSM] write: ...` tracing each step of a save
- `[E][OSM] ...` for critical errors (SD, allocations, etc.)
