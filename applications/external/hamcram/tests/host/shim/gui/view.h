/* View, input and canvas subset for host tests. Drawing calls are recorded
 * instead of rendered, so tests can inspect what would be on screen. */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct View View;
typedef struct Canvas Canvas;

typedef enum {
    InputKeyUp,
    InputKeyDown,
    InputKeyRight,
    InputKeyLeft,
    InputKeyOk,
    InputKeyBack,
} InputKey;

typedef enum {
    InputTypePress,
    InputTypeRelease,
    InputTypeShort,
    InputTypeLong,
    InputTypeRepeat,
} InputType;

typedef struct {
    InputKey key;
    InputType type;
} InputEvent;

typedef enum {
    ColorWhite,
    ColorBlack,
} Color;

typedef enum {
    FontPrimary,
    FontSecondary,
} Font;

typedef enum {
    AlignLeft,
    AlignRight,
    AlignTop,
    AlignBottom,
    AlignCenter,
} Align;

typedef enum {
    ViewModelTypeNone,
    ViewModelTypeLockFree,
    ViewModelTypeLocking,
} ViewModelType;

typedef void (*ViewDrawCallback)(Canvas* canvas, void* model);
typedef bool (*ViewInputCallback)(InputEvent* event, void* context);

View* view_alloc(void);
void view_free(View* view);
void view_allocate_model(View* view, ViewModelType type, size_t size);
void view_set_context(View* view, void* context);
void view_set_draw_callback(View* view, ViewDrawCallback callback);
void view_set_input_callback(View* view, ViewInputCallback callback);
void* view_get_model(View* view);
void view_commit_model(View* view, bool update);

#define with_view_model(view, type, code, update) \
    {                                             \
        type = view_get_model(view);              \
        {code};                                   \
        view_commit_model(view, update);          \
    }

void canvas_set_font(Canvas* canvas, Font font);
void canvas_set_color(Canvas* canvas, Color color);
void canvas_invert_color(Canvas* canvas);
size_t canvas_glyph_width(Canvas* canvas, uint16_t symbol);
uint16_t canvas_string_width(Canvas* canvas, const char* str);
void canvas_draw_box(Canvas* canvas, int32_t x, int32_t y, size_t width, size_t height);
void canvas_draw_line(Canvas* canvas, int32_t x1, int32_t y1, int32_t x2, int32_t y2);
void canvas_draw_str(Canvas* canvas, int32_t x, int32_t y, const char* str);
void canvas_draw_str_aligned(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    Align horizontal,
    Align vertical,
    const char* str);

/* ---- test side ---- */

#define SHIM_MAX_STRINGS 32

typedef struct {
    int32_t x;
    int32_t y;
    Color color;
    char text[96];
} ShimString;

struct Canvas {
    Color color;
    ShimString strings[SHIM_MAX_STRINGS];
    size_t string_count;
    size_t box_count;
    bool scrollbar;
};

/** Run the view's draw callback into a cleared canvas */
void shim_view_draw(View* view, Canvas* canvas);
/** Deliver one input event; returns what the input callback returned */
bool shim_view_input(View* view, InputKey key, InputType type);
