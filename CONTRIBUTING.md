# Contributing to Hotspot Arcade

Thanks for hacking on this. New games and fixes are very welcome. This guide keeps PRs
mergeable in one pass — most of it exists because a real PR missed a step.

## Before you start

- **Branch off the latest `master`.** Game ids and the firmware version are append-only, so
  a PR cut from an old commit collides (that is exactly what happened once: a game came in
  numbered `11` after `11`/`12` were already taken). Rebase before you open the PR.
- **One change per PR.** A new game, a bug fix, or a content pack — not all three.
- Open an issue first for anything large or that changes the protocol, so we can agree on
  the shape before you build it.

## Build and test locally

Zero-install for most of this — only the ESP firmware needs a toolchain.

```sh
node web/build.mjs          # rebuild the phone bundle (web/dist/) — commit the result
bash sim/test/all.sh        # headless engine tests (needs the WASM engine, see sim/engine/build.sh)
sim/serve.sh                # http://localhost:8123/sim/web/ — the real client + engine in the browser
```

The simulator runs the **real** `ha_games.h` compiled to WASM with the real phone client, so
you can develop and demo a whole game without any hardware.

## Conventions

- **Firmware version:** bump `HA_FW_VERSION` when you change anything the ESP runs, and keep
  it **byte-identical** in both `esp32/hotspot-arcade-fw/ha_proto.h` and
  `flipper/hotspot-arcade/ha_proto.h`. The same goes for the `HA_GAME_*` ids and the CRC-8.
- **Web client is ES5.** `var`/`function` only — no `const`/`let`, arrow functions, or
  template literals (the bundle ships to old mobile browsers). Match the surrounding style.
- **Format C with `ufbt format`** (not a hand-run clang-format), from `flipper/hotspot-arcade/`.
- **Committed build outputs:** `web/dist/` and the bundled assets under
  `flipper/hotspot-arcade/assets/` are checked in. Rebuild `web/dist/` yourself
  (`node web/build.mjs`) and commit it. You do **not** need `arduino-cli` — the maintainer
  regenerates the ESP firmware images and `.bundled-fw.sha256` on merge via
  `tools/build-fap.sh`. Never hand-edit a committed `.bin`.
- The `bundled-assets` CI job is the alarm for stale outputs; if it fails on your PR, run
  `node web/build.mjs` and commit.

## Adding a game

A game is a pluggable module: engine logic on the ESP, a small phone module, and a few
registration lines. Pick the **next free** `HA_GAME_*` id (check the current max in
`ha_proto.h` — don't reuse or guess) and bump `HA_FW_VERSION`. Then wire it into every seam:

**Shared protocol** (`esp32/hotspot-arcade-fw/ha_proto.h` **and**
`flipper/hotspot-arcade/ha_proto.h`, kept identical): add `HA_GAME_<NAME>`, bump
`HA_FW_VERSION`.

**ESP engine** (`esp32/hotspot-arcade-fw/ha_games.h`): the game's state struct, its
constants, its methods, and the dispatch seams — `reset()`, the clear-on-switch chain,
`tick`, the intent handler in `onInput`, the state member, the per-player push, `gameName()`,
and (for a whole-group game) the `checkStart` aggregator, or (for a content-driven game)
`loadItem`. Whole-group games reuse the `Party` skeleton and pack pipeline; 1v1 games reuse
the challenge/`matchAccept`/`anyOnLeave` flow. Copy the nearest existing game of the same
kind — it shows every seam.

**Phone client:** `web/games/<name>.js` (an `A.handlers.<t>` handler), and register it in
`web/build.mjs`, `web/core/app.js` (`SCREENS`, `GAME_SCREEN`, `GAME_LABEL`),
`web/src/index.html` (a `<section>`), and `web/core/style.css`.

**Flipper UI:** `scenes/hotspot_arcade_scene_game_select.c` (enum entry, submenu item,
selected-item mapping, and the `on_event` case) and `scenes/hotspot_arcade_scene_lobby.c`
(the display-name case).

**Content packs** (if content-driven): `packs/<game>/*.txt` as `Key: value` blocks (keys are
lowercased on both sides; a `---` or blank line ends an item).

**Simulator:** add the game to `GAMES` in `sim/web/flipper.js` (and to `PACK_DIRS` if it has
packs).

**Tests:** `sim/test/<game>.mjs` driving the real engine headless, added to `sim/test/all.sh`.
Cover the main flow and any hidden-information rule (a player must never receive state they
shouldn't see).

**Docs & counts:** the game count and gallery in `README.md`, a `CHANGELOG.md` entry under
`[Unreleased]`, the count/list in `flipper/hotspot-arcade/catalog/DESCRIPTION.md`, and a new
section in `docs/PROTOCOL.md`. A README GIF is appreciated but the maintainer can add it.

## Opening the PR

Fill in the checklist in the PR template. It is the same list as above — it exists so nothing
gets silently dropped and the PR merges without a rebase round-trip.
