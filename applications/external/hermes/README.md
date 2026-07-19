<div align="center">

<img src="images/banner.png" alt="Hermes — UART baud detector and console tap for the Flipper Zero" width="100%">

# Hermes

**Every wire is talking. Hermes tells you what it's saying.**

Clip onto an unknown serial line. Hermes measures the bit time with the CPU's cycle counter, checks its answer against the real UART, and hands you the console.

[![Flipper Zero](https://img.shields.io/badge/Flipper%20Zero-FAP-FF6900?style=flat-square&logo=flipper&logoColor=white)](https://flipperzero.one/)
[![Build FAP](https://img.shields.io/github/actions/workflow/status/at0m-b0mb/Hermes-FlipperZero/build.yml?style=flat-square&label=build)](https://github.com/at0m-b0mb/Hermes-FlipperZero/actions)
[![Category](https://img.shields.io/badge/category-GPIO-FFB02E?style=flat-square)](#)
[![Firmware API](https://img.shields.io/badge/API-87.1%20(fw%207)-2EDC82?style=flat-square)](#)
[![License](https://img.shields.io/badge/license-MIT-blue?style=flat-square)](LICENSE)
[![Made by at0m-b0mb](https://img.shields.io/badge/made%20by-at0m--b0mb-black?style=flat-square)](https://github.com/at0m-b0mb)

</div>

---

## The problem

You've popped the lid off a router. There are four pads next to the SoC, and one of them is almost certainly a serial console with a root shell behind it. But at what baud? And 8N1, or something stranger?

The usual answer is to guess: open a terminal at 115200, squint at the garbage, try 57600, squint again. Hermes does the measuring instead.

> **It listens before it speaks.** During detection Hermes releases the TX pin entirely — it cannot transmit even by accident. It only drives the line once *you* type.

<div align="center">
<img src="images/screens.png" alt="Hermes screens: the live edge scope while listening, the detected result with confidence, and the console attached to a U-Boot prompt" width="100%">
<br>
<sub><b>Listen → measure → tap.</b> The scope shows the target's real bits. The verdict shows what it is, how sure Hermes is, and why. Then you're on the console.</sub>
</div>

---

## How it actually works

Most auto-baud tools do one of two things. Hermes does both, because each covers the other's blind spot.

### Stage 1 — measure the bit time

The RX pin is taken over as a plain interrupt input. Every edge is timestamped with `DWT->CYCCNT`, the ARM cycle counter, which ticks at 64 MHz — about **15 nanoseconds** per tick.

That gives a list of *segments*: how long the line held high or low between edges. Every one of them is a whole number of bit times, because that's what UART is.

```
 idle   start  d0 d1  d2   d3 d4 d5   d6 d7  stop  idle
 ─────┐      ┌─────┐     ┌────┐     ┌─────┐      ┌──────
      └──────┘     └─────┘    └─────┘     └──────┘
      │  1  │  2   │  1  │ 1  │  1  │  2   │  1  │
        └──────────── every run is k × the bit time ────┘
```

So the question "what is the baud rate?" becomes "**what bit time turns all of these measurements into near-integers?**"

Hermes answers it by testing each standard rate directly and counting how many segments it explains. Two details make that work rather than merely sound clever:

**Harmonics.** Half the true bit time *also* turns every measurement into an integer — just twice as large a one. So does a quarter. The escape is that a UART frame can only hold so many identical bits in a row (a low run is capped by the start and stop bits at nine). At a harmonic, the longer runs need more bits than a frame can physically hold, and they stop fitting. **Among rates that score alike, the slowest is the real one** — the rest are its echoes.

**Tolerance.** A segment's timing error comes from interrupt latency on its two edges, and that error *doesn't grow* with the number of bits in between. So the tolerance is a share of **one** bit time, never of the whole segment. Scale it with the segment instead and an absurdly fast rate will swallow any signal by declaring every pulse to be nine bits of its own — which is exactly the bug this cost me, [caught by the test suite](test/).

Once the rate is known, Hermes refines from it to read the line's *actual* bit time — which is how you tell an on-spec 115200 from a board whose crystal is 3% out.

### Stage 2 — check it against the hardware

A timing fit is still an inference. So Hermes re-opens the same pin as a **real UART** and listens at each candidate for 150 ms, scoring what comes back on two axes:

| Signal | What it means |
|---|---|
| **Framing errors** | The line disagreeing with the guess. The strongest evidence, and the only one that survives a binary protocol. |
| **Printable ratio** | How much of it reads as text. Breaks ties and rewards an actual console. |
| *Overruns* | **Deliberately not scored.** An overrun means Hermes was too slow, not that the rate is wrong — counting it would quietly punish the fastest, correct candidates. |

This is also what detects the **framing**: a right rate with the wrong parity shows up as a clean-ish byte stream riddled with frame errors, so a second pass pins the rate and lets 8N1 / 8E1 / 8O1 / 7E1 compete. That's why Hermes reports `115200 8N1` rather than just a number.

### Why both stages

They fail in opposite directions, which is the point:

- **Edge timing** works from a single short burst — one boot message is enough — but the interrupt can't follow edges past roughly **230400**.
- **The UART sweep** has no speed limit and is hardware-accurate to 921600, but it needs traffic *during* its window.

So when the timing fit comes up empty — which is exactly what happens on a very fast line — Hermes sweeps the whole rate table on the UART instead. And when the line falls silent between the two stages, it says **"timing only"** on the result screen rather than dressing up a guess as a measurement.

---

## Reading the result

<div align="center">
<img src="images/screen_result.png" alt="The result screen: 115200 8N1 at 97% confidence, verified, with a candidate ladder" width="46%">
</div>

- **The number and framing** — what to open the console at.
- **`verified` / `timing only`** — whether real bytes backed this up, or only the edge timing did.
- **The bar** — confidence: how much of the capture the rate explains, scaled by how close the refined measurement landed.
- **The provenance line** — `128 B  94% text`, or `412 edges - 96% fit`. Never just a number; always where it came from.
- **The chips** — the runner-ups, each chip's height its confidence. A close second is *visible*, not buried behind a keypress. **Left / Right** to pick one, **OK** to open it.

---

## The console

<div align="center">
<img src="images/screen_console.png" alt="The console attached to a U-Boot prompt at 115200 8N1" width="46%">
</div>

A real terminal, not a byte dump. Boot logs are full of ANSI colour codes, so Hermes runs a small VT parser over the stream — otherwise the screen fills with `[0;32m` instead of text.

| Key | Does |
|---|---|
| **Up / Down** | Scroll back. New output never yanks you away while you're reading. |
| **Right** | ASCII ⇄ hex (hex has an ASCII gutter, so a familiar string still jumps out) |
| **Left** | Ctrl-key palette — **Ctrl+C**, Ctrl+D, Ctrl+Z, Esc, Tab, **Stop autoboot** |
| **OK** | Type a line and send it |
| **OK (long)** | Send Enter on its own |
| **Back** | Drop the link |

RX runs on DMA, so a full boot log at 921600 arrives without holes in the middle.

### Stop autoboot

`Hit any key to stop autoboot: 3` gives you about a second, and by the time you've read it and reached for a key it's usually gone. **Left → Stop autoboot** hammers Enter for four seconds so you can arm it *before* powering the board and let it catch the window for you. The status bar counts down while it runs, and it's driven from the UI tick rather than a blocking loop, so the screen keeps updating and the incoming boot log still renders.

### Logging

Turn on **Settings → Log to SD** and every console session is written to `/apps_data/hermes/` as `hermes_YYYYMMDD_HHMMSS.log`, with a header recording the rate, framing and port — so a file you find months later still says what it came from.

The file gets the **raw** bytes, escape sequences and all. The screen strips ANSI to stay readable, but a log you'll later grep, diff or replay should be exactly what the wire said. A filled dot appears at the left of the status bar while a capture is running.

---

## Is it me or them?

<div align="center">
<img src="images/screen_selftest.png" alt="The self test: 9600 and 115200 echoed, 460800 and 921600 failed, verdict 'Marginal at speed'" width="46%">
</div>

The worst half-hour in hardware work is the one spent debugging a target that was fine, because a jumper wasn't. **Self Test** ends that argument: bridge TX to RX with a single wire and Hermes sends a pattern out and checks it comes back.

It runs at four rates from 9600 to 921600, because the failure modes are different and worth telling apart:

- **All four echo** → the Flipper, its pins and your wire are all good. The silence is the target's.
- **Slow rates pass, fast ones don't** → the link works but not at speed. Long dupont leads and breadboards do this. Shorten them.
- **Nothing comes back** → the jumper isn't making contact, or it's not on the pins you think it is.

The pattern starts with `0x55 0xAA 0x00 0xFF` — alternating bits, then all-low and all-high — so a wire that only passes certain levels gets caught rather than flattered.

---

## Wiring

<div align="center">
<img src="images/screen_wiring.png" alt="The wiring guide: Flipper pin 14 to target TX, pin 13 to target RX, GND to GND" width="46%">
<img src="images/screen_rules.png" alt="The rules screen: 3.3V logic only, RS-232 kills the Flipper, ground first" width="46%">
</div>

**RX and TX always cross.** Your TX is their RX.

| Flipper | | Target |
|---|---|---|
| **Pin 14** (RX) | ← | **TX** |
| **Pin 13** (TX) | → | **RX** |
| **Pin 8/11/18** (GND) | ↔ | **GND** |

> ### Read this before you clip on
>
> - **GND first, and always.** No common ground means no reference, and the two most common failures — nothing at all, or pure garbage — are both this.
> - **3.3V logic only.**
> - **RS-232 is not UART.** A DB9 serial port swings ±12V and **will destroy your Flipper**. Put a MAX3232 in between.
> - **Don't feed the 5V pin to a board that has its own supply.**
> - If the scope says *"no edges yet"*, it's the ground or the cross-over. It's essentially always one of those two.

Prefer the other pins? **Settings → Port** switches to LPUART on **15/16**, and the wiring guide relabels itself to match.

---

## Install

### Prebuilt `.fap` (fastest)

1. Grab `hermes.fap` from [**Releases**](https://github.com/at0m-b0mb/Hermes-FlipperZero/releases), or from the latest green run under [**Actions**](https://github.com/at0m-b0mb/Hermes-FlipperZero/actions) → *Artifacts*.
2. Open [qFlipper](https://flipperzero.one/update), connect your Flipper.
3. Copy it to **`SD Card / apps / GPIO /`**.
4. On the Flipper: **Apps → GPIO → Hermes**.

### Build it

```bash
python3 -m pip install --upgrade ufbt
git clone https://github.com/at0m-b0mb/Hermes-FlipperZero.git
cd Hermes-FlipperZero

ufbt              # -> dist/hermes.fap
ufbt launch       # build, install and run on a connected Flipper
```

Built against official firmware **fw 7 / API 87.1**, and CI also builds it against the **dev** SDK (API 88.0). It only uses public `furi_hal_serial` and `furi_hal_gpio`, so it tracks both channels.

Drop-in compatible with the app-catalog layout used by **Momentum** / **Unleashed** / **RogueMaster** — put the folder in `applications_user/hermes`.

---

## Tests

The detector is the whole product, and it's the one part a screenshot can't vouch for. So it's tested:

```bash
make -C test
```

This compiles the **real** `autobaud.c` for the host against a stubbed HAL, synthesises 8N1 waveforms at known rates, and drives the **actual interrupt handler** edge by edge — then asks the real fit what it saw. 21 checks, under ASan and UBSan, run in CI on every push:

```
clean signal        1200 · 9600 · 19200 · 38400 · 57600 · 74880 · 115200
interrupt jitter    up to ±200 cycles at 115200
idle gaps           200 bit times of silence between frames
harmonic traps      57600 must not answer 115200; 19200 not 38400
binary payload      random bytes, not text
degenerate          non-standard rate must not be named confidently · silent line
```

The jitter and harmonic cases aren't decoration — both caught real bugs during development.

---

## Layout

```
Hermes-FlipperZero/
├─ application.fam            # manifest (appid, GPIO category, icon)
├─ hermes.c / hermes_i.h      # lifecycle, views, notifications
├─ helpers/
│  ├─ autobaud.[ch]           # the star: edge capture + the bit-time fit
│  ├─ verifier.[ch]           # the second opinion: real UART, real framing errors
│  ├─ uart_tap.[ch]           # the live link (DMA rx, gated tx)
│  ├─ term.[ch]               # terminal model + the ANSI parser
│  ├─ session_log.[ch]        # capture to the SD card, raw bytes kept
│  ├─ selftest.[ch]           # loopback prover: is it me or them?
│  └─ baud_table.[ch]         # standard rates, framings, pin profiles
├─ views/
│  ├─ detect_view.[ch]        # the live square-wave scope
│  ├─ result_view.[ch]        # verdict card + candidate ladder
│  ├─ console_view.[ch]       # the terminal
│  ├─ selftest_view.[ch]      # the loopback report
│  └─ wiring_view.[ch]        # animated wiring + the safety rules
├─ scenes/                    # start · detect · result · console · ctrl · keyboard ·
│                             # manual · custombaud · selftest · wiring · settings · about
├─ test/                      # host test for the fit (stubbed HAL)
└─ tools_gen_*.py             # Pillow asset generators
```

---

## FAQ

**It says "no edges yet".** Ground, or the cross-over. Flipper **RX (14)** goes to target **TX** — not TX to TX. If you're sure of both, the pad may simply be idle; many boards only talk during boot, so power-cycle the target while Hermes listens. If you want to rule your own side out first, run **Self Test**.

**It found the rate but the text is garbage.** The framing is probably not 8N1. Hermes tries 8E1/8O1/7E1 automatically *if there was traffic during verification* — if the board went quiet, pick the framing yourself in **Manual Console**.

**Can it detect 921600?** The rate, yes — via the UART sweep, which is hardware-accurate. The *edge timing* gives up around 230400, because the interrupt can't be entered fast enough to catch edges 4 µs apart. Hermes falls back automatically; you'll see it sweep.

**My device runs at a non-standard rate.** Detection only *names* rates from its table, so something like 100000 baud comes back with low confidence rather than a confident wrong answer — that's deliberate. To open one anyway, use **Manual Console → Custom rate…** and type it in (50 – 2,000,000). Hermes asks the hardware whether a divider actually exists for that rate and refuses it if not, rather than opening a link that can't work.

**Does it write to my board?** Not during detection — the TX pin is actively released. In the console it transmits only what you type (or the autoboot burst, which you have to ask for). **Settings → Transmit: OFF** makes the whole session read-only.

**Where do the logs go?** `/apps_data/hermes/` on the SD card — reachable over qFlipper, or on the Flipper itself via the Archive app. One file per session, named by timestamp.

**Why is my 80-column log wrapping?** 128 pixels is 21 characters. Wrapping is honest; truncating would hide things.

---

## Ethics

Hermes is for **your own boards, and ones you're authorised to test**. A serial console is very often an unauthenticated root shell — that's precisely why it's worth finding on hardware you own, and why finding it on hardware you don't is someone else's line to cross, not yours.

It listens by default and only speaks when you tell it to. The rest is on you. Don't be a jerk.

---

## Credits

Built by **[at0m-b0mb](https://github.com/at0m-b0mb)**. Part of a family of Flipper Zero tools:

- 🗿 **[Rosetta](https://github.com/at0m-b0mb/Rosetta-FlipperZero)** — protocol explainer: watch Mifare, OOK/PSK and 1-Wire happen
- 🛡️ **[Warden](https://github.com/at0m-b0mb/Warden-FlipperZero)** — NFC access-card security grader
- 🔱 **[Trident](https://github.com/at0m-b0mb/Trident-FlipperZero)** — 3-in-1 ESP32 + NRF24 + CC1101 multi-radio
- 👻 **[Specter](https://github.com/at0m-b0mb/Specter-FlipperZero)** — passive NFC reader/skimmer bug-sweep
- 📡 **[Cerberus](https://github.com/at0m-b0mb/flipper-cerberus)** — Sub-GHz RF watchdog
- 🏷️ **[GhostTag](https://github.com/at0m-b0mb/GhostTag-FlipperZero)** — anti-stalking BLE tracker hunter

<div align="center">
<sub>MIT © 2026 at0m-b0mb — Flipper Zero and the dolphin are trademarks of Flipper Devices. Hermes is an independent project.</sub>
</div>
