# Dragotchi — Design Spec

*A dragon-raising virtual pet for the Flipper Zero. Fork of MrModd's
[Matagotchi](https://github.com/MrModd/Matagotchi) (GPLv3); this derivative
stays GPLv3 and retains MrModd's copyright and attribution.*

Date: 2026-09-11
Working title: **Dragotchi** (`appid: dragotchi`) — easy to rename later.
Target: Flipper Zero, firmware **1.4.3 / API 87.1** (build with `ufbt` pinned
to that SDK so the `.fap` installs on the owner's device).

---

## 1. Goal & scope

Turn Matagotchi's thin three-stat pet into a faithful Tamagotchi-style
experience where **how you raise the dragon decides who it becomes**. This is
the "faithful core" scope: one focused, shippable app. Explicitly deferred to
a later version: a full play mini-game, multiple pet *species*, and a "weight"
stat.

Four things the original lacks that this delivers:

1. **Care-driven branching evolution** (white ↔ black dragon by care quality).
2. **Richer needs & interactions** (happiness, hygiene/poop, discipline, sleep).
3. **A life that continues while the app is closed**, plus attention calls.
4. **Dragon-themed art, state animations, and icon meters** instead of raw numbers.

### Honest constraint on "while closed"

A Flipper FAP does **not** execute while closed; stock 1.4.3 has no background
app execution. So the app cannot literally beep while it is not open. We
approximate the feeling three ways, all faithful:

- **Offline simulation** — on launch, reconstruct everything that happened
  while away from the RTC clock (hunger fell, it pooped, fell ill, aged, maybe
  died).
- **"While you were away" alarm** — the instant the app opens, if the dragon
  needs urgent care it immediately beeps + vibrates and shows a summary.
- **Live attention calls** — while open, beep + vibrate + alert bubble the
  moment a need goes critical, and occasional "discipline calls" (see §4).

---

## 2. Reused foundation (from Matagotchi)

Keep and build on the parts that already work and are validated on-device:

- **Two-thread model**: GUI thread + game-logic thread over a `FuriMessageQueue`.
- **Per-feature module pattern**: each need implements `init_/check_/apply_/get_text_`.
- **RTC offline fast-forward** on load (deepened here).
- **Versioned save file** with magic + version header.
- **Scene-based GUI** (`SceneManager`/`ViewDispatcher`) + the custom
  `button_panel` widget.
- **Sound + vibration** hooks and a settings screen (sound / vibration / reset / about).

---

## 3. Needs model

### Visible meters (drawn as icon + 4 pips, Tamagotchi-style)

| Meter | Icon | Falls because | Raised by |
|-------|------|---------------|-----------|
| **Hunger**    | food  | time | **Feed** |
| **Happiness** | heart | time, neglect | **Play** |
| **Health**    | cross | starvation, illness, filth | **Medicine**, good care |

### On-screen states (events, not meters)

- **Poop** 💩 — appears periodically; cleared by **Clean**. Left too long →
  raises illness chance and counts as a care mistake.
- **Sick** 🤒 — random (odds rise when hungry/dirty); cured by **Medicine**;
  ignored illness drains Health and is a care mistake.
- **Asleep** 😴 — at night (RTC hours, default 22:00–08:00). Waking/keeping the
  light on is a care mistake that drains Happiness/Health.

### Hidden stats (shown only on the Stats screen)

- **Discipline** (0–100) — raised by **Scold**ing during a "discipline call"
  (a beep with no real need); spoiling it then (feeding/playing instead) lowers it.
- **Care score** (0–100, starts 50) — a running quality tally. It is the input
  to evolution branching. Adjusted by:
  - **+** feeding when hungry, playing when unhappy, prompt cleaning, prompt
    healing, lights-out at night, correct scolding.
  - **−** (care mistakes) any meter hitting 0, poop ignored past a threshold,
    illness ignored, sleep disturbed, overfeeding, needless/over-scolding.

---

## 4. Actions (button panel)

**Feed · Play · Clean · Medicine · Scold · Lights**, plus **Stats** and **Settings**.

- **Feed** — +Hunger (one meal). Overfeeding when already full = care mistake
  (small penalty).
- **Play** — simple pet-and-boost: short happy animation, +Happiness, with a
  cooldown so it can't be spammed. *(No guessing mini-game — deferred.)*
- **Clean** — removes poop (+care if poop present; no-op otherwise).
- **Medicine** — cures sickness (+care if sick; no-op when healthy, like a real
  Tamagotchi).
- **Scold** — during a **discipline call**: +Discipline, +care. When the pet had
  a genuine need: −care (you misread it).
- **Lights** — toggle sleep at night. Lights-out when it wants sleep = good;
  lights-on at night = care mistake.

**Discipline calls**: occasionally (more often when Discipline is low) the dragon
beeps for attention with no low meter. Scolding then is correct; catering to it
spoils it. Mirrors the classic Tamagotchi discipline loop.

---

## 5. Evolution — the headline feature

Stages advance by **age** (elapsed time since hatching), not by raw XP:

| Stage | Reached ~ | Notes |
|-------|-----------|-------|
| **Egg**       | start   | hatches after a short warm-up / first care |
| **Hatchling** | day 0   | needs attention often |
| **Wyrmling**  | ~day 1  | |
| **Drake** (teen) | ~day 2.5 | |
| **Adult**     | ~day 4.5 | **branches by care** (see below) |
| **Dead**      | old-age death is care-gated — see §5 Death (immortal while care ≥ 90) | |

### Care-driven branch (at Drake → Adult)

The adult character is chosen from the **care score** at the moment of the
transition:

- **care ≥ 66 → White dragon** (the "good" ending)
- **33 ≤ care < 66 → Grey/Bronze dragon** (average)
- **care < 33 → Black dragon** (the "bad" ending)

*(Stretch, if cheap: a second minor branch at Wyrmling → Drake for a
light/dark drake variant. Not required for v1.)*

### Death

- **Health = 0** → dead (starvation or ignored illness), at any stage.
- **Old age** is *care-gated, not a fixed clock.* Once adult, an old-age check
  runs periodically but can only take the dragon if its **sustained care is
  below an excellence bar**:
  - **Care score ≥ 90 (very well looked after) → immortal**: the old-age check
    never fires. Keep it thriving and it lives forever.
  - **Below 90**: old-age mortality applies on a curve — the higher the care,
    the longer it lives; a neglected black dragon dies soonest. Rough guide:
    good care ~day 10–14, poor care sooner.
  - Immortality is *revocable*: let care slip below 90 and the old-age clock
    resumes; bring care back up and the dragon is safe again. (It can still die
    of Health = 0 at any time — immortality is only against *old age*.)

---

## 6. Timescale

Tuned for "check it a few times a day." Defaults (all `#define` constants,
adjustable; a `DEBUG` build compresses to seconds for testing):

- Hunger empties over ~3–4 h; Happiness over ~6 h.
- Poops 2–3×/day; may fall ill a few times over a life.
- Sleeps nightly per RTC (default 22:00–08:00).
- Adult by ~day 4–5. Old-age death is care-gated (see §5): ~day 10–14 on good
  care, sooner if neglected, and **never** while care stays ≥ 90.

---

## 7. Screen layout (128×64)

```
┌──────────────────────────────┐
│ food ●●●○   heart ●●●●  + ●●○○   ☾ │  meters + day/night
│                                    │
│            ( dragon                │
│             60×60 animated )   💩  │  live states
│                                    │
│ [Feed][Play][Clean][Med][Scold][☀]│  action row
└──────────────────────────────┘
```

- **Main scene**: meters top, animated dragon center, state icons (poop / sick
  / Zzz) as they occur, action panel bottom. Attention bubble + beep on
  critical need.
- **Stats scene**: stage, age (days), current alignment/character, Discipline,
  care summary (as words, e.g. "Well cared for").
- **Settings scene**: sound, vibration, reset, about (extends existing).

Animations per state: idle, eating, playing/happy, sick, sleeping (Zzz),
evolution "poof," death.

---

## 8. Art plan (I design it; 1-bit Flipper-native)

Reuse the egg sprite; draw new **dragon** pixel art for every stage and state,
sized like the originals (60×60 sprites, ~2 frames each; 20×20 action icons
matching MrModd's icon style):

- Stages: Egg (reuse/retheme) → Hatchling → Wyrmling → Drake → **Adult ×3
  (white / grey / black)** → Dead.
- State overlays/animations: eating, playing, sick, sleeping, evolve, poop 💩.
- Action icons: Feed, Play, Clean, Medicine, Scold, Lights.

Produced as 1-bit PNGs; the `fbt`/`ufbt` asset pipeline compiles them into the app.

---

## 9. Save format & migration

- Extend the persistent state struct with: happiness, hygiene (poop present +
  timer), sickness (present + timer), sleep state, discipline, care score,
  birth timestamp / age, adult alignment id, and per-need last-update timestamps.
- **Bump the save version and use a new magic byte.** No migration from
  Matagotchi v1 (it is a different game) — a v1 save is ignored and a fresh egg
  starts. Versioned header is retained.

---

## 10. Build, deploy, test

- **Build**: `ufbt` pinned to firmware **1.4.3 / API 87.1**. Output `.fap`
  deployed to `/ext/apps/Games/` (via the verified `write_chunk` upload) and
  launch-tested on the device (`loader open`).
- **Testing (TDD)**: the game logic is pure functions over structs. Build a
  **host test harness** — compile the logic files off-device against a small
  `furi` stub, with a seedable RNG — and test the deterministic behaviour:
  - evolution stage timing and the care→alignment branch thresholds,
  - care-score adjustments for each good/bad event,
  - old-age mortality: death on the expected curve below the excellence bar,
    and **no** old-age death while care ≥ 90 (immortality, and its revocation),
  - offline fast-forward math (elapsed time → correct number of hunger/poop/
    illness/age events),
  - save/restore round-trip and version handling.
  On-device testing covers GUI, input, sound/vibration, and animations.

---

## 11. Licensing & attribution

Derivative of MrModd's Matagotchi (GPLv3). The repo will retain the original
`LICENSE`, keep MrModd's copyright notices, credit the original in `README`
and the in-app About screen, and license Dragotchi under GPLv3. Upstream import
will be a clearly-labelled commit.

---

## 12. Out of scope (later versions)

- Full **play mini-game** (guessing game).
- Multiple pet **species** (beyond the dragon line).
- **Weight** stat and feeding/exercise economy.
- **Ways to show off the immortality achievement** — e.g. a crown/elder sprite,
  a badge, or a records screen tracking longest-lived dragon.
- Any true background/closed-app execution (not possible on stock firmware).
