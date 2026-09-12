/*
 * Screenshots, rendered rather than photographed.
 *
 * Stages a scene, calls the app's own bb_draw() through the real canvas,
 * and writes what comes out as a 128x64 PBM. tools/shoot.sh turns those
 * into the PNGs the Apps Catalog wants.
 */
#define BB_HOST_TEST 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "beepback.h"
#include "beepback_rules.c"
#include "beepback_nav.c"
#include "beepback_game.c"
#include "real_canvas.h"
#include "beepback_draw.c"
#include "beepback_intro.c"

static BeepbackApp app;

static void shot(const char* name) {
    char path[256];
    snprintf(path, sizeof(path), "%s.pbm", name);
    FILE* f = fopen(path, "wb");
    if(!f) {
        perror(path);
        exit(1);
    }
    fprintf(f, "P1\n%d %d\n", BB_W, BB_H);
    for(int y = 0; y < BB_H; y++) {
        for(int x = 0; x < BB_W; x++)
            fputc(rc_pixel(x, y) ? '1' : '0', f);
        fputc('\n', f);
    }
    fclose(f);
    printf("%s\n", path);
}

static void frame(const char* name) {
    u8g2_ClearBuffer(&rc_u8g2);
    u8g2_SetDrawColor(&rc_u8g2, 1);
    bb_draw((Canvas*)&app, &app);
    shot(name);
}

/* a run part-way through, so the in-game screens have something to show */
static void staged(uint8_t mode, uint8_t assist) {
    bb_app_init(&app);
    app.first_run = false;
    app.set.assist = assist;
    app.set.diff = 1;
    app.set.speed = 1;
    app.seed = 0xBEEF7ACEu;
    app.mode = mode;
    app.now = 100000;
    bb_start_game(&app);
    app.run_game_mode = mode;
}

int main(int argc, char** argv) {
    if(argc > 1 && chdir(argv[1]) != 0) {
        perror(argv[1]);
        return 1;
    }
    rc_init();

    /* 0: a classic round being played back, which is the game itself.
       The step drawn is whatever the generator dealt, so the seed is
       chosen rather than the shape: a star says more about the game at a
       glance than the circle the first seed happened to offer. */
    for(uint32_t seed = 1;; seed++) {
        staged(BbModeClassic, BbAssistShapes);
        app.seed = seed;
        bb_start_game(&app);
        if(bb_button_shape[app.base.step[0]] == BbShapeStar) break;
    }
    app.round = 3;
    app.target = 6;
    app.stage = 2;
    app.lives = 3;
    app.base.len = 6;
    app.play_idx = 0;
    app.tone_on = true;
    app.step_start = app.now - BB_FLASH_MS - 1; /* past the inverted pop-in */
    app.score = 480;
    app.scene = BbScenePlayback;
    frame("ss0");

    /* 1: the rule card, which is what makes this more than Simon.
       The rule comes from the generator and the words on screen are
       derived from it, so the only way to choose one is to keep drawing
       seeds until it offers the rule that reads clearest. */
    for(uint32_t seed = 1;; seed++) {
        staged(BbModeRules, BbAssistShapes);
        app.seed = seed;
        bb_start_game(&app);
        if(app.rule_idx == BbRuleSkip) break;
    }
    app.scene = BbSceneRuleCard;
    frame("ss1");

    /* 2: a records table with scores in it, not a grid of dashes */
    staged(BbModeClassic, BbAssistShapes);
    app.score_mode = BbModeClassic;
    app.tbl_assist = 2;
    {
        static const uint32_t v[BB_DIFF_COUNT][BB_SPEED_COUNT] = {
            {1240, 1580, 0}, {960, 2210, 430}, {0, 1120, 0}, {0, 0, 0}};
        for(uint8_t t = 0; t < BB_DIFF_COUNT; t++)
            for(uint8_t s = 0; s < BB_SPEED_COUNT; s++)
                app.rec.best[BbModeClassic][t][s][2] = v[t][s];
    }
    app.scene = BbSceneTable;
    frame("ss2");

    /* 3: reflex, with a cue live and the window draining */
    staged(BbModeReflex, BbAssistShapes);
    app.rx_hits = 18;
    app.score = 180; /* ten a hit, so the HUD agrees with itself */
    app.rx_window = 512;
    app.rx_cue = BbBtnUp;
    app.rx_at = app.now - 200;
    app.phase = app.now + 300;
    app.scene = BbSceneReflexCue;
    frame("ss3");

    /* 4: the menu, for the promo strip */
    bb_app_init(&app);
    app.first_run = false;
    app.now = 100000;
    app.scene = BbSceneMenu;
    frame("ss4");

    /* 5: the stats screen, with something in it */
    bb_app_init(&app);
    app.first_run = false;
    app.now = 100000;
    app.stats.runs = 47;
    app.stats.play_ms = 4520000;
    app.stats.rounds = 96;
    app.stats.notes = 1284;
    app.stats.best_ever = 2210;
    app.stats.longest = 11;
    app.stats.by_mode[BbModeClassic] = 30;
    app.stats.by_assist[2] = 40;
    app.scene = BbSceneStats;
    frame("ss5");

    /* 6: the bottom of the stats list, where the favourites are */
    app.stat_scroll = BB_STAT_ROWS;
    frame("ss6");

    return 0;
}
