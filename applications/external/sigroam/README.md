<p align="center">
  <img src="screenshots/menu.png" width="420" alt="SigRoam main menu running on a Flipper Zero">
</p>

<h1 align="center">SigRoam</h1>

<p align="center">
  <b>A receive-only Wi-Fi survey (wardriving) front-end for the Flipper Zero.</b><br>
  Drives an external ESP32 Marauder scanner over the GPIO serial port.
</p>

<p align="center">
  <a href="https://github.com/pingequalab/sigroam-wardriving/actions/workflows/host-test.yml"><img src="https://github.com/pingequalab/sigroam-wardriving/actions/workflows/host-test.yml/badge.svg" alt="host_test status"></a>
  <a href="https://github.com/pingequalab/sigroam-wardriving/actions/workflows/fap-build.yml"><img src="https://github.com/pingequalab/sigroam-wardriving/actions/workflows/fap-build.yml/badge.svg" alt="fap_build status"></a>
  <img src="https://img.shields.io/badge/firmware-Official%20%7C%20Momentum-orange" alt="Firmware: Official and Momentum">
  <img src="https://img.shields.io/badge/attack%20features-none-brightgreen" alt="Attack features: none">
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-GPL--3.0-blue" alt="License: GPL-3.0"></a>
</p>

---

> **What SigRoam is.** SigRoam turns a Flipper Zero into the control head for an
> ESP32 Marauder Wi-Fi scanner. It starts and stops scans over the GPIO serial
> port, shows AP, BLE and GPS counts live, and exposes the raw serial stream. It
> is receive-only by design: no deauthentication, no handshake capture, no attack
> traffic of any kind.

The Flipper does not scan anything by itself. It starts and stops the scan, shows
what the scanner reports in real time, and lets you read the raw serial lines when
something looks wrong.

## Screenshots

| Main menu | Dashboard |
|---|---|
| <img src="screenshots/menu.png" width="360" alt="SigRoam main menu with Dashboard, Probe firmware and Raw log entries"> | <img src="screenshots/dashboard.png" width="360" alt="SigRoam dashboard showing scanner status and bytes received"> |
| **Settings** | **About** |
| <img src="screenshots/settings.png" width="360" alt="SigRoam settings screen showing baud rate, source, sound and vibro options"> | <img src="screenshots/about.png" width="360" alt="SigRoam about screen showing version, receive-only statement and QR code"> |

## Quick facts

