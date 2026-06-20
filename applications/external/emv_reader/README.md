# emv-flipper — EMV contactless card reader for Flipper Zero

A `.fap` application that reads EMV (chip-card) contactless data from cards you own and decodes it into a paginated, human-readable display. Built for **Momentum firmware** on Flipper Zero hardware.

It does the full standard EMV contactless conversation — `SELECT PPSE → SELECT AID → GET PROCESSING OPTIONS → READ RECORD` — with proper PDOL handling, walks the BER-TLV tree returned by the card, and surfaces the security-relevant fields most readers don't bother decoding.

> **Read-only and educational.** This app reads cards you own, parses the standard EMV TLV fields, and displays them. It does **not** emulate cards, does not produce cryptograms, and is technically incapable of charging anything to your account.

---

## What it shows

After tapping a card, the result splits across **5 pages** (LEFT/RIGHT to navigate):

| Page | Fields |
|---|---|
| **1 — Summary** | Card type label, PAN, expiry, cardholder name |
| **2 — Details** | Service code (ISO 7813 decoded), AIP (Application Interchange Profile) flags, ATC (Application Transaction Counter), PIN-required analysis |
| **3 — CVM list** | Cardholder Verification Method rules (if card publishes them) — methods, conditions, amounts |
| **4 — AIDs** | All Application Identifiers found in PPSE (multi-AID support — US debit cards often expose 2+) |
| **5 — Hex dump** | Full raw record bytes, scrollable, suitable for byte-level analysis |

### The PIN analyzer

The Details page automatically interprets the card's CVM rules + AIP CVM bit and gives you a one-line answer:

- `PIN always req by card` — every transaction requires PIN (rare for contactless)
- `PIN card req >$50.00` — card requests PIN above $50
- `PIN card never asks` — card has no PIN policy; terminal/issuer decides at transaction time
- `PIN ?` — CVM rules present but conditions unrecognized

For cards that defer everything online (typical of modern Visa qVSDC), it adds a `(terminal limit applies)` clarifier — the card itself imposes no offline threshold; PIN prompting is determined by the terminal's configured CVM-required limit (US contactless: typically $50–$100).

### Decoded fields cheat sheet

| Display | Meaning |
|---|---|
| `SC 201 IntlIC/Norm/Free` | Service code, decoded by digit. Digit 1 = interchange (Intl, IC chip), digit 2 = auth processing (Normal), digit 3 = interchange rules (Free / no PIN restriction) |
| `AIP 1980 CVM,TRM,CDA,MAG` | Application Interchange Profile bytes + decoded flags (CVM supported, Terminal Risk Management, Combined DDA, Magstripe-only) |
| `ATC 7` | Application Transaction Counter — number of transactions the card has processed |
| `PIN ...` | PIN requirement analysis from CVM list + AIP CVM bit |

---

## Saved dumps

Each successful read writes a text dump to the Flipper SD card at:

```
/ext/apps_data/emv_reader/<unix_timestamp>_<last4>.txt
```

Includes parsed fields, decoded service code / AIP / CVM list, all AIDs found, and the full hex of the records read. Suitable for offline analysis or evidence in an authorized engagement.

---

## Hardware & firmware requirements

- **Flipper Zero** (any hardware revision)
- **Momentum firmware** (release or dev channel) — tested against Momentum SDK API 87.1
- **USB cable** for installation (or microSD card + qFlipper)

The app uses the modern OFW NFC stack (`Iso14443_4aPoller`), which Momentum tracks closely. It should work on official firmware too with no source changes; you'd just point uFBT at the OFW SDK index instead of Momentum's.

---

## Installation — easy path (prebuilt .fap)

1. Download [`releases/emv_reader.fap`](releases/emv_reader.fap) from this repo.
2. Connect Flipper Zero via USB.
3. Open **qFlipper** → File Manager → `SD Card/apps/NFC/`
4. Drag `emv_reader.fap` into that folder.
5. On the Flipper: `Apps → NFC → EMV Reader`

Or with `ufbt` already installed and Flipper plugged in:

```powershell
ufbt launch <path-to>\emv_reader.fap
```

---

## Installation — building from source

### One-time tooling setup

```powershell
# Install uFBT (the user-mode Flipper Build Tool) via pip
python -m pip install --upgrade ufbt

# Point uFBT at Momentum's SDK index (one-time per workspace)
cd <repo-root>\emv_reader
ufbt update --index-url=https://up.momentum-fw.dev/firmware/directory.json
```

If `ufbt` is missing from PATH after install, your Python user-site `Scripts` directory isn't on PATH — add it (`%APPDATA%\Python\Python3xx\Scripts`) or invoke as `python -m ufbt ...`.

For **official firmware** instead of Momentum, omit `--index-url`. uFBT will pull from Flipper's release SDK by default.

### Build

The repo includes `build.ps1` which handles the source shared library sync:

