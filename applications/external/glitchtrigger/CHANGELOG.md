# Changelog

All notable changes to Glitch Trigger are documented here.
This project adheres to [Semantic Versioning](https://semver.org/).

## [1.0] — 2026-07-17

First public release.

### Pulse engine
- **Cycle-accurate shots** shaped by busy-waiting on the Cortex-M4 **DWT cycle
  counter** inside a critical section (interrupts masked). Resolution ≈ 15.6 ns
  at 64 MHz; delays/widths converted straight from `SystemCoreClock`.
- **Shot model** `trigger → delay → pulse(s)` with configurable delay, width,
  pulse count, inter-pulse gap and **Active-High / Active-Low** polarity.
- **Long-delay handling** — the coarse part of a delay > 2 ms runs with
  interrupts enabled so the system isn't frozen; only the final precision slice
  is masked.
- **Safe pin handling** — output driven to idle on arm, returned to high-Z
  (analog) on leaving any firing screen.

### Triggering
- **Manual** — OK arms, OK fires (repeatable).
- **External** — an edge on the trigger-in pin fires a shot from a GPIO
  **interrupt**, for lowest latency; Rising / Falling selectable.
- **Repeat** — free-running at a configurable 10 ms – 5 s interval.

### Width sweep
- Walk the pulse width across `from … to` by `step`, looping, one shot per tick.
- **OK marks a hit** (captures the width in play); Left pauses, Right restarts.
- Live progress bar, step index, shot/hit counters and last-hit width.

### UI / UX
- **Trigger** screen with an animated schematic pulse timeline (trigger tick,
  dashed delay, up/down pulse train), an **IDLE → ARMED → FIRE** state badge, a
  live shot counter and a spark-on-fire highlight.
- **Wiring** screen — a labelled Flipper → MOSFET-crowbar → target diagram that
  redraws with your selected pins, plus rotating safety reminders.
- **Configure** screen — every parameter on a 1-2-5 "nice number" ladder with
  auto-scaled ns / µs / ms readouts.
- **Settings** — selectable glitch-out and trigger-in header pins, and gated
  Sound / Vibro / LED feedback.
- About screen, custom 10×10 icon, GitHub banner + social card + screen mockups.

### App
- Scene-based UI (start · params · trigger · sweep · wiring · settings · about)
  on the standard view dispatcher.
- FAP category **GPIO**. Builds against official firmware **fw 7 / API 87.1**
  with `ufbt`; CI on the release and dev channels.
