# Flipper Zero — UI Patterns Reference

> Source: https://github.com/jamisonderek/flipper-zero-tutorials/wiki/User-Interface

## Display

- **Resolution:** 128 x 64 pixels, monochrome (1-bit).
- **Origin:** top-left (0,0).
- **Colors:** `ColorBlack` (draw) and `ColorWhite` (erase), plus `ColorXOR`.

## Canvas API

### Fonts

| Font | Use Case | Approx Height |
|------|----------|---------------|
| `FontPrimary` | Titles, headers | ~12px |
| `FontSecondary` | Body text, labels | ~8px |
| `FontKeyboard` | Keyboard layouts | ~8px |
| `FontBigNumbers` | Large numeric display | ~24px |

### Text Drawing

```c
// Simple text at position
canvas_set_font(canvas, FontPrimary);
canvas_draw_str(canvas, x, y, "Hello");  // y is baseline

// Aligned text
canvas_draw_str_aligned(canvas, x, y, AlignCenter, AlignTop, "Centered");
```

Alignment options: `AlignLeft`, `AlignCenter`, `AlignRight`, `AlignTop`, `AlignCenter`, `AlignBottom`.

### Shapes

```c
// Filled rectangle
canvas_draw_box(canvas, x, y, width, height);

// Outlined rectangle
canvas_draw_frame(canvas, x, y, width, height);

// Rounded rectangle
canvas_draw_rframe(canvas, x, y, width, height, radius);
canvas_draw_rbox(canvas, x, y, width, height, radius);

// Circle
canvas_draw_circle(canvas, center_x, center_y, radius);
canvas_draw_disc(canvas, center_x, center_y, radius);  // filled

// Line
canvas_draw_line(canvas, x1, y1, x2, y2);

// Pixel
canvas_draw_dot(canvas, x, y);
```

### Icons

```c
#include "claude_remote_icons.h"
canvas_draw_icon(canvas, x, y, &I_icon_name);
```

### Clear and Color

```c
canvas_clear(canvas);                    // clear entire canvas
canvas_set_color(canvas, ColorBlack);    // set draw color
canvas_invert_color(canvas);             // toggle
```

## Orientation

```c
// Set viewport orientation
view_port_set_orientation(view_port, ViewPortOrientationVertical);

// Options:
// ViewPortOrientationHorizontal     — normal landscape
// ViewPortOrientationHorizontalFlip — landscape 180
// ViewPortOrientationVertical       — portrait (USB down)
// ViewPortOrientationVerticalFlip   — portrait 180 (USB up)
```

When orientation is set, the system **automatically** remaps:
- Screen rendering (Canvas coordinates rotate).
- Input events (button directions rotate to match).

**Important for Claude Remote:** Since the Flipper handles input remapping, the app may not need manual button remapping for orientation. Test on hardware to confirm behavior. If auto-remap is sufficient, just toggle `ViewPortOrientation` on Back press.

## UI Architecture Options

### Option 1: Single ViewPort (simplest — recommended for v1)

One ViewPort with a state machine. Draw callback checks `state->current_mode` and renders accordingly.

```c
static void draw_callback(Canvas* canvas, void* ctx) {
    AppState* state = ctx;
    switch(state->mode) {
    case ModeHome:    draw_home(canvas, state); break;
    case ModeRemote:  draw_remote(canvas, state); break;
    case ModeManual:  draw_manual(canvas, state); break;
    }
}
```

Pros: Minimal code, single event loop, easy to understand.
Cons: Draw callback has branching logic.

### Option 2: ViewDispatcher + Views

Multiple View objects, each with own draw/input callbacks. ViewDispatcher switches between them.

```c
typedef enum {
    ViewHome,
    ViewRemote,
    ViewManual,
} ViewId;

ViewDispatcher* vd = view_dispatcher_alloc();
view_dispatcher_add_view(vd, ViewHome, home_view);
view_dispatcher_add_view(vd, ViewRemote, remote_view);
view_dispatcher_switch_to_view(vd, ViewHome);
```

Pros: Clean separation, built-in back navigation.
Cons: More boilerplate, overkill for 3 simple screens.

### Option 3: SceneManager + ViewDispatcher

Full scene graph with state preservation. Used by complex apps.

**Not recommended for v1** — too much overhead for this app's needs.

## Button Legend Drawing (for Remote Mode)

Example layout for 128x64 screen:

```
┌──────────────────────────┐
│    Claude Remote         │  y=12, FontPrimary
│                          │
│         [2]              │  y=28, centered
│   [1]   OK   [3]        │  y=40
│        Voice             │  y=52
│                          │
│  Back=flip  Long=exit    │  y=62, FontSecondary
└──────────────────────────┘
```

```c
static void draw_remote(Canvas* canvas, AppState* state) {
    canvas_clear(canvas);

    if(!state->bt_connected) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "Not Connected");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, "Pair Flipper as");
        canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignCenter, "Bluetooth keyboard");
        return;
    }

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignTop, "Claude Remote");

    canvas_set_font(canvas, FontSecondary);
    // Button legend
    canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignCenter, "[2] No");
    canvas_draw_str_aligned(canvas, 20, 38, AlignCenter, AlignCenter, "[1]");
    canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, "Enter");
    canvas_draw_str_aligned(canvas, 108, 38, AlignCenter, AlignCenter, "[3]");
    canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignCenter, "Voice");

    canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "Back=flip  Hold=exit");
}
```

## Text Scrolling (for Manual Mode)

```c
#define LINE_HEIGHT 10
#define VISIBLE_LINES 5
#define SCREEN_WIDTH 128

static void draw_manual(Canvas* canvas, AppState* state) {
    canvas_clear(canvas);

    // Chapter title bar
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, state->chapter_title);
    canvas_draw_line(canvas, 0, 13, 128, 13);

    // Scrollable body text
    canvas_set_font(canvas, FontSecondary);
    int y = 24 - state->scroll_offset;
    // Render lines, clipping to visible area
    for(int i = 0; i < state->line_count; i++) {
        if(y > 10 && y < 64) {
            canvas_draw_str(canvas, 2, y, state->lines[i]);
        }
        y += LINE_HEIGHT;
    }

    // Scroll indicators
    if(state->scroll_offset > 0) {
        canvas_draw_str_aligned(canvas, 124, 14, AlignRight, AlignTop, "^");
    }
    if(y > 64) {
        canvas_draw_str_aligned(canvas, 124, 62, AlignRight, AlignBottom, "v");
    }
}
```

Up/Down in main loop adjusts `state->scroll_offset` by `LINE_HEIGHT`.
Left/Right changes `state->chapter_index` and reloads text.
