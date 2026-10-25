# MTools for Flipper Zero

MTools is a Flipper Zero NFC application for checking magic tags, changing UIDs, and opening the compatible-card URL with NDEF emulation. The build uses the Flipper SDK through `ufbt`.

The app uses Flipper Zero's built-in NFC hardware and needs no expansion module.

## Publishing to the Flipper Apps Catalog

The `screenshots/` directory contains unedited qFlipper screenshots for the Catalog.

To submit version 1.0 or a later version, update `fap_version` in `application.fam`,
add a matching `vmajor.minor:` section to `CHANGELOG.md`, and push a commit whose
subject is exactly `release: major.minor` to `main`. The release workflow builds
with the release SDK, validates the Catalog bundle, and opens a PR from the
MTools Tec Catalog fork. Other commits do not publish. The repository must have
the `FLIPPER_CATALOG_TOKEN` Actions secret with access to the Catalog fork.

## License

MTools is distributed under the [GNU General Public License v3.0](LICENSE).

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

## MFC Gen2 / CUID detection

GDM/USCUID cards may also support Gen1a wakeup. Magic Check checks both
capabilities and displays `GDM/USCUID + GEN1A` on one line when both respond.

Magic Check follows Proxmark3's MifareCIdent approach: check known four-byte
and seven-byte UID ATS fingerprints, even when SAK does not advertise
ISO14443-4. If neither matches, authenticate block 0 with default Key B
`FFFFFFFFFFFF` and check the encrypted `A0 00` write-command ACK. The probe
sends no block data and ends the RF session immediately. Authentication failure
or a missing ACK leaves the card unconfirmed; cards with a changed Key B may
therefore remain undetected.

## Asset attribution

`images/NFC_manual_60x50.png` is the NFC manual illustration from [Flipper Zero firmware](https://github.com/flipperdevices/flipperzero-firmware/blob/dev/assets/icons/NFC/NFC_manual_60x50.png), which is licensed under GPL-3.0. Magic Check draws the original Flipper body and overlays a new card illustration at runtime.

## DirectWrite MFU / NTAG UID changes

UID Changer supports seven-byte UIDs on compatible DirectWrite Type 2 magic
cards. Choose MFU / NTAG in manual entry, read a Type 2 source card, or load a
saved Ultralight/NTAG NFC file. This selects a write method; ordinary locked UID tags are not supported.
Magic Check identifies supported Gen2 variants using the Proxmark3 ATS
fingerprint, followed by a compatibility-WRITE stage-1 ACK probe if needed.
The probe sends no page data and ends its RF session before further commands.
Unmatched cards remain unconfirmed; a match does not guarantee that a locked
or password-protected card can be written.

The writer reads pages 0–3, recalculates both BCC bytes, and writes pages 0, 1,
and 2 within one active ISO14443-A poller callback. It keeps the RF field active
and does not reselect the card between writes. Page 2's internal and static
lock bytes are preserved. It verifies pages 0–2 before stopping the poller,
then starts a fresh RF session to check the anticollision UID. If a later page
write fails, it attempts to restore the original UID pages in the same session.
An interrupted RF field or unsuccessful restoration can still require UID/BCC
recovery with a separate recovery tool.

## ISO15693 Gen1 variants

Magic Check does not identify Gen1 from IC reference, UID manufacturer, or
28/56-block capacity alone. It checks the UID at fixed blocks `0x38`/`0x39`
using unaddressed reads, only when these addresses are outside the advertised
user memory. Matching hidden UID bytes are a fingerprint, not a guarantee that
the UID can be changed. If no fingerprint matches, tags with 28 or 56
four-byte blocks show `Possible ISO15 GEN1`, regardless of IC reference.
This is a layout hint and does not confirm write capability; other unmatched
layouts remain unconfirmed. UID Changer can use the manually selected
ISO15693 Gen1 method on a known compatible tag.

The Gen1 writer sends the optional unlock writes to `0x3E`/`0x3F`, then writes
the UID in little-endian order to `0x38`/`0x39`. Some variants return `01 10`
(block unavailable) for both unlock writes; the writer continues for that
specific response. Other explicit errors, including `01 10` on a UID block,
fail the write. Unlock timeouts are tolerated, but both UID block writes
require explicit success responses. Success also requires the inventory UID
to match after restarting the RF session. The same fixed addresses and UID
byte order apply to both 28-block and 56-block variants.

Writing the original UID back can test command compatibility, but unchanged
inventory alone cannot prove the tag stored the bytes or can change its UID.
This is an explicit write through UID Changer, not an automatic Magic Check
probe. A full UID-change test requires a different UID and restoration of the
original; the app does not perform that test automatically.
