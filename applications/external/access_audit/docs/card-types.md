# Card Type Reference

Every `CardType` enum value, its risk rating under the default ruleset, where the card is commonly deployed, and the recommended replacement or remediation.

Risk ratings assume `metadata_complete=true` and a UID is present. Incomplete metadata will add LOW RISK findings and reduce confidence.

---

## 125 kHz RFID

| Card Type | Risk | Common deployment | Recommendation |
|---|---|---|---|
| `CardTypeEm4100Like` | **HIGH RISK** | Legacy office/warehouse/parking | Replace with ISO14443 card using AES auth (DESFire EV2+) |
| `CardTypeHidProxLike` | **HIGH RISK** | HID ProxCard II 26-bit Wiegand, very widespread | Replace with iCLASS SE, Seos, or DESFire EV2+ |
| `CardTypeHidGeneric` | **HIGH RISK** | HID extended/non-standard 125 kHz formats | Replace with iCLASS SE, Seos, or DESFire EV2+ |
| `CardTypeIndala` | **HIGH RISK** | Legacy ASSA ABLOY / Motorola installs | Replace with modern card; Indala is clonable |
| `CardTypeRfid125` | **HIGH RISK** | Any unclassified 125 kHz token | Replace; all 125 kHz protocols lack cryptographic protection |

All 125 kHz RFID cards trigger `legacy_family` and `identifier_only_pattern`. There is no crypto and the UID alone is the credential.

---

## MIFARE Classic family

| Card Type | Risk | Common deployment | Recommendation |
|---|---|---|---|
| `CardTypeMifareClassic` | **HIGH RISK** | Generic fallback, exact size unknown | Treat as Classic 1K; replace with DESFire EV2+ or Plus SL3 |
| `CardTypeMifareClassic1K` | **HIGH RISK** | Mass market access control, hotel keys, transit | Replace with DESFire EV2/EV3 or MIFARE Plus SL3 |
| `CardTypeMifareClassic4K` | **HIGH RISK** | High-capacity Classic deployment | Replace with DESFire EV2/EV3 |
| `CardTypeMifareClassicMini` | **HIGH RISK** | Rarely deployed; same Crypto1 weakness | Replace; Crypto1 is broken regardless of size |

Crypto1 has been publicly broken since 2008 (CRYPTO1 attacks, Darkside, Nested). Any MIFARE Classic deployment should be treated as compromised.

When a Classic card is scanned, the app actively attempts to authenticate sector 0 using common default keys (`FFFFFFFFFFFF` and `A0A1A2A3A4A5`). If either succeeds, the `default_keys` rule fires and the report includes an explicit note. A card with default keys scores 85/100 instead of the standard 70/100.

---

## MIFARE Ultralight / NTAG family

| Card Type | Risk | Common deployment | Recommendation |
|---|---|---|---|
| `CardTypeMifareUltralight` | **HIGH RISK** | Events, loyalty, low-security | Avoid for access control; no mutual authentication |
| `CardTypeMifareUltralightC` | MODERATE | Slightly higher security than Ultralight; 3DES OTP | Avoid for access control; use DESFire EV2+ |
| `CardTypeNtag203` | **HIGH RISK** | Discontinued; legacy NFC tag | Replace |
| `CardTypeNtag213` | **HIGH RISK** | Widespread NFC tag, smart posters | Avoid for access control; no mutual auth |
| `CardTypeNtag215` | **HIGH RISK** | Nintendo Amiibo, NFC tags | Avoid for access control |
| `CardTypeNtag216` | **HIGH RISK** | Large NFC tag storage | Avoid for access control |
| `CardTypeNtagI2C` | **HIGH RISK** | IoT dual-interface tag | Not designed for access control |

None of the Ultralight/NTAG variants offer mutual authentication. Using them for access control relies on UID alone.

---

## MIFARE DESFire family

