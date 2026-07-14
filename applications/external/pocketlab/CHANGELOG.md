# Changelog

All notable changes to this project are documented here. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [1.5] - 2026-07-14

### Added

- **Russian language.** The full interface plus all 36 labs and quizzes are
  translated; pick the language in Settings. A bundled font renders Cyrillic on
  device (English keeps the native system font).
- A **unique badge icon for every lab**, so achievements are easy to tell apart.
- **Quizzes now award XP** (per correct answer). Total XP is capped at 9999.
- Long lab and quiz titles **scroll** so they can be read in full.
- The XP counter goes glitchy once you max it out.

### Changed

- The Profile screen uses the full display width; the level/XP cards and the
  headings are realigned.
- Home tiles and several labels were resized and repositioned for a cleaner fit.

### Fixed

- About screen polish: the version/author icons sit correctly, the coffee button
  no longer overlaps the repo link, and the description wraps cleanly.

## [1.4] - 2026-07-12

### Added

- Separate **LED** and **Vibro** toggles in Settings (both default on),
  independent of the Sound toggle – turn light or vibration off globally while
  keeping the others.

### Fixed

- The About screen now always shows the current version. The version lives in a
  single place that the app binary, the catalog and the About screen all read.

## [1.3] - 2026-07-09

### Added

- **6 advanced labs** that go deeper into each subsystem, with extra quizzes:
  - **MIFARE Keys** – sector keys, the key dictionary, MFKey32 nonce recovery
    and nested/hardnested attacks.
  - **T5577 Cloning** – writing 125 kHz blanks and why LF access control is weak.
  - **IR Protocols** – NEC/RC5, parsed vs raw signals and learning a button.
  - **iButton Cloning** – RW1990 blanks, the Dallas ROM and its CRC.
  - **Advanced Payloads** – OS-aware Bad USB scripts, modifiers and detection.
  - **Logic & Buses** – sniffing UART/SPI/I2C and the 3.3V logic level.
- Content is now **36 labs**; every new quiz feeds the Quiz-mode pool.

### Changed

- Selected home tiles now show a calm "breathing" cursor instead of the
  twinkling-pixel effect.
- Moving through the Quiz answers and the Labs list now plays a quiet click.

### Fixed

- The Bluetooth glyph now pinches at the waist, matching the real logo.

## [1.2.1] - 2026-07-09

### Fixed

- Home screen scroll hints are back at the top and bottom of the grid as small
  3px chevrons, instead of one arrow in the centre.

## [1.2] - 2026-07-09

### Added

- **Quiz mode**: a random exam drawn from the quizzes of your completed labs,
  with an A/B/C answer sheet and a score at the end.
- **Badge gallery**: browse earned and locked badges, each with a unique jingle
  and a targeting reticle over the selection.
- **Daily streak** tracked and shown on the Profile screen.

### Changed

- Content expanded to **30 labs across 11 tracks**.
- Home screen redesigned as a scrollable tile grid with per-item icons.
- About reworked: a matrix-rain intro, a self-typing terminal, and a coffee-break
  page with a donation QR code and an animated mascot.
- Level Up now rains confetti with a richer fanfare; finishing a step plays a
  lighter, distinct chime, kept separate from the level-up tune.
- Resetting progress plays a short "cleared" flash; wrong quiz answers shake and
  buzz for clearer feedback.
- Polish throughout: coin-flip navigation clicks, a winking logo, and twinkling
  pixels on the selected tile.

## [1.1] - 2026-07-08

### Changed

- About screen now links to github.com/PerfectoWeb.
- Presentable README: theme-aware banner, screenshot gallery, contribute section.

## [1.0] - 2026-07-08

First public release.

### Features

- **11 labs** covering the popular Flipper features: IR, Sub-GHz, 125 kHz RFID,
  NFC, iButton, Bad USB, GPIO/UART, Bluetooth LE, U2F, ethics and law, firmware
  and apps.
- **Bite-sized flow** per lab: intro, concept, quiz, hands-on, reward.
- **Randomized quizzes**: each question keeps a pool of distractors and shows the
  correct answer plus two random ones, shuffled so the position is never a hint.
- **Progression**: XP, levels and unlockable badges, saved on the SD card.
- **Per-topic glyphs** shown next to the lesson title so the subject reads at a
  glance.
- **Custom UI**: two-level menu with a branded header, a lab list with completion
  indicators, and a stat-card "My progress" screen.
- **Reward screen** with an animated medal and XP count-up.
- **Sound and LED**: optional audio feedback, with the RGB LED blinking in time
  with the reward melody.
- **Animated About** screen styled as a fake terminal session.
- **Settings**: sound toggle and "Reset progress" with confirmation.
- Builds against the official Flipper SDK for catalog compatibility.
