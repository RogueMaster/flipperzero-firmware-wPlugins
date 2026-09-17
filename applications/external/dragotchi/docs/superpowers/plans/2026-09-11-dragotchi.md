# Dragotchi Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fork MrModd's Matagotchi into a dragon-themed virtual pet with care-driven branching evolution (white/grey/black dragon, immortality at care ≥ 90), richer needs (happiness, hygiene, discipline, sleep), deepened offline simulation with attention calls, and hand-drawn 1-bit dragon art — building through to a working `.fap` on the owner's Flipper (firmware 1.4.3).

**Architecture:** Keep Matagotchi's two-thread model (GUI thread + logic thread over a `FuriMessageQueue`), per-feature `init/check/apply/get_text` module pattern, RTC offline fast-forward, and versioned save. Extend the game model with new needs and a care/evolution engine (all pure functions, unit-tested off-device against a `furi` stub). Replace the icon-only `button_panel` main screen with a custom `pet_view` (draw + input callbacks) that renders meters, the animated dragon, state icons, and a selectable action bar.

**Tech Stack:** C (C11), Flipper `ufbt` SDK pinned to firmware 1.4.3 / API 87.1, GCC (host) for the off-device test harness, Python 3 + Pillow for generating 1-bit PNG art.

**Spec:** `docs/superpowers/specs/2026-09-11-dragotchi-design.md` (read it alongside this plan).

## Global Constraints

- Target firmware **1.4.3 / API 87.1**, hardware `f7`. Every `.fap` must be built against an SDK whose API equals 87.1 or it will not load on the device. Verify with the FAP manifest before deploying.
- License: **GPLv3**. Retain MrModd's `LICENSE` and copyright notices verbatim; credit MrModd in `README` and the in-app About screen.
- Deploy path on device: `/ext/apps/Games/dragotchi.fap`. Upload via the verified `write_chunk` serial method or `ufbt launch`; confirm load with `loader open`.
- Frequent commits: one per completed task, GPLv3-clean. Commit trailers as configured for this repo/session.
- No placeholder art or logic ships — every stage/state has a real sprite before that feature is called done.
- Pure game-logic files must not `#include <furi.h>` for anything the host stub can't provide; keep hardware calls (RNG, RTC, sound, vibro) behind the existing `random_generator` / `settings_management` / a new `clock` seam so logic stays host-testable.

---

## Phase 0 — Toolchain & working baseline

### Task 0.1: Install and pin ufbt to firmware 1.4.3

**Files:**
- Create: `.gitignore` (ignore `.ufbt/`, `dist/`, `build/`, `*.fap`, host `test/build/`)

- [ ] **Step 1:** Install ufbt into a venv: `python3 -m venv ~/Projects/FlipperZero/.ufbt-venv && ~/Projects/FlipperZero/.ufbt-venv/bin/pip install ufbt`
- [ ] **Step 2:** Point ufbt at the release SDK: `ufbt update --channel release`. Then read the fetched SDK's API version: `cat ~/.ufbt/current/sdk/sdk.opts` (or `ufbt status`) and confirm it reports API **87.1** / target f7. If it reports a newer API, stop and reconsider (options: pin to the SDK matching 1.4.3, or update the device) — do not proceed with a mismatched SDK.
- [ ] **Step 3:** Write `.gitignore` with the entries above.
- [ ] **Step 4:** Commit: `git add .gitignore && git commit -m "chore: add gitignore; pin ufbt to 1.4.3 SDK (API 87.1)"`

### Task 0.2: Import Matagotchi as the fork base and rebrand to Dragotchi

**Files:**
- Create: all of MrModd's `src/**`, `assets/**`, `application.fam`, `LICENSE`, `matagotchi_10px.png` → copied into the repo, then renamed.
- Modify: `application.fam` (appid, name, entry_point, description, author credit, version), `README.md`.

