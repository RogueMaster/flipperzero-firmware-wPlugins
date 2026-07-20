# Hotspot Arcade — wire protocols

Two links, two protocols. This file is the source of truth; the Flipper app, the
ESP32 firmware, and the web client must all agree with it.

```
 Flipper Zero  <--- UART v2 (framed) --->  ESP32-S2  <--- WebSocket (JSON) --->  Phones
  host brain                                referee                               players
```

- **UART v2** carries only low-frequency session/meta traffic (asset upload, AP
  control, round orchestration, join/leave, score deltas). Never per-frame state.
- **WebSocket JSON** carries the real-time game traffic, local to the ESP.

---

## 1. UART v2 (Flipper <-> ESP32)

- **115200-safe wiring, run at 921600 8N1** on the Flipper GPIO USART (pins 13/14).
  Fallback 460800 if the trace is noisy. Baud is a build constant on both sides.
- Same expansion-service dance as flytrap: `expansion_disable()` **before**
  `furi_hal_serial_control_acquire()`, `expansion_enable()` after release.

### 1.1 Control frame

All control messages are framed so the link can resync after noise:

```
+------+------+---------+-----------------+------+
| SYNC | TYPE | LEN(2)  | PAYLOAD(LEN)    | CRC8 |
+------+------+---------+-----------------+------+
 0xA5    1B    LE u16     LEN bytes         1B
```

- `SYNC = 0xA5`.
- `TYPE` = one of the message types below.
- `LEN` = payload length, little-endian u16 (0..4096). Payloads are capped at
  **4096** bytes; larger data (asset files) uses the raw-bulk escape (1.3).
- `CRC8` = CRC-8/ATM (poly 0x07, init 0x00) over `TYPE || LEN || PAYLOAD`.
- On a bad CRC or unknown type, the receiver drops the frame and rescans for the
  next `0xA5`.

### 1.2 Message types

**Flipper -> ESP**

| Type | Name         | Payload |
|------|--------------|---------|
| 0x10 | CLEAR_FILES  | (none) — drop all stored assets, start of session |
| 0x11 | FILE_BEGIN   | `flags(1)` `pathlen(1)` `path` `mimelen(1)` `mime` `total(4 LE)` — then `total` **raw** bytes follow (see 1.3). `flags` bit0 = gzip. |
| 0x12 | SET_AP       | `ssid` (UTF-8, <=32B) |
| 0x13 | START        | (none) — bring up AP + DNS + HTTP + WS |
| 0x14 | STOP         | (none) |
| 0x15 | RESET        | (none) — ESP reboots |
| 0x16 | SELECT_GAME  | `gameid(1)` — 0 lobby, 1 trivia, 2 connect4 |
| 0x17 | QUESTION     | JSON: `{"i":<n>,"q":"..","o":["a","b","c","d"],"c":<0-3>,"dur":<sec>}` (trivia) |
| 0x18 | REVEAL       | (none) — close the current question, broadcast the correct answer |
| 0x19 | ROUND_END    | (none) — back to lobby for the active game |
| 0x1A | CONFIG       | JSON: `{"max":8}` optional tuning |
| 0x1B | RESET_SCORES | (none) — zero the ESP live score mirror |

**ESP -> Flipper**

| Type | Name         | Payload |
|------|--------------|---------|
| 0x80 | STATUS       | token: `boot` `files_ok` `ap_ok` `up ip=..` `stopped` `err ..` |
| 0x81 | JOIN         | `pid(1)` `nick` — a player joined |
| 0x82 | LEAVE        | `pid(1)` |
| 0x83 | SCORE        | `pid(1)` `delta(2 LE, signed)` `reason` — authoritative-persist on Flipper |
| 0x84 | ROUND_RESULT | JSON, game-specific (trivia: `{"correct":[pid..]}`, c4: `{"win":pid,"lose":pid}` or `{"draw":[a,b]}`) |
| 0x85 | EVENT        | JSON for host display, e.g. `{"answers":3,"total":5}` or `{"c4":"A vs B started"}` |
| 0x86 | PING         | identity beacon ~every 2s: `magic(4)` + `version(2 LE)`. `magic` = `48 41 52 43` ("HARC"); the Flipper only treats a magic-matched PING as "our board present", and flags `version < HA_FW_VERSION` as an outdated board to update. |

### 1.3 Raw-bulk escape (asset upload)

`FILE_BEGIN` is a normal control frame; immediately after its CRC, the sender
writes exactly `total` **unframed** bytes (the file content, possibly gzipped).
The receiver switches to a raw-read state, counts down `total`, stores the bytes,
then returns to frame parsing. This mirrors flytrap's `sethtml <N>\n` + N bytes,
generalized to named files. Bulk bytes need no escaping because the length is known.

### 1.4 Handshake (session start)

