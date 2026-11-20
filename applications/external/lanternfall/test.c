// Host checks for game.c and render.c:
// cc -O1 -g -fsanitize=address,undefined -I. test.c game.c render.c -o /tmp/lft && /tmp/lft
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "render.h"

static void ticks(Game* g, int n) {
    for(int i = 0; i < n; i++)
        game_tick(g), g->sfx = 0;
}
static void clear_ents(Game* g) {
    for(int i = 0; i < MAX_ENT; i++)
        g->ents[i].type = E_NONE;
}
static void play(Game* g) {
    game_key(g, K_OK, KT_PRESS); // title -> play
    assert(g->ui == UI_PLAY);
}
// Puts the hero in open country away from villages.
static void to_wilds(Game* g) {
    for(int y = 20; y < WORLD - 20; y++)
        for(int x = 20; x < WORLD - 20; x++) {
            bool ok = true;
            for(int dy = -3; dy <= 3 && ok; dy++)
                for(int dx = -6; dx <= 6 && ok; dx++)
                    if(tile_blocks(world_tile(g->seed, x + dx, y + dy)) ||
                       village_at(g->seed, x + dx, y + dy, NULL))
                        ok = false;
            if(ok) {
                g->pl.x = x, g->pl.y = y, g->pl.mv = 0;
                g->in_village = false;
                clear_ents(g);
                return;
            }
        }
    assert(!"no open ground");
}

static void test_world(uint32_t seed) {
    // determinism
    for(int i = 0; i < 2000; i++) {
        int x = (int)(lf_hash(seed, i, 0, 1) % WORLD), y = (int)(lf_hash(seed, i, 1, 1) % WORLD);
        assert(world_tile(seed, x, y) == world_tile(seed, x, y));
    }
    assert(world_tile(seed, -1, 5) == T_DEEP && world_tile(seed, WORLD, 5) == T_DEEP);
    int villages = 0;
    for(int cy = 0; cy * CELL < WORLD; cy++)
        for(int cx = 0; cx * CELL < WORLD; cx++) {
            Village v;
            village_in_cell(seed, cx, cy, &v);
            if(!v.ok) continue;
            villages++;
            // the whole layout sits inside its own cell
            assert((v.x - VIL_W / 2) / CELL == cx && (v.x + VIL_W / 2) / CELL == cx);
            assert((v.y - VIL_H / 2) / CELL == cy && (v.y + VIL_H / 2) / CELL == cy);
            assert(world_tile(seed, v.x, v.y) == T_WELL);
            int hy = v.y + ((v.mirror & 2) ? -1 : 1);
            assert(!tile_blocks(world_tile(seed, v.x, hy)));
            // every house is a whole 2x2 block
            int houses = 0;
            for(int y = v.y - VIL_H / 2; y <= v.y + VIL_H / 2; y++)
                for(int x = v.x - VIL_W / 2; x <= v.x + VIL_W / 2; x++) {
                    if(!house_anchor(seed, x, y)) continue;
                    houses++;
                    assert(
                        world_tile(seed, x + 1, y) == T_HOUSE &&
                        world_tile(seed, x, y + 1) == T_HOUSE &&
                        world_tile(seed, x + 1, y + 1) == T_HOUSE);
                    assert(
                        world_tile(seed, x + 2, y) != T_HOUSE &&
                        world_tile(seed, x, y + 2) != T_HOUSE);
                }
            assert(houses == 9);
            char name[24];
            village_name(&v, name, sizeof(name));
            assert(strlen(name) > 3);
        }
    assert(villages >= 2 && villages == world_villages(seed));
}

