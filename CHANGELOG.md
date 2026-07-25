# Changelog

All notable changes to Hermes are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [1.3] - 2026-07-25

A "power console" pass: four additions that make the live session do more, and
a recap so you can see what it did.

### Added

- **Session recap card.** Pressing Back now shows what the session was —
  bytes received, average throughput, framing errors and duration in four
  tiles, a one-word health verdict (`clean link` / `noisy` / `wrong framing?`)
  and the log filename if one was written. A second Back returns to the menu.
  The throughput/duration/formatting arithmetic is pure and host-tested.
- **Send raw bytes.** Ctrl palette → *Send hex…* asks for a length, then opens
  a hex grid (via `byte_input`) to compose arbitrary bytes — a binary command,
  a magic packet, an exact escape sequence, a literal NUL. Sent verbatim.
- **Live baud nudge.** Ctrl palette → *Baud +* / *Baud −* steps to the next or
  previous standard rate and re-opens the link in place, keeping framing,
  logging and the armed watch — for when detection landed one row off.
- **Log markers.** Ctrl palette → *Drop marker* (shown only while logging)
  writes a numbered, timestamped divider into the capture, so a long session
  has points to jump to.

### Changed

- Leaving the console is now Back → recap → Back → menu, rather than a bare
  pop. The recap reads the session while the link is still open, then the link
  and any log are closed as before.

### Notes

- Hex send is a two-phase scene (length via `number_input`, then bytes via
  `byte_input`) because `byte_input` edits a fixed-length field.
- The baud nudge re-opens the tap rather than reconfiguring it live, so the
  status bar, health counters and watch all reset cleanly to the new rate.
- Host tests are now three suites (autobaud fit · watch matcher · session
  arithmetic), all under ASan/UBSan in CI, on both SDK channels.

## [1.2] - 2026-07-19

Five additions, all aimed at the live session — the part of the workflow v1.1
still left you doing by hand.

### Added

- **Watch for a string.** `Ctrl palette → Watch for…` arms a pattern; when it
  appears in the stream, the Flipper buzzes and flashes, so you can set `login:`
  and walk away during a long boot instead of watching the screen. Shown as an
  inverted strip along the bottom with a live hit count. Case-insensitive, finds
  matches that straddle DMA chunks or sit inside colour codes (it watches the
  raw stream, not the cooked screen). Implemented as KMP with a precomputed
  failure table and unit-tested against self-overlapping patterns and
  byte-at-a-time delivery.
- **Link health.** The console status bar now shows `ERR n` — hardware framing,
  parity and noise errors counted since the link opened. A count that climbs
  with the traffic is the surest sign the framing is wrong even though the rate
  is right. (Overruns stay excluded: those are the Flipper being slow, not the
  line being wrong.)
- **Send break.** `Ctrl palette → Send break` holds TX low past a full frame —
  a real UART break, which some bootloaders and the Linux magic SysRq listen
  for and which cannot be expressed as a byte. Done by taking the pin off the
  UART, driving it low by hand for 25 ms and handing it back, without disturbing
  RX.
- **Script replay.** `Ctrl palette → Run script…` picks a `.txt` off the SD
  card and sends it line by line — a login, a set of U-Boot commands, a recovery
  sequence. Lines are paced ~250 ms apart, blank lines and `#` comments skipped,
  and playback runs off the UI tick so the target's replies keep scrolling in
  between. Progress shows as `SCRIPT n/m`.

### Fixed

- The armed-watch strip drew its text at a baseline that clipped descenders
  (the `g`/`p` in patterns like `login:`); nudged up one pixel. Caught, again,
  by rendering the mockups from the real layout constants.

### Notes

- The break, watch, script and autoboot actions all run off the UI tick or a
  brief bounded hold — none of them blocks the GUI thread, so the incoming boot
  log keeps rendering throughout.
- An armed watch shrinks the terminal from six rows to five to make room for its
  strip; the terminal genuinely gives up the row rather than being drawn over.

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

[1.3]: https://github.com/at0m-b0mb/Hermes-FlipperZero/releases/tag/v1.3
[1.2]: https://github.com/at0m-b0mb/Hermes-FlipperZero/releases/tag/v1.2
[1.1]: https://github.com/at0m-b0mb/Hermes-FlipperZero/releases/tag/v1.1
[1.0]: https://github.com/at0m-b0mb/Hermes-FlipperZero/releases/tag/v1.0
