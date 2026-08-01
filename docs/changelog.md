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
