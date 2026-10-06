# PC Health Remote

Your PC's temperatures and load on your Flipper Zero, with vibration, sound and LED alerts.

**Needs the free PC backend:** download pc-health-remote.exe from
https://github.com/vladatman/pc-health-remote/releases (Windows 11; Linux best-effort).

## Features
- **Temps page:** CPU/GPU temperature and CPU, GPU, RAM and VRAM load bars.
- **Details page:** RAM/VRAM totals, CPU clock, fan, disk, battery, uptime, top CPU and RAM process.
- **Alerts:** CPU/GPU temperature, CPU/GPU/RAM/VRAM/disk load, low battery and lost link. Each rule has its own threshold and signal (Vibro, Sound, Vib+Snd, LED, Off).
- **Tips:** every alert shows the value vs. the limit and 2 short tips. OK snoozes it for 10 minutes.
- **Transport:** Bluetooth LE (default) or USB. Settings are saved on the SD card.

## Setup
1. Run pc-health-remote.exe pair on the PC while this app is open.
2. When the Flipper shows **Verify code**, press **OK**. Windows shows nothing. This is needed only once.
3. From then on the PC reconnects automatically whenever you open the app.

Run pc-health-remote.exe install once from an elevated terminal to start the backend at logon.
CPU temperature is most accurate when LibreHardwareMonitor is running; GPU data needs an NVIDIA GPU.
Full guide: https://github.com/vladatman/pc-health-remote/blob/main/docs/SETUP.md

## Controls
Left/Right: switch pages - OK: settings (or snooze an alert) - Back: exit.
