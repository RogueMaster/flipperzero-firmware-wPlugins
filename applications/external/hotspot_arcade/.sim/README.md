# Simulator

Runs the **real** ESP game engine (`esp32/hotspot-arcade-fw/ha_games.h`, compiled to
WebAssembly) in a browser page, with 2–8 phone panels and a Flipper panel. Lets you play
and design games without a Flipper, a dev board, or eight friends.

## Use

```sh
brew install emscripten     # one-time
cd web && node build.mjs     # build the phone client
cd .. && sim/engine/build.sh # build the engine
sim/serve.sh                 # -> http://localhost:8123/sim/web/
```

Each panel keeps its own saved identity. All panels are iframes on one origin and so
share one `localStorage`, which real phones don't — so with `?harness=<id>` the client
suffixes its saved-nickname key with that id (`storeKey()` in `web/core/app.js`).
Without it every panel would rejoin as whoever saved last. A side benefit: reloading
the page auto-rejoins each panel as itself, so the returning-player path is testable
here at all.

## Headless tests

```sh
sim/test/all.sh
```

Scripted sessions driven straight against the engine, no browser. Not run in CI (that
needs emsdk in the workflow).

## Finding memory bugs

```sh
sim/engine/build.sh --asan && sim/test/all.sh
```

The engine indexes fixed arrays by player id. On the ESP32 an out-of-bounds write
silently corrupts a neighbour; under ASan it aborts at the line.

## What this is not

No WiFi, no ESP heap accounting, no UART timing — the 8-phone scale test still needs
hardware. The Flipper panel shows UART traffic, not the real 1-bit screens.

`loadSamplePacks()` (`flipper.js`) has a hardcoded pack list (`general`, `movies`,
`science`). A new file dropped into `packs/trivia/` will not show up in the "Load
trivia packs" button until that list is updated by hand.

## Fidelity

Game rules come from the real engine, so they cannot drift. Several smaller things
are still hand-copied from the firmware and can drift if only one side is updated:

- The pack block parser (`trivia-packs.js`), which mirrors the generic `Key: value`
  block streamer in `ha_session.c`. Since the Flipper no longer interprets game content,
  this is now a copy of a much smaller grammar than before.
- The `HA_GAME_*` id table (`flipper.js`'s `GAMES` array), copied from
  `flipper/hotspot-arcade/ha_proto.h`. Adding a game to the firmware means adding it
  to `GAMES` too, or the Flipper panel's game picker won't offer it.
- `MAX_PHONES` (`phones.js`), copied from `AP_MAX_CONN` in the `.ino`.
- `WS_MSG_MAX` (`phones.js`), copied from the `.ino`'s same-named constant, used to
  drop oversized WebSocket sends the way the firmware does.
