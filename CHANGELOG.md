# Changelog

All notable changes to Hotspot Arcade are documented here. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.7.0] - 2026-08-05

### Added

- **Secrets**, a 16th game (whole-group). Each round shows a yes/no question and runs
  answer → predict → reveal: every player first secretly answers yes/no, then secretly
  predicts how many of the group said yes (0..N). Only the group's total yes-count is ever
  revealed — the individual answers are never serialized to anyone, enforced server-side in
  the per-player serializer; predictions (guesses about the group, not personal) are shown
  at reveal. An exact prediction scores 1, anything else 0. Six rounds, on the shared
  party lobby/countdown/reveal skeleton, with a votable pack of questions. Ships localized
  (English + German). Firmware **v18**.
- **Phone-side game-change vote.** Any player can propose switching the active game from
  their phone (a 🕹️ button in the header, and the lobby's "Pick a game" row, open a
  game picker); the ESP then freezes the active game and runs a majority vote of the
  *other* players, resuming the game on reject/timeout or switching on approve. The
  proposer is an implicit yes; approval needs a strict majority of the others (a lone
  proposer switches at once); a No majority or a 25s timeout rejects, the proposer can
  withdraw their own proposal, and the proposer leaving cancels it. The picker's last
  entry, **Back to Lobby**, proposes leaving the current game and is voted on the same
  way. This is the one sanctioned phone→host action, gated entirely behind the vote — a
  host-initiated select stays authoritative and immediate. New intents
  `proposeGame{game}` / `voteGame{ok}` and a `gamevote` push. Firmware **v18**.
- **Would You Rather: an agreement chart on the final screen.** The game used to end with
  nothing to talk about; now it closes with a distribution of how strongly the group
  agreed. Agreement per round is the majority share (`max(a,b)/(a+b)`), so a 1/9 split
  reads as 90% just like 9/1 and the value never drops below 50%. The horizontal axis is
  the set of percentages actually reachable with the current number of voters
  (`ceil(n/2)/n … n/n` — 50/60/70/80/90/100 for ten players), each bar counts the rounds
  that landed there, and a dashed line marks the mean with its value, e.g. "You align on
  average by 62% — lots to talk about!". Rounds nobody voted in are skipped rather than
  counted as unanimous. The ESP is the source of truth: `wyrJson()`'s `"final"` phase now
  carries `voters` and a `rounds` array of `{a,b}` splits, because a phone that joined
  late never saw the earlier rounds. Firmware **v18**.
- A serial trace of every identity decision, for debugging on real hardware:
  `[ha] JOIN pid=2 ip=192.168.4.3 mac=AA:BB:CC:DD:EE:FF nick="..."` for a new device, and
  `[ha] NEW BROWSER same device ip=... mac=... -> pid=1 nick="..." (consolidated)` when a
  second browser context is folded onto the player that phone already has.

### Changed

- **The web bundle lives in ESP flash, not RAM.** It used to be streamed from the Flipper
  into a heap buffer every session and held there; it is now written once into a
  **LittleFS** flash partition (the `spiffs`-labelled data partition every board already
  reserves) as it streams, and served from flash — freeing ~47 KB of heap. The ESP
  advertises the stored bundle's **CRC32** in its PING beacon and the Flipper **skips
  re-streaming** when its copy already matches (a changed bundle, or a user override in
  `apps_data/.../web`, still streams). Firmware **v19**; the PING beacon and `manifest.json`
  gained a bundle-CRC field.
- Content packs now allow **8 topics per game**, up from 6, using some of the heap the
  flash bundle freed.

### Fixed

- **The AP no longer exhausts RAM and reboots when phones associate.** The ~47 KB web
  bundle grew across sixteen games; held in heap it left too little for the Wi-Fi/TCP
  stack, so a couple of Wi-Fi associations could crash the board and drop the AP mid-join
  ("unable to join" / "192.168.4.1 times out"), worst in RF-dense areas. Moving the bundle
  to flash (above) fixes it. Firmware **v19**.
- **A phone-voted game change now shows on the Flipper.** Switching the game from a phone
  updated the ESP and the other players but left the Flipper's dashboard showing the old
  game (and an ESP reboot could revert the vote). The PING beacon now carries the ESP's
  current game id and the Flipper mirrors it while hosting, so any game change — by vote or
  by the host — reflects on the dashboard within one beacon. Firmware **v19**.
