# Changelog

All notable changes to Hotspot Arcade are documented here. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.1.2] - 2026-07-22

Third board: **ESP32-C5**. From @xMasterX, tested on C5 hardware. Firmware **v11**
(no protocol change).

### Added

- **ESP32-C5 support.** Same sketch builds for the C5 (against a 3.x esp32 core; S2/WROOM
  stay pinned to 2.0.17), with C5 rows in the board picker and a `c5-merged.bin` on the
  release for computer flashing. The bootloader sits at 0x2000 on this chip.
- The flasher falls back to the plain ROM protocol for chips the stub loader doesn't cover
  (C5/P4), skipping the `FLASH_END` the C5 ROM rejects and rebooting with a DTR pulse. The
  S2/WROOM stub path is unchanged. Flash errors now name the stage, image, and route.

### Changed

- Bundled firmware images use short names (`ha-boot-<board>.bin`, etc.) so they fit the
  Flipper's screen while flashing.
- The fap grows to ~3 MB (a third firmware is bundled), so the **first launch can take up
  to 3 minutes** while the Flipper unpacks it. Docs and the release notes say so.

## [1.1.1] - 2026-07-22

On-device flashing improvements from @xMasterX, who tested on an ESP32 WROOM board.
Firmware **v11** (no protocol change).

### Added

- **One-click flashing.** The board picker now offers an "(auto boot)" row for each board
  that pulses the DTR/RTS reset lines (and power-cycles over OTG) to drop the board into
  download mode on its own — so a board with no reset button flashes in a single tap. The
  manual "hold BOOT, tap RESET" rows stay as the fallback for boards wired differently.

### Fixed

- Pressing Back in the flasher no longer freezes the UI. A cancel flag is threaded into the
  loader's blocking waits (chunked to 50 ms) so the scene's exit unwinds promptly instead of
  sitting through a multi-second library timeout.

## [1.1.0] - 2026-07-22

Multi-board support. Firmware **v11** (no protocol change).

### Added

- **Flash more than one ESP board.** "Install Firmware" now opens a board picker — the
  official Flipper WiFi Dev Board (ESP32-S2) or an ESP32 WROOM board. Each board's
  firmware is bundled, so it's still an offline, no-computer flash. Adding a board later is
  one picker row plus one asset folder. Thanks to @Tyl3rA (#2) for the WROOM support this
  builds on.

### Changed

- `tools/build-fap.sh` and CI now build the firmware once per board, so the bundled images
  can't silently go stale.
- The fap grows to ~1.9 MB (a second full firmware is bundled). Assets stream to SD, so
  it's RAM-neutral, but the **first launch can take up to 2 minutes** while the Flipper
  unpacks everything. Docs and the release notes say so.
- Docs describe multi-board support (README, catalog listing), and drop the "Feed" screen
  removed in 1.0.1.

## [1.0.1] - 2026-07-22

Post-1.0 fixes from on-device testing. Firmware **v11** (no protocol change).

### Added

- An angry reaction emoji (😠).
- Lobby chat now appears on the Flipper's Console, so the host can follow the chatter.

### Fixed

- **Tic-Tac-Toe** stays a proper square on mobile Safari instead of stretching wide —
  the cell drives the square (like Reversi/Connect Four) rather than relying on the
  board's grid rows, which WebKit didn't size reliably.
- **Rematch after your opponent leaves** now returns you to the lobby with an "Opponent
  left" toast, handled in the engine (the earlier client-side timeout was unreliable on
  hardware).
- **You can no longer challenge someone still on their win/lose screen.** Players in a
  1v1 match are marked busy and hidden from the challenge list until they return to the
  lobby.

### Removed

- The Flipper dashboard's redundant **Feed** screen. Games are player-driven and the
  main-menu **Console** already shows the full live event log.

## [1.0.0] - 2026-07-22

First stable release. Firmware **v11**.

### Added

- **Local browser simulator** (`sim/`): the real ESP game engine compiled to WebAssembly
  and driven by 2-8 phone panels (each an iframe of the real phone client) plus a
  data-faithful Flipper panel, so every game is playable and testable with no hardware. A
  CI job compiles the engine so the WASM shim can't rot silently.
- **Generic content packs for four games.** Trivia, Would You Rather, Word Scramble, and
  Draw & Guess all play from plain-text `packs/<game>/*.txt` files. The Flipper is now a
  dumb pipe that no longer parses game content, so adding a content-driven game is a pack
  file plus one engine loader. The party games share a pack-vote strip in the lobby.
- **More content**: six packs per game now ship inside the .fap (14 new packs spanning
  geography/music/games trivia, superpowers/time & space/absurd Would You Rather,
  food/space/music/sports scramble, and food/nature/animals/fantasy draw).
