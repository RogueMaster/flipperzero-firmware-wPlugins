[![ufbt Build and Release action tag](https://github.com/squee72564/F0_Minesweeper_Fap/actions/workflows/build.yml/badge.svg)](https://github.com/squee72564/F0_Minesweeper_Fap/actions/workflows/build.yml)

# Minesweeper Implementation for the Flipper Zero.
Hello!

![Mine Sweeper Example Gameplay Gif](https://github.com/squee72564/F0_Minesweeper_Fap/blob/main/img/github_images/MineSweeperGameplay.gif)
## Added features unique to this implementation:
-	Enable board verifier for board generation to ensure unambiguous boards!
-	Set board width and height
-	Set difficulty
-	A number of different button presses as shortcuts for game actions (see How To Play) 

## Installation
The app is also now available for download on the [Flipper Lab](https://lab.flipper.net/apps/minesweeper_redux) website for easy access.

If you want to manually build and install follow the steps below: 
* Clone this repo to your computer:
	`git clone https://github.com/squee72564/F0_Minesweeper_Fap.git`

* I would recommend using the [uFBT (micro Flipper Build Tool)](https://github.com/flipperdevices/flipperzero-ufbt) to build the application.
	- It allows you to compile into .fap files on the SD card as opposed to re-flashing the entire firmware with the application.
	- With uFBT installed you can go into the root directory of this project after cloning and type
	`ufbt launch` to compile and launch the app on your Flipper Zero

## How To Play
- Up/Down/Left/Right Buttons to move around
	- These buttons can be held down to keep moving in a direction
- Center OK Button to attempt opening up a tile
	- Press OK on a tile to open it up
	- Hold OK on a cleared space with a number to clear all surrounding tiles (correct number of flags must be set around it)
- Hold Back Button on a tile to toggle marking it with a flag
- Hold Back Button on a cleared space to jump to one of the closest tiles (this can help find last tiles on a larger board)
- Press Back Button to access the settings menu where you can do the following:
	- Change board width
	- Change board height
	- Change difficulty
	- Ensure Solvable : Ensures a board can be unambiguously solved.
	- Enable Feedback : This option toggles the haptic and sound feedback for the game.
    - Enable Wrap : This option toggles wrapping movement to the other side of the board when you move across the edge boundary.

## IMPORTANT NOTICE:
The way I set the board up leaves the corners as safe starting positions!

In addition to this, with the "Ensure Solvable" option set to true, the board will always be solvable from 0,0! Without "Ensure Solvable" enabled in the settings there is no guarantee that the game will be solvable without any guesses.

## Application Structure
The following is the current project layout:
- **[F0_Minesweeper_Fap/](https://github.com/squee72564/F0_Minesweeper_Fap)**
	- [.github/workflows/](https://github.com/squee72564/F0_Minesweeper_Fap/tree/main/.github/workflows)
		- CI/build workflow definitions.
	- [assets/](https://github.com/squee72564/F0_Minesweeper_Fap/tree/main/assets)
		- Game image assets (tile sprites, icon, and start screen images).
	- [docs/](https://github.com/squee72564/F0_Minesweeper_Fap/tree/main/docs)
		- Supporting documentation such as the changelog.
	- [engine/](https://github.com/squee72564/F0_Minesweeper_Fap/tree/main/engine)
		- Core game logic, board generation, and solver helpers.
	- [helpers/](https://github.com/squee72564/F0_Minesweeper_Fap/tree/main/helpers)
		- Flipper hardware/service helpers (haptics, LED, speaker, storage, and config).
	- [scenes/](https://github.com/squee72564/F0_Minesweeper_Fap/tree/main/scenes)
		- Scene registry/config (`minesweeper_scene*.{c,h}`) and per-scene enter/event/exit handlers.
		- See `scenes/README.md` for scene wiring details.
	- [views/](https://github.com/squee72564/F0_Minesweeper_Fap/tree/main/views)
		- Custom view implementations for gameplay, generating/loading, and start screen rendering.
	- [img/](https://github.com/squee72564/F0_Minesweeper_Fap/tree/main/img)
		- README/demo images used in GitHub documentation.
	- [dist/](https://github.com/squee72564/F0_Minesweeper_Fap/tree/main/dist)
		- Built `.fap` artifacts output by local builds.
	- `minesweeper.c` / `minesweeper.h`
		- Main application entry and top-level app state/controller wiring.
	- `application.fam`
		- [Flipper application manifest](https://github.com/flipperdevices/flipperzero-firmware/blob/dev/documentation/AppManifests.md#application-definition).

The structure is inspired by [leedave's](https://github.com/leedave/flipper-zero-fap-boilerplate) Flipper Zero FAP boilerplate and adapted for this game's architecture.
