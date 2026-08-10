# Maze 3D

A first-person 3D raycasting maze game for Flipper Zero, tuned for the 128x64 monochrome display.

## Features

- **3D raycasting engine** — distance shading, wall orientation brightness, dithered floor & ceiling, 12 wall textures.
- **50-level Campaign** with three progressive stages:
  - Levels 1-10: Pure Maze — find the exit. BFS-guaranteed solvable.
  - Levels 11-20: Puzzle — collect keys to open locked doors; keys are always placed before doors via BFS reachability check.
  - Levels 21-50: Combat — shoot all enemies to unlock the exit. Enemy count, HP, and maze size scale every few levels.
- **Minecraft Sandbox Mode** — a 15x15 open world with 12 textured blocks (grass, dirt, sand, wood, log, stone, brick, water, leaves, metal, vine, torch). Mine (long OK), place (short OK), cycle block (short Back). Animated water + day/night cycle with sun/moon arc + starfield at night.
- **Endless Run** — find the exit to descend; pick any starting floor.
- **Visitor Mode** — free roam with NPCs wandering the maze.
- **Score Shop System** — earn points by killing enemies (+10 per kill), spend them in the shop to buy 10 useful items. Items are stored in inventory and must be used manually.
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
  | 7 | Amulet | 70 pts | Warp back to start |
  | 8 | Shield | 100 pts | 5-second invincibility (300 frames) |
  | 9 | 2x Fire | 120 pts | 15-second double shot — fires 2 bullets per shot (900 frames) |
- **Item Inventory** — long-press OK during Campaign/Combat/Endless to open the item bar. Use Up/Down to select, OK to use, Back to return. Shows current HP/ammo and active buff timers.
- **Buff System** — Shield grants 5 seconds of damage immunity; Double Fire fires two bullets per shot (offset 11 degrees) for 15 seconds. HUD progress bars show remaining time.
- **Health & Combat** — base 10 HP (+2 per 5 levels, max 20), 2-second invincibility after hit, +1 HP/sec regen. Pistol shooting with particle sparks, explosions, bullet trails, and walking dust. Enemy AI: chase + ranged shooting.
- **World Pre-generation Cache** — complete maze snapshots are saved to App Data for instant loading.
- **BFS Pathfinding** — maze generation uses BFS to find the farthest reachable cell as exit, and BFS reachability for key placement — no dead ends, no keys behind doors.
- **Precision Controls** — short tap = small precise step, hold = accelerating movement (the longer you hold, the faster it goes). Turn: short tap = 11.5 degrees, hold = accelerating rotation up to 2.9 deg/frame. Move: short tap = 0.15 cells, hold = accelerating up to 0.042 cells/frame.
- **Jump (MC mode)** — long press Back to jump (parabolic arc, 28 frames, peak 9px height, landing dust effect).
- **Achievements** — persistent across sessions (First Blood, First Clear, 10/50 Kills, 10/25 Clears, Miner 50, Reach Combat/Late).
- **Bilingual Support** — full Chinese and English UI. Long-press Back on the main menu to toggle language. In English mode, all menus, settings (26 items), shop, item names/descriptions, and buff labels are 100% English with zero Chinese leakage.
- **Dev Settings** — unlock 26 configurable settings (turn sensitivity, move speed, jump height, fog, brightness, volume, maze scale, MC world size, etc.) via a hidden code on the main menu.
- **Contrast Crosshair** — XOR-rendered, always visible against any background.
- **Auto Save** — campaign progress, endless floor, settings, achievements, score, item inventory, and world cache all persist.
- **Performance Tuned** — half-resolution raycasting (64 columns), on-demand world tick (~8 Hz), 12 KB stack. Opening animation + SFX, 18+ speaker sound effects.

## Controls

| Key | In-game | MC Mode |
| --- | --- | --- |
| Up / Down | Move forward / backward | Same |
| Left / Right | Turn left / right | Same |
| OK (short) | Shoot / Confirm | Place block |
| OK (long) | Open item inventory | Mine block |
| Back (short) | Pause | Cycle held block |
| Back (long) | Jump (MC) / 3x long-press Back to exit | Jump + 3x to exit |

## How to play

1. Open the app; the main menu shows all modes.
2. Use Up/Down to pick a mode, OK to start.
3. Look at the compass at the top — it always points toward the exit.
4. Kill enemies to earn points, then visit the Shop (menu item 6) to buy items.
5. During gameplay, long-press OK to open the item bar and use items.
6. Collect keys to open doors, avoid traps, shoot enemies, reach the glowing exit tile.

## Building

Install ufbt, update the SDK, and build. Run these commands in your terminal: pip install ufbt, then ufbt update, then ufbt build. The compiled maze3d.fap will be in the dist folder. Copy it to apps/Games/maze3d.fap on the Flipper SD card, or sideload via qFlipper.

## License

MIT
