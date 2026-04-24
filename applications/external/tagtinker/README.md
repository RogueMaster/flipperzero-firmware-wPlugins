# TagTinker V2.0

<p align="center">
  <strong>Infrared ESL Research Toolkit for Flipper Zero</strong><br>
  <sub>Protocol study • Signal analysis • Digital Art</sub>
</p>

<p align="center">
  <img alt="License: GPL-3.0" src="https://img.shields.io/badge/License-GPL--3.0-blue.svg">
  <img alt="Platform: Flipper Zero" src="https://img.shields.io/badge/Platform-Flipper%20Zero-black.svg">
</p>


<img alt="Demo Image"  src="https://raw.githubusercontent.com/i12bp8/TagTinker/refs/heads/main/tagtinkerdemo.jpg">

## Overview

TagTinker is a Flipper Zero app and Android Companion for exploring infrared electronic shelf-label (ESL) protocols. It allows you to transmit custom images and text to supported graphics tags.

As the Flipper Zero team notes:
> "FYI: this is pure infrared signal, same that you use in TV remotes. The whole security was relying on obscurity of protocol."

This tool is built for IoT security curiosity, learning about obscure protocols, and displaying digital art on e-ink hardware.

> [!WARNING]
> **Hardware Warning:** Many infrared ESL tags store their firmware, address, and display data in volatile RAM to save cost and energy. If you remove the battery or let it fully discharge, the tag will lose all programming and become unresponsive ("dead"). It usually cannot be recovered without the original base station.

## Features

- **TagTinker Flipper App:** High-performance, zero-allocation RLE streaming IR engine.
- **TagTinker Android Companion:** Edit and dither images directly on your phone and sync them instantly to the Flipper Zero over BLE.
- Display text, custom images, and test-patterns.
- Support for monochrome and accent-color (red/yellow) graphics tags.

## Getting Started

The TagTinker Android app manages preparing display payloads and uploading them to the Flipper over BLE.
1. Build the Flipper app from this repository and install it via `ufbt`.
2. Download the pre-built Android Companion APK from the [Releases](https://github.com/i12bp8/TagTinker/releases) page and install it on your device.
3. Open the Flipper app and go to **Phone Sync (Custom Images)**.
4. Use the Android app to connect, prepare an image, and send it directly to the Flipper for transmission.

## FAQ

**Does this require a Flipper Zero?**

No, not at all! You can do this with less than $5 worth of microcontroller hardware (like an ESP32 and an IR LED). The Flipper Zero just happens to be my favorite security research tool, which is why I built the app for this platform.

**Where is the `.fap` release?**

The Flipper app is source-first. Build the `.fap` yourself from this repository with `ufbt` so it matches your firmware and local toolchain.

**What if it crashes or behaves oddly?**

If you are using a custom firmware branch, custom asset packs, or a heavily modified device setup, start by testing from a clean baseline firmware.

## Credits & Background

This project is deeply indebted to the incredible public reverse-engineering work by **furrtek**. 
To understand the underlying protocol, signal structure, and history, please read his research:
- **Furrtek’s ESL research:** [https://www.furrtek.org/?a=esl](https://www.furrtek.org/?a=esl)
- **PrecIR reference implementation:** [https://github.com/furrtek/PrecIR](https://github.com/furrtek/PrecIR)

## Disclaimer

> [!CAUTION]
> **STRICTLY PROHIBITED FOR ILLEGAL USE**
> 
> TagTinker is an independent project intended **strictly** for educational research, security curiosity, and displaying digital art on hardware that **you legally own**. 
> 
> Under no circumstances is this software allowed to be used for illegal activities. You are strictly prohibited from using TagTinker to alter retail displays, modify electronic shelf labels in stores, interfere with third-party infrastructure, or cause any form of vandalism or financial harm. 
> 
> The creator of TagTinker assumes absolutely no liability for any misuse of this software. By using this software, you agree to take full responsibility for your actions and use it responsibly and legally.

## License

Licensed under the **GNU General Public License v3.0** (GPL-3.0). See the [`LICENSE`](LICENSE) file for details.
