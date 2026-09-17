# Dragotchi v0.2 "Hunt" Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add hunting — a one-tap Forage that reads the real sub-GHz airwaves and yields a variable catch (prey feeds the dragon, treasure builds a hoard/rank, rare eggs fill a hatchery), plus legacy-hatch-on-death — building through to a working `.fap` on the Flipper.

**Architecture:** Keep the existing two-thread + pure-logic + host-test structure. All catch/economy logic is pure and host-tested; the ONLY hardware-dependent piece is a thin `hunt_sense()` seam over `furi_hal_subghz` RSSI, stubbed on host. Reuse the clock seam and seedable RNG. One active dragon.

**Tech Stack:** C11, ufbt (fw 1.4.3 / API 87.1), host GCC test harness, `furi_hal_subghz` for RSSI sensing.

**Spec:** `docs/superpowers/specs/2026-09-12-dragotchi-hunt-expeditions-design.md` (v0.2 slice).

## Global Constraints

- Target fw **1.4.3 / API 87.1**, hardware f7. Verify FAP manifest API before deploy.
- GPLv3; keep MrModd attribution.
- Deploy to `/ext/apps/Games/dragotchi.fap` via scratchpad `push_big.py`; launch with `loader open`.
- Pure logic files stay host-testable: no device headers except through the `hunt_sense` seam; RSSI sensing is **receive-only** (no TX — sidesteps region limits).
- New save fields → bump `GAME_STATE_HEADER_MAGIC` (0xD6 → 0xD7); fresh start, no migration.
- Frequent commits; host tests (`make -C test/host run`) green before each commit; TDD for logic.

---

## Task 1: Data model + save bump

**Files:**
- Modify: `src/game_structs.h` (append fields + Catch types), `src/game_model.c` (init), `src/constants.h` (magic 0xD7).
- Test: `test/host/test_hunt.c` (new), wired into `test/host/Makefile`, `tests.h`, `test_runner.c`.

**Interfaces:**
- Produces: added `PersistentGameState` fields `uint32_t hoard; uint16_t eggs_common; uint16_t eggs_rare; uint32_t last_forage_time;`. New `enum CatchCategory { CATCH_PREY, CATCH_TREASURE, CATCH_EGG };` and `struct Catch { uint8_t category; uint8_t tier; uint16_t value; uint8_t band; };` in `game_structs.h`.

- [ ] **Step 1: failing test** — in `test/host/test_hunt.c`:
```c
#include "tests.h"
#include "test_util.h"
#include "game_model.h"
#include "tuning.h"
void run_hunt_tests(void) {
    struct GameState s = {0};
    game_state_init(&s, 1000);
    CHECK(s.persistent.hoard == 0);
    CHECK(s.persistent.eggs_common == 0 && s.persistent.eggs_rare == 0);
    CHECK(s.persistent.last_forage_time == 0);
}
```
Register: add `void run_hunt_tests(void);` to `tests.h`, call it in `test_runner.c`, add `test_hunt.c` to `TESTS` and (later) `src/hunt.c` etc. to `LOGIC` in the Makefile.
- [ ] **Step 2: run → fail** (`make -C test/host run`): unknown fields / undefined.
- [ ] **Step 3: implement** — append the four fields to `PersistentGameState` (after `last_oldage_update`); add the `CatchCategory` enum and `Catch` struct; in `game_model.c` `game_state_init`, set `p->hoard=0; p->eggs_common=0; p->eggs_rare=0; p->last_forage_time=0;`. Bump `GAME_STATE_HEADER_MAGIC` to `0xD7` in `constants.h`.
- [ ] **Step 4: run → pass.**
- [ ] **Step 5: commit** `feat: hunt data model + save bump (v0.2 Task 1)`.

---

## Task 2: Hunt catch logic (`hunt.c/.h` + tuning)

**Files:**
- Create: `src/hunt.h`, `src/hunt.c`. Modify: `src/tuning.h` (hunt constants), `test/host/test_hunt.c`, Makefile `LOGIC += src/hunt.c`.

**Interfaces:**
- Consumes: `random_generator.h` (`random_uniform`, `toss_a_coin`), `care.h` is not needed here.
- Produces:
  - `bool forage_ready(const struct GameState *, uint32_t now);`
  - `struct Catch catch_roll(uint8_t activity /*0-100*/, uint8_t band);` (uses the global seedable RNG)
  - `void apply_catch(struct GameState *, struct Catch, uint32_t now);` (applies effects, sets `last_forage_time=now`)
  - `const char *band_name(uint8_t band);` (0..3)
  - `int catch_describe(struct Catch, char *buf, size_t n);` (e.g. "433 wyrm +18 food")

