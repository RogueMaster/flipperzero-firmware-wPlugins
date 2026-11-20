// Lanternfall for Flipper Zero: 1-bit renderer. Pure C, writes a 128x64 framebuffer.
#include "render.h"
#include "sprites.h"
#include <stdio.h>
#include <string.h>

enum {
    C_WHITE,
    C_BLACK,
    C_XOR
};
enum {
    B_FLIP = 1,
    B_VFLIP = 2,
    B_INVERT = 4,
    B_INK = 8,
    B_XOR = 16
};

static inline void px(uint8_t* fb, int x, int y, int c) {
    if((unsigned)x >= SCR_W || (unsigned)y >= SCR_H) return;
    uint8_t* b = &fb[y * 16 + (x >> 3)];
    uint8_t m = 1 << (x & 7);
    if(c == C_BLACK)
        *b |= m;
    else if(c == C_WHITE)
        *b &= ~m;
    else
        *b ^= m;
}

enum {
    OP_SET,
    OP_INK,
    OP_INKW,
    OP_XOR
};

// Applies one row of up to 16 pixels (bit 0 = leftmost) at screen x, a byte at a time.
static void row_op(uint8_t* fb, int x, int y, uint32_t ink, uint32_t mask, int op) {
    if((unsigned)y >= SCR_H || x <= -16 || x >= SCR_W) return;
    if(x < 0) ink >>= -x, mask >>= -x, x = 0;
    int b = x >> 3, sh = x & 7;
    ink <<= sh, mask <<= sh;
    uint8_t* row = fb + y * 16;
    for(int k = 0; k < 3 && b + k < 16; k++, ink >>= 8, mask >>= 8) {
        uint8_t i = (uint8_t)ink, m = (uint8_t)mask;
        switch(op) {
        case OP_SET:
            row[b + k] = (row[b + k] & ~m) | (i & m);
            break;
        case OP_INK:
            row[b + k] |= i;
            break;
        case OP_INKW:
            row[b + k] &= ~i;
            break;
        default:
            row[b + k] ^= i;
        }
    }
}

static void rect(uint8_t* fb, int x, int y, int w, int h, int c) {
    if(x < 0) w += x, x = 0;
    if(y < 0) h += y, y = 0;
    if(x + w > SCR_W) w = SCR_W - x;
    if(y + h > SCR_H) h = SCR_H - y;
    for(int j = y; j < y + h; j++)
        for(int xx = x; xx < x + w; xx += 16) {
            int n = x + w - xx < 16 ? x + w - xx : 16;
            uint32_t bits = (1u << n) - 1;
            if(c == C_XOR)
                row_op(fb, xx, j, bits, bits, OP_XOR);
            else
                row_op(fb, xx, j, c == C_BLACK ? bits : 0, bits, OP_SET);
        }
}
static void frame(uint8_t* fb, int x, int y, int w, int h) {
    rect(fb, x, y, w, 1, C_BLACK);
    rect(fb, x, y + h - 1, w, 1, C_BLACK);
    rect(fb, x, y, 1, h, C_BLACK);
    rect(fb, x + w - 1, y, 1, h, C_BLACK);
}
static void panel(uint8_t* fb, int x, int y, int w, int h) {
    rect(fb, x, y, w, h, C_WHITE);
    frame(fb, x, y, w, h);
}

static uint16_t rev(uint16_t v, int w) {
    uint16_t r = 0;
    for(int i = 0; i < w; i++)
        r |= ((v >> i) & 1) << (w - 1 - i);
    return r;
}

static void blit(uint8_t* fb, const Spr* s, int x, int y, int flags) {
    x -= s->ox, y -= s->oy;
    if(x >= SCR_W || y >= SCR_H || x + s->w <= 0 || y + s->h <= 0) return;
    for(int r = 0; r < s->h; r++) {
        int sr = (flags & B_VFLIP) ? s->h - 1 - r : r;
        uint32_t ink = s->ink[sr], mask = s->mask[sr] | ink;
        if(flags & B_FLIP) ink = rev(ink, s->w), mask = rev(mask, s->w);
        if(flags & B_XOR)
            row_op(fb, x, y + r, ink, ink, OP_XOR);
        else if(flags & B_INK)
            row_op(fb, x, y + r, ink, ink, (flags & B_INVERT) ? OP_INKW : OP_INK);
        else
            row_op(fb, x, y + r, (flags & B_INVERT) ? (mask & ~ink) : ink, mask, OP_SET);
    }
}

// ---------------------------------------------------------------- text

int fb_text_w(const char* s, int scale) {
    int n = (int)strlen(s);
    return n ? n * 4 * scale - scale : 0;
}

static void glyph(uint8_t* fb, int x, int y, char ch, int scale, int c) {
    if(ch < 32 || ch > 127) ch = '?';
    uint16_t g = FONT3x5[ch - 32];
    for(int gy = 0; gy < 5; gy++)
        for(int gx = 0; gx < 3; gx++)
            if(g >> (gy * 3 + gx) & 1)
                for(int sy = 0; sy < scale; sy++)
                    for(int sx = 0; sx < scale; sx++)
                        px(fb, x + gx * scale + sx, y + gy * scale + sy, c);
}

