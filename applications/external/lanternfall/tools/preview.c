// Host preview: plays scripted scenes and writes raw framebuffers for tools/png.py.
// cc -O2 -I. tools/preview.c game.c render.c -o /tmp/lfprev && /tmp/lfprev out_dir
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../game.h"
#include "../render.h"

static const char* dir = ".";
static void shot(Game* g, const char* name) {
    uint8_t fb[FB_SIZE];
    render(g, fb);
    char p[512];
    snprintf(p, sizeof(p), "%s/%s.fb", dir, name);
    FILE* f = fopen(p, "wb");
    fwrite(fb, 1, FB_SIZE, f);
    fclose(f);
}
static void ticks(Game* g, int n) {
    for(int i = 0; i < n; i++)
        game_tick(g), g->sfx = 0;
}
static void walk(Game* g, int key, int steps) {
    for(int i = 0; i < steps; i++) {
        game_key(g, key, KT_PRESS);
        game_key(g, key, KT_RELEASE);
        ticks(g, 5);
    }
}

int main(int argc, char** argv) {
    if(argc > 1) dir = argv[1];
    static Game g;
    uint32_t seed = argc > 2 ? (uint32_t)strtoul(argv[2], 0, 0) : 0xC0FFEE;
    game_new(&g, seed);
    ticks(&g, 30);
    shot(&g, "01_title");
    game_key(&g, K_OK, KT_PRESS);
    ticks(&g, 10);
    shot(&g, "02_village_day");
    walk(&g, K_DOWN, 1);
    ticks(&g, 70);
    shot(&g, "03_square");
    // out into the wilds
    walk(&g, K_DOWN, 10);
    ticks(&g, 60);
    shot(&g, "04_wilds");
    // a slime right in front of us
    for(int i = 0; i < MAX_ENT; i++)
        g.ents[i].type = E_NONE;
    g.ents[0] = (Ent){
        .type = E_SLIME, .lvl = 1, .hp = 6, .maxhp = 6, .x = g.pl.x + 1, .y = g.pl.y, .timer = 50};
    g.ents[1] = (Ent){
        .type = E_WOLF,
        .lvl = 1,
        .hp = 10,
        .maxhp = 10,
        .x = g.pl.x - 3,
        .y = g.pl.y + 1,
        .timer = 50,
        .face = F_RIGHT};
    g.ents[2] = (Ent){
        .type = E_BOAR,
        .lvl = 1,
        .hp = 16,
        .maxhp = 16,
        .x = g.pl.x + 3,
        .y = g.pl.y - 2,
        .timer = 50,
        .face = F_LEFT};
    g.pl.face = F_RIGHT;
    game_key(&g, K_OK, KT_PRESS);
    ticks(&g, 3);
    shot(&g, "05_fight");
    // dusk and night
    g.clock = DAY_LEN * 60 / 100;
    for(int i = 0; i < MAX_ENT; i++)
        g.ents[i].type = E_NONE;
    ticks(&g, 2);
    shot(&g, "06_dusk");
    g.clock = DAY_LEN * 75 / 100;
    g.pl.x = WORLD / 2 - 2, g.pl.y = WORLD / 2 + 2, g.pl.mv = 0;
    ticks(&g, 2);
    shot(&g, "07_night_village");
    g.pl.y += 14;
    g.ents[0] = (Ent){
        .type = E_SHADE,
        .lvl = 2,
        .hp = 14,
        .maxhp = 18,
        .x = g.pl.x + 2,
        .y = g.pl.y - 1,
        .timer = 80};
    ticks(&g, 2);
    shot(&g, "08_night_wilds");
    g.rain = 3;
    g.rain_t = 9999;
    g.clock = DAY_LEN * 30 / 100;
    g.ents[0].type = E_NONE;
    ticks(&g, 3);
    shot(&g, "09_storm");
    g.ui = UI_MAP;
    shot(&g, "10_map");
    g.ui = UI_PLAY;
    g.pl.hp = 1;
    g.ents[0] = (Ent){
        .type = E_WOLF,
        .lvl = 3,
        .hp = 10,
        .maxhp = 10,
        .x = g.pl.x + 1,
        .y = g.pl.y,
        .state = S_WIND,
        .timer = 1};
    g.inv = 0;
    g.in_village = false;
    ticks(&g, 2);
    shot(&g, "11_dead");
    // idle, by day, out in the wilds
    game_new(&g, 1234);
    game_key(&g, K_OK, KT_PRESS);
    g.lvl = 5, g.wpn = 2, g.pl.maxhp = g.pl.hp = 45, g.bread = 5;
    g.opt |= OPT_ROAM;
    game_set_idle(&g, true, NULL);
    for(int i = 0; i < 1200 && g.goal != G_HUNT; i++)
        ticks(&g, 1);
    ticks(&g, 3);
    shot(&g, "12_idle");
    return 0;
}
