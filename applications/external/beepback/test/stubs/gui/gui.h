#pragma once
#include <furi.h>
#include <input/input.h>
#define RECORD_GUI "gui"
typedef struct Gui Gui;
typedef struct ViewPort ViewPort;
typedef struct Canvas Canvas;
typedef enum {
    ColorWhite,
    ColorBlack,
    ColorXOR
} Color;
typedef enum {
    FontPrimary,
    FontSecondary,
    FontKeyboard,
    FontBigNumbers
} Font;
typedef enum {
    AlignLeft,
    AlignRight,
    AlignTop,
    AlignBottom,
    AlignCenter
} Align;
typedef enum {
    GuiLayerDesktop,
    GuiLayerWindow,
    GuiLayerStatusBarLeft,
    GuiLayerStatusBarRight,
    GuiLayerFullscreen
} GuiLayer;
typedef void (*ViewPortDrawCallback)(Canvas* canvas, void* context);
typedef void (*ViewPortInputCallback)(InputEvent* event, void* context);
ViewPort* view_port_alloc(void);
void view_port_free(ViewPort* vp);
void view_port_enabled_set(ViewPort* vp, bool enabled);
void view_port_update(ViewPort* vp);
void view_port_draw_callback_set(ViewPort* vp, ViewPortDrawCallback cb, void* ctx);
void view_port_input_callback_set(ViewPort* vp, ViewPortInputCallback cb, void* ctx);
void gui_add_view_port(Gui* gui, ViewPort* vp, GuiLayer layer);
void gui_remove_view_port(Gui* gui, ViewPort* vp);
void canvas_clear(Canvas* c);
void canvas_set_color(Canvas* c, Color color);
void canvas_set_font(Canvas* c, Font font);
void canvas_draw_str(Canvas* c, int32_t x, int32_t y, const char* str);
uint16_t canvas_string_width(Canvas* c, const char* str);
void canvas_draw_str_aligned(Canvas* c, int32_t x, int32_t y, Align h, Align v, const char* str);
void canvas_draw_dot(Canvas* c, int32_t x, int32_t y);
void canvas_draw_box(Canvas* c, int32_t x, int32_t y, size_t w, size_t h);
void canvas_draw_rbox(Canvas* c, int32_t x, int32_t y, size_t w, size_t h, size_t r);
void canvas_draw_frame(Canvas* c, int32_t x, int32_t y, size_t w, size_t h);
void canvas_draw_rframe(Canvas* c, int32_t x, int32_t y, size_t w, size_t h, size_t r);
void canvas_draw_line(Canvas* c, int32_t x1, int32_t y1, int32_t x2, int32_t y2);
void canvas_draw_circle(Canvas* c, int32_t x, int32_t y, size_t r);
void canvas_draw_disc(Canvas* c, int32_t x, int32_t y, size_t r);
