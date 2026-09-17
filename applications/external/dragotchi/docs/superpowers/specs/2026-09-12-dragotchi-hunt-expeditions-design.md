# Dragotchi: Hunt & Expeditions — Design Spec

*The "epic leap": the dragon lives in the invisible world of radio signals
around you. You provision for it by hunting the real airwaves and sending it on
expeditions — not just babysitting meters.*

Date: 2026-09-12
Builds on: Dragotchi v0.1.2 (`~/Projects/FlipperZero/Dragotchi`, branch `main`).
Target: Flipper Zero, firmware 1.4.3 / API 87.1. Onboard hardware only (no devboard).

---

## 1. North star & goal

Make Dragotchi unmistakably a *Flipper* game: the one virtual pet that senses
the real electromagnetic environment. The dragon **hunts the sub-GHz airwaves**
for food, treasure and eggs, and you **send it on expeditions** that make "away"
time exciting instead of anxious. Keep **one active dragon** (current
architecture); add a reward economy (hoard rank + hatchery + legacy).

Success: hunting is a quick, surprising thrill that varies by where you are;
expeditions give you something to look forward to; the hoard/hatchery give
long-term goals; and none of it breaks the faithful care loop already shipped.

---

## 2. Core loop

1. **Care** (existing): feed / play / clean / medicine / scold / lights; needs,
   sickness, sleep, care-driven evolution, immortality.
2. **Hunt (active, while playing):** one-tap **Forage** → the dragon reads the
   real airwaves → a **variable catch** (prey / treasure / egg).
3. **Expedition (offline):** **send** the dragon on a timed trip → it returns
   with loot + a journey log.
4. **Spend/collect:** prey feeds it; treasure grows a **hoard** (→ rank);
   eggs fill a **hatchery** (→ your next dragon via legacy).

---

## 3. Hunting (v0.2)

### Feel
A single **Forage** action. The dragon "sniffs" the airwaves for ~1–2 s, then a
**catch-reveal** animation shows what it caught. Low-friction, works anywhere.

### Real-RF sensing (device layer, thin & isolated)
- Sample RSSI across a small set of ISM bands (e.g. 315.0, 433.92, 868.35,
  915.0 MHz). For each band: `furi_hal_subghz_set_frequency` → `..._rx` →
  read `furi_hal_subghz_get_rssi` a few times; take the peak. Release the radio
  after. **Receive-only — no transmit — so region limits don't block sensing.**
- Reduce to an **activity_score 0–100** (normalised peak/aggregate) plus the
  **hottest band** (for flavour).
- This is the ONLY hardware-dependent part; it lives behind a seam
  `hunt_sense(uint8_t *activity_out, uint8_t *band_out)` so the reward logic is
  pure and host-testable. On host tests the seam is stubbed with fixed values.

### Catch resolution (pure logic, host-tested)
`catch_roll(activity_score, band, rng) -> Catch{category, tier, value, flavor}`:
- **category** weighted roll, shifted toward better outcomes by activity:
  Prey (common) → Treasure (uncommon) → Egg (rare). Higher activity raises the
  odds and tiers of Treasure/Egg.
- **tier/value:** prey size (feeds N hunger/happiness), treasure value (hoard
  points), egg rarity (common/rare).