void fb_text(uint8_t* fb, int x, int y, const char* s, int scale, bool outline) {
    if(outline)
        for(const char* p = s; *p; p++) {
            int gx = x + (int)(p - s) * 4 * scale;
            for(int oy = -1; oy <= 1; oy++)
                for(int ox = -1; ox <= 1; ox++)
                    glyph(fb, gx + ox, y + oy, *p, scale, C_WHITE);
        }
    for(const char* p = s; *p; p++)
        glyph(fb, x + (int)(p - s) * 4 * scale, y, *p, scale, C_BLACK);
}

static void text_c(uint8_t* fb, int cx, int y, const char* s, int scale, bool outline) {
    fb_text(fb, cx - fb_text_w(s, scale) / 2, y, s, scale, outline);
}

static void upper(char* s) {
    for(; *s; s++)
        if(*s >= 'a' && *s <= 'z') *s -= 32;
}

// ---------------------------------------------------------------- lighting

typedef struct {
    int16_t x, y, r;
} Light;
#define MAX_LIGHT 20
static Light lights[MAX_LIGHT];
static int nlights;

static void add_light(int x, int y, int r) {
    if(nlights >= MAX_LIGHT || x < -r || y < -r || x > SCR_W + r || y > SCR_H + r) return;
    lights[nlights++] = (Light){x, y, r};
}

static const uint8_t BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

// Outside the light the picture turns to its negative, thinned like moonlight; the edges
// of each pool are dithered so lanterns glow. Light is sampled on a 2x2 grid to keep it cheap.
static uint8_t emap[SCR_H / 2][SCR_W / 2];

static void night_pass(uint8_t* fb, int dark) {
    if(dark <= 0) return;
    uint32_t inv[MAX_LIGHT];
    for(int i = 0; i < nlights; i++)
        inv[i] = (28u << 12) / (uint32_t)(lights[i].r * lights[i].r);
    for(int cy = 0; cy < SCR_H / 2; cy++) {
        int y = cy * 2 + 1;
        int idx[MAX_LIGHT], n = 0;
        for(int i = 0; i < nlights; i++)
            if(lights[i].y - lights[i].r <= y && lights[i].y + lights[i].r >= y) idx[n++] = i;
        for(int cx = 0; cx < SCR_W / 2; cx++) {
            int x = cx * 2 + 1, best = 0;
            for(int k = 0; k < n && best < 16; k++) {
                const Light* l = &lights[idx[k]];
                int dx = x - l->x, dy = y - l->y, r2 = l->r * l->r, d2 = dx * dx + dy * dy;
                if(d2 < r2) {
                    int v = (int)(((uint32_t)(r2 - d2) * inv[idx[k]]) >> 12);
                    if(v > best) best = v;
                }
            }
            if(best > 16) best = 16;
            emap[cy][cx] = (uint8_t)(dark * (16 - best) / 16);
        }
    }
    for(int y = 0; y < SCR_H; y++) {
        uint8_t* row = fb + y * 16;
        const uint8_t* er = emap[y >> 1];
        const uint8_t* bay = BAYER[y & 3];
        for(int bx = 0; bx < 16; bx++) {
            uint8_t orig = row[bx], dm = 0, mm = 0;
            for(int k = 0; k < 8; k++) {
                int x = bx * 8 + k, e = er[x >> 1];
                if(e <= bay[x & 3]) continue;
                dm |= 1 << k;
                // dusk only darkens; full dark shows a thin moonlit negative
                if(e >= 12 && (orig >> k & 1) && ((x ^ y) & 1) && ((x + y) & 2)) mm |= 1 << k;
            }
            row[bx] = (uint8_t)((orig & ~dm) | (dm & ~mm));
        }
    }
}

// ---------------------------------------------------------------- world

#define CW 21
#define CH 13
static uint8_t tc[CH][CW];
static int tcx, tcy; // world tile of tc[0][0]

static uint8_t tile(const Game* g, int x, int y) {
    int i = x - tcx, j = y - tcy;
    if(i >= 0 && j >= 0 && i < CW && j < CH) return tc[j][i];
    return world_tile(g->seed, x, y);
}

static bool pathlike(uint8_t t) {
    return t == T_PATH || t == T_PLAZA || t == T_WELL || t == T_STALL || t == T_BOARD;
}
static bool wet(uint8_t t) {
    return t == T_WATER || t == T_DEEP;
}

static const Spr* const GRASS[4] = {&SPR_GRASS0, &SPR_GRASS1, &SPR_GRASS2, &SPR_GRASS3};

