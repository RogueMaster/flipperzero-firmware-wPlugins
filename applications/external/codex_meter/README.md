# Codex Monitor

Codex usage limits on Flipper Zero, with a Windows desktop companion and a Linux terminal companion by **Dr. Mosfet**.

The Flipper dashboard shows the two quota windows reported by your Codex account,
the remaining allowance and the time until each reset. The companion reads the
local Codex account and sends only quota values to the device over Bluetooth LE
or USB. It does not generate model responses.

## Requirements

- A Flipper Zero with a microSD card and a compatible firmware release.
- Windows or Linux, with Bluetooth LE or a USB cable.
- Codex CLI on Linux. On Windows, use Codex CLI or the Codex desktop application.
- A ChatGPT-backed Codex account and an internet connection for quota reads.

The Windows executable includes Python and its runtime dependencies. Codex itself
must be installed separately. API-key-only authentication does not provide a
ChatGPT subscription quota.

## Getting started on Windows

1. Install Codex Monitor on the Flipper and open it under Apps > Bluetooth.
2. Start the Windows companion and select Bluetooth or USB.
3. Use your existing local Codex login, or select **Zaloguj konto** to complete
   the official OpenAI browser sign-in. The current Windows interface is Polish.
4. Confirm Bluetooth pairing if requested. Close qFlipper when using USB mode.

The desktop companion detects Codex automatically. If detection fails, select
**Wskaż Codex.exe** and choose the executable installed on your computer.

## Getting started on Linux

Install [Codex CLI](https://developers.openai.com/codex/cli) and
[uv](https://docs.astral.sh/uv/getting-started/installation/). In the repository
root, sign in with your ChatGPT account and check that Codex reports your limits:

```bash
codex login
uv run --python 3.11 host/meter.py --once
```

Open Codex Monitor on your Flipper Zero. For native Linux, connect it over USB
and make sure your user has permission to open the serial device. Close
qFlipper if it is using the port.

```bash
uv run --python 3.11 host/meter.py --transport usb
```

If more than one matching device is connected, pass its actual serial device
with `--port`, for example `--port /dev/ttyACM0`.

To connect over Bluetooth LE, start BlueZ and run:

```bash
uv run --python 3.11 host/meter.py --transport ble
```

Confirm the pairing prompt if shown. The Linux companion runs in the terminal.
Bluetooth LE and USB have been tested on Windows hardware. Linux hardware
validation is pending.

## Flipper controls

- **OK:** request a fresh quota read.
- **Left / Right:** switch between remaining and used percentages.
- **Back:** close the application.

Normal screen brightness follows the firmware setting. After its inactivity
timeout the backlight stays at 24% while the app is open. Closing the app restores
automatic backlight management. Quota updates do not wake the backlight.

The header distinguishes live, offline, stale and API-error states. Missing
windows are displayed as unavailable. A reset countdown reaching zero does not
invent a new allowance: a fresh account response is required.

While this app is open it uses the Flipper Bluetooth serial profile, temporarily
replacing the usual mobile-app Bluetooth connection.

## Account and privacy

Each user signs in with their own local Codex account. No developer login, API
key or credential file is included in the companion. Password entry takes place
on the official OpenAI website. Codex manages the resulting local session.

Changing accounts through the companion changes the local Codex session used by
other Codex clients. The Flipper receives percentages, window lengths and reset
countdowns. It receives no password or authentication token.

Monitor preferences contain only the selected transport and executable path.
They are stored under LOCALAPPDATA/DrMosfet/CodexMonitor/settings.json.

## Building

Flipper application, using the current official release SDK:

```powershell
cd flipper
uv tool run --python 3.11 --from ufbt ufbt update --channel release
uv tool run --python 3.11 --from ufbt ufbt
```

Windows companion:

```powershell
./BUILD-PC.ps1
```

The output is desktop/CodexMonitor.exe. For development the command-line bridge
can also be run directly:

```powershell
uv run --python 3.11 host/meter.py
uv run --python 3.11 host/meter.py --transport usb
uv run --python 3.11 host/meter.py --once
```

Run the host tests with:

```powershell
uv run --python 3.11 python -m unittest discover -s tests -v
```

The Flipper application was built against official firmware 1.5.1, API 88.2.
Bluetooth and USB quota transfer were exercised on physical hardware. See the
catalog submission notes for the exact hardware/firmware validation scope.

## Credits and license

Created and maintained by **Dr. Mosfet**. The Bluetooth lifecycle code is adapted
from [Kota](https://github.com/cobanov/kota) by Mert Cobanov under the MIT license.
Its copyright notice is retained in LICENSE-Kota and in the Flipper source.

Software is MIT licensed. DR. MOSFET branding assets remain the property of
Dr. Mosfet and are not covered by the software license. The catalog Flipper
application uses text branding and a letter-based menu icon.

Codex Monitor is an independent project and is not affiliated with or endorsed
by OpenAI or Flipper Devices.
