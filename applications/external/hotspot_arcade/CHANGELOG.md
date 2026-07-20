# Changelog

All notable changes to Hotspot Arcade are documented here. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.2.0] - 2026-07-18

Ten games, player identity, emoji reactions, an on-device ESP flasher, and firmware
versioning. Firmware **v6**.

### Added

- **Player identity**: pick an emoji avatar (16 options) on the landing screen; it shows in
  every player list, lobby, leaderboard, and podium.
- **Emoji reactions**: a corner button fires 👍😂🔥😮🎉❤️ that float up on every phone in any
  game (its own `{t:"emoji"}` message).
- **Four more games** (firmware v5): **Reversi/Othello** (8x8 flanking-capture engine on the
  duel system), and three whole-group party games — **Would You Rather** (live A/B poll),
  **Word Scramble** (unscramble-and-type race), and **Reaction Duel** (fastest-finger with
  false-start DQ).
- **Phone-driven trivia** (firmware v3): trivia is self-organizing and hosted entirely
  by the ESP. Players ready up and vote a topic in the lobby, an all-ready 5-second
  countdown starts the round, questions reveal when everyone answers or a timer expires,
  a collapsible leaderboard rides along, and a final podium ends it (with Play again). The
  Flipper streams every SD pack as a votable topic at session start.
- **Four games** on the shared engine: **Tic-Tac-Toe** and **Dots & Boxes** (1v1
  duels alongside Connect Four, with a rematch), **Drawing & guessing** (rotating drawer,
  stroke relay, chat), and real-time **Pong** (fixed-timestep physics tick).
- **Shared web components**: the whole-group games reuse one implementation each of the
  ready-up lobby, countdown, timer bar, collapsible live leaderboard, and final podium
  (`A.readyLobby` / `A.countdown` / `A.timebar` / `A.showLead` / `A.podium`).
- **Lobby chat**: a shared chat in the app lobby (and Draw & Guess), echo-deduplicated
  server-side.
- **Phone feel**: WebAudio sound effects, `navigator.vibrate` haptics, and micro-animations
  across the web client. Unified duel renderer (`web/games/duel.js`).
- **On-device ESP flasher**: flash the bundled firmware straight from the Flipper over the
  GPIO UART (Espressif `esp-serial-flasher`, Apache-2.0, S2-trimmed). Firmware ships inside
  the fap via `fap_file_assets`, so a fresh install needs no SD setup. Auto-reboots after
  flashing.
- **Firmware identity + versioning**: the beacon carries a project magic and version, so a
  different project's firmware is never mistaken for ours, and an outdated board is offered
  an update. A handshake watchdog prevents hanging when the board runs the wrong firmware.
- **App icon** and a repo logo; mobile-aspect web screenshots and Flipper device screenshots.
- **Release workflow**: tagging `v*` builds all artifacts and publishes a GitHub release.

### Changed

- Game selection is a pure selector; the dashboard hosts the active game. Dashboard layout
  redesigned (status header, grouped join info). "Install Firmware" flashes the bundled
  default directly (no file picker).

### Fixed

- Trivia countdown showed the first pack's name instead of the voted-for topic (the winning
  topic is now locked in when the countdown starts, so the name and questions agree).
- Countdown timer bars mis-calibrated because the ESP sends `deadline` in ms but `dur` in
  seconds; durations are now normalized to ms in one shared place.

## [0.1.0] - 2026-07-17

Initial release: offline multiplayer party games hosted from a Flipper Zero paired with
an official ESP32-S2 WiFi dev board. No internet and no app install required.

### Added

- **Flipper host app** (`flipper/hotspot-arcade`): sets the SSID, picks a trivia pack,
  starts the session, and drives rounds. Shows a live lobby, leaderboard, and raw event
  console. Built with `ufbt` against the Momentum SDK.
- **ESP32-S2 firmware** (`esp32/hotspot-arcade-fw`): brings up an open access point with
  wildcard DNS and a catch-all web server, serves the game bundle, and runs the real-time
  referee over one WebSocket per phone. Built with arduino-cli and the `esp32:esp32@2.0.17`
  core, with AsyncTCP and ESPAsyncWebServer vendored under `esp32/libs`.
- **Phone web client** (`web/`): a vanilla-JS, zero-dependency bundle in the Flipper Zero
  visual language (monochrome, mono type, one orange accent). Builds to a single gzipped
  file of roughly 7 KB.
- **Trivia game**: Kahoot-style rounds driven from a pack on the SD card. Phones answer
  A/B/C/D against a countdown, with points for correct and fast answers and a live
  leaderboard that scales to the whole group.
- **Connect Four game**: players challenge each other from their phones and play 1v1, with
  multiple matches running at once and wins scored on the Flipper leaderboard.
- **Framed UART v2 protocol**: `SYNC | type | len | payload | crc8` at 921600 baud, with a
  raw-bulk escape for streaming asset bytes. Type constants and the CRC-8 stay byte-for-byte
  identical on both the Flipper and ESP sides.
- **Streamed gzipped bundle**: the Flipper streams the compressed web bundle and trivia
  content to the ESP over UART; real-time game traffic stays on the ESP and never crosses
  the slow link.
- **Captive-portal join**: joining the open WiFi pops a captive page that hands off to the
  full browser at `http://192.168.4.1`, since captive mini-browsers do not run WebSockets
  reliably.
- **Trivia packs**: simple text format (`Pack:` / `Q:` / `A:`-`D:` / `Answer:`, blocks split
  by `---`) with sample packs under `trivia-packs/`.
- **Deploy tooling**: `tools/deploy-to-flipper.py` pushes the fap, web bundle, and trivia
  packs to the SD card over USB serial (md5-verified).
- **Mock server** (`web/mock-server.mjs`): a zero-dependency Node mock of the ESP referee
  for previewing the web client through lobby, trivia, and Connect Four in a desktop browser.
- **CI**: a build workflow that compiles all three parts on every push and pull request.

[Unreleased]: https://github.com/tarikbc/hotspot-arcade/compare/v0.2.0...HEAD
[0.2.0]: https://github.com/tarikbc/hotspot-arcade/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/tarikbc/hotspot-arcade/releases/tag/v0.1.0
