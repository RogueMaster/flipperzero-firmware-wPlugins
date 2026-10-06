# Setup guide

This guide takes you from nothing to a working PC Health Remote dashboard on your Flipper Zero.
Tested on Windows 11 with a Flipper Zero running RogueMaster firmware; builds also exist for the official firmware
(SDK API 87.1, fw 1.4.3).

**What you need**

* A Flipper Zero (official firmware or RogueMaster).
* A Windows PC with Bluetooth LE (or a USB cable).
* An NVIDIA GPU if you want GPU values (other GPUs show `--`).

## 1. Install the Flipper app

**Option A - from the catalog.** Open the Flipper mobile app or the web catalog, search for *PC Health Remote* and
install it. It appears under *Apps -> Bluetooth*.

**Option B - from a release file.**

1. Download `pc_health_remote.fap` from the [releases page](https://github.com/vladatman/pc-health-remote/releases).
2. Open qFlipper, go to the file manager (or *Install from file*) and put the file into `/ext/apps/Bluetooth/`
   on the SD card.
3. On the Flipper open *Apps -> Bluetooth -> PC Health Remote*.

Note: use the build matching your firmware. The release is built with the official SDK; if the app refuses to
start on a custom firmware, build it yourself against that firmware's SDK (see
[Build from source](../README.md#build-from-source)).

When the app starts, you see the waiting screen until a PC connects.

## 2. Get the PC backend

1. Download the Windows exe from the [releases page](https://github.com/vladatman/pc-health-remote/releases)
   (`pc-health-remote-windows-x86_64.exe`).
2. Put it in a permanent folder, for example `C:\Tools\pc-health-remote\`, and rename it to
   `pc-health-remote.exe` (the examples below use that name).

The autostart task points to wherever the exe is when you run `install`, so do not move it afterwards
(if you do, run `install` again).

## 3. First pairing (once)

1. On the Flipper, open *PC Health Remote* and leave it on the screen. Make sure the transport in its settings
   is Bluetooth (the default).
2. On the PC, open PowerShell in the exe's folder and run:

   ```powershell
   .\pc-health-remote.exe pair
   ```

3. The backend scans for a device named `PCHealth...`, and starts pairing. The Flipper displays **Verify code**.
   **Press OK on the Flipper.**
4. Windows shows no dialog - the backend accepts the pairing request programmatically. When the command finishes, the
   device is saved to `config.toml` (`last_device`).

From now on, reconnection is automatic and silent: just open the app on the Flipper while the backend is running.

Useful pairing options: `--address AA:BB:CC:DD:EE:FF` (pair a specific device), `--force` (unpair first, for a stale
bond), `--scan-secs 10`.

## 4. Run the backend

```powershell
.\pc-health-remote.exe
```

A tray icon appears and the Flipper switches from the waiting screen to the dashboard:

![Temps page](img/1_temps.png)

Left/Right switches pages. Page 2 ("Details") shows more:

![Details page](img/2_details.png)

To check the backend without the Flipper, run `.\pc-health-remote.exe metrics` - it prints one JSON snapshot and
shows which source supplied each value. `.\pc-health-remote.exe status` shows the config, pairing, USB ports and
the autostart task state.

## 5. Start automatically at logon

Open PowerShell **as administrator** (once) and run:

```powershell
cd C:\Tools\pc-health-remote
.\pc-health-remote.exe install
```

This creates a Task Scheduler task named "PC Health Remote" that starts the backend at logon with highest
privileges (needed to read some sensors). Remove it with `.\pc-health-remote.exe uninstall`.

## 6. Settings and alerts

Press **OK** on the dashboard to open settings. Choose the transport (Bluetooth or USB) and edit alert rules.

![Settings](img/4_settings.png)

Each rule has an on/off switch, a threshold and a signal:

![Alert rule](img/5_rule.png)

Signals: Vibro, Sound, Vib+Snd, LED, Off. Settings are stored on the SD card.

When a rule trips, the alert screen shows the value against the limit and two tips. **OK** snoozes it for
10 minutes, **Back** closes it.

![Alert screen](img/3_alert.png)

Default rules:

| Rule | Default limit | Must hold for | Signal | Enabled |
|------|---------------|---------------|--------|---------|
| CPU temperature | 90 C | - | Vib+Snd | yes |
| GPU temperature | 85 C | - | Vib+Snd | yes |
| CPU load | 95 % | 30 s | Vibro | yes |
| GPU load | 98 % | 30 s | Vibro | off |
| RAM load | 90 % | 10 s | Vibro | yes |
| VRAM load | 95 % | 10 s | Vibro | yes |
| Disk used | 95 % | - | Sound | yes |
| Battery low | 20 % (only while on battery) | - | Vib+Snd | yes |
| PC link lost | 10 s | - | Vib+Snd | yes |

To test alerts without stressing your PC, run the backend with `--simulate`, which sends synthetic changing values.

## 7. Optional: LibreHardwareMonitor (accurate CPU temperature and fan speed)

Without extra software the CPU temperature comes from the Windows ACPI thermal zone, which is coarse or missing on
some machines, and fan speed is not available at all. For the best data:

1. Download [LibreHardwareMonitor](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/releases) and run it
   (as administrator, so it can read the sensors).
2. In its Options menu enable running at Windows startup (and starting minimized if you like).
3. That is all. While LibreHardwareMonitor runs, it publishes its sensors through WMI automatically; the
   "Remote web server" option is **not** needed.

The backend then prefers LibreHardwareMonitor: CPU temperature is the CPU package sensor (most accurate), and the fan
RPM appears on the Details page when LibreHardwareMonitor exposes a fan sensor. The backend re-checks for better
sources every minute, so you can start LibreHardwareMonitor at any time. Run `pc-health-remote.exe metrics` to see
which source is in use.

## 8. USB mode

Instead of Bluetooth you can connect with a USB cable: change the transport to USB in the Flipper app's settings.

* USB mode takes over the Flipper's USB port as a serial device. **qFlipper (and the Flipper CLI) disconnect while
  the app is in USB mode.** Close the app (or switch back to Bluetooth) to use qFlipper again.
* The backend only reads from a Flipper serial port until it has received a valid HELLO from the app, so it never
  writes into the normal Flipper CLI.
* The backend's `transport` setting (`auto`, `ble` or `usb`) lives in `%APPDATA%\pc-health-remote\config.toml`.
  `auto` (default) never holds the COM port while a Bluetooth link is active.

## Troubleshooting

| Symptom | What to try |
|---------|-------------|
| Flipper stays on the waiting screen, or shows "PC link lost" | Check the backend is running (tray icon, or `pc-health-remote.exe status`). Open the app on the Flipper *after* the backend is running (or reopen it). Confirm the transport in the app's settings matches how you connect. Look at the log (see below). |
| `pair` fails, or the Flipper never asks to verify | Keep *PC Health Remote* open on the Flipper during pairing and stay close to the PC. Then run `pc-health-remote.exe unpair`, remove any old "PCHealth..." entry in Windows *Bluetooth & devices* settings, and run `pc-health-remote.exe pair --force` again. Press OK on the Flipper at **Verify code**. |
| Paired before but now no connection | Same as above: `unpair`, then `pair` again (a stale bond on either side breaks the link). |
| CPU temperature shows `--` | Run LibreHardwareMonitor (step 7) as administrator. Without it the backend falls back to the Windows ACPI thermal zone, which some PCs do not provide. Check `pc-health-remote.exe metrics` for what was found. |
| GPU values show `--` | GPU data comes from NVIDIA NVML and works with NVIDIA GPUs only. Install/update the NVIDIA driver. Other GPUs are not supported. |
| Fan shows `--` | Fan speed is only available through LibreHardwareMonitor, and only if your hardware exposes a fan sensor. |
| `install` says access denied | Run it from an elevated (administrator) PowerShell. |
| Backend does not start at logon | Run `pc-health-remote.exe status` to see the task state; run `install` again if you moved the exe. |
| qFlipper cannot see the Flipper | You are probably in USB mode; exit the app or switch to Bluetooth. |
| Alerts never fire | Check the rule is enabled and its signal is not Off; remember some rules need the value to hold for 10-30 s; and a snoozed alert stays silent for 10 minutes. Test with `--simulate`. |

**Logs and config:** `%APPDATA%\pc-health-remote\` contains `config.toml` and `log.txt` (rotated to `log.old.txt` at
about 1 MB). Start the backend with `--console` (and `-v` for more detail) to watch the log live.

Still stuck? [Open an issue](https://github.com/vladatman/pc-health-remote/issues) with your Windows version,
Flipper firmware, the output of `pc-health-remote.exe status` and `metrics`, and the relevant part of `log.txt`.
