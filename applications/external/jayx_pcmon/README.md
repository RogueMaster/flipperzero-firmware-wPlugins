# JAYX

Live PC monitor for [Flipper Zero](https://flipperzero.one) — CPU, RAM, GPU, temps, FPS, and system info over USB.

Built with [ufbt](https://github.com/flipperdevices/flipperzero-ufbt).

**Bluetooth is still in development** — check this repo for updates. Use USB for now.

## Full setup guide

**→ [docs/SETUP.md](docs/SETUP.md)** — complete walkthrough: install tools, flash JAYX, run the PC agent, optional RTSS/temps, and troubleshooting.

## Screenshots

| Main menu | Live stats | FPS |
| :---: | :---: | :---: |
| ![Main menu](screenshots/MainMenu.png) | ![Live stats](screenshots/livestats.png) | ![FPS](screenshots/fpscounter.png) |

| System info | OS | CPU |
| :---: | :---: | :---: |
| ![System](screenshots/systeminfo.png) | ![OS](screenshots/osinfo.png) | ![CPU](screenshots/cpuinfo.png) |

| About | No game / FPS idle | Bluetooth (WIP) |
| :---: | :---: | :---: |
| ![About](screenshots/Aboutpage.png) | ![FPS idle](screenshots/fpscounterNogame.png) | ![Bluetooth WIP](screenshots/Bluetoothpage.png) |

## Quick start

> Prefer the [full setup guide](docs/SETUP.md) the first time.

```sh
# PC tools (once)
cd pc_agent
pip install -r requirements.txt
pip install ufbt

# Flipper — close any monitor.py / qFlipper first
cd ../apps/jayx
ufbt launch

# On Flipper: Apps → USB → JAYX → USB → OK
# Then on PC:
cd ../../pc_agent
python monitor.py --usb
```

| Optional feature | Need |
| --- | --- |
| GPU % / VRAM / GPU temp | NVIDIA drivers |
| FPS | [RTSS](https://www.guru3d.com/files-details/rtss-rivatuner-statistics-server-download.html) |
| CPU temp | LibreHardwareMonitor running |

### Controls

| Key | Action |
| --- | --- |
| Left / Right | System · Game · Specs · About |
| Up / Down | Specs section cards |
| Back | Exit |

## Layout

```
apps/jayx/        # Flipper FAP
pc_agent/         # Windows metrics agent
screenshots/      # Device UI captures
docs/
```

## Docs

- **[docs/SETUP.md](docs/SETUP.md) — full install & usage guide**  
- [docs/CATALOG.md](docs/CATALOG.md) — Flipper Apps Catalog submission checklist  
- [apps/jayx/README.md](apps/jayx/README.md) — app pages & protocol notes  
- [docs/BUILDING.md](docs/BUILDING.md) — ufbt commands  
- [docs/ADDING_AN_APP.md](docs/ADDING_AN_APP.md) — scaffold a new app  

## Requirements

- [ufbt](https://github.com/flipperdevices/flipperzero-ufbt) (`pip install ufbt`)
- Python 3.10+ (PC agent)
- Flipper Zero with matching firmware/API for the built FAP

## License

[MIT](LICENSE) — required for Flipper Apps Catalog distribution.
