# Configuration

This project uses two configuration files: `src/engine_config.hpp` for engine-level settings and `src/config.hpp` for game-level settings.

---

## src/engine_config.hpp

This file configures the underlying engine. Edit this file to adapt the engine to your platform.

### Triangles

`ENGINE_MAX_TRIANGLES_PER_SPRITE` sets the maximum number of triangles per 3D sprite. Decrease it to save memory.

### Logging

Uncomment and set `ENGINE_LOG_INCLUDE` to your platform's log header, then update `ENGINE_LOG_INFO` to call the appropriate log function.

```cpp
#define ENGINE_LOG_INCLUDE "furi.h"
#define ENGINE_LOG_INFO(...) FURI_LOG_I("Ghouls", __VA_ARGS__)
```

### Memory

Set the include and function macros to your platform's memory management. The defaults use the standard library `malloc`/`free` and C++ `new`/`delete`.

```cpp
#define ENGINE_MEM_INCLUDE "stdlib.h"
#define ENGINE_MEM_NEW new
#define ENGINE_MEM_DELETE delete
#define ENGINE_MEM_MALLOC malloc
#define ENGINE_MEM_FREE free
```

### Delay

Uncomment and set `ENGINE_DELAY_INCLUDE` and `ENGINE_DELAY_MS` if your platform requires a delay function.

```cpp
#define ENGINE_DELAY_INCLUDE "furi.h"
#define ENGINE_DELAY_MS(ms) furi_delay_ms(ms)
```

### Font

Uncomment and set the font macros to use platform-specific text rendering.

```cpp
#define ENGINE_FONT_INCLUDE "font/font.h"
#define ENGINE_FONT_SIZE FontSize
#define ENGINE_FONT_DEFAULT FONT_SIZE_SMALL
```

### LCD

Uncomment the LCD macros to connect the engine's drawing calls to your display driver. Each macro maps to a specific drawing operation — see the comments in the file for the expected function signature.

### Storage (optional)

Uncomment `ENGINE_STORAGE_INCLUDE` and `ENGINE_STORAGE_READ` to enable file reading from your platform's storage system.

---

## src/config.hpp

This file configures game-specific features. Edit this file to enable or disable game systems.

### Wireframe

`WIREFRAME_ENABLED` toggles wireframe rendering on or off.

```cpp
#define WIREFRAME_ENABLED true
```

### Time

These macros must be defined for the game to track time. Set them to your platform's timer header and millisecond expression.

- `TIME_INCLUDE` — the header file that provides your timer function
- `TIME_MILLIS` — an expression that returns the current time in milliseconds as an integer

```cpp
#define TIME_INCLUDE "furi.h"
#define TIME_MILLIS furi_get_tick() * 10
```

### Input / Buttons

The `INPUT_KEY_*` macros map logical button names to integer values. Change these values to match your platform's button indices.

```cpp
#define INPUT_KEY_UP     0
#define INPUT_KEY_DOWN   1
#define INPUT_KEY_RIGHT  2
#define INPUT_KEY_LEFT   3
#define INPUT_KEY_CENTER 4
#define INPUT_KEY_BACK   5
```

### HTTP

These macros must be defined to enable network requests and WebSocket support. Set them to the matching functions provided by your platform's HTTP library.

The status constants below are used internally by the game and do not need to be changed:
- `HTTP_INACTIVE` — no HTTP session is active
- `HTTP_IDLE` — session is open and waiting
- `HTTP_RECEIVING` — a response is being received
- `HTTP_SENDING` — a request is being sent
- `HTTP_ISSUE` — an error occurred

The following macros must be uncommented and set:
- `HTTP_INCLUDE` — the header file for your HTTP library
- `HTTP_REQUEST_IS_FINISHED` — function that returns `bool`, true when the last request has completed
- `HTTP_SEND_REQUEST` — function that sends an HTTP request; takes `(const char *url, const char *method, const char *headers, const char *payload)` and returns `bool`
- `HTTP_WEBSOCKET_IS_CONNECTED` — function that returns `bool`, true when a WebSocket connection is open
- `HTTP_WEBSOCKET_SEND` — function that sends a WebSocket message; takes `(const char *message)` and returns `bool`
- `HTTP_WEBSOCKET_START` — function that opens a WebSocket connection; takes `(const char *url, uint16_t port)` and returns `bool`
- `HTTP_WEBSOCKET_STOP` — function that closes the active WebSocket connection; returns `bool`
- `HTTP_GET_RESPONSE` — function that copies the last HTTP response into a buffer; takes `(char *buffer, size_t buffer_size)` and returns `bool`
- `HTTP_GET_WEBSOCKET_RESPONSE` — function that copies the last WebSocket message into a buffer; takes `(char *buffer, size_t buffer_size)` and returns `bool`

### JSON 

These macros must be defined for the game to parse server responses. Set them to the matching functions provided by your platform's JSON library.

- `JSON_INCLUDE` — the header file for your JSON library
- `JSON_GET_VALUE` — function that retrieves a value by key from a JSON string; takes `(const char *key, const char *json_str)` and returns a `char*` that the caller must free
- `JSON_GET_ARRAY_VALUE` — function that retrieves a value by key and index from a JSON array; takes `(const char *key, int index, const char *json_str)` and returns a `char*` that the caller must free

### Sound (optional)

Uncomment `SOUND_INCLUDE` and the `SOUND_PLAY_*` macros to enable audio playback on your platform.
