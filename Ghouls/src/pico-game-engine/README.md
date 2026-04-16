## pico-game-engine
A lightweight C++ game engine for embedded devices with LCD displays.

---

### About
Pico Game Engine provides a complete framework for building games on microcontrollers such as the Raspberry Pi Pico or any embedded platform with an LCD and input controls. It supports both 2D sprite-based rendering and a software 3D rasterizer with filled triangles, back-face culling, and depth sorting — all without requiring a GPU or standard library allocator.

---

### Connect Online
- Discord: https://discord.gg/5aN9qwkEc6
- YouTube: https://www.youtube.com/@jblanked
- Instagram: https://www.instagram.com/jblanked
- Other: https://www.jblanked.com/social/

---

### Features
- 2D sprite rendering with 8-bit palette and 16-bit RGB565 bitmap support
- Software 3D rasterizer with perspective projection, back-face culling, and painter's-algorithm depth sorting
- Pre-built 3D shapes: humanoid, tree, house, pillar, cube, cylinder, sphere, wall, and triangular prism
- First-person and third-person camera modes
- Entity-component architecture with typed states, RPG stats, and AABB collision detection
- Multi-level support with level switching at runtime
- Blocking and non-blocking (async tick) game loop modes for bare-metal or RTOS environments
- Fully macro-driven platform abstraction — wire up any LCD driver without modifying engine code
- Optional file-backed image loading via a configurable storage macro
- No STL dependency; lightweight callback system without `std::function`

---

### Architecture

```
GameEngine            - runs the game loop (blocking or single-tick async)
  └── Game            - top-level state: levels, camera, input, colors, scroll offset
        └── Level     - entity container with update, collision, and render loops
              └── Entity  - game object with position, sprites, callbacks, and stats
                    ├── Image      - 2D bitmap sprite (in-memory or file-backed)
                    └── Sprite3D   - 3D mesh of up to 64 Triangle3D instances

Draw                  - thin C++ wrapper over all LCD macros
Camera                - view parameters for first-person or third-person perspective
Vector                - 3D math primitive (x, y, z) with rotation and scale helpers
Triangle3D            - single colored triangle with depth and wireframe support
callback.hpp          - lightweight function-pointer + void* callback structs
engine_config.hpp     - all platform-specific macros (LCD, memory, font, storage)
```

---

### Configuration

All platform wiring is done via `#define` macros in `engine_config.hpp`. No engine source files need to be modified.

| Category | Key Macros |
|----------|------------|
| Memory   | `ENGINE_MEM_MALLOC`, `ENGINE_MEM_FREE`, `ENGINE_MEM_NEW`, `ENGINE_MEM_DELETE` |
| Delay    | `ENGINE_DELAY_INCLUDE`, `ENGINE_DELAY_MS(ms)` (required) |
| Font     | `ENGINE_FONT_INCLUDE`, `ENGINE_FONT_SIZE`, `ENGINE_FONT_DEFAULT` |
| LCD      | `ENGINE_LCD_INIT`, `ENGINE_LCD_CLEAR`, `ENGINE_LCD_PIXEL`, `ENGINE_LCD_LINE`, `ENGINE_LCD_TRIANGLE`, `ENGINE_LCD_FILL_TRIANGLE`, `ENGINE_LCD_BLIT`, `ENGINE_LCD_BLIT_16BIT`, `ENGINE_LCD_TEXT`, `ENGINE_LCD_SWAP`, and more |
| Storage  | `ENGINE_STORAGE_INCLUDE`, `ENGINE_STORAGE_READ` (optional, for file-backed images) |

`ENGINE_LCD_SWAP` is optional and enables double-buffered rendering when supported by the display driver.

---

### Core Classes

#### `GameEngine`
The top-level runner. Accepts a `Game*` and a target FPS.

| Method | Description |
|--------|-------------|
| `run()` | Blocking game loop: start, update, render, delay, stop |
| `runAsync(shouldDelay)` | Single tick for use with RTOS or cooperative schedulers |
| `updateGameInput(uint8_t)` | Injects button state into the game |
| `stop()` | Stops the game, clears the screen, frees memory |

#### `Game`
Top-level state. Holds up to 10 levels, a `Draw` instance, a `Camera`, the current input value, scroll offset (`pos`), and world size.

| Method | Description |
|--------|-------------|
| `level_add(level)` | Registers a level |
| `level_switch(name)` | Stops the current level, starts the named one |
| `setCamera(Camera)` | Copies camera parameters into the engine |
| `clamp(value, min, max)` | Utility to bound a value |

#### `Level`
A scene that owns a dynamic array of entities. Handles the full per-frame pipeline: update all entities, run AABB collision detection between all active pairs, then render.

| Method | Description |
|--------|-------------|
| `entity_add(entity)` | Adds an entity and calls its `start` callback |
| `entity_remove(entity)` | Calls `stop`, removes from array, and frees non-player entities |
| `clear()` | Stops and removes all non-player entities |
| `is_collision(a, b)` | Returns true if two entities' bounding boxes overlap |
| `collision_list(entity, count)` | Returns all entities currently colliding with the given entity |

#### `Entity`
A game object with position, size, 2D sprites, an optional 3D mesh, callbacks, and RPG stats.