// Opaque 8x8 tile, rows rotated by `shift` for flowing water.
static void blit_tile(uint8_t* fb, const Spr* s, int x, int y, int shift) {
    for(int r = 0; r < 8; r++) {
        uint32_t ink = s->ink[r];
        int sh = ((shift + (r >> 2)) % 8 + 8) % 8;
        if(sh) ink = ((ink << sh) | (ink >> (8 - sh))) & 0xff;
        row_op(fb, x, y + r, ink, 0xff, OP_SET);
    }
}

static void ground(uint8_t* fb, const Game* g, int wx, int wy, int sx, int sy) {
    uint8_t t = tile(g, wx, wy);
    uint32_t h = lf_hash(g->seed, wx, wy, 11);
    uint32_t tk = g->tick;
    switch(t) {
    case T_DEEP:
        blit_tile(fb, &SPR_DEEP, sx, sy, (int)(tk / 10) + (wy & 1) * 3);
        break;
    case T_WATER: {
        blit_tile(fb, &SPR_WATER, sx, sy, (int)(tk / 8) + (int)(h & 7));
        // shore lines where the water meets land
        if(!wet(tile(g, wx, wy - 1)))
            for(int i = 0; i < 8; i++)
                px(fb, sx + i, sy, C_BLACK),
                    px(fb, sx + i, sy + 1, (i + tk / 6) % 4 ? C_WHITE : C_BLACK);
        if(!wet(tile(g, wx, wy + 1)))
            for(int i = 0; i < 8; i++)
                px(fb, sx + i, sy + 7, C_BLACK);
        if(!wet(tile(g, wx - 1, wy)))
            for(int i = 0; i < 8; i++)
                px(fb, sx, sy + i, C_BLACK);
        if(!wet(tile(g, wx + 1, wy)))
            for(int i = 0; i < 8; i++)
                px(fb, sx + 7, sy + i, C_BLACK);
        if(g->rain) { // ripples
            uint32_t rh = lf_hash(g->seed, wx, wy, tk / 6);
            if(rh % (8 - g->rain * 2) == 0) {
                int rx = sx + 2 + (rh >> 8) % 4, ry = sy + 2 + (rh >> 12) % 4;
                px(fb, rx - 1, ry, C_BLACK), px(fb, rx + 1, ry, C_BLACK),
                    px(fb, rx, ry - 1, C_BLACK), px(fb, rx, ry + 1, C_BLACK);
            }
        }
        break;
    }
    case T_SAND:
        blit_tile(fb, h & 1 ? &SPR_SAND1 : &SPR_SAND0, sx, sy, 0);
        break;
    case T_TALL: {
        int sway = g->rain >= 2 ? ((tk / 6 + wx) & 1) : ((tk / 16 + wx + wy) % 4 == 0);
        blit_tile(fb, &SPR_GRASS3, sx, sy, 0);
        blit(fb, &SPR_TALL, sx - sway, sy, B_INK);
        break;
    }
    case T_FLOWER:
        blit_tile(fb, h & 1 ? &SPR_FLOWER1 : &SPR_FLOWER0, sx, sy, 0);
        break;
    case T_PATH:
    case T_PLAZA:
    case T_WELL:
    case T_STALL:
    case T_BOARD:
        blit_tile(fb, t == T_PATH ? (h & 1 ? &SPR_PATH1 : &SPR_PATH0) : &SPR_PLAZA, sx, sy, 0);
        if(!pathlike(tile(g, wx, wy - 1)))
            for(int i = 0; i < 8; i += 2)
                px(fb, sx + i + (wx & 1), sy, C_BLACK);
        if(!pathlike(tile(g, wx, wy + 1)))
            for(int i = 0; i < 8; i += 2)
                px(fb, sx + i + (wx & 1), sy + 7, C_BLACK);
        if(!pathlike(tile(g, wx - 1, wy)))
            for(int i = 0; i < 8; i += 2)
                px(fb, sx, sy + i + (wy & 1), C_BLACK);
        if(!pathlike(tile(g, wx + 1, wy)))
            for(int i = 0; i < 8; i += 2)
                px(fb, sx + 7, sy + i + (wy & 1), C_BLACK);
        break;
    case T_FARM:
        blit_tile(fb, &SPR_FARM, sx, sy, 0);
        break;
    case T_CROP:
        blit_tile(fb, &SPR_FARM, sx, sy, 0);
        blit(fb, &SPR_CROP, sx, sy, B_INK);
        break;
    default: // mostly bare so creatures read against it
        if((h & 7) < 3)
            blit_tile(fb, GRASS[h & 3], sx, sy, 0);
        else
            rect(fb, sx, sy, 8, 8, C_WHITE);
    }
}

