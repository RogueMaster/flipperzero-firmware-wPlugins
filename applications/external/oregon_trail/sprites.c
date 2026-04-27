#include "sprites.h"
#include <gui/gui.h>

// ── Wagon: 28×14 px ──────────────────────────────────────────
void sprite_wagon(Canvas* c, int ox, int oy) {
    // Canopy arch
    box(c, ox+8,  oy,    12, 1);
    box(c, ox+6,  oy+1,  16, 1);
    box(c, ox+5,  oy+2,   1, 5);  // left wall
    box(c, ox+22, oy+2,   1, 5);  // right wall
    canvas_set_color(c, ColorWhite);
    box(c, ox+6,  oy+2,  16, 5);  // hollow interior
    canvas_set_color(c, ColorBlack);
    box(c, ox+9,  oy+2,   1, 5);  // bow 1
    box(c, ox+14, oy+2,   1, 5);  // bow 2
    box(c, ox+19, oy+2,   1, 5);  // bow 3
    // Wagon box
    box(c, ox+2,  oy+7,  24, 1);  // top rail
    box(c, ox+2,  oy+8,   1, 3);  // left side
    box(c, ox+25, oy+8,   1, 3);  // right side
    box(c, ox+2,  oy+11, 24, 1);  // bottom rail
    canvas_set_color(c, ColorWhite);
    box(c, ox+3,  oy+8,  22, 3);  // hollow interior
    canvas_set_color(c, ColorBlack);
    box(c, ox+8,  oy+10, 12, 1);  // axle bar
    // 7px spoked wheels
    for(int w = 0; w < 2; w++) {
        int wx = ox + (w == 0 ? 1 : 20);
        int wy = oy + 7;
        box(c, wx+2, wy,   3, 1);
        px (c, wx+1, wy+1);
        px (c, wx+5, wy+1);
        box(c, wx,   wy+2, 1, 3);
        box(c, wx+6, wy+2, 1, 3);
        px (c, wx+1, wy+5);
        px (c, wx+5, wy+5);
        box(c, wx+2, wy+6, 3, 1);
        box(c, wx+3, wy+1, 1, 2); // N spoke
        box(c, wx+1, wy+3, 2, 1); // W spoke
        box(c, wx+4, wy+3, 2, 1); // E spoke
        box(c, wx+3, wy+4, 1, 2); // S spoke
        px (c, wx+3, wy+3);       // hub
    }
}

// ── Stickman shooter ─────────────────────────────────────────
// Feet at y+11. Barrel tip at (x+17, y+3).
void sprite_shooter(Canvas* c, int x, int y) {
    box(c, x+1, y,    2, 2); // head
    box(c, x+1, y+2,  1, 5); // body
    box(c, x,   y+4,  1, 2); // left arm
    box(c, x+2, y+3,  2, 1); // right arm
    box(c, x+1, y+3,  1, 3); // stock
    box(c, x+6, y+3, 11, 1); // barrel
    box(c, x,   y+7,  1, 4); // left leg
    box(c, x+2, y+7,  1, 4); // right leg
    box(c, x-1, y+11, 2, 1); // left foot
    box(c, x+2, y+11, 2, 1); // right foot
}

// ── Animals ───────────────────────────────────────────────────
void sprite_rabbit(Canvas* c, int x, int y) {
    px (c, x,   y);  px(c, x,   y+1); // left ear
    px (c, x+2, y);  px(c, x+2, y+1); // right ear
    box(c, x,   y+2, 4, 3);            // body
    px (c, x+4, y+3);                  // tail
}

void sprite_deer(Canvas* c, int x, int y) {
    box(c, x,   y,   1, 3); // left antler post
    box(c, x+3, y,   1, 3); // right antler post
    px (c, x-1, y);
    px (c, x+4, y);
    box(c, x,   y+3, 4, 3); // head
    box(c, x+3, y+4, 2, 1); // neck
    box(c, x+4, y+4, 9, 5); // body
    box(c, x,   y+8, 1, 5); // front-left leg
    box(c, x+2, y+8, 1, 5); // front-right leg
    box(c, x+7, y+8, 1, 5); // rear-left leg
    box(c, x+9, y+8, 1, 5); // rear-right leg
}

void sprite_buffalo(Canvas* c, int x, int y) {
    box(c, x+3, y,    8, 3); // hump
    box(c, x,   y+2, 13, 6); // body
    box(c, x-4, y+3,  6, 5); // head
    box(c, x-4, y+7,  3, 2); // beard
    box(c, x,   y+8,  2, 4); // leg 1
    box(c, x+4, y+8,  2, 4); // leg 2
    box(c, x+9, y+8,  2, 4); // leg 3
}

// ── Skull: 5×6 px ────────────────────────────────────────────
void sprite_wolf(Canvas* c, int x, int y) {
    // Head with snout pointing left (wolf runs toward shooter)
    box(c, x,   y,   3, 3);   // head
    box(c, x-2, y+1, 2, 1);   // snout
    px (c, x-2, y);            // ear tip
    // Body
    box(c, x+2, y+1, 7, 4);   // torso
    // Tail up
    box(c, x+8, y,   1, 2);
    px (c, x+9, y);
    // Legs (running pose — alternating)
    box(c, x+2, y+5, 1, 3);   // front-left leg
    box(c, x+4, y+4, 1, 4);   // front-right leg
    box(c, x+6, y+5, 1, 3);   // rear-left leg
    box(c, x+8, y+4, 1, 4);   // rear-right leg
}

// Tombstone: 14×18 px. ox,oy = top-left.
void sprite_tombstone(Canvas* c, int ox, int oy) {
    // Rounded arch top
    box(c, ox+3, oy,   8, 1);
    box(c, ox+1, oy+1,12, 1);
    box(c, ox,   oy+2,14, 8);
    // Hollow interior of arch
    canvas_set_color(c, ColorWhite);
    box(c, ox+2, oy+3, 10, 5);
    canvas_set_color(c, ColorBlack);
    // Cross carved in stone
    box(c, ox+6, oy+3, 2, 5);  // vertical
    box(c, ox+4, oy+5, 6, 1);  // horizontal
    // Rectangular base
    box(c, ox,   oy+10,14, 6);
    // Ground line / base plate
    box(c, ox-1, oy+16,16, 2);
}

void sprite_skull(Canvas* c, int x, int y) {
    box(c, x+1, y,   3, 1);
    box(c, x,   y+1, 5, 2);
    canvas_set_color(c, ColorWhite);
    px(c, x+1, y+2);           // left eye socket
    px(c, x+3, y+2);           // right eye socket
    canvas_set_color(c, ColorBlack);
    box(c, x,   y+3, 5, 1);
    box(c, x+1, y+4, 1, 2);   // left tooth
    box(c, x+3, y+4, 1, 2);   // right tooth
}