static void test_combat(void) {
    Game g;
    game_new(&g, 1234);
    play(&g);
    to_wilds(&g);
    g.pl.face = F_RIGHT;
    g.ents[0] = (Ent){
        .type = E_SLIME, .lvl = 1, .hp = 6, .maxhp = 6, .x = g.pl.x + 1, .y = g.pl.y, .timer = 200};
    int coins = g.coins, xp = g.xp;
    g.q_active = 1, g.q_type = E_SLIME, g.q_need = 1, g.q_have = 0;
    for(int k = 0; k < 10 && g.ents[0].type; k++) {
        game_key(&g, K_OK, KT_PRESS);
        ticks(&g, 12);
        if(g.ents[0].type)
            g.ents[0].x = g.pl.x + 1, g.ents[0].y = g.pl.y, g.ents[0].mv = 0; // undo knockback
    }
    assert(g.ents[0].type == E_NONE);
    assert(g.coins > coins && g.xp > xp && g.kills[E_SLIME] == 1);
    assert(g.q_have == 1);

    // a wolf winds up next to the hero and bites
    to_wilds(&g);
    int hp = g.pl.hp;
    g.ents[0] =
        (Ent){.type = E_WOLF, .lvl = 1, .hp = 10, .maxhp = 10, .x = g.pl.x - 1, .y = g.pl.y};
    ticks(&g, 30);
    assert(g.pl.hp < hp);

    // boars charge in a straight line and end up dazed
    to_wilds(&g);
    g.inv = 0;
    hp = g.pl.hp = g.pl.maxhp;
    g.ents[0] =
        (Ent){.type = E_BOAR, .lvl = 1, .hp = 16, .maxhp = 16, .x = g.pl.x + 4, .y = g.pl.y};
    bool charged = false, stunned = false;
    for(int i = 0; i < 80; i++) {
        ticks(&g, 1);
        charged |= g.ents[0].state == S_CHARGE;
        stunned |= g.ents[0].state == S_STUN;
    }
    assert(charged && stunned && g.pl.hp < hp);

    // dying sends the hero home with full health and a lighter purse
    to_wilds(&g);
    g.inv = 0;
    g.pl.hp = 1;
    g.coins = 100;
    g.ents[0] =
        (Ent){.type = E_WOLF, .lvl = 5, .hp = 10, .maxhp = 10, .x = g.pl.x + 1, .y = g.pl.y};
    ticks(&g, 30);
    assert(g.ui == UI_DEAD && g.coins == 80);
    game_key(&g, K_OK, KT_PRESS);
    assert(g.ui == UI_PLAY && g.pl.hp == g.pl.maxhp && g.pl.x == g.home_x && g.pl.y == g.home_y);
    assert(!tile_blocks(world_tile(g.seed, g.pl.x, g.pl.y)));
}

static void test_village_safe(void) {
    Game g;
    game_new(&g, 99);
    play(&g);
    ticks(&g, 5);
    assert(g.in_village && g.vil_id >= 0);
    int hp = g.pl.hp = g.pl.maxhp - 5;
    ticks(&g, 200);
    assert(g.pl.hp > hp); // resting in the square heals
    // monsters never step inside a village, even chasing
    clear_ents(&g);
    g.pl.x = WORLD / 2, g.pl.y = WORLD / 2 + VIL_H / 2; // bottom edge of the first village
    for(int i = 0; i < 4000; i++) {
        game_tick(&g);
        g.sfx = 0;
        for(int k = 0; k < MAX_ENT; k++) {
            Ent* e = &g.ents[k];
            if(e->type && e->type != E_VILLAGER) assert(!village_at(g.seed, e->x, e->y, NULL));
        }
    }
}