static void prop(uint8_t* fb, const Game* g, int wx, int wy, int sx, int sy, int dark) {
    uint8_t t = tile(g, wx, wy);
    uint32_t tk = g->tick;
    switch(t) {
    case T_TREE:
        blit(fb, &SPR_TREE, sx, sy - 2, 0);
        break;
    case T_PINE:
        blit(fb, &SPR_PINE, sx, sy - 2, 0);
        break;
    case T_BUSH:
        blit(fb, &SPR_BUSH, sx, sy, 0);
        break;
    case T_ROCK:
        blit(fb, &SPR_ROCK, sx, sy, 0);
        break;
    case T_FENCE:
        blit(fb, &SPR_FENCE, sx, sy, 0);
        break;
    case T_WELL:
        blit(fb, &SPR_WELL, sx, sy, 0);
        break;
    case T_STALL:
        blit(fb, &SPR_STALL, sx, sy - 2, 0);
        break;
    case T_BOARD: {
        blit(fb, &SPR_BOARD, sx, sy - 2, 0);
        if(!g->q_active || g->q_have >= g->q_need)
            blit(fb, &SPR_ALERT, sx + 2, sy - 11 + (int)((tk / 6) & 1), 0);
        break;
    }
    case T_LANTERN: {
        uint32_t f = lf_hash(wx, wy, tk / 3, 2);
        blit(fb, (f & 3) ? &SPR_LANTERN0 : &SPR_LANTERN1, sx, sy - 4, 0);
        if(dark) add_light(sx + 4, sy - 1, 25 + (int)(f >> 8) % 3);
        break;
    }
    case T_HOUSE: {
        if(tile(g, wx - 1, wy) == T_HOUSE || tile(g, wx, wy - 1) == T_HOUSE) break;
        bool tea = lf_hash(g->seed, wx, wy, 5) & 1;
        blit(fb, tea ? &SPR_TEAHOUSE : &SPR_HOUSE, sx, sy, 0);
        if(!tea) // chimney smoke
            for(int k = 0; k < 3; k++) {
                int ph = (int)((tk + k * 23 + (wx * 7)) % 69);
                int py = sy - 1 - ph / 4, pxx = sx + 11 + ((ph / 10 + k) % 3) - 1 + ph / 23;
                if(ph < 46) {
                    px(fb, pxx, py, C_BLACK);
                    if(ph < 20) px(fb, pxx + 1, py, C_BLACK);
                }
            }
        if(dark >= 8) {
            if(tea)
                add_light(sx + 8, sy + 12, 12);
            else
                add_light(sx + 4, sy + 9, 9), add_light(sx + 12, sy + 9, 9);
        }
        break;
    }
    }
}

static void hp_bar(uint8_t* fb, int x, int y, int hp, int maxhp) {
    if(hp >= maxhp || maxhp <= 0) return;
    rect(fb, x - 1, y - 1, 10, 3, C_WHITE);
    rect(fb, x, y, hp * 8 / maxhp + 1, 1, C_BLACK);
}

static void draw_hero(uint8_t* fb, const Game* g, int sx, int sy) {
    const Ent* p = &g->pl;
    if(g->inv && (g->tick & 2)) return;
    int fr = p->mv ? (p->anim & 1) : 0;
    const Spr* s;
    int fl = 0;
    switch(p->face) {
    case F_UP:
        s = fr ? &SPR_PL_U1 : &SPR_PL_U0;
        break;
    case F_LEFT:
        s = fr ? &SPR_PL_R1 : &SPR_PL_R0, fl = B_FLIP;
        break;
    case F_RIGHT:
        s = fr ? &SPR_PL_R1 : &SPR_PL_R0;
        break;
    default:
        s = fr ? &SPR_PL_D1 : &SPR_PL_D0;
    }
    int bob = (!p->mv && (g->tick / 12) % 2) ? 0 : 0;
    blit(fb, s, sx, sy - 2 + bob, fl);
}

static void draw_slash(uint8_t* fb, const Game* g, int sx, int sy) {
    if(g->swing < 2 || g->swing > 5) return;
    static const int8_t DX[4] = {0, 0, -1, 1}, DY[4] = {1, -1, 0, 0};
    int f = g->pl.face, x = sx + DX[f] * 7, y = sy + DY[f] * 7 - 1;
    const Spr* s = (f == F_LEFT || f == F_RIGHT) ? &SPR_SLASH_R : &SPR_SLASH_D;
    int fl = (f == F_LEFT ? B_FLIP : 0) | (f == F_UP ? B_VFLIP : 0);
    blit(fb, s, x, y, fl | B_XOR);
    if(g->swing == 4 || g->swing == 3)
        blit(
            fb, s, x + (f == F_RIGHT) - (f == F_LEFT), y + (f == F_DOWN) - (f == F_UP), fl | B_XOR);
}

