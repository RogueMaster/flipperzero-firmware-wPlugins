# Changelog

All notable changes to Hermes are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [1.1] - 2026-07-18

Four additions, each closing a gap v1.0 left open.

### Added

- **Self Test (loopback).** Bridge TX to RX with one jumper and Hermes sends a
  pattern out and checks it returns, at 9600 / 115200 / 460800 / 921600. The
  three outcomes are genuinely different diagnoses: everything echoes (your side
  is fine, the silence is the target's), only the slow rates pass (a marginal
  link — shorten the leads), or nothing comes back (the jumper isn't on the pins
  you think). The pattern leads with `0x55 0xAA 0x00 0xFF` so a wire that only
  passes certain levels is caught rather than flattered.
- **Session logging.** `Settings → Log to SD` writes each console session to
  `/apps_data/hermes/hermes_YYYYMMDD_HHMMSS.log`, headed with the rate, framing
  and port. The file keeps the **raw** bytes including escape sequences — the
  screen strips ANSI to stay readable, but a log you will grep or replay should
  be what the wire said. A filled dot in the status bar shows it is running.
- **Custom baud entry.** `Manual Console → Custom rate…` accepts anything from
  50 to 2,000,000, which lifts v1.0's restriction to the built-in table. The
  hardware is asked whether a divider exists for the rate and the link is
  refused if not, rather than opening something that cannot work.
- **Stop autoboot.** A Ctrl-palette action that hammers Enter for four seconds,
  so you can arm it before powering the board and let it catch the
  `Hit any key to stop autoboot` window instead of racing it by hand.

### Fixed

- The self-test result screen laid its fourth rate row through the footer text.
  The row block now has an explicit vertical budget that keeps it clear.

### Notes

- The autoboot burst is driven from the UI tick rather than a blocking delay
  loop, so the screen keeps redrawing and incoming boot output still renders
  while it runs.
- Logging is opt-in. Writing to the user's SD card is their call, not a default.

## [1.0] - 2026-07-16

First release. Built against official firmware fw 7 / API 87.1, and verified
against the dev SDK (API 88.0).

### Added

- **Baud detection in two stages.** Edge timing on the RX pin via `DWT->CYCCNT`
  (15 ns resolution) fits a bit time by testing each standard rate against the
  captured segments and taking the slowest that explains them — the faster ones
  are harmonics. The result is then re-checked on the real UART, scored by
  hardware framing errors and printable ratio. The two stages fail in opposite
  directions: timing works from a single burst but tops out near 230400, while
  the UART sweep needs live traffic but is accurate to 921600. When the timing
  fit comes up empty, Hermes sweeps the whole table instead.
- **Framing detection** — 8N1 / 8E1 / 8O1 / 7E1, resolved by pinning the rate
  and letting the framings compete on frame- and parity-error counts.
- **Live edge scope** that reconstructs the target's actual square wave from the
  captured timings, one bit per 3 px, so a start bit reads as a narrow notch.
- **Result screen** with confidence, a provenance line (`412 edges - 96% fit`),
  a `verified` / `timing only` label, and a ladder of runner-up candidates you
  can open directly.
- **Interactive console** — DMA receive, 96 lines of scrollback, ASCII/hex with
  an ASCII gutter, an ANSI/VT escape parser so boot logs render as text, a
  Ctrl-key palette (Ctrl+C/D/Z, Esc, Tab), and selectable CR / LF / CRLF.
- **Wiring guide** with an animated diagram that relabels itself for the active
  port profile, plus a rules page: 3.3V only, RS-232 needs a MAX3232, GND first.
- **Port profiles** — USART on pins 13/14, LPUART on 15/16.
- **Settings** — port, transmit enable, Enter mode, local echo, sound, LED.
- **Host test suite** (`make -C test`) that compiles the real `autobaud.c`
  against a stubbed HAL and drives the actual interrupt handler with synthetic
  waveforms: 21 checks under ASan/UBSan, run in CI alongside both SDK channels.

### Notes

- Detection releases the TX pin outright, so Hermes cannot drive the target's RX
  line until you open the console and type. `Transmit: OFF` keeps a whole
  session read-only.
- Overrun errors are tracked but deliberately excluded from candidate scoring —
  an overrun means Hermes was too slow, not that the rate is wrong, and counting
  it would penalise the fastest correct candidates.
- Only rates in the built-in table are named. A non-standard line reports low
  confidence rather than a confident wrong answer; use Manual Console if you
  already know the rate.

[1.1]: https://github.com/at0m-b0mb/Hermes-FlipperZero/releases/tag/v1.1
[1.0]: https://github.com/at0m-b0mb/Hermes-FlipperZero/releases/tag/v1.0
