# JAYX — Full setup guide

Get live PC stats on your Flipper Zero end-to-end: build the app, install the Windows agent, and stream CPU / RAM / GPU / temps / FPS / system info over **USB**.

> **Bluetooth is still in development.** Use USB only for a working setup.

---

## What you need

| Item | Notes |
| --- | --- |
| **Flipper Zero** | Official or compatible firmware; USB cable |
| **Windows PC** | Agent is written for Windows (psutil / NVML / RTSS / WMI) |
| **Python 3.10+** | [python.org](https://www.python.org/downloads/) — check “Add to PATH” |
| **pip** | Comes with Python |
| **ufbt** | Flipper build tool (`pip install ufbt`) |
| **qFlipper** (optional) | Useful to confirm the device shows up; close it when using JAYX USB |

### Optional (for richer data)

| Feature | Software |
| --- | --- |
| GPU % / VRAM / GPU temp | NVIDIA GPU + current Game Ready / Studio drivers |
| FPS + game name | [RivaTuner Statistics Server (RTSS)](https://www.guru3d.com/files-details/rtss-rivatuner-statistics-server-download.html) (or MSI Afterburner, which includes RTSS) |
| CPU temperature | [LibreHardwareMonitor](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor) — leave it running |

Without these, JAYX still runs; missing sensors show as `N/A` or `--`.

---

## 1. Get the project

```powershell
git clone https://github.com/contactjayclatty/JAYX.git
cd JAYX
```

Or download the ZIP from GitHub and extract it.

---

## 2. Install tools on the PC

Open **PowerShell**:

```powershell
# Python packages for the agent
cd C:\path\to\JAYX\pc_agent
python -m pip install --upgrade pip
python -m pip install -r requirements.txt

# Flipper build tool (once per machine)
python -m pip install ufbt
```

First time using ufbt, it will download the Flipper SDK when you build (needs internet).

Check versions:

```powershell
python --version
ufbt --version
```

---

## 3. Install JAYX on the Flipper

### Important order

1. **Stop** any running `python monitor.py` (it locks the COM port).
2. **Close** qFlipper / other serial tools if open.
3. Plug Flipper in via USB.
4. Then build/launch.

```powershell
cd C:\path\to\JAYX\apps\jayx
ufbt launch
```

What this does:

- Builds `jayx.fap`
- Copies it to the SD card (`/ext/apps/USB/jayx.fap`)
- Starts the app

### If `ufbt launch` fails with “Access is denied” / COM busy

Something is holding the serial port:

```powershell
# Find leftover agent / runfap processes
Get-CimInstance Win32_Process |
  Where-Object { $_.CommandLine -match 'monitor\.py|runfap' } |
  Select-Object ProcessId, CommandLine

# Stop the agent if needed (replace PID)
Stop-Process -Id <PID> -Force
```

Then unplug/replug the Flipper and run `ufbt launch` again.

### Open the app later

On the Flipper:

**Apps → USB → JAYX**

---

## 4. Start a monitoring session (USB)

Do this **every time** you want live stats:

### On the Flipper

1. Open **JAYX**
2. Highlight **USB** (Up/Down if needed)
3. Press **OK**
4. You should see: `USB · waiting...` / `monitor.py --usb`

### On the PC

```powershell
cd C:\path\to\JAYX\pc_agent
python monitor.py --usb
```

You should see something like:

```text
JAYX agent starting (USB)
JAYX agent · USB mode
Connected to COMx
Specs: collecting ...
Specs: ready (5 sections ...)
```

Within about **1–2 seconds**, the Flipper **System** page should fill with live bars.

### When you’re done

1. On PC: **Ctrl+C** in the agent window  
2. On Flipper: **Back** to exit JAYX (restores normal USB/serial)

---

## 5. Using the pages

| Page | What it shows | Keys |
| --- | --- | --- |
| **System** | CPU %, temp · RAM · GPU %, temp · VRAM | Left / Right to change page |
| **Game** | Window/game title · large FPS · frametime | Same |
| **Specs** | SYSTEM / OS / CPU / MEMORY / GPU cards | **Up / Down** switch cards |
| **About** | App version, protocol, link type | Same |

Page dots at the bottom show which page you’re on.

---

## 6. Optional: better FPS and temps

### FPS (Game page)

1. Install and run **RTSS** (tray icon should be visible).
2. Start a game (or any app RTSS hooks).
3. Keep that window **focused**.
4. On Flipper, go to the **Game** page (Left/Right).

If RTSS isn’t running or nothing is hooked, you’ll see idle / no FPS.

### CPU temperature

1. Download **LibreHardwareMonitor**.
2. Run it (can minimize to tray).
3. Restart the agent if it was already running.

### GPU metrics

Need an **NVIDIA** GPU with drivers installed. AMD/Intel iGPU will usually show GPU/VRAM as N/A.

---

## 7. Everyday checklist (cheat sheet)

```text
□ Close qFlipper / stop old monitor.py
□ Flipper plugged in
□ Apps → USB → JAYX → USB → OK
□ pc_agent:  python monitor.py --usb
□ (Optional) RTSS + game for FPS
□ (Optional) LibreHardwareMonitor for CPU temp
□ Done → Ctrl+C agent, Back on Flipper
```

---

## 8. Troubleshooting

| Problem | Fix |
| --- | --- |
| Flipper stuck on “USB · waiting…” | Agent not running, wrong mode, or wrong COM. Run `python monitor.py --usb` while JAYX USB screen is open. |
| `Flipper Zero not found` | Cable, drivers, close qFlipper, open JAYX USB mode first. |
| `Access is denied` on COM | Stop `monitor.py` / `runfap` / qFlipper; unplug/replug. |
| GPU always N/A | Non-NVIDIA GPU, or drivers missing (`nvidia-ml-py` needs NVML). |
| FPS always empty | Install/start RTSS; focus a game window. |
| CPU temp N/A | Start LibreHardwareMonitor. |
| Specs empty / “Collecting” | Wait a few seconds after agent starts (WMI runs in background). |
| `ufbt` / API mismatch | Run `ufbt update` in `apps/jayx`, rebuild with `ufbt launch`. |
| qFlipper can’t connect while JAYX is open | Expected — JAYX owns USB CDC. Exit JAYX with Back. |
| Bluetooth option | WIP — shows “still in development”. Use USB. |

### Verify Python can see the Flipper (optional)

```powershell
python -c "import serial.tools.list_ports as p; print([ (x.device, hex(x.vid or 0), hex(x.pid or 0), x.description) for x in p.comports() ])"
```

Flipper often appears as VID `0x0483` PID `0x5740`.

---

## 9. Updating JAYX

```powershell
cd C:\path\to\JAYX
git pull

cd apps\jayx
# stop agent first!
ufbt launch

cd ..\..\pc_agent
python -m pip install -r requirements.txt
python monitor.py --usb
```

---

## 10. Project layout (reference)

```text
JAYX/
  apps/jayx/          # Flipper app source (ufbt)
  pc_agent/           # Windows agent
    monitor.py        # entry: python monitor.py --usb
    requirements.txt
  screenshots/        # UI captures
  docs/
    SETUP.md          # this guide
    BUILDING.md
```

---

## Still stuck?

1. Confirm agent prints `Connected to COMx`.  
2. Confirm Flipper is on **USB · waiting…** (not the main menu only).  
3. Try another cable/port.  
4. Open an issue on [github.com/contactjayclatty/JAYX](https://github.com/contactjayclatty/JAYX) with:
   - Agent terminal output  
   - Flipper screen state  
   - Windows version + GPU brand  
