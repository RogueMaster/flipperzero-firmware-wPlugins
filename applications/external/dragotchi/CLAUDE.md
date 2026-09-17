# CLAUDE.md — Dragotchi

Guidance for an AI agent (or any dev) picking this project up. Read this first.

## What this is

**Dragotchi** — a dragon-raising virtual pet FAP for the **Flipper Zero**. GPLv3
fork of MrModd's [Matagotchi](https://github.com/MrModd/Matagotchi). It adds
care-driven branching evolution, and — the distinctive part — it **senses the
real RF world**: hunting reads the sub-GHz airwaves, and an optional ESP32-S2
WiFi devboard adds a WiFi-density "signal storm".

- **Current version:** 0.4.0, branch `main`.
- **Public source:** https://github.com/Mohnki/dragotchi-flipper (`origin`, ssh; `gh` is authed as account **Mohnki**).
- **Apps Catalog submission:** PR https://github.com/flipperdevices/flipper-application-catalog/pull/1232 (pending moderation as of 2026-09-12).
- **Target firmware:** 1.4.3 / **API 87.1**, target `f7` (the current release, matched by the catalog build).

## Build / test / deploy

- **Toolchain:** `ufbt` at `~/Projects/FlipperZero/.ufbt-venv/bin/ufbt` (SDK pinned to release 1.4.3). If missing: `pip install ufbt && ufbt update --channel release`.
- **Build:** run `ufbt` in the repo root → `dist/dragotchi.fap`.
- **Host tests (pure logic, no device):** `make -C test/host run` → must print `ALL OK`. Do this before every build; the game logic is TDD'd here.
- **DEBUG (fast) build:** `ufbt --extra-define=DEBUG` — compresses all timescales in `src/tuning.h` (ages/cooldowns in seconds) for on-device testing.
- **Deploy + run (preferred):** `ufbt launch` (builds, uploads to `/ext/apps/Games/dragotchi.fap`, runs). Or on device: Apps → Games → Dragotchi.
- **Deploy over serial (fallback, headless):** the Flipper is a USB CDC device at `/dev/ttyACM0` (VID:PID `0483:5740`; a udev symlink `/dev/flipper` may exist). Upload with the storage RPC/CLI, then `loader open /ext/apps/Games/dragotchi.fap`. Confirm with `loader info` (→ `Application "Dragotchi" is running`). **`storage md5 <path>` vs local `md5sum` verifies a deploy.** NOTE: the CLI `input` command does NOT reach a running FAP — you cannot script button presses; drive the UI physically and verify state by reading the save back.

## Architecture (how the code is laid out)

Two threads over a `FuriMessageQueue`: a **GUI thread** and a **game-logic
thread** (`src/threads.c`). Pure game logic lives behind thin hardware seams so
it is host-testable.

- `src/game_structs.h` — the data model. `PersistentGameState` (POD, saved) + transient `GameState` fields.
- `src/hunt.c/.h` — catch rolls, cooldown, signal-storm math (pure, host-tested).
- `src/hunt_hw.c` — **device-only** sub-GHz RSSI sensing (stubbed on host as `test/host/hunt_hw_stub.c`).
- `src/esp_link.c/.h` — **device-only** WiFi devboard UART link (no host stub; only referenced from `state_management.c`, which the host build does not compile).
- `src/expedition.c`, `src/economy.c`, `src/game_logic.c`, `src/states.c`, `src/needs.c`, `src/evolution.c`, `src/care.c`, `src/discipline.c` — pure logic.
- `src/state_management.c` — orchestration (`do_forage`, `do_expedition`, `init_state`, `tick_state`); calls the device seams. **Not** in the host build.
- `src/gui/` — custom views (`pet_view.c`, `stats_view.c`, `inventory_view.c`, `storm_view.c`) + scenes (`gui/scenes/*.c`).
- `src/tuning.h` — ALL balance constants, with `#ifdef DEBUG` vs normal profiles.
- `application.fam` — manifest (`sources` globs `src/*.c`, `src/gui/*.c`, etc., so new files in those dirs are picked up automatically). `requires=["gui","expansion"]`.
- `test/host/` — host harness: compiles pure logic against a `furi` stub with a **seedable xorshift RNG** and a clock seam. Add a test → register in `tests.h` + `test_runner.c`.

**Adding a GUI scene** (e.g. how `storm` was added): edit `gui/scenes/scenes.h`
(enum + all 3 handler arrays), `entry_point.c` (alloc / `add_view` / `remove_view`
/ free), and `flipper_structs.h` (`ApplicationContext`). A custom animated view
can drive itself with a `FuriTimer` started in the scene's `on_enter` and stopped
in `on_exit` (see `storm_scene.c` + `storm_view.c`, ~12 fps).

## Critical quirks / gotchas (these cost real time to rediscover)

1. **Sub-GHz from a FAP works** but: `subghz_devices_begin()` returns `false`
   even though the radio is usable — **ignore its return**; and the sweep needs
   a **≥4 KB secondary-thread stack** (1 KB overflows → crash). See `hunt_hw.c`
   and the stack size in `entry_point.c`. Receive-only, so region limits don't apply.
