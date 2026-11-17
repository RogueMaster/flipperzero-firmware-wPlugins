/* KEILEREI – LCD Brawler: plattformunabhaengiger Spielkern (reines C99, keine Abhaengigkeiten).
 * Identische Logik wie web/index.html. Ausgabe: 128x64 1-Bit-Framebuffer im XBM-Format
 * (16 Bytes pro Zeile, Bit 0 = linkes Pixel) -> direkt mit canvas_draw_xbm() auf den Flipper. */
#pragma once
#include <stdint.h>

#define FG_W       32
#define FG_H       14
#define FG_MAXE    14
#define FG_MAXB    16
#define FG_MAXFX   8
#define FG_FB_SIZE 1024

/* Tasten-Bits (gehalten / neu gedrueckt) */
#define K_UP    1
#define K_RIGHT 2
#define K_DOWN  4
#define K_LEFT  8
#define K_A     16
#define K_B     32

/* Ereignis-Bits fuer Ton/Vibration (nach fight_step in g->evt) */
#define EV_HIT  1
#define EV_HURT 2
#define EV_PICK 4
#define EV_KILL 8
#define EV_WAVE 16
#define EV_SHOT 32
#define EV_OVER 64

enum {
    S_TITLE,
    S_INTRO,
    S_PLAY,
    S_PAUSE,
    S_CLEAR,
    S_OVER
};

typedef struct {
    uint8_t on, t, cd, acd, fl, adj;
    int8_t x, y;
    int16_t hp;
} FEnemy;
typedef struct {
    uint8_t on, d, own, dmg;
    int8_t x, y;
} FBullet;
typedef struct {
    int8_t x, y;
    uint8_t t;
} FFx;

typedef struct {
    uint8_t map[FG_W * FG_H];
    uint32_t rng, tick, score, hi;
    int st, stT, wave, newHi, env;
    int px, py, pd, hp, inv, wp, dur, sh, mcd, acd, slide;
    FEnemy e[FG_MAXE];
    FBullet b[FG_MAXB];
    FFx fx[FG_MAXFX];
    int left[5], spawnT, iv, loot, alive, spawned, total;
    char msg[24];
    int msgT, evt;
} Fight;

void fight_init(Fight* g, uint32_t seed, uint32_t hiscore);
void fight_step(Fight* g, uint8_t held, uint8_t pressed);
void fight_render(const Fight* g, uint8_t* fb);
