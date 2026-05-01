# TagTinker V2.1

<p align="center">
  <strong>Infrared ESL Research Toolkit for Flipper Zero</strong><br>
  <sub>Protocol study • Signal analysis • Digital Art</sub>
</p>

<p align="center">
  <img alt="License: GPL-3.0" src="https://img.shields.io/badge/License-GPL--3.0-blue.svg">
  <img alt="Platform: Flipper Zero" src="https://img.shields.io/badge/Platform-Flipper%20Zero-black.svg">
  <a href="https://i12bp8.github.io/TagTinker/"><img alt="Image Prep" src="https://img.shields.io/badge/Image%20Prep-Open%20in%20browser-a78bfa?logo=github"></a>
</p>

<p align="center">
  <strong><a href="https://i12bp8.github.io/TagTinker/">→ Launch the TagTinker Image Prep web app ←</a></strong>
</p>

<img alt="Demo Image"  src="https://raw.githubusercontent.com/i12bp8/TagTinker/refs/heads/main/tagtinkerdemo.jpg">

## Overview

TagTinker is a Flipper Zero app for exploring infrared electronic shelf-label (ESL) protocols. It allows you to transmit custom images and text to supported graphics tags. A companion **web image preparer** runs entirely in the browser and lets you drop, dither and download Flipper-ready BMPs without any install.

As the Flipper Zero team notes:
> "FYI: this is pure infrared signal, same that you use in TV remotes. The whole security was relying on obscurity of protocol."

This tool is built for IoT security curiosity, learning about obscure protocols, and displaying digital art on e-ink hardware.

> [!WARNING]
> **Hardware Warning:** Many infrared ESL tags store their firmware, address, and display data in volatile RAM to save cost and energy. If you remove the battery or let it fully discharge, the tag will lose all programming and become unresponsive ("dead"). It usually cannot be recovered without the original base station.

## Features

- **TagTinker Flipper App:** High-performance, zero-allocation RLE streaming IR engine.
- **TagTinker Image Prep (web):** Single-file, dependency-free HTML page that lists every supported tag profile, runs a full image pipeline (tone, contrast, detail, sharpen, dither, photo-grade Oklab 3-colour quantisation) and exports a Flipper-ready BMP. Hosted at **[i12bp8.github.io/TagTinker](https://i12bp8.github.io/TagTinker/)** (source: `web-image-prep/`).
- **Drop-folder image flow:** Drop a prepared BMP into `apps_data/tagtinker/dropped/` on the Flipper SD card, then open `Targeted Payloads → <tag> → Set Image` and pick it. The Flipper rescales any BMP on the fly so a single file can target any tag and any page.
- **NFC Tag Scan:** Instantly identify ESL targets by scanning their NFC tag — no manual barcode entry needed.
- **WiFi Plugins (optional):** Plug a Flipper WiFi Dev Board (ESP32-S2) into the GPIO header to unlock live, network-rendered tag designs — crypto price cards, weather tiles, identicons, and more — auto-discovered by the FAP. New plugins live entirely on the cloud worker; the Flipper firmware never has to be re-flashed to add one.
<img alt="image" src="https://raw.githubusercontent.com/i12bp8/TagTinker/refs/heads/main/PXL_20260427_092219442.jpg" />

- Display text, custom images, and test-patterns.
- Support for monochrome and accent-color (red/yellow) graphics tags.

## Getting Started

1. Build the Flipper app from this repository and install it via `ufbt`. The first launch creates `apps_data/tagtinker/dropped/` on your SD card.
2. Open **[i12bp8.github.io/TagTinker](https://i12bp8.github.io/TagTinker/)** in any browser, pick your tag profile, drop an image, tweak, and download the BMP.
3. Copy the BMP into `apps_data/tagtinker/dropped/` on the SD card (over `qFlipper`, USB MTP, or whatever you use).
4. On the Flipper open `Targeted Payloads → <your tag> → Set Image`, pick the BMP, choose a page, send.

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

NFC tag decoding contributed by **7h30th3r0n3**.  

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
