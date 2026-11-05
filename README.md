<div align="center">

<table>
  <tr>
    <td align="center" width="50%">
      <a href="https://www.pingequa.com/products/flipper-zero-2-in-1-rf-module?utm_source=github&utm_medium=referral&utm_campaign=fz-rflab-2in1&utm_content=banner">
        <img src="images/hardware_devboard.jpg" alt="PINGEQUA 2-in-1 RF Devboard hardware" width="100%" />
      </a>
      <br><sub><b>The Hardware</b> · 2-in-1 NRF24 + CC1101</sub>
    </td>
    <td align="center" width="50%">
      <img src="images/demo.gif" alt="PINGEQUA RF Lab live demo — main menu navigation on Flipper Zero" width="100%" />
      <br><sub><b>Live Demo</b> · Scanner · Signal Gen · About</sub>
    </td>
  </tr>
</table>

# PINGEQUA RF Lab

### Plug-and-play across every firmware.

**2.4 GHz spectrum analyzer + NRF24 jammer for Flipper Zero, designed for the [PINGEQUA 2-in-1 RF Devboard](https://www.pingequa.com/products/flipper-zero-2-in-1-rf-module?utm_source=github&utm_medium=referral&utm_campaign=fz-rflab-2in1&utm_content=hero).**

[![Platform](https://img.shields.io/badge/platform-Flipper%20Zero-FF8200?style=flat-square)](https://flipperzero.one/)
[![Release](https://img.shields.io/github/v/release/pingequalab/rf-lab?style=flat-square&color=success&label=release)](https://github.com/pingequalab/rf-lab/releases/latest)
[![Firmware](https://img.shields.io/badge/firmware-OFW%20%7C%20Momentum%20%7C%20Unleashed%20%7C%20RogueMaster-blue?style=flat-square)](#firmware-compatibility)
[![FAP size](https://img.shields.io/badge/FAP-44%20KB-success?style=flat-square)](dist/)
[![License](https://img.shields.io/badge/license-MIT-yellow?style=flat-square)](LICENSE)
[![Hardware](https://img.shields.io/badge/hardware-PINGEQUA%202--in--1-000000?style=flat-square)](https://www.pingequa.com/products/flipper-zero-2-in-1-rf-module?utm_source=github&utm_medium=referral&utm_campaign=fz-rflab-2in1&utm_content=badge)

[**🛒 Get the Hardware**](https://www.pingequa.com/products/flipper-zero-2-in-1-rf-module?utm_source=github&utm_medium=referral&utm_campaign=fz-rflab-2in1&utm_content=cta-top) · [**📦 Download FAP**](../../releases) · [**📖 Quickstart**](docs/QUICKSTART.md) · [**❓ FAQ**](docs/FAQ.md)

</div>

---

## What It Does

An open-source Flipper Application Package (FAP) that turns Flipper Zero plus the PINGEQUA 2-in-1 RF Devboard into:

- **2.4 GHz spectrum analyzer** — 126 channels real-time with WiFi 1/6/11 + BLE 37/38/39 band markers, max-hold, microsecond dwell tuning, cursor inspection, **CSV scan export** (long-press OK, v0.5.0+)
- **NRF24 jammer with 7 modes** (v0.4.0+):
  - CW Custom · BLE Adv · **BLE React** (RPD reactive — first on Flipper NRF24) · WiFi 1/6/11 (CW band hop) · ALL 2.4G
  - Disrupts nearby BLE / 2.4 GHz WiFi at close range
  - **Auto session log** + **mode/channel persistence** (v0.5.0+)
- **Human-readable export filenames** (v0.5.2+) — `scan_<date>_<time>_ch<peak>.csv` / `jam_<date>_<time>_<mode>_<dur>s.csv`. Sorting by name = sorting by time.
- **On-device About screen** (v0.5.2+) — three pages, Up/Down to navigate, with scannable QR codes for the shop and GitHub repo so visitors can land on the right page with their phone.
- **Two FAP builds, one source tree** (v0.5.5) — API 88.2 for Official 1.5.1, Unleashed, and RogueMaster; API 87.1 for Momentum mntm-012. No firmware-specific source changes. Install the file whose API major matches the device.
- **Clean exit** — other Flipper apps (Sub-GHz Read, NFC, Bad-USB) keep working perfectly after you exit

---

## How It Compares

| Feature | PINGEQUA RF Lab | Legacy NRF24 scanner apps |
|---|:---:|:---:|
| Built for dual-chip 2-in-1 boards | ✅ | ❌ Single-chip designs only |
| Plug-and-play across all major firmware | ✅ | ⚠️ Often firmware-specific tweaks needed |
| Clean co-existence with Flipper Sub-GHz / NFC apps | ✅ | ❌ Frequently break other apps until reboot |
| Max-hold spectrum-analyzer mode | ✅ | ❌ Rare |
| WiFi 1/6/11 + BLE 37/38/39 frequency markers | ✅ | ❌ |
| Adjustable dwell time (130–2000 µs) | ✅ | ⚠️ Usually fixed |
| Cursor channel inspection with live readout | ✅ | ❌ Rare |
| NRF24 jammer (7 modes incl. RPD reactive + WiFi band hop) | ✅ | ❌ Usually CW only, separate app |
| RPD-driven reactive BLE jamming | ✅ | ❌ |
| WiFi-band CW jamming (ch 1 / 6 / 11) | ✅ | ❌ |
| CSV scan export for analysis | ✅ | ❌ |
| Continuous active development | ✅ | ⚠️ Often abandoned |
| Compact FAP size | ✅ ~44 KB | varies |
| Open source MIT license | ✅ | varies |

---

## Screenshots

<table>
  <tr>
    <td align="center" width="50%">
      <img src="images/screen_menu.png" alt="PINGEQUA RF Lab main menu — Channel Scanner, NRF24 Signal Gen, About" width="300" /><br>
      <sub><b>Main Menu</b><br>Scanner · Signal Gen · About</sub>
    </td>
    <td align="center" width="50%">
      <img src="images/photo_scanner.png" alt="Channel Scanner running — live 2.4 GHz RPD spectrum" width="300" /><br>
      <sub><b>Channel Scanner</b><br>126-ch live RPD spectrum, max-hold</sub>
    </td>
  </tr>
  <tr>
    <td align="center" width="50%">
      <img src="images/screen_siggen.png" alt="NRF24 Signal Gen — CW mode on channel 42" width="300" /><br>
      <sub><b>NRF24 Signal Gen</b><br>7 TX modes — CW shown</sub>
    </td>
    <td align="center" width="50%">
      <img src="images/about.png" alt="About screen with scannable QR code to the shop" width="300" /><br>
      <sub><b>About</b> (v0.5.2)<br>On-device QR to shop &amp; GitHub</sub>
    </td>
  </tr>
</table>

---

## Install

### 1. Mount the board (hardware)

> ⚠️ **Always power off Flipper before inserting or removing the board** — hot-swapping the SPI bus can damage either chip.

1. **Power OFF** your Flipper Zero (hold `Back` ~2 seconds → Power off)
2. Insert the PINGEQUA 2-in-1 board onto the **LEFT GPIO headers** (the 18-pin row on the top-left edge)
3. **Power ON** Flipper

### 2. Install the app (software)

**qFlipper drag-and-drop** (recommended):
1. Download the FAP that matches the firmware on the Flipper (see [Firmware Compatibility](#firmware-compatibility)). From the v0.5.5 [release](https://github.com/pingequalab/rf-lab/releases/tag/v0.5.5):
   - Official **1.5.1**, Unleashed, RogueMaster (API 88) → `rf_lab-official-api88.2.fap`
   - Momentum **mntm-012** (API 87) → `rf_lab-momentum-mntm-012.fap`
2. In qFlipper: drag into `/ext/apps/GPIO/`
3. On Flipper: `Apps → GPIO → PINGEQUA RF Lab`

The file in [`dist/`](dist/) is the previous v0.5.4 build (API 87.1). It loads on Momentum mntm-012 and is rejected on official 1.5.1.

**ufbt sideload** (build from source). Install one of the two release SDKs, then launch:
```bash
pip install --user "ufbt==0.2.6"
git clone https://github.com/pingequalab/rf-lab
cd rf-lab
```

Official firmware 1.5.1 (API 88.2):
```bash
ufbt update --channel=release
ufbt launch
```

Momentum mntm-012 (API 87.1):
```bash
ufbt update --index-url=https://up.momentum-fw.dev/firmware/directory.json --channel=release
ufbt launch
```

---

## Usage

Launch from `Apps → GPIO → PINGEQUA RF Lab`. Main menu offers **Scanner**, **Jammer**, or **About** (v0.5.2+).

### Scanner
- ← / → moves the cursor (long-press = ±5 channels)
- ↑ / ↓ adjusts dwell time (short = ±10 µs, long = ±50 µs)
- OK pauses / resumes
- **Long-press OK exports current scan to CSV** at `/ext/apps_data/pingequa/scans/scan_<ts>.csv` (v0.5.0+). Top-right shows `SAVED!` for ~1 sec.
- Back returns to main menu
- When any bar saturates → automatic **MAX HOLD** mode (display freezes for analysis). Press OK to clear and rescan.

### Jammer (v0.4.0 — 7 modes)
- ↑ / ↓ cycles through 7 modes:
  1. **CW Custom** — single channel CW, ←/→ adjusts ±1 / ±5
  2. **BLE Adv** — blind CW hop {37, 38, 39}
  3. **BLE React** ★ — RPD-driven reactive jam (listens, jams on detection — 1st on Flipper NRF24)
  4. **WiFi 1** — CW hop across the WiFi-1 band (ch 1–23)
  5. **WiFi 6** — CW hop across the WiFi-6 band (ch 26–48)
  6. **WiFi 11** — CW hop across the WiFi-11 band (ch 51–73)
  7. **ALL 2.4G** — full-band CW sweep
- ← / → adjusts CW Custom channel (no-op in other modes)
- OK starts / stops
- Back returns to main menu — **session auto-logs** to `/ext/apps_data/pingequa/siggen/siggen_<ts>.csv` (mode, duration, chunks, reactive jam count) and **mode/channel persists** for next launch (v0.5.0+).
- Disrupts nearby BLE / 2.4 GHz WiFi at close range.
- **Note:** during a long continuous transmit run the radio may occasionally stop responding — hold **Left + Back** (~5 s) to reboot, then relaunch. **CW Custom** is the most robust for long sessions; use the multi-channel sweep modes in shorter bursts.

### Data Export (v0.5.0+)

The app writes research-grade data to your Flipper SD card. Open with qFlipper, Excel, Numbers, or `pandas.read_csv` (skip `#` comment lines):

**Filenames embed RTC wall-clock + summary** (v0.5.2+) so sorting by name in qFlipper = sorting by time, and the filename itself tells you what's inside without opening it:

```
/ext/apps_data/pingequa/
├── jammer.conf                                        ← Last jammer mode + channel (auto-restored)
├── scans/scan_<YYYY-MM-DD>_<HHMMSS>_ch<peak>.csv      ← e.g. scan_2026-05-15_143022_ch42.csv
└── jammer/jam_<YYYY-MM-DD>_<HHMMSS>_<mode>_<dur>s.csv ← e.g. jam_2026-05-15_143530_BLEreact_19s.csv
```

When the RTC isn't set, filenames fall back to a boot-tick form (`scan_boot<tick>_ch<peak>.csv`) so you never see `1970-01-01` garbage names.

<table>
  <tr>
    <td align="center" width="50%">
      <img src="images/qflipper_data_dir.png" alt="qFlipper showing /ext/apps_data/pingequa/" width="100%" /><br>
      <sub>Data directory in qFlipper's file manager</sub>
    </td>
    <td align="center" width="50%">
      <img src="images/qflipper_session_logs.png" alt="qFlipper showing accumulated jammer session CSVs" width="100%" /><br>
      <sub>Each Back-from-Jammer creates one session CSV</sub>
    </td>
  </tr>
</table>

**Scanner CSV** contains all 126 channel hits, peak channel, dwell, sweep count, and wall-clock datetime.
**Jammer session CSV** (v0.5.2+ schema) carries `datetime`, `mode`, `engine`, derived `target_freq_mhz`, and `scene_duration_s` — with conditional `cw_channel` / `reactive_jams` only when the mode actually produces those values. No more redundant or misleading fields.

Detailed walkthroughs: [docs/QUICKSTART.md](docs/QUICKSTART.md), [docs/UI_GUIDE.md](docs/UI_GUIDE.md), [docs/USE_CASES.md](docs/USE_CASES.md).

---

## Using Pingequa Hardware with Other Apps

The PINGEQUA 2-in-1 board works with this official FAP **and** with third-party apps and Flipper's built-in radio tools — but the dual-chip layout uses non-default SPI pins, so other apps need a one-time setup.

### Third-party NRF24 apps (NRF24 Sniffer, Mousejack, etc.)

The Pingequa NRF24 chip is wired to the **Extra 7** SPI bus (rather than the conventional pin set). On **Momentum** firmware:

1. From the main menu, navigate to **`Settings → Protocols → GPIO Pins`** (path may read `START → PROTOCOLS → GPIO PINS` on some menu variants)
2. Set **NRF24 SPI** to **`Extra 7`**
3. Launch your third-party NRF24 app normally

> Some third-party apps hard-code different pin assignments and may show partial incompatibility. The official PINGEQUA RF Lab app (this one) handles routing transparently — **no setup needed**.

### Official Sub-GHz app with Pingequa CC1101

To use Flipper's built-in **Sub-GHz Read / Read External** through the longer-range Pingequa CC1101:

1. Open **Sub-GHz** from the main menu
2. Go to **`Radio Settings → Module`**
3. Select **`External`**
4. Use Sub-GHz normally — it now routes through the Pingequa CC1101

> The `External` option is exposed on Momentum, Unleashed, and RogueMaster. Official Flipper firmware (OFW) has a similar `Read External` mode but no settings UI — use as-is.

If you don't see `External`, see [docs/HARDWARE.md](docs/HARDWARE.md) for firmware-specific notes.

---

## Hardware

> ### 🛒 [PINGEQUA Flipper Zero NRF24+CC1101 2-in-1 RF Devboard](https://www.pingequa.com/products/flipper-zero-2-in-1-rf-module?utm_source=github&utm_medium=referral&utm_campaign=fz-rflab-2in1&utm_content=hardware-block)
>
> The only officially supported hardware. One module gives your Flipper both 2.4 GHz (nRF24L01+) and Sub-GHz (CC1101) chips, software-switched. No jumpers, no rewiring.

Other NRF24 boards may partially work but are unsupported. See [docs/HARDWARE.md](docs/HARDWARE.md).

---

## Firmware Compatibility

A FAP loads when its **API major** matches the firmware. The minor compare in `flipper_application_manifest_is_too_new` is commented out on Official 1.5.1, Unleashed `unlshd-094`, Momentum `mntm-012`, and RogueMaster branch `420` (read 2026-11-05). The file is still linked against one SDK, so an API 88 file does not load on API 87. v0.5.4 was a single API 87.1 file, and official 1.5.1 rejects it. v0.5.5 is that same application built twice.

API numbers below are the `Version` row of `targets/f7/api_symbols.csv`, read 2026-11-05.

| Firmware | Version checked | API | v0.5.5 asset |
|---|---|---|---|
| [Official](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/targets/f7/api_symbols.csv) release | 1.5.1 | 88.2 | `rf_lab-official-api88.2.fap` |
| [Official](https://github.com/flipperdevices/flipperzero-firmware/blob/dev/targets/f7/api_symbols.csv) dev | dev | 88.4 | same API 88.2 file |
| [Unleashed](https://github.com/DarkFlippers/unleashed-firmware/blob/unlshd-094/targets/f7/api_symbols.csv) release | unlshd-094 | 88.19 | same API 88.2 file |
| [Unleashed](https://github.com/DarkFlippers/unleashed-firmware/blob/dev/targets/f7/api_symbols.csv) dev | dev | 88.24 | same API 88.2 file |
| [RogueMaster](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/blob/RM1024-1347-392a65c3/targets/f7/api_symbols.csv) release | RM1024-1347-392a65c3 | 88.23 | same API 88.2 file |
| [RogueMaster](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/blob/420/targets/f7/api_symbols.csv) branch 420 | 420 @ 2026-11-05 | 88.24 | same API 88.2 file |
| [Momentum](https://github.com/Next-Flip/Momentum-Firmware/blob/mntm-012/targets/f7/api_symbols.csv) release | mntm-012 | 87.1 | `rf_lab-momentum-mntm-012.fap` |
| [Momentum](https://github.com/Next-Flip/Momentum-Firmware/blob/dev/targets/f7/api_symbols.csv) dev | dev | 87.1 | same API 87.1 file |

Xtreme was not in this check. [flash.pingequa.com/devices/flipper-rf-lab](https://flash.pingequa.com/devices/flipper-rf-lab) reads `firmware_api_major` and `firmware_api_minor` and writes the matching file to `/ext/apps/GPIO/pingequa_rf_toolkit.fap`. Neither v0.5.5 file has been run on a device yet.

---

## Roadmap

| Version | Status | Headline |
|---|:---:|---|
| v0.2.0 | ✅ | Channel scanner, max-hold, WiFi/BLE markers |
| v0.3.0 | ✅ | NRF24 jammer (CW + Sweep), main menu |
| v0.4.0 | ✅ | RPD reactive BLE jam, WiFi-band CW jam, 7 jammer modes |
| v0.5.0 | ✅ | CSV scan export, jammer session log, settings persistence |
| v0.5.1 | ✅ | OFW compatibility — single-handle SPI arbiter, all firmwares |
| v0.5.2 | ✅ | Readable export filenames, jammer log redesign, About screen + QR codes |
| v0.5.3 | ✅ | In-app rename: transmit feature now displays as "Signal Generator" (neutral, instrument-grade naming) |
| v0.5.4 | ✅ | Signal Generator now actually radiates (held-bus driver + FIFO-flush ignition); honest power wording; long-run reboot note |
| **v0.5.5** | ✅ | **API 88.2 build (Official, Unleashed, RogueMaster) and API 87.1 build (Momentum mntm-012)** |
| v0.6.x | 🔜 next | **Shaped by community feedback** — candidates: cross-chip dual-band sweep (nRF24 2.4 GHz + CC1101 Sub-GHz in one view), spectrum CSV export, custom scan presets. [Tell us what you want](../../issues/new?template=feature.yml) |
| v1.0.0 | planned | Polished release, full documentation set, Catalog submission |
| v1.5.0 | concept | Companion mobile viewer over Bluetooth |
| v2.0.0 | concept | Unified UI bridging NRF24 + CC1101 |

[Vote on the roadmap](../../issues) · [Request a feature](../../issues/new?template=feature.yml)

---

## Documentation

| Doc | For |
|---|---|
| [Quickstart](docs/QUICKSTART.md) | First-time users — 5 minutes to first scan |
| [UI Guide](docs/UI_GUIDE.md) | Every pixel of the screen explained |
| [Hardware](docs/HARDWARE.md) | Pin map, firmware compatibility |
| [Use Cases](docs/USE_CASES.md) | 10 real-world scenarios |
| [FAQ](docs/FAQ.md) | Common questions |
| [Troubleshooting](docs/TROUBLESHOOTING.md) | Error decoding, hardware diagnostics |
| [Build](docs/BUILD.md) | ufbt setup, contributor build steps |

---

## Legal Notice

PINGEQUA RF Lab includes opt-in **transmit features (NRF24 Jammer)** — the Channel Scanner is passive listen-only, but the 7 Jammer modes actively transmit on the 2.4 GHz ISM band.

> **On output power.** The 2.4 GHz band is power-regulated (FCC §15 in the US, ETSI EN 300 328 in the EU, equivalent elsewhere). **To respect those legal limits, the firmware deliberately restrains output power and does not drive the radio to its electrical maximum.** This is an intentional compliance choice — the transmit range is correspondingly modest by design, not a defect.

You are responsible for compliance with your local regulations. Use only on hardware and networks you own or have authorization to test. The authors accept no liability for misuse.

You are responsible for compliance with your local regulations. Use only on hardware and networks you own or have authorization to test. The authors accept no liability for misuse.

Full disclosure: [SECURITY.md](SECURITY.md).

---

## Support

- **Issues**: [GitHub Issues](../../issues)
- **Email**: [support@pingequa.com](mailto:support@pingequa.com)
- **Hardware**: [pingequa.com](https://www.pingequa.com/?utm_source=github&utm_medium=referral&utm_campaign=brand&utm_content=footer-store)
- **YouTube**: [@PINGEQUA](https://www.youtube.com/@PINGEQUA)

---

## License

[MIT](LICENSE) © 2026 PINGEQUA. PINGEQUA® is a trademark.

---

<div align="center">

<sub><b>Precision Gear for Hackers.</b></sub><br>
<sub>Made for the PINGEQUA 2-in-1 RF Devboard ·
<a href="https://www.pingequa.com/?utm_source=github&utm_medium=referral&utm_campaign=brand&utm_content=footer-brand">PINGEQUA</a></sub>

⭐ **Star the repo if PINGEQUA RF Lab saves you time.**

</div>
