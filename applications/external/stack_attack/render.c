#include "render.h"
#include "sprites.h"

#include <stdio.h>
#include <string.h>

#define CRANE_Y (G_CRANE_BOX_Y - 11) // crane sprite top: hangs from the lower rail line

static const Sprite* const box_sprites[G_BOX_TYPES + 1] =
    {NULL, &spr_box1, &spr_box2, &spr_box3, &spr_box4, &spr_box5, &spr_box6, &spr_box7, &spr_box8};

void fb_clear(uint8_t* fb) {
    memset(fb, 0, FB_SIZE);
}

void fb_pixel(uint8_t* fb, int x, int y, bool black) {
    if(x < 0 || x >= FB_W || y < 0 || y >= FB_H) return;
    uint8_t bit = 1 << (x & 7);
    if(black)
        fb[y * FB_STRIDE + x / 8] |= bit;
    else
        fb[y * FB_STRIDE + x / 8] &= ~bit;
}

void fb_rect(uint8_t* fb, int x, int y, int w, int h, bool black) {
    for(int j = y; j < y + h; j++)
        for(int i = x; i < x + w; i++)
            fb_pixel(fb, i, j, black);
}

static inline bool bit_at(const uint8_t* rows, int w, int x, int y) {
    return rows[y * ((w + 7) / 8) + x / 8] & (0x80 >> (x & 7));
}

// Draw a sprite; pixels outside [clip_x0, clip_x1) are skipped.
static void blit_clip(uint8_t* fb, const Sprite* s, int x, int y, int clip_x0, int clip_x1) {
    for(int j = 0; j < s->h; j++) {
        for(int i = 0; i < s->w; i++) {
            int px = x + i;
            if(px < clip_x0 || px >= clip_x1) continue;
            if(s->mask && !bit_at(s->mask, s->w, i, j)) continue;
            fb_pixel(fb, px, y + j, bit_at(s->bits, s->w, i, j));
        }
    }
}

static void blit(uint8_t* fb, const Sprite* s, int x, int y) {
    blit_clip(fb, s, x, y, 0, FB_W);
}

// Field objects disappear behind the walls, like cranes entering in the original.
static void blit_field(uint8_t* fb, const Sprite* s, int x, int y) {
    blit_clip(fb, s, x, y, G_WALL, G_FIELD_W - G_WALL);
}

void fb_text(uint8_t* fb, int x, int y, const char* s) {
    for(; *s; s++, x += 4) {
        const char* p = strchr(font3x5_chars, *s);
        if(!p) continue;
        const uint8_t* g = font3x5 + (p - font3x5_chars) * 5;
        for(int j = 0; j < 5; j++)
            for(int i = 0; i < 3; i++)
                if(g[j] & (0x80 >> i)) fb_pixel(fb, x + i, y + j, true);
    }
}

static const Sprite* worker_sprite(const Game* g) {
    const GWorker* w = &g->w;
    bool left = w->face < 0;
    if(w->vstate != WGround) return left ? &spr_worker_jump_l : &spr_worker_jump_r;
    if(w->move_left) {
        bool step = (w->anim >> 2) & 1;
        if(left) return step ? &spr_worker_l1 : &spr_worker_l0;
        return step ? &spr_worker_r1 : &spr_worker_r0;
    }
    if(w->idle >= 10) {
        // look at the player, glance down now and then
        return ((g->tick / 3) % 16 == 0) ? &spr_worker_front1 : &spr_worker_front0;
    }
    return left ? &spr_worker_l0 : &spr_worker_r0;
}

static void draw_cell(uint8_t* fb, const Game* g, int r, int c) {
    uint8_t t = g->cell[r][c];
    if(!t) return;
    int x = g_col_x(c), y = g_row_y(r);
    if(!(g->clear_mask[r] & (1u << c))) {
        blit(fb, box_sprites[t], x, y);
        return;
    }
    // explosion stage of this cell: small ring in the box, medium ring, full ring, fade
    static const Sprite* const stages[G_CLEAR_CELL_TICKS] = {
        &spr_clear1,
        &spr_clear1,
        &spr_boom2,
        &spr_boom2,
        &spr_boom2,
        &spr_boom3,
        &spr_boom3,
        &spr_boom3,
        &spr_boom3,
        &spr_boom4,
        &spr_boom4,
        &spr_boom5};
    int e = g->clear_age[r] - 1 - g_clear_delay(c);
    if(e < 0)
        blit(fb, box_sprites[t], x, y);
    else if(e < G_CLEAR_CELL_TICKS)
        blit(fb, stages[e], x, y);
}

// 3x5 digits on a 4 px pitch, right edge one pixel in from the screen edge.
static void fb_text_right(uint8_t* fb, int y, const char* s) {
    fb_text(fb, FB_W - 4 * (int)strlen(s), y, s);
}

// Side panel, one line every 7 px, everything right-aligned: label, value, blank line.
static void draw_panel(uint8_t* fb, const Game* g, uint32_t hiscore) {
    char buf[12];
    fb_text_right(fb, 3, "SCORE");
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)g->score);
    fb_text_right(fb, 10, buf);
    fb_text_right(fb, 24, "HI");
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)(g->score > hiscore ? g->score : hiscore));
    fb_text_right(fb, 31, buf);
    // cranes in rotation, the game's only difficulty knob
    fb_text_right(fb, 45, "CR");
    snprintf(buf, sizeof(buf), "%u", (unsigned)g->cranes);
    fb_text_right(fb, 52, buf);
}

void render_game(uint8_t* fb, const Game* g, uint32_t hiscore) {
    // The background never changes: draw it pixel by pixel once, then copy.
    static uint8_t bg_fb[FB_SIZE];
    static bool bg_ready = false;
    if(!bg_ready) {
        fb_clear(bg_fb);
        blit(bg_fb, &spr_background, 0, 0);
        bg_ready = true;
    }
    memcpy(fb, bg_fb, FB_SIZE);

    for(int r = 0; r < G_ROWS; r++)
        for(int c = 0; c < G_COLS; c++)
            draw_cell(fb, g, r, c);
    for(int i = 0; i < g->n_falling; i++)
        blit(fb, box_sprites[g->falling[i].type], g->falling[i].x, g->falling[i].y);
    if(g->has_pushed) blit(fb, box_sprites[g->pushed.type], g->pushed.x, g->pushed.y);

    blit(fb, worker_sprite(g), g->w.x, g->w.y);
    if(g->debris_timer)
        blit(fb, &spr_debris, g->debris_x, g->debris_y - (G_DEBRIS_TICKS - g->debris_timer));

    for(int i = 0; i < G_MAX_CRANES; i++) {
        const GCrane* c = &g->crane[i];
        if(!c->active) continue;
        if(c->state == CraneOpen) {
            blit_field(fb, &spr_crane_open, c->x - 4, CRANE_Y);
        } else {
            blit_field(fb, &spr_crane_closed, c->x - 2, CRANE_Y);
            if(c->state == CraneCarry) blit_field(fb, box_sprites[c->type], c->x, G_CRANE_BOX_Y);
        }
    }

    draw_panel(fb, g, hiscore);
}

void render_title(uint8_t* fb) {
    fb_clear(fb);
    blit(fb, &spr_logo, (FB_W - spr_logo.w) / 2, 0);
}