```
Flipper                         ESP
  |-- CLEAR_FILES -------------->|
  |-- FILE_BEGIN + bytes (xN) -->|   (index.html.gz, app.js.gz, app.css.gz, ...)
  |-- SET_AP ------------------->|
  |-- START -------------------->|
  |<------------- STATUS ap_ok --|
  |<------------- STATUS up ip=..|   AP live, phones can join
```
Then live: JOIN/LEAVE/SCORE/EVENT/ROUND_RESULT flow up; SELECT_GAME/QUESTION/
REVEAL/ROUND_END flow down as the host drives rounds. PING beacons throughout.

---

## 2. WebSocket JSON (Phone <-> ESP32)

- Endpoint: `ws://192.168.4.1/ws`. One socket per phone.
- All messages are a single JSON object with a `t` (type) field. Small; one frame.
- The ESP is authoritative: clients render server state and send intents only.

### 2.1 Client -> Server

| `t`        | Fields | Meaning |
|------------|--------|---------|
| `hello`    | `nick` | Join / re-join with a nickname (from localStorage) |
| `answer`   | `c` (0-3) | Trivia: buzz an answer for the current question |
| `challenge`| `to` (pid) | Connect4: challenge a player in the lobby |
| `accept`   | `from` (pid) | Connect4: accept a pending challenge |
| `cancel`   | | Connect4: withdraw my challenge / decline |
| `move`     | `col` (0-6) | Connect4: drop a disc in a column |
| `leaveGame`| | Connect4: forfeit/exit the current match |
| `ping`     | | keepalive |

### 2.2 Server -> Client

| `t`      | Fields | Meaning |
|----------|--------|---------|
| `welcome`| `pid`, `nick` | Assigned player id after `hello` |
| `lobby`  | `game` ("none"/"trivia"/"connect4"), `players` (`[{pid,nick,score}]`), `me` (pid) | Lobby snapshot; sent on change |
| `trivia` | `phase` ("idle"/"question"/"reveal"), `i`, `q`, `o` (opts), `dur`, `deadline` (ms epoch-ish, server `millis`), `mine` (my choice or -1), `counts` ([n0..n3]), `correct` (reveal only), `scores` | Full trivia view for this client |
| `c4`     | `phase` ("lobby"/"playing"/"over"), lobby: `challenges` (`[{from,to}]`); playing: `mid`, `board` (42 ints: 0 empty/1/2), `turn` (pid), `me` (1 or 2), `opp` (nick), `you` (pid); over: `result` ("win"/"lose"/"draw") | Full connect4 view for this client |
| `toast`  | `msg` | Transient message to show |
| `pong`   | | keepalive reply |

Server `millis` is used for `deadline`; the client shows a countdown from
`deadline - now_estimate`, so exact clock sync is not required (the server is the
referee for scoring; the client bar is cosmetic).

### 2.3 Scoring split

The ESP scores the live session (speed+correctness for trivia, win/draw for c4)
and (a) keeps a **live mirror** it broadcasts to phones in `players[].score`, and
(b) reports each delta to the Flipper via UART `SCORE` for the host display and
persistent leaderboard. Both stay consistent because every delta is reported.

---

## 3. v0.2 game expansion

New game ids (UART `SELECT_GAME` / lobby `game`): `3` tictactoe, `4` dots,
`5` draw, `6` pong. Lobby `game` string adds: `"tictactoe"`, `"dots"`, `"draw"`,
`"pong"`.

### 3.1 Duels (connect4, tictactoe, dots) — unified

All three are 1v1 and share the same lobby flow. Client intents:
`challenge{to}`, `accept{from}`, `cancel`, `move{n}`, `rematch`, `leaveGame`.
`move.n` is a grid index whose meaning depends on `kind` (below). `rematch` in an
`over` match restarts the same pairing (first move alternates) if the opponent is
still present.

Server -> client message `t:"duel"`, common fields: `kind`
("c4"/"ttt"/"dots"), `phase` ("lobby"/"playing"/"over"), `you` (pid), `me`
(1 or 2), `opp` (nick), `turn` (pid), `result` ("win"/"lose"/"draw", over only),
`challenges` (`[{from,to}]`, lobby only).

- **c4** (`kind:"c4"`): `cols:7`, `rows:6`, `need:4`, `gravity:true`, `board`
  (42 ints, row-major, row 0 top, 0/1/2). `move.n` = column 0..6.
- **ttt** (`kind:"ttt"`): `cols:3`, `rows:3`, `need:3`, `gravity:false`, `board`
  (9 ints). `move.n` = cell 0..8.
- **dots** (`kind:"dots"`): boxes grid `w`,`h` (e.g. 5x5 boxes). `hedges`
  (`(h+1)*w` ints 0/1 = drawn), `vedges` (`h*(w+1)` ints), `boxes`
  (`w*h` ints 0/1/2 = owner). `sme`,`sopp` (box counts). `move.n` = edge index:
  horizontal edges `0..(h+1)*w-1` then vertical edges after. Completing a box
  grants another turn.

### 3.2 Drawing + guessing (`draw`)

