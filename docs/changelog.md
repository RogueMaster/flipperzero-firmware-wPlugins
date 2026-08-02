v4.3:
- Opening animation on app start: fluent 4-stage intro with BGM —
  (1) logo drops from the top with bounce easing + impact wave,
  (2) 8-direction particle/star burst from logo center with twinkles,
  (3) "k20120509 presents" subtitle slides up from the bottom,
  (4) blinking "press any key" prompt.
  A happy C-major chiptune BGM (40 notes, ~4.5s) plays on a dedicated
  sequencer channel during the opening. Pressing any key skips straight
  to the menu. Can be disabled from the new Settings screen (persisted).
- SFX (sound effects) using the Flipper Zero speaker: menu move / confirm,
  pickup key / item, door open / locked, trap / damage, attack hit / enemy
  kill, quest complete, level clear, game over, story page turn. A tiny
  tick-based sequencer in sfx.c plays short note sequences (C/D/E/F/G/A/B/C6
  scale) with no external audio files. BGM has priority over SFX.
- New menu item #4 "Settings": two toggles — SFX on/off and Opening on/off,
  persisted to save (MAZ4 magic, MAZ3 old saves migrated). Up/Down choose
  row; OK or Left/Right toggle; Back returns to menu.
- Save format bumped to MAZ4 with explicit settings fields; older MAZ3 saves
  are still read (settings fall back to on/on).

v4.2:
- Quest / task system for story levels: long-press OK now pauses the game and
  opens the Inventory (instead of the map panel). The Inventory is now a 2-page
  view — page 1 lists items, page 2 shows the current level's quest. Press
  Left/Right to switch pages (page 2 only exists when the level has a quest).
- Only "story" levels carry quests: Level 1 (Find Exit), puzzle levels 10-19
  (Get Key + Open Door), combat levels 20+ (Kill all enemies). Other campaign
  levels and endless/visitor modes have no quest.
- Quest tracking: picking up a key, opening a door, reaching the exit, and
  killing enemies auto-update each subtask's progress with a checkbox and
  x/N counter. Completing all subtasks grants a one-time reward (full HP
  restore).
- Enemy combat: enemies now have HP (2). Dashing (OK) into an enemy in front
  of you deals 1 damage instead of moving; two dashes kill it. Dead enemies
  stop attacking.
- Map panel preserved: from the Inventory, long-press OK opens the full map
  panel (minimap + exit arrow + HUD); OK/Back returns to the game.

v4.1:
- FIX: Endless Mode crash root cause found and fixed. The maze-carving DFS
  used two local int arrays of MAP_MAX*MAP_MAX (31*31*4 = 3844 bytes each,
  7688 bytes total) on the 8KB stack — combined with the rest of the call
  chain this overflowed the stack and crashed the app every time a new
  endless floor was generated. Arrays moved to static storage (BSS);
  stack_size also raised from 8KB to 12KB as a safety margin. Endless Mode
  now plays through floor transitions without freezing.

v4.0:
- Critical stability fixes for Endless Mode: tight size cap (max 19x19 maze cells, DDA steps capped, render loop guarded), cleared pending turn/move targets at every new level, better seed hashing to avoid repeated similar layouts — eliminates the freeze/crash when selecting or progressing in Endless
- Smooth controls rewrite: direction keys now set "target" rotation/move amounts; game_update applies a fractional step each frame (~45% of remaining turn target) so rotating, moving forward/back, and dashing all feel progressively smoother instead of one-shot jerky steps
- Big exit marker in 3D view: a giant black down-arrow (↓) painted at the top of the screen at the horizontal offset matching the exit direction relative to player facing; flickers every 16 frames; when exit is straight ahead (within ±20°), a big blinking rectangular frame is drawn around screen top to signal "exit is directly in front" — you see this anywhere in the maze, not just when nearby
- 3D sprite rendering of ground items: keys/torches/potions/amulets/traps/exit cells are raycast-projected onto the screen at proper floor depth with size scaling, each with a distinct glyph (key = hollow square, torch = cross, potion = solid square, amulet = diamond, trap = X, exit = blinking frame); flickers for visibility
v3.3:
- Level select screen rewrite: smaller FontSecondary text, scrollable list (7 visible rows) with up/down navigation and ^/v scroll indicators
- In-game HUD is now hidden by default for an unobstructed 3D view; long-press OK opens a full Map Panel instead
- New Map Panel (long-press OK): compact HUD bar (level/floor + HP + keys + torches + potions + amulets) in the smallest font, full-maze minimap centered on screen, player crosshair marker with facing line, and a large bold black arrow pointing from the nearest edge straight to the exit cell (exit cell also drawn as a solid black box for maximum visibility)
- Map Panel controls: short OK / Back closes the panel and resumes play; long-press OK again jumps into the Inventory
- Inventory screen now shows a compact HUD status bar (level/floor + HP) at the top so opening the inventory also serves as a status dashboard

v3.2:
- Full Chinese localization of story mode, inventory and level-select screens (XBM bitmaps for titles, body lines, item names, level tags)

v3.1:
- Force full rebuild of maze3d.fap so the published binary actually contains the v3.0 features (previous v3.0 fap was a stale artifact)

v3.0:
- Remove old endless-mode crash: cap maze size (<=21), always enable exit compass, strict actor limit
- Endless mode now lets you pick ANY level to enter (level select screen)
- New Inventory: long-press OK in game to open, Up/Down to move cursor, OK to use item, Back to resume
- New items: Potion (restore HP), Amulet (warp back to start) — shown on minimap and HUD
- New Story mode (renamed Campaign): 600-word prologue with 2 branching choices (Warrior +HP / Seeker +torch) that change starting stats
- Level select before entering Story mode: cleared levels replayable, uncleared levels locked
- Story text system (story.c) with paged English narration
- HUD shows Potion (P) and Amulet (A) counts

v2.1:
- Bilingual UI: switch Chinese / English from the menu (Left / Right key)
- All HUD labels, prompts and overlay text support both languages
- Updated application.fam metadata (author, version 2.1, English description)
- README.md and changelog added for App Catalog submission

v2.0:
- Half-resolution raycasting (64 columns) for stable performance
- On-demand world tick (~8 Hz) to prevent flicker / crash in endless mode
- Stack size raised to 8 KB
- Compass pointing to exit, minimap, exit highlight
- All in-game text replaced with Chinese XBM bitmaps (zh_chars.h)
- Pause (short Back) / Exit (long Back) flow

v1.0:
- Initial release: Campaign + Endless + Visitor modes
- Recursive-backtracker maze generation, difficulty scaling
- Items (key / torch / trap / door), enemy AI, NPC visitors
- 4 wall textures with distance & orientation shading