```powershell
# From the repo root:
.\build.ps1            # build only
.\build.ps1 -Flash     # build + push to Flipper + launch
.\build.ps1 -Clean     # full rebuild
.\build.ps1 -NoSync    # skip the _shared\ → emv_reader\ sync (advanced)
```

After a successful build, the .fap appears in:
- `emv_reader\dist\emv_reader.fap` — the build output
- `dist\emv_reader.fap` — staged copy
- `releases\emv_reader.fap` — staged copy (matches the prebuilt download)

### Direct uFBT invocation

If you don't want to use `build.ps1`, you can build manually — but you must first copy the shared library into the app directory:

```powershell
# Sync shared lib (once, or after edits)
copy _shared\emv_lib\*.* emv_reader\

# Then build from inside the app dir
cd emv_reader
ufbt              # build .fap → dist\emv_reader.fap
ufbt launch       # build + flash + run
```

---

## Repository layout

```
emv-flipper/
├── README.md               # this file
├── LICENSE                 # MIT
├── .gitignore
├── build.ps1               # sync + build + flash wrapper
│
├── _shared/
│   └── emv_lib/            # source shared library (~300 LoC)
│       ├── ber_tlv.{c,h}   # BER-TLV walker, multi-byte tags, recursion
│       └── emv_apdu.{c,h}  # AID table, APDU builders, decoders, analyzers
│
├── emv_reader/             # the Flipper Zero app
│   ├── application.fam     # uFBT manifest
│   ├── emv_reader_main.c   # entry, GUI, NFC poller, save logic (~700 LoC)
│   ├── ber_tlv.{c,h}       # synced from _shared/ at build time
│   └── emv_apdu.{c,h}      # synced from _shared/ at build time
│
├── dist/
│   └── emv_reader.fap      # prebuilt — drag-and-drop install
│
└── releases/
    └── emv_reader.fap      # prebuilt — same as dist/, suitable for GitHub Releases
```

### Why `_shared/`?

The `_shared/emv_lib/` folder is the **source of truth** for the BER-TLV parser and EMV APDU helpers. The build script copies them into `emv_reader/` before each build. This is uFBT-idiomatic: each app's `application.fam` glob (`sources=["*.c"]`) only sees files inside the app dir, so cross-app sharing is done at the filesystem level rather than via include paths.

If you fork and add more apps later (emulator, fuzzer, etc.), they can pull from the same `_shared/emv_lib/` and the build script handles per-app file manifests.

---

## Architecture

The reader's flow:

```
1. nfc_poller_alloc(NfcProtocolIso14443_4a)
2. On Iso14443_4aPollerEventTypeReady (card activated):
   3. SELECT PPSE (2PAY.SYS.DDF01)
   4. Walk PPSE response → collect all AIDs (tag 4F at any depth)
   5. SELECT first AID
   6. Parse FCI for PDOL request (tag 9F38)
   7. Build PDOL data: TTQ, amount, currency, country, date, UN, etc.
   8. GET PROCESSING OPTIONS with PDOL data
   9. Parse GPO response (template 1 or 2):
       - AIP (tag 82)
       - AFL (tag 94)
       - Inline records (Visa qVSDC fast read)
   10. If AFL present, READ RECORD loop
   11. Walk accumulated TLVs:
       - PAN (5A), expiry (5F24), holder (5F20), track2 (57)
       - ATC (9F36), CVM list (8E)
       - Service code (extracted from track2)
   12. Run PIN analyzer (AIP CVM bit + CVM rules → status + threshold)
   13. Save dump to SD
```

Designed to handle both EMV response formats:
- **Template 1** (`80 LL AIP[2] AFL[N]`) — older / simpler cards
- **Template 2** (`77 LL <TLV...>`) — Mastercard PayPass / M/Chip 4

And the Visa **qVSDC inline-data variant** where AFL is empty and all the fields live in the GPO body itself.

---

## Use cases

