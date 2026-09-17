# Third-party notices

The Flipper application is AGPL-3.0-only. Its license does not replace the
following dependency licenses or upstream copyright notices.

## Inherited application

DFC is a modified fork of Eric Betts' and contributors'
[seos_compatible](https://github.com/bettse/seos_compatible).
The original license was added in this repository at commit `b8f4f1a`; the
complete AGPL v3 terms are preserved in the root [LICENSE](LICENSE).
Inherited application code remains subject to AGPL v3. DFC adds the
DESFire-compatible engine integration, credential storage, and Flipper UI
changes. See the Git history for authorship and modification dates.

We also acknowledge Eric Betts and contributors to
[Seader](https://github.com/bettse/seader), a related Flipper credential-reading
project. Seader's separate GPL v3 license is not this app's inherited AGPL
license; the direct fork ancestry is `seos_compatible`.

## Portable engine and crypto

- `lib/core`: [CinderSocket dfc-core](https://github.com/cindersocket/dfc-core),
  GPL-2.0-or-later. Preserve [its license](lib/core/LICENSE).
- `lib/tiny-AES-c`: kokke and Mistial Dev, Unlicense. Preserve
  [unlicense.txt](lib/tiny-AES-c/unlicense.txt) and source notices.
- `lib/tiny-DES-c`: Mistial Dev, Unlicense. Preserve
  [LICENSE](lib/tiny-DES-c/LICENSE) and source notices.
- The core host tests use Evan Nemerson's µnit under the MIT license, retained
  in [munit.h](lib/core/tests/munit/munit.h). µnit is not linked into the FAP.
  Crypto repositories also contain test-only material with separate notices.

The Git submodule entries pin the exact dependency revisions. Their host
examples, tests, and benchmark tools are not included in the application build.

## Flipper firmware, SDK, and assets

The application uses Flipper APIs through the SDK. The firmware's top-level
license is [GPL v3](https://github.com/flipperdevices/flipperzero-firmware/blob/dev/LICENSE);
individual components can carry different terms. Preserve the notices for the
specific SDK/firmware revision and any code copied or adapted from it.
No Flipper licensing exception is assumed for this application.

The `images/` dolphin and NFC artwork was inherited in the initial import
(`1b7cc79`) from the official Flipper Zero firmware repository. Each file is an
exact historical firmware blob:

- `DolphinMafia_115x62.png` and `DolphinNice_96x59.png` came from
  `assets/icons/iButton/`.
- `Nfc_10px.png` came from `assets/icons/Archive/`.
- `RFIDDolphinReceive_97x61.png` and `RFIDDolphinSend_97x61.png` came from
  `assets/icons/RFID/`.

The firmware repository licenses these assets under GPL v3. They retain their
upstream copyright and license; our AGPL v3 notice does not relicense Flipper
artwork, logos, or trademarks.

## Distribution boundary

The combined derivative application retains the inherited AGPL v3 requirements.
Select GPL v3 under the “or later” grant for the GPL-2.0-or-later core when
combining them with AGPL v3 material; preserve each component's own license.
Do not label the entire application GPL-2.0-only or claim that Flipper-derived
material has been relicensed. Include applicable license texts and notices,
and provide corresponding source, pinned dependencies, and build instructions
for distributed binaries.