- **The phone game plays in the iOS captive pop-up, not just the full browser.** The captive
  window is served the real app now (it's a WebKit view), while the OS's background
  captive-detection probes get a tiny "open 192.168.4.1 in your browser" landing — so
  serving the ~47 KB bundle to the burst of probes no longer starves the ESP's heap.
- **Would You Rather's agreement chart no longer misreads a divided group.** With few voters
  a round can only land at 50% or 100%, so a half-unanimous/half-split game averaged to a
  meaningless "75% — like-minded" with a marker in an empty gap. It now detects that
  polarization, drops the mean marker, and says "Split down the middle — N unanimous, M
  split." Localized (en/de/pt-BR).
- **One phone = one player.** A player is now bound to the device rather than to the
  WebSocket, so a phone can no longer show up as two or three players. iOS opens the portal
  in a captive mini-browser whose storage is separate from Safari's, so playing in both (or
  in a second tab, or a second browser) used to create a player per context. A `hello` from
  a device that is already playing now rebinds the existing player to the new socket — never
  a second player — keeping their pid, name, avatar and score, and the `welcome` reply hands
  that identity to the new context (it now carries `avatar` alongside `pid`/`nick`, and the
  client adopts and stores both) so it shows the same name straight away. Firmware **v18**.

  The device is identified by the station's **MAC**, not by its IP: the IP is assigned by
  the ESP's own DHCP server and is a derived value, while the MAC is the device. The AP's
  DHCP events report the assigned address together with the client MAC, so the firmware
  keeps a small IP → MAC table; a station whose lease predates the handler is resolved from
  lwIP's ARP cache, and if the MAC cannot be resolved at all the IP is used as the key.
  (Phones use a randomized private MAC these days, but it is stable per SSID, so it lasts
  exactly as long as a session does.) The engine itself just stores an opaque 64-bit device
  key and knows nothing about MACs or IPs.
- **No more ghost players from a stale socket.** A phone that drops (screen lock, WiFi off)
  closes nothing — the ESP only sees the socket die when TCP times out, minutes later. A
  disconnect now removes a player only if the closing socket is still that player's
  *current* socket, so a phone that reconnected in the meantime is no longer taken down by
  its own stale connection.

  Deliberate trade-off: two people can no longer share one phone as two players.
- **A dead link now shows the reconnect bar** instead of the page looking connected while
  the round moves on. A phone dropping (WiFi off, sleep) often produces no socket close for
  minutes; the client now tracks how long the host has been silent and surfaces the existing
  reconnect UI once it goes quiet. Client-only.

### Changed

- `sim/`: each simulated socket now carries a stub device key (one per socket, so every
  panel stays its own phone), with `ha_ws_device()` to put two sockets on one phone. New
  headless test `sim/test/identity.mjs` covers rebind, distinct devices, the stale-socket
  disconnect, and the unknown-device fallback.

## [1.6.0] - 2026-08-03

Chess joins as the fifteenth game. Firmware **v17**.

### Added

- **Chess**, a 15th game (1v1). Full FIDE rules refereed on the ESP: legal move
  generation, check/checkmate/stalemate, castling, en passant, and underpromotion.
  Threefold repetition and the 50-move rule are claimable, fivefold repetition and the
  75-move rule end the game automatically, and a flag fall loses unless the opponent
  couldn't mate by any legal sequence (FIDE 6.9), which draws instead. A 5+0 blitz clock
  per side, server-authoritative. Ships fully localized (English + Brazilian Portuguese)
  from day one. Firmware **v17**.
- A **keep-awake helper** on the phone client: the Wake Lock API where available, falling
  back to a looping muted video where it isn't (the AP's captive page is served over an
  insecure context, so `navigator.wakeLock` is absent there) — keeps the screen on through
  a long chess clock or any other match.

### Fixed

- `selectGame()`'s clear-chain was missing `gcClear()` and `battleClear()`, so switching
  the active game left live Guess the Color and Battleship matches occupying their slots.
  Fixed alongside adding the new `chessClear()`.
- Battleship's "A vs B" match `EVENT` never reached the Flipper: the UART dispatcher's
  event key chain checked `duel`/`pong`/`draw` but not `bs`. Added (alongside `chess`).

## [1.5.0] - 2026-08-02

