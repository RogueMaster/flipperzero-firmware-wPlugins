# Windows Companion (0.5.4 Beta)

Python 3.12, Tkinter and Bleak; Windows 10 2004+ / Windows 11, x64. The unsigned
beta is published only as a matching Mac, Flipper and Windows release set after
the release gates pass. ARM64 and Linux sender packages are not included.

The Windows companion edits the same JSON macro libraries as the Mac: searchable
library, creation/duplication/deletion, parameters, templates, key combinations,
delays, Move Mouse, one-level Repeat, step reordering, import/export and backup
recovery. Quick Paste is separate from the macro editor. Every run/save has an
expanded-output review and a fresh explicit confirmation. No direct-send hotkey,
global hook, automatic clipboard watcher or automatic replay is installed.

## Setup

Use the installer or extract the **whole** portable ZIP. Both bundle Python and
runtime dependencies. Installation is per-user, requires no administrator rights,
does not change Bluetooth settings, and does not launch the application or any
output. Upgrades/uninstall preserve `%APPDATA%\BLE Clipboard\macros.json`, its
`.backup`, and `preferences.json`.

Install the matching FAP separately with qFlipper. Open BLE Clipboard on Flipper,
pair **Paste** in Windows Bluetooth settings, and approve matching codes on both
devices. Refresh the companion's device list and select the intended Flipper.
The USB target may run macOS, Windows or Linux; it does not need this companion.
Match its active keyboard layout before confirming Run. Global shortcut handlers
can intercept HID keys; focus restoration is not guaranteed.

Flipper's persisted keyboard wins on reconnect. The Keyboard dialog changes its
local-run keyboard explicitly; existing desktop macro layouts and SD snapshot
files stay unchanged. Save on Flipper uploads a compiled plaintext snapshot,
including the chosen typing speed, without running it. Standalone runs continue
after a companion disconnect; physical Stop and reconnect/remote Stop remain
available. Other runs stop on disconnect. Never automatically resend a failed job.

## Development

```powershell
py -3.12 -m venv .venv
.venv\Scripts\Activate.ps1
python -m pip install -r requirements-build.txt
python -m unittest discover -s tests -v
python launch.py --offline-ui-test dist/ui-checks
.\build.ps1
```

Build on native Windows x64 using x64 Python. Inno Setup 6 is required for the
per-user installer. `VERSION` supplies the application, PE and installer version;
build tooling rejects a mismatched version or non-x64 executable. CI retains
genuine Windows UI renders at 100/150/200% Tk scaling and installer receipts.
Those offline checks do not certify physical BLE/USB, target focus/layout,
SmartScreen reputation or multi-hour runs. See the
[parity checklist](../docs/WINDOWS-PARITY.md) and
[hardware validation](../docs/HARDWARE-VALIDATION.md).

The BLE asyncio loop lives on a dedicated MTA worker. Tk remains on the main
thread; no `allow_sta()` workaround is used. Runtime license texts are included
in `THIRD-PARTY.txt` inside the app and `WINDOWS-THIRD-PARTY.txt` in the release.
