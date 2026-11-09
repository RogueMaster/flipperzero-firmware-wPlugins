# BLE Clipboard

The Flipper Zero companion for the BLE Clipboard Mac and Windows apps. Receive
reviewed clipboard text and macros over authenticated Bluetooth LE, then send
keyboard and mouse input over USB to a device you own or have permission to use.
No Ducker dependency or target-side agent is required.

Version 0.5.4 adds persistent standalone keyboard settings and matching Mac and
full Windows companions. Windows is an unsigned x64 beta.

## Features

- Clipboard text, literal text, parameters, shortcuts and delays.
- 28 target keyboard layouts, with built-in US and macOS Turkish Q mappings.
- Experimental native Turkish letters on Turkish Q; other native scripts and
  arbitrary Unicode are not supported.
- Move Mouse with fixed or ranged offsets, and timed Repeat blocks up to 24 hours.
- Explicit standalone jobs with elapsed/remaining timer and physical Stop.
- Save up to 20 compiled clipboard or macro snapshots on SD for confirmed local Run.
- Info and Downloads screens with a matching-release QR code.
- An idle Left > Keyboard menu with persistent layout/OS preferences and Mac sync.

## Requirements

- Flipper Zero with official firmware 1.5.1 or a compatible API.
- A matching Mac companion (macOS 14+) or Windows x64 beta (Windows 10 2004+/11)
  to create and send jobs. The USB target does not need a companion.
- A USB cable to the intended keyboard/mouse target.
- An SD card for saved jobs, programs larger than 4 KiB, and non-built-in layout assets.
  Layout files come from the official firmware resources.

[Download the matching desktop app and release files](https://github.com/otcan/ble-clipboard/releases/tag/v0.5.4).

## Use

Open BLE Clipboard on Flipper, then connect from the desktop app. On Windows,
pair Paste in Bluetooth settings first. Compare and approve
the pairing codes. Choose the target OS/layout, review the expanded output and
confirm target focus before Run. Save to Flipper stores a fixed snapshot without
executing it. On Flipper, Up/Down selects a saved entry and OK opens it; Run needs
a second confirmation. Right from the idle list opens Info, then OK opens Downloads.

Left from the idle list opens Keyboard. Up/Down selects Target, Layout, Saved
runs or Save keyboard; OK changes/opens a choice, and Save keyboard commits it.
Left/Back cancels an unsaved edit. Selecting a layout enables Saved runs: Device;
choose Original instead to keep each snapshot's own layout/OS. The preference
survives BLE loss and app restart. Local Run shows its effective keyboard and
validates all text against it; it never falls back to English on a missing map.
Snapshots are not rewritten. Compiled shortcut modifiers are not recompiled for
a different OS. A keyboard change while Run confirmation is open requires review
again. The Mac reads the device setting on reconnect; changes sync only while idle.

Left or Back stops an active job. Normal jobs stop on Bluetooth loss. Explicit
standalone jobs continue without the Mac, but USB loss or exiting the app stops
them permanently. Nothing runs automatically after restart or reconnection.

## Safety and Privacy

Use a disposable text document for initial checks. Newlines, tabs and shortcuts
are real input. Stop cannot undo input that already reached the target. Mouse
movement is observable and is not guaranteed to prevent an idle policy or be
undetectable. Saved snapshots are plaintext on SD; do not save secrets.

The 0.5.1 button-release guard is retained. Automated tests are not a substitute
for physical layout, focus, drag-lock or multi-hour checks on your target.

[Source and setup guide](https://github.com/otcan/ble-clipboard/tree/v0.5.4)

GPL-3.0-only. Not affiliated with Flipper Devices.
