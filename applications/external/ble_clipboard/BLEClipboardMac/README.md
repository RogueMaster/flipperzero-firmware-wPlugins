# BLE Clipboard 0.5.4 for macOS

One native Mac app contains Quick Paste and the existing macro library. Its
companion is the independent **BLE Clipboard 0.5.4** Flipper app under
**Apps > Bluetooth**. Both apps must match; no third-party typing engine is used.

Build with `./build_app.sh --universal`; output is `dist/BLE Clipboard.app`.
Run `swift test` and `sh ../scripts/check_clipboard.sh` for unit, sanitizer and
cross-language wire/engine checks. The latter uses only fake USB, never hardware.

Create/edit/reorder macros, enter parameters, review expanded output, confirm
the intended target and explicitly Run. Quick Paste additionally loads the
clipboard or a UTF-8 text file. There are 28 layout choices; native Turkish
letters remain experimental. Unsupported Unicode is rejected.

The Flipper Keyboard toolbar panel and Quick Paste share persistent keyboard
defaults. With a matching companion, settings sync both ways while idle and
the Flipper's persisted choice wins on reconnect. This includes settings changed
on Flipper without the Mac. New macros inherit these defaults; existing macro
definitions keep their explicit target/layout. Use for Saved Runs controls the
device's local-run override without editing snapshots. An older companion can
still connect, but has no keyboard sync. No sync or settings request emits input.

Prior JSON schema 1 and existing IDs stay readable; mouse-containing documents
use schema 3; schema 2 mouse actions migrate to generic steps. Store-preview containers
and the direct build have separate library locations; use export/import to move
definitions deliberately. Neither library is silently overwritten.

The Mac compiles typed text, key, delay, Move Mouse and Repeat records and transfers a bounded,
CRC-verified program. It does not interpret bracket text as script commands.
The Flipper validates the entire program before a separate execution command.
Stop keeps Bluetooth connected. Normal jobs stop on BLE loss/Mac sleep/quit;
explicit standalone runs continue until complete, physical Stop or USB loss.
No request is automatically replayed. The mouse icon creates an eight-hour
Mouse Jiggle preset built from Repeat and Move Mouse, with editable duration,
interval and X/Y ranges, sampled on the Flipper. No undetectability guarantee.

Send to Flipper saves a reviewed, plaintext compiled snapshot without executing.
Clipboard/time/parameters are fixed when sent. Up to 20 snapshots persist on SD;
the device has browse, confirmed Run and confirmed Delete controls. Physical
runs are standalone and require USB already connected. Left/Back stops a busy
job; Back from the idle library exits. No firmware update or external engine.

`--ui-testing` disables Bluetooth and uses a temporary library. This flag is for
offline UI checks, not device validation. No app build installs the Flipper FAP,
changes pairing or sends keystrokes. See [setup](../README.md),
[macros](../docs/MACROS.md) and [protocol](../docs/PROTOCOL.md).
