# PocketLab
<div align="center">
<img src="screenshots/demo_v1.2.gif" alt="PocketLab walkthrough" width="520">

<br/>

**Gamified, on-device learning for [Flipper Zero](https://github.com/flipperdevices).**

[![Flipper Apps Catalog](https://img.shields.io/badge/Flipper%20Apps%20Catalog-PocketLab-FF8200)](https://lab.flipper.net/apps/pocketlab)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Latest release](https://img.shields.io/github/v/release/PerfectoWeb/flipper-pocketlab?color=FF8200&label=release)](https://github.com/PerfectoWeb/flipper-pocketlab/releases/latest)
[![Flipper](https://img.shields.io/badge/Flipper-Official%20FW-FF8200)](https://flipperzero.one/)

</div>

---

## 📚 What is it?

The biggest problem in the Flipper ecosystem is new-user churn: people buy a
[Flipper Zero](https://github.com/flipperdevices), don't know where to start, get
bored, and resell it. **PocketLab is the missing onboarding layer** – a guided,
legal and genuinely fun way to learn what your Flipper can do, one small lab at a
time.

## ✨ Features

- 🎓 **36 labs** across 11 tracks: IR, Sub-GHz, RFID, NFC, iButton, Bad USB,
  GPIO, Bluetooth, security, system and more – including **advanced deep-dives**
  (MIFARE key recovery / MFKey32, T5577 cloning, IR protocols, and more)
- 🎲 **Randomized quizzes** – a pool of distractors, shuffled every attempt, so
  you learn the answer, not its position
- 🧠 **Quiz mode** – a random exam drawn from the labs you've completed, scored
  at the end, to test what stuck
- 🏆 **Progression** – XP, levels, a daily streak and unlockable badges, all
  saved on the SD card
- 🖼️ **Per-topic glyphs** next to each lesson title, so the subject reads at a glance
- 🔊 **Sound & LED** – audio feedback with the RGB LED blinking to the reward tune
- 🖥️ **Custom animated UI** – tile menu, a badge gallery, stat cards and a
  matrix-rain About screen
- 📦 **No extra hardware** – pure software, runs on a stock Flipper Zero


## 📸 Screenshots

<div align="center">

| Menu | Labs | Quiz |
|:---:|:---:|:---:|
| <img src="screenshots/menu.png" width="240"> | <img src="screenshots/labs.png" width="240"> | <img src="screenshots/quiz.png" width="240"> |
| **Profile** | **Achievements** | **About** |
| <img src="screenshots/profile.png" width="240"> | <img src="screenshots/achievements.png" width="240"> | <img src="screenshots/about.png" width="240"> |

</div>

## 📥 Installation

### A. Flipper Apps Catalog (easiest)

Open **[PocketLab on the Flipper Apps Catalog »](https://lab.flipper.net/apps/pocketlab)**
and hit **Install** from the Flipper mobile app or qFlipper, or find it right on
the device under **Apps → Tools**.

### B. Manual install

1. Download **`pocketlab.fap`** from the
   [latest release »](https://github.com/PerfectoWeb/flipper-pocketlab/releases/latest/download/pocketlab.fap)
2. Copy it to your Flipper's SD card into `apps/Tools/`
3. On the device open **Apps → Tools → PocketLab** 🎉

### C. Build from source (ufbt)

<details>
<summary>Show build instructions</summary>

[`ufbt`](https://pypi.org/project/ufbt/) builds Flipper apps without a full
firmware checkout:

```sh
pipx install ufbt # or: pip3 install --user ufbt

git clone https://github.com/PerfectoWeb/flipper-pocketlab.git
cd flipper-pocketlab

ufbt # builds dist/pocketlab.fap
ufbt launch # build, upload to a connected Flipper, and run
```

The build output lands in `dist/pocketlab.fap`.

</details>

## 🧩 Create your own lab

Labs are **data, not code**. Add a `PocketLabStep` array and a `PocketLabLab`
entry in [`helpers/pocketlab_content.c`](helpers/pocketlab_content.c) – the engine
renders `text`, `quiz`, `try` and `reward` steps for you. No engine changes needed.

## 🏗️ Architecture

Standard Flipper app structure: a `ViewDispatcher` driving a `SceneManager`, with
content and state kept as plain data.

```text
pocketlab.c              App lifecycle, XP/level/award logic, entry point
pocketlab_i.h            Shared app context and public helpers
scenes/                  SceneManager scenes (menu, labs, lesson, progress, badges,
                         levelup, exam, settings, reset_confirm, about)
views/                   Custom animated views (home, lesson, labs list, progress,
                         badges, levelup, exam, about)
helpers/
  pocketlab_content.*    Labs as data (step arrays + topic glyph)
  pocketlab_i18n.*       UI string table
  pocketlab_sound.*      Notification sequences
  pocketlab_storage.*    Versioned save/load on the SD card
```

## 💬 Support & Contributions

- 💬 Found a bug or have a feature request? [Open an Issue](https://github.com/PerfectoWeb/flipper-pocketlab/issues)
- ⭐ Like the project? [Star the repo](https://github.com/PerfectoWeb/flipper-pocketlab)! [Give a coffee](https://perfecto-web.com/d/)!
- 🛠 Want to contribute? [Fork it](https://github.com/PerfectoWeb/flipper-pocketlab/fork) and submit a pull request.

## 📝 License

MIT – see [LICENSE](LICENSE). 
Made with ♥ by [PerfectoWeb](https://github.com/PerfectoWeb).
