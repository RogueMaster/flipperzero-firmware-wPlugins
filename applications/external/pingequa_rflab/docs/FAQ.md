# Frequently Asked Questions

## General

### What is PINGEQUA RF Lab?

PINGEQUA RF Lab is an open-source Flipper Application Package (FAP) that turns Flipper Zero plus the [PINGEQUA 2-in-1 RF Devboard](https://www.pingequa.com/products/flipper-zero-nrf24-cc1101-2-in-1-rf-devboard?utm_source=github&utm_medium=faq&utm_campaign=rflab) into a real-time 2.4 GHz spectrum analyzer with optional NRF24 jammer (v0.3.0+).

### How is it different from existing NRF24 scanners?

Most existing NRF24 scanner apps were built for older single-chip add-on boards. They don't handle the dual-chip 2.4 GHz + Sub-GHz layout of modern expansion modules like the PINGEQUA 2-in-1, often resulting in failed scans, broken Sub-GHz access after exit, or firmware-specific tweaks needed.

PINGEQUA RF Lab was designed from the start for the 2-in-1 board's actual electrical layout. See the [How It Compares](../README.md#how-it-compares) table.

### Is it free?

Yes — open source under [MIT License](../LICENSE). The source code, FAP binary, and documentation are all free. The hardware (PINGEQUA 2-in-1 board) is a separate purchase.

### Is the hardware required?

For real-world use, yes. The app is officially supported only on the [PINGEQUA 2-in-1 RF Devboard](https://www.pingequa.com/products/flipper-zero-nrf24-cc1101-2-in-1-rf-devboard?utm_source=github&utm_medium=faq&utm_campaign=rflab).

## Installation & Setup

### Which Flipper firmwares are supported?

| Firmware | API checked 2026-11-05 | v0.5.5 file |
|---|---|---|
| Official 1.5.1 and current dev (88.4) | 88.2 / 88.4 | `rf_lab-official-api88.2.fap` |
| Unleashed unlshd-094 and current dev | 88.19 / 88.24 | same API 88.2 file |
| RogueMaster RM1024 and branch 420 | 88.23 / 88.24 | same API 88.2 file |
| Momentum mntm-012 and current dev | 87.1 | `rf_lab-momentum-mntm-012.fap` |

The loader compares API major only. v0.5.4 was API 87.1 and does not load on official 1.5.1. Xtreme was not rechecked. See [Firmware Compatibility](../README.md#firmware-compatibility).

### How do I install?

Three options, easiest first:

1. **qFlipper drag-and-drop**: Download the v0.5.5 asset that matches the table above from [Releases](https://github.com/pingequalab/rf-lab/releases/tag/v0.5.5), drag it into `/ext/apps/GPIO/` via qFlipper.
2. **Browser**: [flash.pingequa.com/devices/flipper-rf-lab](https://flash.pingequa.com/devices/flipper-rf-lab) reads the Flipper's API and writes the matching file.
3. **ufbt sideload**: install the SDK for that API major, then `ufbt launch`. See the README. The `dist/pingequa_rf_toolkit.fap` in the tree is still the v0.5.4 API 87.1 file.

See [Quickstart](QUICKSTART.md) for step-by-step.

## Usage

### How do I read the bars?

Each vertical bar represents a 1 MHz wide channel between 2400 MHz and 2525 MHz. Bar height shows accumulated activity at that frequency. Higher bar = more detection.

See [UI Guide](UI_GUIDE.md) for full details.

### What's "max hold" mode?

When any bar reaches the top, the app auto-pauses the scan and freezes the display — the standard spectrum-analyzer convention. Captures the snapshot for analysis. Press OK to clear and rescan.

### How do I find my home WiFi channel?

1. Launch the app
2. Wait 10–30 seconds with normal WiFi activity nearby
3. Look for a tall bar near one of the ▲ markers (WiFi 1 / 6 / 11)
4. Move cursor onto the tall bar → header shows the exact frequency

### What's the difference between dwell and sweep rate?

- **Dwell** = how long each channel is sampled (130–2000 µs, ↑/↓ to adjust)
- **Sweep rate** = how many full 126-channel passes per second

Lower dwell → faster sweep but might miss brief signals. Higher dwell → slower sweep, catches weaker activity. Default 150 µs gives roughly 17 sweeps per second.

### Can I export scan data?

Not in v0.3.0. CSV export is planned for v0.4.0 — see [Roadmap](../README.md#roadmap).

### Does the Jammer work on every channel?

Yes — CW (constant wave) mode allows targeting any channel 0–125 (2400–2525 MHz). Sweep mode hops through all 126 channels in chunks. **Output power is deliberately restrained to respect the legal transmit-power limits for the 2.4 GHz band** — the firmware does not drive the radio to its electrical maximum. As a result the transmit range is modest by design (close range), which is an intentional compliance choice rather than a limitation.

## Hardware

### Which hardware is supported?

✅ **Officially**: The [PINGEQUA Flipper Zero NRF24+CC1101 2-in-1 RF Devboard](https://www.pingequa.com/products/flipper-zero-nrf24-cc1101-2-in-1-rf-devboard?utm_source=github&utm_medium=faq_hardware&utm_campaign=rflab).

⚠️ **Other NRF24 boards**: may partially work, but unsupported. The app's pin mapping assumes the PINGEQUA 2-in-1 layout.

### Will it interfere with my Sub-GHz / NFC / Bad-USB apps?

No. The app cleans up all pins on exit. After you Back out, Flipper's stock apps (Sub-GHz Read, NFC Read, etc.) work normally.

If you observe interference, please file a bug — that's a regression we want to fix.

## Legal & Safety

### Is RF scanning legal?

**Passive listening** (which is what the scanner does) is generally unregulated worldwide. You're not transmitting, just observing — comparable to using a radio receiver.

**Active transmission** (the v0.3.0+ Jammer feature) requires opt-in user activation and is subject to local regulations:
- US: FCC §15 / §333
- EU: ETSI EN 300 328
- Other regions: equivalent national authorities

You are responsible for compliance with your local laws. See [SECURITY.md](../SECURITY.md) for full disclosure.

### Can I use this for security research?

Yes, for **authorized** research on equipment you own or have permission to test. Common use cases:
- Auditing your home WiFi for congestion / interference
- Investigating an unknown 2.4 GHz emitter in your space
- BLE device discovery / mapping
- Educational demonstrations
- Coexistence testing on equipment you own

Not a tool for unauthorized eavesdropping, network attack, or interfering with networks you don't own.

## Development

### Can I contribute?

Yes! See [CONTRIBUTING.md](../CONTRIBUTING.md) for guidelines.

### Can I fork and modify?

The MIT license permits commercial and private modification with attribution. Note: PINGEQUA® is a registered trademark — forks must use a different name.

### How do I report a bug?

Use the [bug report template](../../../issues/new?template=bug.yml). Include firmware, hardware, exact steps, and any error screen text.

### How do I suggest a feature?

Use the [feature request template](../../../issues/new?template=feature.yml). Check the [Roadmap](../README.md#roadmap) first — your idea may already be planned.

## Brand & Support

### Who makes the PINGEQUA boards?

[PINGEQUA](https://www.pingequa.com/?utm_source=github&utm_medium=faq_brand&utm_campaign=rflab) is a hardware brand specializing in expansion modules for Flipper Zero, M5Cardputer, and M5Stick. Tagline: "Precision Gear for Hackers".

### Where do I buy the board?

Official store with worldwide shipping (195+ countries):
🛒 **[pingequa.com](https://www.pingequa.com/products/flipper-zero-nrf24-cc1101-2-in-1-rf-devboard?utm_source=github&utm_medium=faq_buy&utm_campaign=rflab)**

### Where can I get support?

- **GitHub Issues**: [bug reports](../../../issues/new?template=bug.yml) / [feature requests](../../../issues/new?template=feature.yml)
- **GitHub Discussions**: design questions, idea exchange
- **Email**: [support@pingequa.com](mailto:support@pingequa.com)
- **YouTube**: [@PINGEQUA](https://www.youtube.com/@PINGEQUA) for video tutorials

---

Question not answered here? [Open a discussion](../../../discussions) or email [support@pingequa.com](mailto:support@pingequa.com).
