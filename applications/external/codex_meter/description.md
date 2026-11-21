# Codex Monitor

Keep your Codex usage limits on your Flipper Zero. View remaining or used
allowance for both account quota windows and a live countdown to each reset.

- Two quota meters with window lengths reported by Codex.
- Local reset countdowns between updates.
- Bluetooth LE or USB connection to the Windows or Linux companion.
- Manual refresh with OK and remaining/used toggle with Left or Right.
- Visible live, offline, stale, unavailable and API-error states.
- Backlight dims to 24% instead of turning off while the app is open.

## Requirements

Run the desktop companion on Windows or the terminal companion on Linux.
The Linux companion requires Codex CLI. On Windows, use Codex CLI or the Codex
desktop application. Sign in with your ChatGPT account and connect the computer
to the internet to read quota data. No additional Flipper hardware is required.

The companion reads account limits through the local Codex app-server. Only quota
numbers and reset countdowns are sent to the Flipper; no login tokens are sent to
the device. The application does not generate model responses.

While open, the app temporarily uses the Flipper Bluetooth serial profile in
place of the normal mobile-app connection. For USB mode, close qFlipper first.

[Companion, source code and setup](https://github.com/kamylwnb/codex-monitor)

Created and maintained by Dr. Mosfet. Bluetooth lifecycle code is adapted from
Kota by Mert Cobanov under the MIT license. This is an independent project.