2. **Save format:** `toolbox/saved_struct` — 8-byte header `[magic, version,
   checksum=sum(payload)&0xFF, flags, timestamp(4)]` + POD payload. Current magic
   `0xDA` (`GAME_STATE_HEADER_MAGIC` in `constants.h`), `PersistentGameState` is
   **120 bytes** (`eggs_storm` at offset 118 fit in existing padding, so size
   stayed 120). **Any field that changes `sizeof` → old save rejected → fresh
   egg.** Save path on device: `/ext/apps_data/dragotchi/dragotchi.save`.
3. **Re-crafting a pet after a wipe** (read a fresh save for the correct format,
   then patch): key offsets — `stage@0`, `alignment@1`, `birth_timestamp@4`,
   `stage_entered_timestamp@8`, `hunger@12`, `happiness@20`, `health@28`,
   `care_score@68`(int32), `discipline@72`, `eggs_storm@118`. Set `birth =
   now - N*86400` for an N-day pet, `stage_entered = birth + AGE_<STAGE>`, leave
   the `last_*_update` cursors from the fresh save (= now) so it doesn't drain,
   recompute the checksum, push. (`now` = a fresh egg's `birth_timestamp`.)
4. **ESP32-S2 devboard = WiFi only, NO Bluetooth.** The Flipper itself has no
   onboard WiFi and BLE is peripheral-only (no scan API for FAPs). So all real-RF
   features are sub-GHz (onboard) + WiFi density (devboard).
5. **`esp_link.c`** must `expansion_disable(RECORD_EXPANSION)` before
   `furi_hal_serial_control_acquire(FuriHalSerialIdUsart)` and `expansion_enable`
   after — required for a FAP to use the expansion USART.

## ESP32 WiFi devboard ("signal storm")

- Firmware + docs live in `esp32/` (MicroPython + `main.py` reporter, `flash.sh`, `upload_main.py`, `README.md`).
- Reporter emits `DRAGO wifi=<n> rssi=<-dbm>` at 115200 8N1 on ESP UART0 pins (GPIO43 TX / GPIO44 RX), wired by the devboard to the Flipper's expansion USART (pins 13 TX / 14 RX). It **transmits the cached scan continuously (~5/s)** and rescans on a slow timer — a blocking `wlan.scan()` once per loop was too sparse and the Flipper's ~1.5 s listen window missed it.
- `esp_link.c` `esp_probe()` listens ~1.5 s during a Hunt; if it hears a `DRAGO` line the catch becomes a storm (boosted odds, guaranteed floor at 8+ APs, WiFi-themed names, exclusive "storm egg", animated radar screen). No board → normal sub-GHz hunt.
- **Flashing quirks:** manual download mode only (hold BOOT, tap RST → USB `303a:0002`); no auto-reset circuitry (native USB). **Flash at default baud** — 460800 fails mid-erase over the S2's native USB. Running MicroPython enumerates as `303a:4001`. `esptool` needed (`pip install esptool`). This OVERWRITES Marauder (reversible: reflash the Marauder image).

## Publishing an update to the Flipper Apps Catalog

1. Bump `fap_version` in `application.fam` (catalog needs a strictly higher version each submission; it inherits from the fam).
2. Take fresh **qFlipper** screenshots (native export, don't resize) → `screenshots/`. The catalog REQUIRES real qFlipper captures.
3. Commit + push to `origin`; note the new commit SHA.
4. Update `commit_sha` in the catalog `manifest.yml` (`applications/Games/dragotchi/manifest.yml`).
5. Validate locally from a catalog checkout: venv + `pip install pyyaml ufbt jsonschema dataclass-wizard==0.26.0 Markdown==3.7 requests==2.32.3 Pillow` (the pinned `Pillow==10.4.0` fails to build on Python 3.12 — a newer Pillow is fine), then `UFBT_HOME=$PWD/venv/ufbt ufbt update` and `python3 tools/bundle.py --nolint applications/Games/dragotchi/manifest.yml bundle.zip` → `Bundle created` = valid.
6. PR the manifest change to `flipperdevices/flipper-application-catalog` (fork is `Mohnki/flipper-application-catalog`; branch scheme `Mohnki/dragotchi_<version>`). Fill the AI-usage disclosure honestly.

## Conventions

- Match the surrounding C style (no `strcat`/`strncpy` — they're disabled in the Flipper API; use `snprintf`/manual copies). Watch `-Werror=misleading-indentation` and `format-truncation` (size snprintf buffers generously).
- Keep new game logic **pure and host-tested**; keep device I/O behind seams.
- `DRAGOTCHI_VERSION` in `constants.h` (menu header) and `fap_version` in `application.fam` should stay in sync.
- Commit attribution when asked to commit: see the session's attribution lines.

## Current device state (as of last session, 2026-09-12)

The Flipper's save is a **day-3 Drake** (care 80, meters full). The devboard is
flashed with the MicroPython reporter and works on the GPIO header (verified
`ok=1 wifi=18–24`). v0.4.0 is deployed and committed; catalog PR #1232 pending.
