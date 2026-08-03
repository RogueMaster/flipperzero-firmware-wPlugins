# Maze 3D / 3D 迷宫

A first-person 3D raycasting maze game for Flipper Zero, with three game modes, in-app language switching (Chinese / English), compass + minimap, save progress, and a small-footprint renderer tuned for the Flipper's 128×64 monochrome display.

一款为 Flipper Zero 打造的第一人称 3D 光线投射迷宫游戏,内含三种玩法、中英文随时切换、罗盘 + 小地图、进度自动保存,并针对 128×64 单色屏做了轻量化渲染优化。

## Features / 特性

- **3D raycasting engine** — distance shading, wall orientation brightness, Bayer-dithered floor / ceiling, 9 wall textures.
- **v6.0 50-level campaign / 50 关闯关** — three progressive stages:
  - **1–10 纯迷宫** — pure maze, just find the exit.
  - **11–20 解谜关** — collect N keys (scaling up) and open the locked door to unlock the exit.
  - **21–50 战斗关** — pistol combat: kill ALL enemies before the exit unlocks. Enemy count + HP + maze size scale every few levels.
  - The exit is **locked** (CELL_LOCKED_EXIT) until the level's quest is done — no more clicking through to instant-clear.
- **MC sandbox mode / MC 沙盒模式** — a 15×15 world with grass / tree / water / sand / wood / brick terrain and a bedrock border. Mine blocks (OK), place blocks (long OK), cycle held block (short Back). 6 holdable block types; mining counts toward the Miner achievement. A center crosshair aids aiming.
- **Endless Run / 无尽挑战** — find the exit to descend, auto-saves the floor.
- **Visitor / 游客漫游** — free exploration with other "visitors" wandering the maze.
- **Achievement system / 成就系统** — persistent across sessions (First Blood, First Clear, 10/50 Kills, 10/25 Clears, Miner 50, Reach Combat/Late). Each unlock fires a popup toast + arpeggio SFX.
- **Combat: pistol + crosshair / 手枪 + 准星** — OK shoots (limited ammo, 4-cell range, raycast hit detection); a center crosshair is drawn in combat and MC modes.
- **Compass + Minimap** — points to exit, shows layout around the player.
- **Bilingual UI / 双语界面** — switch Chinese / English any time from the menu (← / → key).
- **Rich feedback / 反馈丰富** — 18 SFX (incl. shoot / locked / achievement / no-ammo), task progress + quest-done toasts, always-on HUD (level / HP / enemy count / keys / ammo).
- **Auto save / 自动存档** — campaign progress, endless floor, settings, and achievements persist (MAZ5 format; backward-compatible with MAZ4/MAZ3).
- **Performance tuned / 性能优化** — half-resolution raycasting, on-demand rendering (~8 Hz world tick), 12 KB stack.

## Controls / 操作

| Key / 按键 | Action / 动作 |
| --- | --- |
| `Up` / `Down` | Move forward / backward / 前进 / 后退 |
| `Left` / `Right` | Turn left / right (in menu: switch language) / 转向 (菜单中: 切换语言) |
| `OK` | Combat stage: shoot pistol / 战斗关: 手枪射击; other: dash / 其他: 冲刺; menu: confirm / 菜单: 确认 |
| `OK` (long) | MC mode: place held block / MC 模式: 放置方块 |
| `Back` (short) | Pause / 暂停; MC mode: cycle held block / MC: 切换方块 |
| `Back` (long) | Exit to Flipper home / 退出到桌面 |

## How to play / 玩法

1. Open the app, the main menu shows three modes and the current language.
2. Use **Up/Down** to pick a mode, **Left/Right** to toggle 中文 / English, **OK** to start.
3. Inside the maze, look at the **compass** at the top — it always points toward the exit.
4. Pick up keys (🔑) to open doors, torches (火把) to light the way, avoid traps and enemies.
5. Reach the glowing exit tile to clear the level / descend to the next floor.

## Building / 编译

```bash
pip install ufbt
ufbt update
ufbt build
```

The compiled `maze3d.fap` will be in `dist/`. Copy it to `apps/Misc/maze3d.fap` on the Flipper's SD card, or sideload via qFlipper.

## Source / 源码

Public repository: https://github.com/k20120509/maze3d

## License

MIT
