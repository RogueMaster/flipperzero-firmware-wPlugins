/*
 * Lettercade (ASCII Arcade) - Flipper Zero
 *
 * Port of the three ESP32-clock ASCII games:
 *   Letters Rain, Snake Letters, Tennis Letters
 *
 * Playfield: 32 x 9 character cells (4 x 6 px each), drawn with a built-in
 * 3x5 pixel font so the 128x64 screen keeps the ASCII-terminal look.
 *
 * Custom word list (optional): put words, separated by spaces/commas/new
 * lines, in  SD Card/apps_data/lettercade/words.txt  (a-z, 2-15 letters,
 * up to 24 words).
 */

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/widget.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <input/input.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lettercade_icons.h"

/* ------------------------------------------------------------------ */
/* constants                                                           */
/* ------------------------------------------------------------------ */
#define COLS       32
#define ROWS       9
#define CELL_W     4
#define CELL_H     6
#define FIELD_Y    10
#define MAXENT     12
#define MAXLEN     200
#define MAXDIS     10
#define PMIN       3
#define PMAX       10
#define WORDS_MAX  24
#define WORD_LEN   15
#define TICK_MS    20
#define SAVE_MAGIC 0x4C544302u

#define CX(c) ((c) * CELL_W)
#define CY(r) (FIELD_Y + (r) * CELL_H)

typedef enum {
    GameRain = 0,
    GameSnake = 1,
    GameTennis = 2
} GameId;

typedef enum {
    ViewCover,
    ViewMain,
    ViewGameMenu,
    ViewSetRain,
    ViewSetSnake,
    ViewSetTennis,
    ViewSetApp,
    ViewReset,
    ViewAbout,
    ViewGame,
} ViewId;

/* ------------------------------------------------------------------ */
/* settings + saved data                                               */
/* ------------------------------------------------------------------ */
static const uint16_t FALL_MS[7] = {1200, 1000, 850, 700, 600, 500, 400};
static const uint16_t SNAKE_MS[7] = {900, 750, 600, 480, 380, 300, 200};
static const uint16_t BALL_MS[7] = {900, 700, 500, 400, 300, 200, 120};

/* every value is stored as a small index (see Opt table below) */
typedef struct {
    uint8_t sound;
    uint8_t rain_fall; /* index into FALL_MS            (default 850 ms) */
    uint8_t rain_paddle; /* size = idx + 3                (default 6)     */
    uint8_t rain_decoys; /* decoys = idx + 1              (default 4)     */
    uint8_t snake_speed; /* index into SNAKE_MS           (default 600)   */
    uint8_t snake_dis; /* distractions 0..10            (default 3)     */
    uint8_t snake_vw; /* side walls                    (default on)    */
    uint8_t snake_hw; /* top/bottom walls              (default off)   */
    uint8_t tn_size; /* size = idx + 1                (default 6)     */
    uint8_t tn_ball; /* index into BALL_MS            (default 500)   */
} Settings;

static const Settings DEFAULTS = {1, 2, 3, 3, 2, 3, 1, 0, 5, 2};

typedef struct {
    uint32_t magic;
    Settings s;
    uint16_t best[6]; /* [game*2 + mode]; Rain/Alphabet stores "last" */
} Saved;

typedef struct {
    char w[WORDS_MAX][WORD_LEN + 1];
    uint8_t n;
} Words;

/* ------------------------------------------------------------------ */
/* 3x5 pixel font                                                      */
/* ------------------------------------------------------------------ */
#define G5(a, b, c, d, e) ((uint16_t)(((a) << 12) | ((b) << 9) | ((c) << 6) | ((d) << 3) | (e)))
static const uint16_t GLY[26] = {
    G5(2, 5, 7, 5, 5), /* A */
    G5(6, 5, 6, 5, 6), /* B */
    G5(3, 4, 4, 4, 3), /* C */
    G5(6, 5, 5, 5, 6), /* D */
    G5(7, 4, 6, 4, 7), /* E */
    G5(7, 4, 6, 4, 4), /* F */
    G5(3, 4, 5, 5, 3), /* G */
    G5(5, 5, 7, 5, 5), /* H */
    G5(7, 2, 2, 2, 7), /* I */
    G5(1, 1, 1, 5, 2), /* J */
    G5(5, 5, 6, 5, 5), /* K */
    G5(4, 4, 4, 4, 7), /* L */
    G5(5, 7, 7, 5, 5), /* M */
    G5(7, 5, 5, 5, 5), /* N */
    G5(2, 5, 5, 5, 2), /* O */
    G5(7, 5, 7, 4, 4), /* P */
    G5(2, 5, 5, 7, 3), /* Q */
    G5(6, 5, 6, 5, 5), /* R */
    G5(3, 4, 2, 1, 6), /* S */
    G5(7, 2, 2, 2, 2), /* T */
    G5(5, 5, 5, 5, 7), /* U */
    G5(5, 5, 5, 5, 2), /* V */
    G5(5, 5, 7, 7, 5), /* W */
    G5(5, 5, 2, 5, 5), /* X */
    G5(5, 5, 2, 2, 2), /* Y */
    G5(7, 1, 2, 4, 7), /* Z */
};

static uint16_t glyph_bits(char ch) {
    if(ch >= 'A' && ch <= 'Z') return GLY[ch - 'A'];
    switch(ch) {
    case '+':
        return G5(0, 2, 7, 2, 0);
    case '-':
        return G5(0, 0, 7, 0, 0);
    case '*':
        return G5(5, 2, 7, 2, 5);
    case '/':
        return G5(1, 1, 2, 4, 4);
    case '_':
        return G5(0, 0, 0, 0, 7);
    default:
        return 0;
    }
}

static void draw_glyph(Canvas* canvas, int x, int y, char ch) {
    uint16_t b = glyph_bits(ch);
    for(int r = 0; r < 5; r++) {
        uint8_t row = (b >> (12 - 3 * r)) & 7;
        for(int q = 0; q < 3; q++) {
            if(row & (4 >> q)) canvas_draw_dot(canvas, x + q, y + r);
        }
    }
}

/* ------------------------------------------------------------------ */
/* sound (tiny non-blocking sequencer, driven from the tick)           */
/* ------------------------------------------------------------------ */
typedef struct {
    uint16_t f;
    uint16_t ms;
} Note;

static const Note N_BEEP_RAIN[] = {{600, 50}};
static const Note N_BEEP_SNAKE[] = {{600, 40}};
static const Note N_BEEP_TENNIS[] = {{800, 40}};
static const Note N_OK[] = {{523, 120}, {659, 120}, {784, 120}, {1046, 120}};
static const Note N_BAD[] = {{392, 180}, {330, 180}, {262, 180}};

static struct {
    const Note* seq;
    uint8_t n, i;
    int32_t left;
    bool on, held;
} S;
static bool g_sound = true;

static void snd_stop(void) {
    if(S.held) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }
    S.held = false;
    S.on = false;
}

static void snd_play(const Note* seq, uint8_t n) {
    if(!g_sound) return;
    if(S.on && S.n > 1 && n == 1) return; /* don't cut a tune with a beep */
    if(!S.held) S.held = furi_hal_speaker_acquire(30);
    if(!S.held) return;
    S.seq = seq;
    S.n = n;
    S.i = 0;
    S.left = seq[0].ms;
    S.on = true;
    furi_hal_speaker_start(seq[0].f, 0.5f);
}

