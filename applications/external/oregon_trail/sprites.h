#pragma once
#include <gui/gui.h>

// ── Pixel helpers (inline — safe in headers) ──────────────────
static inline void px(Canvas* c, int x, int y) {
    canvas_draw_box(c, x, y, 1, 1);
}
static inline void box(Canvas* c, int x, int y, int w, int h) {
    canvas_draw_box(c, x, y, w, h);
}

// ── Sprite declarations ───────────────────────────────────────
// Implementations live in sprites.c

// Wagon: 28×14 px. ox,oy = top-left. Wheels land at oy+13.
void sprite_wagon(Canvas* c, int ox, int oy);

// Stickman in rifle-aim pose. Feet at y+11. Barrel tip at (x+17, y+3).
void sprite_shooter(Canvas* c, int x, int y);

// Animals
void sprite_rabbit (Canvas* c, int x, int y);
void sprite_deer   (Canvas* c, int x, int y);
void sprite_buffalo(Canvas* c, int x, int y);
void sprite_wolf      (Canvas* c, int x, int y);
void sprite_skull     (Canvas* c, int x, int y);
void sprite_tombstone (Canvas* c, int x, int y); // ~14×18 px