static void draw_ent(uint8_t* fb, const Game* g, const Ent* e, int i, int sx, int sy) {
    int fr = e->mv ? (e->anim & 1) : (int)((g->tick / 10 + i) & 1);
    int fl = e->face == F_LEFT ? B_FLIP : 0;
    int inv = e->flash ? B_INVERT : 0;
    int shake =
        e->state == S_WIND || (e->state == S_CHARGE && e->timer) ? ((g->tick & 1) ? 1 : -1) : 0;
    switch(e->type) {
    case E_SLIME:
        blit(fb, (e->mv || fr) ? &SPR_SLIME1 : &SPR_SLIME0, sx + shake, sy, inv);
        break;
    case E_WOLF:
        blit(fb, fr ? &SPR_WOLF1 : &SPR_WOLF0, sx - 1 + shake, sy, fl | inv);
        break;
    case E_BOAR:
        blit(fb, fr ? &SPR_BOAR1 : &SPR_BOAR0, sx - 1 + shake, sy, fl | inv);
        if(e->state == S_CHARGE && !e->timer && (g->tick & 1)) { // dust behind a charging boar
            px(fb, sx + 4 - e->dx * 7, sy + 7 - e->dy * 3, C_BLACK);
            px(fb, sx + 3 - e->dx * 9, sy + 6 - e->dy * 5, C_BLACK);
        }
        break;
    case E_SHADE: {
        if(e->state == S_FADE && (g->tick & 1)) return;
        int bob = (int)((g->tick / 6 + i) % 4);
        bob = bob == 3 ? 1 : bob > 1 ? 1 : 0;
        blit(
            fb, fr ? &SPR_SHADE1 : &SPR_SHADE0, sx + shake, sy - 3 - bob, e->flash ? 0 : B_INVERT);
        break;
    }
    case E_VILLAGER: {
        bool alt = e->seed & 1;
        const Spr* s = alt ? (fr && e->mv ? &SPR_VIL2_D1 : &SPR_VIL2_D0) :
                             (fr && e->mv ? &SPR_VIL_D1 : &SPR_VIL_D0);
        blit(fb, s, sx, sy - 2, 0);
        return;
    }
    }
    if(e->state == S_WIND || (e->state == S_CHARGE && e->timer))
        blit(fb, &SPR_ALERT, sx + 3, sy - 12, 0);
    if(e->state == S_STUN) { // circling stars
        int a = (int)(g->tick / 3) % 4;
        static const int8_t SX[4] = {0, 3, 6, 3}, SY[4] = {0, -1, 0, 1};
        px(fb, sx + 1 + SX[a], sy - 4 + SY[a], C_XOR);
        px(fb, sx + 1 + SX[(a + 2) % 4], sy - 4 + SY[(a + 2) % 4], C_XOR);
    }
    hp_bar(fb, sx, sy - (e->type == E_SHADE ? 7 : 4), e->hp, e->maxhp);
}

static void draw_num(uint8_t* fb, int x, int y, int v, bool crit) {
    char b[8];
    snprintf(b, sizeof(b), crit ? "%d!" : "%d", v);
    fb_text(fb, x - fb_text_w(b, 1) / 2 + 2, y, b, 1, true);
}

static void draw_parts(uint8_t* fb, const Game* g, int cx, int cy, int dark) {
    for(int i = 0; i < MAX_PART; i++) {
        const Part* p = &g->parts[i];
        if(!p->kind) continue;
        int x = p->x / 4 - cx, y = p->y / 4 - cy;
        switch(p->kind) {
        case P_NUM:
            draw_num(fb, x, y, p->val, false);
            break;
        case P_CRIT:
            draw_num(fb, x, y, p->val, true);
            break;
        case P_SPARK:
            blit(fb, &SPR_SPARK, x, y, B_XOR);
            if(dark) add_light(x + 2, y + 2, 9);
            break;
        case P_POOF: {
            int r = (10 - p->life) / 2 + 2;
            static const int8_t DX[8] = {2, 1, 0, -1, -2, -1, 0, 1},
                                DY[8] = {0, 1, 2, 1, 0, -1, -2, -1};
            for(int k = 0; k < 8; k++)
                px(fb, x + DX[k] * r / 2, y + DY[k] * r / 2, C_XOR);
            break;
        }
        case P_HEART:
            blit(fb, &SPR_HEART, x, y, 0);
            break;
        case P_COIN: {
            char b[8];
            snprintf(b, sizeof(b), "+%d", p->val);
            blit(fb, &SPR_COIN, x - 4, y - 1, 0);
            fb_text(fb, x + 3, y, b, 1, true);
            break;
        }
        case P_LEAF:
            px(fb, x, y, C_BLACK), px(fb, x + 1, y, C_BLACK);
            break;
        }
    }
}

static void draw_rain(uint8_t* fb, const Game* g) {
    int n = g->rain * 14;
    int slant = g->rain == 3 ? 2 : 3; // storms blow harder
    for(int i = 0; i < n; i++) {
        uint32_t h = lf_hash(0x5eed, i, 0, 7);
        int land = 6 + (int)((h >> 16) % 58);
        int y = (int)(((h >> 8) % 80 + g->tick * 4) % 80) - 8;
        int x0 = (int)(h % 170);
        if(y < land) {
            for(int k = 0; k < 3; k++)
                px(fb, x0 - (y + k) / slant, y + k, C_XOR);
        } else if(y < land + 5) {
            int x = x0 - land / slant;
            px(fb, x - 1, land, C_XOR), px(fb, x + 1, land, C_XOR);
            if(y < land + 2) px(fb, x, land - 1, C_XOR);
        }
    }
}