static void snd_tick(uint32_t dt) {
    if(!S.on) return;
    S.left -= (int32_t)dt;
    if(S.left > 0) return;
    S.i++;
    if(S.i < S.n) {
        S.left += S.seq[S.i].ms;
        furi_hal_speaker_start(S.seq[S.i].f, 0.5f);
    } else {
        snd_stop();
    }
}

/* ------------------------------------------------------------------ */
/* game state                                                          */
/* ------------------------------------------------------------------ */
typedef struct {
    uint8_t c, r;
    char ch;
} Ent;

typedef enum {
    StRun,
    StPause,
    StEnding,
    StOver
} GState;

typedef struct {
    uint8_t game, mode;
    GState st;
    int32_t ending_ms;
    bool end_ok, beat, won, dirty;
    uint16_t score, best, prev_best;
    uint32_t fa, pa;
    uint32_t fall_ms, pad_ms; /* rain: fall / paddle;  tennis: ball / paddle;  snake: speed */
    uint32_t start_ms;
    bool kl, kr;
    int8_t mv;
    Settings cfg;
    Words* words;
    const Note* beep;
    uint8_t beep_n;

    /* paddle games */
    int8_t px;
    uint8_t size;

    /* rain */
    Ent ents[MAXENT];
    uint8_t nents, ti, lastcol, star, rt, wi;
    int8_t modc;

    /* words (rain + snake) */
    char word[WORD_LEN + 1];
    uint8_t wlen, blank;

    /* snake */
    uint8_t bc[MAXLEN], br[MAXLEN];
    uint16_t blen, seq;
    uint8_t dir, q[2], qn;
    Ent t;
    char tch;
    Ent dis[MAXDIS];
    uint8_t ndis;
    bool hasm;
    Ent m;
    int32_t modT;

    /* tennis */
    int8_t bx, by, dx, dy;
    uint8_t li;
} Game;

typedef struct {
    Game g;
} GameModel;

static uint32_t rnd(uint32_t n) {
    return furi_hal_random_get() % n;
}

static void sfx_beep(Game* g) {
    snd_play(g->beep, g->beep_n);
}
static void sfx_ok(void) {
    snd_play(N_OK, 4);
}

static void game_finish(Game* g, bool ok) {
    g->st = StEnding;
    g->ending_ms = 500;
    g->end_ok = ok;
}

/* ===================== LETTERS RAIN ===================== */
static char r_target(Game* g) {
    return g->mode ? g->word[g->ti] : (char)('A' + g->ti);
}

static uint32_t chbit(char ch) {
    if(ch >= 'A' && ch <= 'Z') return 1u << (ch - 'A');
    if(ch == '+') return 1u << 26;
    if(ch == '-') return 1u << 27;
    if(ch == '*') return 1u << 28;
    return 0;
}

static void r_usage(Game* g, bool* used, uint32_t* chm) {
    memset(used, 0, COLS);
    *chm = 0;
    for(uint8_t i = 0; i < g->nents; i++) {
        used[g->ents[i].c] = true;
        *chm |= chbit(g->ents[i].ch);
    }
}

static int pick_col(const bool* used, int near, int lo, int hi) {
    uint8_t cand[COLS];
    uint8_t n = 0;
    for(int i = 1; i < COLS - 1; i++) {
        if(used[i]) continue;
        if(near >= 0) {
            int d = abs(i - near);
            if(d < lo || d > hi) continue;
        }
        cand[n++] = i;
    }
    if(!n && near >= 0) {
        for(int i = 1; i < COLS - 1; i++)
            if(!used[i]) cand[n++] = i;
    }
    return n ? cand[rnd(n)] : -1;
}

static void r_add(Game* g, int c, int r, char ch) {
    if(g->nents >= MAXENT) return;
    g->ents[g->nents].c = c;
    g->ents[g->nents].r = r;
    g->ents[g->nents].ch = ch;
    g->nents++;
}

static void r_wave(Game* g) {
    bool used[COLS];
    uint32_t chm;
    r_usage(g, used, &chm);
    char t = r_target(g);
    int tc = pick_col(used, g->lastcol, 5, 15);
    if(tc < 0) return;
    r_add(g, tc, 1, t);
    used[tc] = true;
    g->lastcol = tc;
    int decoys = g->cfg.rain_decoys + 1;
    for(int d = 0; d < decoys && g->nents < MAXENT; d++) {
        char pool[26];
        uint8_t n = 0;
        for(int l = 0; l < 26; l++) {
            char L = 'A' + l;
            if(L != t && !(chm & (1u << l))) pool[n++] = L;
        }
        int dc = pick_col(used, -1, 0, 0);
        if(!n || dc < 0) break;
        char p = pool[rnd(n)];
        r_add(g, dc, 1 + rnd(4), p);
        used[dc] = true;
        chm |= 1u << (p - 'A');
    }
    g->modc = 3 + rnd(3);
}

static void r_trickle(Game* g) {
    if(g->nents >= MAXENT) return;
    bool used[COLS];
    uint32_t chm;
    r_usage(g, used, &chm);
    char t = r_target(g);
    char pool[26];
    uint8_t n = 0;
    for(int l = 0; l < 26; l++) {
        char L = 'A' + l;
        if(L != t && !(chm & (1u << l))) pool[n++] = L;
    }
    int c = pick_col(used, -1, 0, 0);
    if(!n || c < 0) return;
    r_add(g, c, 1, pool[rnd(n)]);
}

static void r_mods(Game* g) {
    bool used[COLS];
    uint32_t chm;
    r_usage(g, used, &chm);
    const char mods[2] = {'+', '-'};
    for(int i = 0; i < 2; i++) {
        if((chm & chbit(mods[i])) || g->nents >= MAXENT) continue;
        int c = pick_col(used, -1, 0, 0);
        if(c < 0) break;
        r_add(g, c, 1, mods[i]);
        used[c] = true;
        chm |= chbit(mods[i]);
    }
    if(++g->star >= 20 && !(chm & chbit('*')) && g->nents < MAXENT) {
        int c = pick_col(used, -1, 0, 0);
        if(c >= 0) {
            r_add(g, c, 1, '*');
            g->star = 0;
        }
    }
    g->modc = -1;
}

static void r_ramp(Game* g) {
    uint32_t floor_ms = FALL_MS[g->cfg.rain_fall] < 600 ? FALL_MS[g->cfg.rain_fall] : 600;
    g->fall_ms = (g->fall_ms > floor_ms + 10) ? g->fall_ms - 10 : floor_ms;
    g->pad_ms = (g->pad_ms > 55) ? g->pad_ms - 5 : 50;
}

static void set_size(Game* g, int n) {
    if(n < PMIN) n = PMIN;
    if(n > PMAX) n = PMAX;
    g->size = n;
    if(g->px > COLS - 1 - g->size) g->px = COLS - 1 - g->size;
}

static void load_word(Game* g) {
    const char* w = g->words->w[g->wi % g->words->n];
    strncpy(g->word, w, WORD_LEN);
    g->word[WORD_LEN] = 0;
    g->wlen = strlen(g->word);
    g->ti = 0;
}

static void r_end(Game* g, bool won) {
    bool ok = won;
    if(g->mode) {
        g->prev_best = g->best;
        if(g->score > g->best) {
            g->beat = true;
            g->best = g->score;
            g->dirty = true;
        }
        ok = won || g->beat;
    } else {
        g->best = g->score; /* "last" */
        g->dirty = true;
    }
    g->won = won;
    game_finish(g, ok);
}

