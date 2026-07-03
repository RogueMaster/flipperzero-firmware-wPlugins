# Flipper Zero — App Architecture Reference

> Sources:
> - https://github.com/mfulz/Flipper-Plugin-Tutorial
> - https://github.com/m1ch3al/flipper-zero-dev-tutorial
> - https://instantiator.dev/post/flipper-zero-app-tutorial-01/

## Entry Point

Every FAP has a single entry point matching the name in `application.fam`:

```c
#include <furi.h>
#include <gui/gui.h>

int32_t claude_remote_app(void* p) {
    UNUSED(p);
    // init, loop, cleanup
    return 0;
}
```

- Signature: `int32_t function_name(void* p)`
- `p` is unused for most apps. Always `UNUSED(p)`.
- Return 0 on clean exit.

## Event-Driven Architecture

Apps follow an event loop pattern using FuriMessageQueue:

```
┌─────────────┐     ┌──────────────┐     ┌────────────┐
│ Input HW    │────▸│ Input CB     │────▸│ Message    │
│ (buttons)   │     │ (enqueue)    │     │ Queue      │
└─────────────┘     └──────────────┘     └─────┬──────┘
                                               │
                    ┌──────────────┐     ┌──────▼─────┐
                    │ Draw CB      │◂────│ Main Loop  │
                    │ (render)     │     │ (dequeue)  │
                    └──────────────┘     └────────────┘
```

## Core Components

### FuriMessageQueue

Thread-safe event queue between callbacks and main loop.

```c
// Allocate (8 slots, each holding an InputEvent)
FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(InputEvent));

// In main loop — blocks up to 100ms
InputEvent event;
FuriStatus status = furi_message_queue_get(queue, &event, 100);
if(status == FuriStatusOk) {
    // handle event
}

// Cleanup
furi_message_queue_free(queue);
```

### FuriMutex (current SDK)

Thread-safe state access. **Do NOT use deprecated `ValueMutex`.**

```c
FuriMutex* mutex = furi_mutex_alloc(FuriMutexTypeNormal);

// Acquire
furi_mutex_acquire(mutex, FuriWaitForever);
// ... modify state ...
furi_mutex_release(mutex);

// Cleanup
furi_mutex_free(mutex);
```

### App State

```c
typedef struct {
    bool is_flipped;          // orientation
    bool bt_connected;        // BLE HID status
    uint8_t current_mode;     // 0=home, 1=remote, 2=manual
    // manual reader state
    uint8_t chapter_index;
    int16_t scroll_offset;
} ClaudeRemoteState;
```

Allocate with `malloc(sizeof(ClaudeRemoteState))`, free on exit.

## ViewPort + GUI Registration

```c
// Allocate viewport
ViewPort* view_port = view_port_alloc();
view_port_draw_callback_set(view_port, draw_callback, app_state);
view_port_input_callback_set(view_port, input_callback, event_queue);

// Register with GUI
Gui* gui = furi_record_open(RECORD_GUI);
gui_add_view_port(gui, view_port, GuiLayerFullscreen);

// ... event loop ...

// Cleanup (reverse order)
gui_remove_view_port(gui, view_port);
furi_record_close(RECORD_GUI);
view_port_free(view_port);
```

## Callbacks

### Draw Callback

Called on GUI thread whenever `view_port_update()` is called.

```c
static void draw_callback(Canvas* canvas, void* ctx) {
    ClaudeRemoteState* state = (ClaudeRemoteState*)ctx;
    // Must be fast, no blocking, no allocation
    furi_mutex_acquire(state->mutex, FuriWaitForever);

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, "Claude Remote");

    furi_mutex_release(state->mutex);
}
```

**Rules:**
- No dynamic allocation.
- No blocking I/O.
- Acquire mutex briefly, read state, release.

### Input Callback

Called on input thread when any button event occurs.

```c
static void input_callback(InputEvent* input_event, void* ctx) {
    FuriMessageQueue* queue = (FuriMessageQueue*)ctx;
    furi_message_queue_put(queue, input_event, FuriWaitForever);
}
```

**Rules:**
- Only enqueue the event. Process in main loop.
- Never do heavy work here.

## Main Loop Pattern

```c
InputEvent event;
for(bool running = true; running;) {
    FuriStatus status = furi_message_queue_get(queue, &event, 100);

    if(status == FuriStatusOk) {
        if(event.type == InputTypePress || event.type == InputTypeShort) {
            switch(event.key) {
            case InputKeyBack:
                if(event.type == InputTypeLong) {
                    running = false;  // exit app
                } else {
                    // short back = toggle orientation
                }
                break;
            case InputKeyOk:
                // Enter
                break;
            case InputKeyLeft:
                // send "1"
                break;
            case InputKeyUp:
                // send "2"
                break;
            case InputKeyRight:
                // send "3"
                break;
            case InputKeyDown:
                // voice toggle
                break;
            }
        }
    }

    view_port_update(view_port);
}
```

## Input Event Types

| Type | When |
|------|------|
| `InputTypePress` | Button physically pressed down |
| `InputTypeRelease` | Button released |
| `InputTypeShort` | Short press completed |
| `InputTypeLong` | Long press threshold reached |
| `InputTypeRepeat` | Held past repeat threshold |

For HID key sending, use `InputTypeShort` to avoid double-fires.
For back button, distinguish `InputTypeShort` (orientation toggle) from `InputTypeLong` (exit/go back).

## Logging

```c
#define TAG "CRemote"
FURI_LOG_I(TAG, "Remote mode entered");
FURI_LOG_E(TAG, "Failed to open file: %s", path);
FURI_LOG_D(TAG, "Button pressed: %d", event.key);
```

View logs via Flipper CLI: `log` command over serial/USB.

## Memory Management Checklist

On exit, free in reverse allocation order:
1. `gui_remove_view_port(gui, view_port)`
2. `furi_record_close(RECORD_GUI)`
3. `view_port_free(view_port)`
4. `furi_message_queue_free(queue)`
5. `furi_mutex_free(mutex)`
6. `free(app_state)`
