# Maze 3D / 3D 迷宫




Endless Challenge and Tourist Roam are not fully developed, which may cause system exceptions.


Some features are not fully‑developed. Unknown risks may occur, please use with caution.

Some features are not fully‑developed. Unknown risks may occur, please use with caution.



A first-person 3D raycasting maze game for Flipper Zero, with three game modes, in-app language switching (Chinese / English), compass + minimap, save progress, and a small-footprint renderer tuned for the Flipper's 128×64 monochrome display.

一款为 Flipper Zero 打造的第一人称 3D 光线投射迷宫游戏,内含三种玩法、中英文随时切换、罗盘 + 小地图、进度自动保存,并针对 128×64 单色屏做了轻量化渲染优化。

## Features / 特性

- **3D raycasting engine** — distance shading, wall orientation brightness, dithered floor / ceiling.
- **Three game modes / 三种模式**
  - **Campaign / 闯关模式** — 1–10 纯迷宫, 11–20 加入道具/敌人/解谜, 之后自动生成, 难度递增.
  - **Endless Run / 无尽挑战** — 找到出口进入下一层, 自动保存层数, 每层贴图变化.
  - **Visitor / 游客漫游** — 无目标自由探索, 还有其他"游客"在迷宫里走动.
- **Compass + Minimap** — points to exit, shows layout around the player.
- **Bilingual UI / 双语界面** — switch Chinese / English any time from the menu (← / → key).
- **Auto save / 自动存档** — campaign progress and endless floor are persisted.
- **Performance tuned / 性能优化** — half-resolution raycasting, on-demand rendering (~8 Hz world tick), 8 KB stack.

## Controls / 操作

| Key / 按键 | Action / 动作 |
| --- | --- |
| `Up` / `Down` | Move forward / backward / 前进 / 后退 |
| `Left` / `Right` | Turn left / right (in menu: switch language) / 转向 (菜单中: 切换语言) |
| `OK` | Confirm / select / 确认 |
| `Back` (short) | Pause / 暂停 |
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