static void r_fall(Game* g) {
    if(g->modc > 0 && --g->modc == 0) r_mods(g);
    for(uint8_t i = 0; i < g->nents; i++)
        g->ents[i].r++;
    Ent hit[MAXENT];
    uint8_t nh = 0, nk = 0;
    Ent keep[MAXENT];
    for(uint8_t i = 0; i < g->nents; i++) {
        if(g->ents[i].r >= ROWS - 1)
            hit[nh++] = g->ents[i];
        else
            keep[nk++] = g->ents[i];
    }
    memcpy(g->ents, keep, nk * sizeof(Ent));
    g->nents = nk;
    char tch = r_target(g);
    for(uint8_t i = 0; i < nh; i++) {
        Ent e = hit[i];
        bool caught = e.c >= g->px && e.c < g->px + g->size;
        bool is_t = (e.ch == tch);
        if(caught) {
            if(is_t) {
                r_ramp(g);
                if(g->mode) {
                    g->ti++;
                    if(g->ti >= g->wlen) {
                        g->score++;
                        sfx_ok();
                        g->wi++;
                        load_word(g);
                    } else {
                        sfx_beep(g);
                    }
                } else {
                    g->score++;
                    sfx_beep(g);
                }
                if(!g->mode && g->score >= 26) {
                    r_end(g, true);
                    return;
                }
                if(!g->mode) g->ti++;
                char nt = r_target(g);
                uint8_t k = 0;
                for(uint8_t j = 0; j < g->nents; j++)
                    if(g->ents[j].ch != nt) g->ents[k++] = g->ents[j];
                g->nents = k;
                r_wave(g);
                return; /* the rest of this row's hits were already collected; state changed */
            } else if(e.ch == '+') {
                set_size(g, g->size + 1);
            } else if(e.ch == '-') {
                set_size(g, g->size - 1);
            } else if(e.ch == '*') {
                set_size(g, g->cfg.rain_paddle + 3);
            } else {
                set_size(g, g->size - 1);
            }
        } else if(is_t) {
            r_end(g, false);
            return;
        }
    }
    if(g->st == StRun && ++g->rt >= 3) {
        g->rt = 0;
        r_trickle(g);
    }
}

/* ===================== SNAKE LETTERS ===================== */
#define F_AHEAD 1
#define F_T     2
#define F_DIS   4
#define F_MOD   8

static const int8_t DX[4] = {0, 0, -1, 1};
static const int8_t DY[4] = {-1, 1, 0, 0};
static const uint8_t OPP[4] = {1, 0, 3, 2};

static int cmin_(Game* g) {
    return g->cfg.snake_vw ? 1 : 0;
}
static int cmax_(Game* g) {
    return g->cfg.snake_vw ? COLS - 2 : COLS - 1;
}
static int rmin_(Game* g) {
    return g->cfg.snake_hw ? 1 : 0;
}
static int rmax_(Game* g) {
    return g->cfg.snake_hw ? ROWS - 2 : ROWS - 1;
}

static bool s_on_body(Game* g, int c, int r) {
    for(uint16_t i = 0; i < g->blen; i++)
        if(g->bc[i] == c && g->br[i] == r) return true;
    return false;
}

static bool s_ahead(Game* g, int c, int r, int gap) {
    int hc = g->bc[0], hr = g->br[0];
    switch(g->dir) {
    case 3:
        return r == hr && c > hc && c - hc <= gap;
    case 2:
        return r == hr && c < hc && hc - c <= gap;
    case 1:
        return c == hc && r > hr && r - hr <= gap;
    default:
        return c == hc && r < hr && hr - r <= gap;
    }
}

static bool s_bad(Game* g, int c, int r, int flags) {
    if(s_on_body(g, c, r)) return true;
    if((flags & F_AHEAD) && s_ahead(g, c, r, 4)) return true;
    if((flags & F_T) && g->t.c == c && g->t.r == r) return true;
    if((flags & F_MOD) && g->hasm && g->m.c == c && g->m.r == r) return true;
    if(flags & F_DIS) {
        for(uint8_t i = 0; i < g->ndis; i++)
            if(g->dis[i].c == c && g->dis[i].r == r) return true;
    }
    return false;
}

static void s_place(Game* g, int flags, uint8_t* oc, uint8_t* orr) {
    int c = 0, r = 0;
    for(int n = 0; n < 300; n++) {
        c = cmin_(g) + rnd(cmax_(g) - cmin_(g) + 1);
        r = rmin_(g) + rnd(rmax_(g) - rmin_(g) + 1);
        if(!s_bad(g, c, r, flags)) break;
    }
    *oc = c;
    *orr = r;
}

static void s_next_target(Game* g) {
    if(g->mode) {
        const char* w = g->words->w[g->seq % g->words->n];
        strncpy(g->word, w, WORD_LEN);
        g->word[WORD_LEN] = 0;
        g->wlen = strlen(g->word);
        g->blank = rnd(g->wlen);
        g->tch = g->word[g->blank];
    } else {
        g->tch = 'A' + (g->seq % 26);
    }
}

static void s_spawn_letters(Game* g) {
    s_next_target(g);
    g->ndis = 0;
    s_place(g, F_AHEAD | F_MOD, &g->t.c, &g->t.r);
    int want = (g->mode || g->score >= 10) ? g->cfg.snake_dis : 0;
    if(want > MAXDIS) want = MAXDIS;
    char pool[26];
    uint8_t n = 0;
    for(int l = 0; l < 26; l++)
        if('A' + l != g->tch) pool[n++] = 'A' + l;
    for(int i = n - 1; i > 0; i--) {
        int j = rnd(i + 1);
        char tmp = pool[i];
        pool[i] = pool[j];
        pool[j] = tmp;
    }
    for(int d = 0; d < want; d++) {
        s_place(g, F_AHEAD | F_T | F_MOD | F_DIS, &g->dis[d].c, &g->dis[d].r);
        g->dis[d].ch = pool[d];
        g->ndis = d + 1;
    }
}

static int32_t s_mod_wait(void) {
    return (10 + rnd(21)) * 1000;
}

static void s_end(Game* g) {
    if(g->score > g->best) {
        g->beat = true;
        g->won = true;
        g->best = g->score;
        g->dirty = true;
    }
    game_finish(g, g->beat || (g->mode == 0 && g->seq >= 26));
}

static void s_steer(Game* g, int d) {
    int last = g->qn ? g->q[g->qn - 1] : g->dir;
    if(d == last || d == OPP[last] || g->qn >= 2) return;
    g->q[g->qn++] = d;
}

