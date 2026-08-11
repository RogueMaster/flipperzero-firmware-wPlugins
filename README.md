<p align="center">
  <img src="docs/img/logo.png" alt="Hotspot Arcade" width="600">
</p>

# Hotspot Arcade

[![build](https://img.shields.io/github/actions/workflow/status/tarikbc/hotspot-arcade/build.yml?style=for-the-badge&color=ff8200)](https://github.com/tarikbc/hotspot-arcade/actions/workflows/build.yml)
[![release](https://img.shields.io/github/v/release/tarikbc/hotspot-arcade?sort=semver&style=for-the-badge&color=ff8200)](https://github.com/tarikbc/hotspot-arcade/releases/latest)
[![downloads](https://img.shields.io/github/downloads/tarikbc/hotspot-arcade/total?style=for-the-badge&color=ff8200)](https://github.com/tarikbc/hotspot-arcade/releases)
[![license](https://img.shields.io/badge/license-MIT-ff8200?style=for-the-badge)](LICENSE)

**Offline multiplayer party games hosted from a Flipper Zero + ESP32 WiFi board.**
No internet, no app install. You host an open WiFi network from the Flipper; people
nearby join it, a captive page hands them into a game in their phone browser, and
everyone plays together over the local network. Built for dead zones: buses, planes,
subways, campsites, anywhere with no signal.

The Flipper is the **game master**: it shows the lobby and live scoreboard and drives
the rounds. The ESP32 board is the **referee**: it runs the WiFi access point, serves
the game to phones, and keeps the real-time game state. See
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

Seventeen games, all phone-driven. Pick your emoji avatar on the way in and fire off emoji
Sixteen games, all phone-driven. Pick your emoji avatar on the way in and fire off emoji
Sixteen games, all phone-driven. Pick your emoji avatar on the way in and fire off emoji
Seventeen games, all phone-driven. Pick your emoji avatar on the way in and fire off emoji
reactions that float up on everyone's screen mid-game.

**Whole-group** (scale to everyone in the room, ready-up lobby, shared live leaderboard):

- **Trivia** — Kahoot-style and fully self-organizing. Players ready up and vote a topic
  in the lobby; an all-ready 5-second countdown starts it; phones buzz in A/B/C/D with
  points for correct and fast; a collapsible leaderboard rides along and a podium ends
  it. Topics are the trivia packs on the SD card.
- **Would You Rather** — a live A/B poll; tap your pick, watch the split reveal. Prompts
  are the wyr packs on the SD card, votable in the lobby.
- **Word Scramble** — unscramble the word and type it first; fastest correct scores most.
  Words are the scramble packs on the SD card, votable in the lobby.
- **Reaction Duel** — fastest finger: wait for green, tap first to win, false-start and
  you're out for the round.
- **Guess the Color** — a random color swatch appears; dial in its R/G/B (0-255) with a
  slider per channel and submit. Closest guess wins the round, with a speed bonus, over
  five rounds to a podium. You never see a preview of your color while guessing.
- **Spectrum** — a Wavelength-style guessing game. Each round one player is the psychic:
  they see a hidden target on a 0-100 spectrum between two opposing words (Cold to Hot)
  and type a clue; everyone else slides a dial to guess where it lands. Points by
  closeness, and the psychic scores by how well the group guesses, so a good clue pays
  off. Six rounds, rotating psychic. Prompts are the spectrum packs on the SD card.
- **Kiss Marry Kill** — each round a rotating chooser secretly labels three people (drawn
  from the pack) Kiss, Marry, and Kill; everyone else predicts the chooser's assignment.
  Points for matching positions, and the chooser scores by how well the group reads them.
  Six rounds. People are the kmk packs on the SD card.
- **Secrets** — each round shows a yes/no question. Everyone first secretly answers, then
  secretly predicts how many of the group said "yes". Only the total yes-count is ever
  revealed, never who answered what — an exact prediction scores 1, anything else 0.
  Six rounds. Questions are the secrets packs on the SD card, votable in the lobby.
- **Fill the Blank** — a prompt card with a blank goes up; everyone but the rotating Czar
  plays an answer card from their hand, and the played cards are shuffled and shown
  anonymously for the Czar to judge. The winning card scores 1 and the Czar scores 1 too
  for picking a real player's card — because every round also mixes in one answer card
  drawn at random by the deck itself, and if the Czar falls for that one, nobody scores.
  The reveal then shows every card with the player who played it. Six rounds. Inspired by
  Cards Against Humanity; the cards shipped here are original and written for this
  project. Prompts and answers are the fillblank packs on the SD card.
- **Werewolf** — the social deduction classic, with the phones as referee: they deal the
  secret roles (werewolves, a seer, a doctor from six players up, the rest villagers) and
  run the clock, while the arguing happens out loud in the room. Night: the wolves converge
  on a victim (their phones are the only way they can coordinate — no whispering), the seer
  privately checks one player, and the doctor shields one, which can make the kill fail.
  Day: the outcome is announced and the village votes someone out, role revealed — early on
  a majority hammer, and a tied vote hangs nobody. Repeat until the wolves are gone or they
  outnumber the village. Needs five players; no packs — the roles are code.
- **Spyfall** — everyone at the table shares a secret location and holds a role there
  (Beach: lifeguard, surfer, sunburnt tourist...) except one player, the spy, who is told
  neither and only sees the list of possible locations. Read your card, then it goes away:
  after that **Show my card** only reveals it while you hold the button. Six minutes of
  questioning each other out loud, and the round ends the moment somebody dares to press
  **I know the spy** (open to everyone, the spy included, as cover — get it wrong and
  that's your one accusation gone) or **I know the location** (the spy's gamble). If the
  clock runs out with nobody daring, the table goes round one seat at a time nominating a
  suspect, and a nomination only sticks if everyone else backs it. Four rounds, rotating
  spy. Locations are the spyfall packs on the SD card.
- **Draw a Monster** — the exquisite-corpse drawing game. Everyone starts a sheet and draws
  a head in the top third; the sheets then rotate one seat so somebody else adds a torso,
  and rotate again for the legs — and all you ever see of the panel above yours is a
  thin sliver to line up with. The finished creatures then walk past one at a time, each
  band labelled with the player who drew it, and the room votes thumbs-up / thumbs-down
  live as it watches; the best net score is crowned and shown again. The host keeps every
  sheet as an SVG on the Flipper’s SD card. Needs three players. No packs.

**1v1 duels** (challenge a player, many matches at once, rematch button, wins score on
the Flipper leaderboard):

- **Connect Four**, **Tic-Tac-Toe**, **Dots & Boxes**, **Reversi/Othello**.
- **Pong** — real-time rally with on-screen paddles.
- **Battleship** — place a hidden fleet, then fire at the enemy grid; a hit lets you fire
  again, sink all five ships to win.
- **Chess** — full FIDE rules refereed on the board, 5-minute blitz clock per side; offer
  or claim a draw, or resign.

**Cooperative-ish:**

- **Drawing & guessing** — one player draws on their phone canvas, everyone else guesses
  in a chat; points for the drawer and the first correct guess; rounds rotate the drawer.
  Words are the draw packs on the SD card (no vote strip — the first pack streamed is
  the one played).

All games run on one pluggable engine on the ESP (the real-time referee), and the web
client shares one implementation of the lobby, countdown, timer, leaderboard, and podium,
so adding a game is mostly a small module on each side.

## Screenshots

**Phone game client** — the retro, Flipper-flavored web app players open in their browser.
Pick a nickname and an emoji avatar, then land in the lobby:

<p align="center">
  <img src="docs/img/web-landing.png" alt="Landing screen: nickname entry and emoji avatar picker" width="19%">
  <img src="docs/img/web-trivia-lobby.png" alt="Trivia lobby: ready up and vote a topic with live tallies" width="19%">
  <img src="docs/img/web-trivia.png" alt="Trivia question with A/B/C/D tiles and a collapsible leaderboard" width="19%">
  <img src="docs/img/web-trivia-reveal.png" alt="Trivia reveal: correct answer, per-option counts, live leaderboard" width="19%">
  <img src="docs/img/web-trivia-final.png" alt="Trivia final podium" width="19%">
</p>

The other phone games — the shared-lobby party games (Would You Rather, Word Scramble,
Reaction Duel, Guess the Color), Draw &amp; Guess, and a real-time Pong (animated below):

<p align="center">
  <img src="docs/img/web-wyr.gif" alt="Would You Rather: A/B poll with the live vote split" width="19%">
  <img src="docs/img/web-scramble.png" alt="Word Scramble: unscramble the letters and type the word" width="19%">
  <img src="docs/img/web-react.png" alt="Reaction Duel: fastest finger with reaction time and leaderboard" width="19%">
</p>
<p align="center">
  <img src="docs/img/web-guesscolor.gif" alt="Guess the Color: dial in the swatch's RGB with a slider per channel, then the reveal" width="19%">
  <img src="docs/img/web-spectrum.gif" alt="Spectrum: the psychic's clue points at a hidden target on a Cold-to-Hot dial; guessers slide to it" width="19%">
  <img src="docs/img/web-kmk.gif" alt="Kiss Marry Kill: the chooser secretly labels three people Kiss/Marry/Kill and everyone predicts it" width="19%">
  <img src="docs/img/web-draw.gif" alt="Draw &amp; Guess: the drawer's canvas with strokes forming and the secret word" width="19%">
  <img src="docs/img/web-pong.gif" alt="Pong: real-time 1v1 rally with on-screen paddle controls" width="19%">
</p>

The social-deduction and party batch (Fill the Blank, Werewolf, Spyfall, Draw a Monster):

<p align="center">
  <img src="docs/img/web-fillblank.gif" alt="Fill the Blank: a prompt with a gap, a hand of answer cards, and the reveal naming every card" width="19%">
  <img src="docs/img/web-werewolf.gif" alt="Werewolf: the werewolf picks a night victim, then the village votes a wolf out at dusk" width="19%">
  <img src="docs/img/web-spyfall.gif" alt="Spyfall: the spy's card, the questioning options, and the reveal after the spy calls the location" width="19%">
  <img src="docs/img/web-frankendraw.gif" alt="Draw a Monster: draw a head, carry on from the sliver, and the assembled creature reveal" width="19%">
</p>

The 1v1 board duels (Connect Four, Tic-Tac-Toe, Dots &amp; Boxes, Reversi, Battleship, Chess):

<p align="center">
  <img src="docs/img/web-connect4.png" alt="Connect Four: 7x6 board mid-game, your turn" width="19%">
  <img src="docs/img/web-ttt.png" alt="Tic-Tac-Toe: 3x3 duel, your turn" width="19%">
  <img src="docs/img/web-dots.png" alt="Dots &amp; Boxes: claimed boxes and live score" width="19%">
  <img src="docs/img/web-reversi.gif" alt="Reversi/Othello: 8x8 board with legal-move hints and disc counts" width="19%">
  <img src="docs/img/web-battleship.gif" alt="Battleship: place a fleet, then fire at the enemy grid with hits, misses, and sinks" width="19%">
  <img src="docs/img/web-chess.gif" alt="Chess: full FIDE rules with legal-move hints, blitz clocks, and a checkmate finish" width="19%">
</p>

**On the Flipper** — the host device shows the app menu, the live broadcasting dashboard
(join info + players + active game), and the game selector on its 1-bit screen. Every game
is phone-driven, so the Flipper selects the game and keeps score (a Console in the menu
carries the live event log):

<p align="center">
  <img src="docs/img/flipper-menu.png" alt="Flipper app menu: Start Session, SSID, Install Firmware" width="31%">
  <img src="docs/img/flipper-dashboard.png" alt="Flipper broadcasting dashboard: join address, player count, Games/Scores" width="31%">
  <img src="docs/img/flipper-gameselect.png" alt="Flipper game selector: Word Scramble, Reaction Duel, Connect Four" width="31%">
</p>

## Hardware

- **Flipper Zero** (developed on **Momentum** firmware; other forks work with a matching
  `ufbt` SDK).
- **An ESP32 WiFi board** on the GPIO header, wired to the Flipper over UART — the
  **official Flipper WiFi Dev Board (ESP32-S2)**, an **ESP32 WROOM** board, or an
  **ESP32-C5** board. You pick your board when flashing from the Flipper.

**No Flipper?** There's a community port to the
[**M5Stack Cardputer**](https://github.com/genkigenki/hotspot-arcade-cardputer) by
[@genkigenki](https://github.com/genkigenki) — a single ESP32-S3 device that is both the
access point and the host UI, so it needs no Flipper. It vendors this engine unmodified, so
the games stay in sync.

## How it works

```
 Phones (browser)  <-- WebSocket -->  ESP32     <-- UART 921600 -->  Flipper Zero
   play the game                     AP + web + referee              host / scoreboard
```

- The ESP hosts an **open AP + wildcard DNS + catch-all web server**, so joining the
  WiFi pops a captive page on every phone.
- The captive page hands off to the game web app at `http://192.168.4.1` (captive
  mini-browsers are too limited for WebSockets, so it is a "tap to open in your browser"
  handoff).
- The Flipper streams the (gzipped) web bundle and content packs to the ESP over a
  framed UART protocol, then orchestrates rounds. The web bundle is stored in a **LittleFS
  flash partition** on the ESP and served from flash (so it costs no RAM and survives a
  reboot); the Flipper re-streams it only when it changes — the board reports the bundle's
  CRC in its beacon and the Flipper skips the transfer when it already matches. Real-time
  game traffic stays on the ESP and never crosses the slow UART. Protocol:
  [docs/PROTOCOL.md](docs/PROTOCOL.md).

## Install

**You only need `hotspot_arcade.fap`.** Grab it from the
[latest release](https://github.com/tarikbc/hotspot-arcade/releases/latest) and drop it in
`/ext/apps/GPIO/` on the SD card (qFlipper, or the Flipper's own file manager). No SD
setup, no separate downloads: the ESP firmware, the phone game bundle, and the content
packs all ship inside the .fap.

> **First launch can take up to 3 minutes.** The .fap carries about 3 MB of bundled
> content (three board firmwares, the web bundle, the packs) and the Flipper unpacks it to
> the SD card the first time you open the app (and again after an update). The hourglass is
> the system loader doing that, not a hang, so give it a minute or two. Every launch after
> that is instant.

Then, on the Flipper: **Apps → GPIO → [ESP32] Hotspot Arcade**.

### Flashing the ESP board (no computer)

The app embeds Espressif's `esp-serial-flasher` and carries the ESP firmware, so the board
is flashed from the Flipper itself: use **Install Firmware** in the main menu (then pick
your board — the official S2 dev board, an ESP32 WROOM, or an ESP32-C5), or accept the prompt the lobby
shows when it doesn't see a board (or sees one on older firmware).
Put the ESP in download mode when asked (**hold BOOT, tap RESET, release BOOT**); it
verifies with MD5, then asks you to **tap RESET** to boot the new firmware, and continues
on its own once the board comes back.

Prefer a computer? `firmware-merged.bin` on the release flashes at `0x0` with esptool
(**never `--erase-all` on the S2**).

### Custom content

The bundled web bundle and content packs live in `/ext/apps_assets/hotspot_arcade/`, which
the loader rewrites from the .fap on every launch. To add your own, use
`/ext/apps_data/hotspot_arcade/` instead, which is never touched:

- `packs/<game>/*.txt` — your packs are offered alongside the bundled ones (yours win a
  name clash). One directory per game, e.g. `packs/trivia/`.
- `web/` — a `manifest.json` here replaces the bundled game client entirely.

## Build from source

Full commands and gotchas are in [CLAUDE.md](CLAUDE.md); the short version:

**1. Web bundle** (Node)
```sh
cd web && node build.mjs        # -> web/dist/{index.html.gz, manifest.json}
```

**2. ESP32 firmware** (arduino-cli, esp32 core 2.0.17 for S2/WROOM and 3.x for the C5,
vendored libs in `esp32/libs`). One sketch, built once per supported board —
`tools/build-fap.sh` does them all for you, or by hand:
```sh
# Official dev board (ESP32-S2)
arduino-cli compile --fqbn esp32:esp32:esp32s2:PartitionScheme=huge_app \
  --libraries esp32/libs --output-dir esp32/hotspot-arcade-fw/build/official_devboard esp32/hotspot-arcade-fw
# ESP32 WROOM
arduino-cli compile --fqbn esp32:esp32:esp32 \
  --libraries esp32/libs --output-dir esp32/hotspot-arcade-fw/build/wroom esp32/hotspot-arcade-fw
# ESP32-C5 (needs the 3.x core: arduino-cli core install esp32:esp32@3.3.11)
arduino-cli compile --fqbn esp32:esp32:esp32c5:PartitionScheme=huge_app,CDCOnBoot=default \
  --libraries esp32/libs --output-dir esp32/hotspot-arcade-fw/build/c5 esp32/hotspot-arcade-fw
```

**3. Flipper app** — use the wrapper, not bare `ufbt`: it refreshes the bundled firmware
images, web bundle, and content packs inside `assets/` before packaging.
```sh
tools/build-fap.sh                         # -> dist/hotspot_arcade.fap
python3 tools/deploy-to-flipper.py --port /dev/cu.usbmodemflip_XXXX
```
The deploy script pushes the fap to `/ext/apps/GPIO/` and your working copies of the web
bundle and content packs to `/ext/apps_data/hotspot_arcade/`, where they override the
bundled ones — so you can iterate on the web client without rebuilding the fap.

## Development

You don't need a Flipper or an ESP board to work on the games. `sim/` compiles the
**real** ESP game engine (`esp32/hotspot-arcade-fw/ha_games.h`) to WebAssembly and runs it
in a browser page, with 2–8 phone panels (each an iframe of the real phone client) plus a
data-faithful Flipper panel — enough to play and design games solo. It exists because an
earlier JS reimplementation of the game rules drifted four games behind the firmware;
running the real engine means it can't drift.

```sh
brew install emscripten     # one-time
cd web && node build.mjs     # build the phone client
cd .. && sim/engine/build.sh # build the engine
sim/serve.sh                 # -> http://localhost:8123/sim/web/
```

Headless tests: `sim/test/all.sh`. Memory-bug hunting: `sim/engine/build.sh --asan &&
sim/test/all.sh`. Full docs, including caveats and what still has to be hand-kept in sync
with the firmware, in [sim/README.md](sim/README.md).

New games and fixes are welcome. See [CONTRIBUTING.md](CONTRIBUTING.md) for the build/test
loop and the "adding a game" checklist (a `pr-checklist` action reviews PRs against it).

## Usage

On the Flipper: **Apps → GPIO → [ESP32] Hotspot Arcade**.

1. **Set the SSID** (optional). Content packs are picked up automatically.
2. **Start Session** — the ESP brings up the AP; the dashboard shows **Broadcasting**.
3. People **join the WiFi** and open `192.168.4.1`, pick a nickname, and land in the lobby.
4. **Games** → pick a game. Everything is player-driven from the phones: the whole-group
   games (Trivia, Would You Rather, Word Scramble, Reaction Duel) self-run once players ready
   up; the duels (Connect Four / Tic-Tac-Toe / Dots & Boxes / Reversi), **Drawing**, and
   **Pong** organize themselves too. The dashboard **Feed** watches events.
5. **Leaderboard** shows live scores; **Console** shows the raw event log.

## Content packs

Seven games are content-driven from plain-text files under `packs/`, one directory per
game (`trivia/`, `wyr/`, `scramble/`, `draw/`, `spectrum/`, `kmk/`, `spyfall/`). Format:
`Key: value` lines, blocks split by `---` or a blank line, `Pack:` names the pack. The
keys are per game — e.g. Trivia uses `Q:`, `A:`-`D:` and `Answer:`; Would You Rather uses
`A:` / `B:`; Word Scramble and Draw &amp; Guess use `Word:`; Spyfall uses `Loc:` plus one
`R:` line per role. Packs ship inside the .fap; drop your own
into `/ext/apps_data/hotspot_arcade/packs/<game>/` to add to them (yours win a name
clash). See [packs/README.md](packs/README.md).

**Languages.** The host picks a language in Settings, and both the phone UI and the game
content follow it. English is the default; **Brazilian Portuguese** ships as the first
translation — the phone UI is fully localized, with a starter content pack per game.
Translated packs live in a `<lang>/` subdirectory (`packs/<game>/pt-br/`), falling back to
English per game, and content is UTF-8. The Flipper's own host menus stay English.

## Responsible use

Hotspot Arcade runs an **open** WiFi access point and a captive page that serves a game.
It is for fun and learning on your own hardware, among people who want to play. Running
an open AP may be restricted in some places (e.g. on aircraft, or where it could
interfere) — only operate it where that is allowed. It captures no credentials and
serves only the bundled game.

## Layout

```
flipper/hotspot-arcade/   Flipper app (C, ufbt/Momentum) — host + scoreboard
esp32/hotspot-arcade-fw/  ESP32 firmware (Arduino) — AP + web + WebSocket referee
esp32/libs/               vendored AsyncTCP + ESPAsyncWebServer
web/                      phone game client (vanilla JS, gzipped bundle)
packs/                    content packs, one dir per game (trivia, wyr, scramble, draw)
sim/                      browser simulator — real engine compiled to WASM, no hardware
tools/deploy-to-flipper.py
docs/                     ARCHITECTURE.md, PROTOCOL.md
```

Sibling project to flytrap, which this reuses the AP/captive-portal plumbing and
Flipper UART patterns from.