static void test_talk_shop_board(void) {
    Game g;
    game_new(&g, 7);
    play(&g);
    ticks(&g, 2);
    // walk up to the stall: it sits right of the board above the well (no mirror in the first village)
    g.pl.x = WORLD / 2 + 1, g.pl.y = WORLD / 2 - 2, g.pl.face = F_DOWN;
    assert(world_tile(g.seed, g.pl.x, g.pl.y + 1) == T_STALL);
    game_key(&g, K_OK, KT_PRESS);
    assert(g.ui == UI_SHOP);
    g.coins = 4;
    g.cursor = 0;
    game_key(&g, K_OK, KT_PRESS);
    assert(g.bread == 2 && (g.sfx & SFX_DENY)); // too poor
    g.coins = 50;
    game_key(&g, K_OK, KT_PRESS);
    assert(g.bread == 3 && g.coins == 45);
    game_key(&g, K_DOWN, KT_PRESS);
    game_key(&g, K_DOWN, KT_PRESS);
    game_key(&g, K_OK, KT_PRESS);
    assert(g.wpn == 1 && g.coins == 5);
    g.loot[L_PELT] = 2;
    g.cursor = 5;
    game_key(&g, K_OK, KT_PRESS);
    assert(g.coins == 15 && g.loot[L_PELT] == 0);
    game_key(&g, K_BACK, KT_PRESS);
    assert(g.ui == UI_PLAY);
    // the board offers a hunt, accepting it starts the quest
    g.pl.x = WORLD / 2 - 1;
    assert(world_tile(g.seed, g.pl.x, g.pl.y + 1) == T_BOARD);
    game_key(&g, K_OK, KT_PRESS);
    assert(g.ui == UI_BOARD);
    char line[48];
    game_menu(&g, 0, line, sizeof(line));
    assert(strstr(line, "Accept"));
    game_key(&g, K_OK, KT_PRESS);
    assert(g.q_active && g.ui == UI_PLAY);
    g.q_have = g.q_need;
    int coins = g.coins;
    game_key(&g, K_OK, KT_PRESS);
    game_key(&g, K_OK, KT_PRESS);
    assert(!g.q_active && g.coins > coins);
    // menus wrap
    game_key(&g, K_BACK, KT_PRESS);
    assert(g.ui == UI_PAUSE && g.cursor == 0);
    game_key(&g, K_UP, KT_PRESS);
    assert(g.cursor == 6);
    // talking to a villager fills the dialogue
    clear_ents(&g);
    g.ui = UI_PLAY;
    g.pl.x = WORLD / 2 - 3, g.pl.y = WORLD / 2, g.pl.face = F_LEFT;
    g.ents[0] = (Ent){
        .type = E_VILLAGER,
        .x = g.pl.x - 1,
        .y = g.pl.y,
        .home_x = WORLD / 2,
        .home_y = WORLD / 2,
        .seed = 77};
    game_key(&g, K_OK, KT_PRESS);
    assert(g.ui == UI_TALK && strlen(g.dlg_text) > 20 && !strstr(g.dlg_text, "{v}"));
}

static void test_save(void) {
    Game a, b;
    game_new(&a, 4242);
    play(&a);
    a.coins = 321, a.lvl = 4, a.wpn = 2, a.q_active = 1, a.q_type = E_WOLF, a.q_need = 5,
    a.q_have = 2;
    a.explored[10] = 0x5a;
    uint8_t buf[SAVE_SIZE];
    int n = game_save(&a, buf);
    assert(n > 0 && n <= SAVE_SIZE);
    assert(game_load(&b, buf, n));
    assert(
        b.seed == a.seed && b.coins == 321 && b.lvl == 4 && b.wpn == 2 && b.q_have == 2 &&
        b.explored[10] == 0x5a);
    assert(b.pl.x == a.pl.x && b.pl.y == a.pl.y && b.has_save);
    buf[20] ^= 1;
    assert(!game_load(&b, buf, n)); // checksum catches corruption
    assert(!game_load(&b, buf, 3));
}

static int explored_count(const Game* g) {
    int n = 0;
    for(int i = 0; i < 1024; i++)
        n += (g->explored[i >> 3] >> (i & 7)) & 1;
    return n;
}