// ---------------------------------------------------------------- HUD

static void hud(uint8_t* fb, const Game* g) {
    // health
    panel(fb, -1, -1, 46, 9);
    blit(fb, &SPR_HEART, 1, 1, 0);
    frame(fb, 8, 2, 24, 5);
    int w = g->pl.maxhp ? g->pl.hp * 22 / g->pl.maxhp : 0;
    rect(fb, 9, 3, w, 3, C_BLACK);
    if(g->pl.hp * 4 <= g->pl.maxhp && (g->tick & 4))
        rect(fb, 9, 3, w, 3, C_WHITE); // low health blink
    char b[16];
    snprintf(b, sizeof(b), "L%d", g->lvl);
    fb_text(fb, 34, 2, b, 1, false);
    // coins, time of day, weather
    snprintf(b, sizeof(b), "%d", g->coins);
    int tw = fb_text_w(b, 1);
    int x0 = SCR_W - tw - 9 - 9 - (g->rain ? 7 : 0);
    panel(fb, x0 - 2, -1, SCR_W - x0 + 3, 9);
    blit(fb, game_is_night(g) ? &SPR_MOON : &SPR_SUN, x0, 0, 0);
    if(g->rain) blit(fb, &SPR_DROP, x0 + 8, 1, 0);
    blit(fb, &SPR_COIN, SCR_W - tw - 7, 1, 0);
    fb_text(fb, SCR_W - tw - 1, 2, b, 1, false);
    // quest tracker
    if(g->q_active) {
        if(g->q_have >= g->q_need)
            snprintf(b, sizeof(b), "TO A BOARD!");
        else
            snprintf(b, sizeof(b), "%s %d/%d", MON_NAME[g->q_type], g->q_have, g->q_need);
        upper(b);
        fb_text(fb, 1, 11, b, 1, true);
    }
    if(g->idle) { // what idle is up to, top middle
        const char* t = g->rest           ? "REST" :
                        g->goal == G_HUNT ? "HUNT" :
                        g->goal == G_ROAM ? "ROAM" :
                                            "IDLE";
        char b2[12];
        snprintf(b2, sizeof(b2), "%s%.*s", t, (int)(g->tick / 8 % 4), "...");
        fb_text(fb, 64 - (fb_text_w(t, 1) + 12) / 2, 2, b2, 1, true);
    }
    if(g->banner_t) {
        char n[24];
        snprintf(n, sizeof(n), "%s", g->banner);
        upper(n);
        int bw = fb_text_w(n, 1) + 20, bx = (SCR_W - bw) / 2;
        int slide = g->banner_t > 52 ? (g->banner_t - 52) * 2 :
                    g->banner_t < 8  ? (8 - g->banner_t) * 2 :
                                       0;
        int by = 14 - slide;
        panel(fb, bx, by, bw, 11);
        frame(fb, bx + 2, by + 2, bw - 4, 7);
        px(fb, bx + 5, by + 5, C_BLACK), px(fb, bx + bw - 6, by + 5, C_BLACK);
        fb_text(fb, bx + 10, by + 3, n, 1, false);
    }
    if(g->toast_t) {
        int tw2 = fb_text_w(g->toast, 1) + 8, tx = (SCR_W - tw2) / 2;
        panel(fb, tx, 47, tw2, 9);
        fb_text(fb, tx + 4, 49, g->toast, 1, false);
    }
}

// ---------------------------------------------------------------- screens

static void title(uint8_t* fb, const Game* g) {
    memset(fb, 0, FB_SIZE);
    // distant rooftops and a lantern-lit square
    for(int x = 0; x < SCR_W; x++) {
        int h = 44 + (int)(lf_hash(3, x / 9, 0, 1) % 6);
        for(int y = h; y < SCR_H; y++)
            px(fb, x, y, C_BLACK);
    }
    int sway = (int)((g->tick / 8) % 4);
    sway = sway == 3 ? 1 : sway;
    nlights = 0;
    add_light(64, 14, 30 + (int)(lf_hash(1, g->tick / 3, 0, 0) % 3));
    night_pass(fb, 16);
    for(int y = 0; y < 4; y++)
        px(fb, 64, y, C_BLACK);
    blit(fb, &SPR_BIGLANTERN, 56 + sway - 1, 3, 0);
    draw_rain(fb, g);
    text_c(fb, 64, 27, "LANTERNFALL", 2, true);
    int n = game_menu(g, -1, NULL, 0);
    for(int i = 0; i < n; i++) {
        char b[20];
        game_menu(g, i, b, sizeof(b));
        int y = n == 1 ? 48 : 44 + i * 8;
        text_c(fb, 64, y, b, 1, true);
        if(g->cursor == i && (g->tick & 8)) {
            int w = fb_text_w(b, 1);
            fb_text(fb, 64 - w / 2 - 8, y, ">", 1, true);
            fb_text(fb, 64 + w / 2 + 5, y, "<", 1, true);
        }
    }
}

