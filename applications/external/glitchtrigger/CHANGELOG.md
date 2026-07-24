# Changelog

All notable changes to Glitch Trigger are documented here.
This project adheres to [Semantic Versioning](https://semver.org/).

## [1.3] — 2026-07-23

The **serial** update — close the glitch loop over UART.

### UART auto-hit
- **Feedback source** — Auto-hit can now read a **GPIO Pin** (as before) *or*
  watch the target's **UART** (USART, pins 13 TX / 14 RX) for a **success
  string**. When the substring appears after a shot, that shot glitched.
- New **UART baud** (9600–230400) and **Success str** settings (on-device text
  entry). The matcher is an always-correct sliding-window substring search fed
  from an interrupt-driven RX ring buffer.
- New `helpers/glitch_serial` (acquire/init USART, async RX in ISR → stream
  buffer, drained and matched on the UI thread).

### Campaign stats
- The Sweep screen now shows a live **hit-rate %** alongside the shot/hit counts.

### Persistence
- `success_str`, `uart_baud` and `fb_source` are part of the saved config, so they
  ride along in profiles and the restored last-config.
- Profile format bumped to v4 and last-config to v2 (GlitchParams grew).

### UI
- **Settings** gains **FB source**, **UART baud** and a **Success str** editor
  (press OK on the row to type it); new `successstr` scene.
- About text and README updated; UART hook-up added to the pin table.

## [1.2] — 2026-07-21

The **fault map** update — see the fault window take shape and take it with you.

### Fault Map
- New **Fault Map** screen: every sweep hit is plotted on a **delay × width
  heatmap**. A movable cursor reads out any cell's exact width/delay and whether
  it hit.
- **CSV export** — press OK to write the grid to
  `apps_data/glitch_trigger/faultmap.csv` (ranges in the header, one row per delay).
- New `helpers/glitch_map` (packed bit-grid, up to 100×44 cells) and
  `views/faultmap_view`.

### Sweep
- **Search strategy** — Linear (in-order) or **Random** grid traversal, to dodge
  periodic aliasing with a target's own timing.
- Hits now populate the shared fault map as they happen.

### Persistence
- The **last-used config** and feedback/Sound/Vibro/LED settings are saved on exit
  and restored on the next launch (`glitch.settings` blob).
- Profile format version bumped (3) as `GlitchParams` grew a field.

### UI
- New **Fault Map** menu entry and scene; **Configure** gains **Search**.
- About text, screen mockups and README updated for the new features.

## [1.1] — 2026-07-20

The **campaign** update — turns the single-shot tool into a fault-injection
workbench.

### Sweep
- **2D sweep** — optionally sweep a **delay × width grid**, not just width, to
  cover the real parameter search space; progress tracks position across the grid.
- **Dwell** — fire *N* shots at each sweep point for statistics before advancing.
- **Auto-hit** — sample a target **feedback pin** after each shot and mark a hit
  automatically when it reaches the configured **success level** (HIGH/LOW), so a
  sweep can run unattended. Manual **OK** marking still works.
- Sweep screen now shows the live **width + delay**, `2D` / `A` (auto) chips, and
  a `width @ delay` last-hit readout.

### Profiles & logging
- **Profiles** — save / load / delete named parameter sets on the SD card under
  `apps_data/glitch_trigger/profiles`, with on-device name entry.
- **CSV hit log** — hits (manual or auto) append to
  `apps_data/glitch_trigger/hits.csv` with delay, width, pulses, gap, shot index
  and timestamp. Toggleable.
- New `helpers/glitch_storage` module (binary profile blobs + CSV append).

### Engine
- **Feedback read** — a third selectable GPIO, sampled for the success level and
  pulled to the inactive level so a floating line stays quiet; skipped if it
  collides with the output pin.
- Tightened masked-window bound to **~1 ms**: only the fine delay + glitch edge is
  masked, and burst inter-pulse gaps now run with interrupts on.

### UI
- New **Profiles** menu entry and scenes (list · name entry · load/delete).
- **Settings** gains Feedback pin, Success level, Auto-hit and Log-hits.
- **Configure** gains Sweep 2D, Dwell and the 2D delay from/to/step range.
- About text, screen mockups and README updated for the new features.

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
