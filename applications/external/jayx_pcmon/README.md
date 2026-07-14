# JAYX

Live **PC monitor** for Flipper Zero over **USB**.

Shows CPU, RAM, GPU, temperatures, FPS, and hardware/OS info on the Flipper screen. A small Windows agent streams data from your PC.

**Bluetooth is still in development.** Use USB for now.

## Full setup

See the main guide: **[docs/SETUP.md](../../docs/SETUP.md)**

Quick path:

1. On PC: `pip install ufbt` and `pip install -r pc_agent/requirements.txt`
2. Build/install: `cd apps/jayx && ufbt launch` (stop any agent first)
3. Flipper: **Apps → USB → JAYX → USB → OK**
4. PC: `cd pc_agent && python monitor.py --usb`

## Pages

| Page | Content |
|------|---------|
| **System** | CPU / RAM / GPU / VRAM usage and temps |
| **Game** | Game/window name, large FPS, frametime |
| **Specs** | SYSTEM · OS · CPU · MEMORY · GPU (Up/Down) |
| **About** | Version and link type |

## Controls

| Key | Action |
| --- | --- |
| **OK** | Start USB (from menu) |
| **Left / Right** | Change page |
| **Up / Down** | Specs section cards |
| **Back** | Exit (or leave Bluetooth WIP screen) |

## Requirements

| Need | For |
|------|-----|
| Windows PC + USB cable | Required |
| Python 3.10+ agent | Required |
| NVIDIA drivers | GPU stats |
| RTSS | FPS |
| LibreHardwareMonitor | CPU temp |

## Source

- App: `apps/jayx/`
- Agent: `pc_agent/`
- License: MIT (see repository root `LICENSE`)
- Repo: https://github.com/contactjayclatty/JAYX
