# Getting Started with `f0-template`

Welcome on your path to creating Flipper Zero Application!  
Here are the few stuff to get started!

## Before you begin

### Change `AppID`

1. Open [application.fam](applciation.fam).
2. head to `appid="demo_app"` and change it to your own.
3. Open [./src/scenes/about/main.c](src/scenes/about/main.c).
4. Update `#include <demo_app_icons.h>` into `#include <{app_id}_icons.h>`.  
   If there is intelliSense error for missing variable, try rebuilding application.

## Developer Resources

Here are the resources for developing applications for Flipper Zero:

- [Flipper Zero Firmware Docs](https://developer.flipper.net/flipperzero/doxygen/)
  - [`struct` list](https://developer.flipper.net/flipperzero/doxygen/annotated.html) ([index](https://developer.flipper.net/flipperzero/doxygen/classes.html))
- [Flipper Zero code examples](https://github.com/m1ch3al/flipper-zero-dev-tutorial)
- [Lopaka, Graphics Editor for Embedded Devices](https://lopaka.app/)  
  Note that Flipper Zero has screen dimension of `128x64`.

## Directory Structure

`f0-template` consists of following directories

- `.github` - Reserved for GitHub Actions
- `icons` - For generating `Icon` (Sprites) for use in GUI elements
- `screenshots` - When uploaded to store, these are the Screenshots available from store page
- `src` - Where the C source is located
  - `src/main.c` - Where the main entrypoint is available.
  - `src/scenes` - Scene definitions for Scene Manager - Refer to [Using SceneManager](#using-scenemanager)
- `application.fam` - Flipper Zero's application manifest file - Refer to [this docs](https://developer.flipper.net/flipperzero/doxygen/app_manifests.html)
- `CHANGELOG.md` - When uploaded to store, these are the changelog available from store page
- `icon.png` - The Application Icon which is shown next to application name in Apps menu.

### `icons`

> [!WARNING]  
> alpha channel of PNG is **NOT** supported.
> Make sure that background is white!

This directory is for generating `Icon` struct which can be used for drawing sprites in flipper zero `Canvas`.  
It can be used as following:

1. Put your `PNG` files into icons.
2. Build (`ufbt build` or anything triggers build job) at least once before use.
3. Import `{app_id}_icons.h`.
4. On `draw_callback`, draw Icon by calling `canvas_draw_icon(canvas, loc_x, loc_y, &I_{png_filename});`
5. Done!

### Using `SceneManager`

This template implements `SceneManager`, A "Scene" based framework for programming flipper zero GUIs.

#### Creating Scenes

The Scenes are defined via using macro: `SCENE_ACTION`. By default, the `f0-template` provides two example scenes

1. Goto [`./src/scenes/list.h`](/src/scenes/list.h).
2. Add your own scene by using macro: `SCENE_ACTION`.
   (e.g. If you want to make new scene called `Store`, You should type `SCENE_ACTION(Store)`)
3. Implement `_on_enter`, `_on_event`, `_on_exit`, `_get_view`, `_alloc`, `_free` accordingly. Refer to [`./src/scenes/home/main.c`](/src/scenes/home/main.c) for more info.  
   (F0 doesn't make sure the precendence of `_on_exit` and `_free`. Please make sure those two are independent by checking each other's free'd state)
4. Add headers to export those functions.
5. Include your header in [`./src/scenes/import.h`](/src/scenes/import.h).
