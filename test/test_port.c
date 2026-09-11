/* Host-side check of the port: the scenes, the run, the daily and every
   screen, against the browser build this was copied from. */
#define BB_HOST_TEST 1
#include <stdio.h>
#include <string.h>
#include "beepback.h"
#include "../beepback_rules.c"
#include "../beepback_tables.c"
#include "../beepback_game.c"
#include "../beepback_nav.c"
#include "fake_canvas.h"
#include "../beepback_draw.c"
#include "../beepback_intro.c"
#include "../beepback_save.c"

static int fails = 0;
static void check(const char* name, int ok, const char* extra) {
    printf("%s %s%s%s\n", ok ? "ok  " : "FAIL", name, extra && *extra ? "  -> " : "", extra ? extra : "");
    if(!ok) fails++;
}

static const char* const scene_name[BbSceneCount] = {
    "launcher", "splash",    "menu",       "mode",     "chpick",   "setup",     "rulecard",
    "listen",   "playback",  "go",         "input",    "hold",     "success",   "roundclear",
    "wrong",    "retry",     "reflexgap",  "reflexcue", "gameover", "settings", "detail",
    "reset",    "help",      "tutorial",   "rulesguide", "rulelist", "ruleinfo", "reflexguide",
    "soundtest", "scorepick", "scores",    "credits",
};
static const InputKey keys[6] = {
    InputKeyUp, InputKeyDown, InputKeyLeft, InputKeyRight, InputKeyOk, InputKeyBack};

static InputKey key_of(uint8_t b) {
    switch(b) {
    case BbBtnUp: return InputKeyUp;
    case BbBtnDown: return InputKeyDown;
    case BbBtnLeft: return InputKeyLeft;
    case BbBtnRight: return InputKeyRight;
    default: return InputKeyOk;
    }
}
static void boot(BeepbackApp* app) {
    bb_app_init(app);
    app->seed = 0xB33FBACCu;
}
static void frame(BeepbackApp* app) {
    fake_canvas_reset();
    bb_draw((Canvas*)app, app);
}
static bool wait_scene(BeepbackApp* app, BbScene want, int max) {
    for(int i = 0; i < max; i++) {
        if(app->scene == want) return true;
        if(app->scene == BbSceneGameOver) return want == BbSceneGameOver;
        bb_tick(app, BB_TICK_MS);
    }
    return app->scene == want;
}
static void play_stage(BeepbackApp* app) {
    uint8_t n = app->expected.len;
    for(uint8_t i = 0; i < n; i++) bb_press(app, key_of(app->expected.press[i]));
}