static void s_move(Game* g) {
    if(g->qn) {
        g->dir = g->q[0];
        g->q[0] = g->q[1];
        g->qn--;
    }
    int nx = g->bc[0] + DX[g->dir], ny = g->br[0] + DY[g->dir];
    if(g->cfg.snake_hw) {
        if(ny < rmin_(g) || ny > rmax_(g)) return s_end(g);
    } else {
        ny = (ny + ROWS) % ROWS;
    }
    if(g->cfg.snake_vw) {
        if(nx < cmin_(g) || nx > cmax_(g)) return s_end(g);
    } else {
        nx = (nx + COLS) % COLS;
    }
    for(uint16_t i = 0; i + 1 < g->blen; i++)
        if(g->bc[i] == nx && g->br[i] == ny) return s_end(g);
    for(uint8_t i = 0; i < g->ndis; i++)
        if(g->dis[i].c == nx && g->dis[i].r == ny) return s_end(g);
    bool ate_t = (nx == g->t.c && ny == g->t.r);
    bool ate_m = g->hasm && nx == g->m.c && ny == g->m.r;
    bool grow = ate_t && g->blen < MAXLEN;
    if(grow) g->blen++;
    memmove(g->bc + 1, g->bc, g->blen - 1);
    memmove(g->br + 1, g->br, g->blen - 1);
    g->bc[0] = nx;
    g->br[0] = ny;
    if(ate_m) {
        char mch = g->m.ch;
        g->hasm = false;
        g->modT = s_mod_wait();
        uint16_t min = 3;
        if(mch == '-') {
            if(g->blen > min) g->blen--;
        } else {
            g->blen = (g->blen >> 1) < min ? min : (g->blen >> 1);
        }
        sfx_beep(g);
    }
    if(ate_t) {
        g->score++;
        g->seq++;
        uint32_t floor_ms = SNAKE_MS[g->cfg.snake_speed] < 200 ? SNAKE_MS[g->cfg.snake_speed] :
                                                                 200;
        g->fall_ms = (g->fall_ms > floor_ms + 5) ? g->fall_ms - 5 : floor_ms;
        bool cycle = g->mode == 0 && g->seq % 26 == 0;
        bool new_high = !g->beat && g->score > g->best;
        if(cycle || new_high) {
            g->won = true;
            if(new_high) {
                g->beat = true;
                g->best = g->score;
                g->dirty = true;
            }
            sfx_ok();
        } else {
            sfx_beep(g);
        }
        s_spawn_letters(g);
    }
}

/* ===================== TENNIS LETTERS ===================== */
static void t_ramp(Game* g) {
    uint32_t floor_ms = BALL_MS[g->cfg.tn_ball] < 200 ? BALL_MS[g->cfg.tn_ball] : 200;
    g->fall_ms = (g->fall_ms > floor_ms + 10) ? g->fall_ms - 10 : floor_ms;
    g->pad_ms = (g->pad_ms > 55) ? g->pad_ms - 5 : 50;
}

static int t_deflect(Game* g, int x) {
    float c = (g->size - 1) / 2.0f;
    int sp = g->mv;
    int cur = g->dx > 0 ? 1 : (g->dx < 0 ? -1 : 1);
    float o = c > 0 ? (x - g->px - c) / c : 0;
    if(c == 0 || fabsf(o) < 0.25f) return sp ? sp * (sp == cur ? 2 : 1) : cur;
    float m = fabsf(o);
    int side = o < 0 ? -1 : 1;
    int d = side * (m < 0.7f ? 1 : 2) + sp;
    if(d > 2) d = 2;
    if(d < -2) d = -2;
    return d ? d : side;
}

static void t_end(Game* g) {
    if(g->score > g->best) {
        g->best = g->score;
        g->dirty = true;
    }
    game_finish(g, g->beat || g->score >= 26);
}

static void t_ball(Game* g) {
    int nx = g->bx + g->dx, ny = g->by + g->dy;
    bool bounced = false;
    if(nx < 1) {
        nx = 2 - nx;
        g->dx = -g->dx;
        bounced = true;
    } else if(nx > COLS - 2) {
        nx = 2 * (COLS - 2) - nx;
        g->dx = -g->dx;
        bounced = true;
    }
    if(ny <= 0) {
        g->dy = -g->dy;
        t_ramp(g);
        ny = g->by + g->dy;
        bounced = true;
    }
    if(ny >= ROWS - 1) {
        if(nx >= g->px && nx < g->px + g->size) {
            g->dy = -g->dy;
            ny = g->by + g->dy;
            bounced = true;
            g->score++;
            if(g->mode) {
                g->dx = t_deflect(g, nx);
                nx = g->bx + g->dx;
                if(nx < 1) {
                    nx = 2 - nx;
                    g->dx = -g->dx;
                } else if(nx > COLS - 2) {
                    nx = 2 * (COLS - 2) - nx;
                    g->dx = -g->dx;
                }
            }
            bool alpha = g->score % 26 == 0;
            bool hi = !g->beat && g->score > g->best;
            if(hi) {
                g->beat = true;
                g->best = g->score;
                g->dirty = true;
                sfx_ok();
            } else if(alpha) {
                sfx_ok();
            }
            g->li = (g->li + 1) % 26;
        } else {
            g->bx = nx;
            g->by = ny;
            t_end(g);
            return;
        }
    }
    if(bounced) sfx_beep(g);
    g->bx = nx;
    g->by = ny;
}

/* ===================== shared paddle movement ===================== */
static void pad_step(Game* g, int d) {
    int lo = 1, hi = COLS - 1 - g->size;
    int np = g->px + d;
    if(np < lo) np = lo;
    if(np > hi) np = hi;
    g->px = np;
    g->mv = d > 0 ? 1 : (d < 0 ? -1 : 0);
}

static void pad_tick(Game* g) {
    int dir = (g->kr ? 1 : 0) - (g->kl ? 1 : 0);
    g->mv = dir;
    if(dir) pad_step(g, dir);
}

/* ===================== start / tick ===================== */
static void
    game_start(Game* g, const Settings* s, Words* words, uint8_t game, uint8_t mode, uint16_t best) {
    memset(g, 0, sizeof(*g));
    g->game = game;
    g->mode = mode;
    g->cfg = *s;
    g->words = words;
    g->best = best;
    g->st = StRun;
    g->modc = -1;
    switch(game) {
    case GameRain:
        g->beep = N_BEEP_RAIN;
        g->beep_n = 1;
        g->size = s->rain_paddle + 3;
        g->fall_ms = FALL_MS[s->rain_fall];
        g->pad_ms = 150;
        g->lastcol = COLS >> 1;
        g->px = (COLS - g->size) / 2;
        if(mode) load_word(g);
        r_wave(g);
        break;
    case GameSnake: {
        g->beep = N_BEEP_SNAKE;
        g->beep_n = 1;
        g->fall_ms = SNAKE_MS[s->snake_speed];
        int mc = (cmin_(g) + cmax_(g)) >> 1, mr = (rmin_(g) + rmax_(g)) >> 1;
        g->blen = 3;
        for(int i = 0; i < 3; i++) {
            g->bc[i] = mc - i < cmin_(g) ? cmin_(g) : mc - i;
            g->br[i] = mr;
        }
        g->dir = 3;
        g->modT = s_mod_wait();
        s_spawn_letters(g);
        break;
    }
    default:
        g->beep = N_BEEP_TENNIS;
        g->beep_n = 1;
        g->size = s->tn_size + 1;
        g->fall_ms = BALL_MS[s->tn_ball];
        g->pad_ms = 150;
        g->bx = COLS >> 1;
        g->by = ROWS >> 1;
        g->dx = rnd(2) ? 1 : -1;
        g->dy = 1;
        g->px = (COLS - g->size) >> 1;
        break;
    }
}

