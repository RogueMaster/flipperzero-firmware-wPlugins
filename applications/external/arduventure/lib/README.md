# ArduboyLIB

ArduboyLIB is a shared library for building Arduino/Arduboy-style games on Flipper Zero.
It provides a common runtime (`arduboy_app`) and Arduboy-compatible API surface, so your game code can stay in classic Arduino style with:

- `void setup()`
- `void loop()`

The runtime is implemented in `scr/runtime.cpp` and uses Flipper Zero `ViewPort` + GUI input callbacks for rendering/input delivery.

## How It Works

- Your app entry point in `.fam` is `entry_point="arduboy_app"`.
- The runtime calls your `setup()` once, then calls `loop()` continuously.
- Frame pacing can be controlled in your game with:

```cpp
arduboy.setFrameRate(TARGET_FRAMERATE);
```

## Compile Flags

**required refactor**

Define flags at the very top of your `main.cpp` before library includes.

### `ARDULIB_RANDOM_LEGACY`

Use legacy Arduboy-compatible pseudo-random generator instead of Flipper HAL RNG.

```cpp
#define ARDULIB_RANDOM_LEGACY
```

### `ARDULIB_USE_ATM`

Enable built-in ATM integration from runtime (init/idle/deinit), so game code can stay Arduino-style
without engine hook functions.

```cpp
#define ARDULIB_USE_ATM
```

### `ARDULIB_USE_TONES`

Enable `ArduboyTones` playback support. Without this flag the `ArduboyTones` API stays available as
an empty compatibility stub and does not start the tone worker.

```cpp
#define ARDULIB_USE_TONES
```

### `ARDULIB_SWAP_AB`

Swap A and B button flags. When defined, `A_BUTTON` becomes `0x20` and `B_BUTTON` becomes `0x10`.
This is useful for games that expect different button layouts (e.g., some Arduboy games use B as "accept" instead of A).

```cpp
#define ARDULIB_SWAP_AB
```

## Add ArduboyLIB To Your Project

Use one of the following approaches inside your app folder.

### Option 1: Add as Git Submodule

```bash
git submodule add https://github.com/apfxtech/ArduboyLIB.git lib
git submodule update --init --recursive
```

If you already have a submodule and want updates:

```bash
git submodule update --remote --merge
```

### Option 2: Clone Directly Into `lib`

```bash
git clone https://github.com/apfxtech/ArduboyLIB.git lib
```

## Required `.fam` Setup


```python
sources=["main.cpp", "lib/scr/*.cpp", "game/*.cpp"]
```

This means:

- `main.cpp` is your game entry source (`setup/loop`).
- `lib/scr/*.cpp` pulls ArduboyLIB runtime implementation.
- `game/*.cpp` contains game logic.

Use a manifest pattern like this:

```python
App(
    appid="your_app_id",
    name="Your App",
    apptype=FlipperAppType.EXTERNAL,
    entry_point="arduboy_app",
    requires=["gui"],
    sources=["main.cpp", "lib/scr/*.cpp", "game/*.cpp"],
)
```

## Minimal `main.cpp` Shape

```cpp
#include "lib/Arduboy2.h"
#include "lib/runtime.h"

#define TARGET_FRAMERATE 60

void setup() {
    arduboy.setFrameRate(TARGET_FRAMERATE);
}

void loop() {
    if(!arduboy.nextFrame()) return;
    arduboy.pollButtons();
    // draw/update here
    arduboy.display();
}
```

## Project using ArduboyLIB
23.02.2026: all ported but not commited
- [Arduventure](https://github.com/apfxtech/FlipperArduventure)
- [Catacombs of the Damned](https://github.com/apfxtech/FlipperCatacombs)
- [Wolfenstein](https://github.com/apfxtech/FlipperWolfenstein)
- [Mystic Balloon](https://github.com/apfxtech/FlipperMysticBalloon)
- [ArduGolf](https://github.com/apfxtech/FlipperGolf)
- [ArduDriver](https://github.com/apfxtech/FlipperDrivin)
- [MicroTD](https://github.com/apfxtech/FlipperTowerDefense.git) - (private)
- [MicroCity](https://github.com/apfxtech/FlipperMicroCity.git) - (private)
- [Minecraft](https://github.com/apfxtech/FlipperMinecraft.git) - (private)

## Original
**MrBlinky**
[Arduboy homemade package](https://github.com/MrBlinky/Arduboy-homemade-package.git)

Creative Commons Legal Code
(CC0 1.0 Universal)

## Contributors and authors
<a href="https://github.com/apfxtech/ArduboyLIB/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=apfxtech/ArduboyLIB" />
  <img src="https://contrib.rocks/image?repo=MrBlinky/Arduboy-homemade-package" />
</a>