- **flavor:** the hottest band picks a name theme (e.g. 433→"wyrm", 868→"drake
  spirit", 915→"sky serpent", 315→"gremlin") for the reveal text and the dex-ish
  flavour. Purely cosmetic in v0.2.

### Applying a catch
- **Prey** → restores hunger (+ a little happiness). The fun alternative to
  waiting; hunting genuinely sustains the pet.
- **Treasure** → `hoard += value`.
- **Egg** → `eggs[rarity] += 1` (hatchery).

### Cooldown (retention, not grind)
A real-time **forage cooldown** (`FORAGE_COOLDOWN`, e.g. ~3 min normal /
seconds in DEBUG) tracked by `last_forage_time`. While cooling down, Forage
shows "still sniffing…". Gives a reason to return without being punishing.

---

## 4. Expeditions (v0.3)

- **Send-off:** choose a duration — **Short 30 m / Long 2 h / Epic 8 h**. The
  dragon leaves: `on_expedition=1`, off-screen, **needs paused** while away
  (like sleep). You cannot interact until it returns.
- **Return:** on next open after the duration elapses (RTC), resolve the trip:
  loot (prey/treasure/eggs) scaled by duration and the dragon's level/care, plus
  a short generated **journey log**. **Small risk** (rises with duration) of
  returning **hurt** (health hit) or empty-handed.
- Resolution is pure logic (`expedition_resolve(duration, care, rng)`),
  host-tested; the RTC elapsed check reuses the existing clock seam.

---

## 5. Economy & progression

- **Hoard → Rank:** cumulative treasure maps to a title ladder (e.g. "Nest
  Scrounger" → "Wyrm of Wealth" → "Dragon Sovereign"). Shown on the Stats
  screen. Pure score/prestige in this feature (no shop yet).
- **Hatchery:** counts of collected eggs by rarity.
- **Legacy (v0.2, lite):** when the dragon **dies**, if the hatchery holds an
  egg, offer **"Hatch heir"** — consume an egg to start a fresh dragon that
  inherits a small head-start (e.g. +starting care or a care-score floor by egg
  rarity). Makes death a turning point, not a dead end. (Deeper genetics =
  later breeding update.)

---

## 6. Data model additions

Append to `PersistentGameState` (POD; bump save magic/version, fresh start — no
migration from v0.1.x):
- v0.2: `uint32_t hoard; uint16_t eggs_common; uint16_t eggs_rare;
  uint32_t last_forage_time;`
- v0.3: `uint8_t on_expedition; uint32_t expedition_start;
  uint16_t expedition_minutes; uint8_t expedition_pending;` (+ any resolved-loot
  scratch handled at return).

New pure modules: `hunt.c/.h` (catch_roll, apply_catch, cooldown check),
`economy.c/.h` (hoard rank, hatchery, legacy hatch), `expedition.c/.h` (v0.3).
Hardware seam: `hunt_sense()` in a thin `hunt_hw.c` (device) / stub (host).

---

## 7. UI / screens

- **Main pet view:** add a **Hunt** entry point (extend the action selector, or
  an "Adventure" sub-menu opened with a button) — avoid crowding the six care
  actions; likely a short menu: Hunt / Expedition / Hoard.
- **Catch reveal:** a small animation + line ("Caught a 433 MHz wyrm! +18
  hunger" / "Treasure! +7 hoard" / "A rare egg!"). Reuses sound/vibro cues.
- **Expedition screen (v0.3):** pick duration → "away" state on main view →
  return log popup.
- **Hoard & Hatchery (Stats):** rank/title, hoard total, egg counts.

---

## 8. Build, test, verify

- **Host tests (TDD):** `catch_roll` distributions & activity influence (seeded),
  `apply_catch` effects, cooldown gating, hoard→rank thresholds, legacy-hatch,
  and (v0.3) `expedition_resolve` scaling/risk. All with the existing seedable
  RNG + clock seam.
- **On-device:** RF sensing (`hunt_sense`) — verify a forage reads plausible,
  varying RSSI and that catches differ by environment; GUI screens; save
  round-trip. Deploy via `push_big.py`; DEBUG build for fast iteration.
- Build against 1.4.3 / API 87.1; keep the FAP within memory (radio buffers are
  released immediately after sensing).

---

## 9. Explicitly out of scope (future epic updates)

IR fire-breath; NFC-tag destinations; breeding/genetics; Flipper-to-Flipper
battles/trading; multiple concurrent dragons (a stable); a treasure shop.

---

## 10. Delivery

- **v0.2 — Hunt:** Forage + real-RF sensing + variable catches + hoard/rank +
  hatchery + legacy-hatch-on-death. (This spec's first implementation plan.)
- **v0.3 — Expeditions:** send-off + journey log + risk, on the same reward pools.

---

## 11. Feasibility & constraints (verified)

- APIs present in SDK: `furi_hal_subghz_set_frequency` / `..._rx` /
  `furi_hal_subghz_get_rssi` (RSSI sensing); NFC poller and IR worker exist for
  later. RX-only sensing sidesteps TX region limits.
- No background execution: hunting happens while playing; expeditions are
  offline-simulated on return (RTC), like the existing fast-forward.
- No accelerometer: "exploration" comes from RF/where-you-are, not steps.
