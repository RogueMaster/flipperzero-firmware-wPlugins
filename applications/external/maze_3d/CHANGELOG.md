v6.11:
- Score Shop System: earn points by killing enemies (+10 per kill), spend on 10 usable items.
- 10 Items with full functionality: Potion S (+5HP), Potion L (+10HP), Full Heal, Ammo+5, Ammo Max, Key+1, Torch+3, Amulet (warp to start), Shield (5s invincibility), 2x Fire (15s double shot).
- Item Inventory: long-press OK during gameplay to open item bar. Up/Down to select, OK to use, Back to return.
- Buff System: Shield (300 frames invincibility) and Double Fire (900 frames, fires 2 bullets per shot at 11-degree offset). HUD progress bars show remaining time.
- Bilingual Support: full Chinese and English UI. In English mode, all menus, settings (26 items), shop, item names/descriptions, and buff labels are 100% English.
- Dev Settings: 26 configurable parameters with bilingual labels (turn sensitivity, move speed, fog, brightness, volume, maze scale, MC world size, etc.).
- Item inventory persistence: items array (10 entries) saved to SaveData, persists across sessions.
- Zero Chinese leakage in English mode: all settings labels, category tags, option values (On/Off, Low/Mid/High, etc.), shop descriptions, and buff timers fully translated.

v6.10:
- Enemy AI: smooth movement, auto-lock targeting.
- Near-death blinking: player flashes when HP is low.
- Wall occlusion: enemies behind walls are hidden.
- MC mode crash prevention: added bounds checking.

v6.8:
- Precision controls: short tap = small precise step, hold = accelerating movement (longer hold = faster). Turn short tap = 11.5 deg, hold up to 2.9 deg/frame. Move short tap = 0.15 cells, hold up to 0.042 cells/frame.
- MC mode jump: long press Back to jump (parabolic arc, 28 frames, peak 9px, landing dust).
- Triple long-press Back to exit (anti-misclick protection, 1.5s window).
- Texture system expanded to 24 slots (12 active + 12 reserved for future blocks).
- Jump visual: all projected elements (walls, sprites, enemies, bullets, particles) shift with camera height.

v6.6:
- BFS pathfinding replaces Manhattan distance for exit placement — guarantees solvable mazes (no dead ends).
- BFS reachability check for key placement — keys never hidden behind doors.
- World pre-generation cache — complete maze snapshots saved to App Data for instant loading.
- Endless mode stage mismatch fix — was passing floor+100 causing false combat branch.
