# Maze 3D

A first-person 3D raycasting maze game for Flipper Zero, tuned for the 128x64 monochrome display (ST7567 LCD controller, reflective). No extra hardware required. Runs on stock Flipper Zero firmware.

## Features

- **3D raycasting engine** — distance shading, wall orientation brightness, dithered floor & ceiling, 12 wall textures.
- **50-level Campaign** with three progressive stages:
  - Levels 1-10: Pure Maze — find the exit. BFS-guaranteed solvable.
  - Levels 11-20: Puzzle — collect keys to open locked doors; keys are always placed before doors via BFS reachability check.
  - Levels 21-50: Combat — shoot all enemies to unlock the exit. Enemy count, HP, and maze size scale every few levels.
- **Minecraft Sandbox Mode** — a 15x15 open world with 12 textured blocks (grass, dirt, sand, wood, log, stone, brick, water, leaves, metal, vine, torch). Mine (long OK), place (short OK), cycle block (short Back). Animated water + day/night cycle with sun/moon arc + starfield at night.
- **Endless Run** — find the exit to descend one floor deeper. Pick any starting floor: easier floors (1-10) or combat floors (21+).
- **Visitor Mode** — free roam with NPCs wandering the maze. No combat, no goals — just explore.
- **Score Shop System** — earn points by killing enemies (+10 per kill), spend them in the Shop (main menu entry 6) to buy 10 useful items. Items are stored in inventory slots and must be used manually from the item bar.
- **10 Usable Items** (all fully functional):
  | # | Item | Price | Effect |
  | --- | --- | --- | --- |
  | 0 | Potion S | 30 pts | +5 HP |
  | 1 | Potion L | 50 pts | +10 HP |
  | 2 | Full Heal | 80 pts | Restore to full HP |
  | 3 | Ammo +5 | 20 pts | +5 bullets |
  | 4 | Ammo Max | 80 pts | Fill ammo to 99 |
  | 5 | Key +1 | 20 pts | +1 key |
  | 6 | Torch +3 | 15 pts | +3 torches |
  | 7 | Amulet | 70 pts | Warp back to start tile |
  | 8 | Shield | 100 pts | 5-second invincibility (300 frames) |
  | 9 | 2x Fire | 120 pts | 15-second double shot — fires 2 bullets per shot (900 frames, 11 deg offset) |
- **Item Inventory** — long-press OK during Campaign/Combat/Endless to open the item bar. Use Up/Down to select, OK to use, Back to return. Shows current HP, ammo count, and active buff timer progress bars.
- **Buff System** — Shield grants 5 seconds of damage immunity; Double Fire fires two bullets per shot (offset 11 degrees) for 15 seconds. HUD progress bars show remaining frame time.
- **Health & Combat** — base 10 HP (+2 per 5 levels, max 20), 2-second invincibility after hit, +1 HP/sec regen. Pistol shooting with particle sparks, explosions, bullet trails, and walking dust. Enemy AI: sight-line raycast check + chase + strafe + ranged shooting.
- **World Pre-generation Cache** — complete maze snapshots are serialized to App Data so subsequent loads are instant.
- **BFS Pathfinding** — maze generation uses BFS to find the farthest reachable cell as exit, and BFS reachability for key placement — no dead ends, no keys behind locked doors.
- **Precision Controls** — short tap = small precise step, hold = accelerating movement (the longer you hold, the faster it goes). Turn short tap = 11.5 degrees, hold acceleration up to 2.9 deg/frame. Move short tap = 0.15 cells, hold acceleration up to 0.042 cells/frame.
- **Jump (MC mode)** — long press Back to jump (parabolic arc, 28 frames, peak ~9px camera height, landing dust particle effect).
- **Achievements** — persistent across sessions. Unlocks: First Blood, First Clear, 10 Kills, 50 Kills, 10 Clears, 25 Clears, Miner 50, Reach Combat, Reach Late Game.
- **Bilingual Support** — full Chinese and English UI. Long-press Back on the main menu to toggle language. A small CN or EN tag appears top-right to show the active language. In English mode, all menus, settings (26 items), shop, item names and descriptions, buff labels, toast messages, and stage-cleared prompts are 100 percent English with zero Chinese leakage.
- **Dev Settings** — 26 configurable settings across 6 categories. Unlock via a hidden code on the main menu. Categories:
  - Basic — Sound FX toggle, Opening animation toggle, Contrast, Language, Save Slot
  - Control — Turn sensitivity (3 levels), Short-turn angle, Move speed (3 levels), Back ratio, Jump height (Off/6px/9px/12px), 3x-back-exit toggle
  - Video — Render density (32/48/64 columns), Fog toggle, Brightness (3 levels), Sky/Ceiling toggle
  - Audio — Volume (Low/Mid/High), Menu SFX toggle
  - Game — Maze scale (3 levels), Start HP (10/15/20), MC map size (3 levels), MC start block, Max enemies
  - Debug — Debug info toggle
