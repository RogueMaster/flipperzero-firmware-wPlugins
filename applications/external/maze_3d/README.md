# Maze 3D

A minimal first-person 3D raycasting maze game for Flipper Zero.

Walk through randomly generated mazes and find the exit. Pure English interface.

## Features

- 3D raycasting engine with textured walls and distance shading
- Procedurally generated mazes (recursive backtracker + loops, BFS-placed exit)
- Smooth grid movement and turning
- On-screen exit direction arrow
- Level progression: maze grows larger each floor
- Bilingual UI toggle (English / Chinese), English by default
- Sound effects for menu, steps, and level clear

## Controls

### Menu
- Up / Down - select item
- Left / Right - switch language
- OK - confirm
- Long Back - exit app

### In-game
- Up - move forward
- Down - move backward
- Left / Right - turn
- Long OK - toggle HUD
- Back - return to menu
- Long Back - return to menu

## Building

```
ufbt
```

The compiled `maze3d.fap` is placed in `dist/`.

## Install

Copy `dist/maze3d.fap` to your Flipper Zero SD card under `apps/Games/`.

## License

MIT