Tuning (`src/tuning.h`, both profiles unless noted):
```c
#define HUNT_BANDS 4               /* 315, 433, 868, 915 MHz (indices 0..3) */
#ifdef DEBUG
#define FORAGE_COOLDOWN 5          /* seconds */
#else
#define FORAGE_COOLDOWN 180        /* 3 min */
#endif
/* category odds (percent) at activity 0 and 100; interpolate by activity */
#define PREY_PCT_LO 70
#define PREY_PCT_HI 45
#define TREASURE_PCT_LO 25
#define TREASURE_PCT_HI 40
/* egg = remainder (5 at lo, 15 at hi) */
#define PREY_FOOD_SMALL 15
#define PREY_FOOD_MED   30
#define PREY_FOOD_LARGE 45
#define TREASURE_SMALL 5
#define TREASURE_MED   12
#define TREASURE_LARGE 25
```

- [ ] **Step 1: failing tests** (append to `run_hunt_tests`):
```c
#include "hunt.h"
#include "rng_control.h"
// cooldown
struct GameState g = {0}; game_state_init(&g, 0);
g.persistent.last_forage_time = 1000;
CHECK(forage_ready(&g, 1000 + FORAGE_COOLDOWN) == true);
CHECK(forage_ready(&g, 1000 + FORAGE_COOLDOWN - 1) == false);
// catch category is valid and band is preserved
rng_seed(5);
struct Catch c = catch_roll(50, 1);
CHECK(c.category <= CATCH_EGG);
CHECK(c.band == 1);
// higher activity yields more treasure+egg over many rolls than low activity
rng_seed(9); int hi_te = 0; for(int i=0;i<400;i++){ struct Catch x=catch_roll(100,1); if(x.category!=CATCH_PREY) hi_te++; }
rng_seed(9); int lo_te = 0; for(int i=0;i<400;i++){ struct Catch x=catch_roll(0,1); if(x.category!=CATCH_PREY) lo_te++; }
CHECK(hi_te > lo_te);
// apply_catch: prey feeds; treasure hoards; egg to hatchery; sets cooldown cursor
game_state_init(&g, 0); g.persistent.hunger = 10;
apply_catch(&g, (struct Catch){CATCH_PREY, TIER_SMALL, PREY_FOOD_SMALL, 1}, 500);
CHECK(g.persistent.hunger == 10 + PREY_FOOD_SMALL);
CHECK(g.persistent.last_forage_time == 500);
apply_catch(&g, (struct Catch){CATCH_TREASURE, TIER_MED, TREASURE_MED, 1}, 600);
CHECK(g.persistent.hoard == TREASURE_MED);
apply_catch(&g, (struct Catch){CATCH_EGG, 1 /*rare*/, 0, 1}, 700);
CHECK(g.persistent.eggs_rare == 1);
```
(Define `enum CatchTier { TIER_SMALL, TIER_MED, TIER_LARGE };` in `hunt.h`.)
- [ ] **Step 2: run → fail.**
- [ ] **Step 3: implement** `hunt.c`:
  - `forage_ready`: `now - last_forage_time >= FORAGE_COOLDOWN` (guard `now >= last`).
  - `catch_roll`: interpolate prey/treasure percentages by `activity`; roll `random_uniform(0,100)`; assign category (egg = remainder); pick tier by a second activity-weighted roll; set `value` from tier tables; keep `band`.
  - `apply_catch`: PREY → `hunger = min(MAX_HU, hunger+value)` (+ small happiness, clamp); TREASURE → `hoard += value`; EGG → `tier? eggs_rare++ : eggs_common++`; always `last_forage_time = now`; set `display_state = DISP_EATING` for prey (reuse reveal), else DISP_PLAYING.
  - `band_name`: static array {"315","433","868","915"}. `catch_describe`: snprintf.
- [ ] **Step 4: run → pass.**
- [ ] **Step 5: commit** `feat: hunt catch logic + tuning (v0.2 Task 2)`.

---

## Task 3: Economy — hoard rank, hatchery, legacy hatch (`economy.c/.h`)

**Files:** Create `src/economy.h`, `src/economy.c`. Modify `src/tuning.h` (rank thresholds), `test/host/test_hunt.c`, Makefile `LOGIC += src/economy.c`.

**Interfaces:**
- Consumes: `game_model.h` (`game_state_init`), `game_structs.h`.
- Produces:
  - `const char *hoard_rank(uint32_t hoard);`
  - `bool has_heir_egg(const struct GameState *);`
  - `void hatch_heir(struct GameState *, uint32_t now);` (consume one egg — prefer rare — and re-init a fresh dragon with a care head-start; caller ensures an egg exists)

