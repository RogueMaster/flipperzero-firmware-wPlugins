# Flipper Zero 1D Pacman

This repository contains an implementation of 1D Pacman using the [Flipper Zero Game Engine](https://github.com/flipperdevices/flipperzero-game-engine) as boilerplate.

## Cloning source code

Make sure you clone the source code with submodules:
```
git clone https://github.com/flipperdevices/flipperzero-game-engine-example.git --recurse-submodules
```

We have modified the game engine locally - the game engine referenced currently is out of date.

## Building

Build an app using [micro Flipper Build Tool](https://pypi.org/project/ufbt/):
```
ufbt
```
You can now run the application (actually, the build step is not needed, it will be built and then launched):
```
ufbt launch
```
