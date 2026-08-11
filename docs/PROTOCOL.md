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
| 0x1A | CONFIG       | JSON: `{"max":8,"lang":"pt-br"}` — station cap and the host's phone-UI language (`""`/absent = English). The ESP stores `lang` and echoes it back in each `welcome`. |
| 0x1B | RESET_SCORES | (none) — zero the ESP live score mirror |
| 0x1C | CONTENT_CLEAR | (none) — drop all packs, for every game |
| 0x1D | CONTENT_PACK | game byte + pack name — begin a pack for that game |
| 0x1E | CONTENT_ITEM | JSON object of the file's own keys — append one item to the current pack |

> Content is opaque to the Flipper. It parses only `Key: value` blocks and ships them
> verbatim; every game's interpretation of those keys lives in the ESP firmware, so a new
> content game needs no protocol change. Per-game item shapes (no new opcodes, just what
> the ESP expects in each `CONTENT_ITEM` JSON object): trivia `{q,a,b,c,d,answer}`, wyr
> `{a,b}` (the two options), scramble and draw `{word}` (a single plain word).
>
> Content **language** is resolved entirely on the Flipper: for the host's chosen `lang`
> it streams `packs/<game>/<lang>/` (falling back to the English packs at `packs/<game>/`
> per game), so the wire opcodes above are language-agnostic. Item text is UTF-8.

**ESP -> Flipper**

