# Embraco Starter

A simple, safe PWM generator app for Flipper Zero to test **Embraco** inverter compressors.

**Website:** https://experthub.app/

## Features
- **Safety-first startup**: the app opens on **Help** and keeps PA7 in **Hi-Z**.
- **4 menu items**:
  - **Power off** — PWM stopped (pin driven LOW)
  - **Low speed** — 55 Hz (≈2000 RPM VNE / 1800 RPM VEG & FMF)
  - **Mid speed** — 100 Hz (≈3000 RPM VNE/VEG/FMF)
  - **Max speed** — 160 Hz (≈4500 RPM VNE/VEG/FMF)
- **Hardware PWM** on **PA7** for stable frequency, 50% duty.
- On exit: PA7 returns to **Hi-Z**.

## Wiring
- **2 (A7)** → inverter **+** (usually RED wire)
- **8 (GND)** → inverter **-** (usually WHITE wire)

## Usage
1. Launch app → read **Help** (output is cut to Hi‑Z while reading).
2. Press **BACK** to enter the main menu; default mode is **Power off**.
3. Select a speed with **OK** (Low / Mid / Max).
4. Re-enter **Help** any time to cut output (Hi‑Z) while reading.

> Embraco compressors can run at many speeds with fine 30‑RPM steps; this app exposes three convenient test speeds.

## Build (uFBT)
```bash
python3 -m pip install --upgrade ufbt
ufbt
# resulting .fap is in ./dist
