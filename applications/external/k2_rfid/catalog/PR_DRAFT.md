# Pull Request Draft: Submit CFS RFID to Flipper Application Catalog

**Target Repository**: `flipperdevices/flipper-application-catalog`  
**Base Branch**: `main`  
**Head Branch**: `mitchsurp:mitchsurp/k2_rfid_1.0`  
**PR Title**: `CFS RFID: Creality K2/K1/CFS RFID Spool Reader, Writer & Emulator (k2_rfid)`

---

## PR Body (conforming to catalog template)

```markdown
# Application Submission

CFS RFID allows users to read, decrypt, configure, write, and emulate RFID spool tags for the Creality Filament System (CFS) on the Creality K2 Plus, K1 Max, and CFS-compatible 3D printers.

Features:
- Scan & Decrypt: Reads official Creality CFS tags, decrypting the encrypted filament data in Sector 1.
- Spool Configuration: Select printer model (K2, K1, HI), all 66 official Creality filament materials, 20 color presets, custom spool weights (250g-1000g), and serial numbers.
- Write Blank Tags: Programs standard MIFARE Classic 1K tags with AES encryption, derived sector trailer keys, and printer metadata.
- Tag Emulation: Emulate any configured or scanned spool directly to the CFS reader bays in real time.
- Save & Load: Store and browse .nfc spool files on the SD card.
- Tag Reset: Wipe or format previously programmed tags back to default transport keys.
- Crypto Engine: Clean, standalone AES-128 implementation with zero external library dependencies.

# Extra Requirements 

MIFARE Classic 1K tags (13.56MHz) for writing spool tags. Two tags are needed per physical spool (one on each flange) for reliable detection by CFS reader bays.

# Author Checklist (Fill this out)

- [x] I've read the [contribution guidelines](../blob/HEAD/documentation/Contributing.md) and my PR follows them
- [x] I own the code I'm submitting or have code owner's permission to submit it
- [x] I have performed a self-review of my own code
- [x] I have commented my code, particularly in hard-to-understand areas
- [x] I [have validated](../blob/HEAD/documentation/Contributing.md#validating-manifest) the manifest file(s) with `python3 tools/bundle.py --nolint applications/CATEGORY/APPID/manifest.yml bundle.zip`

# AI usage disclosure (Fill this out)

Partially AI assisted - clarify below which parts were AI assisted and briefly explain what they do.

The application C codebase, scene navigation flow, and AES cryptography routines were ported from the upstream open-source DnG-Crafts/K2-RFID project with agentic coding assistance.

# Reviewer Checklist (Don't fill this out, and don't remove it from the template)

- [ ] Bundle is valid
- [ ] There are no obvious issues with the source code
- [ ] I've ran this application and verified its functionality
```

---

## Catalog Manifest Content (`applications/NFC/k2_rfid/manifest.yml`)

```yaml
sourcecode:
  type: git
  location:
    origin: https://github.com/mitchsurp/K2-RFID-Flipper.git
    commit_sha: 55f9ad0797d39689f2a0eb0e972007b2961bb9c6
short_description: Creality K2/K1/CFS RFID Spool Reader, Writer & Emulator
description: "@catalog/description.md"
changelog: "@catalog/changelog.md"
screenshots:
  - catalog/screenshots/screenshot_1.png
  - catalog/screenshots/screenshot_2.png
  - catalog/screenshots/screenshot_3.png
```
