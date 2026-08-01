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
