/* Host-side check of the screens. Nothing is rendered; the canvas is a
   bookkeeper that records the rectangle every call would have touched,
   which is the only way to catch an arrow drawn past the bottom edge. */
#define BB_HOST_TEST 1
#include <stdio.h>
#include <string.h>
#include "beepback.h"
#include "../beepback_rules.c"
#include "../beepback_nav.c"
#include "../beepback_game.c"
#include "fake_canvas.h"
#include "../beepback_draw.c"
#include "../beepback_intro.c"

static int fails = 0;
static void check(const char* name, int ok, const char* extra) {
    printf("%s %s%s%s\n", ok ? "ok  " : "FAIL", name, extra && *extra ? "  -> " : "", extra ? extra : "");
    if(!ok) fails++;
}

static const char* const scene_name[BbSceneCount] = {
    "launcher", "splash",   "tutorial", "menu",      "modeselect", "rulepick", "setup",
    "game",     "pause",    "over",     "howtopick", "howto",      "soundtest", "settings",
    "sounds",   "scores",   "detail",   "reset",     "boardpick",  "board",    "credits",
};

static const InputKey keys[6] = {
    InputKeyUp, InputKeyDown, InputKeyLeft, InputKeyRight, InputKeyOk, InputKeyBack};

/* Draw one frame and hand back what went wrong with it. */
static void frame(BeepbackApp* app) {
    fake_canvas_reset();
    bb_draw((Canvas*)app, app);
}