| | |
|---|---|
| **Platform** | Flipper Zero (FAP), built with [ufbt](https://github.com/flipperdevices/flipperzero-ufbt) |
| **Scanner protocol** | [ESP32 Marauder](https://github.com/justcallmekoko/ESP32Marauder) serial |
| **Serial pins** | 13 (TX), 14 (RX); 5 V on pin 1 |
| **Default baud** | 115200, six choices in Settings |
| **Unique-BSSID estimator** | 4 KB Bloom filter — 32768 bits, 4 hashes, RAM only |
| **Firmware targets** | Official and Momentum — one `.fap` for both; both are built in CI |
| **Attack features** | None, permanently — see below |
| **Tests** | Host-side unit tests under gcc and clang, plus ASan/UBSan |
| **License** | GPL-3.0 |

## What it deliberately does not do

SigRoam is **receive-only**. It never transmits attack traffic, and it never will.
The following are permanently out of scope — not "not yet implemented", but a
product rule the project will not accept exceptions to:

- deauthentication / disassociation frames
- WPA handshake capture
- evil twin, karma, or rogue AP
- beacon spam and BLE spam
- password cracking of any kind

The app only parses what the scanner prints. If you need those features, this is
the wrong project.

## Hardware you need

- A Flipper Zero.
- An ESP32 Marauder board with **GPS + microSD**, connected to the GPIO header.
  Dual-band (2.4 + 5 GHz) needs **ESP32-C5**. Without GPS you get live counts,
  not locations; without SD there is no WiGLE CSV.

The app talks the Marauder serial protocol, not a specific product.

On-device development and verification used
**[Scout Lite](https://github.com/pingequalab/scout-lite)**
(ESP32-C5 + Quectel L86-M33 GPS + microSD, pre-flashed Marauder).
Other C5 + GPS + SD Marauder boards that present the same UART on Flipper
pins 13/14 should work; they have not been bench-tested.

> ### Reference board — Scout Lite
>
> Same protocol, same 5V-on-pin-1 budget, GPS already wired, no adapter pin
> map to guess. The pin notes below assume this board.
>
> **[Hardware and build files](https://github.com/pingequalab/scout-lite)**
> · **[pingequa.com](https://www.pingequa.com/products/scout-lite)**

## Wiring and required setup

| Flipper pin | Use |
|---|---|
| 1 (5V) | Power for the scanner |
| 13 (TX) | Serial to the scanner |
| 14 (RX) | Serial from the scanner |

Two things will stop the app from working if you skip them:

- **`Settings → System → Log Device` must be `Off`.** Pins 13/14 are shared with
  the firmware's serial log. If the log owns them, the app cannot open the port —
  it detects this and says so on screen rather than failing silently.
- **5 V on pin 1 has two independent sources, and the app handles both.** With USB
  plugged in, the pin is fed straight from USB VBUS and the firmware deliberately
  keeps the boost converter off; on battery, the same pin is fed by the OTG boost,
  which the app requests and then confirms before opening the port. Either way the
  scanner is powered, so you can run the app on USB power or on battery.
- **Unplugging USB mid-session restarts the scanner.** Power handover to the
  battery itself is seamless, but the board browns out and comes back in its
  non-scanning state. The app notices the silence, shows `Resyncing...` and
  re-issues the scan command until the board answers again — measured at 67 to
  77 seconds on the reference board, with the retries capped at 2 minutes.

**Do not power the scanner from pin 9 (3.3 V).** It is limited to 150 mA and a
5 GHz-capable ESP32 will exceed that on transmit peaks.

## Interface

The main menu has five entries:

- **Dashboard** — four tabs, switched with left/right:
  - `Dash` — unique BSSID estimate, AP/BLE counts, GPS fix count, bytes received,
    elapsed time. **OK starts and stops the scan.**
  - `Strm` — the most recent parsed records, scrolling.
  - `GPS` — fix status, satellites, coordinates. OK requests a GPS sample.
  - `Sess` — session and diagnostic counters.
- **Probe firmware** — sends `info` and reports what answered, so you can tell
  "nothing connected" from "connected but not Marauder".
- **Raw log** — the raw serial lines, including anything the parser did not
  recognise. This is the first place to look when the numbers look wrong.
- **Settings** — Baud (six choices, default 115200), Source, Sound, Vibro,
  Backlight, Stealth, Debug rows. Sound, Vibro and the LED fire when the GPS fix
  is acquired or lost during a survey; Stealth suppresses the LED, and Backlight
  holds the display lit while you are on the dashboard.
- **About** — version, compliance statement, and a QR code to this repository.

While a scan is running, Back returns to the main menu **without stopping the
scan**. Stopping is done only with OK on the Dash tab.

## Where the survey data goes

**SigRoam does not write survey data to the Flipper's SD card.** The only thing it
persists on the Flipper is your settings.

Logging is the scanner's job: Marauder writes its own CSV to the SD card on the
scanner board, and that file is what you upload to [WiGLE](https://wigle.net/).
SigRoam gives you control and live visibility over that session; it is not a
second recorder.

The unique-BSSID number on the Dash tab is an estimate from a 4 KB Bloom filter
(32768 bits, 4 hashes), kept in RAM for the session only. It can undercount
slightly at high AP densities by design — it is a progress indicator, not the
record of what you collected.

## Building

Built with [ufbt](https://github.com/flipperdevices/flipperzero-ufbt).

```bash
ufbt              # build
ufbt launch       # build, install, and run on a connected Flipper
ufbt cli          # device log
```

Host-side unit tests cover the pure-logic layer (line assembly, parsers, model,
Bloom filter) and run without a Flipper:

```bash
make -C tools/host_test        # guards + tests
make -C tools/host_test asan   # same, with ASan/UBSan
```

## Firmware compatibility

**One `.fap` covers both supported firmwares.** A release ships a single
`sigroam-<version>.fap`, built against the Official SDK; it runs on Official and
on Momentum, and there is nothing to pick at download time.

That works because loading is gated on the firmware API *major*, which is
compared exactly, while the minor is not compared at all. Official `1.4.3` and
Momentum `mntm-012` both report `Target: 7, API: 87.1`, and building this source
against both SDKs produces .fap files that differ in five bytes: a debug-link
checksum the loader never reads, and one byte of section size. Every code, data,
relocation and symbol section is byte-identical.

Unleashed is a different API major (88.x) and is **not** covered by releases. It
still builds from the same source, but it is not part of the regular pipeline.

The app uses only the common Flipper API — no firmware-specific headers — so it
builds against the Official SDK, which the Flipper Apps Catalog requires. CI
keeps building both targets on every push; that matrix is what catches a
firmware-specific API before it can reach a release.

## FAQ

**Does SigRoam scan Wi-Fi on its own?**
No. The Flipper Zero has no Wi-Fi radio. SigRoam is the control head; an external
ESP32 Marauder board does the scanning and the logging.

**Can it capture handshakes or send deauth frames?**
No, and it never will. Those features are permanently out of scope as a product
rule, not a missing feature. The app is receive-only and only parses what the
scanner prints.

**Where is the survey data stored?**
On the scanner's own microSD card, written by Marauder as CSV. SigRoam persists
only your settings on the Flipper. The CSV is what you upload to WiGLE.

**Can I use it with USB plugged in?**
Yes. Pin 1 is fed from USB VBUS whenever USB is connected, and from the OTG boost
converter when it is not. The app checks both before it opens the serial port, so
running on a car charger and running on battery are equally supported.

**The scan went quiet right after I unplugged USB. What happened?**
The Flipper switches to battery cleanly, but the scanner board browns out during
the handover and restarts into its non-scanning state. The app detects that the
data stream has stopped and re-issues the scan command until the board answers;
the counters resume on their own. On the reference board this takes 67 to 77
seconds, which is how long the scanner needs to boot and accept commands again.

**Why does the app say the serial port is busy?**
Pins 13/14 are shared with the firmware's serial log. Set
`Settings → System → Log Device` to `Off`.

**Which firmware does it need?**
Official or Momentum, and the same file works on both — a release ships one
`sigroam-<version>.fap`, not one per firmware. Both targets are built in CI from
the same source. The app uses only the common Flipper API, so it also builds
against the Official SDK required by the Flipper Apps Catalog. Unleashed runs a
different API major and is not covered by releases.

**Which hardware is recommended?**
An ESP32-C5 Marauder board with GPS and microSD. Development and on-device
verification used [Scout Lite](https://github.com/pingequalab/scout-lite).
Other Marauder-compatible boards that expose the same UART, GPS and SD
should work; they have not been bench-tested here.

## License

GPL-3.0. See [LICENSE](LICENSE).

---

<p align="center">
  <b>PINGEQUA_Lab</b> — hardware and firmware for RF, GPS and field survey work.<br>
  <a href="https://pingequa.com">pingequa.com</a> &nbsp;·&nbsp;
  <a href="https://github.com/pingequalab/scout-lite">Scout Lite scanner board</a> &nbsp;·&nbsp;
  <a href="https://github.com/pingequalab">More tools</a>
</p>