| Card Type | Risk | Crypto | Recommendation |
|---|---|---|---|
| `CardTypeMifareDesfire` | SECURE | Unknown variant | Verify key diversification and mutual auth |
| `CardTypeMifareDesfireEV1` | SECURE | DES / 3DES | Upgrade to EV2/EV3; AES not available on EV1 |
| `CardTypeMifareDesfireEV2` | SECURE | AES-128 | Verify key diversification; consider EV3 for latest security |
| `CardTypeMifareDesfireEV3` | SECURE | AES-128 + enhanced | Verify key diversification and mutual auth configuration |
| `CardTypeMifareDesfireLight` | SECURE | AES-128 | Verify key diversification; Light has reduced feature set |

DESFire scores SECURE but the report advice flags EV1's older DES/3DES crypto. All DESFire variants can be misconfigured; the card being present does not mean the application is properly locked.

---

## MIFARE Plus family

| Card Type | Risk | Security level | Recommendation |
|---|---|---|---|
| `CardTypeMifarePlus` | SECURE | Unknown SL | Determine security level in use |
| `CardTypeMifarePlusSL1` | **HIGH RISK** | Classic-compat mode (no AES) | Upgrade to SL3 or replace with DESFire EV2+ |
| `CardTypeMifarePlusSL2` | MODERATE | AES crypto, Classic frames | Upgrade to SL3 for full ISO14443-4 protocol |
| `CardTypeMifarePlusSL3` | SECURE | AES crypto + ISO14443-4 | Verify key diversification and mutual auth |

SL1 is particularly dangerous: the card physically supports AES but the system has not migrated, so Crypto1 attacks still apply.

---

## ISO standards (generic fallbacks)

| Card Type | Risk | Notes |
|---|---|---|
| `CardTypeIso14443A` | MODERATE | Unclassified ISO14443-A card; deeper classification unavailable |
| `CardTypeIso14443B` | MODERATE | Unclassified ISO14443-B card |
| `CardTypeIso15693` | MODERATE | Unclassified ISO15693 card |

These types appear when the poller cannot determine a more specific sub-type. Treat as MODERATE until the actual card type is confirmed.

---

## HID iCLASS family

| Card Type | Risk | Crypto | Recommendation |
|---|---|---|---|
| `CardTypeHidIclass` | MODERATE | Unconfirmed (ISO15693 card detected, iCLASS not yet verified) | Run iCLASS scan mode to confirm |
| `CardTypeHidIclassLegacy` | **HIGH RISK** | DES/3DES, memory size unknown | Upgrade to iCLASS SE or Seos |
| `CardTypeHidIclassLegacy2k` | **HIGH RISK** | DES/3DES, 2 kilobit (standard) | Upgrade to iCLASS SE or Seos |
| `CardTypeHidIclassLegacy16k` | **HIGH RISK** | DES/3DES, 16 kilobit | Upgrade to iCLASS SE or Seos |
| `CardTypeHidIclassLegacy32k` | **HIGH RISK** | DES/3DES, 32 kilobit | Upgrade to iCLASS SE or Seos |

The iCLASS standard DES/3DES master key was publicly disclosed. Attacks against iCLASS Legacy are well-documented and practical. HID's own recommendation is to migrate to iCLASS SE (AES) or Seos.

---

## FeliCa family

| Card Type | Risk | Notes |
|---|---|---|
| `CardTypeFelica` | SECURE | FeliCa Standard, proprietary crypto; verify application configuration |
| `CardTypeFeliCaLite` | **HIGH RISK** | FeliCa Lite, no mutual authentication; UID-only credential, trivially clonable. Avoid for access control; use standard FeliCa or DESFire EV2+ |

FeliCa Lite is common in transit and low-security building systems. It is physically the same form factor as standard FeliCa but lacks the cryptographic authentication layer.

---

## Other NFC protocols

| Card Type | Risk | Notes |
|---|---|---|
| `CardTypeSlix` | MODERATE | NXP SLIX (ISO15693 variant); typically used for asset tracking, not access |
| `CardTypeSt25tb` | MODERATE | STMicroelectronics ISO14443-B; verify application crypto |

---

## Implementation reference

- Enum definition: [core/observation.h](../core/observation.h)
- String labels: [core/scoring.c](../core/scoring.c) (`card_type_to_string`)
- Report advice: [core/report.c](../core/report.c) (`report_advice`)
- Rule matching: [core/rules.c](../core/rules.c)
