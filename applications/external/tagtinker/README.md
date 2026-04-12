# TagTinker

<p align="center">
  <strong>Infrared ESL Research Toolkit for Flipper Zero</strong><br>
  <sub>Protocol study • Signal analysis • Controlled display experiments on authorized hardware</sub>
</p>

<p align="center">
  <img alt="License: GPL-3.0" src="https://img.shields.io/badge/License-GPL--3.0-blue.svg">
  <img alt="Platform: Flipper Zero" src="https://img.shields.io/badge/Platform-Flipper%20Zero-black.svg">
  <img alt="Status: Research Project" src="https://img.shields.io/badge/Status-Research%20Only-informational.svg">
</p>

<p align="center">
  <img src="https://github.com/i12bp8/TagTinker/blob/main/demo.jpg" width="700" alt="TagTinker demo">
</p>

<p align="center">
  <sub>Owner-authorized lab display experiment</sub>
</p>

---

> [!IMPORTANT]
> **TagTinker is a research tool.**
>
> It is intended **only** for protocol study, signal analysis, and controlled experiments on hardware you personally own or are explicitly authorized to test.
>
> This repository does **not** authorize access to, modification of, or interference with any third-party deployment, commercial installation, or retail environment.

> [!WARNING]
> **Strictly prohibited uses include:**
> - Testing against deployed third-party systems
> - Use in retail or commercial environments
> - Altering prices, product data, or operational displays
> - Interfering with business operations
> - Bypassing pairing, authorization, or security controls
> - Any unauthorized, unlawful, or harmful activity

## Overview

TagTinker is a Flipper Zero app for **educational research into infrared electronic shelf-label protocols** and related display behavior on authorized test hardware.

It is focused on:
- protocol observation and replay analysis
- controlled display experiments
- monochrome image preparation workflows
- local tooling for research and interoperability testing

This README intentionally avoids deployment-oriented instructions and excludes guidance for interacting with live commercial systems.

## Features

- Text, image, and test-pattern display experiments
- Local web-based image preparation utility (`tools/tagtinker.html`)
- Signal and response testing for authorized bench hardware
- Small, modular codebase suitable for further research
- Research-first project structure with clear scope boundaries

## FAQ

**Where is the `.fap` release?**

The Flipper app is source-first. Build the `.fap` yourself from this repository with `ufbt` so it matches your firmware and local toolchain.

**What if it crashes or behaves oddly?**

The maintainer primarily uses TagTinker on Momentum firmware with asset packs disabled and has not had issues in that setup. If you are using a different firmware branch, custom asset packs, or a heavily modified device setup, start by testing from a clean baseline.

**What happens if I pull the battery out of the tag?**

Many infrared ESL tags store their firmware, address, and display data in volatile RAM (not flash memory) to save cost and energy.  
If you remove the battery or let it fully discharge, the tag will lose all programming and become **unresponsive ("dead")**. It usually cannot be recovered without the original base station.

**I found a bug or want to contribute — how can I get in touch?**

You can contact me on:
- Discord: **@i12bp8**
- Telegram: **@i12bp8**

I'm currently traveling, so response times may be slower than usual. Feel free to open issues or Pull Requests anyway — contributions (bug fixes, improvements, documentation, etc.) are very welcome and will help keep the project alive while I'm away.



## How It Works

TagTinker is built around the study of **infrared electronic shelf-label communication** used by fixed-transmitter labeling systems.

At a high level:

- tags receive modulated infrared transmissions rather than ordinary consumer-IR commands
- communication is based on addressed protocol frames containing command, parameter, and integrity fields
- display updates are carried as prepared payloads for supported monochrome graphics formats
- local tooling in this project helps researchers prepare assets and perform controlled experiments on authorized hardware

This project is intended to help researchers understand:
- signal structure
- frame and payload behavior
- display data preparation constraints
- safe, authorized bench-testing workflows

For the underlying reverse-engineering background and deeper protocol research, see:
- **Furrtek’s ESL research:** [https://www.furrtek.org/?a=esl](https://www.furrtek.org/?a=esl)
- **PrecIR reference implementation:** [https://github.com/furrtek/PrecIR](https://github.com/furrtek/PrecIR)

## Project Scope

TagTinker is limited to **home-lab and authorized research use**, including:

- infrared protocol study
- signal timing and frame analysis
- controlled experiments on owned or authorized hardware
- monochrome asset preparation for testing
- educational diagnostics and interoperability research

It is **not** a retail tool, operational tool, or field-use utility.

## Responsible Use

You are solely responsible for ensuring that any use of this software is lawful, authorized, and appropriate for your environment.

The maintainer does not authorize, approve, or participate in any unauthorized use of this project, and disclaims responsibility for misuse, damage, disruption, legal violations, or any consequences arising from such use.

If you do not own the hardware, or do not have explicit written permission to test it, **do not use this project on it**.

Any unauthorized use is outside the intended scope of this repository and is undertaken entirely at the user’s own risk.

## No Affiliation

This is an **independent research project**.

It is not affiliated with, endorsed by, authorized by, or sponsored by any electronic shelf-label vendor, retailer, infrastructure provider, or system operator.

Any references to external research, public documentation, or reverse-engineering work are included strictly for educational and research context.

## Credits

This project is a port and adaptation of the excellent public reverse-engineering work by **[furrtek / PrecIR](https://github.com/furrtek/PrecIR)** and related community research.

## License

Licensed under the **GNU General Public License v3.0** (GPL-3.0).  
See the [`LICENSE`](LICENSE) file for details.

## Warranty Disclaimer

This software is provided **“AS IS”**, without warranty of any kind, express or implied.

In no event shall the authors or copyright holders be liable for any claim, damages, or other liability arising from the use of this software.

## Maintainer Statement

This repository is maintained as a **narrowly scoped educational research project**.

The maintainer does **not** authorize, encourage, condone, or accept responsibility for use against third-party devices, deployed commercial systems, retail infrastructure, or any environment where the user lacks explicit permission.

**Research responsibly.**
