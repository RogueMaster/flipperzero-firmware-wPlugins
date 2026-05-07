<div align="center">

<a href="https://www.pingequa.com/products/flipper-zero-nrf24-cc1101-2-in-1-rf-devboard?utm_source=github&utm_medium=readme&utm_campaign=rflab&utm_content=banner">
  <img src="images/hardware_devboard.jpg" alt="PINGEQUA 2-in-1 RF Devboard — nRF24L01+ + CC1101 with antennas" width="55%" />
</a>

# PINGEQUA RF Lab

### Plug-and-play across every firmware.

**2.4 GHz spectrum analyzer + NRF24 jammer for Flipper Zero, designed for the [PINGEQUA 2-in-1 RF Devboard](https://www.pingequa.com/products/flipper-zero-nrf24-cc1101-2-in-1-rf-devboard?utm_source=github&utm_medium=readme&utm_campaign=rflab&utm_content=hero).**

[![Platform](https://img.shields.io/badge/platform-Flipper%20Zero-FF8200?style=flat-square)](https://flipperzero.one/)
[![Release](https://img.shields.io/github/v/release/pingequalab/rf-lab?style=flat-square&color=success&label=release)](https://github.com/pingequalab/rf-lab/releases/latest)
[![Firmware](https://img.shields.io/badge/firmware-OFW%20%7C%20Momentum%20%7C%20Unleashed%20%7C%20RogueMaster-blue?style=flat-square)](#firmware-compatibility)
[![FAP size](https://img.shields.io/badge/FAP-40%20KB-success?style=flat-square)](dist/)
[![License](https://img.shields.io/badge/license-MIT-yellow?style=flat-square)](LICENSE)
[![Hardware](https://img.shields.io/badge/hardware-PINGEQUA%202--in--1-000000?style=flat-square)](https://www.pingequa.com/products/flipper-zero-nrf24-cc1101-2-in-1-rf-devboard?utm_source=github&utm_medium=readme&utm_campaign=rflab&utm_content=badge)

[**🛒 Get the Hardware**](https://www.pingequa.com/products/flipper-zero-nrf24-cc1101-2-in-1-rf-devboard?utm_source=github&utm_medium=readme&utm_campaign=rflab&utm_content=cta_top) · [**📦 Download FAP**](../../releases) · [**📖 Quickstart**](docs/QUICKSTART.md) · [**❓ FAQ**](docs/FAQ.md)

</div>

---

## What It Does

An open-source Flipper Application Package (FAP) that turns Flipper Zero plus the PINGEQUA 2-in-1 RF Devboard into:

- **2.4 GHz spectrum analyzer** — 126 channels real-time with WiFi 1/6/11 + BLE 37/38/39 band markers, max-hold, microsecond dwell tuning, cursor inspection, **CSV scan export** (long-press OK, v0.5.0+)
- **NRF24 jammer with 7 modes** (v0.4.0+):
  - CW Custom · BLE Adv · **BLE React** (RPD reactive — first on Flipper NRF24) · WiFi 1/6/11 (pilot-aware OFDM) · ALL 2.4G
  - Real-device verified to disconnect BLE devices and 2.4 GHz WiFi within room range
  - **Auto session log** + **mode/channel persistence** (v0.5.0+)
- **Plug-and-play across firmware** — Momentum, Unleashed, RogueMaster, Xtreme, and Official Flipper firmware. No firmware-specific patching.
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
| NRF24 jammer (7 modes incl. RPD reactive + WiFi pilot-aware) | ✅ | ❌ Usually CW only, separate app |
| RPD-driven reactive BLE jamming | ✅ | ❌ |
| WiFi pilot-aware OFDM jamming (Clancy 2011) | ✅ | ❌ |
| CSV scan export for analysis | ✅ | ❌ |
| Continuous active development | ✅ | ⚠️ Often abandoned |
| Compact FAP size | ✅ 40 KB | varies |
| Open source MIT license | ✅ | varies |

---

## Screenshots

<table>
  <tr>
    <td align="center" width="33%">
      <img src="images/menu.png" alt="Main menu" width="240" /><br>
      <sub><b>Main Menu</b><br>Channel Scanner / NRF24 Jammer</sub>
    </td>
    <td align="center" width="33%">
      <img src="images/ChannelScanner.png" alt="Channel Scanner UI" width="240" /><br>
      <sub><b>Channel Scanner</b><br>126-ch live RPD spectrum, max-hold</sub>
    </td>
    <td align="center" width="33%">
      <img src="images/NRF24Jammer.png" alt="NRF24 Jammer BLE React mode" width="240" /><br>
      <sub><b>NRF24 Jammer (BLE React)</b><br>RPD reactive on adv ch 37/38/39</sub>
    </td>
  </tr>
</table>

---

## Install

**qFlipper drag-and-drop** (recommended):
1. Download `pingequa_rf_toolkit.fap` from [Releases](../../releases) or [`dist/`](dist/)
2. In qFlipper: drag into `/ext/apps/GPIO/`
3. On Flipper: `Apps → GPIO → PINGEQUA RF Lab`

**ufbt sideload**:
```bash
pip install --user ufbt
ufbt update --channel=dev
git clone https://github.com/pingequa/pingequa_rf_toolkit
cd pingequa_rf_toolkit
ufbt launch
```

---

## Usage

Launch from `Apps → GPIO → PINGEQUA RF Lab`. Main menu offers **Scanner** or **Jammer**.

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
  4. **WiFi 1** — pilot-aware OFDM jam (4 pilots, +7.5 dB efficient per Clancy 2011)
  5. **WiFi 6** — same for WiFi channel 6
  6. **WiFi 11** — same for WiFi channel 11
  7. **ALL 2.4G** — full-band CW sweep
- ← / → adjusts CW Custom channel (no-op in other modes)
- OK starts / stops
- Back returns to main menu — **session auto-logs** to `/ext/apps_data/pingequa/jammer/session_<ts>.csv` (mode, duration, chunks, reactive jam count) and **mode/channel persists** for next launch (v0.5.0+).
- Real-device verified: BLE devices and 2.4G WiFi can be disconnected within room range.

### Data Export (v0.5.0+)

The app writes research-grade data to your Flipper SD card. Open with qFlipper, Excel, Numbers, or `pandas.read_csv` (skip `#` comment lines):

```
/ext/apps_data/pingequa/
├── jammer.conf                      ← Last jammer mode + channel (auto-restored)
├── scans/scan_<boot_ms>.csv         ← Long-press OK in Scanner exports here
└── jammer/session_<start_ms>.csv    ← Each Jammer session auto-logs here
```

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

**Scanner CSV** contains all 126 channel hits, peak channel, dwell, sweep count.
**Jammer session CSV** contains mode, duration, total chunks, and (for BLE React) reactive jam count.

Detailed walkthroughs: [docs/QUICKSTART.md](docs/QUICKSTART.md), [docs/UI_GUIDE.md](docs/UI_GUIDE.md), [docs/USE_CASES.md](docs/USE_CASES.md).

---

## Hardware

> ### 🛒 [PINGEQUA Flipper Zero NRF24+CC1101 2-in-1 RF Devboard](https://www.pingequa.com/products/flipper-zero-nrf24-cc1101-2-in-1-rf-devboard?utm_source=github&utm_medium=readme&utm_campaign=rflab&utm_content=hardware_block)
>
> The only officially supported hardware. One module gives your Flipper both 2.4 GHz (nRF24L01+) and Sub-GHz (CC1101) chips, software-switched. No jumpers, no rewiring.

Other NRF24 boards may partially work but are unsupported. See [docs/HARDWARE.md](docs/HARDWARE.md).

---

## Firmware Compatibility

| Firmware | Status |
|---|:---:|
| Momentum (mntm-dev) | ✅ Primary build target |
| Unleashed | ✅ Compatible |
| RogueMaster | ✅ Compatible |
| Official Flipper firmware (API ≥ 87.1) | ✅ Compatible |
| Xtreme | ✅ Compatible |

---

## Roadmap

| Version | Status | Headline |
|---|:---:|---|
| v0.2.0 | ✅ | Channel scanner, max-hold, WiFi/BLE markers |
| v0.3.0 | ✅ | NRF24 jammer (CW + Sweep), main menu |
| v0.4.0 | ✅ | RPD reactive BLE jam, WiFi pilot-aware OFDM, 7 jammer modes |
| **v0.5.0** | ✅ | **CSV scan export, jammer session log, settings persistence** |
| v0.6.x | 🔜 next | BLE protocol-aware (decode access address before jamming) |
| v0.7.x | planned | Multi-waveform mixing, About scene, jam intensity UI |
| v1.0.0 | planned | Polished release, multi-language UI (zh / en) |
| v1.5.0 | planned | Companion mobile viewer over Bluetooth |
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

PINGEQUA RF Lab includes opt-in **transmit features (NRF24 Jammer)** — the Channel Scanner is passive listen-only, but the 7 Jammer modes actively transmit on 2.4 GHz at up to +20 dBm. Active RF emission in the unlicensed 2.4 GHz band is regulated (FCC §15 in the US, ETSI EN 300 328 in the EU, equivalent elsewhere).

You are responsible for compliance with your local regulations. Use only on hardware and networks you own or have authorization to test. The authors accept no liability for misuse.

Full disclosure: [SECURITY.md](SECURITY.md).

---

## Support

- **Issues**: [GitHub Issues](../../issues)
- **Email**: [support@pingequa.com](mailto:support@pingequa.com)
- **Hardware**: [pingequa.com](https://www.pingequa.com/?utm_source=github&utm_medium=readme&utm_campaign=rflab&utm_content=footer_store)
- **YouTube**: [@PINGEQUA](https://www.youtube.com/@PINGEQUA)

---

## License

[MIT](LICENSE) © 2026 PINGEQUA. PINGEQUA® is a trademark.

---

<div align="center">

<sub><b>Precision Gear for Hackers.</b></sub><br>
<sub>Made for the PINGEQUA 2-in-1 RF Devboard ·
<a href="https://www.pingequa.com/?utm_source=github&utm_medium=readme&utm_campaign=rflab&utm_content=footer_brand">PINGEQUA</a></sub>

⭐ **Star the repo if PINGEQUA RF Lab saves you time.**

</div>