static uint8_t mapc[64 * 64 / 8];
static bool map_ok;

static void map_screen(uint8_t* fb, const Game* g) {
    memset(fb, 0, FB_SIZE);
    if(!map_ok) {
        memset(mapc, 0, sizeof(mapc));
        for(int my = 0; my < 64; my++)
            for(int mx = 0; mx < 64; mx++) {
                int ci = (my / 2) * 32 + mx / 2; // 8x8-tile chunks, two map pixels each
                bool seen = g->explored[ci >> 3] >> (ci & 7) & 1;
                bool black;
                if(!seen)
                    black = ((mx + my) & 1) || (my & 1);
                else {
                    uint8_t t = world_base(g->seed, mx * 4 + 2, my * 4 + 2);
                    if(t == T_DEEP)
                        black = true;
                    else if(t == T_WATER)
                        black = (mx + my) & 1;
                    else if(t == T_TREE || t == T_PINE || t == T_BUSH)
                        black = !(mx & 1) && !(my & 1);
                    else if(t == T_ROCK)
                        black = (mx & 1) && (my & 1);
                    else
                        black = false;
                }
                if(black) mapc[my * 8 + mx / 8] |= 1 << (mx & 7);
            }
        map_ok = true;
    }
    for(int my = 0; my < 64; my++)
        memcpy(fb + my * 16, mapc + my * 8, 8);
    for(int cy = 0; cy * CELL < WORLD; cy++)
        for(int cx = 0; cx * CELL < WORLD; cx++) {
            Village v;
            village_in_cell(g->seed, cx, cy, &v);
            if(!v.ok) continue;
            int ci = (v.y / 8) * 32 + v.x / 8;
            if(!(g->explored[ci >> 3] >> (ci & 7) & 1)) continue;
            int x = v.x / 4, y = v.y / 4;
            rect(fb, x - 2, y - 2, 5, 5, C_BLACK);
            rect(fb, x - 1, y - 1, 3, 3, C_WHITE);
        }
    int hx = g->pl.x / 4, hy = g->pl.y / 4;
    if(g->tick & 4) {
        rect(fb, hx - 2, hy, 5, 1, C_XOR);
        rect(fb, hx, hy - 2, 1, 5, C_XOR);
    }
    rect(fb, 64, 0, 64, 64, C_WHITE);
    frame(fb, 64, 0, 64, 64);
    text_c(fb, 96, 4, "MAP", 2, false);
    char b[24];
    Village v;
    if(village_at(g->seed, g->pl.x, g->pl.y, &v))
        village_name(&v, b, sizeof(b));
    else
        snprintf(b, sizeof(b), "THE WILDS");
    upper(b);
    text_c(fb, 96, 20, b, 1, false);
    snprintf(b, sizeof(b), "DAY %lu", (unsigned long)(g->play_ticks / DAY_LEN + 1));
    text_c(fb, 96, 30, b, 1, false);
    int seen = 0;
    for(int i = 0; i < 128; i++)
        for(int k = 0; k < 8; k++)
            seen += g->explored[i] >> k & 1;
    snprintf(b, sizeof(b), "%d%% SEEN", seen ? 1 + seen * 100 / 1024 : 0);
    text_c(fb, 96, 38, b, 1, false);
    rect(fb, 70, 50, 5, 5, C_BLACK), rect(fb, 71, 51, 3, 3, C_WHITE);
    fb_text(fb, 77, 50, "TOWN", 1, false);
    rect(fb, 100, 52, 5, 1, C_BLACK), rect(fb, 102, 50, 1, 5, C_BLACK);
    fb_text(fb, 107, 50, "YOU", 1, false);
}

