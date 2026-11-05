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

The source uses the public Flipper HAL. The installed file still has to match the firmware API major. Checked 2026-11-05 from `targets/f7/api_symbols.csv`:

| Firmware | API | v0.5.5 file |
|---|---|---|
| Official 1.5.1, Unleashed unlshd-094, RogueMaster RM1024 | 88.x | `rf_lab-official-api88.2.fap` |
| Momentum mntm-012 | 87.1 | `rf_lab-momentum-mntm-012.fap` |

v0.5.4 (API 87.1) does not load on official 1.5.1. These two v0.5.5 files have not been run on a device. Xtreme was not rechecked. Details: [Firmware Compatibility](../README.md#firmware-compatibility).

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