int main(void) {
    char msg[300];
    BeepbackApp app;

    /* ---- the canvas really does catch what it claims to ---- */
    fake_canvas_reset();
    canvas_set_font((Canvas*)0, FontSecondary);
    canvas_draw_str_aligned((Canvas*)0, 64, 62, AlignCenter, AlignCenter, "OFF THE BOTTOM");
    check("a 9px line centred at y=62 is caught", fake.out_of_bounds == 1, "");
    fake_canvas_reset();
    canvas_draw_str_aligned((Canvas*)0, 64, BB_FOOT_Y, AlignCenter, AlignCenter, "IN THE FOOTER");
    check("and the same line in the footer band is fine", fake.out_of_bounds == 0, "");
    fake_canvas_reset();
    canvas_draw_str((Canvas*)0, 0, 30, "A STRING FAR TOO LONG TO FIT ON THIS LITTLE SCREEN");
    check("an over-wide string is caught", fake.too_wide == 1, "");

    /* ---- every scene, every cursor, drawn and measured ---- */
    {
        uint32_t oob = 0, wide = 0;
        int p = 0;
        msg[0] = 0;
        char widest[64] = "";
        for(int s = 0; s < BbSceneCount; s++) {
            for(int cur = 0; cur < 9; cur++) {
                bb_app_init(&app);
                app.seed = 0x5150u + (uint32_t)cur;
                app.now = 100000;
                /* fill the records so no field is drawn at its narrowest */
                memset(&app.rec, 0x11, sizeof(app.rec));
                app.rec.daily_done = (cur & 1) != 0; /* memset leaves no valid bool */
                app.rec.daily_date = 20260910u;
                app.menu_cur = app.settings_cur = app.scores_cur = app.reset_cur = (uint8_t)cur;
                app.board_cur = app.howto_cur = app.sounds_cur = app.sound_btn = (uint8_t)cur;
                app.rule_cur = (uint8_t)cur;
                app.det_mode = (uint8_t)(cur % BB_MODE_COUNT);
                app.det_cur = (uint8_t)(cur % 3);
                app.det_diff = (uint8_t)(cur % BB_DIFF_COUNT);
                app.det_speed = (uint8_t)(cur % BB_SPEED_COUNT);
                app.set.diff = (uint8_t)(cur % BB_DIFF_COUNT);
                app.set.speed = (uint8_t)(cur % BB_SPEED_COUNT);
                app.set.assist = (uint8_t)(cur % BB_ASSIST_COUNT);
                app.set.volume = (uint8_t)(cur % BB_VOL_COUNT);
                app.setup_cur = (uint8_t)(cur % 3);
                app.mode_page = (uint8_t)(cur % 3);
                app.mode_row = (uint8_t)(cur % 2);
                if(app.mode_row >= bb_list_count(&app, BbSceneModeSelect)) app.mode_row = 0;
                app.tut_page = (uint8_t)(cur % 4);
                app.howto_cur = (uint8_t)(cur % 3);
                app.howto_page = (uint8_t)(cur % bb_howto_pages(app.howto_cur));
                app.run.mode = (BbMode)(cur % BB_MODE_COUNT);
                app.scene = (BbScene)s;
                app.scene_at = app.now - (uint32_t)cur * 100;
                frame(&app);
                if(fake.out_of_bounds) {
                    oob += fake.out_of_bounds;
                    if(p < 200) p += sprintf(msg + p, "%s ", scene_name[s]);
                }
                if(fake.too_wide) {
                    wide += fake.too_wide;
                    if(fake.worst_w > 128) strcpy(widest, fake.worst);
                }
            }
        }
        sprintf(msg + p, "| %lu off screen, %lu too wide %s", (unsigned long)oob, (unsigned long)wide, widest);
        check("no screen draws outside 128x64", oob == 0, msg);
        check("and no string is wider than the screen", wide == 0, msg);
    }

    /* ---- every phase of every mode ---- */
    {
        uint32_t oob = 0, wide = 0;
        char worst[64] = "";
        int p = 0;
        msg[0] = 0;
        for(int mode = 0; mode < BB_MODE_COUNT; mode++) {
            for(int phase = 0; phase < BbPhaseCount; phase++) {
                for(int assist = 0; assist < BB_ASSIST_COUNT; assist++) {
                    for(int big = 0; big < 2; big++) {
                        bb_app_init(&app);
                        app.seed = 99u;
                        app.set.assist = (uint8_t)assist;
                        app.run.mode = (BbMode)mode;
                        bb_run_start(&app, (BbMode)mode);
                        app.scene = BbSceneGame;
                        app.now = 50000;
                        app.scene_at = 49000;
                        BbRun* r = &app.run;
                        r->phase = (BbPhase)phase;
                        r->phase_end = app.now + 100;
                        /* the worst case for the HUD: everything at its widest */
                        if(big) {
                            r->score = 999999;
                            r->round = 99;
                            r->hits = 99999;
                            r->target = BB_MAX_SEQ;
                            r->seq.len = BB_MAX_SEQ;
                            r->shown = BB_MAX_SEQ;
                            r->award = 99999;
                            r->bonus = 99999;
                            bb_run_build_presses(r);
                            r->idx = r->press.len;
                        }
                        r->play_i = (uint8_t)(r->shown ? r->shown - 1 : 0);
                        r->play_on = (big != 0);
                        r->flash_end = app.now + 10;
                        r->flash_btn = (uint8_t)(phase % BbBtnCount);
                        r->win_ms = 4000;
                        r->win_end = app.now + 2000;
                        frame(&app);
                        if(fake.out_of_bounds) {
                            oob += fake.out_of_bounds;
                            if(p < 180)
                                p += sprintf(msg + p, "%s/p%d/a%d ", bb_mode_name[mode], phase, assist);
                        }
                        if(fake.too_wide) {
                            wide += fake.too_wide;
                            strcpy(worst, fake.worst);
                        }
                    }
                }
            }
        }
        sprintf(msg + p, "| %lu off screen, %lu wide %s", (unsigned long)oob, (unsigned long)wide, worst);
        check("no game phase draws outside the screen either", oob == 0, msg);
        check("nor prints anything too wide for it", wide == 0, msg);
    }

    /* ---- the HUD says the right thing per mode ---- */
    bb_app_init(&app);
    app.seed = 7;
    bb_run_start(&app, BbModeChallenge);
    app.scene = BbSceneGame;
    app.run.phase = BbPhaseInput;
    frame(&app);
    check("a challenge HUD shows its length", fc_saw("LEN"), "");
    check("and no stage out of a target it does not have", !fc_saw("/"), "");

    bb_app_init(&app);
    app.seed = 7;
    bb_run_start(&app, BbModeClassic);
    app.scene = BbSceneGame;
    app.run.phase = BbPhaseInput;
    frame(&app);
    check("a classic HUD shows the stage and its target", fc_saw("/"), "");

    bb_app_init(&app);
    app.seed = 7;
    bb_run_start(&app, BbModeReflex);
    app.scene = BbSceneGame;
    frame(&app);
    check("reflex says a miss ends it before the first cue", fc_saw("ONE MISS ENDS IT"), "");
    check("and that BACK is not a pause", fc_saw("BACK ENDS THE RUN"), "");

    /* ---- the setup footer shows the multiplier, and it moves ---- */
    bb_app_init(&app);
    app.run.mode = BbModeClassic;
    app.set.diff = 2;
    app.set.speed = 1;
    bb_go(&app, BbSceneSetup);
    frame(&app);
    check("the setup footer shows what the run is worth", fc_saw("SCORE  X1.55"), "");
    app.set.diff = 1;
    frame(&app);
    check("and follows the dials", fc_saw("SCORE  X1.00"), "");

    /* ---- what game over reports is what was banked ---- */
    bb_app_init(&app);
    app.seed = 3;
    bb_run_start(&app, BbModeClassic);
    app.run.score = 1234;
    app.run.mult = 15500;
    app.run.record = true;
    bb_go(&app, BbSceneOver);
    app.now += BB_OVER_LOCK;
    frame(&app);
    check("game over prints the score", fc_saw("1234"), "");
    check("with the multiplier beside it", fc_saw("X1.55"), "");
    check("and says when it was a best", fc_saw("NEW BEST"), "");

    /* ---- arrows appear only where you can go ---- */
    bb_app_init(&app);
    bb_go(&app, BbSceneModeSelect);
    frame(&app);
    uint32_t first_page = fake.ops;
    app.mode_page = 1;
    frame(&app);
    check("the middle page offers both directions", fake.ops > first_page, "");
    app.mode_page = 2;
    frame(&app);
    check("and the last page offers fewer", fake.ops < first_page + 4, "");

    /* the wipe covers the screen at the moment game over opens */
    bb_app_init(&app);
    bb_go(&app, BbSceneOver);
    frame(&app);
    check("the game over wipe starts covering everything", fake.out_of_bounds == 0, "");
    app.now += BB_OVER_LOCK / 2;
    frame(&app);
    check("and halfway down is still on screen", fake.out_of_bounds == 0, "");

    /* ---- the screens the browser build argued over ---- */

    /* Pausing is when you have forgotten what you are obeying, so the
       pause screen carries the rule, and the progress the HUD was
       showing, and the lives. */
    {
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
        frame(&app);
        check("pause names the rule", fc_saw("SKIP"), "");
        check("and the button the rule is about", fc_saw("OK"), "");
        check("and the round", fc_saw("ROUND 3"), "");
        check("and the stage out of its target", fc_saw("2/6"), "");
        check("and says what the buttons do", fc_saw("OK: RESUME") && fc_saw("BACK: QUIT"), "");
        sprintf(msg, "%lu ops", (unsigned long)fake.ops);
        check("with nothing off screen", fake.out_of_bounds == 0 && fake.too_wide == 0, msg);

        /* a challenge has no rounds, so it says what it does have */
        bb_app_init(&app);
        app.seed = 5;
        bb_run_start(&app, BbModeChallenge);
        bb_go(&app, BbScenePause);
        frame(&app);
        check("a paused challenge shows its length, not a round", fc_saw("LEN"), "");
        check("and no stage out of a target it has not got", !fc_saw("/"), "");

        /* classic has no rule to show */
        bb_app_init(&app);
        app.seed = 5;
        bb_run_start(&app, BbModeClassic);
        bb_go(&app, BbScenePause);
        frame(&app);
        check("a paused classic names the mode instead", fc_saw("CLASSIC"), "");
    }

    /* Game over keeps the run's settings on a second page, because a
       record only means something later if you can see what it was set
       to. */
    {
        bb_app_init(&app);
        app.seed = 4;
        bb_run_start(&app, BbModeClassic);
        app.run.score = 1240;
        app.run.mult = 15500;
        app.run.longest = 7;
        app.run.prev_best = 980;
        bb_go(&app, BbSceneOver);
        app.now += BB_OVER_LOCK;
        frame(&app);
        check("game over leads with the multiplier, then the score",
              fc_saw("X1.55  1240"), "");
        check("and says how far the run got", fc_saw("LONGEST") && fc_saw("7"), "");
        check("and what it was trying to beat", fc_saw("PREVIOUS BEST") && fc_saw("980"), "");

        bb_input(&app, InputKeyRight);
        frame(&app);
        check("a right press opens the second page", app.over_page == 1, "");
        check("which carries the mode", fc_saw("MODE") && fc_saw("CLASSIC"), "");
        check("and the assist", fc_saw("ASSIST"), "");
        check("and the time and speed", fc_saw("TIME") && fc_saw("SPEED"), "");
        check("with the score nowhere on it", !fc_saw("1240"), "");
        check("and nothing off screen", fake.out_of_bounds == 0 && fake.too_wide == 0, "");
        bb_input(&app, InputKeyLeft);
        check("and a left press comes back", app.over_page == 0, "");
        bb_go(&app, BbSceneOver);
        check("opening it fresh starts on the first page", app.over_page == 0, "");

        /* reflex counts hits, not sequence length */
        bb_app_init(&app);
        app.seed = 4;
        bb_run_start(&app, BbModeReflex);
        app.run.hits = 23;
        bb_go(&app, BbSceneOver);
        app.now += BB_OVER_LOCK;
        frame(&app);
        check("a reflex game over counts hits", fc_saw("HITS") && fc_saw("23"), "");
        check("and never claims a longest sequence", !fc_saw("LONGEST"), "");
    }

    /* Mode select is the only place the game explains the modes. */
    {
        bb_app_init(&app);
        bb_go(&app, BbSceneModeSelect);
        app.mode_page = 1;
        app.mode_row = 1;
        frame(&app);
        check("mode select is titled MODE", fc_saw("MODE"), "");
        check("and says what the highlighted mode is", fc_saw("ONE RULE, NO ROUNDS"), "");
        check("with the selection inverted rather than bulleted", fake.rboxes == 1, "");
        int blurbs = 0;
        for(uint8_t m = 0; m < BB_MODE_COUNT; m++) {
            app.mode_page = (uint8_t)(m / 2);
            app.mode_row = (uint8_t)(m % 2);
            frame(&app);
            if(fc_saw(bb_mode_blurb[m])) blurbs++;
        }
        sprintf(msg, "%d of %d", blurbs, BB_MODE_COUNT);
        check("every mode has a line of its own", blurbs == BB_MODE_COUNT, msg);
    }

    /* Settings fits all five rows, so it has no footer and never scrolls. */
    {
        bb_app_init(&app);
        app.set.volume = 2;
        bb_go(&app, BbSceneSettings);
        frame(&app);
        int rows = fc_saw("VOLUME") + fc_saw("ASSIST") + fc_saw("SOUNDS") + fc_saw("SCORES") +
                   fc_saw("RESET");
        sprintf(msg, "%d of 5 rows", rows);
        check("settings shows all five rows at once", rows == 5, msg);
        check("with the volume named, not numbered", fc_saw("MID") && !fc_saw("2"), "");
        app.set.volume = BB_VOL_COUNT - 1;
        frame(&app);
        check("and the loudest step is HIGH, not a fifth level", fc_saw("HIGH"), "");
        app.set.volume = 0;
        app.set.assist = BbAssistOff;
        frame(&app);
        check("and silence with ears only says what it fell back to",
              fc_saw("ASSIST (SILENT)") && fc_saw("SHAPES"), "");
    }

    /* An adjustable row shows an arrow only where a press would move. */
    {
        bb_app_init(&app);
        app.run.mode = BbModeClassic;
        bb_go(&app, BbSceneSetup);
        app.setup_cur = 0;
        app.set.diff = 0;
        frame(&app);
        check("at the bottom of a range there is no left arrow", !fc_saw("<"), "");
        check("but there is a right one", fc_saw(">"), "");
        app.set.diff = BB_DIFF_COUNT - 1;
        frame(&app);
        check("and at the top the right one is gone", !fc_saw(">"), "");
        check("with the left one there", fc_saw("<"), "");
        app.set.diff = 1;
        frame(&app);
        check("in the middle both are offered", fc_saw("<") && fc_saw(">"), "");
        check("and the row carries its window in seconds", fc_saw("NORMAL 4S"), "");
    }

    /* ---- the pop-in on a playback cue ---- */
    /* A cue is drawn inverted for the first BB_FLASH_MS of its step, so
       it lands rather than fades in and two of the same button running
       read as two hits. Playback only, and only where something is
       drawn at all. */
    {
        static const uint8_t assists[4] = {BbAssistOff, BbAssistLed, BbAssistShapes, BbAssistArrows};
        uint32_t early[4], late[4];
        for(int i = 0; i < 4; i++) {
            bb_app_init(&app);
            app.seed = 11;
            app.set.assist = assists[i];
            bb_run_start(&app, BbModeClassic);
            app.scene = BbSceneGame;
            app.now = 40000;
            app.run.phase = BbPhasePlayback;
            app.run.play_on = true;
            app.run.play_i = 0;
            uint16_t tone = bb_speed_tone[app.run.speed];

            app.run.phase_end = app.now + tone; /* the step has just begun */
            frame(&app);
            early[i] = fake.whites;
            uint32_t oob_early = fake.out_of_bounds;

            app.run.phase_end = app.now + tone - BB_FLASH_MS - 1; /* past the pop */
            frame(&app);
            late[i] = fake.whites;
            sprintf(msg, "%s", bb_assist_name[assists[i]]);
            check("the pop-in never draws off screen", oob_early == 0 && fake.out_of_bounds == 0, msg);
        }
        sprintf(msg, "shapes %lu then %lu, arrows %lu then %lu", (unsigned long)early[2],
                (unsigned long)late[2], (unsigned long)early[3], (unsigned long)late[3]);
        check("a shape cue starts inverted and stops", early[2] > late[2] && late[2] == 0, msg);
        check("and so does an arrow", early[3] > late[3] && late[3] == 0, msg);
        sprintf(msg, "ears %lu, led %lu", (unsigned long)early[0], (unsigned long)early[1]);
        check("with nothing to invert in EARS or LED", early[0] == 0 && early[1] == 0, msg);
    }

    /* the player's own presses do not pop, and neither does a reflex cue */
    {
        bb_app_init(&app);
        app.seed = 11;
        app.set.assist = BbAssistShapes;
        bb_run_start(&app, BbModeClassic);
        app.scene = BbSceneGame;
        app.now = 40000;
        app.run.phase = BbPhaseInput;
        app.run.flash_end = app.now + BB_PRESS_LED_MS;
        app.run.phase_end = app.now + 10;
        frame(&app);
        check("a press of your own is not inverted", fake.whites == 0, "");

        bb_app_init(&app);
        app.seed = 11;
        app.set.assist = BbAssistShapes;
        bb_run_start(&app, BbModeReflex);
        app.scene = BbSceneGame;
        app.now = 40000;
        app.run.phase = BbPhaseRxCue;
        app.run.phase_end = app.now + app.run.rx_win;
        app.run.win_ms = app.run.rx_win;
        app.run.win_end = app.now + app.run.rx_win;
        frame(&app);
        check("nor a reflex cue, which is not playback", fake.whites == 0, "");
    }

    /* ---- game over offers another go, and the daily does not ---- */
    {
        bb_app_init(&app);
        app.seed = 4;
        bb_run_start(&app, BbModeClassic);
        bb_go(&app, BbSceneOver);
        app.now += BB_OVER_LOCK;
        frame(&app);
        check("game over says OK plays again", fc_saw("OK: AGAIN"), "");

        bb_app_init(&app);
        app.seed = 4;
        bb_daily_refresh(&app, bb_today_seed());
        bb_run_start(&app, BbModeDaily);
        bb_run_end(&app, false);
        bb_go(&app, BbSceneOver);
        app.now += BB_OVER_LOCK;
        frame(&app);
        check("but a spent daily does not pretend to", !fc_saw("OK: AGAIN"), "");
        check("and offers the menu instead", fc_saw("BACK: MENU"), "");
    }

    /* ---- the intro, every tick of it ---- */
    {
        uint32_t oob = 0, wide = 0, blank = 0;
        uint32_t total = BB_SP_HOLD + BB_SP_FADE + BB_SP_GLIDE + BB_SP_FLASH + BB_SP_WIPE;
        /* did each of the five stages actually draw something? */
        uint32_t stage_ops[5] = {0};
        const uint32_t edge[5] = {
            BB_SP_HOLD,
            BB_SP_HOLD + BB_SP_FADE,
            BB_SP_HOLD + BB_SP_FADE + BB_SP_GLIDE,
            BB_SP_HOLD + BB_SP_FADE + BB_SP_GLIDE + BB_SP_FLASH,
            total};
        bb_app_init(&app);
        bb_go(&app, BbSceneSplash);
        for(uint32_t t2 = 0; t2 <= total + 200; t2 += BB_TICK_MS) {
            app.now = app.scene_at + t2;
            frame(&app);
            oob += fake.out_of_bounds;
            wide += fake.too_wide;
            /* the flash whites the screen out on purpose, so a blank
               frame is only a fault outside that stage */
            if(fake.ops <= 1 && t2 < total && !(t2 >= edge[2] && t2 < edge[3])) blank++;
            for(int st = 0; st < 5; st++)
                if(t2 < edge[st]) {
                    stage_ops[st] += fake.ops;
                    break;
                }
        }
        sprintf(msg, "%lu off screen, %lu wide", (unsigned long)oob, (unsigned long)wide);
        check("the intro never draws outside the screen", oob == 0 && wide == 0, msg);
        sprintf(msg, "%lu %lu %lu %lu %lu ops", (unsigned long)stage_ops[0],
                (unsigned long)stage_ops[1], (unsigned long)stage_ops[2],
                (unsigned long)stage_ops[3], (unsigned long)stage_ops[4]);
        check("and all five of its stages draw something",
              stage_ops[0] && stage_ops[1] && stage_ops[2] && stage_ops[3] && stage_ops[4], msg);
        sprintf(msg, "%lu frames", (unsigned long)blank);
        check("with no dead frame outside the flash", blank == 0, msg);

        /* The wheel is wider than the screen, so most of it is never
           seen. What matters is that cars keep riding through: each one
           is a filled disc, which the rim never draws. */
        uint32_t frames = 0, carrying = 0, most = 0;
        for(uint32_t t2 = edge[1]; t2 < edge[2]; t2 += BB_TICK_MS) {
            app.now = app.scene_at + t2;
            frame(&app);
            frames++;
            if(fake.discs) carrying++;
            if(fake.discs > most) most = fake.discs;
        }
        sprintf(msg, "%lu of %lu frames, up to %lu at once", (unsigned long)carrying,
                (unsigned long)frames, (unsigned long)most);
        check("the wheel carries cars through the frame", carrying * 2 > frames && most >= 2, msg);
    }

    /* ---- a long walk, drawing every frame ---- */
    {
        bb_app_init(&app);
        app.seed = 0xD1CEu;
        BbRng rng;
        bb_rng_seed(&rng, 31337u);
        uint32_t oob = 0, wide = 0;
        char worst[64] = "";
        int p = 0;
        msg[0] = 0;
        for(uint32_t i = 0; i < 120000; i++) {
            bb_tick(&app, BB_TICK_MS);
            bb_input(&app, keys[bb_rng_below(&rng, 6)]);
            if(!app.running) {
                app.running = true;
                bb_go(&app, BbSceneLauncher);
            }
            frame(&app);
            if(fake.out_of_bounds) {
                oob++;
                if(p < 150) p += sprintf(msg + p, "%s ", scene_name[app.scene]);
            }
            if(fake.too_wide) {
                wide++;
                strcpy(worst, fake.worst);
            }
        }
        sprintf(msg + p, "| %lu bad frames, %lu wide %s", (unsigned long)oob, (unsigned long)wide, worst);
        check("a hundred thousand frames of real play stay inside the screen", oob == 0, msg);
        check("and never overflow a line", wide == 0, msg);
    }

    printf(fails ? "\n%d FAILURES\n" : "\nall screen checks passed\n", fails);
    return fails ? 1 : 0;
}
