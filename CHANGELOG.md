# Changelog

## Unreleased

**Repository**
- The repository moved from `vladatman/pc-health-remote` to `ataman-lab/pc-health-remote`; old links redirect.

**Telegram alerts (beta)**
- New Telegram bot (`telegram/`, Cloudflare Worker): `/link` pairs a PC, alerts arrive as chat messages.
  Free: 1 PC, 10 alerts/day. Pro: 5 PCs, unlimited, 99 Telegram Stars per 30 days (`/pro`).
- Backend: the Flipper alert rules now also run on the PC; `--link-telegram <CODE>` and a `[telegram]` config section.
- `/pair` is rate limited per IP; the bot stores only chat id, language, source tag, device keys, PC names and counters.

**Flipper app (v1.1)**
- Every alert type has its own sound: hot CPU/GPU = fast high alarms, load = rising tones, RAM/VRAM = falling tones,
  disk = low thuds, low battery = slow falling tones, lost link = drop to a low note. The Vibro+Sound signal uses the
  same melodies; vibration patterns are unchanged.

## v1.0.0 - 2026-10-06

Initial release.

**Flipper app (v1.0)**
- Dashboard page "Temps": CPU/GPU temperature and CPU/GPU/RAM/VRAM load bars.
- Dashboard page "Details": RAM/VRAM totals, CPU clock, fan, disk %, battery, uptime, top CPU and top RAM process.
- Alerts with configurable threshold and signal (Vibro, Sound, Vib+Snd, LED, Off) per rule: CPU temp, GPU temp,
  CPU load, GPU load, RAM, VRAM, disk, low battery, PC link lost. The alert screen shows value vs. limit and 2 tips;
  OK snoozes for 10 minutes.
- Settings screen (OK on the dashboard), Bluetooth LE (default) or USB transport, settings saved on the SD card.
- Builds for the official firmware SDK (API 87.1, fw 1.4.3) and RogueMaster (API 88.4).

**PC backend (`pc-health-remote`)**
- Windows tray app (Linux best-effort) written in Rust.
- Commands: `run`, `pair`, `unpair`, `metrics`, `status`, `install`, `uninstall`; flags `--simulate`, `--console`.
- One-time BLE pairing with numeric comparison confirmed on the Flipper; silent automatic reconnects afterwards.
- Autostart through a Task Scheduler logon task with highest privileges.
- CPU temperature from LibreHardwareMonitor (WMI) or the Windows ACPI thermal zone; GPU via NVIDIA NVML;
  fan RPM via LibreHardwareMonitor.
- Wire protocol documented in `docs/PROTOCOL.md`.