static void game_tick(Game* g, uint32_t dt) {
    if(g->st == StEnding) {
        g->ending_ms -= (int32_t)dt;
        if(g->ending_ms <= 0) {
            g->st = StOver;
            if(g->end_ok)
                snd_play(N_OK, 4);
            else
                snd_play(N_BAD, 3);
        }
        return;
    }
    if(g->st != StRun) return;

    g->fa += dt;
    g->pa += dt;
    switch(g->game) {
    case GameRain:
        while(g->st == StRun && g->fa >= g->fall_ms) {
            g->fa -= g->fall_ms;
            r_fall(g);
        }
        while(g->st == StRun && g->pa >= g->pad_ms) {
            g->pa -= g->pad_ms;
            pad_tick(g);
        }
        break;
    case GameSnake:
        while(g->st == StRun && g->fa >= g->fall_ms) {
            g->fa -= g->fall_ms;
            s_move(g);
        }
        if(g->st == StRun) {
            g->modT -= (int32_t)dt;
            if(g->modT <= 0) {
                if(g->hasm) {
                    g->hasm = false;
                } else {
                    s_place(g, F_T | F_DIS, &g->m.c, &g->m.r);
                    g->m.ch = rnd(2) ? '-' : '/';
                    g->hasm = true;
                }
                g->modT = s_mod_wait();
            }
        }
        break;
    default:
        while(g->st == StRun && g->fa >= g->fall_ms) {
            g->fa -= g->fall_ms;
            t_ball(g);
        }
        while(g->st == StRun && g->pa >= g->pad_ms) {
            g->pa -= g->pad_ms;
            pad_tick(g);
        }
        break;
    }
}

/* ------------------------------------------------------------------ */
/* drawing                                                             */
/* ------------------------------------------------------------------ */
static void hud_center(Canvas* canvas, const char* s, int hl) {
    int w = canvas_string_width(canvas, s);
    int x0 = (128 - w) / 2;
    if(hl < 0 || hl >= (int)strlen(s)) {
        canvas_draw_str(canvas, x0, 7, s);
        return;
    }
    char pre[WORD_LEN + 2];
    memcpy(pre, s, hl);
    pre[hl] = 0;
    char cur[2] = {s[hl], 0};
    int pw = canvas_string_width(canvas, pre);
    int cw = canvas_string_width(canvas, cur);
    canvas_draw_str(canvas, x0, 7, pre);
    canvas_draw_box(canvas, x0 + pw - 1, 0, cw + 2, 9);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, x0 + pw, 7, cur);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_str(canvas, x0 + pw + cw, 7, s + hl + 1);
}

static void draw_hud(Canvas* canvas, Game* g) {
    char buf[24];
    canvas_set_font(canvas, FontSecondary);
    snprintf(buf, sizeof(buf), "S:%u", g->score);
    canvas_draw_str(canvas, 0, 7, buf);
    const char* lbl = (g->game == GameRain && g->mode == 0) ? "L" : "B";
    snprintf(buf, sizeof(buf), "%s:%u", lbl, g->best);
    canvas_draw_str_aligned(canvas, 127, 7, AlignRight, AlignBottom, buf);
    if(g->game == GameRain) {
        if(g->mode)
            hud_center(canvas, g->word, g->ti);
        else {
            char t[2] = {r_target(g), 0};
            hud_center(canvas, t, 0);
        }
    } else if(g->game == GameSnake) {
        if(g->mode) {
            char w[WORD_LEN + 1];
            strncpy(w, g->word, WORD_LEN);
            w[WORD_LEN] = 0;
            if(g->blank < g->wlen) w[g->blank] = '_';
            hud_center(canvas, w, g->blank);
        } else {
            char t[2] = {g->tch, 0};
            hud_center(canvas, t, 0);
        }
    }
    canvas_draw_line(canvas, 0, 9, 127, 9);
}

static void draw_frame(Canvas* canvas, bool top, bool sides, bool bottom) {
    int y0 = CY(0) + 2, y1 = CY(ROWS - 1) + 2;
    if(top) canvas_draw_line(canvas, 0, y0, 127, y0);
    if(bottom) canvas_draw_line(canvas, 0, y1, 127, y1);
    if(sides) {
        canvas_draw_line(canvas, 1, y0, 1, bottom ? y1 : 63);
        canvas_draw_line(canvas, 126, y0, 126, bottom ? y1 : 63);
    }
}

static void draw_paddle(Canvas* canvas, Game* g) {
    canvas_draw_box(canvas, CX(g->px), CY(ROWS - 1) + 2, g->size * CELL_W - 1, 3);
}

static void draw_overlay(Canvas* canvas, Game* g) {
    char l1[32], l2[32];
    const char* title;
    if(g->st == StPause) {
        title = "Paused";
        snprintf(l1, sizeof(l1), "Score: %u", g->score);
        snprintf(l2, sizeof(l2), "OK resume   Back menu");
    } else {
        if(g->game == GameRain && g->won)
            title = "You win!";
        else if(g->beat)
            title = (g->game == GameTennis) ? "New high score!" : "New best!";
        else
            title = "Game over";
        const char* unit = (g->game != GameTennis && g->mode) ? "words" : "letters";
        const char* lbl = (g->game == GameRain && g->mode == 0) ? "Last" : "Best";
        snprintf(l1, sizeof(l1), "%u %s  %s %u", g->score, unit, lbl, g->best);
        snprintf(l2, sizeof(l2), "OK again   Back menu");
    }
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 8, 14, 112, 38);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, 8, 14, 112, 38, 3);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 27, AlignCenter, AlignBottom, title);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignBottom, l1);
    canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignBottom, l2);
}

static void game_draw(Canvas* canvas, void* model) {
    GameModel* m = model;
    Game* g = &m->g;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    draw_hud(canvas, g);

    switch(g->game) {
    case GameRain:
        draw_frame(canvas, true, true, false);
        for(uint8_t i = 0; i < g->nents; i++)
            draw_glyph(canvas, CX(g->ents[i].c), CY(g->ents[i].r), g->ents[i].ch);
        draw_paddle(canvas, g);
        break;
    case GameSnake: {
        draw_frame(canvas, g->cfg.snake_hw, g->cfg.snake_vw, g->cfg.snake_hw);
        for(uint8_t i = 0; i < g->ndis; i++)
            draw_glyph(canvas, CX(g->dis[i].c), CY(g->dis[i].r), g->dis[i].ch);
        if(g->hasm) draw_glyph(canvas, CX(g->m.c), CY(g->m.r), g->m.ch);
        draw_glyph(canvas, CX(g->t.c), CY(g->t.r), g->tch);
        for(uint16_t i = g->blen; i-- > 0;) {
            if(i == 0)
                canvas_draw_box(canvas, CX(g->bc[0]), CY(g->br[0]), 3, 5);
            else
                canvas_draw_box(canvas, CX(g->bc[i]), CY(g->br[i]) + 1, 3, 3);
        }
        break;
    }
    default:
        draw_frame(canvas, true, true, false);
        draw_paddle(canvas, g);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, CX(g->bx), CY(g->by), 3, 5);
        canvas_set_color(canvas, ColorBlack);
        draw_glyph(canvas, CX(g->bx), CY(g->by), (char)('A' + g->li));
        break;
    }
    if(g->st == StPause || g->st == StOver) draw_overlay(canvas, g);
}

/* ------------------------------------------------------------------ */
/* app                                                                 */
/* ------------------------------------------------------------------ */
typedef enum {
    OptNum,
    OptOnOff,
    OptMs
} OptKind;

typedef struct App App;

typedef struct {
    App* app;
    uint8_t* val;
    uint8_t base;
    OptKind kind;
    const uint16_t* table;
} Opt;