int main(void) {
    char msg[300];
    BeepbackApp app;

    /* ---- the scene set is the browser's ---- */
    check("there are as many scenes as the browser has", BbSceneCount == 32, "");
    boot(&app);
    check("a cold start is the launcher", app.scene == BbSceneLauncher, "");
    check("with the browser's defaults",
          app.set.volume == 2 && app.set.assist == 2 && app.set.speed == 1 && app.set.diff == 1, "");
    check("and a first run pending", app.first_run, "");

    /* ---- every scene is reachable ---- */
    {
        bool seen[BbSceneCount];
        memset(seen, 0, sizeof(seen));
        boot(&app);
        seen[app.scene] = true;
        BbRng rng;
        bb_rng_seed(&rng, 4242u);
        for(uint32_t i = 0; i < 400000; i++) {
            bb_tick(&app, BB_TICK_MS);
            seen[app.scene] = true;
            /* not every tick: a press on every one of them means the input
               window never gets a frame to itself */
            if(bb_rng_below(&rng, 4) == 0) {
                bb_press(&app, keys[bb_rng_below(&rng, 6)]);
                seen[app.scene] = true;
            }
        }
        /* Two scenes a random masher never sees, because both need the
           right button at the right moment: clearing a round, and living
           long enough in reflex for a cue to go up. Played on purpose. */
        boot(&app);
        app.mode = BbModeClassic;
        bb_start_game(&app);
        for(int stage = 0; stage < BB_START_LEN + 1; stage++) {
            for(int i = 0; i < 8000 && app.scene != BbSceneInput; i++) {
                bb_tick(&app, BB_TICK_MS);
                seen[app.scene] = true;
            }
            if(app.scene != BbSceneInput) break;
            play_stage(&app);
            seen[app.scene] = true;
            /* only until the next stage starts: ticking on past that would
               sit in the input window until it timed out */
            for(int i = 0; i < 400 && app.scene != BbSceneListen &&
                           app.scene != BbSceneRuleCard;
                i++) {
                bb_tick(&app, BB_TICK_MS);
                seen[app.scene] = true;
            }
        }
        boot(&app);
        app.mode = BbModeReflex;
        bb_start_game(&app);
        for(int i = 0; i < 4000; i++) {
            bb_tick(&app, BB_TICK_MS);
            seen[app.scene] = true;
            if(app.scene == BbSceneReflexCue) bb_press(&app, key_of(app.rx_cue));
        }

        int missing = 0, p = 0;
        msg[0] = 0;
        for(int s = 0; s < BbSceneCount; s++)
            if(!seen[s]) {
                missing++;
                p += sprintf(msg + p, "%s ", scene_name[s]);
            }
        check("every scene can be reached by pressing buttons", missing == 0,
              missing ? msg : "all 32");
    }

    /* ---- the daily, pinned against the browser build ---- */
    {
        bb_seed_override = 20260910u;
        boot(&app);
        app.mode = BbModeDaily;
        bb_start_game(&app);
        for(int i = 0; i < 29; i++) bb_challenge_grow(&app);
        char got[64];
        int at = 0;
        for(uint8_t i = 0; i < 30 && i < app.base.len; i++)
            at += sprintf(got + at, "%u", app.base.step[i]);
        got[at] = 0;
        sprintf(msg, "rule %d, %u/%u", app.rule_idx, app.rule_a, app.rule_b);
        check("2026-09-10 picks the rule the browser picks",
              app.rule_idx == BbRuleNoDoubles && app.rule_a == 2 && app.rule_b == 0, msg);
        check("and grows the sequence the browser grows",
              strcmp(got, "102114334400314231030040221432") == 0, got);
        check("with time and speed locked to normal", app.set.diff == 1 && app.set.speed == 1, "");

        bb_seed_override = 20260911u;
        boot(&app);
        app.mode = BbModeDaily;
        bb_start_game(&app);
        for(int i = 0; i < 29; i++) bb_challenge_grow(&app);
        at = 0;
        for(uint8_t i = 0; i < 30 && i < app.base.len; i++)
            at += sprintf(got + at, "%u", app.base.step[i]);
        got[at] = 0;
        check("2026-09-11 is the browser's other pinned day",
              app.rule_idx == BbRuleEveryOther &&
                  strcmp(got, "033221042032412034102302402014") == 0, got);
        bb_seed_override = 0;
    }

    /* ---- a classic run pays what the browser pays ---- */
    boot(&app);
    app.mode = BbModeClassic;
    bb_start_game(&app);
    check("a run opens on stage one of four", app.stage == 1 && app.target == BB_START_LEN, "");
    check("with three lives", app.lives == BB_LIVES, "");
    check("and x1.00 captured", app.run_mult == 10000, "");
    for(int stage = 0; stage < BB_START_LEN; stage++) {
        if(!wait_scene(&app, BbSceneInput, 8000)) break;
        play_stage(&app);
        if(!wait_scene(&app, BbSceneSuccess, 400) && app.scene != BbSceneRoundClear)
            wait_scene(&app, BbSceneRoundClear, 400);
    }
    sprintf(msg, "%lu", (unsigned long)app.score);
    /* The stage that clears the round pays the round bonus and not a
       stage award: hold goes to roundclear rather than success, and only
       success pays 10 a step. 10+20+30 then 50. */
    check("round one at x1.00 pays 10+20+30 and a 50 bonus", app.score == 110, msg);
    check("and the round bonus replaced the last stage award", app.round == 2, "");

    /* ---- pausing stops the clock ---- */
    boot(&app);
    app.mode = BbModeClassic;
    bb_start_game(&app);
    wait_scene(&app, BbSceneInput, 8000);
    {
        uint32_t left = app.input_end - app.now;
        bb_press(&app, InputKeyBack);
        check("BACK pauses rather than leaving", app.paused && bb_in_game(app.scene), "");
        for(int i = 0; i < 100; i++) bb_tick(&app, BB_TICK_MS);
        check("and the window does not drain", app.scene == BbSceneInput, "");
        bb_press(&app, InputKeyOk);
        sprintf(msg, "%lu vs %lu", (unsigned long)(app.input_end - app.now), (unsigned long)left);
        check("OK resumes with the window it had",
              !app.paused && app.input_end - app.now == left, msg);
        bb_press(&app, InputKeyBack);
        bb_press(&app, InputKeyBack);
        check("BACK twice quits to the menu", app.scene == BbSceneMenu, scene_name[app.scene]);
    }

    /* ---- reflex ---- */
    boot(&app);
    app.mode = BbModeReflex;
    bb_start_game(&app);
    check("reflex gets one life", app.lives == 1, "");
    check("and starts at the shared window", app.rx_window == BB_RX_START, "");
    wait_scene(&app, BbSceneReflexCue, 4000);
    for(int i = 0; i < 6; i++) bb_tick(&app, BB_TICK_MS);
    bb_press(&app, key_of(app.rx_cue));
    sprintf(msg, "%u ms", app.rx_fastest);
    check("a hit records its reaction time", app.rx_fastest >= 100, msg);
    wait_scene(&app, BbSceneReflexCue, 4000);
    bb_press(&app, key_of((uint8_t)((app.rx_cue + 1) % BbBtnCount)));
    check("a wrong button ends the run", app.scene == BbSceneGameOver, scene_name[app.scene]);
    check("and reports the fastest as its best", app.run_best == app.rx_fastest, "");

    boot(&app);
    app.mode = BbModeReflex;
    bb_start_game(&app);
    wait_scene(&app, BbSceneReflexGap, 100);
    bb_press(&app, InputKeyOk);
    check("pressing in the gap is a miss", app.scene == BbSceneGameOver, "");
    boot(&app);
    app.mode = BbModeReflex;
    bb_start_game(&app);
    bb_press(&app, InputKeyBack);
    check("and BACK ends it rather than pausing",
          app.scene == BbSceneGameOver && !app.paused, "");

    /* ---- the guides exist and lead where the browser says ---- */
    boot(&app);
    bb_enter(&app, BbSceneHelp);
    app.help_idx = 1;
    bb_press(&app, InputKeyOk);
    check("HOW TO PLAY / RULES opens the rules guide", app.scene == BbSceneRulesGuide, scene_name[app.scene]);
    bb_press(&app, InputKeyOk);
    bb_press(&app, InputKeyOk);
    check("whose last page opens the rule list", app.scene == BbSceneRuleList, scene_name[app.scene]);
    bb_press(&app, InputKeyOk);
    check("and a rule opens its own page", app.scene == BbSceneRuleInfo, scene_name[app.scene]);
    bb_press(&app, InputKeyDown);
    check("which steps to the next rule", app.rule_sel == 1, "");
    bb_press(&app, InputKeyBack);
    check("BACK returns to the list", app.scene == BbSceneRuleList, "");
    boot(&app);
    bb_enter(&app, BbSceneHelp);
    app.help_idx = 2;
    bb_press(&app, InputKeyOk);
    bb_press(&app, InputKeyOk);
    bb_press(&app, InputKeyOk);
    check("the reflex guide ends at the sound test", app.scene == BbSceneSoundTest, scene_name[app.scene]);

    /* ---- every screen, every cursor, inside 128x64 ---- */
    {
        uint32_t oob = 0, wide = 0;
        char worst[64] = "";
        int p = 0;
        msg[0] = 0;
        for(int s = 0; s < BbSceneCount; s++)
            for(int cur = 0; cur < 9; cur++) {
                boot(&app);
                app.now = 100000;
                memset(&app.rec, 0x11, sizeof(app.rec));
                app.rec.daily_done = (cur & 1) != 0;
                app.rec.daily_date = 20260910u;
                app.menu_idx = app.set_idx = app.reset_idx = (uint8_t)cur;
                app.score_mode = app.help_idx = app.test_btn = (int8_t)cur;
                app.ch_idx = app.ch_scroll = app.rule_sel = app.rule_scroll = (uint8_t)cur;
                app.det_row = (uint8_t)(cur % 3);
                app.det_mode = (uint8_t)(cur % 3);
                app.det_time = (uint8_t)(cur % BB_DIFF_COUNT);
                app.det_speed = (uint8_t)(cur % BB_SPEED_COUNT);
                app.set.volume = (uint8_t)(cur % BB_VOL_COUNT);
                app.set.assist = (uint8_t)(cur % BB_ASSIST_COUNT);
                app.set.diff = (uint8_t)(cur % BB_DIFF_COUNT);
                app.set.speed = (uint8_t)(cur % BB_SPEED_COUNT);
                app.mode = app.mode_idx = (uint8_t)(cur % BB_MODE_COUNT);
                app.setup_idx = (uint8_t)(cur % 3);
                app.tut_page = (uint8_t)(cur % 5);
                app.stage = (uint8_t)(1 + cur);
                app.target = (uint8_t)(4 + cur);
                app.round = (uint8_t)(1 + cur);
                app.lives = (uint8_t)(cur % 4);
                app.score = 123456;
                app.rx_hits = 99;
                app.rx_window = 400;
                app.run_mult = 22475;
                app.run_game_mode = (uint8_t)(cur % BB_MODE_COUNT);
                app.go_page = (uint8_t)(cur & 1);
                app.base.len = (uint8_t)(1 + cur);
                app.expected.len = (uint8_t)(1 + cur);
                app.rule_idx = (int8_t)(cur % BB_RULE_COUNT);
                app.rule_a = 1;
                app.rule_b = 3;
                app.scene = (BbScene)s;
                frame(&app);
                if(fake.out_of_bounds) {
                    oob += fake.out_of_bounds;
                    if(p < 180) p += sprintf(msg + p, "%s ", scene_name[s]);
                }
                if(fake.too_wide) {
                    wide += fake.too_wide;
                    strcpy(worst, fake.worst);
                }
            }
        sprintf(msg + p, "| %lu off screen, %lu wide %s", (unsigned long)oob,
                (unsigned long)wide, worst);
        check("no screen draws outside 128x64", oob == 0, msg);
        check("and no string is wider than the screen", wide == 0, msg);
    }

    /* ---- the screens say what the browser's say ---- */
    boot(&app);
    app.mode = BbModeChallenge;
    bb_start_game(&app);
    app.scene = BbSceneListen;
    frame(&app);
    check("a challenge HUD shows its length", fc_saw("LEN"), "");
    check("and no stage out of a target it has not got", !fc_saw("/"), "");
    boot(&app);
    app.mode = BbModeClassic;
    bb_start_game(&app);
    app.scene = BbSceneListen;
    frame(&app);
    check("a classic HUD shows the round and the stage", fc_saw("R1") && fc_saw("1/4"), "");

    boot(&app);
    app.mode = BbModeReflex;
    bb_enter(&app, BbSceneSetup);
    frame(&app);
    check("reflex sets a ramp, not a sequence window", fc_saw("RAMP"), "");
    check("in milliseconds", fc_saw("MS"), "");
    boot(&app);
    app.mode = BbModeClassic;
    app.set.diff = 2;
    app.set.speed = 2;
    bb_enter(&app, BbSceneSetup);
    frame(&app);
    check("and classic sets a time in seconds", fc_saw("HARD 3S"), "");
    check("with the multiplier in the footer", fc_saw("SCORE  X2.25"), "");

    boot(&app);
    app.set.volume = 0;
    app.set.assist = 0;
    bb_enter(&app, BbSceneSettings);
    frame(&app);
    check("silence with the assist off says SHAPES!", fc_saw("SHAPES!"), "");

    boot(&app);
    app.mode = BbModeRules;
    bb_start_game(&app);
    app.paused = true;
    frame(&app);
    check("a paused rules run carries its rule", fake.ops > 20, "");
    check("and says what the buttons do", fc_saw("OK: RESUME") && fc_saw("BACK: QUIT"), "");

    /* ---- the save file still round-trips ---- */
    {
        uint8_t buf[BB_SAVE_BYTES];
        boot(&app);
        app.set.volume = 1;
        app.set.assist = 3;
        app.set.tutorial_done = true;
        app.rec.ch_best[3][2] = 4321;
        size_t n = bb_save_pack(&app, buf, sizeof(buf));
        BeepbackApp b;
        boot(&b);
        check("the save block is the size the header says", n == BB_SAVE_BYTES, "");
        check("and reads back", bb_save_unpack(&b, buf, n), "");
        check("with the settings intact",
              b.set.volume == 1 && b.set.assist == 3 && b.set.tutorial_done, "");
        check("and the records", b.rec.ch_best[3][2] == 4321, "");
        check("and firstRun is the tutorial flag inverted", !b.first_run, "");
        buf[4] = 4;
        boot(&b);
        check("an older version is discarded", !bb_save_unpack(&b, buf, BB_SAVE_BYTES), "");
    }

    printf(fails ? "\n%d FAILURES\n" : "\nall port checks passed\n", fails);
    return fails ? 1 : 0;
}
