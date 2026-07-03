# Hardware

## Required hardware

> ### 🛒 [PINGEQUA Flipper Zero NRF24+CC1101 2-in-1 RF Devboard](https://www.pingequa.com/products/flipper-zero-nrf24-cc1101-2-in-1-rf-devboard?utm_source=github&utm_medium=docs_hardware&utm_campaign=rflab&utm_content=cta_hero)

The only officially supported hardware for PINGEQUA RF Lab. One expansion module provides Flipper Zero with both nRF24L01+ (2.4 GHz) and CC1101 (Sub-GHz) chips on a shared SPI bus, with software-defined chip switching.

Worldwide shipping (195+ countries) via [pingequa.com](https://www.pingequa.com/?utm_source=github&utm_medium=docs_hardware&utm_campaign=rflab).

## Pin mapping

| Function | Flipper pin |
|----------|-------------|
| nRF24L01+ CSN | PC3 |
| nRF24L01+ CE | PB2 |
| CC1101 CSN | PA4 |
| CC1101 GD0 | PB2 (shared with NRF24 CE — software-switched) |
| Shared MISO | PA6 |
| Shared MOSI | PA7 |
| Shared SCK | PB3 |

## Firmware compatibility

The app uses only standard Flipper HAL APIs. Any firmware shipping these works.

✅ **Tested**:
- Momentum (mntm-dev) — primary target
- Unleashed
- RogueMaster

✅ **Compatible by API surface**:
- Official Flipper firmware (API ≥ 87.1)
- Xtreme

If you encounter firmware-specific issues, [file a bug](../../../issues/new?template=bug.yml) with the firmware version.

## Channel-to-frequency reference

```
nRF24 ch N ↔ frequency 2400 + N MHz
```

| nRF24 ch | Frequency | Common use |
|----------|-----------|------------|
| 2 | 2402 MHz | BLE adv 37 |
| 12 | 2412 MHz | WiFi 1 |
| 26 | 2426 MHz | BLE adv 38 |
| 37 | 2437 MHz | WiFi 6 |
| 50 | 2450 MHz | Microwave oven (ISM center) |
| 62 | 2462 MHz | WiFi 11 |
| 72 | 2472 MHz | WiFi 13 (EU/CN) |
| 80 | 2480 MHz | BLE adv 39 |

## Other hardware

❓ **DIY or non-PINGEQUA boards**: please use the [hardware compatibility template](../../../issues/new?template=hardware.yml) to share your results — both successes and failures help the community.

⚠️ **Wiring errors are the #1 cause** of "Board Not Detected" reports. The PINGEQUA 2-in-1 board is plug-and-play; DIY setups require careful pin verification.

---

🛒 **Get the recommended hardware**: [PINGEQUA 2-in-1 RF Devboard](https://www.pingequa.com/products/flipper-zero-nrf24-cc1101-2-in-1-rf-devboard?utm_source=github&utm_medium=docs_hardware&utm_campaign=rflab&utm_content=footer)
