---
title: Fujitsu AC IR Compatibility — ASHG12KPCE-based app
---

# Fujitsu AC IR Compatibility

Reference notes for the Flipper Zero Fujitsu AC remote app, based on reverse-engineered signals from a Fujitsu General ASHG12KPCE.

## Protocol identification

The app's captured signal uses the Fujitsu AC IR protocol in its **128-bit (16-byte) long-frame** form for most commands, and a **56-bit (7-byte) short-frame** form for a handful of stateless button presses (Set, Power Off, Economy, Powerful).

Fixed header bytes (long frame, bytes 0–7):

```
0x14 0x63 0x00 0x10 0x10 0xFE 0x09 0x31
```

Fixed header bytes (short frame, bytes 0–4 — byte 5 onward carries the button command):

```
0x14 0x63 0x00 0x10 0x10
```

This header is a byte-for-byte match to the protocol variant the IRremoteESP8266 project calls **`ARREW4E`** (protocol/version byte = `0x31` at offset 7; other Fujitsu variants use `0x30` there, and older ones use a `0xFC` rate byte instead of `0xFE`). This is the variant used by the **AR-REW1E / AR-REW4E** family of Fujitsu remotes.

Why this matters: Fujitsu's AC protocol has several incompatible sub-variants (`ARRAH2E`, `ARDB1`, `ARREB1E`, `ARJW2`, `ARRY4`, `ARREW4E`) with different frame lengths, checksums, and bit layouts. The IRremoteESP8266 maintainers warn that sending the wrong variant's commands to a unit — swing/mode commands especially — can lock the A/C up until it's physically power-cycled. So compatibility claims below are split by confidence level, and only the "Confirmed" tier should be advertised without caveats.

## Compatibility tiers

### Tested

- **ASHG12KPCE** (this app's source unit)

### Confirmed (byte-identical header, independently verified)

- **ASTG09K**, **ASTG18K** — confirmed by community reverse-engineering (IRremoteESP8266 discussion #1701)
- **ASYG09KETA-B** — confirmed working with the `ARREW4E` model setting (IRremoteESP8266 issue #1455)
- Remotes: **AR-REW1E**, **AR-REW4E**

### Likely (same product generation/remote family, not independently byte-verified)

Fujitsu General's R32 "K-series" wall-mount splits typically ship one shared remote across a whole capacity range, so these are good candidates but should be flagged as unconfirmed until a user reports back:

- ECO Series (Compact) KPCE line: **ASHG07KPCE**, **ASHG09KPCE** (same family/range as ASHG12KPCE)
- Wider KMTB/KMTA/KPCA range grouped under one aftermarket universal remote: **ASHG07KMTB**, **ASHG09KMTB**, **ASHG12KMTB**, **ASHG14KMTB**, **ASHG18KMTA**, **ASHG24KMTA**, **ASHG07KPCA**, **ASHG09KPCA**
- Commercial AIRSTAGE units listed against AR-REW1E by aftermarket sellers: **ASTG18KMTC**, **ASTG22KMTC**, **ASTG24KMTC**, **ASTG30KMTC**, **ASTG34KMTC**, **ASTG30CMTA**, **ASTG34CMTA** (lower confidence — commercial VRF units sometimes use longer/different frames despite sharing a remote model name)

Caveat: e-commerce "universal remote" cross-reference listings often bundle many different Fujitsu protocol generations into one configurable remote product. Appearing on the same listing is a hint, not proof of an identical raw IR signal — treat the "Likely" tier as "worth testing," not "confirmed."

## Sources

- [IRremoteESP8266 src/ir_Fujitsu.h](https://github.com/crankyoldgit/IRremoteESP8266/blob/master/src/ir_Fujitsu.h)
- [IRremoteESP8266 src/ir_Fujitsu.cpp](https://github.com/crankyoldgit/IRremoteESP8266/blob/master/src/ir_Fujitsu.cpp)
- [Issue #1455 — Support for Fujitsu Remote AR-REW4E / ASYG09KETA-B](https://github.com/crankyoldgit/IRremoteESP8266/issues/1455)
- [Discussion #1701 — Strange IR signals from ASTG18K and ASTG09K](https://github.com/crankyoldgit/IRremoteESP8266/discussions/1701)
- [Amazon — AR-REW1E remote compatible with ASTG18KMTC etc.](https://www.amazon.com/Replacement-Conditioner-Compatible-ASTG30CMTA-ASTG34CMTA/dp/B0F1KR9R76)
- [Amazon — universal remote for ASHG07/09/12/14KMTB, ASHG18/24KMTA, ASHG07/09KPCA](https://www.amazon.com/ASHG07KMTB-ASHG09KMTB-ASHG12KMTB-ASHG14KMTB-Conditioner/dp/B0BS3R3KVV)
- [Fujitsu General Europe — ECO Series KPCE range](https://www.general-hvac.com/eu/products/split/wall/kpce/index.html)