- **Contrast Crosshair** — XOR-rendered, always visible against any background (walls, sky, sprites).
- **Auto Save** — campaign progress, endless floor, settings, achievements, score, the 10-slot item inventory, and the world pre-generation cache all persist across sessions.
- **Performance Tuned** — half-resolution raycasting (64 columns), on-demand world tick (~8 Hz), 12 KB stack. Short opening animation with SFX, plus 18+ speaker sound effects for shooting, hits, explosions, steps, jumps, pickups, UI buttons, and more.

## Controls

| Key | In-game (Campaign / Endless / Combat / Visitor) | MC Sandbox Mode |
| --- | --- | --- |
| Up / Down | Move forward / backward | Same |
| Left / Right | Turn left / right | Same |
| OK (short) | Shoot / Confirm menu selection | Place block |
| OK (long) | Open item inventory bar | Mine block (hold until block breaks) |
| Back (short) | Pause menu | Cycle held block type |
| Back (long) | Jump (MC mode) / 3x long Back to force exit app | Jump (parabolic arc) / 3x long Back to exit |
| Long Back on main menu | Toggle language (Chinese / English) — CN/EN tag top-right | Same |

## Mode Guide

- **Campaign (Levels 1-10 / 11-20 / 21-50)** — the main story arc. Stages 1-10 teach movement; stages 11-20 introduce key-and-door puzzles; stages 21-50 add enemy combat. Between fights, visit the Shop to spend combat points on items, then open the item bar (long OK) mid-level to use them.
- **MC Sandbox** — creative building mode. 12 block types, all with raycasted textures. Short OK places a block, long OK mines it, short Back cycles the held block, long Back jumps. Watch the sun arc across the sky during the day, then switch to a starfield at night. Animated water tiles shimmer while you build.
- **Endless Run** — procedurally generated floors, descending forever. Pick a starting floor, find the glowing exit tile to go one level deeper.
- **Visitor Mode** — no enemies, no timers, no goals. Walk the maze freely and watch NPC sprites wander around. Good for relaxing and testing controls.
- **Shop** — main menu entry 6. Spend combat points on 10 usable items. Items land in inventory and must be manually activated via the item bar (long OK during gameplay).
- **Settings** — adjust controls, video, audio, and game parameters. Long-press Back here to return to main menu.

## Tips for new players

1. Follow the compass arrow at the top of the screen — it always points to the exit.
2. On Combat stages (21+), every enemy must be dead before the exit opens. If the exit tile is not glowing, someone is still alive.
3. Collect every key you see before a locked door — BFS guarantees a key is never behind the door it opens, so if you can not find one, backtrack and search more carefully.
4. Long-press OK during combat to open the item bar and pop a Shield right before a big fight. The 5-second invincibility is enough to clear most rooms safely.
5. In MC mode, try the day/night cycle by standing still for about two minutes — the sun sets and the starfield appears.

## Hardware requirements

- Stock Flipper Zero (any hardware revision).
- Firmware 1.4.3 or newer (API 87.1, Target 7 / armv7m).
- MicroSD card is optional — the game saves to App Data even without one. An SD card is only needed if you sideload the FAP manually.
- No external hardware required.

## Building

Install ufbt, update the SDK, and build. Run these commands in your terminal: pip install ufbt, then ufbt update, then ufbt build. The compiled maze3d.fap will be in the dist folder. Copy it to apps/Games/maze3d.fap on the Flipper SD card, or sideload directly via qFlipper.

## License

MIT
