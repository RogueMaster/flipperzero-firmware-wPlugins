# Dragotchi v0.3 "Expeditions" Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Send the dragon on a timed offline expedition (Short 30m / Long 2h / Epic 8h) that pauses its needs while away, then returns loot + a journey log on the next open — building through to a working, tested `.fap` on the Flipper.

**Architecture:** Reuse the existing structure. Expedition timing/loot is pure logic (host-tested) using the clock seam + seedable RNG; resolution reuses the tested `catch_roll`. No early recall. Away-state pauses `advance_state`; on return it resolves and shows a journey-log screen (reusing the text_box module).

**Tech Stack:** C11, ufbt (fw 1.4.3 / API 87.1), host GCC tests.

**Spec:** `docs/superpowers/specs/2026-09-12-dragotchi-hunt-expeditions-design.md` (§4 Expeditions, v0.3 slice).

## Global Constraints

- Target fw 1.4.3 / API 87.1; verify manifest API before deploy.
- New save fields → bump `GAME_STATE_HEADER_MAGIC` 0xD8 → 0xD9; fresh start (re-craft the owner's pet after).
- Durations use real RTC time (an 8h trip takes 8h); a DEBUG build compresses via `EXPED_SEC_PER_MIN`.
- No early recall: once sent, away until done; only Stats/Inventory/Settings reachable.
- An expedition never outright kills — worst case it returns hurt (health floored at 1).
- Host tests green before each commit; TDD for logic.

---

## Task 1: Data model, message arg, tuning

**Files:** Modify `src/game_structs.h` (fields + `ThreadsMessage.arg` + `PROCESS_EXPEDITION` + `EVT_EXPED_RETURN`), `src/game_model.c` (init), `src/constants.h` (magic 0xD9), `src/tuning.h` (expedition constants). Test: `test/host/test_expedition.c` (new) wired into Makefile/tests.h/test_runner.

**Interfaces:**
- Produces: `PersistentGameState` fields `uint8_t on_expedition; uint32_t expedition_start; uint16_t expedition_minutes;`. Transient `GameState` fields `char journey_log[96]; uint8_t journey_ready;`. `ThreadsMessage` gains `uint32_t arg;`. New `PROCESS_EXPEDITION` message type and `EVT_EXPED_RETURN (1u<<11)`.
- Tuning:
```c
#ifdef DEBUG
#define EXPED_SEC_PER_MIN 1
#else
#define EXPED_SEC_PER_MIN 60
#endif
#define EXPED_SHORT_MIN 30
#define EXPED_LONG_MIN 120
#define EXPED_EPIC_MIN 480
#define EXPED_FINDS_SHORT 2
#define EXPED_FINDS_LONG 5
#define EXPED_FINDS_EPIC 12
#define EXPED_ACT_SHORT 40
#define EXPED_ACT_LONG 60
#define EXPED_ACT_EPIC 85
#define EXPED_RISK_SHORT 5
#define EXPED_RISK_LONG 15
#define EXPED_RISK_EPIC 30
#define EXPED_HURT_MIN 5
#define EXPED_HURT_MAX 20
```

- [ ] **Step 1:** Add fields/enum/constants; init `on_expedition=0; expedition_start=0; expedition_minutes=0;` and transient `journey_ready=0`. Bump magic. Add `test_expedition.c` with a defaults test (`on_expedition==0`). Wire into host build.
- [ ] **Step 2:** `make -C test/host run` → fail then (after adding) pass.
- [ ] **Step 3:** Commit `feat: expedition data model + message arg + tuning (v0.3 Task 1)`.

---

## Task 2: Expedition logic (`expedition.c/.h`)

**Files:** Create `src/expedition.h`, `src/expedition.c`; add to host Makefile `LOGIC`. Tests in `test/host/test_expedition.c`.

**Interfaces:**
- `void expedition_start(struct GameState *, uint16_t minutes, uint32_t now);` — set `on_expedition=1, expedition_start=now, expedition_minutes=minutes`.
- `bool expedition_done(const struct GameState *, uint32_t now);` — `now >= start + minutes*EXPED_SEC_PER_MIN`.
- `uint32_t expedition_remaining_sec(const struct GameState *, uint32_t now);`
- `GameEventFlags expedition_resolve(struct GameState *, char *log, size_t logn);` — roll loot (reusing `catch_roll`), apply to gs, roll risk, build log, clear `on_expedition`.

Loot: `finds` and `activity` from `expedition_minutes` (>=EPIC → EPIC values, >=LONG → LONG, else SHORT). For each find, `struct Catch c = catch_roll(activity, random_uniform(0, HUNT_BANDS))`; treasure → `hoard += c.value` + tier count; egg → `eggs_common/rare++` + `eggs_caught++`; prey → accumulate food + `prey_caught++`. Apply food to `hunger` (clamp). Risk: `toss_a_coin(risk)` → `hp_loss = rnd(EXPED_HURT_MIN,MAX)`, `health = max(1, health-hp_loss)`. Build `log` (e.g. "Home!\n+18 hoard  1 egg\nFed +30  Hurt!").

- [ ] **Step 1: failing tests** (seeded): `expedition_start` sets fields; `expedition_done` true only at/after end (using `EXPED_SEC_PER_MIN`); `expedition_remaining_sec` counts down; `expedition_resolve` clears `on_expedition`, and an EPIC trip yields more hoard than a SHORT over many seeds; health never drops below 1.
```c
// e.g.
rng_seed(3); struct GameState s={0}; game_state_init(&s,0);
expedition_start(&s, EXPED_SHORT_MIN, 1000);
CHECK(s.persistent.on_expedition==1);
CHECK(expedition_done(&s, 1000+EXPED_SHORT_MIN*EXPED_SEC_PER_MIN)==true);
CHECK(expedition_done(&s, 1000+EXPED_SHORT_MIN*EXPED_SEC_PER_MIN-1)==false);
char log[96];
// epic vs short hoard
uint32_t hs=0,he=0;
for(int i=0;i<40;i++){ rng_seed(100+i); struct GameState a={0}; game_state_init(&a,0); a.persistent.expedition_minutes=EXPED_SHORT_MIN; a.persistent.on_expedition=1; expedition_resolve(&a,log,sizeof(log)); hs+=a.persistent.hoard; }
for(int i=0;i<40;i++){ rng_seed(100+i); struct GameState a={0}; game_state_init(&a,0); a.persistent.expedition_minutes=EXPED_EPIC_MIN; a.persistent.on_expedition=1; expedition_resolve(&a,log,sizeof(log)); he+=a.persistent.hoard; }
CHECK(he>hs);
CHECK(s.persistent.on_expedition==1); // start didn't resolve
struct GameState r={0}; game_state_init(&r,0); r.persistent.expedition_minutes=EXPED_EPIC_MIN; r.persistent.on_expedition=1; r.persistent.health=3; rng_seed(1);
expedition_resolve(&r,log,sizeof(log));
CHECK(r.persistent.on_expedition==0);
CHECK(r.persistent.health>=1);
```
- [ ] **Step 2–4:** fail → implement → pass.
- [ ] **Step 5:** Commit `feat: expedition logic - start/done/remaining/resolve (v0.3 Task 2)`.

---

## Task 3: Integrate into advance_state + actions

**Files:** Modify `src/game_logic.c` (`advance_state`: pause while away, resolve on done), `src/state_management.h/.c` (`do_expedition(gs, minutes)`), tests in `test_expedition.c`.

**Interfaces:** `void do_expedition(struct GameState *, uint16_t minutes);` (calls `expedition_start` with `game_now()`).

`advance_state` change (top, before needs): if `on_expedition`: if `expedition_done(gs, now)` → `expedition_resolve(gs, gs->journey_log, sizeof)`, set `gs->journey_ready=1`, return `EVT_EXPED_RETURN`; else set `display_state=DISP_SLEEPING` (calm) and `return EVT_NONE` (needs paused). Stage evolution still runs? No — while away, skip everything (return early). (Age still advances by wall-clock via birth; evolution catches up on return via the next normal tick.)

- [ ] **Step 1: failing tests:** with `on_expedition=1` and now < end, `advance_state` leaves needs unchanged (hunger stays) and `on_expedition` stays 1; with now >= end, `advance_state` resolves (on_expedition→0, journey_ready→1, returns EVT_EXPED_RETURN, hoard/loot applied).
- [ ] **Step 2–4:** fail → implement → pass.
- [ ] **Step 5:** Commit `feat: expedition pause+resolve in advance_state; do_expedition (v0.3 Task 3)`.

---

## Task 4: GUI — away-state, duration picker, journey log, menu

**Files:** Modify `src/gui/pet_view.c/.h` (away-state draw + pass on_expedition/remaining), `src/gui/scenes/menu_scene.c` (conditional items + "Expedition"), create `src/gui/scenes/expedition_scene.c/.h` (duration submenu), create `src/gui/scenes/journey_scene.c/.h` (reuses `text_box_module`), `src/threads.c` (handle `PROCESS_EXPEDITION` + `EVT_EXPED_RETURN` sound), `src/gui/scenes/main_scene.c` (on tick, if `journey_ready` open scene_journey), `src/gui/scenes/scenes.h`, `src/entry_point.c` (register text_box under scene_journey too; register expedition submenu — reuse `care_module`? use a dedicated small one or reuse menu flow), `src/flipper_structs.h` if a new module is needed.

- [ ] **Step 1:** `pet_view`: when `on_expedition`, draw away-state (empty nest glyph + "On Expedition" + "Returns in Xh Ym" from remaining secs) instead of pet/meters; `pet_view_update` passes `on_expedition` + `remaining_sec` (compute in `main_scene` via `expedition_remaining_sec(game_now())`, or pass now).
- [ ] **Step 2:** `menu_scene`: if `on_expedition` → only Stats/Inventory/Settings; else if alive → Care/Hunt/**Expedition**/Inventory/Stats/Settings; dead → Hatch Heir/Stats/Settings. Add `MENU_EXPEDITION` → `scene_manager_next_scene(scene_expedition)`.
- [ ] **Step 3:** `expedition_scene`: a Submenu (reuse `care_module`) "Short (30m)", "Long (2h)", "Epic (8h)" → send `ThreadsMessage{PROCESS_EXPEDITION, .arg=minutes}` → back to main. (Add `arg` plumbing in threads: `case PROCESS_EXPEDITION: do_expedition(gs, message.arg);`.)
- [ ] **Step 4:** `journey_scene`: `text_box_set_text(context->text_box_module, gs->journey_log)`; Back → main. `main_scene` tick: if `gs->journey_ready` → set `journey_ready=0`, `scene_manager_next_scene(scene_journey)`. Register text_box view under scene_journey in entry_point.
- [ ] **Step 5:** `threads.c`: `PROCESS_EXPEDITION` → `do_expedition`; on tick flags `EVT_EXPED_RETURN` → play a return jingle (`play_level_up` + `vibrate_long`).
- [ ] **Step 6:** Build; fix; commit `feat: expedition GUI - away-state, duration picker, journey log, menu (v0.3 Task 4)`.

---

## Task 5: Integration, on-device verification, release v0.3.0

- [ ] **Step 1:** `make -C test/host run` → ALL OK; `ufbt` release → API 87.1.
- [ ] **Step 2:** DEBUG build (`EXPED_SEC_PER_MIN=1`): via a temporary safe auto-send hook OR by crafting a save with `on_expedition=1` and a past `expedition_start`, launch → confirm it resolves on-device (read save: `on_expedition==0`, hoard increased, `journey_log` set), no crash. Remove any hook.
- [ ] **Step 3:** Bump `fap_version` 0.2.2 → 0.3.0; CHANGELOG; deploy release; re-craft the owner's pet (new 0xD9 format, day-old healthy + existing inventory). Commit; merge to `main`.

---

## Self-review notes

- Spec §4 → Tasks 2,3,4; data model (§6 v0.3) → Task 1; UI (§7) → Task 4; test (§8) → Tasks 2,3,5. No early recall (owner choice) honored (no recall path). Reuses `catch_roll` (tested) for loot; RNG seeded and clock via `game_now()`/passed `now` in all tests.
- Type consistency: `expedition_start(gs,minutes,now)`, `expedition_done(gs,now)`, `expedition_remaining_sec(gs,now)`, `expedition_resolve(gs,log,logn)`, `do_expedition(gs,minutes)`, `ThreadsMessage.arg`, `EVT_EXPED_RETURN`, `journey_log`/`journey_ready` — consistent across tasks.
- Save wipe: bump 0xD9 wipes the current pet; re-craft after (Task 5), per prior slices.
