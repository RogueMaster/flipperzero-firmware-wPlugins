# VGM Game Engine
This project is not affiliated with Flipper Devices and utilizes the [PicoDVI](https://github.com/Wren6991/PicoDVI) and [pico-game-engine](https://github.com/jblanked/pico-game-engine) libraries.

## Getting Started

### Flipper Zero App
The Flipper Zero app allows you to use your d-pad as input in games created with the engine. The source code is located in the `App` folder. Use [ufbt](https://github.com/flipperdevices/flipperzero-ufbt) to compile the source code into a `.fap` file or install it directly on your Flipper Zero from https://lab.flipper.net/apps/vgm_game_remote

### Arduino Library
The Arduino library is used to create and manage games. Copy the `vgm_game_engine` folder from the `Arduino` directory into your Arduino `libraries` folder (typically located at `User/Documents/Arduino/libraries`). To use the library, add `#include "VGMGameEngine.h"` at the top of your `.ino` file. Review the provided examples to understand how to use the library.

### Compiled Example
There are two installation methods; choose the one that is most convenient for you:

**Method 1**
1. Install the Video Game Module Tool app on your Flipper Zero from the Apps catalog: [Video Game Module Tool](https://lab.flipper.net/apps/video_game_module_tool).
2. Download one of the examples from the `Arduino` directory:
- Doom: `Doom-VGM-Engine.uf2`
- Doom (8-bit): `Doom_8bit-VGM-Engine.uf2`
- FlipWorld: `FlipWorld-VGM-Engine.uf2`
- Air Labyrinth: `AirLabyrinth-VGM-Engine.uf2`
- T-Rex Runner: `T-Rex-Runner-VGM-Engine.uf2`
- FlappyBird: `FlappyBird-VGM-Engine.uf2`
- Flight Assault: `FlightAssault-VGM-Engine.uf2`
- Pong: `Pong-VGM-Engine.uf2`
- Tetris: `Tetris-VGM-Engine.uf2`
- Arkanoid: `Arkanoid-VGM-Engine.uf2`
- FuriousBirds: `FuriousBirds-VGM-Engine.uf2`
- Hawaii (Animation): `Hawaii-Pico-Engine.uf2`
3. Disconnect your Video Game Module from your Flipper and connect your Flipper to your computer.
4. Open qFlipper.
5. Navigate to the `SD Card/apps_data/vgm/` directory. If the folder doesn’t exist, create it. Inside that folder, create a folder named `Engine`.
6. Drag the file you downloaded earlier into the `Engine` directory.
7. Disconnect your Flipper from your computer, then turn it off.
8. Connect your Video Game Module to your Flipper, then turn your Flipper on.
9. Open the Video Game Module Tool app on your Flipper. It should be located in the `Apps -> Tools` folder from the main menu.
10. In the Video Game Module Tool app, select `Install Firmware from File`, then choose `apps_data`.
11. Scroll down and click on the `vgm` folder, then the `Engine` folder, and finally select the `.uf2` file.
12. The app will begin flashing the firmware to your Video Game Module. Wait until the process is complete.

**Method 2**
1. Download one of the examples from the `Arduino` directory:
- Doom: `Doom-VGM-Engine.uf2`
- Doom (8-bit): `Doom_8bit-VGM-Engine.uf2`
- FlipWorld: `FlipWorld-VGM-Engine.uf2`
- Air Labyrinth: `AirLabyrinth-VGM-Engine.uf2`
- T-Rex Runner: `T-Rex-Runner-VGM-Engine.uf2`
- FlappyBird: `FlappyBird-VGM-Engine.uf2`
- Flight Assault: `FlightAssault-VGM-Engine.uf2`
- Pong: `Pong-VGM-Engine.uf2`
- Tetris: `Tetris-VGM-Engine.uf2`
- Arkanoid: `Arkanoid-VGM-Engine.uf2`
- FuriousBirds: `FuriousBirds-VGM-Engine.uf2`
- Hawaii (Animation): `Hawaii-Pico-Engine.uf2`
2. Press and hold the `BOOT` button on your Video Game Module for 2 seconds.
3. While holding the `BOOT` button, connect the Video Game Module to your computer using a USB cable.
4. Drag and drop the downloaded file onto the device. It will automatically reboot and begin running the example.

## Usage
You need to flash your Video Game Module with an example (or your own custom script) and install the app on your Flipper Zero. The Video Game Module handles video output and manages the game engine, while the Flipper Zero acts as a controller (almost the opposite of its previous use). Developers can create their own games or recreate their Flipper games with the advantages of increased memory, colorful graphics, and video output to their TV or monitor.
