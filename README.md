# MTools for Flipper Zero

MTools is a Flipper Zero NFC application for checking magic tags, changing UIDs, and opening the compatible-card URL with NDEF emulation. The build uses the Flipper SDK through `ufbt`.

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

GDM/USCUID UID writes use the card's GDM or Gen1a magic wakeup. The writer builds
the public block 0, the hidden block needed for a seven-byte UID, and switches
the personalization byte only after the UID blocks are ready. It verifies each
write and the final UID. Cards with only the `0x80` encrypted magic-auth path
enabled cannot currently be written by this path; they report a wakeup error.

## Asset attribution

`images/NFC_manual_60x50.png` is the NFC manual illustration from [Flipper Zero firmware](https://github.com/flipperdevices/flipperzero-firmware/blob/dev/assets/icons/NFC/NFC_manual_60x50.png), which is licensed under GPL-3.0. Magic Check draws the original Flipper body and overlays a new card illustration at runtime.