void render(const Game* g, uint8_t* fb) {
    nlights = 0;
    if(g->ui == UI_TITLE) {
        title(fb, g);
        return;
    }
    if(g->ui == UI_MAP) {
        map_screen(fb, g);
        return;
    }
    map_ok = false;
    memset(fb, 0, FB_SIZE);

    int hx, hy;
    game_hero_px(g, &hx, &hy);
    int cx = hx - (SCR_W / 2 - 4), cy = hy - (SCR_H / 2 - 3);
    if(cx < 0) cx = 0;
    if(cy < 0) cy = 0;
    if(cx > WORLD * TILE - SCR_W) cx = WORLD * TILE - SCR_W;
    if(cy > WORLD * TILE - SCR_H) cy = WORLD * TILE - SCR_H;
    if(g->shake && (g->opt & OPT_SHAKE)) cx += (g->tick & 1) ? 1 : -1, cy += (g->tick & 2) ? 1 : 0;
    int dark = game_darkness(g);

    static uint32_t tc_seed;
    static bool tc_ok;
    int ntx = (cx >> 3) - 2, nty = (cy >> 3) - 2;
    if(!tc_ok || ntx != tcx || nty != tcy ||
       tc_seed != g->seed) { // the world never changes, only the view
        tcx = ntx, tcy = nty, tc_seed = g->seed, tc_ok = true;
        for(int j = 0; j < CH; j++)
            for(int i = 0; i < CW; i++)
                tc[j][i] = world_tile(g->seed, tcx + i, tcy + j);
    }

    int tx0 = cx >> 3, ty0 = cy >> 3;
    for(int ty = ty0; ty <= ty0 + 8; ty++)
        for(int tx = tx0; tx <= tx0 + 16; tx++)
            ground(fb, g, tx, ty, tx * TILE - cx, ty * TILE - cy);

    for(int i = 0; i < MAX_FOOT; i++) { // footprints fade out
        const Foot* f = &g->foot[i];
        if(f->age >= 200 || (f->age > 120 && (f->age & 1))) continue;
        if(f->x == 0 && f->y == 0) continue;
        int sx = f->x * TILE - cx, sy = f->y * TILE - cy;
        bool vert = f->face == F_UP || f->face == F_DOWN;
        px(fb, sx + (vert ? 2 : 1), sy + (vert ? 2 : 3), C_BLACK);
        px(fb, sx + (vert ? 5 : 5), sy + (vert ? 5 : 5), C_BLACK);
    }

    // actors in screen space, drawn row by row so props in front cover them
    typedef struct {
        int x, y, row;
        int8_t idx; // -1 hero
    } Act;
    Act act[MAX_ENT + 1];
    int na = 0;
    act[na++] = (Act){hx - cx, hy - cy, (hy + 7) >> 3, -1};
    for(int i = 0; i < MAX_ENT; i++) {
        const Ent* e = &g->ents[i];
        if(!e->type) continue;
        int ex, ey;
        ent_px(e, &ex, &ey);
        if(ex - cx < -16 || ex - cx > SCR_W + 8 || ey - cy < -16 || ey - cy > SCR_H + 16) continue;
        act[na++] = (Act){ex - cx, ey - cy, (ey + 7) >> 3, (int8_t)i};
    }
    for(int ty = ty0 - 1; ty <= ty0 + 10; ty++) {
        for(int tx = tx0 - 2; tx <= tx0 + 16; tx++)
            prop(fb, g, tx, ty, tx * TILE - cx, ty * TILE - cy, dark);
        for(int a = 0; a < na; a++) {
            if(act[a].row != ty) continue;
            int sx = act[a].x, sy = act[a].y;
            if(act[a].idx < 0) {
                draw_hero(fb, g, sx, sy);
                draw_slash(fb, g, sx, sy);
            } else
                draw_ent(fb, g, &g->ents[act[a].idx], act[a].idx, sx, sy);
            // wading through tall grass
            int wx = (sx + cx + 4) >> 3, wy = (sy + cy + 4) >> 3;
            const Ent* e = act[a].idx < 0 ? &g->pl : &g->ents[act[a].idx];
            if(tile(g, wx, wy) == T_TALL && !(e->type == E_SHADE))
                blit(fb, &SPR_TALLFRONT, wx * TILE - cx, wy * TILE - cy + 4, B_INK);
        }
    }

    draw_parts(fb, g, cx, cy, dark);

    if(dark) {
        int r = LAMP_R[g->lamp] + (int)(lf_hash(9, g->tick / 4, 0, 0) % 2);
        add_light(hx - cx + 4, hy - cy + 3, r);
        night_pass(fb, dark);
        if(dark >= 12) // fireflies over the long grass
            for(int ty = ty0; ty <= ty0 + 8; ty++)
                for(int tx = tx0; tx <= tx0 + 16; tx++) {
                    if(tile(g, tx, ty) != T_TALL) continue;
                    uint32_t h = lf_hash(g->seed, tx, ty, 21);
                    if(h % 5) continue;
                    int ph = (int)((g->tick + h) % 64);
                    if(ph > 40) continue;
                    int fx = tx * TILE - cx + (int)(h >> 8) % 8 + (ph / 8) % 3 - 1,
                        fy = ty * TILE - cy + (int)(h >> 12) % 6 - ph / 16;
                    px(fb, fx, fy, C_WHITE);
                }
    }
    if(g->rain && (g->opt & OPT_RAIN)) draw_rain(fb, g);
    if((g->bolt == 3 || g->bolt == 1) && (g->opt & OPT_FLASH))
        for(int i = 0; i < FB_SIZE; i++)
            fb[i] ^= 0xff;

    hud(fb, g);

    if(g->ui == UI_DEAD) {
        for(int i = 0; i < FB_SIZE; i++)
            fb[i] |= ((i >> 4) & 1) ? 0x55 : 0xaa;
        panel(fb, 14, 14, 100, 36);
        frame(fb, 16, 16, 96, 32);
        text_c(fb, 64, 20, "YOU FELL", 2, false);
        text_c(fb, 64, 33, g->dlg_text, 1, false);
        text_c(fb, 64, 41, "OK: WAKE BY THE WELL", 1, false);
    }
}