struct App {
    Gui* gui;
    ViewDispatcher* vd;
    NotificationApp* notif;
    Storage* storage;
    Submenu* main_menu;
    Submenu* game_menu;
    VariableItemList* set_rain;
    VariableItemList* set_snake;
    VariableItemList* set_tennis;
    VariableItemList* set_app;
    DialogEx* reset_dlg;
    Widget* about;
    View* cover;
    View* game_view;
    Saved save;
    Words words;
    Opt opts[10];
    uint8_t cur_game;
    bool in_game;
    uint32_t last_tick;
};

static const char* GAME_NAME[3] = {"Letters Rain", "Snake Letters", "Tennis Letters"};
static const char* MODE_NAME[3][2] = {
    {"Alphabet", "Words"},
    {"Alphabet", "Words"},
    {"Easy", "Hard"}};

/* ----- persistence ----- */
static void save_write(App* app) {
    File* f = storage_file_alloc(app->storage);
    if(storage_file_open(f, APP_DATA_PATH("lettercade.save"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(f, &app->save, sizeof(Saved));
    }
    storage_file_close(f);
    storage_file_free(f);
}

static void save_load(App* app) {
    app->save.magic = SAVE_MAGIC;
    app->save.s = DEFAULTS;
    memset(app->save.best, 0, sizeof(app->save.best));
    File* f = storage_file_alloc(app->storage);
    if(storage_file_open(f, APP_DATA_PATH("lettercade.save"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        Saved tmp;
        if(storage_file_read(f, &tmp, sizeof(Saved)) == sizeof(Saved) && tmp.magic == SAVE_MAGIC) {
            app->save = tmp;
        }
    }
    storage_file_close(f);
    storage_file_free(f);
    /* clamp everything so a damaged file can't crash the game */
    Settings* s = &app->save.s;
    s->sound = s->sound ? 1 : 0;
    if(s->rain_fall > 6) s->rain_fall = DEFAULTS.rain_fall;
    if(s->rain_paddle > 7) s->rain_paddle = DEFAULTS.rain_paddle;
    if(s->rain_decoys > 7) s->rain_decoys = DEFAULTS.rain_decoys;
    if(s->snake_speed > 6) s->snake_speed = DEFAULTS.snake_speed;
    if(s->snake_dis > MAXDIS) s->snake_dis = DEFAULTS.snake_dis;
    s->snake_vw = s->snake_vw ? 1 : 0;
    s->snake_hw = s->snake_hw ? 1 : 0;
    if(s->tn_size > 9) s->tn_size = DEFAULTS.tn_size;
    if(s->tn_ball > 6) s->tn_ball = DEFAULTS.tn_ball;
}

/* ----- words ----- */
static void words_load(App* app) {
    static const char* DEF[] = {"BOX", "CAT", "DOG", "CACTUS", "RAINBOW"};
    Words* w = &app->words;
    memset(w, 0, sizeof(*w));
    File* f = storage_file_alloc(app->storage);
    if(storage_file_open(f, APP_DATA_PATH("words.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buf[512];
        size_t n = storage_file_read(f, buf, sizeof(buf) - 1);
        buf[n] = 0;
        char cur[WORD_LEN + 1];
        uint8_t len = 0;
        for(size_t i = 0; i <= n; i++) {
            char c = buf[i];
            bool letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
            if(letter) {
                if(len < WORD_LEN) cur[len++] = (c >= 'a') ? c - 32 : c;
            } else {
                if(len >= 2 && w->n < WORDS_MAX) {
                    memcpy(w->w[w->n], cur, len);
                    w->w[w->n][len] = 0;
                    w->n++;
                }
                len = 0;
            }
        }
    }
    storage_file_close(f);
    storage_file_free(f);
    if(!w->n) {
        for(uint8_t i = 0; i < 5; i++)
            strcpy(w->w[i], DEF[i]);
        w->n = 5;
    }
}

/* ----- settings UI ----- */
static void opt_text(VariableItem* item, Opt* o, uint8_t idx) {
    char b[16];
    switch(o->kind) {
    case OptNum:
        snprintf(b, sizeof(b), "%u", idx + o->base);
        break;
    case OptOnOff:
        snprintf(b, sizeof(b), "%s", idx ? "On" : "Off");
        break;
    default:
        snprintf(b, sizeof(b), "%u ms", o->table[idx]);
        break;
    }
    variable_item_set_current_value_text(item, b);
}

static void opt_changed(VariableItem* item) {
    Opt* o = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    *o->val = idx;
    opt_text(item, o, idx);
    if(o->val == &o->app->save.s.sound) g_sound = idx;
    save_write(o->app);
}

static void add_opt(VariableItemList* list, const char* name, Opt* o, uint8_t count) {
    VariableItem* item = variable_item_list_add(list, name, count, opt_changed, o);
    variable_item_set_current_value_index(item, *o->val);
    opt_text(item, o, *o->val);
}

static void
    set_opt(App* a, uint8_t slot, uint8_t* val, uint8_t base, OptKind kind, const uint16_t* table) {
    a->opts[slot] = (Opt){a, val, base, kind, table};
}

static void app_enter_cb(void* ctx, uint32_t index) {
    App* app = ctx;
    if(index == 1) view_dispatcher_switch_to_view(app->vd, ViewReset);
}

static void build_settings(App* app) {
    Settings* s = &app->save.s;
    set_opt(app, 0, &s->rain_fall, 0, OptMs, FALL_MS);
    set_opt(app, 1, &s->rain_paddle, PMIN, OptNum, NULL);
    set_opt(app, 2, &s->rain_decoys, 1, OptNum, NULL);
    set_opt(app, 3, &s->snake_speed, 0, OptMs, SNAKE_MS);
    set_opt(app, 4, &s->snake_dis, 0, OptNum, NULL);
    set_opt(app, 5, &s->snake_vw, 0, OptOnOff, NULL);
    set_opt(app, 6, &s->snake_hw, 0, OptOnOff, NULL);
    set_opt(app, 7, &s->tn_size, 1, OptNum, NULL);
    set_opt(app, 8, &s->tn_ball, 0, OptMs, BALL_MS);
    set_opt(app, 9, &s->sound, 0, OptOnOff, NULL);

    variable_item_list_reset(app->set_rain);
    add_opt(app->set_rain, "Fall speed", &app->opts[0], 7);
    add_opt(app->set_rain, "Paddle size", &app->opts[1], 8);
    add_opt(app->set_rain, "Decoy letters", &app->opts[2], 8);

    variable_item_list_reset(app->set_snake);
    add_opt(app->set_snake, "Speed", &app->opts[3], 7);
    add_opt(app->set_snake, "Distractions", &app->opts[4], 11);
    add_opt(app->set_snake, "Side walls", &app->opts[5], 2);
    add_opt(app->set_snake, "Top/bottom walls", &app->opts[6], 2);

    variable_item_list_reset(app->set_tennis);
    add_opt(app->set_tennis, "Paddle size", &app->opts[7], 10);
    add_opt(app->set_tennis, "Ball speed", &app->opts[8], 7);

    variable_item_list_reset(app->set_app);
    add_opt(app->set_app, "Sound", &app->opts[9], 2);
    VariableItem* reset = variable_item_list_add(app->set_app, "Reset to defaults", 1, NULL, app);
    variable_item_set_current_value_text(reset, "OK");
    variable_item_list_set_enter_callback(app->set_app, app_enter_cb, app);
}

static void reset_result(DialogExResult result, void* ctx) {
    App* app = ctx;
    if(result == DialogExResultRight) {
        app->save.s = DEFAULTS;
        g_sound = app->save.s.sound;
        save_write(app);
        build_settings(app);
    }
    view_dispatcher_switch_to_view(app->vd, ViewSetApp);
}

/* ----- menus ----- */
static void start_game(App* app, uint8_t mode) {
    uint16_t best = app->save.best[app->cur_game * 2 + mode];
    with_view_model(
        app->game_view,
        GameModel * m,
        { game_start(&m->g, &app->save.s, &app->words, app->cur_game, mode, best); },
        true);
    app->in_game = true;
    app->last_tick = furi_get_tick();
    notification_message(app->notif, &sequence_display_backlight_enforce_on);
    view_dispatcher_switch_to_view(app->vd, ViewGame);
}

static void game_menu_cb(void* ctx, uint32_t index) {
    App* app = ctx;
    if(index < 2)
        start_game(app, index);
    else {
        ViewId v = app->cur_game == GameRain  ? ViewSetRain :
                   app->cur_game == GameSnake ? ViewSetSnake :
                                                ViewSetTennis;
        view_dispatcher_switch_to_view(app->vd, v);
    }
}

static void fill_game_menu(App* app) {
    Submenu* m = app->game_menu;
    char label[40];
    submenu_reset(m);
    submenu_set_header(m, GAME_NAME[app->cur_game]);
    for(uint8_t mode = 0; mode < 2; mode++) {
        uint16_t b = app->save.best[app->cur_game * 2 + mode];
        const char* lbl = (app->cur_game == GameRain && mode == 0) ? "last" : "best";
        if(b)
            snprintf(label, sizeof(label), "%s  (%s %u)", MODE_NAME[app->cur_game][mode], lbl, b);
        else
            snprintf(label, sizeof(label), "%s", MODE_NAME[app->cur_game][mode]);
        submenu_add_item(m, label, mode, game_menu_cb, app);
    }
    submenu_add_item(m, "Settings", 2, game_menu_cb, app);
    submenu_set_selected_item(m, 0);
}

static void main_menu_cb(void* ctx, uint32_t index) {
    App* app = ctx;
    if(index < 3) {
        app->cur_game = index;
        fill_game_menu(app);
        view_dispatcher_switch_to_view(app->vd, ViewGameMenu);
    } else if(index == 3) {
        view_dispatcher_switch_to_view(app->vd, ViewSetApp);
    } else {
        view_dispatcher_switch_to_view(app->vd, ViewAbout);
    }
}

/* ----- navigation (Back) ----- */
static uint32_t nav_exit(void* ctx) {
    UNUSED(ctx);
    return VIEW_NONE;
}
static uint32_t nav_main(void* ctx) {
    UNUSED(ctx);
    return ViewMain;
}
static uint32_t nav_game_menu(void* ctx) {
    UNUSED(ctx);
    return ViewGameMenu;
}
static uint32_t nav_set_app(void* ctx) {
    UNUSED(ctx);
    return ViewSetApp;
}

/* ----- cover ----- */
static void cover_draw(Canvas* canvas, void* model) {
    UNUSED(model);
    canvas_clear(canvas);
    canvas_draw_icon(canvas, 0, 0, &I_cover);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 104, 62, AlignCenter, AlignBottom, "Press OK");
}

static bool cover_input(InputEvent* e, void* ctx) {
    App* app = ctx;
    if(e->type == InputTypeShort && e->key == InputKeyOk) {
        view_dispatcher_switch_to_view(app->vd, ViewMain);
        return true;
    }
    return false;
}

/* ----- game view input ----- */
static void leave_game(App* app) {
    bool dirty = false;
    with_view_model(
        app->game_view,
        GameModel * m,
        {
            Game* g = &m->g;
            if(g->dirty) {
                app->save.best[g->game * 2 + g->mode] = g->best;
                g->dirty = false;
                dirty = true;
            }
        },
        false);
    if(dirty) save_write(app);
    app->in_game = false;
    snd_stop();
    notification_message(app->notif, &sequence_display_backlight_enforce_auto);
    fill_game_menu(app);
    view_dispatcher_switch_to_view(app->vd, ViewGameMenu);
}

static bool game_input(InputEvent* e, void* ctx) {
    App* app = ctx;
    bool leave = false, restart = false;
    uint8_t restart_mode = 0;

    with_view_model(
        app->game_view,
        GameModel * m,
        {
            Game* g = &m->g;
            if(e->key == InputKeyBack) {
                if(e->type == InputTypeShort) {
                    if(g->st == StRun) {
                        g->st = StPause;
                        g->kl = false;
                        g->kr = false;
                    } else
                        leave = true;
                }
            } else if(e->key == InputKeyOk) {
                if(e->type == InputTypeShort) {
                    if(g->st == StRun) {
                        g->st = StPause;
                        g->kl = false;
                        g->kr = false;
                    } else if(g->st == StPause) {
                        g->st = StRun;
                        g->kl = false;
                        g->kr = false;
                    } else if(g->st == StOver) {
                        restart = true;
                        restart_mode = g->mode;
                    }
                }
            } else if(g->st == StRun) {
                if(g->game == GameSnake) {
                    if(e->type == InputTypePress) {
                        if(e->key == InputKeyUp)
                            s_steer(g, 0);
                        else if(e->key == InputKeyDown)
                            s_steer(g, 1);
                        else if(e->key == InputKeyLeft)
                            s_steer(g, 2);
                        else if(e->key == InputKeyRight)
                            s_steer(g, 3);
                    }
                } else if(e->key == InputKeyLeft || e->key == InputKeyRight) {
                    int d = e->key == InputKeyRight ? 1 : -1;
                    if(e->type == InputTypePress) {
                        if(d > 0)
                            g->kr = true;
                        else
                            g->kl = true;
                        pad_step(g, d);
                        g->pa = 0;
                    } else if(e->type == InputTypeRelease) {
                        if(d > 0)
                            g->kr = false;
                        else
                            g->kl = false;
                    }
                }
            }
        },
        true);

    if(leave) {
        leave_game(app);
    } else if(restart) {
        /* flush best first, then start again */
        bool dirty = false;
        with_view_model(
            app->game_view,
            GameModel * m,
            {
                Game* g = &m->g;
                if(g->dirty) {
                    app->save.best[g->game * 2 + g->mode] = g->best;
                    g->dirty = false;
                    dirty = true;
                }
            },
            false);
        if(dirty) save_write(app);
        start_game(app, restart_mode);
    }
    return true; /* Back is always handled here */
}

/* ----- tick (runs on the dispatcher thread, 50 Hz) ----- */
static void tick_cb(void* ctx) {
    App* app = ctx;
    uint32_t now = furi_get_tick();
    uint32_t dt = now - app->last_tick;
    app->last_tick = now;
    if(dt > 100) dt = 100;
    snd_tick(dt);
    if(!app->in_game) return;

    bool save = false;
    with_view_model(
        app->game_view,
        GameModel * m,
        {
            Game* g = &m->g;
            game_tick(g, dt);
            if(g->dirty && g->st != StRun) {
                app->save.best[g->game * 2 + g->mode] = g->best;
                g->dirty = false;
                save = true;
            }
        },
        true);
    if(save) save_write(app);
}

/* ----- about ----- */
static const char ABOUT_TEXT[] =
    "\e#Lettercade\e#\n"
    "ASCII Arcade\n\n"
    "\e#Letters Rain\e#\n"
    "Slide the paddle with Left/Right and catch the letter shown at the top. Wrong letters "
    "shrink the paddle, + grows it, - shrinks it, * resets it. Miss the target and it's over.\n\n"
    "\e#Snake Letters\e#\n"
    "Steer with the D-pad and eat the target letter. Any other letter is deadly. "
    "- shrinks the snake, / halves it.\n\n"
    "\e#Tennis Letters\e#\n"
    "Keep the letter ball in play. Hard mode: landing spot and paddle motion change the angle.\n\n"
    "OK = pause, Back = menu.\n\n"
    "\e#Custom words\e#\n"
    "Put words in\n"
    "SD Card/apps_data/\n"
    "lettercade/words.txt\n"
    "(a-z, 2-15 letters, up to 24, separated by spaces, commas or new lines).";

/* ----- alloc / free ----- */
static App* app_alloc(void) {
    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));
    app->storage = furi_record_open(RECORD_STORAGE);
    app->notif = furi_record_open(RECORD_NOTIFICATION);
    app->gui = furi_record_open(RECORD_GUI);

    save_load(app);
    words_load(app);
    g_sound = app->save.s.sound;

    app->vd = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->vd, app);
    view_dispatcher_set_tick_event_callback(app->vd, tick_cb, furi_ms_to_ticks(TICK_MS));
    view_dispatcher_attach_to_gui(app->vd, app->gui, ViewDispatcherTypeFullscreen);

    /* cover */
    app->cover = view_alloc();
    view_set_context(app->cover, app);
    view_set_draw_callback(app->cover, cover_draw);
    view_set_input_callback(app->cover, cover_input);
    view_set_previous_callback(app->cover, nav_exit);
    view_dispatcher_add_view(app->vd, ViewCover, app->cover);

    /* main menu */
    app->main_menu = submenu_alloc();
    submenu_set_header(app->main_menu, "Lettercade");
    submenu_add_item(app->main_menu, GAME_NAME[0], 0, main_menu_cb, app);
    submenu_add_item(app->main_menu, GAME_NAME[1], 1, main_menu_cb, app);
    submenu_add_item(app->main_menu, GAME_NAME[2], 2, main_menu_cb, app);
    submenu_add_item(app->main_menu, "Settings", 3, main_menu_cb, app);
    submenu_add_item(app->main_menu, "About", 4, main_menu_cb, app);
    view_set_previous_callback(submenu_get_view(app->main_menu), nav_exit);
    view_dispatcher_add_view(app->vd, ViewMain, submenu_get_view(app->main_menu));

    /* per-game menu */
    app->game_menu = submenu_alloc();
    view_set_previous_callback(submenu_get_view(app->game_menu), nav_main);
    view_dispatcher_add_view(app->vd, ViewGameMenu, submenu_get_view(app->game_menu));

    /* settings lists */
    app->set_rain = variable_item_list_alloc();
    app->set_snake = variable_item_list_alloc();
    app->set_tennis = variable_item_list_alloc();
    app->set_app = variable_item_list_alloc();
    view_set_previous_callback(variable_item_list_get_view(app->set_rain), nav_game_menu);
    view_set_previous_callback(variable_item_list_get_view(app->set_snake), nav_game_menu);
    view_set_previous_callback(variable_item_list_get_view(app->set_tennis), nav_game_menu);
    view_set_previous_callback(variable_item_list_get_view(app->set_app), nav_main);
    view_dispatcher_add_view(app->vd, ViewSetRain, variable_item_list_get_view(app->set_rain));
    view_dispatcher_add_view(app->vd, ViewSetSnake, variable_item_list_get_view(app->set_snake));
    view_dispatcher_add_view(app->vd, ViewSetTennis, variable_item_list_get_view(app->set_tennis));
    view_dispatcher_add_view(app->vd, ViewSetApp, variable_item_list_get_view(app->set_app));
    build_settings(app);

    /* reset dialog */
    app->reset_dlg = dialog_ex_alloc();
    dialog_ex_set_header(app->reset_dlg, "Reset to defaults?", 64, 2, AlignCenter, AlignTop);
    dialog_ex_set_text(
        app->reset_dlg,
        "All settings return to\ntheir default values.\nScores are kept.",
        64,
        30,
        AlignCenter,
        AlignCenter);
    dialog_ex_set_left_button_text(app->reset_dlg, "Cancel");
    dialog_ex_set_right_button_text(app->reset_dlg, "Reset");
    dialog_ex_set_context(app->reset_dlg, app);
    dialog_ex_set_result_callback(app->reset_dlg, reset_result);
    view_set_previous_callback(dialog_ex_get_view(app->reset_dlg), nav_set_app);
    view_dispatcher_add_view(app->vd, ViewReset, dialog_ex_get_view(app->reset_dlg));

    /* about */
    app->about = widget_alloc();
    widget_add_text_scroll_element(app->about, 0, 0, 128, 64, ABOUT_TEXT);
    view_set_previous_callback(widget_get_view(app->about), nav_main);
    view_dispatcher_add_view(app->vd, ViewAbout, widget_get_view(app->about));

    /* game */
    app->game_view = view_alloc();
    view_set_context(app->game_view, app);
    view_allocate_model(app->game_view, ViewModelTypeLocking, sizeof(GameModel));
    view_set_draw_callback(app->game_view, game_draw);
    view_set_input_callback(app->game_view, game_input);
    view_set_previous_callback(app->game_view, nav_game_menu);
    view_dispatcher_add_view(app->vd, ViewGame, app->game_view);

    app->last_tick = furi_get_tick();
    return app;
}

static void app_free(App* app) {
    snd_stop();
    notification_message(app->notif, &sequence_display_backlight_enforce_auto);

    view_dispatcher_remove_view(app->vd, ViewCover);
    view_dispatcher_remove_view(app->vd, ViewMain);
    view_dispatcher_remove_view(app->vd, ViewGameMenu);
    view_dispatcher_remove_view(app->vd, ViewSetRain);
    view_dispatcher_remove_view(app->vd, ViewSetSnake);
    view_dispatcher_remove_view(app->vd, ViewSetTennis);
    view_dispatcher_remove_view(app->vd, ViewSetApp);
    view_dispatcher_remove_view(app->vd, ViewReset);
    view_dispatcher_remove_view(app->vd, ViewAbout);
    view_dispatcher_remove_view(app->vd, ViewGame);

    view_free(app->cover);
    submenu_free(app->main_menu);
    submenu_free(app->game_menu);
    variable_item_list_free(app->set_rain);
    variable_item_list_free(app->set_snake);
    variable_item_list_free(app->set_tennis);
    variable_item_list_free(app->set_app);
    dialog_ex_free(app->reset_dlg);
    widget_free(app->about);
    view_free_model(app->game_view);
    view_free(app->game_view);
    view_dispatcher_free(app->vd);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_STORAGE);
    free(app);
}

int32_t lettercade_app(void* p) {
    UNUSED(p);
    App* app = app_alloc();
    view_dispatcher_switch_to_view(app->vd, ViewCover);
    view_dispatcher_run(app->vd);
    app_free(app);
    return 0;
}