Tuning:
```c
#define RANK1_MIN 0     /* "Nest Scrounger" */
#define RANK2_MIN 50    /* "Trinket Keeper" */
#define RANK3_MIN 200   /* "Hoarder" */
#define RANK4_MIN 600   /* "Wyrm of Wealth" */
#define RANK5_MIN 1500  /* "Dragon Sovereign" */
#define HEIR_RARE_CARE 65   /* head-start care for a rare-egg heir */
#define HEIR_COMMON_CARE 55 /* head-start care for a common-egg heir */
```

- [ ] **Step 1: failing tests** (append):
```c
#include "economy.h"
CHECK(strcmp(hoard_rank(0), "Nest Scrounger") == 0);
CHECK(strcmp(hoard_rank(RANK4_MIN), "Wyrm of Wealth") == 0);
CHECK(strcmp(hoard_rank(999999), "Dragon Sovereign") == 0);
struct GameState h = {0}; game_state_init(&h, 0);
CHECK(has_heir_egg(&h) == false);
h.persistent.eggs_rare = 1; CHECK(has_heir_egg(&h) == true);
h.persistent.stage = DEAD; h.persistent.hoard = 123;
hatch_heir(&h, 5000);
CHECK(h.persistent.stage == EGG);                 // fresh dragon
CHECK(h.persistent.eggs_rare == 0);               // egg consumed
CHECK(h.persistent.care_score == HEIR_RARE_CARE); // head-start
CHECK(h.persistent.hoard == 123);                 // hoard/rank persists across heirs
CHECK(h.persistent.birth_timestamp == 5000);
```
(`hatch_heir` must preserve `hoard`/`eggs` across the re-init — re-init the dragon but keep the collection.)
- [ ] **Step 2: run → fail.**
- [ ] **Step 3: implement** `economy.c`:
  - `hoard_rank`: threshold ladder → title string.
  - `has_heir_egg`: `eggs_rare>0 || eggs_common>0`.
  - `hatch_heir`: capture `hoard/eggs`; pick rare if available else common and decrement; `game_state_init(gs, now)`; restore `hoard` and the (decremented) egg counts; set `care_score` to `HEIR_RARE_CARE`/`HEIR_COMMON_CARE`.
- [ ] **Step 4: run → pass.**
- [ ] **Step 5: commit** `feat: economy - hoard rank, hatchery, legacy heir (v0.2 Task 3)`.

---

## Task 4: RF sensing seam (`hunt_hw`)

**Files:** Create `src/hunt_hw.h`, `src/hunt_hw.c` (device), `test/host/hunt_hw_stub.c` (host). Modify Makefile (host uses the stub, NOT the device file).

**Interfaces:**
- Produces: `void hunt_sense(uint8_t *activity_out, uint8_t *band_out);`
  - Device: for each of the 4 bands, `furi_hal_subghz_set_frequency(freq)`, `furi_hal_subghz_rx()`, read `furi_hal_subghz_get_rssi()` ~5× (small `furi_delay_us`), track the peak; map the strongest band → `band_out`; normalise aggregate/peak RSSI (dBm, roughly −110..−30) → `activity_out` 0..100; then `furi_hal_subghz_idle()` / release. Acquire/release the subghz HAL around the sweep (`furi_hal_subghz_init`/`sleep` or the `subghz_devices` API) so it doesn't clash with the app.
  - Host stub: returns values set by `hunt_sense_set_for_test(uint8_t activity, uint8_t band)` (declared in `hunt_hw.h` under `#ifdef HOST_TEST` or always; the stub defines it).

- [ ] **Step 1: failing test** (host): set stub to (80, 2), call `hunt_sense`, CHECK outputs match.
- [ ] **Step 2: run → fail** (stub not yet built in).
- [ ] **Step 3: implement** the host stub + device impl. Add `test/host/hunt_hw_stub.c` to the host Makefile `STUBS`; ensure `src/hunt_hw.c` is compiled by ufbt (device) but NOT by the host Makefile.
- [ ] **Step 4: run → pass.**
- [ ] **Step 5: commit** `feat: sub-GHz RSSI sensing seam + host stub (v0.2 Task 4)`.

---

## Task 5: Device wiring — Forage action

**Files:** Modify `src/game_structs.h` (`ThreadsMessageType` add `PROCESS_FORAGE`, `PROCESS_HATCH_HEIR`), `src/state_management.h/.c` (`do_forage`, `do_hatch_heir`, expose `last catch` for reveal), `src/game_logic.c/.h` (if the action wrappers live there), `src/threads.c` (handle the new messages + sounds). On-device verification (no host test for hardware path).