static void test_auto_attack(void) {
    Game g;
    game_new(&g, 1234);
    play(&g);
    to_wilds(&g);
    g.pl.face = F_UP;
    g.ents[0] = (Ent){
        .type = E_SLIME, .lvl = 1, .x = g.pl.x + 1, .y = g.pl.y, .hp = 3, .maxhp = 3, .timer = 200};
    ticks(&g, 40);
    assert(g.ents[0].type == E_SLIME); // off by default: nothing happens without a key
    g.opt |= OPT_AUTO;
    ticks(&g, 40);
    assert(g.ents[0].type == E_NONE && g.kills[E_SLIME] == 1 && g.pl.face == F_RIGHT);
    // never at villagers
    g.ents[1] = (Ent){
        .type = E_VILLAGER,
        .x = g.pl.x - 1,
        .y = g.pl.y,
        .home_x = g.pl.x,
        .home_y = g.pl.y,
        .hp = 1,
        .timer = 200};
    ticks(&g, 40);
    assert(g.ents[1].type == E_VILLAGER && !g.swing);
}

static void test_idle(void) {
    Game g;
    game_new(&g, 1234);
    play(&g);
    to_wilds(&g);
    g.lvl = 4, g.wpn = 2, g.arm = 2, g.pl.maxhp = g.pl.hp = 40, g.bread = 9;
    int sx = g.pl.x, sy = g.pl.y;
    game_set_idle(&g, true, NULL);
    assert(g.idle && g.idle_x == sx);
    int moved = 0, far = 0;
    for(int i = 0; i < 6000 && g.idle; i++) {
        int px = g.pl.x, py = g.pl.y;
        ticks(&g, 1);
        moved += px != g.pl.x || py != g.pl.y;
        int d = abs(g.pl.x - sx) + abs(g.pl.y - sy);
        if(d > far) far = d;
    }
    int kills = g.kills[E_SLIME] + g.kills[E_WOLF] + g.kills[E_BOAR] + g.kills[E_SHADE];
    printf("idle near: moved %d, kills %d, farthest %d, idle %d\n", moved, kills, far, g.idle);
    assert(g.idle && moved > 100 && kills > 0 && far <= 12 + 8 + 8 + 2);
    // a press of your own takes over
    game_key(&g, K_UP, KT_PRESS);
    assert(!g.idle);

    // badly hurt with no food: sits it out and mends, then carries on
    to_wilds(&g);
    g.bread = g.tonic = 0, g.pl.hp = 5;
    game_set_idle(&g, true, NULL);
    int i = 0, rested = 0;
    for(; i < 4000 && g.pl.hp * 100 <= g.pl.maxhp * 85 && g.idle; i++) {
        clear_ents(&g); // nothing to finish the hero off meanwhile
        ticks(&g, 1);
        rested += g.rest && !g.pl.mv;
    }
    printf("idle mend: %d ticks, hp %d/%d, %s\n", i, g.pl.hp, g.pl.maxhp, g.toast);
    assert(g.idle && rested > 800 && g.pl.hp * 100 > g.pl.maxhp * 85);
    ticks(&g, 20);
    assert(!g.rest);

    // roaming finds unseen country
    game_new(&g, 99);
    play(&g);
    g.lvl = 6, g.wpn = 3, g.arm = 3, g.pl.maxhp = g.pl.hp = 60, g.bread = 9, g.tonic = 9;
    g.opt |= OPT_ROAM;
    int before = explored_count(&g);
    game_set_idle(&g, true, NULL);
    for(i = 0; i < 20000 && g.idle; i++)
        ticks(&g, 1);
    int after = explored_count(&g);
    printf("idle roam: chunks seen %d -> %d, idle %d, lvl %d\n", before, after, g.idle, g.lvl);
    assert(after > before + 40);
}