| Type | Name         | Payload |
|------|--------------|---------|
| 0x80 | STATUS       | token: `boot` `files_ok` `ap_ok` `up ip=..` `stopped` `err ..` |
| 0x81 | JOIN         | `pid(1)` `nick` — a player joined |
| 0x82 | LEAVE        | `pid(1)` |
| 0x83 | SCORE        | `pid(1)` `delta(2 LE, signed)` `reason` — authoritative-persist on Flipper |
| 0x84 | ROUND_RESULT | JSON, game-specific (trivia: `{"correct":[pid..]}`, c4: `{"win":pid,"lose":pid}` or `{"draw":[a,b]}`) |
| 0x85 | EVENT        | JSON for host display, e.g. `{"answers":3,"total":5}` or `{"c4":"A vs B started"}` |
| 0x86 | PING         | identity beacon ~every 2s: `magic(4)` + `version(2 LE)` + `bundleCrc(4 LE, v19+)` + `game(1, v19+)`. `magic` = `48 41 52 43` ("HARC"); the Flipper only treats a magic-matched PING as "our board present", and flags `version < HA_FW_VERSION` as an outdated board to update. `bundleCrc` is the CRC-32/IEEE of the web bundle the ESP holds in flash (0 = none); the Flipper skips re-streaming when it equals the manifest's `crc`. `game` is the ESP's current game id (`HA_GAME_*`); while hosting the Flipper mirrors it (ignoring 0/NONE) so a phone-vote game change reflects on the dashboard reliably — the beacon always arrives, unlike a one-off EVENT. Pre-v19 boards omit bytes 6-10 (backward-compatible). |
| 0x86 | PING         | identity beacon ~every 2s: `magic(4)` + `version(2 LE)`. `magic` = `48 41 52 43` ("HARC"); the Flipper only treats a magic-matched PING as "our board present", and flags `version < HA_FW_VERSION` as an outdated board to update. |
| 0x87 | ART          | finished artwork, streamed: `op(1)` + JSON. `op` 0 = begin a sheet (`{"game":"frankendraw","id":n,"w0":"..","w1":"..","w2":".."}` — the three panels' drawers), 1 = one line segment (`{"p":panel,"x0":..,"y0":..,"x1":..,"y1":..}`, 0..255 sheet units), 2 = end (`{"id":n}`). One frame per segment: the picture is streamed as it is finished, so neither side ever buffers a drawing. The Flipper writes each sheet to `/ext/apps_data/hotspot_arcade/art/fd-<YYMMDD-HHMMSS>-<n>.svg`. |

### 1.3 Raw-bulk escape (asset upload)

`FILE_BEGIN` is a normal control frame; immediately after its CRC, the sender
writes exactly `total` **unframed** bytes (the file content, possibly gzipped).
The receiver switches to a raw-read state, counts down `total`, **writes the bytes to a
LittleFS flash partition** (not RAM), then returns to frame parsing. This mirrors flytrap's
`sethtml <N>\n` + N bytes, generalized to named files. Bulk bytes need no escaping because
the length is known.

### 1.4 Handshake (session start)

```
Flipper                         ESP
  |-- CLEAR_FILES -------------->|   (skipped when the bundle is unchanged, see below)
  |-- FILE_BEGIN + bytes -------->|   index.html.gz -> ESP writes it to LittleFS flash
  |-- (content packs) ---------->|   trivia/party packs (always streamed)
  |-- SET_AP ------------------->|
  |-- START -------------------->|
  |<------------- STATUS ap_ok --|
  |<------------- STATUS up ip=..|   AP live, phones can join
```
The ESP persists the streamed bundle in flash and serves it from there, so it survives a
reboot. It advertises the bundle's CRC-32 in every PING (§1.2); when that equals the `crc`
in the Flipper's `manifest.json`, the Flipper **skips `CLEAR_FILES` and the whole file
stream**, jumping straight to the content packs + `SET_AP` — the board reuses the copy it
already holds (no ~47 KB re-transfer per session). A changed bundle, or an
`apps_data/.../web` override whose manifest carries a different/absent `crc`, streams
normally and overwrites the stored copy. Skipping is an optimization only: the ESP always
writes+serves from flash, so an older Flipper that always streams still works.

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
| `welcome`| `pid`, `nick`, `avatar`, `lang` | Assigned player id after `hello`, with the identity the server holds for this device (see §10 — on a rebind these are the existing player's, not what the client just sent); `lang` is the host's phone-UI language (`""` = English), which the client uses to pick its message catalog |
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
  existing `answer{c:0|1}` intent. No scoring — it's a poll. Its `"final"` adds
  `voters` (players connected now) and `rounds` — here an **array** of the whole
  game's splits, `[{a,b}, …]`, one entry per round played, latched by the ESP at
  each reveal. (`rounds` is a count in the play phases and this array in `"final"`.)
  A round nobody voted in is sent as `{"a":0,"b":0}`. The client draws the
  agreement chart from it: per-round agreement is the majority share
  `max(a,b)/(a+b)`, bucketed onto the percentages reachable with `voters` players
  (`ceil(n/2)/n … n/n`), plus the mean. The history has to come from the ESP —
  a phone that joined late never received the earlier rounds. Firmware **v18**.
- **Word Scramble** (`t:"scramble"`): `"play"` carries `round`, `rounds`, `scram`
  (shuffled letters), `len`, `solved` (bool, you), `deadline`, `dur`, `scores`.
  Guess with the existing `guess{text}` intent; first correct scores most
  (200/120/80/40). `"reveal"` carries `word`; `"final"` a `board` podium.
- **Reaction Duel** (`t:"react"`): `"armed"` carries `round`, `rounds`, `light`
  ("wait"/"go"), `dq`, `tapped`, `scores`. Tap with the new `tap` intent; the
  first valid tap after `light:"go"` wins (200), tapping while `"wait"` DQs you
  for the round. `"reveal"` carries `winner` (nick or null), `ms`, `iwon`;
  `"final"` a `board` podium.

## 5. Guess the Color (`gc`) — game id `11`

Whole-group round game on the same `Party` skeleton (`lobby -> countdown -> play
-> reveal -> ... -> final`, 5 rounds). Select with UART `SELECT_GAME` id `11`;
lobby `game` string is `"gc"`. Firmware **v12**.

Client intents: `ready{ready:bool}` (lobby), `again` (from final), and
`guess{r,g,b}` (submit your color, each 0-255). The `guess` type is shared with
draw/scramble, which send `guess{text}`; the ESP routes by which fields are present.

Server `{t:"gc",phase,...}`:
- `"play"`: `round`, `rounds`, `color` (`"#RRGGBB"`, the target swatch to match —
  the numeric answer is hidden), `submitted` (bool, you), `scores`.
- `"reveal"`: `round`, `rounds`, `r`,`g`,`b` (the true color), `color`, `your`
  (`{r,g,b,color,dist,points}` or null if you didn't guess), `winner` (nick or
  null), `iwon`, `scores`.
- `"final"`: `board` (podium).

Scoring per round: `points = closeness + speed_bonus`, where
`closeness = round(200 * (1 - dist/441.67))` clamped ≥ 0 (`dist` = Euclidean RGB
distance) and `speed_bonus = round(100 * (1 - submit_ms/12000))` clamped ≥ 0.
The round winner is the highest points (ties broken by the faster submit).

## 6. Battleship (`bs`) — game id `12`

A 1v1 match game (like Pong): shares the challenge/lobby flow (`challenge`/`accept`/
`cancel`, `rematch`, `leaveGame`) but has its own state and screen. 10x10 grid, five ships
(5,4,3,3,2 = 17 cells). Select with UART `SELECT_GAME` id `12`; lobby `game` string `"bs"`.
Firmware **v13**.

Client intents (besides the shared match ones): `place{ships}` and `fire{n}`.
- `place{ships}`: `ships` is a string `"r,c,d;r,c,d;..."`, one triple per ship in fixed
  order (5,4,3,3,2); `d`=0 horizontal, `d`=1 vertical (anchor is the top/left cell). The
  server reconstructs the cells, validates bounds and no-overlap, stores the fleet, and
  marks the player ready. Invalid layouts get a `toast` and no ready. Both ready -> firing.
- `fire{n}` (0..99): a hit keeps your turn (shoot again); a miss passes it. Sinking a ship
  sends a `toast` to both players.

Server `{t:"bs",phase,...}`:
- `"place"`: `you`, `me` (1/2), `opp`, `ready`, `oppReady`. The client renders its own board.
- `"fire"`: `turn`, `yourTurn`, `opp`, `myShips`, `oppShips`, and two 100-cell arrays:
  `mine` (your fleet: 0 empty, 1 ship, 2 miss, 3 hit) and `track` (your shots on the enemy:
  0 un-shot, 1 miss, 2 hit, 3 hit+sunk).
- `"over"`: adds `result` (win/lose) and `oppFleet` (the enemy fleet revealed).

**Hidden information:** `track` is derived only from the shots you've fired, so an enemy
ship cell you haven't hit is never in the payload. `oppFleet` appears only in `"over"`.

## 7. Spectrum (`spectrum`) — game id `13`

A whole-group party game (Wavelength-style) on the shared party skeleton (lobby with a
ready-up + pack vote, countdown, reveal). Content reuses the pack pipeline: each item is a
`Left`/`Right` word pair. Select with UART `SELECT_GAME` id `13`; lobby `game` string
`"spectrum"`. Firmware **v14**.

Each round rotates a **psychic** who sees a hidden target on a 0-100 spectrum and types a
clue; everyone else slides a dial to guess. Points by closeness (±2 = 4, ±7 = 3, ±12 = 2),
and the psychic earns the guessers' average, so a good clue pays off. Six rounds.

Client intents: `ready`, `vote{pack}`, `clue{text}` (psychic only), `slide{n}` (0..100,
guessers only), `again`.

Server `{t:"spectrum",phase,...}`:
- `"lobby"`: `you`, `players`, `packs` (name/votes), `myvote`.
- `"countdown"`: `sec`.
- `"play"` with `stage` `"clue"` | `"guess"` | `"reveal"`: `round`, `rounds`, `left`,
  `right`, `psychic` (nick), `iam` (am I the psychic), `deadline`/`dur` for the timer bar.
  - `target` (0..100) is sent **only to the psychic** during clue/guess, and to everyone on
    reveal — an un-revealed target never reaches a guesser.
  - `clue` appears once the psychic has submitted; `myguess` is the guesser's own locked value.
  - reveal adds `guesses` (`nick`/`g`/`pts`) and `mygain`.
- `"final"`: `board` (the shared leaderboard).

## 8. Kiss Marry Kill (`kmk`) — game id `14`

A whole-group party game on the shared party skeleton (lobby with a ready-up + pack vote,
countdown, reveal). Content reuses the pack pipeline: each item is a `Name` (one person or
character). Select with UART `SELECT_GAME` id `14`; lobby `game` string `"kmk"`. Firmware
**v15**.

Each round rotates a **chooser** and draws three people from the pack. The chooser secretly
assigns Kiss / Marry / Kill; everyone else predicts that assignment. Points are the number
of matching positions (0, 1, or 3 — matching two forces the third), and the chooser earns
the guessers' average. Six rounds.

Client intents: `ready`, `vote{pack}`, `assign{kiss,marry,kill}` (each is the 0-2 index of
the person getting that label; the chooser sends it in the choose stage, guessers in the
guess stage), `again`.

Server `{t:"kmk",phase,...}`:
- `"lobby"`: `you`, `players`, `packs` (name/votes), `myvote`.
- `"countdown"`: `sec`.
- `"play"` with `stage` `"choose"` | `"guess"` | `"reveal"`: `round`, `rounds`, `chooser`
  (nick), `iam` (am I the chooser), `people` (the three names), `deadline`/`dur` for the timer.
  - `answer` (the chooser's Kiss/Marry/Kill labels) is sent **only to the chooser** from the
    guess stage on, and to everyone on reveal — a guesser never sees it early.
  - `mine` is the guesser's own submitted labels.
  - reveal adds `guesses` (`nick`/labels/`pts`) and `mygain`.
- `"final"`: `board` (the shared leaderboard).

## 9. Chess (`chess`) — game id `15`

A 1v1 duel (like Pong/Battleship): shares the challenge/lobby flow (`challenge`/`accept`/
`cancel`, `rematch`, `leaveGame`) but plays full FIDE rules, refereed entirely on the ESP.
Select with UART `SELECT_GAME` id `15`; lobby `game` string `"chess"`. Firmware **v17**.
Chess has no content packs — its UI strings are localized client-side from the message
catalog like every game (the host's `lang`, set via `CONFIG` and echoed in `welcome`); the
per-language `packs/<game>/<lang>/` streaming that content games use does not apply here.

Client intents (besides the shared match ones): `move{from,to[,promo]}`, `resign`, `draw`
(offer, or accept one already pending), `claim`.
- `move{from,to,promo}`: squares are 0-63, `a1 = 0` row-major (`h8 = 63`). `promo` is
  required exactly when the move lands a pawn on the last rank — `2` knight, `3` bishop,
  `4` rook, `5` queen — and must be omitted/0 otherwise; a mismatched, illegal, or
  malformed move is silently ignored.
- `resign`: forfeit the game immediately.
- `draw`: offers a draw if none is pending; if the opponent already offered, accepts it and
  ends the game as a draw. The opponent gets a `toast` when an offer arrives.
- `claim`: claims a draw when threefold repetition or the 50-move count currently stands
  for the player to move; a no-op otherwise.

Server `{t:"chess",phase,...}`:
- `"playing"`: `you`, `opp`, `white` (bool, are you playing white), `turn` (pid),
  `yourTurn`, `board` (64 chars, index 0 = a1, row-major to h8: `PNBRQK`/`pnbrqk`/`.`),
  `moves` (`[from*64+to, ...]`, your legal moves, always present but populated only for the player to move), `check`,
  `last` (`from*64+to` of the last move played, `-1` before the first), `deadline`, `run`,
  `oms`, `wtm`, `claim3`, `claim50`, `offer` (`0` or the pid with a pending draw offer).
- `"over"`: the same fields minus `moves`/`claim3`/`claim50`, plus `result`
  (`"win"`/`"lose"`/`"draw"`) and `reason` (`mate`/`stalemate`/`resign`/`flag`/`flagdraw`/
  `material`/`rep3`/`rep5`/`move50`/`move75`/`agree`/`left`). **Quirk:** the server keeps
  recomputing `deadline` off the current time even after the game ends, so clients must
  read the clocks from `run`/`oms` (which the server does freeze on finish) rather than
  animate a countdown from `deadline` here. `offer` is also stale in this phase — it is not
  cleared when the game ends — so clients should ignore it once `phase` is `"over"`.
- `"lobby"`: `challenges` only.

Clocks: fixed 5+0 blitz, no increment, server-authoritative. `run` is the milliseconds left
on the clock of the side to move (`wtm` = white to move) and `oms` is the other side's
frozen remaining time; `deadline` is the server's `now + run`, so the client animates a
countdown without needing clock sync (same pattern as the other timed games). A flag fall
loses the game for the side whose clock ran out, unless the opponent could not mate by any
legal sequence (FIDE 6.9), in which case it's a draw (`flagdraw`).

Draw rules: stalemate, dead position (insufficient material), fivefold repetition, and the
75-move rule end the game automatically; threefold repetition and the 50-move rule are
claimable only by the player to move, via `claim`, exactly while `claim3`/`claim50` reads
true; either player can also offer or accept a draw via `draw`. A pending offer lapses the
moment either side plays a move.

State is pushed only on events — a move, resign, draw, claim, or a flag fall the ESP
notices on its own clock tick — never on a periodic heartbeat; clients animate the
countdown locally between pushes from `deadline`.

## 10. Secrets (`secrets`) — game id `16`

A whole-group party game on the shared party skeleton (lobby with a ready-up + pack vote,
countdown, reveal). Content reuses the pack pipeline: each item is a `Q` (one yes/no
question). Select with UART `SELECT_GAME` id `16`; lobby `game` string `"secrets"`.
Firmware **v18**.

Each round shows one question and runs **answer → predict → reveal**. First everyone
secretly **answers** yes/no; then everyone secretly **predicts** how many of the `N` joined
players said yes (an integer `0..N`); then reveal. Only the group's total yes-count is ever
revealed — the individual yes/no answers are never serialized to anyone. An exact
prediction scores 1, otherwise 0. Six rounds.

Client intents: `ready`, `vote{pack}`, `reply{v}` (`1` = yes, `0` = no; answer stage),
`predict{n}` (your yes-count guess, clamped `0..N`; predict stage), `again`. The distinct
`reply`/`predict` verbs avoid colliding with Would You Rather's `answer`/`vote`.

Server `{t:"secrets",phase,...}`:
- `"lobby"`: `you`, `players`, `packs` (name/votes), `myvote`.
- `"countdown"`: `sec`.
- `"answer"` / `"predict"`: `round`, `rounds`, `n` (player count / predict upper bound),
  `q` (the question), `locked`/`total` (aggregate progress in the current step),
  `myprediction` and `myanswer` (**your own only**, `-1` if unset), `deadline`/`dur` for
  the timer bar, and `scores` (the shared leaderboard).
- `"reveal"`: adds `yes` (the group total, the **only** answer information ever sent),
  `guesses` (`[{nick, n, pts}]` — every player's prediction and points; predictions are
  guesses about the group, not personal, so they're public here), and `mygain` (your
  points). No individual yes/no **answer** is ever serialized, in any phase — anonymity is
  enforced in `secretsJson(pid)`.
- `"final"`: `board` (the shared leaderboard).
---

## 11. Player identity — one phone = one player

Firmware **v18**. A player is bound to the **device**, not to the WebSocket. The ESP
resolves each connection to the station's **MAC address** and keys the player on that; the
WebSocket layer only knows a peer IP, and that IP is something the ESP's own DHCP server
made up, so it is a derived value, not an identity.

Why it is needed: a phone could show up as two or three players at once.

- iOS (and Android) open a **captive mini-browser** for the portal. It is a separate
  browser context from Safari/Chrome with its own `localStorage`, so the saved-identity
  auto-rejoin the client does cannot help — play in the captive window *and* open
  `192.168.4.1` in Safari and the ESP used to see two joins. A second tab, or a second
  browser, is the same story.
- A phone that drops (screen lock, WiFi off) closes nothing; the ESP only learns about it
  when TCP times out, which can take minutes. If the phone comes back first, it used to
  become a new player while the old one lingered as a ghost on the leaderboard.

How the MAC is obtained (all firmware-side, the wire protocol is unchanged): the AP's DHCP
server reports the assigned address *and* the client MAC together
(`ip_event_ap_staipassigned_t`), so the firmware keeps a small IP → MAC table from that
event. A station whose lease predates the handler is looked up in lwIP's ARP cache
instead, and if the MAC still cannot be resolved the IP itself is used as the key. The
engine never sees any of this — it stores an opaque 64-bit device key, `0` = unknown.

Modern phones present a randomized "private" MAC, but it is **stable per SSID**: it
survives reconnects and only changes if the AP is renamed, which is exactly the lifetime a
session needs.

Rules the ESP applies to `hello`:

1. **Known socket** → the existing player; `nick`/`avatar` in the message are an edit from
   the header identity editor and are applied (unchanged behaviour).
2. **New socket, device already playing** → *rebind*: the existing player is re-pointed at
   the new socket, keeping their `pid`, `nick`, `avatar` and `score`. Never a second
   player. The `hello`'s own `nick`/`avatar` are ignored (a fresh context sends a name of
   its own, and letting that rename a player mid-session is the bug, not the fix). No UART
   `JOIN` is emitted — nobody joined. The `welcome` reply carries the *existing* identity,
   and the client adopts it, so the second context immediately shows the same name and
   avatar.
3. **New socket, new or unknown device** → a new player, exactly as before.

A `DISCONNECT` only removes a player if the closing socket is still that player's **current**
socket. The superseded context's late close — and any message it still sends — is ignored.

Device key `0` means "unknown" (reported for anything that is not a joined station,
including the AP's own address) and never matches, so those clients fall back to one player
per connection.

The ESP traces each decision to its serial console:
`[ha] JOIN pid=2 ip=192.168.4.3 mac=AA:BB:CC:DD:EE:FF nick="..."` and
`[ha] NEW BROWSER same device ip=192.168.4.2 mac=AA:BB:CC:DD:EE:FF -> pid=1 nick="..." (consolidated)`.

**Trade-off, deliberate:** two people can no longer share one phone as two players — the
second `hello` from that phone joins the first player instead of creating a second. Playing
one phone per person is the assumption everywhere else in the UI (the phone is the
controller), and duplicate players from a single phone were the far more common failure. A
phone whose MAC changes (the AP was renamed) simply becomes a new player, which is the
pre-existing behaviour.

---

## 12. Game-change vote (`gamevote`) — cross-cutting, firmware v18

A player can change the active game **from their phone** by majority vote, so the host
device needs no operation. This is the one sanctioned phone→host action — the engine
otherwise forbids a phone from selecting a game — and it is gated entirely behind the vote.
A host-initiated `SELECT_GAME` stays authoritative and immediate (no vote), and cancels any
pending proposal.

The vote sits **above** the active game (it is not a per-game phase). While a proposal is
pending the active game is **frozen**: `tick` advances only the vote timeout, `onInput`
honors only `voteGame` (and `leaveGame`), and `pushAll` sends the `gamevote` overlay to
every client instead of any game/lobby state. On resolution the previous state resumes
(reject/timeout) or the new game's lobby appears (approve), both signalled by the next
normal `lobby` push — the client closes the modal when a `lobby` message arrives.

Client intents:
- `proposeGame{game}`: `game` is the engine game-name string (e.g. `"wyr"`, `"trivia"`), or
  `"none"` for "back to the lobby" — leaving the current game is voted on like any other
  change. Starts a proposal if none is pending and the target is a valid game **other than
  the active one** (so `"none"` is refused while already in the lobby). The proposer counts
  as an implicit YES. A second proposal while one is pending is ignored.
- `voteGame{ok}`: one vote per non-proposer pid (`true` = OK, `false` = No). From the
  **proposer**, `ok:true` is a no-op (their YES is already implicit) and `ok:false`
  **withdraws** the proposal — the reject path, resuming the frozen game at once. That is
  what the Cancel button on the proposer's own overlay sends; no separate intent exists.

Server `{t:"gamevote",...}` (pushed to every client while pending): `proposer` (nick),
`avatar` (the proposer's emoji, so the voters' line can lead with it), `game` (target name,
`"none"` for the lobby), `label` (same as `game`; the client maps it to a pretty label),
`yes` (count **including** the proposer), `no`, `others` (`playerCount - 1`), `need` (YES
votes needed from the others = `floor(others/2)+1`), `youproposed`, `youvoted`.

Resolution (recomputed on every vote, join/leave, and tick):
- **Approve** when YES among the other players is a strict majority — `yesOthers*2 >
  others` — or immediately if the proposer is the only player. → `selectGame(target)`.
- **Reject** as soon as that majority is impossible — `noOthers*2 >= others` — or on
  **timeout** (`GAMEVOTE_SECS` = 25s). → clear and resume the frozen game.
- **Withdrawn** when the proposer sends `voteGame{ok:false}` (their Cancel button). → same
  as reject: clear and resume.
- If the **proposer leaves**, the proposal is cancelled (reject). A non-proposer leaving
  recomputes the tally (fewer `others` can tip it either way).

On approve the ESP also emits a UART `EVENT` `{"gamevote":"approved","game":"<name>","id":<N>}`
(the `id` added in v19). The Flipper reads `id` — the numeric `HA_GAME_*` — to update its
displayed active game to match the vote and to avoid reverting it on an ESP reboot; `game`
is the short name, for logging. (The Flipper has no name→id map, hence the numeric id.)
On approve the ESP also emits a UART `EVENT` `{"gamevote":"approved","game":"<name>"}` for
host-side observability.

## 13. Fill the Blank (`fillblank`) — game id `17`

A whole-group party game on the shared party skeleton (ready-up + pack vote lobby,
countdown, reveal), in the "a judge picks the funniest answer" shape — inspired by Cards
Against Humanity, with cards written for this project (no affiliation, and none of their
text). Select with UART `SELECT_GAME` id `17`; lobby `game` string `"fillblank"`. Firmware
**v19**.

Content reuses the generic pack pipeline, but a pack carries two decks: a block with a
`P` key is a **prompt card** (it contains the `_____` blank), a block with an `A` key is
an **answer card**. Caps are this game's own — `FB_MAX_PACKS` 3 packs, `FB_MAX_PROMPTS` 24
prompts and `FB_MAX_ANSWERS` 56 answers per pack — not the shared `PACK_MAX_ITEMS`/
`TRIVIA_MAX_TOPICS`, because an answer deck needs far more than 32 entries and a judging
game only ever plays one pack per session.

Each round rotates a **Czar** (the `czarSeq mod N`-th connected player, in pid order) and
shows one prompt. Every other player holds a hand of 6 answer cards and plays exactly one,
face down; the played card **stays in their hand**, marked as the one they chose while the
rest grey out, and is discarded and redrawn when the next round deals. When all of them have
played (or the safety timer expires) the pile is shuffled and shown anonymously.

The pile always carries **one extra answer card drawn at random from the deck itself**,
judged blind alongside the players'. It is marked internally by the sentinel author pid
`FB_DECK_PID` (0, never a real pid) and is indistinguishable from a real submission until
the pick lands.

Only the Czar may pick. Scoring:

| the Czar picks | winning author | Czar |
| --- | --- | --- |
| a player's card | +1 | +1 |
| the deck's card | nobody scores | 0 |

Giving the Czar a point for landing on a real card is what makes judging well worth
something; picking the deck's card means the deck beat the room and nobody scores at all.
Hands refill from a draw pile that reshuffles used cards back in, so the deck never dead-
ends. Six rounds, then the podium.

The game needs a quorum of 3 (a Czar plus two answers to choose between): below that the
lobby keeps waiting rather than starting a round. A player who joins mid-round is dealt a
hand immediately but sits that round out (`waiting`); a player who leaves stops being
waited on, and if the **Czar** leaves the round ends with no winner and the rotation
carries on.

Client intents: `ready`, `vote{pack}`, `play{card}` (the index of a slot in your own hand),
`pick{i}` (the Czar's index into the shuffled submissions), `again`.

Server `{t:"fillblank",phase,...}`:
- `"lobby"`: `you`, `players`, `packs` (name/votes), `myvote`, `min` (the quorum).
- `"countdown"`: `sec`.
- `"play"` with `stage` `"play"` | `"judge"` | `"reveal"`: `round`, `rounds`, `czar` (nick),
  `iam` (am I the Czar), `prompt`, `played`/`total` (submissions in / expected),
  `deadline`/`dur` for the timer, `scores`.
  - `hand` is sent **only to its owner** and only when they are not the Czar. It is
    slot-indexed and an empty slot arrives as `""`, so the index a client sends back in
    `play{card}` is the slot the engine dealt. `mine` is the slot they played (-1 = none)
    and `waiting` is true for a player who joined after this round was dealt.
  - `subs` appears from the judge stage on: an array of **bare card text**, shuffled. It
    carries no pid, no nick, and no hint of which entry is the deck's own card, so nothing
    in it maps a card to its author — that is the hidden-information rule this game
    enforces, and it holds for the Czar's copy too. `played`/`total` count **players**, so
    the deck's card is never included in the tally.
  - reveal releases all the authorship at once: `authors` runs parallel to `subs` and names
    the player who played each card, and `deckcard` is the index in `subs` of the deck's own
    card (`-1` when there is none). A card whose author has since left serializes as an
    empty nick, which is why the deck's card is identified by index rather than by an empty
    author — the two must not be confusable.
  - reveal also adds `pick` (the winning index in `subs`), `winner` (the author's nick, `""`
    when the deck won or the round was aborted), `deckwon` (the Czar picked the deck's
    card), `czarpts` (`1` when the Czar scored), `mywin`, and `mygain` (what *this* player
    earned this round).
- `"final"`: `board` (the shared leaderboard).

## 10. Werewolf (`werewolf`) — game id `18`

A whole-group party game on the shared party skeleton (ready-up lobby, countdown), but with
no content packs — the roles are code. Select with UART `SELECT_GAME` id `18`; lobby `game`
string `"werewolf"`. Firmware **v19**. Needs **5 players** minimum; the countdown does not
arm below that.

The phones referee, they do not carry the conversation: players argue out loud in the room
and only the secret actions and the vote go through the phone. That also makes the phone the
werewolves' *only* coordination channel — they are sitting in the same room and cannot
whisper — which is why their tally is pushed live (see `packvotes` below).

**Roles** are dealt over a shuffled roster when the countdown ends: `2` = werewolf, `3` =
seer, `4` = doctor, `1` = villager. `0` means "not in this game" — a spectator, i.e. someone
who joined mid-game or whose pid was vacated by a leaver.

| players | werewolves | seer | doctor | villagers |
| --- | --- | --- | --- | --- |
| 5 | 1 | 1 | — | 3 |
| 6 | 1 | 1 | 1 | 3 |
| 7 | 1 | 1 | 1 | 4 |
| 8 | 2 | 1 | 1 | 4 |
| 9–11 | 2 | 1 | 1 | 5–7 |
| 12 | 3 | 1 | 1 | 7 |

Werewolves are `n / 4` (at least one), capped so the village always starts ahead **and**
always keeps a plain villager beside its specials. The doctor is dealt from **6 players up**;
at five the table is small enough already.

**Phases.** `phase` is `"lobby"` / `"countdown"` / `"play"` / `"final"`; play is subdivided
by `stage`, which cycles:

| stage | window | what happens |
| --- | --- | --- |
| `roles` | 12 s | each phone privately shows its own role; werewolves also see the pack |
| `night` | 60 s, **fixed** | wolves each tap a victim, the seer checks one player, the doctor shields one |
| `dawn` | 8 s | the night's outcome is announced; a body's role is revealed |
| `day` | `60 + 20 x living`, clamped to 90–240 s | every living player accuses someone; the tally is public |
| `dusk` | 8 s | the player voted out (if any) is announced, role revealed |

The **night never ends early**, even when every special role has acted: a short night would
tell the room how many specials are still alive. The **day** may end early, but only on a
**hammer** — the moment anyone reaches a strict majority of the living (`living / 2 + 1`),
which is public information because the day tally is public. Otherwise the window expires and
whoever did not act is simply skipped; nothing is ever announced about a player failing to
act, so an idle seer just gets no vision.

**Night resolution.** The wolves' ballot takes the most-picked target, ties broken uniformly
at random. If no wolf picked at all, **nobody dies** — the engine never invents a kill. If
the doctor was shielding the chosen target, the attack is **blocked** and nobody dies. A wolf
may only target a living non-wolf. The doctor may shield **anyone living including
themselves**, but **not the same player two nights running**. At **6 players or fewer the
first night takes nobody at all** (`nokill`) — a night-one kill at that size drops the game
to four with no information; the seer and doctor still act. `dawnkind` distinguishes all four
outcomes: `killed`, `saved`, `quiet` (no wolf hunted), `nokill`.

**Day resolution.** Plurality, but a **tie hangs nobody** — a village that could not agree
is a village that does not execute, and a coin-flip lynch quietly favours the wolves. An
empty ballot likewise hangs nobody.

**Winning.** Villagers win when the last werewolf is out; werewolves win as soon as they are
no longer outnumbered (`wolves >= villagers`). The check also runs on every roster change, so
the last werewolf disconnecting is a village win. **Each player still alive on the winning
side scores 1**, reported over UART `SCORE` with reason `werewolf`.

**Secrecy is enforced server-side.** Roles live only on the ESP and leave it through one
serializer, which asks a single predicate about every player it emits: you always see your
own role, werewolves see each other, a dead player's role is public, and at the final every
role opens up. A role a phone is not entitled to is simply absent from the bytes it receives
— there is nothing for a client to hide. The same gate covers the seer's reading, the
doctor's shield and the pack's tally.

Client intents: `ready`, `kill{n}` (living werewolves, night stage), `see{n}` (the seer,
night stage, one reading per night), `guard{n}` (the doctor, night stage, one shield per
night), `accuse{n}` (living players with a role, day stage), `again`.

Server `{t:"werewolf",phase,...}`:
- `"lobby"`: `you`, `players` (nick/avatar/ready), `min` (5), `enough`.
- `"countdown"`: `sec`.
- `"play"`: `stage`, `you`, `day` (night/day number), `myrole`, `alive`, `wolvesleft`,
  `villagersleft`, `deadline`/`dur`, and `players` — each entry `pid`, `nick`, `avatar`,
  `in` (holds a role), `alive`, and **`role` only when this viewer may see it**.
  - `owe` says whether **this** phone still has an action outstanding. At night the *count*
    of outstanding actors is deliberately never sent — it is a headcount of the surviving
    special roles.
  - `mykill`, `packsize` and `packvotes` (`by`/`pid`) go **only to living werewolves in the
    night stage**, pushed on every tap so the pack converges without speaking. A villager's
    payload carries no trace that a night vote is happening.
  - `myguard` and `lastguard` (the target barred this night) go **only to the doctor**.
  - `check` (`pid`/`nick`/`wolf`) goes **only to the seer**, from the moment they look until
    the next night falls.
  - `nokill` marks the small-table opening night (public: it follows from the player count).
  - `dawn` adds `victim` (pid, `0` = nobody died) and `dawnkind`; `dusk` adds `lynched`
    (pid, `0` = nobody); `day` adds the public `myvote`, `votes` (`by`/`pid`), `waiting`,
    `voters` and `needed` (the hammer threshold).
- `"final"`: `winner` (`"villagers"` | `"wolves"`), `myrole`, `players` with every role
  revealed, `log` (per night: `day`, `victim`, `kind`, `lynched`), and `board` (the shared
  leaderboard).

## 10. Spyfall (`spyfall`) — game id `19`

A whole-group party game on the shared party skeleton (lobby with a ready-up + pack vote,
countdown, reveal). Content reuses the pack pipeline with a two-key block: a `Loc:` line
plus one `R:` line per role played there. Select with UART `SELECT_GAME` id `19`; lobby
`game` string `"spyfall"`. Firmware **v18**. Minimum **3 players** — the lobby holds until
three are connected, and the `lobby` message carries `need` so the phone can say so.

Everyone is dealt the same location and a role at it, except a rotating **spy** who is told
neither and instead receives the pack's full list of candidate locations. The round is
driven by players **pressing things**, not by a clock: the six minutes are the fallback,
and running them out does not end the round — it starts a nomination the table has to win.

A playing round (`phase:"play"`) walks four stages:

| `stage` | what it is | ends when |
|---|---|---|
| `"card"` | your card is on screen; tap OK | everyone has, or `SPYFALL_CARD_SECS` (30 s) |
| `"talk"` | card hidden, questioning out loud, buttons live | a button lands, or `SPYFALL_TALK_SECS` (360 s) |
| `"nominate"` | `nomStage` `"hush"` (4 s) → `"pick"` (30 s) → `"poll"` (20 s), round-robin | an accusation stands, or every seat has nominated |
| `"reveal"` | location, spy, roles, misses | `SPYFALL_REVEAL_MS` (9 s) |

The **six-minute clock only starts when the last phone has acknowledged its card**, not at
round start — hardware play showed the round felt "far too fast" when it was already
running while people were still reading.

During `"talk"`:
- **`solve{loc}`** ("I know the location", spy only, any time): right or wrong it ends the
  round. `loc` indexes the `locs` list.
- **`accuse{pid}`** ("I know the spy", **every** player including the spy — deliberately,
  as cover the spy can use): right ends the round; wrong records a miss, spends that
  player's single press for the round (`spent`), and play carries straight on. A spent
  player's further presses are dropped outright, even a correct one.

During `"nominate"`, once the clock has run out and discussion stops:
- **`nominate{pid}`** — only the current `nominator`, in `nomStage:"pick"`.
- **`agree{in}`** — everyone in the round, in `nomStage:"poll"`. The nominator counts as in
  automatically. A nomination **stands** when the yes count reaches `need` (the number of
  non-spies, i.e. players in the round minus one) and then settles the round either way:
  the spy nominated → the table scores; an innocent nominated through → the spy scores.
  Short of `need` the turn passes to the next seat that hasn't nominated. Once **every**
  seat has nominated in vain, the spy wins outright.

Scoring — every outcome is worth exactly **1 point**, so the shared leaderboard stays flat
across all of them:

| `outcome` | who scores |
|---|---|
| `caught` | the spy was named (by a press or a standing nomination) → every non-spy **+1** |
| `escaped` | an innocent was nominated through, or nobody pinned the spy → spy **+1** |
| `solved` | the spy called the location right → spy **+1** |
| `failed` | the spy called it wrong → every non-spy **+1** |
| `aborted` | the spy left, or the table fell under three → nobody scores |

Four rounds, then the podium.

Client intents: `ready`, `vote{pack}`, `seen`, `solve{loc}`, `accuse{pid}`,
`nominate{pid}`, `agree{in}`, `again`. `accuse`/`solve` are distinct intent names rather
than reusing `vote`/`guess`, which already carry pack votes and text/colour guesses for
other games.

Server `{t:"spyfall",phase,...}`:
- `"lobby"`: `you`, `need`, `players`, `packs` (name/votes), `myvote`.
- `"countdown"`: `sec`.
- `"play"`, all stages: `stage`, `round`, `rounds`, `me` (was I dealt into this round — a
  mid-round joiner is not, and waits for the next one), `spy` (am I the spy),
  `deadline`/`dur`, `scores`.
  - `loc` is sent to a **non-spy from the start of the round**, and to the **spy only on
    reveal**. There is no other field carrying it and the location index is never
    serialized, so nothing derivable leaks either.
  - `role` is sent **only to the player holding it**. The full `roles` table exists only
    on reveal.
  - `locs` (the pack's whole candidate list, in pack order — identical every round, so it
    carries no information about this round) goes **only to the spy**.
  - `"card"` adds `seen` / `total` / `myseen`.
  - `"talk"` adds `cands` (`pid`/`nick`/`avatar` of everyone in the round), `spent` (have I
    burnt my accusation) and `misses` (`by`/`of`, the failed accusations so far).
  - `"nominate"` adds `nomStage` (`hush`|`pick`|`poll`), `cands`, `need`, `nominator` +
    `nominatorNick`, `nomMe` (is it my turn), and in `poll` also `nominee` +
    `nomineeNick`, `agreed` and `myagree` (`-1` unanswered, `0` no, `1` in).
  - `"reveal"` adds `outcome`, `spyPid`/`spyNick`, `called` (the location the spy named, if
    they did), `blamedNick` (who the round ended on, absent when nobody was pinned),
    `misses`, `roles` (`pid`/`nick`/`role`/`spy`) and `mygain`.
- `"final"`: `board` (the shared leaderboard).

The card is hidden on the phone by default in `talk`/`nominate` and only shown while the
player **holds** the "Show my card" button; that is presentation, and the per-player gating
above is what actually makes it safe.

Timers, so nobody can stall the room: every wait has a deadline — the card
acknowledgement, the questioning clock, the hush, each nomination turn, and each agreement
poll. A nomination turn that expires passes on; a poll that expires resolves with whatever
was cast. If the spy disconnects (or the table falls under three players) the round ends
immediately as `aborted` with no scoring, and the rotation carries on into the next round.

---

## 13. Draw a Monster (`frankendraw`) — game id `20`

The exquisite-corpse drawing game, on the shared party skeleton (ready-up lobby,
countdown, final podium) but with its own three-round body. Select with UART
`SELECT_GAME` id `20`; lobby `game` string `"frankendraw"`. Firmware **v20**. No content
packs — its UI strings are localized client-side from the message catalog.

Everyone starts a sheet and everyone draws **at the same time**: round 1 the head, round
2 the torso, round 3 the legs, 75 seconds each (the round ends early once every player
has tapped Next). Between rounds the sheets **rotate one seat**, so each sheet is drawn
by three different players. Minimum three players.

**Sheet geometry.** A sheet is a `unit` x `unit` grid (255) split into three equal
`band`s of 85; panel *p* owns `[p*band, p*band+band]`. Strokes go up as the same
`{t:"stroke",x0,y0,x1,y1}` Draw & Guess uses — normalised 0..1 over the whole sheet — and
the server quantises them onto the grid and clamps both endpoints into the sender's own
band. Ink comes back down in grid units, as flat `[x0,y0,x1,y1, ...]` arrays. The client
also draws a faint join mark (a dashed line, two ticks and a "neck"/"legs" label) on each
band edge so a torso starts near the neck it is continuing; that is presentation only —
the whole band stays drawable.

**Ink budget.** A panel holds `cap` segments (192, see the memory note). `used` is how
many it currently holds, so the client can render a filling ink bar and stop the pen at
the cap; the server refuses anything past it. `{t:"undo"}` drops the last segment of your
own panel and pushes, so `used` follows it back down. There is no "clear".

**What a drawer may see.** During a round the server sends a drawer exactly one thing:
the bottom `over` units (7, about 8% of a band) of the panel **directly above theirs**,
on the sheet **currently in their hands**. A segment is only included if *both* its
endpoints sit at or below that line, so a stroke that merely dips into the sliver cannot
drag its other end down with it. Nothing else is serialised — not the rest of that panel,
not the panel below, not any other sheet — and a drawer's own strokes are never echoed
back to them. Nothing is relayed live between players at all.

**Rotation, joining and leaving.** Seats are frozen when the game starts (everyone
connected then, in pid order). In round *r*, seat *k* holds sheet `(k + seats - (r-1)) %
seats`. Joining mid-game gets no seat (`panel:-1`, `wait:true`) and plays from the next
game. Leaving vacates the seat for the rest of the game: the sheet in that player's hands
is **not** reassigned — everybody still playing is already holding a sheet, so handing it
on would mean drawing two panels at once — it simply rotates on to its next scheduled
holder, leaving that one panel blank. A panel is credited to whoever held it when the
round started.

**The gallery walk.** Each finished creature is shown to everyone for 5 s with a progress
bar, a name label on each band, and thumbs-up / thumbs-down buttons. `{t:"thumb",sheet,v}`
(`v` > 0 up, < 0 down) applies to the creature currently on screen and only during the
walk; the same thumb again takes it back. Every tap pushes the (small) state message to
everyone, so the counts move live for the whole room. After the last creature the winner
is the highest **net** score (ups minus downs) and is **shown again for 8 s** as the
finale. Ties break on the most thumbs-up, then on the creature shown first (lowest sheet
index) — deterministic, because a coin flip is impossible to explain to the room.

**Scoring.** Each sheet pays its net score x 100 points to each of its three
contributors, floored at zero, so a creature the room disliked earns nothing rather than
costing the people who drew it. With exactly three players every sheet has the same three
contributors, so the podium is flat by construction and the crowned creature is the
result.

Client intents: `ready`, `stroke{x0,y0,x1,y1}`, `undo`, `done`, `thumb{sheet,v}`, `again`.

Server `{t:"frankendraw",phase,...}`:
- `"lobby"`: `you`, `players`, `need` (3).
- `"countdown"`: `sec`.
- `"draw"`: `round`, `rounds` (3), `unit`, `band`, `over`, `cap`, `deadline`/`dur`,
  `scores`, and per player either `panel:-1`/`wait:true` (no seat) or `panel`, `top`,
  `bot`, `sheet`, `used`, `done`, `waiting` (how many are still drawing) and `ink` (the
  sliver above).
- `"show"`: `n`, `total`, `final` (is this the winner's encore), `up`, `down`, `mine`
  (your thumb, -1/0/1), `deadline`/`dur`, plus `net` on the finale. Deliberately **no
  ink**: see `fdart` below.
- `"final"`: `best` (sheet index), `net`, `who`, `board` (the shared leaderboard).

`{t:"fdart"}` carries the picture itself — `n`, `total`, `unit`, `band`, `who` (the three
drawers, `""` where a panel went undrawn, used for the band labels) and `ink` (an array of
three flat panel arrays). It is **broadcast once** when the gallery advances, rather than
pasted into every `"show"` push, because it is two orders of magnitude bigger than that
message and `"show"` is re-pushed on every thumb tap. A phone that joins or reconnects
mid-creature gets one unicast copy.

**Memory.** Sheets live in a fixed array sized for the worst case
(`HA_MAX_PLAYERS` sheets x 3 panels x `FD_PANEL_STROKES` segments x 4 bytes; 28 KB of
`.bss` at 192) and are never grown — that budget is the `cap` above, sized to stay
comfortable on the smallest supported board (the ESP32-S2's 320 KB of SRAM, most of it
WiFi/lwIP and the heap-held web bundle). Saving is streamed through the `ART` report as
the gallery walks the sheets, a segment at a time, so keeping the pictures costs no
additional RAM on either side.
