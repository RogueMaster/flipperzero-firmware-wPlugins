# PocketLab – session brief (drop this into a fresh session to continue)

Gamified, on-device learning for Flipper Zero – "Duolingo for Flipper". This is
the **shipped** app, not a concept. Use this file to bootstrap a new dev session.

## Current state (as of v1.3)
- **Native C app** on the official Flipper SDK, built with `ufbt` (NOT Momentum
  JS – the early JS idea was dropped in favour of a native app so it can live in
  the official catalog).
- **36 labs across 11 tracks** (Basics, IR, Sub-GHz, RFID, NFC, iButton, USB,
  GPIO, Bluetooth, Security, System), including 6 "advanced" deep-dives
  (MIFARE keys/MFKey32, T5577 cloning, IR protocols, iButton cloning, advanced
  Bad USB, logic/bus sniffing).
- Quiz mode (exam drawn from completed labs), badge gallery with per-badge
  jingles, XP/levels/daily-streak, animated home tile grid, matrix-rain About +
  coffee/donation page.
- **Shipped: v1.3, tag `v1.3`, release commit `eb95266…`.** In the official
  catalog (update PR flipperdevices/flipper-application-catalog#1126, merging).

## Where things are
- Repo: **`PerfectoWeb/flipper-pocketlab`**, local at
  `/Volumes/BASE/www/flipper-pocketlab` (appid/filenames short = `pocketlab`).
- `pocketlab.c` (app entry + view/scene wiring), `pocketlab_i.h` (app struct,
  view/scene/event enums).
- `views/` – custom animated views: `home_view`, `lesson_view`,
  `labs_list_view`, `progress_view`, `badges_view`, `levelup_view`, `exam_view`,
  `about_view`.
- `scenes/` – SceneManager scenes; `scenes/pocketlab_scene_config.h` registers
  them (ADD_SCENE macro).
- **Content is data-driven:** `helpers/pocketlab_content.c/.h`. A lab = one
  `PocketLabLab` entry (id, title, badge, track, xp, 10x10 icon glyph, steps).
  A step = `PocketLabStep` of type Text / Quiz / Try / Reward. New lab = add a
  `*_steps[]` array + one entry in `pocketlab_labs[]`. `completed_mask` is
  `uint64_t` → max 64 labs.
- `helpers/pocketlab_sound.c/.h` – sound ids + `pocketlab_sound_play(...)`.
- Docs: `docs/catalog-description.md` + `docs/catalog-changelog.md` are what the
  **catalog** renders (NOT README – the catalog markdown sanitizer rejects
  images/external badges). `CHANGELOG.md`, `README.md` are GitHub-only.
- Web donation page (separate repo `ruview`): `/Volumes/BASE/www/ruview/d/index.html`.

## Build / verify
```bash
cd /Volumes/BASE/www/flipper-pocketlab
export PATH="$HOME/Library/Python/3.9/bin:$PATH"
ufbt            # build (shell cwd resets; always cd first)
ufbt format     # clang-format
ufbt lint       # must be clean; -Werror is on (cast uint32 vs int, no unused)
```
Cannot test on real hardware from here – build + lint is the gate; final check
is on the device.

## Roadmap / open ideas
- **More advanced labs** for the tracks that still only have basics:
  **Bluetooth, Security, System** (moderator asked for "advanced topics for each
  subsystem"; NFC/IR/RFID/iButton/USB/GPIO already have advanced labs).
- **Bonus hardware levels** (real IR capture→replay, NFC clone, 433 replay) were
  prototyped and **reverted**: the IR `InfraredWorker` path caused a UsageFault
  and every hardware module bloats the learning app. Decision: if revisited, do
  it as a **separate companion app**, not inside PocketLab.
- Catalog reach: after PR#1126 merges, PocketLab auto-propagates to Momentum
  (FlipStore uses the catalog API) and Unleashed/RogueMaster (via
  `xMasterX/all-the-plugins` catalog sync). Optional: PR to
  `djsime1/awesome-flipperzero` for visibility.

## Studio conventions (MUST follow)
- Reply to the user in **Russian**.
- **Never** mention AI/Claude/Anthropic in repos/output; **no** `Co-Authored-By`
  trailer; commit as the user.
- **Never auto-commit/push** – only edit files, the user commits.
- Commit subject **≤ 50 chars**. Comments in **English**, "why" not "what".
- Use "–" (en dash), **never** an em dash.
- Keep `docs/*`, `README`, `CHANGELOG`, catalog manifest in sync on every change.
