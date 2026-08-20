# ArduboyLIB Documentation {#ardulib}

ArduboyLIB is a shared library for building Arduino/Arduboy-style games on Flipper Zero. It provides a common runtime (`arduboy_app`) and Arduboy-compatible API surface, so your game code can stay in classic Arduino style.

## Quick Start

### 1. Add ArduboyLIB to Your Project

**Option A: Git Submodule**
```bash
git submodule add https://github.com/apfxtech/ArduboyLIB.git lib
git submodule update --init --recursive
```

**Option B: Direct Clone**
```bash
git clone https://github.com/apfxtech/ArduboyLIB.git lib
```

### 2. Configure application.fam

```python
App(
    appid="your_app_id",
    name="Your Game",
    apptype=FlipperAppType.EXTERNAL,
    entry_point="arduboy_app",
    requires=["gui"],
    sources=["main.cpp", "lib/scr/*.cpp", "game/*.cpp"],
)
```

### 3. Create main.cpp

```cpp
#include "lib/Arduboy2.h"
#include "lib/runtime.h"

#define TARGET_FRAMERATE 60

void setup() {
    arduboy.setFrameRate(TARGET_FRAMERATE);
    // Your initialization code
}

void loop() {
    if(!arduboy.nextFrame()) return;
    arduboy.pollButtons();
    
    // Your game logic and drawing
    arduboy.clear();
    // ... draw here ...
    arduboy.display();
}
```

## Compile Flags

Define these flags at the top of your `main.cpp` **before** library includes.

- **`ARDULIB_RANDOM_LEGACY`** — Use legacy Arduboy-compatible pseudo-random generator instead of Flipper HAL RNG.

- **`ARDULIB_USE_ATM`** — Enable built-in ATM integration from runtime (init/idle/deinit), so game code can stay Arduino-style without engine hook functions.

- **`ARDULIB_USE_TONES`** — Enable `ArduboyTones` playback support. Without this flag, the `ArduboyTones` API stays available as an empty compatibility stub and does not start the tone worker.

- **`ARDULIB_USE_FX`** — Enable FX (external flash) support for games that use the FX chip for additional storage. This flag enables the `FX` class for reading game data and save files from external flash memory.

- **`ARDULIB_SWAP_AB`** — Swap A and B button flags. When defined, `A_BUTTON` becomes `0x20` and `B_BUTTON` becomes `0x10`. Useful for games that expect different button layouts.

- **`ARDULIB_USE_VIEW_PORT`** {#view_port_flag} — Switches the runtime from legacy framebuffer mode to ViewPort mode for screen rendering and button input.

    > **Important:** This flag requires manual local build. Auto-builders (GitHub Workflow, flipper.lab) will fail due to stricter code checks. This is a temporary measure until the new display API is published by firmware developers.

    **Modes Comparison:**

    | Feature | Legacy Mode (default) | ViewPort Mode |
    |---------|----------------------|---------------|
    | Build method | Any (auto/manual) | Manual build only |
    | Input stability | Less stable | More reliable |
    | Display rendering | Framebuffer callback | ViewPort callback |
    | API used | Public API | Internal API |

    **When to use:**
    - Use **legacy mode** (default) for compatibility with auto-builders (GitHub Workflow, flipper.lab)
    - Use **ViewPort mode** for more stable input handling when building locally

## API Overview

### Core Functions

- `arduboy.begin()` - Initialize the library
- `arduboy.setFrameRate(fps)` - Set target frame rate
- `arduboy.nextFrame()` - Frame pacing control
- `arduboy.pollButtons()` - Update button state
- `arduboy.display()` - Render framebuffer to screen

### Button Input

```cpp
arduboy.pressed(UP_BUTTON)      // Currently held
arduboy.justPressed(A_BUTTON)   // Just pressed this frame
arduboy.justReleased(B_BUTTON)  // Just released this frame
```

### Drawing

```cpp
arduboy.clear()                          // Clear buffer
arduboy.drawPixel(x, y, color)           // Draw pixel
arduboy.drawLine(x0, y0, x1, y1, color)  // Draw line
arduboy.fillRect(x, y, w, h, color)      // Filled rectangle
arduboy.drawBitmap(x, y, bitmap, w, h)   // Draw bitmap
```

## Projects Using ArduboyLIB

- [Arduventure](https://github.com/apfxtech/FlipperArduventure)
- [Catacombs of the Damned](https://github.com/apfxtech/FlipperCatacombs)
- [Wolfenstein](https://github.com/apfxtech/FlipperWolfenstein)
- [Mystic Balloon](https://github.com/apfxtech/FlipperMysticBalloon)
- [ArduGolf](https://github.com/apfxtech/FlipperGolf)
- [ArduDriver](https://github.com/apfxtech/FlipperDrivin)
- [MicroTD](https://github.com/apfxtech/FlipperTowerDefense.git)
- [MicroCity](https://github.com/apfxtech/FlipperMicroCity.git)
- [Minecraft](https://github.com/apfxtech/FlipperMinecraft.git)

## Credits

**Original:** MrBlinky - [Arduboy homemade package](https://github.com/MrBlinky/Arduboy-homemade-package)

**License:** CC0 1.0 Universal