- [ ] **Step 1:** Copy the cloned upstream tree (from the review clone) into the repo, preserving `LICENSE`. Commit it **unmodified** first as a clearly-labelled import: `git commit -m "Import MrModd/Matagotchi v1.0 as fork base (GPLv3)"` — this preserves attribution provenance.
- [ ] **Step 2:** Rename the app in `application.fam`: `appid="dragotchi"`, `name="Dragotchi"`, `entry_point="dragotchi_app"`, `fap_description="Raise a dragon"`, keep `fap_author` crediting MrModd + note fork, `fap_version="0.1"`, `fap_category="Games"`. Rename `matagotchi_app` → `dragotchi_app` in `src/entry_point.c`. Rename icon references (`matagotchi_icons.h` is generated from the `assets/` dir + app id — after renaming appid it becomes `dragotchi_icons.h`; update the two `#include "matagotchi_icons.h"` in `game_decoder.c` and `main_scene.c`).
- [ ] **Step 3:** Rewrite `README.md`: what Dragotchi is, that it's a GPLv3 fork of MrModd's Matagotchi with a link, build/install instructions.
- [ ] **Step 4:** Build: `ufbt` (from repo root). Expected: a `dist/dragotchi.fap` is produced with no errors.
- [ ] **Step 5:** Verify the FAP's manifest API: extract `.fapmeta` and confirm API 87.1 / target f7 (same check used during ZeroFIDO review).
- [ ] **Step 6:** Deploy and launch-test on device (Flipper on USB, main menu): upload to `/ext/apps/Games/dragotchi.fap`, `loader open Dragotchi`, confirm it starts (egg screen) with no crash. Close it.
- [ ] **Step 7:** Commit the rebrand: `git commit -m "Rebrand fork to Dragotchi; verify baseline builds and runs on 1.4.3"`

---

## Phase 1 — Off-device test harness

### Task 1.1: Host build + furi stub + seedable RNG + first test

**Files:**
- Create: `test/host/furi_stub.h`, `test/host/furi_stub.c` (minimal `FURI_LOG_*` no-ops, `furi_assert`, `furi_hal_random_get`), `test/host/rng_control.h` (seed the stub RNG), `test/host/test_runner.c` (tiny assert-based runner + `main`), `test/host/Makefile`.
- Modify: `src/random_generator.c` — route `furi_hal_random_get()` through a seam so the host build can seed it (device build unchanged).

- [ ] **Step 1: Write the failing test.** In `test/host/test_runner.c`:
```c
#include "rng_control.h"
#include "random_generator.h"
#include <assert.h>
#include <stdio.h>
static int fails = 0;
#define CHECK(c) do{ if(!(c)){ printf("FAIL %s:%d %s\n",__FILE__,__LINE__,#c); fails++; } }while(0)
static void test_rng_is_seedable(void) {
    rng_seed(1234);
    uint32_t a = random_uniform(0, 100);
    rng_seed(1234);
    uint32_t b = random_uniform(0, 100);
    CHECK(a == b);              // same seed -> same sequence
    CHECK(a < 100);
}
int main(void){ test_rng_is_seedable(); printf(fails?"FAILED %d\n":"OK\n",fails); return fails?1:0; }
```
- [ ] **Step 2: Run to verify it fails.** `make -C test/host` → expect a link/compile failure (`rng_seed` undefined).
- [ ] **Step 3: Implement.** Add a `furi_hal_random_get()` in `furi_stub.c` backed by a deterministic PRNG (e.g. xorshift32) with `rng_seed(uint32_t)` in `rng_control.h`. Make `random_generator.c` compile unchanged against the stub (it already only calls `furi_hal_random_get`). Write the `Makefile` to compile `src/random_generator.c` + stub + runner into `test/host/build/tests`.
- [ ] **Step 4: Run to verify it passes.** `make -C test/host && ./test/host/build/tests` → `OK`.
- [ ] **Step 5: Commit.** `git commit -m "test: host harness with furi stub and seedable RNG"`

---

## Phase 2 — Game model (pure logic, TDD)

> All Phase 2 tasks add real test code to `test/host/` and run on the host. Each new logic file is added to the host `Makefile` and to the device build (fbt picks up `src/**` automatically). A `clock` seam (`uint32_t game_now(void)`) wraps `furi_hal_rtc` on device and is settable on host.

### Task 2.1: Extend the state model & clock seam

**Files:**
- Modify: `src/game_structs.h` (new `LifeStage` values, `DragonAlignment`, extended `PersistentGameState`, extended `GameEvents`, new `ThreadsMessageType`s).
- Create: `src/clock.h`, `src/clock.c` (device: RTC → timestamp; a `clock_set_for_test` compiled only for host).
- Modify: `src/constants.h` (new tunables; keep DEBUG block).