Host selects the game; the ESP runs rounds off its built-in word list, rotating
the drawer. Server -> client `t:"draw"`:
- `phase:"draw"`, `role:"drawer"`: `word`, `round`, `drawer` (pid), `scores`.
- `phase:"draw"`, `role:"guesser"`: `len` (word length), `round`, `drawer`
  (nick), `scores`.
- `phase:"reveal"`: `word`, `winner` (pid or null), `scores`.
- `phase:"idle"`: `scores`.

Ink: the drawer sends line segments `stroke{x0,y0,x1,y1}` (normalized 0..1) and
`clear{}`; the server relays to guessers as `ink{x0,y0,x1,y1}` / `ink{clear:true}`.
Guessing: a guesser sends `guess{text}`; a correct guess (case-insensitive) scores
and ends the round; a wrong guess is broadcast as `chat{nick,text}`.

### 3.3 Pong (`pong`)

1v1 via the same `challenge`/`accept`/`cancel`/`leaveGame` flow. Real-time: the
ESP ticks the ball + paddles and broadcasts. Server -> client `t:"pong"`:
`phase` ("lobby"/"playing"/"over"), `challenges` (lobby); playing: `you`, `me`
(1/2), `opp`, `ball{x,y}` (0..1), `p1`, `p2` (paddle y, 0..1), `s1`, `s2`
(scores); over: `result`. Client input: `paddle{dir}` with `dir` -1/0/1.

### 3.4 Trivia depth (additive)

The in-question `EVENT` (ESP -> Flipper) gains a `counts` array so the host screen
can show live per-option bars: `{"answers":n,"total":m,"counts":[c0,c1,c2,c3]}`.
The final podium is Flipper-side (from its roster scores); no new message.

### 3.5 Notes

- Only one game is active at a time (host-selected), so the duel lobby/challenge
  machinery is shared and parameterized by the active `kind`.
- `move` unifies to `{t:"move","n":<index>}` for every duel (connect4 included;
  it previously used `col`).

---

## 4. v0.2.0 — identity, reactions, four more games

New game ids (UART `SELECT_GAME` / lobby `game`): `7` react, `8` wyr, `9`
scramble, `10` reversi. Lobby `game` string adds `"react"`, `"wyr"`,
`"scramble"`, `"reversi"`. Firmware **v6** (`HA_FW_VERSION`).

### 4.1 Player identity + reactions

- `hello` gains an optional `avatar` field (an emoji, UTF-8, default 🙂). Every
  player object in `players`/leaderboard/podium messages now carries `avatar`.
- New client intent `react{emoji}`. The ESP broadcasts it to everyone as a
  **distinct** type `{"t":"emoji","pid","nick","avatar","emoji"}` (not `react`,
  which is the reaction-duel game state — see below).
- New client intent `say{text}` broadcasts a lobby/draw chat line as
  `{"t":"chat","nick","text"}` (the server echoes it back, so clients never render
  their own locally).

### 4.2 Reversi / Othello (`kind:"reversi"`)

A fourth duel on the shared challenge/accept/rematch flow. `cols:8`, `rows:8`,
`board` (64 ints, row-major, 0 empty / 1 / 2). `move.n` = cell 0..63; only cells
that flank and flip at least one opponent disc are legal. Extra fields: `sme`,
`sopp` (disc counts) and `valid` (array of legal cell indices for the player to
move, so the client can hint them). The ESP auto-passes a player with no legal
move and ends the game — most discs wins — when neither can move.

### 4.3 Whole-group party games

Three self-organizing games share a lobby -> countdown -> round -> reveal ->
final flow. Common client intents: `ready{ready:bool}` (ready-up in the lobby),
`again` (replay from the final screen). Common server phases: `"lobby"`
(`players:[{pid,nick,avatar,ready}]`), `"countdown"` (`sec`), and `"final"`.
Durations are sent in **seconds**; deadlines in ms (server `millis`).

- **Would You Rather** (`t:"wyr"`): `"vote"`/`"reveal"` carry `round`, `rounds`,
  `a`, `b` (the two options), `myvote` (0/1/-1), `counts` ([a,b]). Vote with the
  existing `answer{c:0|1}` intent. No scoring — it's a poll.
- **Word Scramble** (`t:"scramble"`): `"play"` carries `round`, `rounds`, `scram`
  (shuffled letters), `len`, `solved` (bool, you), `deadline`, `dur`, `scores`.
  Guess with the existing `guess{text}` intent; first correct scores most
  (200/120/80/40). `"reveal"` carries `word`; `"final"` a `board` podium.
- **Reaction Duel** (`t:"react"`): `"armed"` carries `round`, `rounds`, `light`
  ("wait"/"go"), `dq`, `tapped`, `scores`. Tap with the new `tap` intent; the
  first valid tap after `light:"go"` wins (200), tapping while `"wait"` DQs you
  for the round. `"reveal"` carries `winner` (nick or null), `ms`, `iwon`;
  `"final"` a `board` podium.
