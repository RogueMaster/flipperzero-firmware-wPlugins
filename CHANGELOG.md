# Changelog

All notable changes to Hermes are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

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

[1.0]: https://github.com/at0m-b0mb/Hermes-FlipperZero/releases/tag/v1.0
