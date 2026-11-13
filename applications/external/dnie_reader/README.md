# IDentity for Flipper Zero

*[Versión en español](README.es.md)*

A [Flipper Zero](https://flipperzero.one) app that reads **electronic identity documents** over NFC — the Spanish eID (DNIe), **ePassports** from any country and ICAO-compliant national ID cards — shows a little ID card with the **photo as pixel art**, and **checks that the document is genuine**.

> Educational RFID/NFC security project. Only use it with your own documents or with the holder's permission.

## Features

- **Secure access exactly like official readers**
  - **PACE** with the CAN (6 digits printed on the card) or with the MRZ data: ECDH Generic Mapping on brainpoolP256r1, AES-128.
  - **BAC** (3DES) for passports without PACE.
  - Automatic **ISO14443-4A / 4B** detection.
- **Holder data (DG1)**: TD1 (ID cards) and TD3 (passports) MRZ, with check digits.
- **Photo (DG2) as pixel art**
  - Custom minimal **JPEG2000** decoder (luma only, reduced resolution).
  - Minimal **JPEG** decoder at 1/8 scale (DC only, no IDCT) — bit-exact with libjpeg.
  - Atkinson, Floyd-Steinberg or Bayer dithering.
- **Authenticity checks**
  - DG1/DG2/DG14 hashes against EF.SOD (**Passive Authentication**).
  - SOD signature with the Document Signer certificate: RSA PKCS#1 v1.5 and **RSA-PSS**.
  - **CSCA chain**: the Document Signer certificate must be signed by the issuing country's CSCA. Keys for Spain and the 20 largest countries are bundled (from the German BSI CSCA Master List).
  - **Chip Authentication** (detects cloned chips).
  - Full-screen **"NOT AUTHENTIC"** warning when something does not match.
- **Sounds**: the issuing country's **national anthem** when the document is genuine, a low "wah-wah" when it is not. The music stops immediately when you leave the app.
- **Saved readings**, encrypted with the Flipper's unique enclave key. Two modes: full document, or *album* (name, photo, birthday and country only). Opening a saved reading plays its country's anthem.
- **English interface**, with an anthem volume setting (Off, 25-100 %) and a **Demo** mode with a fictitious sample ID (for screenshots, cannot be saved).

## Supported documents

| Document | Access | Status |
|---|---|---|
| 🇪🇸 Spanish eID (DNIe 3.0/4.0) | PACE + CAN | ✅ Tested: data, photo, hashes, signature, CSCA chain and Chip Authentication |
| Passports (any ICAO country) | PACE or BAC + MRZ | ✅ Tested with a 🇺🇸 US passport (data and JPEG photo) |
| ICAO national ID cards | PACE + CAN or MRZ | ⚠️ Should work, not tested yet |
| 🇩🇪 German ID card and others that require an authorised terminal | — | ❌ Not supported by design |

### CSCA chain by country

| Result | Countries |
|---|---|
| ✅ Verified (RSA / RSA-PSS) | 🇪🇸 Spain, 🇺🇸 USA, 🇫🇷 France, 🇮🇹 Italy, 🇨🇦 Canada, 🇯🇵 Japan, 🇰🇷 South Korea, 🇨🇳 China, 🇦🇺 Australia, 🇳🇱 Netherlands, 🇮🇩 Indonesia, 🇦🇷 Argentina, 🇲🇽 Mexico, 🇬🇧 UK (partly), 🇹🇷 Turkey (partly) |
| ⏳ ECDSA, not yet supported | 🇩🇪 Germany, 🇧🇷 Brazil, 🇷🇺 Russia, 🇮🇳 India, 🇸🇦 Saudi Arabia, 🇨🇭 Switzerland |
| ❔ Not bundled | Every other country: data and hashes are still checked |

For countries other than Spain, a signature that does not match any bundled key is reported as *"key not found"* rather than *fake*, because older keys may be missing from the list.

### Seals on the ID card

| Seal | Meaning |
|---|---|
| **GENUINE** | Hashes + signature + CSCA chain + Chip Authentication |
| **DATA OK** | Hashes + signature + CSCA chain; the chip does not support Chip Authentication |
| **SOD OK** | Hashes match, but the chain could not be checked |
| **TAMPER / SIG! / CSCA! / CA FAIL** | Something does not match: do not trust the document |

### Anthems

🇪🇸 Spain, 🇺🇸 USA, 🇫🇷 France, 🇬🇧 UK, 🇨🇦 Canada, 🇯🇵 Japan, 🇨🇳 China, 🇮🇩 Indonesia, 🇩🇪 Germany, 🇮🇹 Italy and 🇳🇱 Netherlands. Other countries get a short fanfare. All melodies are in the public domain; most were converted with LilyPond from the scores on Wikipedia (`tools/anthems/`).

## Install

1. Download the `.fap` from *Releases*, or build it yourself (see below).
2. Copy it to the Flipper's SD card under `apps/NFC/`.
3. On the Flipper: **Apps → NFC → IDentity**.

The first launch copies the CSCA keys (`csca.bin`, 36 KB) to the SD card.

## Build

You need Python 3 and [ufbt](https://github.com/flipperdevices/flipperzero-ufbt):

```sh
pip install ufbt
cd dnie_reader
ufbt          # builds dist/dnie_reader.fap
ufbt launch   # builds, installs and opens the app on a USB-connected Flipper
```

## Tests

`tests/` has PC tests, using official test vectors wherever they exist:

- **PACE**: ICAO Doc 9303-11, appendix G.1
- **BAC and 3DES Secure Messaging**: ICAO 9303-11, appendix D
- **CMAC**: RFC 4493
- **MRZ** TD1 and TD3 (ICAO specimen)
- **JPEG2000** decoder vs OpenJPEG; **JPEG** decoder vs libjpeg (bit-exact)
- **CSCA chain** with real, public certificates: 49 of the 68 current CSCA certificates of the 20 countries verify, 17 of them with RSA-PSS; no tampered certificate is accepted
- SOD, RSA signatures, Chip Authentication (simulated chip and clone) and saved records

See `tests/README.md`.

## Privacy

- All test data in this repository is **synthetic** or public (CSCA certificates).
- Saved readings are encrypted with the Flipper's unique key and cannot be opened on another device.
- The unencrypted photo is deleted from the SD card as soon as it has been turned into pixel art.
- Album mode never stores the document number, MRZ or expiry date.

## Code layout

| File | Contents |
|---|---|
| `dnie_pace.c`, `dnie_pace_math.c`, `dnie_ec.c` | PACE ECDH-GM, brainpoolP256r1 |
| `dnie_bac.c` | Basic Access Control |
| `dnie_sm.c`, `dnie_crypto.c` | AES/3DES Secure Messaging, KDF, CMAC, retail MAC |
| `dnie_card.c`, `dnie_lds.c`, `dnie_mrz.c` | ISO14443-4A/4B transport, file reading, MRZ parser |
| `dnie_j2k.c`, `dnie_jpeg.c`, `dnie_pixelart.c` | JPEG2000 and JPEG decoders, pixel art |
| `dnie_sod.c`, `dnie_pa.c`, `dnie_dg14.c`, `dnie_ca.c` | SOD, signatures, Chip Authentication |
| `dnie_csca.c`, `dnie_csca_keys.inc`, `assets/csca.bin` | CSCA chain (Spain built in, other countries on the SD card) |
| `dnie_sounds.c`, `dnie_anthems.inc` | Verdict, anthem player |
| `dnie_store.c`, `dnie_record.c` | Encrypted saved readings |
| `scenes/` | App screens |

## Roadmap

- ECDSA CSCA chains (Germany, Brazil, Russia, India, Saudi Arabia, Switzerland)
- Active Authentication (anti-cloning used by many passports instead of Chip Authentication)
- Driving licences (to be investigated)

## License

MIT (see `LICENSE`). `dnie_reader/lib/mbedtls_ct/` is code from [Mbed TLS](https://github.com/Mbed-TLS/mbedtls) under its own license (Apache-2.0 or GPL-2.0-or-later). CSCA certificates come from the [BSI CSCA Master List](https://www.bsi.bund.de/EN/Themen/Oeffentliche-Verwaltung/Elektronische-Identitaeten/Public-Key-Infrastrukturen/CSCA/csca.html).
