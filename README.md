# Maze 3D / 3D 迷宫

A first-person 3D raycasting maze game for Flipper Zero, tuned for the 128×64 monochrome display. Four modes: 50-level Campaign, Endless Run, Visitor roam, and Minecraft-style Sandbox with day/night cycle and particle effects.

一款为 Flipper Zero 打造的第一人称 3D 光线投射迷宫游戏,针对 128×64 单色屏深度优化。四种模式:50 关闯关、无尽挑战、游客漫游,以及带昼夜循环和粒子特效的 Minecraft 风格沙盒。

## Features / 特性

- **3D raycasting engine** — distance shading, wall orientation brightness, dithered floor & ceiling, 12 wall textures.
- **50-level Campaign** with three progressive stages:
  - **1–10 Pure Maze** — find the exit. BFS-guaranteed solvable.
  - **11–20 Puzzle** — collect keys to open locked doors; keys are always placed before doors via BFS reachability check.
  - **21–50 Combat** — shoot all enemies to unlock the exit. Enemy count, HP, and maze size scale every few levels.
- **Minecraft Sandbox Mode** — a 15×15 open world with 12 textured blocks (grass, dirt, sand, wood, log, stone, brick, water, leaves, metal, vine, torch). Mine (long OK), place (short OK), cycle block (short Back). Animated water + day/night cycle with sun/moon arc + starfield at night.
- **Endless Run** — find the exit to descend; pick any starting floor.
- **Visitor Mode** — free roam with NPCs wandering the maze.
- **Health & Combat** — base 10 HP (+2 per 5 levels, max 20), 2-second invincibility after hit, +1 HP/sec regen. Pistol shooting with particle sparks, explosions, bullet trails, and walking dust. Enemy AI: chase + ranged shooting.
- **World Pre-generation Cache** — complete maze snapshots are saved to App Data for instant loading. No more waiting for generation.
- **BFS Pathfinding** — maze generation uses BFS (not Manhattan distance) to find the farthest reachable cell as exit, and BFS reachability for key placement — no dead ends, no keys behind doors.
- **Achievements** — persistent across sessions (First Blood, First Clear, 10/50 Kills, 10/25 Clears, Miner 50, Reach Combat/Late).
- **Contrast Crosshair** — XOR-rendered, always visible against any background.
- **Bilingual UI** — switch Chinese / English any time from the menu (← / → key).
- **Auto Save** — campaign progress, endless floor, settings, achievements, and world cache all persist.
- **Performance Tuned** — half-resolution raycasting (64 columns), on-demand world tick (~8 Hz), 12 KB stack. Opening animation + SFX, 18+ speaker sound effects.

## Controls / 操作

- **Up / Down** — Move forward / backward / 前进 / 后退
- **Left / Right** — Turn left / right (in menu: switch language) / 转向 (菜单中: 切换语言)
- **OK short** — Shoot (combat) / Place block (MC mode) / Confirm (menu) / 射击 (战斗) / 放置方块 (MC) / 确认 (菜单)
- **OK long** — Mine block (MC mode) / 挖掘方块 (MC 模式)
- **Back short** — Pause / Cycle held block (MC mode) / 暂停 / 切换方块 (MC)
- **Back long** — Exit to Flipper home / 退出到桌面

## How to play / 玩法

1. Open the app; the main menu shows all modes and the current language.
2. Use Up/Down to pick a mode, Left/Right to toggle 中文 / English, OK to start.
3. Look at the compass at the top — it always points toward the exit.
4. Collect keys to open doors, avoid traps, shoot enemies, reach the glowing exit tile.

## Building / 编译

```bash
pip install ufbt
ufbt update
ufbt build
```

The compiled `maze3d.fap` will be in `dist/`. Copy it to `apps/Games/maze3d.fap` on the Flipper's SD card, or sideload via qFlipper.

## License

MIT