static void test_settings(void) {
    Game g;
    game_new(&g, 77);
    play(&g);
    assert(g.opt == OPT_DEFAULT);
    game_key(&g, K_BACK, KT_PRESS);
    for(int i = 0; i < 5; i++)
        game_key(&g, K_DOWN, KT_PRESS);
    game_key(&g, K_OK, KT_PRESS);
    assert(g.ui == UI_SETTINGS && g.cursor == 0);
    char line[48];
    game_menu(&g, 0, line, sizeof(line));
    assert(!strcmp(line, "Auto-attack\tOff"));
    game_key(&g, K_OK, KT_PRESS);
    assert((g.opt & OPT_AUTO) && g.opt_dirty);
    game_key(&g, K_DOWN, KT_PRESS);
    game_key(&g, K_RIGHT, KT_PRESS);
    assert(g.opt & OPT_ROAM);
    game_key(&g, K_UP, KT_PRESS);
    game_key(&g, K_UP, KT_PRESS); // wraps to Controls
    game_key(&g, K_OK, KT_PRESS);
    assert(g.ui == UI_TALK && !strcmp(g.dlg_name, "Controls"));
    game_key(&g, K_BACK, KT_PRESS);
    assert(g.ui == UI_SETTINGS);
    game_key(&g, K_BACK, KT_PRESS);
    assert(g.ui == UI_PAUSE && g.cursor == 5);
    // Go idle from the pause menu, and the menu then offers to stop
    g.cursor = 1;
    game_key(&g, K_OK, KT_PRESS);
    assert(g.ui == UI_PLAY && g.idle);
    game_key(&g, K_BACK, KT_PRESS);
    game_menu(&g, 1, line, sizeof(line));
    assert(!strcmp(line, "Stop idle"));
    // a new journey keeps the settings
    uint16_t opt = g.opt;
    g.ui = UI_TITLE, g.cursor = 1, g.has_save = true;
    game_key(&g, K_OK, KT_PRESS);
    assert(g.opt == opt && !g.idle);
}

static void test_fuzz(uint32_t seed) {
    static Game g;
    game_new(&g, seed);
    play(&g);
    uint8_t fb[FB_SIZE];
    uint32_t r = seed | 1;
    for(int i = 0; i < 30000; i++) {
        r ^= r << 13, r ^= r >> 17, r ^= r << 5;
        if(r % 7 == 0) {
            uint8_t key = (r >> 8) % 6, type = (r >> 12) % 5;
            if(key == K_BACK && type == KT_LONG) type = KT_PRESS;
            game_key(&g, key, type);
            g.quit = false;
        }
        if(g.ui != UI_PLAY && r % 31 == 0) g.ui = UI_PLAY; // keep exploring
        if(i % 3000 == 0)
            g.opt = (uint16_t)(r >> 3) & OPT_ALL, game_set_idle(&g, (r >> 20) & 1, NULL);
        game_tick(&g);
        g.sfx = 0;
        if(i % 97 == 0) render(&g, fb);
        assert(g.pl.hp <= g.pl.maxhp && g.pl.hp >= 0);
        assert(!tile_blocks(world_tile(g.seed, g.pl.x, g.pl.y)));
        for(int k = 0; k < MAX_ENT; k++) {
            Ent* e = &g.ents[k];
            if(!e->type) continue;
            assert(!(e->x == g.pl.x && e->y == g.pl.y));
            for(int m = k + 1; m < MAX_ENT; m++)
                assert(!(g.ents[m].type && g.ents[m].x == e->x && g.ents[m].y == e->y));
            uint8_t t = world_tile(g.seed, e->x, e->y);
            if(e->type != E_SHADE)
                assert(!tile_blocks(t));
            else
                assert(t != T_WATER && t != T_DEEP && t != T_HOUSE);
        }
    }
}

int main(void) {
    for(uint32_t s = 1; s < 40; s++)
        test_world(s * 7919);
    test_combat();
    test_village_safe();
    test_talk_shop_board();
    test_save();
    test_auto_attack();
    test_idle();
    test_settings();
    for(uint32_t s = 1; s < 5; s++)
        test_fuzz(s * 31337);
    puts("ok");
    return 0;
}
