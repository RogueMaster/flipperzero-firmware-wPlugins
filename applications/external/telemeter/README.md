# Telemeter

Measure how far a storm or a cannon is from the delay between **light** and **sound**.

Light arrives almost instantly. Sound does not: at 20 °C it travels at about **343 m/s** (roughly 3 seconds per kilometre). The app times the gap between the two events and computes:

```text
v = 331.3 + 0.606 × T
d = v × t
```

`T` is air temperature in °C, adjustable in the menu.

## How to use

1. A splash screen appears, then the menu.
2. **Up/Down**: Storm or Cannon.
3. **Left/Right**: temperature from −10 to 40 °C.
4. **OK**: start a measurement.
5. On the **flash** (or **flame**) press OK: the timer starts.
6. On the **thunder** (or **boom**) press OK: you get metres, kilometres, and seconds.
7. **OK** starts another measurement, **Back** returns to the menu. **Back** on the menu exits.

## Install

Copy `dist/telemetro.fap` to the microSD card under `apps/Tools/`. On the Flipper: **Apps → Tools → Telemeter**.

Build:

```bash
ufbt
```

Firmware: match the API printed by `APPCHK`.

## Limits

This is an estimate, not a professional instrument. Humidity, wind, and obstacles are ignored. Your button reaction time is part of the measurement: fine for thunder (seconds), less so for very close blasts.

## Catalog

- App icon: `telemetro.png` (10×10, 1-bit)
- Splash and UI icons: `images/`
- Official screenshots: capture with **qFlipper** into `screenshots/` (do not crop)
- GitHub previews: `docs/previews/`
