# MTools for Flipper Zero

MTools is a Flipper Zero NFC application for checking magic tags, changing UIDs, and opening the compatible-card URL with NDEF emulation. The build uses the Flipper SDK through `ufbt`.

The app uses Flipper Zero's built-in NFC hardware and needs no expansion module.

## Publishing to the Flipper Apps Catalog

The official Catalog requires an unedited screenshot exported directly from qFlipper.
Place it at `screenshots/mtools-home.png` before the first release.

To submit version 1.0 or a later version, update `fap_version` in `application.fam`,
add a matching `vmajor.minor:` section to `CHANGELOG.md`, and push a commit whose
subject is exactly `release: major.minor` to `main`. The release workflow builds
with the release SDK, validates the Catalog bundle, and opens a PR from the
MTools Tec Catalog fork. Other commits do not publish. The repository must have
the `FLIPPER_CATALOG_TOKEN` Actions secret with access to the Catalog fork.

## Source layout

| Path | Responsibility |
| --- | --- |
| `application.fam` | FAP metadata and explicit source list |
| `mtools_app.c`, `mtools_app.h` | App allocation, scene routing, shared handles and state |
| `features/magic_check.c` | Magic Check scan, probe and result timing |
| `features/uid_changer.c` | UID Changer steps, input and read/write orchestration |
| `features/about_ndef.c` | About page URL emulation |
| `ui/mtools_ui.c` | Home, About and Magic Check drawing |
| `ui/uid_changer_ui.c` | UID Changer drawing and generation labels |
| `ui/nfc_scan_art.c` | Shared Flipper, arrow, fob and card drawing |
| `nfc/card_info.c` | Shared scanner protocol classification and ISO15 chip names |
| `nfc/card_reader.c` | Shared scanner/poller lifecycle and scan LED handling |
| `nfc/magic_detector.c` | Read-only generation probes |
| `nfc/magic_writer.c` | Generation-specific writes and readback verification |
| `nfc/magic_tag.h` | Generation enum and detector/writer API |

## Adding a tool

1. Put its state machine and NFC orchestration in `features/<tool>.c/.h`. Keep drawing in `ui/<tool>_ui.c/.h`.
2. Add its scene route in `mtools_app.c` and its source files in `application.fam`.
3. Use `nfc/card_info.h` for scanner protocol classification and `nfc/card_reader.h` for scanner start and scanner/poller stop. Send a custom event to the view dispatcher for scene transitions; a probe that needs the active poller may run in its ready callback.
4. Put reusable card commands in `nfc/`. Keep generation-specific command sequences in `magic_detector.c` or `magic_writer.c`, rather than in UI code.
5. If a detection cannot be proven without changing a card, return an unconfirmed result. Writes must verify the result by reading back from the card.

Run `ufbt` to build. With a connected Flipper, run `ufbt launch FLIP_PORT=<serial-port>` to install and start the FAP.

GDM/USCUID UID writes first try the card's GDM or Gen1a magic wakeup. If both
are disabled, the writer tries the card's `0x80` Crypto1 magic authentication
with the default zero key, temporarily enables its configured wakeup, and then
writes the UID. It builds the public block 0 and the hidden block needed for a
seven-byte UID, switches the personalization byte only after the UID blocks are
ready, and restores the original wakeup setting. Each block and the final UID
are verified. A failed write after the temporary configuration is enabled may
leave `7A FF` enabled so the card can be recovered through its magic wakeup.

## Asset attribution

`images/NFC_manual_60x50.png` is the NFC manual illustration from [Flipper Zero firmware](https://github.com/flipperdevices/flipperzero-firmware/blob/dev/assets/icons/NFC/NFC_manual_60x50.png), which is licensed under GPL-3.0. Magic Check draws the original Flipper body and overlays a new card illustration at runtime.
