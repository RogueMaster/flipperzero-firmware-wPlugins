Run SSH commands and scripts on your servers from the Flipper Zero, over LAN or WiFi.

**Requires an ESP32-S3 board with a W5500 Ethernet chip, running the SSH Runner firmware.** Two boards run it: the SSH Runner add-on board, which plugs straight onto the Flipper's GPIO header with no wiring (design files in the repository's [hardware folder](https://github.com/SeanChangX/flipper-ssh-runner/tree/main/hardware), CC BY-NC-SA 4.0), or a Waveshare ESP32-S3-ETH wired to the GPIO header (TX, RX and GND; optionally 3V3 to run it from the Flipper's battery). The app does nothing without one. Wiring and setup: [Quick Start](https://github.com/SeanChangX/flipper-ssh-runner/blob/main/docs/QUICKSTART.md) and [Hardware and Wiring](https://github.com/SeanChangX/flipper-ssh-runner/blob/main/docs/HARDWARE.md).

## What it does

- **Quick Connect:** type user@host, check the host key, enter the password, run a command and read its output with the exit code.
- **Hosts:** saved hosts, each with its own commands, key, transport (LAN or WiFi) and host-key policy.
- **Scripts:** shell scripts from the SD card, run on the host.
- **Keys:** Ed25519, ECDSA and RSA keys from the SD card, or generated on the board; an encrypted key needs its passphrase only once.
- **Network:** LAN and WiFi status, DHCP or a static address, WiFi scan and join, ping.

Passwords are never written to the SD card. The SSH connection runs on the board; the Flipper only shows it.

Source code, the board's firmware and the PC tool: [github.com/SeanChangX/flipper-ssh-runner](https://github.com/SeanChangX/flipper-ssh-runner)