**Interfaces:**
- Produces: `enum LifeStage { EGG, HATCHLING, WYRMLING, DRAKE, ADULT, DEAD, LIFE_STAGES_NUM }`; `enum DragonAlignment { ALIGN_NONE, ALIGN_WHITE, ALIGN_GREY, ALIGN_BLACK }`; extended `struct PersistentGameState` with fields: `uint8_t stage, alignment; uint32_t birth_timestamp, stage_entered_timestamp; uint32_t hunger, last_hunger_update; uint32_t happiness, last_happiness_update; uint32_t health, last_health_update; uint8_t poop, sick; uint32_t last_poop_update, last_sick_update; uint8_t lights_off; int32_t care_score; uint32_t discipline; uint8_t attention_call;`. `uint32_t game_now(void)`.

- [ ] **Step 1:** Write a failing test `test_state_struct_defaults()` asserting a helper `game_state_init(&s, /*now=*/1000)` sets `stage==EGG`, `hunger==MAX_HU`, `happiness==MAX_HAPPINESS`, `health==MAX_HP`, `care_score==CARE_START (50)`, `alignment==ALIGN_NONE`, `birth_timestamp==1000`.
- [ ] **Step 2:** Run → fail (symbols undefined).
- [ ] **Step 3:** Add the enums/struct fields and a small `game_state_init()` (in a new `src/game_model.c`/`.h` or extend `state_management`); add the constants (`MAX_HAPPINESS`, `CARE_START`, etc.). Implement `clock.c` with host override.
- [ ] **Step 4:** Run → pass.
- [ ] **Step 5:** Commit.

### Task 2.2: Happiness feature (decay + Play)

**Files:** Create `src/features/happiness.c/.h` (or extend `feature_management.c`); add to both builds. Test in `test/host/test_features.c`.

**Interfaces:** `void init_happiness(GameState*, uint32_t now)`, `void check_happiness(const GameState*, uint32_t now, GameEvents*)`, `bool apply_happiness(GameState*, GameEvents)`, `void generate_happiness(GameState*, uint32_t now, GameEvents*)` (Play boost). Mirrors the existing HU pattern.

- [ ] **Step 1:** Failing tests: (a) after `HAPPINESS_DECAY_FREQUENCY * n` seconds, `check` proposes the right decay; (b) `apply` clamps at 0; (c) Play (`generate_happiness`) raises toward `MAX_HAPPINESS` and clamps.
- [ ] **Step 2:** Run → fail.
- [ ] **Step 3:** Implement following the `hu` model in `feature_management.c` (deterministic decay with seedable coin where relevant; Play adds a fixed+random amount).
- [ ] **Step 4:** Run → pass.
- [ ] **Step 5:** Commit.

### Task 2.3: Hygiene / poop feature

**Files:** `src/features/hygiene.c/.h`; tests.

**Interfaces:** `check_hygiene` (accumulates poop over time via coin tosses), `apply_hygiene`, `clean_poop(GameState*)` (Clean action → clears poop, returns whether there was any).

- [ ] **Step 1:** Failing tests: poop appears after expected elapsed time; `poop` count caps; `clean_poop` clears and reports.
- [ ] **Step 2–4:** Fail → implement → pass.
- [ ] **Step 5:** Commit.

### Task 2.4: Sickness feature (coupled to hunger & hygiene)

**Files:** `src/features/sickness.c/.h`; tests.

**Interfaces:** `check_sickness` (illness odds rise when `hunger==0` or `poop>0`), `apply_sickness`, `give_medicine(GameState*)` (cure if sick; no-op if healthy).

- [ ] **Step 1:** Failing tests with seeded RNG: sick probability higher when hungry/dirty; `give_medicine` cures only when sick.
- [ ] **Step 2–4:** Fail → implement → pass.
- [ ] **Step 5:** Commit.

### Task 2.5: Health rework (starvation + illness + filth drains)

**Files:** Modify `src/feature_management.c` HP logic; tests.

**Interfaces:** `check_hp`/`apply_hp` updated so Health drains when starving, sick, or filthy; `give_pill`/medicine restores; death at 0.

- [ ] **Step 1:** Failing tests: HP drains when hunger==0; drains when sick; medicine restores; HP hitting 0 sets `stage=DEAD`.
- [ ] **Step 2–4:** Fail → implement → pass.
- [ ] **Step 5:** Commit.