**Interfaces:**
- Produces: `GameEventFlags do_forage(struct GameState *, uint32_t now, struct Catch *out);` — if `!forage_ready` returns EVT_NONE and sets `out->value=0` sentinel (or a `bool` "on cooldown"); else `hunt_sense()` → `catch_roll` → `apply_catch`, return a flag (`EVT_FED`/`EVT_PLAYED`/a new `EVT_CAUGHT`) and fill `*out`.
- Add `EVT_CAUGHT (1u<<10)` in `game_structs.h`.

- [ ] **Step 1:** add message types + `EVT_CAUGHT`; implement `do_forage` in `state_management.c` (uses `clock`+`hunt_hw`+`hunt`) and `do_hatch_heir` (calls `economy.hatch_heir`).
- [ ] **Step 2:** in `threads.c`, handle `PROCESS_FORAGE` (call `do_forage`, stash the resulting `Catch` in the `GameState`/context for the reveal, map flags→sound: prey=play_action, treasure=play_level_up, egg=play_level_up+long vibro; cooldown=short buzz) and `PROCESS_HATCH_HEIR`.
- [ ] **Step 3:** build with ufbt; fix compile errors.
- [ ] **Step 4:** commit `feat: wire Forage + hatch-heir into logic thread (v0.2 Task 5)`.

*(No host test: this task is glue over already-tested logic + the hardware seam. Behaviour is verified on-device in Task 7.)*

---

## Task 6: GUI — Adventure menu, catch reveal, Stats, legacy prompt

**Files:** Modify `src/gui/pet_view.c` (Hunt entry + catch-reveal overlay), `src/gui/scenes/main_scene.c` (route new custom events), `src/gui/scenes/status_scene.c` (+ hoard rank + egg counts via `state_management`/`economy`), and add a small reveal/among the existing scenes. Modify `src/state_management.c` `get_state_str` to include rank + eggs.

- [ ] **Step 1:** Add a **Hunt** action to the pet view selector (7th action) OR an "Adventure" entry that sends `PET_EVT_HUNT`; `main_scene` maps it to a `PROCESS_FORAGE` message.
- [ ] **Step 2:** Catch reveal — on `EVT_CAUGHT`, `pet_view` shows a 1–2 s banner from the stashed `Catch` (`catch_describe`) with the reveal animation; on cooldown, show "still sniffing…".
- [ ] **Step 3:** Stats scene shows **Rank: <hoard_rank>**, **Hoard: N**, **Eggs: C common / R rare** (extend `get_state_str`).
- [ ] **Step 4:** Legacy — when `stage==DEAD` and `has_heir_egg`, the pet view offers "OK: Hatch heir" → sends `PROCESS_HATCH_HEIR`.
- [ ] **Step 5:** build; commit `feat: hunt GUI - menu, catch reveal, hoard/eggs, heir (v0.2 Task 6)`.

---

## Task 7: Integration, on-device verification, release v0.2

- [ ] **Step 1:** `make -C test/host run` → ALL OK; `ufbt` release build → API 87.1.
- [ ] **Step 2:** Deploy; on device: **Forage** a few times → catches vary; verify RSSI sensing responds to environment (forage near a noisy source vs. quiet); prey raises hunger, treasure raises hoard (check Stats rank), egg increments hatchery; cooldown blocks rapid re-forage. Verify save persists across close/reopen. Use a DEBUG build (short cooldown) for fast iteration; confirm on the release build too.
- [ ] **Step 3:** Legacy: force a death (DEBUG neglect or crafted save), confirm "Hatch heir" starts a fresh dragon and keeps hoard.
- [ ] **Step 4:** Bump `fap_version` 0.1.2 → 0.2.0; update CHANGELOG/README; commit; merge to `main` via finishing-a-development-branch.

---

## Self-review notes

- Spec coverage: §3 Hunting → Tasks 2,4,5,6; §5 economy/rank/hatchery/legacy → Task 3,6; §6 data model → Task 1 (+expedition fields deferred to v0.3, correctly out of this plan); §7 UI → Task 6; §8 test → all logic tasks + Task 7; §11 constraints → Global + Task 4 (RX-only). Expeditions (§4) intentionally deferred to v0.3.
- Determinism: every probabilistic test seeds the RNG; the hardware seam is stubbed on host.
- Type consistency: `struct Catch{category,tier,value,band}`, `enum CatchCategory{CATCH_PREY,CATCH_TREASURE,CATCH_EGG}`, `enum CatchTier{TIER_SMALL,TIER_MED,TIER_LARGE}`, `hunt_sense(activity_out,band_out)`, `catch_roll(activity,band)`, `apply_catch(gs,catch,now)`, `hoard_rank(hoard)`, `hatch_heir(gs,now)` — used consistently across tasks.
