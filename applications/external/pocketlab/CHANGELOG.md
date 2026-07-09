# Changelog

All notable changes to this project are documented here. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

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