Full phone-UI localization and per-language content packs, with Brazilian Portuguese as
the first language. Firmware **v16** (the board relays the host's language to each phone).

### Added

- **Phone UI localization.** The host's chosen language is relayed to each phone (the ESP
  echoes it in `welcome`), and the client renders from a message catalog with English as
  the fallback for any untranslated string. The whole phone UI is localized, static labels
  and in-game dynamic text alike, across all fourteen games (**Brazilian Portuguese** is
  the first language). Server-sent toasts stay English. Firmware **v16** (the board relays
  the language). The Flipper's own menus stay English (host-facing, and the Flipper font
  has no accented glyphs).
- **Content languages.** The host picks a content language in Settings, and each game
  streams that language's packs from `packs/<game>/<lang>/`, falling back to English per
  game where a language has none (so a partly translated language still plays). English is
  the unchanged default. **Brazilian Portuguese (pt-BR)** is the first, with a starter pack
  for every content game. Adding a language is a set of pack files plus one row in the
  settings picker.
- **German (de).** A full German translation of all six content games — every pack for
  Trivia, Would You Rather, Spectrum, Kiss Marry Kill, Word Scramble and Draw (32 packs).

### Fixed

- **Spectrum and Kiss Marry Kill now get their content packs on hardware.** The Flipper's
  pack streamer only covered trivia/wyr/scramble/draw, so those two games started empty on
  a real board (the simulator streamed them, which hid it). Both are streamed now.
- Word Scramble and Draw are now UTF-8-safe, so non-English content packs work: the
  scramble shuffles whole letters (not bytes), the Draw blank count is per character, and
  the scramble tiles upper-case ASCII only (so a German `ß` stays one tile). ASCII content
  is unchanged. From @genkigenki
  ([#9](https://github.com/tarikbc/hotspot-arcade/pull/9)) — the enabler for language packs.

## [1.4.0] - 2026-08-02

A 14th game. Firmware **v15**.

### Added

- **Kiss Marry Kill**, a 14th game (whole-group), contributed by @genkigenki
  ([#8](https://github.com/tarikbc/hotspot-arcade/pull/8)). Each round a rotating chooser
  secretly labels three people (drawn from the pack) Kiss, Marry, and Kill; everyone else
  predicts the chooser's assignment. Points for matching positions, and the chooser scores
  by how well the group reads them. Six rounds, four content packs. Firmware **v15**.

## [1.3.0] - 2026-08-01

A 13th game. Firmware **v14**.

### Added

- **Spectrum**, a 13th game (whole-group), a Wavelength-style guessing game contributed by
  @genkigenki ([#7](https://github.com/tarikbc/hotspot-arcade/pull/7)). Each round one
  player is the psychic: they see a hidden target on a 0-100 spectrum between two opposing
  words and type a clue; everyone else slides a dial to guess where it lands. Points by
  closeness, and the psychic scores by how well the group guesses. Six rounds, rotating
  psychic, four content packs. Reuses the whole-group party skeleton and the pack pipeline.
  Firmware **v14**.
- A contributor guide (`CONTRIBUTING.md`), a PR template, and a `pr-checklist` CI action that
  reviews PRs against the "adding a game" checklist.

## [1.2.0] - 2026-07-31

Two new games bring the count to **twelve**. Firmware **v13**.

### Added

- **Guess the Color**, an 11th game (whole-group). A random color swatch appears and
  everyone dials in its R/G/B with a slider per channel; closest guess wins the round with
  a speed bonus, scored **0-10**, over five rounds to a podium. You never see your own color
  while guessing — the reveal then lines up every player's guess beside the answer as a
  split swatch (answer on the left, that player's guess on the right) so you can see how
  close each was. Firmware **v12**.
- **Battleship**, a 12th game (1v1). Place a hidden fleet of five ships on a 10x10 grid,
  then fire at the enemy grid; a hit lets you fire again, and sinking all five ships wins.
  Distinct hit / miss / sunk sounds, an orange border on your grid when it's your turn, and
  the enemy fleet revealed at the end. Firmware **v13**.

### Changed

- The official ESP32-S2 dev board is manual-only in the flasher again: its reset lines
  aren't wired to the pins the auto-boot pulse drives, so the "(auto boot)" row never
  dropped it into download mode. WROOM and C5 keep both their auto-boot and manual rows.

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