**Types**: `ENTITY_PLAYER`, `ENTITY_ENEMY`, `ENTITY_NPC`, `ENTITY_ICON`, `ENTITY_3D_SPRITE`

**States**: `ENTITY_IDLE`, `ENTITY_MOVING`, `ENTITY_ATTACKING`, `ENTITY_ATTACKED`, `ENTITY_DEAD`

**Callbacks** (all optional):

| Callback | Signature |
|----------|-----------|
| Start / Stop | `void(Entity*, Game*, void*)` |
| Update | `void(Entity*, Game*, void*)` |
| Render | `void(Entity*, Draw*, Game*, void*)` |
| Collision | `void(Entity*, Entity*, Game*, void*)` |

Built-in stats available on every entity: `health`, `max_health`, `strength`, `speed`, `level`, `xp`, `radius`, `health_regen`, attack and move timers.

#### `Sprite3D`
A 3D mesh of up to 64 triangles. Pre-built shapes are available via initialization methods.

| Shape | Method |
|-------|--------|
| Humanoid | `initializeAsHumanoid(position, height, color)` |
| Tree | `initializeAsTree(position, height, color)` |
| House | `initializeAsHouse(position, width, height, color)` |
| Pillar | `initializeAsPillar(position, height, radius, color)` |
| Custom | Build manually with `addTriangle(...)` |

Colors are RGB565. The engine applies a shading factor to different faces automatically for built-in shapes.

#### `Camera`
Controls the 3D view. Supports `CAMERA_FIRST_PERSON` and `CAMERA_THIRD_PERSON` perspectives.

| Field | Default | Description |
|-------|---------|-------------|
| `position` | `(0,0,0)` | World-space camera location |
| `direction` | `(1,0,0)` | Look direction |
| `plane` | `(0,0.66,0)` | Camera plane controlling field of view |
| `height` | `1.6` | Camera height above ground |
| `distance` | `2.0` | Follow distance for third-person mode |

#### `Draw`
A C++ wrapper over all LCD macros. All drawing methods accept either a `Vector` or raw `int16_t` coordinates.

Available operations: `fillScreen`, `pixel`, `line`, `circle`, `fillCircle`, `rectangle`, `fillRectangle`, `triangle`, `fillTriangle`, `image` (8-bit and 16-bit), `text`, `swap`.

#### `Image`
A 2D bitmap sprite backed by an in-memory pointer or a file path. Supports 8-bit palette and 16-bit RGB565 formats.

#### `Vector`
A 3D math type (`float x, y, z`) with operators (`+`, `-`, `*`, `/`) and methods for rotation around Y, per-axis scaling, and translation.

---

### Usage Example

**Step 1 — Create your config file (one-time setup)**

```sh
cp engine_config.hpp.example engine_config.hpp
```

`engine_config.hpp` is listed in `.gitignore`, so `git pull` will never overwrite your changes.

Open `engine_config.hpp` and uncomment/fill in the macros for your platform.

The memory macros default to standard `new`/`delete`/`malloc`/`free` and only need changing on custom allocator platforms.

**Step 2 onwards — Game code**

```cpp
// 1. Define entity callbacks
void player_update(Entity *self, Game *game) {
    if (game->input == BUTTON_LEFT)
        self->position_set(self->position.x - self->speed, self->position.y);
    if (game->input == BUTTON_RIGHT)
        self->position_set(self->position.x + self->speed, self->position.y);
    game->clamp(self->position.x, 0, game->size.x);
}

void player_collision(Entity *self, Entity *other, Game *game) {
    if (other->type == ENTITY_ENEMY)
        self->health -= other->strength;
}

// 2. Create sprites and entities
Image *sprite = new Image(Vector(16, 16), false, my_bitmap_data);
Entity *player = new Entity(
    "player", ENTITY_PLAYER, Vector(64, 32), Vector(16, 16), sprite,
    nullptr, nullptr,
    {},                            // start callback
    {},                            // stop callback
    {player_update, nullptr},      // update callback
    {},                            // render callback
    {player_collision, nullptr}    // collision callback
);
player->is_player = true;
player->speed     = 1.5f;
player->health    = player->max_health = 100.0f;

// 3. Create the game, level, and add entities
Draw  *draw  = new Draw();
Game  *game  = new Game("MyGame", Vector(256, 128), draw, 0x0000, 0xFFFF);
Level *level = new Level("level1", Vector(256, 128), game);
level->entity_add(player);
game->level_add(level);

// 4a. Blocking loop (bare-metal)
GameEngine engine(game, 30.0f);
engine.run();

// 4b. Non-blocking loop (RTOS / cooperative scheduler)
GameEngine engine(game, 30.0f);
while (true) {
    engine.updateGameInput(read_buttons());
    engine.runAsync();
}
```

#### Adding a 3D Entity

```cpp
Entity *npc = new Entity(
    "soldier", ENTITY_3D_SPRITE, Vector(100, 100), Vector(1, 2),
    nullptr, nullptr, nullptr,
    {}, {}, {npc_update, nullptr}, {}, {},
    false, SPRITE_3D_HUMANOID, 0xF800  // red humanoid
);
npc->set3DSpriteRotation(1.57f);
level->entity_add(npc);
```