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
    check("the setup footer shows what the run is worth", fc_saw("score x1.55"), "");
    app.set.diff = 1;
    frame(&app);
    check("and follows the dials", fc_saw("score x1.00"), "");

    /* ---- what game over reports is what was banked ---- */
    bb_app_init(&app);
    app.seed = 3;
    bb_run_start(&app, BbModeClassic);
    app.run.score = 1234;
    app.run.mult = 155;
    app.run.record = true;
    bb_go(&app, BbSceneOver);
    app.now += BB_OVER_LOCK;
    frame(&app);
    check("game over prints the score", fc_saw("1234"), "");
    check("with the multiplier beside it", fc_saw("x1.55"), "");
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
