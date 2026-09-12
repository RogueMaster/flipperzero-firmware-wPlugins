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
    "splash",    "menu",       "mode",        "chpick",    "setup",     "rulecard",
    "listen",    "playback",   "go",          "input",     "hold",      "success",
    "roundclear", "wrong",     "retry",       "reflexgap", "reflexcue", "gameover",
    "settings",  "reset",      "help",        "tutorial",  "rulesguide", "rulelist",
    "ruleinfo",  "reflexguide", "soundtest",  "scorepick", "credits",   "stats",
    "table",     "nocue",
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
    check("the scene set is the browser's, less its launcher and its two dead"
          " score screens, plus stats, the table and the warning",
          BbSceneCount == 32, "");
    boot(&app);
    check("a cold start goes straight into the intro", app.scene == BbSceneSplash, "");
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
            if(!app.running) { /* BACK off the menu leaves; come back in */
                app.running = true;
                bb_enter(&app, BbSceneMenu);
            }
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
              missing ? msg : "all 31");
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
    for(int stage = 0; stage < BB_START_LEN; stage++) {
        if(!wait_scene(&app, BbSceneInput, 8000)) break;
        play_stage(&app);
        if(!wait_scene(&app, BbSceneSuccess, 400) && app.scene != BbSceneRoundClear)
            wait_scene(&app, BbSceneRoundClear, 400);
    }
    sprintf(msg, "%lu", (unsigned long)app.score);
    /* A point a note, and ten for the round. The stage that clears the
       round pays the bonus and not a stage award: hold goes to roundclear
       rather than success, and only success pays per note. 10+20+30 then 100.
       No multiplier is involved at any point, which is the whole idea. */
    check("round one pays 10+20+30 and a 100 bonus", app.score == 160, msg);
    check("and the round bonus replaced the last stage award", app.round == 2, "");

    /* Play the same round on the hardest setting there is. The score has
       to come out identical: difficulty is how hard the run is, not what
       it pays, and the whole point of dropping the multiplier was that
       the number on screen is one the player could have counted. */
    boot(&app);
    app.mode = BbModeClassic;
    app.set.diff = 3;
    app.set.speed = 2;
    bb_start_game(&app);
    for(int stage = 0; stage < BB_START_LEN; stage++) {
        if(!wait_scene(&app, BbSceneInput, 8000)) break;
        play_stage(&app);
        if(!wait_scene(&app, BbSceneSuccess, 400) && app.scene != BbSceneRoundClear)
            wait_scene(&app, BbSceneRoundClear, 400);
    }
    sprintf(msg, "%lu", (unsigned long)app.score);
    check("insane and fast pays exactly the same 160", app.score == 160, msg);

    /* ---- a best is a best at the setting you played ---- */
    boot(&app);
    app.rec.best[BbModeClassic][0][0][2] = 5000; /* an old easy and slow run */
    app.run_game_mode = BbModeClassic;
    app.run_mode = 2;
    app.run_diff = 3;
    app.run_speed = 2;
    app.score = 400;
    bb_enter(&app, BbSceneGameOver);
    /* 400 would lose to the 5000 on the headline board, which is how NEW
       BEST used to be judged and why it almost never appeared. */
    check("400 on insane beats nothing on insane, so it is a new best", app.new_best, "");
    sprintf(msg, "%lu", (unsigned long)app.rec.best[BbModeClassic][3][2][2]);
    check("and it lands in that setting's slot",
          app.rec.best[BbModeClassic][3][2][2] == 400, msg);
    check("leaving the easy record where it was",
          app.rec.best[BbModeClassic][0][0][2] == 5000, "");
    app.score = 300;
    bb_enter(&app, BbSceneGameOver);
    check("a worse run on the same setting is not a new best", !app.new_best, "");
    check("and does not lower the record",
          app.rec.best[BbModeClassic][3][2][2] == 400, "");
    sprintf(msg, "%lu", (unsigned long)app.prev_best);
    check("what it is measured against is that slot", app.prev_best == 400, msg);

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
        bb_press(&app, InputKeyBack);
        check("and BACK on the menu leaves the app", !app.running, "");
    }

    /* ---- one physical press is one action ---- */
    /* The device reports a press twice, down and up. A press that
       changes which of those two counts - BACK in a running game, which
       pauses - used to be read by both, so it paused and then quit. */
    {
        boot(&app);
        app.mode = BbModeClassic;
        bb_start_game(&app);
        wait_scene(&app, BbSceneInput, 8000);
        /* something has to be sounding, or the next check proves nothing */
        bb_tone(&app, bb_button_hz[BbBtnOk], 5000);
        check("a long note is sounding", app.tone_hz != 0, "");
        bb_input_event(&app, InputKeyBack, InputTypePress);
        check("BACK going down pauses", app.paused, "");
        check("and the note stops with it", app.tone_hz == 0, "");
        bb_tick(&app, BB_TICK_MS);
        check("and stays stopped while it is held", app.tone_hz == 0, "");
        bb_input_event(&app, InputKeyBack, InputTypeShort);
        check("and BACK coming up does not also quit", app.paused, "");
        check("so the run is still there", bb_in_game(app.scene), scene_name[app.scene]);
        bb_input_event(&app, InputKeyOk, InputTypeShort);
        check("OK then resumes it", !app.paused, "");

        /* a hold long enough that no Short ever arrives: the release has
           to clear the latch, or the next press is swallowed */
        bb_input_event(&app, InputKeyBack, InputTypePress);
        check("a held BACK still pauses", app.paused, "");
        bb_input_event(&app, InputKeyBack, InputTypeRelease);
        check("and holding it does not also quit", app.paused, "");
        check("with the latch clear again", app.press_latch == 0, "");
        /* now a second, separate press, which is what quitting takes */
        bb_input_event(&app, InputKeyBack, InputTypePress);
        bb_input_event(&app, InputKeyBack, InputTypeShort);
        check("a second BACK quits the paused run", !app.paused, "");
        check("to the menu", app.scene == BbSceneMenu, scene_name[app.scene]);

        /* a game button is acted on going down, not twice */
        boot(&app);
        app.mode = BbModeClassic;
        bb_start_game(&app);
        wait_scene(&app, BbSceneInput, 8000);
        uint8_t want = app.expected.press[0];
        bb_input_event(&app, key_of(want), InputTypePress);
        uint8_t after = app.input_idx;
        bb_input_event(&app, key_of(want), InputTypeShort);
        sprintf(msg, "%u then %u", after, app.input_idx);
        check("a game press counts once, on the way down",
              after == 1 && app.input_idx == 1, msg);

        /* and in a menu it is the release that counts, once */
        boot(&app);
        bb_enter(&app, BbSceneMenu);
        bb_input_event(&app, InputKeyDown, InputTypePress);
        check("a menu press does nothing going down", app.menu_idx == 0, "");
        bb_input_event(&app, InputKeyDown, InputTypeShort);
        check("and moves one row coming up", app.menu_idx == 1, "");
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
    sprintf(msg, "%lu vs %u hits", (unsigned long)app.score, app.rx_hits);
    check("and is worth what a note is worth, whatever the window",
          app.score == BB_NOTE_POINTS * app.rx_hits, msg);
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
                app.stat_page = (uint8_t)(cur % BB_STAT_PAGES);
                app.tbl_assist = (uint8_t)(cur % BB_ASSIST_COUNT);
                app.tbl_scroll = (uint8_t)(cur % BB_RULE_COUNT);
                app.score_mode = (uint8_t)(cur % BB_MODE_COUNT);
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
    check("and promises no multiplier for it", !fc_saw("X2.25") && !fc_saw("SCORE"), "");

    /* LEFT goes back wherever it is not already spoken for. Nothing on
       screen says so - the device has a BACK button and that is the
       obvious way out - but the shortcut has to keep working, and it must
       never fire on a screen where LEFT means something else. */
    {
        uint8_t went = 0, wrong = 0;
        int p = 0;
        msg[0] = 0;
        for(uint8_t sc = 0; sc < BbSceneCount; sc++) {
            if(bb_in_game((BbScene)sc) || sc == BbSceneSplash) continue;
            boot(&app);
            app.first_run = false;
            app.scene = (BbScene)sc;
            if(!bb_left_is_back(&app)) continue;
            BbScene to = bb_back_target(&app);
            bb_press(&app, InputKeyLeft);
            if(app.scene == to) {
                went++;
            } else {
                wrong++;
                if(p < 150) p += sprintf(msg + p, "%s ", scene_name[sc]);
            }
        }
        sprintf(msg + p, "| %u screens", went);
        check("LEFT goes where the back table says", wrong == 0, msg);
        check("on most of the menus", went >= 8, msg);
    }

    boot(&app);
    bb_enter(&app, BbSceneScorePick);
    frame(&app);
    check("BEST SCORES points right, where the credits are",
          fc_ink_in(119, 28, 127, 44), "");

    /* SETTINGS is where it must not fire: LEFT is adjusting a value on the
       top two rows and would throw the screen away mid-change. */
    boot(&app);
    bb_enter(&app, BbSceneSettings);
    app.set_idx = 0;
    app.set.volume = 3;
    bb_press(&app, InputKeyLeft);
    check("LEFT turns the volume down rather than leaving",
          app.scene == BbSceneSettings && app.set.volume == 2, "");
    app.set_idx = 2;
    bb_press(&app, InputKeyLeft);
    check("but leaves from a row it does nothing to", app.scene == BbSceneMenu,
          scene_name[app.scene]);

    boot(&app);
    app.set.assist = 0; /* the hardest way to play, with the sound still on */
    bb_enter(&app, BbSceneSettings);
    app.set_idx = 1;
    frame(&app);
    check("the quietest assist is called EARS, not OFF",
          fc_saw("EARS") && !fc_saw("OFF"), "");

    /* Silence with no cues is playable by nobody, but it is allowed: the
       row says what you picked and the way out of SETTINGS asks once. */
    boot(&app);
    app.set.volume = 0;
    app.set.assist = 0;
    bb_enter(&app, BbSceneSettings);
    app.set_idx = 1;
    frame(&app);
    check("the assist row says what you actually chose",
          fc_saw("EARS") && !fc_saw("SHAPES"), "");
    check("and the game does not quietly pick another one", bb_assist(&app) == 0, "");
    app.set_idx = 2; /* a row LEFT does not adjust, so LEFT is the way out */
    bb_press(&app, InputKeyLeft);
    check("leaving warns first", app.scene == BbSceneNoCue, scene_name[app.scene]);
    frame(&app);
    check("in as many words", fc_saw("IMPOSSIBLE") && fc_saw("NOTHING WILL TELL YOU"), "");
    bb_press(&app, InputKeyBack);
    check("BACK goes back to fix it", app.scene == BbSceneSettings, scene_name[app.scene]);
    app.set_idx = 2;
    bb_press(&app, InputKeyLeft);
    bb_press(&app, InputKeyOk);
    check("and OK plays it anyway", app.scene == BbSceneMenu, scene_name[app.scene]);
    check("with the settings left exactly as chosen",
          app.set.volume == 0 && app.set.assist == 0, "");
    app.set.volume = 2;
    bb_enter(&app, BbSceneSettings);
    app.set_idx = 2;
    bb_press(&app, InputKeyLeft);
    check("turning the sound back on makes the way out plain again",
          app.scene == BbSceneMenu, scene_name[app.scene]);

    boot(&app);
    app.mode = BbModeRules;
    bb_start_game(&app);
    app.paused = true;
    frame(&app);
    check("a paused rules run carries its rule", fake.ops > 20, "");
    check("and says what the buttons do", fc_saw("OK: RESUME") && fc_saw("BACK: QUIT"), "");

    /* ---- every control that looks adjustable actually adjusts ---- */
    /* The setup rows for classic and rules did nothing for a while: the
       rows were built without saying what they were for, so LEFT and
       RIGHT fell through. Nothing here is spot-checked any more. */
    {
        int dead = 0, p2 = 0;
        msg[0] = 0;
        for(uint8_t m = 0; m < BB_MODE_COUNT; m++) {
            if(m == BbModeDaily) continue; /* the day picks those, on purpose */
            boot(&app);
            app.mode = m;
            app.ch_idx = 0;
            bb_enter(&app, BbSceneSetup);
            uint8_t rows = 0;
            {
                const char* l[4];
                const char* v[4];
                uint8_t k[4];
                char a[24], b[24];
                rows = bb_setup_rows(&app, l, v, k, a, b, sizeof(a));
            }
            for(uint8_t r = 0; r + 1 < rows; r++) { /* the last row is the action */
                app.setup_idx = r;
                app.set.diff = 1;
                app.set.speed = 1;
                uint8_t d0 = app.set.diff, s0 = app.set.speed;
                bb_press(&app, InputKeyRight);
                if(app.set.diff == d0 && app.set.speed == s0) {
                    dead++;
                    p2 += sprintf(msg + p2, "%s row %u ", bb_mode_name[m], r);
                }
            }
        }
        sprintf(msg + p2, "| %d dead", dead);
        check("every setup row in every mode responds to LEFT and RIGHT", dead == 0, msg);

        /* the daily is locked, and that is not the same as broken */
        bb_seed_override = 20260910u;
        boot(&app);
        app.mode = BbModeDaily;
        bb_enter(&app, BbSceneSetup);
        uint8_t d0 = app.set.diff;
        for(uint8_t r = 0; r < 4; r++) {
            app.setup_idx = r;
            bb_press(&app, InputKeyRight);
            bb_press(&app, InputKeyLeft);
        }
        check("but the daily's rows stay where the day put them",
              app.set.diff == d0 && app.setup_idx == 3, "");
        bb_seed_override = 0;
    }

    /* settings, and the scores detail, the same way */
    {
        boot(&app);
        bb_enter(&app, BbSceneSettings);
        app.set_idx = 0;
        app.set.volume = 1;
        bb_press(&app, InputKeyRight);
        check("VOLUME moves", app.set.volume == 2, "");
        bb_press(&app, InputKeyLeft);
        check("and back", app.set.volume == 1, "");
        app.set_idx = 1;
        app.set.assist = 1;
        bb_press(&app, InputKeyRight);
        check("ASSIST moves", app.set.assist == 2, "");
        for(int i = 0; i < 9; i++) bb_press(&app, InputKeyRight);
        check("and stops at the last one", app.set.assist == BB_ASSIST_COUNT - 1, "");

        bb_enter(&app, BbSceneScorePick);
        bb_press(&app, InputKeyDown);
        check("the records list walks the modes", app.score_mode == 1, "");
        bb_press(&app, InputKeyOk);
        check("and OK opens that mode's table", app.scene == BbSceneTable,
              scene_name[app.scene]);
        bb_press(&app, InputKeyRight);
        check("RIGHT turns the table to the next assist", app.tbl_assist == 1, "");
        for(int i = 0; i < BB_ASSIST_COUNT; i++) bb_press(&app, InputKeyRight);
        check("and comes back round rather than stopping", app.tbl_assist == 1, "");

        bb_enter(&app, BbSceneChPick);
        app.ch_idx = 0;
        for(int i = 0; i < 9; i++) bb_press(&app, InputKeyDown);
        sprintf(msg, "at %u of %u, scrolled to %u", app.ch_idx, BB_RULE_COUNT + 1, app.ch_scroll);
        check("the rule picker walks all eight and stops",
              app.ch_idx == BB_RULE_COUNT && app.ch_scroll == BB_RULE_COUNT + 1 - 4, msg);
    }

    /* ---- the tutorial is a first-launch thing, once ---- */
    {
        boot(&app);
        check("a fresh install has not seen the tutorial", !app.set.tutorial_done, "");
        bb_splash_done(&app);
        check("the first launch opens it", app.scene == BbSceneTutorial, scene_name[app.scene]);
        check("and records that it did", app.set.tutorial_done, "");

        /* what the save carries is what decides the next launch */
        uint8_t buf[BB_SAVE_BYTES];
        bb_save_pack(&app, buf, sizeof(buf));
        BeepbackApp next;
        boot(&next);
        bb_save_unpack(&next, buf, BB_SAVE_BYTES);
        check("so a later launch does not", !next.first_run, "");
        bb_splash_done(&next);
        check("and goes straight to the menu", next.scene == BbSceneMenu, scene_name[next.scene]);

        /* until RESET / TUTORIAL asks for it back */
        bb_enter(&next, BbSceneReset);
        next.reset_idx = BbResetTutorial;
        bb_press(&next, InputKeyOk);
        check("one press on a reset does nothing at all",
              !next.first_run && next.set.tutorial_done, "");
        bb_press(&next, InputKeyOk);
        check("RESET TUTORIAL asks for it again", next.first_run && !next.set.tutorial_done, "");
        check("and stays in the app, because only EVERYTHING leaves", next.running, "");
    }

    /* ---- the daily is one run, and it borrows your settings ---- */
    {
        bb_seed_override = 20260910u;
        boot(&app);
        app.set.diff = 3;
        app.set.speed = 2;
        bb_daily_refresh(&app, bb_daily_seed());
        app.mode = BbModeDaily;
        bb_enter(&app, BbSceneSetup);
        bb_press(&app, InputKeyOk);
        check("the daily runs at NORMAL whatever you had set",
              app.set.diff == 1 && app.set.speed == 1, "");
        bb_enter(&app, BbSceneGameOver);
        sprintf(msg, "diff %u speed %u", app.set.diff, app.set.speed);
        check("and gives your own settings back when it ends",
              app.set.diff == 3 && app.set.speed == 2, msg);

        app.now += BB_OVER_LOCK + 1;
        bb_press(&app, InputKeyOk);
        check("OK on its game over does not hand it back for another go",
              app.scene == BbSceneGameOver, scene_name[app.scene]);
        bb_enter(&app, BbSceneSetup);
        bb_press(&app, InputKeyOk);
        check("and neither does the setup screen", app.scene == BbSceneSetup, "");

        /* while any other mode plays again from there, as it should */
        boot(&app);
        app.mode = BbModeClassic;
        bb_start_game(&app);
        bb_enter(&app, BbSceneGameOver);
        app.now += BB_OVER_LOCK + 1;
        bb_press(&app, InputKeyOk);
        check("but classic does play again from its game over",
              bb_in_game(app.scene), scene_name[app.scene]);
        bb_seed_override = 0;
    }

    /* ---- the volume is worth something on this speaker ---- */
    check("silence is silent", bb_vol_gain[0] == 0, "");
    sprintf(msg, "%u %u %u %u", bb_vol_gain[0], bb_vol_gain[1], bb_vol_gain[2], bb_vol_gain[3]);
    check("and the loudest step drives the speaker fully", bb_vol_gain[BB_VOL_COUNT - 1] == 100, msg);
    check("with every step louder than the last",
          bb_vol_gain[1] < bb_vol_gain[2] && bb_vol_gain[2] < bb_vol_gain[3], msg);

    /* ---- a flash always reaches the LED ---- */
    {
        boot(&app);
        bb_led_flash(&app, BbLedGreen, 100);
        uint8_t gen = app.led_gen;
        bb_led_flash(&app, BbLedGreen, 100);
        check("the same colour twice is two flashes, not one", app.led_gen != gen, "");
        bb_tick(&app, 200);
        check("and it goes out on its own", app.led == BbLedOff, "");
    }

    /* ---- a reset asks first ---- */
    boot(&app);
    app.rec.best[BbModeClassic][1][1][2] = 500;
    app.stats.runs = 9;
    bb_enter(&app, BbSceneReset);
    app.reset_idx = BbResetRecords;
    bb_press(&app, InputKeyOk);
    check("one press arms it and wipes nothing",
          app.rec.best[BbModeClassic][1][1][2] == 500, "");
    frame(&app);
    check("and says so", fc_saw("SURE?"), "");
    bb_press(&app, InputKeyOk);
    check("the second press inside the second does it",
          app.rec.best[BbModeClassic][1][1][2] == 0, "");
    check("and leaves the stats alone, which are the other row",
          app.stats.runs == 9, "");
    frame(&app);
    check("the row says it is done", fc_saw("DONE") && !fc_saw("SURE?"), "");

    boot(&app);
    app.stats.runs = 9;
    bb_enter(&app, BbSceneReset);
    app.reset_idx = BbResetStats;
    bb_press(&app, InputKeyOk);
    for(int i = 0; i < 60; i++) bb_tick(&app, BB_TICK_MS); /* well past a second */
    bb_press(&app, InputKeyOk);
    check("a second press too late arms it again rather than firing",
          app.stats.runs == 9, "");
    bb_press(&app, InputKeyOk);
    check("and the one after that clears the stats", app.stats.runs == 0, "");

    boot(&app);
    app.rec.daily_best[1] = 70;
    bb_enter(&app, BbSceneReset);
    app.reset_idx = BbResetRecords;
    bb_press(&app, InputKeyOk);
    bb_press(&app, InputKeyDown);
    bb_press(&app, InputKeyOk);
    check("moving to another row disarms the one you left",
          app.rec.daily_best[1] == 70, "");

    boot(&app);
    app.stats.runs = 3;
    app.rec.daily_best[1] = 70;
    app.set.diff = 3;
    bb_enter(&app, BbSceneReset);
    app.reset_idx = BbResetAll;
    bb_press(&app, InputKeyOk);
    check("EVERYTHING asks like the rest", app.running, "");
    bb_press(&app, InputKeyOk);
    check("then takes the lot", app.stats.runs == 0 && app.rec.daily_best[1] == 0, "");
    check("settings included", app.set.diff == 1 && app.set.assist == 2, "");
    check("and it is the only row that leaves the app", !app.running, "");

    /* ---- the records section ---- */
    boot(&app);
    app.first_run = false;
    bb_enter(&app, BbSceneMenu);
    bb_press(&app, InputKeyRight);
    check("RIGHT off the menu opens the stats", app.scene == BbSceneStats,
          scene_name[app.scene]);
    frame(&app);
    check("which counts what you have done", fc_saw("RUNS") && fc_saw("PLAYED"), "");
    bb_press(&app, InputKeyRight);
    check("RIGHT turns the page", app.stat_page == 1, "");
    frame(&app);
    check("to the favourites", fc_saw("FAVOURITE") && fc_saw("LONGEST"), "");
    check("which say nothing rather than nought before you have played",
          fc_saw("-"), "");
    bb_press(&app, InputKeyRight);
    check("and stops at the last page", app.stat_page == 1, "");
    bb_press(&app, InputKeyLeft);
    check("LEFT is the page before", app.stat_page == 0 && app.scene == BbSceneStats, "");
    bb_press(&app, InputKeyLeft);
    check("and the way out from the first", app.scene == BbSceneMenu, scene_name[app.scene]);

    boot(&app);
    app.rec.best[BbModeClassic][2][2][1] = 880; /* hard, fast, led */
    bb_enter(&app, BbSceneStats);
    bb_press(&app, InputKeyOk);
    check("OK opens the records", app.scene == BbSceneScorePick, scene_name[app.scene]);
    bb_press(&app, InputKeyOk);
    check("and a mode opens its table", app.scene == BbSceneTable, scene_name[app.scene]);
    frame(&app);
    check("which is every time against every speed at once",
          fc_saw("EASY") && fc_saw("INSN") && fc_saw("SLOW") && fc_saw("FAST"), "");
    check("on EARS to begin with, and says so", fc_saw("EARS"), "");
    check("where nothing has been set, so every cell is a dash",
          !fc_saw("880"), "");
    bb_press(&app, InputKeyRight);
    frame(&app);
    check("RIGHT turns it to the next assist", fc_saw("LED"), "");
    check("and there is the hard and fast record", fc_saw("880"), "");
    bb_press(&app, InputKeyLeft);
    check("LEFT goes back to the mode list", app.scene == BbSceneScorePick,
          scene_name[app.scene]);

    boot(&app);
    app.score_mode = BbModeChallenge;
    app.rec.ch_best[0][0] = 640;
    bb_enter(&app, BbSceneTable);
    frame(&app);
    check("challenge lists its rules instead of a grid",
          fc_saw("SKIP") && fc_saw("640"), "");
    bb_press(&app, InputKeyDown);
    check("and scrolls them", app.tbl_scroll == 1, "");
    boot(&app);
    app.score_mode = BbModeDaily;
    app.rec.daily_best[2] = 310;
    bb_enter(&app, BbSceneTable);
    frame(&app);
    check("the daily is one number an assist, all four at once",
          fc_saw("SHAPES") && fc_saw("310"), "");

    /* the counters behind the stats pages */
    boot(&app);
    app.mode = BbModeClassic;
    bb_start_game(&app);
    check("starting a run counts it", app.stats.runs == 1, "");
    check("under the mode it was played in", app.stats.by_mode[BbModeClassic] == 1, "");
    check("and the assist it was played with", app.stats.by_assist[2] == 1, "");
    for(int stage = 0; stage < BB_START_LEN; stage++) {
        if(!wait_scene(&app, BbSceneInput, 8000)) break;
        play_stage(&app);
        if(!wait_scene(&app, BbSceneSuccess, 400) && app.scene != BbSceneRoundClear)
            wait_scene(&app, BbSceneRoundClear, 400);
    }
    sprintf(msg, "%lu notes, %lu rounds", (unsigned long)app.stats.notes,
            (unsigned long)app.stats.rounds);
    check("every note played back right is counted", app.stats.notes == 6, msg);
    check("and every round cleared", app.stats.rounds == 1, msg);
    check("playtime is the time a run was running", app.stats.play_ms > 0, "");
    {
        uint32_t held = app.stats.play_ms;
        bb_enter(&app, BbSceneMenu);
        for(int i = 0; i < 40; i++) bb_tick(&app, BB_TICK_MS);
        check("and not the time the menu was open", app.stats.play_ms == held, "");
    }

    /* ---- the save file still round-trips ---- */
    {
        uint8_t buf[BB_SAVE_BYTES];
        boot(&app);
        app.set.volume = 1;
        app.set.assist = 3;
        app.set.tutorial_done = true;
        app.rec.ch_best[3][2] = 4321;
        app.stats.runs = 77;
        app.stats.play_ms = 1234567;
        app.stats.notes = 9001;
        app.stats.longest = 19;
        app.stats.by_mode[BbModeRules] = 40;
        app.stats.by_assist[1] = 31;
        size_t n = bb_save_pack(&app, buf, sizeof(buf));
        BeepbackApp b;
        boot(&b);
        check("the save block is the size the header says", n == BB_SAVE_BYTES, "");
        check("and reads back", bb_save_unpack(&b, buf, n), "");
        check("with the settings intact",
              b.set.volume == 1 && b.set.assist == 3 && b.set.tutorial_done, "");
        check("and the records", b.rec.ch_best[3][2] == 4321, "");
        check("and the stats, which are their own thing",
              b.stats.runs == 77 && b.stats.play_ms == 1234567 && b.stats.notes == 9001 &&
                  b.stats.longest == 19 && b.stats.by_mode[BbModeRules] == 40 &&
                  b.stats.by_assist[1] == 31,
              "");
        check("and firstRun is the tutorial flag inverted", !b.first_run, "");
        buf[4] = 4;
        boot(&b);
        check("an older version is discarded", !bb_save_unpack(&b, buf, BB_SAVE_BYTES), "");
    }

    printf(fails ? "\n%d FAILURES\n" : "\nall port checks passed\n", fails);
    return fails ? 1 : 0;
}