- **Change your nickname and avatar mid-game** from the header identity chip.
- **Captive-browser handoff.** When a phone opens the game inside the iOS/Android Wi-Fi
  popup (where WebSockets can't hold a connection), an overlay guides the player to their
  real browser with a copiable `192.168.4.1` address.

### Changed

- **Reactions are scoped** to the people sharing your screen and now show the sender's name.
- **Countdowns start from 3** across the party games, with a vote-and-reveal countdown bar
  in Would You Rather.
- Firmware advanced **v7 -> v11** over this release (scoped reactions, generic content
  ingest, themed packs, and the 3-second countdowns each bumped it).
- README and docs: looping gameplay GIFs, regenerated screenshots, and pack documentation
  that covers all four content games instead of trivia alone.

### Fixed

- **Reversi and Connect Four no longer collapse on mobile Safari.** The boards set explicit
  grid rows instead of relying on cell aspect-ratio, which WebKit sized to near-zero so
  discs and pieces from different rows overlapped.
- **Tic-Tac-Toe** no longer renders a squashed, cut-off board; marks are proper crosses and
  circles, optically centred.
- **Rematch** after your opponent has left now returns you to the lobby instead of doing
  nothing.
- **Would You Rather** lets you vote again after a skipped round, and no longer serves a
  stale prompt pack after a re-clear with no replacement.
- The **Pong** ball now actually makes contact with the paddle.

## [0.3.0] - 2026-07-21

Install is now a single file. Firmware **v7**.

### Added

- **The .fap bundles everything**: the phone web bundle and the trivia packs ship inside it
  alongside the ESP firmware, so installing just `hotspot_arcade.fap` gives a playable app
  with no SD setup. This also fixes the app-catalog build, which builds the Flipper subdir
  alone and so had no web bundle to serve at all.
- **User content still wins**: `/ext/apps_data/hotspot_arcade/trivia/*.txt` is offered
  alongside the bundled packs (yours wins a name clash), and a `manifest.json` under
  `.../web/` replaces the bundled client outright. `apps_assets` is rewritten from the .fap
  on every launch, so user content deliberately lives in `apps_data`, which is never touched.
- **The flasher continues on its own** once the freshly-flashed board reboots, so tapping
  RESET is the only step. Continue remains for the failure path.

### Changed

- **Nicknames are shown in all caps everywhere.** Uppercased once on the ESP as the player
  joins, so the phone UI, the Flipper roster, and the strings the engine composes itself
  ("A vs B", "X got it", challenge toasts) all agree. ASCII only, so accented and emoji
  nicknames pass through intact.
- The flasher's done screen no longer claims the board reboots by itself. It always needs a
  RESET tap, and the message is two lines so it can't render under the Continue button.

## [0.2.1] - 2026-07-21

### Fixed

- **The released 0.2.0 `.fap` did not actually contain the ESP firmware**, so "Install
  Firmware" could not work for anyone who installed it from the GitHub release. The
  firmware images are bundled via `fap_file_assets`, but they were untracked build
  artifacts at the 0.2.0 tag, so the CI `.fap` was built without them (68 KB instead of
  ~890 KB). The images are now committed, and the 0.2.1 `.fap` carries them. Anyone on
  0.2.0 should reinstall the app, or flash the board from a computer.
- Release workflow: `esptool` was resolved with a glob that matches the Python *package
  directory* on Linux, so the `merge_bin` step ran `__init__.py` and failed the release.

### Changed

- Flipper sources reformatted to the official `clang-format` (app-catalog lint).
- App-catalog metadata: manifest, changelog, and qFlipper screenshots.
- README refreshed with screenshots and a fixed build badge.

No gameplay or protocol changes. ESP firmware is still **v6**.

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

[Unreleased]: https://github.com/tarikbc/hotspot-arcade/compare/v1.1.2...HEAD
[1.1.2]: https://github.com/tarikbc/hotspot-arcade/compare/v1.1.1...v1.1.2
[1.1.1]: https://github.com/tarikbc/hotspot-arcade/compare/v1.1.0...v1.1.1
[1.1.0]: https://github.com/tarikbc/hotspot-arcade/compare/v1.0.1...v1.1.0
[1.0.1]: https://github.com/tarikbc/hotspot-arcade/compare/v1.0.0...v1.0.1
[1.0.0]: https://github.com/tarikbc/hotspot-arcade/compare/v0.3.0...v1.0.0
[0.3.0]: https://github.com/tarikbc/hotspot-arcade/compare/v0.2.1...v0.3.0
[0.2.1]: https://github.com/tarikbc/hotspot-arcade/compare/v0.2.0...v0.2.1
[0.2.0]: https://github.com/tarikbc/hotspot-arcade/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/tarikbc/hotspot-arcade/releases/tag/v0.1.0
