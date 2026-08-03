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
- **Health & Combat** — base 10 HP (+2 per 5 levels, max 20), 2-second invincibility after hit, +1 HP/sec regen. Pistol shooting with particle sparks, explosions, bullet trails, and walking dust. Enemy AI: chase + ranged shooting.
- **World Pre-generation Cache** — complete maze snapshots are saved to App Data for instant loading.
- **BFS Pathfinding** — maze generation uses BFS to find the farthest reachable cell as exit, and BFS reachability for key placement — no dead ends, no keys behind doors.
- **Precision Controls** — short tap = small precise step, hold = accelerating movement (the longer you hold, the faster it goes). Turn: short tap = 11.5 degrees, hold = accelerating rotation up to 2.9 deg/frame. Move: short tap = 0.15 cells, hold = accelerating up to 0.042 cells/frame.
- **Jump (MC mode)** — long press Back to jump (parabolic arc, 28 frames, peak 9px height, landing dust effect).
- **Achievements** — persistent across sessions (First Blood, First Clear, 10/50 Kills, 10/25 Clears, Miner 50, Reach Combat/Late).
- **Contrast Crosshair** — XOR-rendered, always visible against any background.
- **Auto Save** — campaign progress, endless floor, settings, achievements, and world cache all persist.
- **Performance Tuned** — half-resolution raycasting (64 columns), on-demand world tick (~8 Hz), 12 KB stack. Opening animation + SFX, 18+ speaker sound effects.

## Controls

| Key | In-game | MC Mode |
| --- | --- | --- |
| Up / Down | Move forward / backward | Same |
| Left / Right | Turn left / right | Same |
| OK (short) | Shoot / Confirm | Place block |
| OK (long) | Open inventory | Mine block |
| Back (short) | Pause | Cycle held block |
| Back (long) | Jump (MC) / 3x long-press Back to exit | Jump + 3x to exit |

## How to play

1. Open the app; the main menu shows all modes.
2. Use Up/Down to pick a mode, OK to start.
3. Look at the compass at the top — it always points toward the exit.
4. Collect keys to open doors, avoid traps, shoot enemies, reach the glowing exit tile.

## Building

```bash
pip install ufbt
ufbt update
ufbt build
```

The compiled `maze3d.fap` will be in `dist/`. Copy it to `apps/Games/maze3d.fap` on the Flipper's SD card, or sideload via qFlipper.

## License

MIT
