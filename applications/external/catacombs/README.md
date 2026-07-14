# Catacombs of the Damned!

![banner](package/banner.png)

**Catacombs of the Damned!** is a first-person shooter / dungeon crawler for the [Arduboy miniature game system](https://www.arduboy.com).
Explore **10 floors** of a procedurally generated dungeon, destroy monsters with magical fireballs, and collect as much loot as you can.

The game is partially inspired by the classic **Catacomb 3D** series.

## Features

![demo](package/demo.gif)

* Smooth real-time 3D gameplay
* Sound effects
* Procedurally generated levels
* Four different enemy types
* Plenty of treasures to collect

## About the Project

This repository is a **fork** of the original [Arduboy3D](https://github.com/jhhoward/Arduboy3D) project by James Howard.

The main goal of this fork is to **port the game from Arduboy to the Flipper Zero** handheld device.

## Screenshots

|                                             |                                             |
| ------------------------------------------- | ------------------------------------------- |
| ![screen2](package/screenshots/screen2.png) | ![screen3](package/screenshots/screen3.png) |
| ![screen4](package/screenshots/screen4.png) | ![screen5](package/screenshots/screen5.png) |
| ![screen6](package/screenshots/screen6.png) | ![screen1](package/screenshots/screen1.png) |

## Build Instructions

To compile the game, you will need the **Flipper Zero firmware source code** and the required build toolchain.

### 1. Clone the Firmware Source Code

Make sure you have enough free disk space, then clone the firmware repository with all submodules:

```bash
git clone --recursive https://github.com/flipperdevices/flipperzero-firmware.git
```

### 2. Prepare the Build Environment

Build the base firmware using the **Flipper Build Tool (fbt)** to verify that your environment is set up correctly:

```bash
./fbt
```

### 3. Install and Build the Game

1. Copy the game source code directory into the firmware’s user applications folder:

```
./flipperzero-firmware/applications_user/
```

2. Build the application (`.fap` file) using the following command:

```bash
./fbt fap_catacombs && mv build/f7-firmware-D/.extapps/catacombs.fap ./
```

After a successful build, the resulting application file can be found in the `build/` directory.
Copy `catacombs.fap` to your **Flipper Zero SD card** under:

```
apps/
```

## Community Discussion (Arduboy)

The development history and original discussion can be found in the Arduboy community [forum thread](https://community.arduboy.com/t/another-fps-style-3d-demo/6565) (English).

## Original Project

**James Howard**
[Arduboy3D](https://github.com/jhhoward/Arduboy3D)