### Task 2.6: Sleep / day-night

**Files:** `src/features/sleep.c/.h`; tests.

**Interfaces:** `bool is_night(uint32_t now)` (RTC hour vs `NIGHT_START`/`NIGHT_END`), `bool is_asleep(const GameState*, uint32_t now)` (night && lights_off), `void set_lights(GameState*, bool off)`. Being kept awake at night (night && !lights_off) is a care mistake and drains happiness/health in the relevant `check_*`.

- [ ] **Step 1:** Failing tests: `is_night` true/false across boundary hours; `is_asleep` requires lights_off at night; keeping lights on at night registers a care penalty.
- [ ] **Step 2–4:** Fail → implement → pass.
- [ ] **Step 5:** Commit.

### Task 2.7: Discipline & attention calls

**Files:** `src/features/discipline.c/.h`; tests.

**Interfaces:** `void check_attention_call(GameState*, uint32_t now)` (with prob rising as discipline falls, sets `attention_call=1` when no real need is pending), `void scold(GameState*)` (if `attention_call`: +discipline, +care, clear call; else: −care — misread), `bool has_real_need(const GameState*)`.

- [ ] **Step 1:** Failing tests (seeded): low discipline → call raised more often; `scold` during a call raises discipline & care; `scold` with a real need lowers care.
- [ ] **Step 2–4:** Fail → implement → pass.
- [ ] **Step 5:** Commit.

### Task 2.8: Care score engine

**Files:** `src/care.c/.h`; tests. Central helpers the features call.

**Interfaces:** `void care_reward(GameState*, int amount)`, `void care_penalty(GameState*, int amount)` (both clamp `care_score` to [0,100]); constants for each good/bad event (feed-when-hungry, play-when-sad, prompt-clean, prompt-heal, lights-out, correct-scold as rewards; any meter → 0, poop ignored past `POOP_TOLERANCE`, illness ignored past `SICK_TOLERANCE`, sleep disturbed, overfeed, needless/over-scold as penalties). Wire the reward/penalty calls into the feature `apply_*`/action functions from 2.2–2.7.

- [ ] **Step 1:** Failing tests: each good action nudges care up (clamped ≤100); each mistake nudges down (clamped ≥0); an ignored-poop timeout applies exactly one penalty, not repeatedly.
- [ ] **Step 2–4:** Fail → implement → pass.
- [ ] **Step 5:** Commit.

### Task 2.9: Evolution engine (age-based stages + care branch + immortality)

**Files:** `src/evolution.c/.h`; tests. Replaces XP-driven staging.

**Interfaces:**
- `void check_evolution(GameState*, uint32_t now, GameEvents*)` — advances `stage` by age thresholds `STAGE_AGE_SECONDS[]` (Egg→Hatchling→Wyrmling→Drake→Adult). On Drake→Adult, set `alignment` from `care_score`: `≥CARE_WHITE(66)→ALIGN_WHITE`, `≥CARE_GREY(33)→ALIGN_GREY`, else `ALIGN_BLACK`.
- `void check_old_age(GameState*, uint32_t now, GameEvents*)` — only when `stage==ADULT`. If `care_score >= CARE_IMMORTAL(90)`: never dies of old age. Else roll death on a care-scaled curve past a per-alignment minimum adult age. (Health=0 death handled in HP task, independent.)

- [ ] **Step 1:** Failing tests: (a) stage advances at exact age thresholds; (b) alignment picked correctly at each care band on the Drake→Adult tick; (c) with `care_score=95`, no old-age death across a long simulated span; (d) with mid care, dies of old age within the expected window; (e) low care dies sooner. Use seeded RNG + `clock_set_for_test`.
- [ ] **Step 2–4:** Fail → implement → pass.
- [ ] **Step 5:** Commit.

### Task 2.10: Offline fast-forward integration

**Files:** Modify `src/state_management.c` (`generate_new_random_events`/`process_events`, `fast_forward_state`) to drive all features + evolution + old-age from `last_*_update` up to `game_now()`; `correct_state` zeroes needs when DEAD. Tests.

**Interfaces:** `bool process_events(GameState*, GameEvents)` returns whether anything changed; `void fast_forward_state(GameState*)` masks vibro/sound and applies all elapsed events.

