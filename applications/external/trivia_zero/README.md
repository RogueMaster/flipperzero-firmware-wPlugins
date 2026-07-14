# Trivia Zero

**Flipper Zero** external application (**FAP**). Install it on the **microSD** of your Flipper and run it from **Apps → Games → Trivia Zero**.

Flashcard-style trivia: a question is displayed on screen, press OK to reveal the answer, press right to advance to the next random question. Two languages (Spanish and English) shipped by default. No score, no timer — just variety.

<p align="center">
  <img src="assets/screen1.png" width="256" alt="Trivia Zero question screen on Flipper Zero">
  <img src="assets/screen2.png" width="256" alt="Trivia Zero answer screen on Flipper Zero">
</p>
<p align="center"><sub>Question (left) — press <code>OK</code> to reveal the answer (right), then <code>▶</code> for the next one.</sub></p>

## Features

- **Bilingual** UI and content (ES + EN), language selection persisted on SD.
- **6 + 1 categories** (the classic 6 Trivial Pursuit categories plus "Cultura General"), shown as an inverted-video header above each question.
- **Random pool** with in-session anti-repetition.
- **Resume on relaunch** — the app remembers the last question you saw.

## Install on Flipper Zero

1. Build or download the `.fap` for this app.
2. Copy `flipper_trivia_zero.fap` to the SD card (e.g. `apps/Games/`).
3. On the Flipper: **Apps → Games → Trivia Zero**.

## Build

- **Host tests**: `make test` (gcc, no Flipper SDK).
- **FAP**: set `FLIPPER_FIRMWARE_PATH` to your firmware checkout, then `make prepare` and `make fap`.

## Requirements

- [flipperzero-firmware](https://github.com/flipperdevices/flipperzero-firmware) and `./fbt` for device builds.
