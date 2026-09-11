/* Dump every draw call the firmware makes, screen by screen, so the two
   builds can be diffed rather than described. */
#define BB_HOST_TEST 1
#include <stdio.h>
#include <string.h>
#include "beepback.h"
#include "../beepback_rules.c"
#include "../beepback_nav.c"
#include "../beepback_game.c"
#include "trace_canvas.h"
#include "../beepback_draw.c"
#include "../beepback_intro.c"

static BeepbackApp app;

static void head(const char* name) {
    printf("\n=== firmware: %s ===\n", name);
}

int main(int argc, char** argv) {
    const char* only = argc > 1 ? argv[1] : NULL;
#define WANT(n) (!only || strcmp(only, (n)) == 0)

    if(WANT("over")) {
        bb_app_init(&app);
        app.seed = 3;
        bb_run_start(&app, BbModeClassic);
        app.run.score = 1240;
        app.run.mult = 155;
        app.run.record = true;
        app.run.round = 3;
        app.run.longest = 7;
        app.run.prev_best = 980;
        bb_go(&app, BbSceneOver);
        app.now += BB_OVER_LOCK;
        head("game over");
        bb_draw((Canvas*)&app, &app);
        app.over_page = 1;
        head("game over page 2");
        bb_draw((Canvas*)&app, &app);
    }

    if(WANT("modesel")) {
        bb_app_init(&app);
        bb_go(&app, BbSceneModeSelect);
        app.mode_page = 1;
        app.mode_row = 1;
        head("mode select page 2");
        bb_draw((Canvas*)&app, &app);
    }

    if(WANT("setup")) {
        bb_app_init(&app);
        app.run.mode = BbModeClassic;
        app.set.diff = 2;
        app.set.speed = 2;
        bb_go(&app, BbSceneSetup);
        app.setup_cur = 2;
        head("setup");
        bb_draw((Canvas*)&app, &app);
    }

    if(WANT("settings")) {
        bb_app_init(&app);
        app.set.volume = 2;
        app.set.assist = BbAssistShapes;
        bb_go(&app, BbSceneSettings);
        app.settings_cur = 3;
        head("settings");
        bb_draw((Canvas*)&app, &app);
    }

    if(WANT("pause")) {
        bb_app_init(&app);
        app.seed = 5;
        bb_run_start(&app, BbModeRules);
        app.run.rule = BbRuleSkip;
        app.run.ra = BbBtnOk;
        app.run.round = 3;
        app.run.shown = 2;
        app.run.target = 6;
        app.run.lives = 3;
        bb_go(&app, BbScenePause);
        head("pause");
        bb_draw((Canvas*)&app, &app);
    }

    if(WANT("board")) {
        bb_app_init(&app);
        bb_go(&app, BbSceneBoard);
        head("board");
        bb_draw((Canvas*)&app, &app);
    }

    if(WANT("menu")) {
        bb_app_init(&app);
        bb_go(&app, BbSceneMenu);
        head("menu");
        bb_draw((Canvas*)&app, &app);
    }
    tc_flush_dots();
    return 0;
}
