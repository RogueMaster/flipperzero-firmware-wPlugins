/* Dump every draw call the firmware makes, screen by screen, so the two
   builds can be diffed rather than described. See reference/README.md.

   With no argument it traces every scene; with one it traces the scenes
   whose name contains that word, which is what you want when chasing a
   single screen. */
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

static const char* const scene_name[BbSceneCount] = {
    "splash",   "menu",       "mode",      "chpick",   "setup",       "rulecard",   "listen",
    "playback", "go",         "input",     "hold",     "success",     "roundclear", "wrong",
    "retry",    "reflexgap",  "reflexcue", "gameover", "settings",    "reset",      "help",
    "tutorial", "rulesguide", "rulelist",  "ruleinfo", "reflexguide", "soundtest",  "scorepick",
    "credits",  "stats",      "table",     "nocue",
};

/* One run, mid-flight, so every in-game screen has something to draw. */
static void stage_a_run(uint8_t mode) {
    bb_app_init(&app);
    app.first_run = false;
    app.set.diff = 2;
    app.set.speed = 2;
    app.seed = 3;
    app.mode = mode;
    bb_start_game(&app);
    app.round = 3;
    app.target = 6;
    app.stage = 4;
    app.lives = 2;
    app.score = 148;
    app.run_best = 7;
    app.prev_best = 120;
    app.new_best = true;
    app.run_game_mode = mode;
    app.run_diff = 2;
    app.run_speed = 2;
    app.rx_hits = 21;
    app.rx_window = 400;
    app.base.len = 6;
    app.expected.len = 6;
    app.rule_idx = BbRuleSkip;
    app.rule_a = BbBtnOk;
    app.rule_b = BbBtnUp;
    app.now += BB_OVER_LOCK; /* past the game over wipe */
}

int main(int argc, char** argv) {
    const char* only = argc > 1 ? argv[1] : NULL;
    for(uint8_t s = 0; s < BbSceneCount; s++) {
        if(only && !strstr(scene_name[s], only)) continue;
        stage_a_run(bb_in_game((BbScene)s) ? BbModeRules : BbModeClassic);
        app.scene = (BbScene)s;
        printf("\n=== firmware: %s ===\n", scene_name[s]);
        bb_draw((Canvas*)&app, &app);
    }
    tc_flush_dots();
    return 0;
}
