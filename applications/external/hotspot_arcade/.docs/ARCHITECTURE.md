# Architecture

Hotspot Arcade is three programs cooperating over two links:

```
 Phones (browser)        ESP32-S2 board            Flipper Zero
 -----------------       ----------------          ------------------
 web/ game client  <ws>  esp32/hotspot-arcade-fw   flipper/hotspot-arcade
   trivia.js             AP + wildcard DNS         host UI (scenes)
   connect4.js           catch-all HTTP (assets)   UART v2 (ha_uart)
   app.js (core)         AsyncWebSocket /ws        session/roster/rounds
                         game engine (referee)     trivia pack -> QUESTION
                              ^                          ^
                              +------ UART 921600 -------+
```

## The split: why the work lands where it does

The ESP32-S2 (240 MHz) is faster than the Flipper (64 MHz) **and** it is where the
phone sockets terminate, so all **real-time** state lives on the ESP. The Flipper has
the screen, the buttons, and the SD card, so it owns the **session/meta** layer. The
UART only ever carries low-frequency, high-level messages, never per-frame state.

| Flipper owns (session/meta)                         | ESP owns (real-time)                    |
|-----------------------------------------------------|-----------------------------------------|
| Lobby roster + live scoreboard (mirrored)           | WebSocket connections, socket<->player  |
| Which game is active, round flow (start/reveal/next)| Per-move / per-question game state       |
| Trivia question bank (SD) fed one round at a time    | Move validation, buzz timing            |
| Authoritative persistent scores, host display        | Broadcasting state to phones            |
| Streaming the web bundle to the ESP                  | Serving the bundle from RAM             |

Scores are computed by the ESP (it is the referee), broadcast to phones for display,
**and** reported to the Flipper via `SCORE`/`ROUND_RESULT` so the host screen and
leaderboard stay in sync. Both stay consistent because every delta is reported.

## ESP32 firmware (`esp32/hotspot-arcade-fw/`)

Single Arduino sketch plus header-only helpers (one translation unit):

- `hotspot-arcade-fw.ino` — AP/DNS/HTTP bring-up, the WebSocket `/ws` handler, the
  framed UART RX/TX, and the engine "sink" implementations. Two FreeRTOS mutexes:
  `serialMutex` serializes whole UART frames (emitted from the loop and async tasks),
  `engineMutex` (recursive) guards engine state touched from both tasks.
- `ha_assets.h` — in-RAM file table. The Flipper streams gzipped files in; the HTTP
  catch-all serves them (with `Content-Encoding: gzip`). No filesystem, so nothing
  survives a reboot; the Flipper re-streams on the next session.
- `ha_games.h` — the engine: player roster (with emoji avatars) plus all ten games and
  their per-client JSON serialization. The whole-group games (Trivia, Would You Rather,
  Word Scramble, Reaction Duel) are phone-driven and self-organizing (ready-up -> countdown
  -> rounds -> podium); Connect Four / Tic-Tac-Toe / Dots & Boxes / Reversi share one
  generalized duel + challenge system (parameterized by kind); Drawing rotates a drawer and
  relays ink; Pong and Reaction Duel run on `tick()` (called from the `.ino` loop) alongside
  the trivia/party/draw round timers. Emoji reactions broadcast to everyone as a `emoji`
  message. Trivia and the duels are event-driven; Pong is the real-time path.
- `ha_json.h` / `ha_proto.h` — tiny JSON reader/writer and the UART frame constants +
  CRC-8.

The web app is streamed into RAM (about 7 KB gzipped), so RAM stays flat regardless of
trivia pack size (questions are pushed one at a time, never stored in bulk).

## Flipper app (`flipper/hotspot-arcade/`)

A `ViewDispatcher` + `SceneManager` app, same shape as flytrap:

- `hotspot_arcade.c` — alloc/free, the UART-worker -> GUI custom-event bridge, and a 1s
  liveness tick (the ESP beacons `PING` ~every 2s; silence for 5s flags a lost board).
- `ha_uart.c` — the race-free UART transport (IRQ -> stream buffer -> worker -> GUI-thread
  parse), including the mandatory `expansion_disable()` dance before acquiring the GPIO
  USART. Runs at 921600.
- `ha_proto.c` — framed message encode.
- `helpers/ha_session.c` — the heart: the RX frame parser, the start **handshake**
  state machine (CLEAR_FILES -> stream bundle -> SET_AP -> START, driven by ESP acks),
  the roster (JOIN/LEAVE/SCORE), and trivia round orchestration (parse a pack question,
  build + send `QUESTION`, `REVEAL`, next).
- `helpers/ha_storage.c` — config (FlipperFormat), `manifest.json` parsing, binary-safe
  file reads (pre-reserved buffers to avoid an OOM-inducing 2x realloc peak), trivia
  pack loading.
- `scenes/` — main_menu, lobby (dashboard + start flow), game_select, host_duel (event feed
  for the player-driven games), leaderboard, settings, ssid_input, flasher, textview
  (console). Every game is phone-driven, so the Flipper only selects the game and watches
  the feed — there is no per-game host screen.
- `helpers/ha_esp_port.c` + `helpers/ha_flasher.c` + `scenes/..._flasher.c` — an on-device
  ESP flasher over the GPIO UART (Espressif `esp-serial-flasher`, Apache-2.0, vendored in
  `lib/esp-serial-flasher/` trimmed to the ESP32-S2 and ESP32 WROOM stubs). It borrows the serial line via
  `ha_uart_suspend`/`resume`, polls for download mode, flashes the SD firmware bundle with
  MD5 verify on a worker thread, and reboots the ESP into the new firmware.

All app state is single-threaded (mutated only on the GUI thread after RX is drained),
so there are no locks on the Flipper side.

## Web client (`web/`)

Vanilla JS, no framework, built into a single gzipped `index.html` (about 18 KB). `app.js`
owns the WebSocket, identity (nickname + emoji avatar in localStorage), lobby, screen
router, emoji reactions, and the shared game-UI components (`A.readyLobby` / `A.countdown`
/ `A.timebar` / `A.showLead` leaderboard / `A.podium`). Each game module (`trivia.js`,
`duel.js` for the four board duels, `draw.js`, `pong.js`, `wyr.js`, `scramble.js`,
`react.js`) registers handlers for its message types and reuses those components. Styled to the Flipper design
system ([../web/DESIGN.md](../web/DESIGN.md)): dark, monochrome, one orange accent,
mono/uppercase, sharp borders. The captive page is a real-browser handoff because iOS/
Android captive mini-browsers do not run WebSockets reliably.

## Wire protocols

Both links are specified in [PROTOCOL.md](PROTOCOL.md): the framed UART v2 (with the
raw-bulk escape used to stream files) and the WebSocket JSON. The protocol is the source
of truth; all three programs are kept in sync with it.

## Data flow: a trivia round

1. Host picks a pack (SD) and Start Session. Flipper streams the bundle to the ESP,
   which brings up the AP. Phones join, `hello` -> `welcome`/`lobby`.
2. Host selects Trivia. Flipper parses question N from the pack, sends `QUESTION`.
3. ESP broadcasts `trivia` state; phones tap; ESP validates + times each `answer`,
   reports live `EVENT {answers,total}` to the Flipper.
4. Host taps Reveal. Flipper sends `REVEAL`; ESP scores correct answers (speed bonus),
   reports `SCORE` per player + `ROUND_RESULT`, broadcasts the reveal.
5. Host taps Next -> back to step 2, until the pack ends (`ROUND_END`).
