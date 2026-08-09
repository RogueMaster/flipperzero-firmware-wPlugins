# Internet Time Screen

External Flipper Zero app (`.fap`) that shows Swatch Internet Time (`@000`–`@999`)
alongside local time. Optional **DCF77** sync corrects the Flipper RTC from an
external 77.5 kHz GPIO receiver. Stock-firmware compatible; no network or
firmware fork. DCF77 is **not** received via SubGHz (CC1101 cannot tune
77.5 kHz).

## Requirements

- [uFBT](https://github.com/flipperdevices/flipperzero-ufbt) on the official
  **release** channel (local builds use SDK **1.4.3** / API **87.1**)
- Flipper Zero with microSD card, USB data cable
- C11 host compiler (`cc`) for unit tests
- (Optional) DCF77 receiver module + ferrite antenna for live sync

## Build

```bash
ufbt lint
ufbt
```

The FAP lands in `dist/`.

## Host tests

```bash
./tests/run_host_tests.sh
```

## Launch on device

```bash
ufbt launch
```

- **Clock:** Short OK / Back exits. Long OK opens settings.
- **Settings:** Up/Down select row; Left/Right change offset / Auto / Invert;
  OK on “Start sync” opens DCF77 sync; Back saves and returns.
- **DCF77 Sync:** listens on GPIO **C0**; Back cancels; Long OK runs a **demo**
  fixture that writes a known time through the same RTC path (no RF needed).

Configure UTC offset (15-minute steps) so Biel Mean Time is derived correctly
from the Flipper RTC. Auto-sync (default On) starts sync once per app session
when the last success is older than 12 hours.

## DCF77 hardware

Wiring (confirm against your module silk; matches common Flipper DCF77 apps):

| Module | Flipper |
| --- | --- |
| VDD | pin 9 (3V3) |
| GND | pin 11 (GND) |
| PON / P (active-low) | GND when required |
| OUT / T | pin 16 (**C0**) |

Keep the ferrite rod ≥ ~10 cm from the Flipper and near a window. Reception
across Europe is typically better at night.

## Where to buy modules

- [Reichelt — FREI DCF 77 receiver](https://www.reichelt.com/ie/en/shop/product/dcf_77_receiver_module-57772)
- [Shotech — DCF-1060N-MAS ± antenna](https://www.shotech.de/en/dcf-1060n-mas-77-5-khz-receiver-module-with-mas6181b.html)
- [CANADUINO / Universal Solder — Receiver V4](https://www.universal-solder.ca/product/dcf77-funkuhr-empfanger-atomic-clock-receiver-77-5khz-for-europe/)
- [Xtals — DCF77 single-frequency module](https://www.xtals.co.uk/product-page/dcf77-single-frequency-modules-radio-time-signal-receiver)
- [Amazon.de — DCF77 module pack](https://www.amazon.de/-/en/DCF77-Receiver-Module-Radio-Antenna/dp/B0C7L2BB2Y)
- [Amazon.com — 77.5 kHz module](https://www.amazon.com/frequency-Receiver-Demodulator-Synchronization-Operating/dp/B0H4FWC776)

Search “DCF77 module” / “77.5 kHz RCC” if a listing disappears. Prefer 1.1–3.3 V
boards with a tuned ferrite antenna.

## Note on firmware

Development acceptance may run on API-compatible custom firmware (e.g.
RogueMaster) when API major/minor matches the release SDK. Catalog-oriented
builds still target the official release SDK.