- [ ] **Step 1:** Failing integration tests (seeded, `clock_set_for_test`): from a fresh egg, advancing the clock by N days produces the expected stage; a well-fed/cleaned/happy timeline yields ALIGN_WHITE; a neglected timeline yields ALIGN_BLACK and earlier death; fast-forwarding 12h reproduces the same end-state as stepping hour-by-hour (determinism/consistency).
- [ ] **Step 2–4:** Fail → implement → pass.
- [ ] **Step 5:** Commit.

---

## Phase 3 — Persistence

### Task 3.1: Extend save format, bump version, fresh start

**Files:** Modify `src/save_restore.c` (uses `saved_struct_*` — automatically handles the new struct size), `src/constants.h` (`GAME_STATE_HEADER_MAGIC` new value e.g. `0xD6`, `GAME_STATE_HEADER_VERSION 0x01`), filenames → `dragotchi.save`/`dragotchi.settings`. Extend `PersistentSettings` only if needed. Host test for round-trip is not possible (needs Storage) — instead add a host test that a `memcpy` round-trip of `PersistentGameState` preserves fields (guards accidental field reordering), and verify real save/load on device in Phase 6.

- [ ] **Step 1:** Failing host test: serialize→zero→deserialize (plain `memcpy` to a buffer of `sizeof`) preserves all new fields.
- [ ] **Step 2–4:** Fail → implement (confirm struct is POD, no pointers) → pass.
- [ ] **Step 5:** Commit.

---

## Phase 4 — GUI

> GUI tasks are verified on-device (build + `loader open` + visual check), not host unit tests. Keep each task to one deliverable and commit after an on-device smoke test.

### Task 4.1: Custom `pet_view` — draw meters + dragon + states

**Files:** Create `src/gui/pet_view.c/.h` (a `View` with a draw callback reading `GameState`, and a model holding a snapshot). Modify `entry_point.c` to allocate it and add it as `scene_main`'s view instead of the button panel; modify `main_scene.c` to use it.

- [ ] **Step 1:** Implement the draw callback: top row three meters (icon + 4 pips each for hunger/happiness/health), day/night glyph; center the current sprite from `decode_image`; draw poop/sick/Zzz glyphs when those states are set. Draw a bottom action bar with the six actions and a highlighted cursor.
- [ ] **Step 2:** Wire the tick (`SceneManagerEventTypeTick`) to request a redraw with the latest `GameState` snapshot.
- [ ] **Step 3:** Build, deploy, launch on device; confirm meters + egg render and animate. Commit.

### Task 4.2: Input handling & action dispatch

**Files:** Modify `src/gui/pet_view.c` input callback + `main_scene.c`. Add `ThreadsMessageType`s: `PROCESS_FEED, PROCESS_PLAY, PROCESS_CLEAN, PROCESS_MEDICINE, PROCESS_SCOLD, TOGGLE_LIGHTS`. Modify `threads.c` secondary-thread switch to call the matching action functions (`give_candy`→feed, `generate_happiness`→play, `clean_poop`, `give_medicine`, `scold`, `set_lights`) then `process_events` and trigger animation/sound/vibro.

- [ ] **Step 1:** Left/Right moves the action cursor; OK sends the selected action's message; Back exits (as today). Up/short-press could open Stats/Settings (map to existing scenes).
- [ ] **Step 2:** Implement the `threads.c` handlers for all six actions (mirror existing candy/pill handlers, with the right sound/vibro cue).
- [ ] **Step 3:** Build, deploy; verify each action changes the right meter/state on device. Commit.

### Task 4.3: Dragon `game_decoder` for stages + states + adult alignment

**Files:** Modify `src/gui/game_decoder.c` to select sprite by `(stage, alignment, transient state)` — idle/eat/play/sick/sleep frames per stage, and for ADULT pick white/grey/black sprite sets. Extend `GameState.next_animation_index` usage for multi-frame animation and add a transient `display_state` (idle/eating/playing/sick/sleeping) set by actions/events.

- [ ] **Step 1:** Implement the selection table keyed on stage+alignment+display_state; fall back to idle frames.
- [ ] **Step 2:** Build with placeholder-free art from Phase 5 (this task lands after 5.1 provides sprites, or use temporary reuse then swap). Deploy; verify correct sprite per state. Commit.