### Authorized red-team / pentest engagements
- Audit terminal CVM enforcement: read your test cards, compare to terminal's actual CVM behavior.
- Validate PCI-DSS compliance: verify terminals don't downgrade to magstripe inappropriately.
- Demonstrate iCVV vs CVV1 distinction to clients (chip dump can't be cloned to magstripe).

### Education / research
- Understand the EMV contactless protocol byte-by-byte.
- See what your own cards actually expose (versus what marketing says).
- Compare AIP / CVM behavior across issuers and card products.

### Personal cards only
- Read your own card to see PAN/expiry/holder for record-keeping.
- Verify a new card's chip is functioning.

---

## What this app does NOT do

- ❌ Does not emulate cards.
- ❌ Does not generate `GENERATE AC` cryptograms (would require issuer keys, which are HSM-protected and unavailable).
- ❌ Does not write to cards or modify card data.
- ❌ Does not capture the printed CVV2 (3-digit on the back) — that value is **never** stored on the chip or magstripe by design.
- ❌ Does not extract anything from the chip that could be used to charge a transaction (the chip's authentication keys never leave the secure element, even to legitimate cardholders).

For why an emulator built from this data couldn't fool a real terminal, see [Why CVV2 isn't readable](#why-cvv2-isnt-readable) below.

---

## Troubleshooting

### `ufbt launch` fails with "has to be closed manually"

The previous version of EMV Reader is still running on the Flipper. Press **BACK** on the Flipper to close it, then re-run. The newest `.fap` is already on the SD card; it just needs the running instance to release. (`build.ps1` treats this as a non-fatal warning when the upload step succeeds.)

### COM port "Access is denied"

Another tool is holding the Flipper's serial port. Common culprits: qFlipper, lab.flipper.net browser tab, another `ufbt` invocation, a serial-monitor app. Close them and retry.

### "Failed to find connected Flipper"

Check USB cable and Flipper power. The Flipper should be on the home screen or in the Apps menu (not in DFU/recovery mode).

### Build fails with `error: implicit declaration of function 'snprintf'`

Means `<stdio.h>` is missing from one of the source files. The shared lib already includes it; if you're modifying source and adding new format-string usage, make sure the file has `#include <stdio.h>` near the top.

### `No AID in PPSE`

Card returned no Application Identifiers in PPSE select. Most modern contactless cards support PPSE; this can fire on:
- Very old EMV cards that only support PSE (`1PAY.SYS.DDF01`) for contact use
- Non-EMV cards (e.g., transit cards, MIFARE, NDEF tags)
- Card not properly tapped (re-tap, hold steady)

### `GPO SW=6985 pdol=N`

Card is rejecting our GPO with "Conditions of use not satisfied." Usually means the card needs more PDOL fields filled. The default PDOL defaults cover TTQ, amount, currency, country, date, UN, terminal type, terminal capabilities, app version — most cards accept this, but some issuer customizations may need additional tags. Open an issue with the card type and SW.

### Hex dump page is blank

Records buffer is empty — usually means GPO returned no data and AFL was empty (corner case). Try a different card to confirm the app works.

---

## Why CVV2 isn't readable

People sometimes ask: "why can the reader see PAN but not the CVV?" Quick answer:

There are **four different "CVV" values**, each with different storage:

| Name | Where it lives | What it's for |
|---|---|---|
| **CVV2** (3 digits printed on back) | Printed only — not in chip, not in magstripe | Card-not-present / online verification |
| **CVV1** | Magstripe track 2 discretionary data | Card-present mag swipe verification |
| **iCVV** | Chip's track2-equivalent (tag 57) | Same role as CVV1 but for chip — **deliberately different value** so chip dumps can't be encoded onto magstripe |
| **dCVV** / cryptogram | Generated per-tap by the chip in response to GENERATE AC, signed with the issuer's master key | Contactless tap verification — one-shot, can't be replayed |

The reader **can** see iCVV (it's in the hex dump, inside the track2-equivalent discretionary data — different per scheme). The reader **cannot** see CVV2 (it's not stored anywhere on the card) and **cannot** generate dCVV (would require the issuer's master key, which lives in HSMs that even bank staff can't extract from).

This is the design — by isolating each CVV in a different location with a different value, the network ensures that data captured from any one channel can't be reused on another channel. It's why card-skimming kits target physical contact pads or magstripes, not chips.

---

## Authorized use

Read your **own** cards. The data on a card belongs to the issuer, but reading the data from a card in your possession that you have legitimate access to is generally fine in the US for personal/educational use.

For consulting work in payment-security space:
- Get the engagement scoped in writing (SOW) — what cards, what terminals, what tests.
- Reading-only of your own cards (or cards specifically in scope under the SOW) is generally outside the unauthorized-access-device statutes that apply in most jurisdictions, but laws vary — check your state/country before any engagement.
- Emulation, replay, or transmitting captured card data is a different legal posture than reading. This app does none of those.

Don't tap cards you don't own without explicit authorization. Don't capture contactless emissions from cards in someone else's pocket — the chip's static data is technically broadcast on a tap, but reasonable-expectation-of-privacy and the golden-rule ethic still apply.

---

## Acknowledgements

- **Flipper Devices** — Flipper Zero hardware and OFW NFC stack
- **Momentum Firmware** — community fork with extended app support and uFBT-friendly SDK
- **EMVCo** — public EMV book 1–4 specifications
- **uFBT** — `flipperdevices/flipperzero-ufbt` user-mode build tool
- 
---

## License

MIT — see [LICENSE](LICENSE).

<b>Final Source: AmsaOne @ https://github.com/AmsaOne </b>
<br>
It is never expected but if you feel the need and my guides/wiring diagrams are useful to you, Donations are always happily accepted 
<br>
<b>BTC:</b> 12RF 3PWZ RFYJ CTDV Y9NA ALTP POUC FGPDWK
<br>
<b>ETH:</b> 0XE7 92F4 B5B5 30 D4A9 DOBE EB9E 09 ABB2 5A3O C977 1B
<br>
<b>DOGE:</b> D6X1 DRJL OBF9 9VZA Z310 R2V0 74GC OK1B 6C </p>

