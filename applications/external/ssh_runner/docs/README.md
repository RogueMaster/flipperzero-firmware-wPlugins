# SSH Runner Documentation

[← Back to the project README](../README.md)

SSH Runner is a Flipper Zero app plus firmware for an ESP32-S3 Ethernet/WiFi board that runs
SSH commands from the Flipper. Start with the Quick Start; the rest is reference.

## Getting Started

| Document | What it covers |
|---|---|
| [Quick Start](QUICKSTART.md) | Wiring, flashing the board, installing the app, the first connection, what each message means |
| [Hardware and Wiring](HARDWARE.md) | The prototype board, pin assignments, desk and portable power, the safety rule for USB-C |
| [Add-on Board](../hardware/README.md) | The SSH Runner add-on PCB: design files, ordering, errata, design notes and bring-up |

## Using

| Document | What it covers |
|---|---|
| [Flipper App](../flipper-app/README.md) | Every screen, the files on the SD card, the built-in examples, demo mode, firmware compatibility |
| [PC Tool (`sshrunner`)](../cli/README.md) | Editing hosts, commands, keys and scripts from a computer; backups; command-line use |

## Reference

| Document | What it covers |
|---|---|
| [Architecture](ARCHITECTURE.md) | Repository layout, how the app and the board talk, build outputs |
| [UART Protocol](PROTOCOL.md) | Framing, reliability and every message between the Flipper and the board |
| [Security](SECURITY.md) | What is secret and where it is stored, secret protection, host keys, wiping a board |

## Project

| Document | What it covers |
|---|---|
| [Changelog](../CHANGELOG.md) | Changes in each release |
| [Contributing](../CONTRIBUTING.md) | Reporting bugs, running the checks, sending a change |
| [Releasing](RELEASING.md) | Cutting a release, its files, verifying them, the Flipper Apps Catalog |
| [Hardware-in-the-Loop Tests](../tools/hil/README.md) | Test scripts that drive the board from a computer |
| [Third-Party Components](../THIRD_PARTY.md) | Bundled and linked components and their licenses |
| [License](../LICENSE) | GPL-3.0-only; the add-on board's design is [CC BY-NC-SA 4.0](../hardware/LICENSE) (non-commercial) |
