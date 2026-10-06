# PC Health Remote - PC backend

Streams CPU/GPU/RAM/disk/battery metrics from a Windows 11 PC (Linux works best-effort) to the
PC Health Remote Flipper Zero app over BLE or USB. Wire protocol: [`../docs/PROTOCOL.md`](../docs/PROTOCOL.md).

## Build

```
cargo build --release          # Windows: GUI subsystem, no console window
cargo test
```

Linux needs `libdbus-1-dev libudev-dev pkg-config`. The tray icon is Windows-only.

## Commands

```
pc-health-remote [run]            tray app (default); streams every 1 s or what the Flipper's HELLO asks
pc-health-remote pair             scan for "PCHealth*", pair the strongest with no Windows UI, save it
    --address AA:BB:CC:DD:EE:FF   pair a specific device        --force   unpair first (stale bond)
    --scan-secs 10
pc-health-remote unpair           remove the pairing and the saved device
pc-health-remote metrics          print one JSON snapshot incl. which source each value came from
pc-health-remote install          create the "PC Health Remote" scheduled task (run ONCE, elevated)
pc-health-remote uninstall        remove the task
pc-health-remote status           config, pairing, USB ports, task state, running instances
```

Global flags: `--console` (attach/allocate a console and echo the log), `-v/--verbose`,
`--simulate` (synthetic changing values to test Flipper alerts), `--transport auto|ble|usb`,
`--interval 1..10`, `--no-tray`.

The release exe has no console. Commands other than `run` attach to the parent terminal
automatically; in `cmd.exe` use `start /wait pc-health-remote.exe status` if the prompt returns early.

## Transports

* **BLE**: a watcher waits for the saved device, then connects silently (bonded, no toast), subscribes
  to the Flipper serial service and waits for HELLO. Re-arms with backoff after any disconnect.
* **USB**: serial ports `0483:5740` are opened read-only until a valid HELLO arrives (3 s), then streamed.
  Rescans every 2 s. USB wins when both deliver a HELLO.
* The Flipper's HELLO sets the interval; frames are never more than 4 s apart (its link timeout is 5 s).

## Files

`%APPDATA%\pc-health-remote\config.toml` (`last_device`, `transport`, `interval_s`) and `log.txt`
(rotates to `log.old.txt` at ~1 MB). Override the folder with `PHR_CONFIG_DIR`; log level with `PHR_LOG`.

## Sensors

CPU temperature: LibreHardwareMonitor WMI (`root\LibreHardwareMonitor`, start LHM with its WMI
provider / "Run on Windows startup") -> `ThermalZoneInformation` perf counter -> `MSAcpi_ThermalZoneTemperature`
(admin). The first working source is cached and better ones are re-probed every minute. Fan RPM is only
reported when LibreHardwareMonitor exposes a fan sensor. GPU via NVML (`nvml.dll`). Run
`pc-health-remote metrics` to see what was picked.