### Task 4.4: Attention calls + "while you were away"

**Files:** Modify `main_scene.c`/`pet_view.c` (alert bubble + beep/vibro when a need goes critical or `attention_call` set) and `state_management.c`/`threads.c` (on init after `fast_forward_state`, if the pet is currently in a bad state, raise an immediate alarm + a summary popup listing what happened while away).

- [ ] **Step 1:** Live call: when hunger/happiness/health hit critical or `attention_call` becomes set on a tick, beep + vibrate + draw a "!" bubble until addressed.
- [ ] **Step 2:** On-open summary: build a short string ("While away: got hungry, pooped x2, fell ill") from a diff captured during `fast_forward_state`; show via the existing popup/status scene on first entry, with an urgent cue if currently critical.
- [ ] **Step 3:** Build, deploy; close app, wait, reopen → verify summary + alarm. Commit.

### Task 4.5: Stats, Settings, About refresh

**Files:** Modify `status_scene.c` (show stage, age in days, alignment/character name, Discipline, care summary words), `settings_scene.c` (keep sound/vibration/reset), `about_scene.c`/`constants.h` ABOUT_TEXT (Dragotchi + MrModd credit + GPLv3).

- [ ] **Step 1:** Update `get_state_str`/status text to the new model; map care_score→words ("Thriving/Well cared/Neglected"); alignment→dragon name.
- [ ] **Step 2:** Update About text and reset confirmation copy.
- [ ] **Step 3:** Build, deploy; verify screens. Commit.

---

## Phase 5 — Art

### Task 5.1: Generate 1-bit dragon sprites & icons

**Files:** Create `tools/gen_art.py` (Pillow) that emits 1-bit PNGs into `assets/`; Create/replace `assets/*.png`. Sprites (60×60, ≥2 frames): egg, hatchling, wyrmling, drake, adult_white, adult_grey, adult_black, dead; state frames: eating, playing, sick, sleeping (Zzz), evolve puff; `poop` (~20×20). Action icons (20×20 + hover): feed, play, clean, medicine, scold, lights (reuse candy/pill style).

- [ ] **Step 1:** Write `tools/gen_art.py` producing all PNGs at the right sizes, 1-bit, transparent/black-on-white per Flipper convention (match existing asset pixel style).
- [ ] **Step 2:** Run it; eyeball the PNGs (send key sprites to the owner for a quick look).
- [ ] **Step 3:** Build so fbt compiles them into `dragotchi_icons.h`; fix any naming the decoder expects. Deploy; verify sprites render. Commit art + generator.

---

## Phase 6 — Integration, on-device verification, polish

### Task 6.1: Full playthrough with DEBUG timescale on device

- [ ] **Step 1:** Build a `DEBUG` fap (compressed timescale) with `ufbt --extra-define=DEBUG`. Deploy.
- [ ] **Step 2:** On device, drive a full life in minutes: hatch → feed/play/clean/heal → confirm evolution and that good care → white dragon; then a neglect run → black dragon + early death. Confirm save persists across app close/reopen and offline fast-forward matches. Capture the Flipper screen (RPC screenstream or photos) for evidence.
- [ ] **Step 3:** Fix any issues found; re-run. Commit fixes.

### Task 6.2: Ship the release build

- [ ] **Step 1:** Build the normal (non-DEBUG) `dragotchi.fap` against 1.4.3; verify manifest API 87.1.
- [ ] **Step 2:** Deploy to `/ext/apps/Games/dragotchi.fap`; `loader open Dragotchi`; confirm it runs from a clean install (fresh egg) with real timescale.
- [ ] **Step 3:** Final README + CHANGELOG (`0.1` initial Dragotchi release). Tag/commit. Report done with on-device evidence.

---

## Self-review notes

- Spec §1–§4 needs → Tasks 2.2–2.8; §5 evolution/immortality → 2.9; §6 timescale → constants in 2.1/2.9; §7 screens → 4.1/4.4/4.5; §8 art → 5.1; §9 save → 3.1; §10 build/test → 0.1/1.1/6.x; §11 license → 0.2/4.5. All covered.
- Determinism: every probabilistic test seeds the RNG stub and sets the clock; no wall-clock or hardware RNG in host tests.
- The one non-host-testable area (Storage save/load, GUI) is explicitly verified on-device in Phases 4 and 6.
