/*
 * BEEPBACK - scenes, cursors and input.
 *
 * The whole state machine lives here and touches no Flipper API: time
 * arrives through bb_tick() and hardware leaves through app->led and
 * app->tone_hz, which the app loop applies. That keeps every transition
 * reachable from a host test.
 *
 * Two rules run through all of it. Nothing wraps: every cursor and every
 * value clamps at its ends, and the screens draw an arrow only where
 * bb_list_move() would actually have moved. And BACK is defined in one
 * place, bb_back_target(), so the navigation map is a table rather than a
 * habit spread over twenty switch statements.
 */
#include "beepback.h"

/* ------------------------------------------------------------------ */
/* Presentation tables                                                 */
/*                                                                     */
/* Tone, colour and shape all climb together, so a player who learns    */
/* one ladder has learned the other two.                                */
/* ------------------------------------------------------------------ */

const uint16_t bb_button_hz[BbBtnCount] = {
    1397, /* UP    F6 */
    440, /* DOWN  A4 */
    587, /* LEFT  D5 */
    1047, /* RIGHT C6 */
    784, /* OK    G5 */
};

const uint8_t bb_button_shape[BbBtnCount] = {
    BbShapeStar, /* UP    */
    BbShapeCircle, /* DOWN  */
    BbShapeTriangle, /* LEFT  */
    BbShapePentagon, /* RIGHT */
    BbShapeSquare, /* OK    */
};

const char* const bb_button_name[BbBtnCount] = {"UP", "DOWN", "LEFT", "RIGHT", "OK"};

/* Moved here from beepback_led.c, which keeps the notification sequences:
   the mapping is presentation, and the host tests need to link it. */
const uint8_t bb_button_led[BbBtnCount] = {
    BbLedViolet, /* UP    - highest tone */
    BbLedRed, /* DOWN  - lowest tone  */
    BbLedYellow, /* LEFT  */
    BbLedBlue, /* RIGHT */
    BbLedGreen, /* OK    */
};

/* ------------------------------------------------------------------ */
/* Cursors                                                             */
/* ------------------------------------------------------------------ */

bool bb_list_move(uint8_t* cur, uint8_t count, int8_t delta) {
    if(count == 0) return false;
    int16_t next = (int16_t)*cur + delta;
    if(next < 0 || next >= (int16_t)count) return false; /* clamp, never wrap */
    *cur = (uint8_t)next;
    return true;
}

/* The scores detail screen shows a different set of dials per mode,
   because challenge is keyed by rule and the daily by nothing at all. */
static uint8_t bb_detail_dials(uint8_t mode) {
    if(mode == BbModeChallenge) return 2; /* MODE, RULE  */
    if(mode == BbModeDaily) return 1; /* MODE        */
    return 3; /* MODE, TIME, SPEED */
}

uint8_t bb_list_count(const BeepbackApp* app, BbScene scene) {
    switch(scene) {
    case BbSceneMenu:
        return 3;
    case BbSceneModeSelect:
        return app->mode_page == 2 ? 1 : 2;
    case BbSceneRulePick:
        return BB_RULE_COUNT + 1; /* the seven, then RANDOM */
    case BbSceneSetup:
        return 3; /* TIME, SPEED, START */
    case BbSceneSettings:
        return 5;
    case BbSceneSounds:
    case BbSceneSoundTest:
        return BbBtnCount;
    case BbSceneScores:
    case BbSceneBoardPick:
        return BB_MODE_COUNT;
    case BbSceneScoreDetail:
        return bb_detail_dials(app->det_mode);
    case BbSceneReset:
        return 2; /* SCORES, TUTORIAL */
    case BbSceneHowToPick:
        return 3; /* CLASSIC, RULES, REFLEX */
    default:
        return 0;
    }
}

/* Page counts for the screens that read left to right instead. */
uint8_t bb_howto_pages(uint8_t topic) {
    return topic == 2 ? 3 : 4; /* reflex has less to explain */
}

uint8_t bb_effective_assist(const BeepbackApp* app) {
    /* Sound off and ears only leaves nothing to go on. */
    if(app->set.volume == 0 && app->set.assist == BbAssistOff) return BbAssistShapes;
    return app->set.assist;
}

/* ------------------------------------------------------------------ */
/* The navigation map                                                  */
/* ------------------------------------------------------------------ */

BbScene bb_back_target(const BeepbackApp* app) {
    switch(app->scene) {
    case BbSceneLauncher:
        return BbSceneCount; /* leaves the app */
    case BbSceneSplash:
        return app->set.tutorial_done ? BbSceneMenu : BbSceneTutorial;
    case BbSceneTutorial:
        return BbSceneMenu;
    case BbSceneMenu:
        return BbSceneLauncher; /* the save is written on the way out */
    case BbSceneModeSelect:
        return BbSceneMenu;
    case BbSceneRulePick:
        return BbSceneModeSelect;
    case BbSceneSetup:
        /* challenge came through the rule picker, so that is where back goes */
        return app->run.mode == BbModeChallenge ? BbSceneRulePick : BbSceneModeSelect;
    case BbSceneGame:
        /* reflex cannot pause: freezing a live cue would be a cheat */
        return app->run.mode == BbModeReflex ? BbSceneOver : BbScenePause;
    case BbScenePause:
        return BbSceneOver; /* backing out of a pause quits the run */
    case BbSceneOver:
        return BbSceneMenu;
    case BbSceneHowToPick:
        return BbSceneMenu;
    case BbSceneHowTo:
    case BbSceneSoundTest:
        return BbSceneHowToPick;
    case BbSceneSettings:
        return BbSceneMenu;
    case BbSceneSounds:
    case BbSceneScores:
    case BbSceneReset:
        return BbSceneSettings;
    case BbSceneScoreDetail:
        return BbSceneScores;
    case BbSceneBoardPick:
        return BbSceneMenu;
    case BbSceneBoard:
    case BbSceneCredits:
        return BbSceneBoardPick;
    default:
        return BbSceneMenu;
    }
}

void bb_go(BeepbackApp* app, BbScene scene) {
    if(scene >= BbSceneCount) {
        app->running = false;
        return;
    }
    app->scene = scene;
    app->scene_at = app->now;
    app->led = BbLedOff;
    app->tone_hz = 0;

    switch(scene) {
    case BbSceneTutorial:
        app->tut_page = 0;
        break;
    case BbSceneHowTo:
        app->howto_page = 0;
        break;
    case BbSceneScoreDetail:
        app->det_cur = 0;
        break;
    case BbSceneOver:
        app->over_page = 0;
        break;
    case BbSceneSetup:
        /* the daily pins the cursor to START; nothing else on it moves */
        app->setup_cur = (app->run.mode == BbModeDaily) ? 2 : 0;
        /* and it belongs to a date, which may have turned over since the
           save was read - a session left open past midnight is a new day */
        if(app->run.mode == BbModeDaily) bb_daily_refresh(app, bb_today_seed());
        break;
    default:
        break;
    }
}

void bb_app_init(BeepbackApp* app) {
    memset(app, 0, sizeof(*app));
    app->running = true;
    app->set.volume = 3;
    app->set.assist = BbAssistShapes;
    app->set.diff = 1; /* NORMAL */
    app->set.speed = 1; /* NORMAL */
    app->rule_cur = BB_RULE_COUNT; /* RANDOM, the friendlier default */
    app->scene = BbSceneLauncher;
}

/* ------------------------------------------------------------------ */
/* Input                                                               */
/* ------------------------------------------------------------------ */

/* Which value a left or right press on this screen edits, and how many
   steps it has. One place decides it, so the screens can ask the same
   question the input handler answers and no arrow is ever offered where
   there is nothing to reach. */
static uint8_t* bb_adjust_target(BeepbackApp* app, uint8_t* count) {
    switch(app->scene) {
    case BbSceneSetup:
        if(app->run.mode == BbModeDaily) return NULL; /* the day picks these */
        if(app->setup_cur == 0) {
            *count = BB_DIFF_COUNT;
            return &app->set.diff;
        }
        if(app->setup_cur == 1) {
            *count = BB_SPEED_COUNT;
            return &app->set.speed;
        }
        return NULL;
    case BbSceneSettings:
        if(app->settings_cur == 0) {
            *count = BB_VOL_COUNT;
            return &app->set.volume;
        }
        if(app->settings_cur == 1) {
            *count = BB_ASSIST_COUNT;
            return &app->set.assist;
        }
        return NULL;
    case BbSceneScoreDetail:
        if(app->det_cur == 0) {
            *count = BB_MODE_COUNT;
            return &app->det_mode;
        }
        if(app->det_mode == BbModeChallenge) {
            *count = BB_RULE_COUNT;
            return &app->rule_cur;
        }
        if(app->det_mode == BbModeDaily) return NULL; /* the daily has no dials */
        if(app->det_cur == 1) {
            *count = BB_DIFF_COUNT;
            return &app->det_diff;
        }
        *count = BB_SPEED_COUNT;
        return &app->det_speed;
    default:
        return NULL;
    }
}

static bool bb_adjust(BeepbackApp* app, int8_t d) {
    uint8_t count = 0;
    uint8_t* value = bb_adjust_target(app, &count);
    if(!value || !bb_list_move(value, count, d)) return false;
    /* changing the mode on the detail screen changes which dials exist */
    if(app->scene == BbSceneScoreDetail && app->det_cur == 0) {
        uint8_t dials = bb_detail_dials(app->det_mode);
        if(app->det_cur >= dials) app->det_cur = (uint8_t)(dials - 1);
    }
    return true;
}

bool bb_can_adjust(const BeepbackApp* app, int8_t d) {
    uint8_t count = 0;
    /* read-only: bb_adjust_target hands back a pointer into the app, and
       nothing here writes through it */
    uint8_t* value = bb_adjust_target((BeepbackApp*)(uintptr_t)app, &count);
    if(!value) return false;
    int16_t next = (int16_t)*value + d;
    return next >= 0 && next < (int16_t)count;
}

static void bb_confirm(BeepbackApp* app) {
    app->flash_at = app->now;
}

static void bb_menu_ok(BeepbackApp* app) {
    switch(app->menu_cur) {
    case 0:
        bb_go(app, BbSceneModeSelect);
        break;
    case 1:
        bb_go(app, BbSceneHowToPick);
        break;
    default:
        bb_go(app, BbSceneSettings);
        break;
    }
}

static void bb_mode_ok(BeepbackApp* app) {
    BbMode mode = (BbMode)(app->mode_page * 2 + app->mode_row);
    app->run.mode = mode; /* setup and back both need to know it */
    if(mode == BbModeChallenge) {
        bb_go(app, BbSceneRulePick);
    } else {
        bb_go(app, BbSceneSetup);
    }
}

bool bb_can_start(const BeepbackApp* app, BbMode mode) {
    if(mode != BbModeDaily) return true;
    return !(app->rec.daily_done && app->rec.daily_date == bb_today_seed());
}

/* The one way into a run. bb_run_start() clears the run, so the mode has
   to arrive as an argument rather than be read back out of it. */
static void bb_begin_run(BeepbackApp* app, BbMode mode) {
    if(!bb_can_start(app, mode)) return;
    bb_run_start(app, mode);
    bb_go(app, BbSceneGame);
}

static void bb_setup_ok(BeepbackApp* app) {
    if(app->setup_cur != 2) return; /* only START starts */
    bb_begin_run(app, app->run.mode);
}

static void bb_settings_ok(BeepbackApp* app) {
    switch(app->settings_cur) {
    case 2:
        bb_go(app, BbSceneSounds);
        break;
    case 3:
        bb_go(app, BbSceneScores);
        break;
    case 4:
        bb_go(app, BbSceneReset);
        break;
    default:
        break; /* volume and assist are edited in place */
    }
}

static void bb_reset_ok(BeepbackApp* app) {
    if(app->reset_cur == 0) {
        memset(&app->rec, 0, sizeof(app->rec));
    } else {
        app->set.tutorial_done = false;
    }
    bb_confirm(app);
}

void bb_input(BeepbackApp* app, InputKey key) {
    if(key == InputKeyBack) {
        switch(app->scene) {
        case BbSceneGame:
            /* reflex ends instead of pausing; anything else freezes */
            if(app->run.mode == BbModeReflex) {
                bb_run_end(app, true);
                bb_go(app, BbSceneOver);
            } else {
                app->pause_at = app->now;
                bb_go(app, BbScenePause);
            }
            return;
        case BbScenePause:
            bb_run_end(app, true);
            bb_go(app, BbSceneOver);
            return;
        case BbSceneOver:
            if(app->now - app->scene_at < BB_OVER_LOCK) return; /* the wipe owns the screen */
            break;
        case BbSceneTutorial:
            app->set.tutorial_done = true;
            break;
        default:
            break;
        }
        bb_go(app, bb_back_target(app));
        return;
    }

    switch(app->scene) {
    case BbSceneLauncher:
        bb_go(app, BbSceneSplash);
        return;

    case BbSceneSplash:
        /* skippable, and every key skips it */
        bb_go(app, app->set.tutorial_done ? BbSceneMenu : BbSceneTutorial);
        return;

    case BbSceneTutorial:
        if(key == InputKeyLeft) bb_list_move(&app->tut_page, BB_TUT_PAGES, -1);
        if(key == InputKeyRight) bb_list_move(&app->tut_page, BB_TUT_PAGES, 1);
        if(key == InputKeyOk) {
            if(!bb_list_move(&app->tut_page, BB_TUT_PAGES, 1)) {
                app->set.tutorial_done = true;
                bb_go(app, BbSceneMenu);
            }
        }
        return;

    case BbSceneMenu:
        if(key == InputKeyUp) bb_list_move(&app->menu_cur, 3, -1);
        if(key == InputKeyDown) bb_list_move(&app->menu_cur, 3, 1);
        if(key == InputKeyRight) bb_go(app, BbSceneBoardPick);
        if(key == InputKeyOk) bb_menu_ok(app);
        return;

    case BbSceneModeSelect:
        if(key == InputKeyUp) bb_list_move(&app->mode_row, bb_list_count(app, app->scene), -1);
        if(key == InputKeyDown) bb_list_move(&app->mode_row, bb_list_count(app, app->scene), 1);
        /* a page change lands on that page's first mode, so the cursor is
           never left pointing at a row the new page does not have */
        if(key == InputKeyLeft && bb_list_move(&app->mode_page, BB_MODE_PAGES, -1))
            app->mode_row = 0;
        if(key == InputKeyRight && bb_list_move(&app->mode_page, BB_MODE_PAGES, 1))
            app->mode_row = 0;
        /* the last page holds one mode, so a row from a full page may be gone */
        if(app->mode_row >= bb_list_count(app, app->scene))
            app->mode_row = bb_list_count(app, app->scene) - 1;
        if(key == InputKeyOk) bb_mode_ok(app);
        return;

    case BbSceneRulePick:
        if(key == InputKeyUp) bb_list_move(&app->rule_cur, BB_RULE_COUNT + 1, -1);
        if(key == InputKeyDown) bb_list_move(&app->rule_cur, BB_RULE_COUNT + 1, 1);
        if(key == InputKeyOk) bb_go(app, BbSceneSetup);
        return;

    case BbSceneSetup:
        if(app->run.mode != BbModeDaily) {
            if(key == InputKeyUp) bb_list_move(&app->setup_cur, 3, -1);
            if(key == InputKeyDown) bb_list_move(&app->setup_cur, 3, 1);
        }
        if(key == InputKeyLeft) bb_adjust(app, -1);
        if(key == InputKeyRight) bb_adjust(app, 1);
        if(key == InputKeyOk) bb_setup_ok(app);
        return;

    case BbSceneGame:
        /* pressing during GO! is not a mistake, it is being ready: skip the
           banner and count the press */
        if(app->run.phase == BbPhaseGo && key != InputKeyBack) {
            app->run.phase_end = app->now;
            bb_run_tick(app);
        }
        switch(key) {
        case InputKeyUp:
            bb_run_press(app, BbBtnUp);
            break;
        case InputKeyDown:
            bb_run_press(app, BbBtnDown);
            break;
        case InputKeyLeft:
            bb_run_press(app, BbBtnLeft);
            break;
        case InputKeyRight:
            bb_run_press(app, BbBtnRight);
            break;
        default:
            bb_run_press(app, BbBtnOk);
            break;
        }
        if(app->run.phase == BbPhaseOver) bb_go(app, BbSceneOver);
        return;

    case BbScenePause:
        if(key == InputKeyOk) {
            /* every deadline moves on by however long the pause lasted, so
               the run picks up exactly where it stopped */
            uint32_t held = app->now > app->pause_at ? app->now - app->pause_at : 0;
            bb_go(app, BbSceneGame);
            bb_run_shift(app, held);
        }
        return;

    case BbSceneOver:
        if(app->now - app->scene_at < BB_OVER_LOCK) return;
        /* a second page holds the settings the run was played on */
        if(key == InputKeyRight) app->over_page = 1;
        if(key == InputKeyLeft) app->over_page = 0;
        /* same mode, same settings, straight into another run - not back
           out to the setup screen to press START again */
        if(key == InputKeyOk) bb_begin_run(app, app->run.mode);
        return;

    case BbSceneHowToPick:
        if(key == InputKeyUp) bb_list_move(&app->howto_cur, 3, -1);
        if(key == InputKeyDown) bb_list_move(&app->howto_cur, 3, 1);
        if(key == InputKeyOk) bb_go(app, BbSceneHowTo);
        return;

    case BbSceneHowTo: {
        uint8_t pages = bb_howto_pages(app->howto_cur);
        if(key == InputKeyLeft) bb_list_move(&app->howto_page, pages, -1);
        if(key == InputKeyRight) bb_list_move(&app->howto_page, pages, 1);
        if(key == InputKeyOk) {
            /* the last page hands you to the sound test */
            if(!bb_list_move(&app->howto_page, pages, 1)) bb_go(app, BbSceneSoundTest);
        }
        return;
    }

    case BbSceneSoundTest:
    case BbSceneSounds: {
        uint8_t* cur = (app->scene == BbSceneSounds) ? &app->sounds_cur : &app->sound_btn;
        if(key == InputKeyUp) bb_list_move(cur, BbBtnCount, -1);
        if(key == InputKeyDown) bb_list_move(cur, BbBtnCount, 1);
        if(key == InputKeyOk) {
            app->tone_hz = bb_button_hz[*cur];
            app->led = bb_button_led[*cur];
            app->flash_at = app->now;
        }
        return;
    }

    case BbSceneSettings:
        if(key == InputKeyUp) bb_list_move(&app->settings_cur, 5, -1);
        if(key == InputKeyDown) bb_list_move(&app->settings_cur, 5, 1);
        if(key == InputKeyLeft) bb_adjust(app, -1);
        if(key == InputKeyRight) bb_adjust(app, 1);
        if(key == InputKeyOk) bb_settings_ok(app);
        return;

    case BbSceneScores:
        if(key == InputKeyUp) bb_list_move(&app->scores_cur, BB_MODE_COUNT, -1);
        if(key == InputKeyDown) bb_list_move(&app->scores_cur, BB_MODE_COUNT, 1);
        if(key == InputKeyOk) {
            app->det_mode = app->scores_cur;
            bb_go(app, BbSceneScoreDetail);
        }
        return;

    case BbSceneScoreDetail: {
        uint8_t dials = bb_detail_dials(app->det_mode);
        if(key == InputKeyUp) bb_list_move(&app->det_cur, dials, -1);
        if(key == InputKeyDown) bb_list_move(&app->det_cur, dials, 1);
        if(key == InputKeyLeft) bb_adjust(app, -1);
        if(key == InputKeyRight) bb_adjust(app, 1);
        return;
    }

    case BbSceneReset:
        if(key == InputKeyUp) bb_list_move(&app->reset_cur, 2, -1);
        if(key == InputKeyDown) bb_list_move(&app->reset_cur, 2, 1);
        if(key == InputKeyOk) bb_reset_ok(app);
        return;

    case BbSceneBoardPick:
        if(key == InputKeyUp) bb_list_move(&app->board_cur, BB_MODE_COUNT, -1);
        if(key == InputKeyDown) bb_list_move(&app->board_cur, BB_MODE_COUNT, 1);
        if(key == InputKeyLeft) bb_go(app, BbSceneMenu);
        if(key == InputKeyRight) bb_go(app, BbSceneCredits);
        if(key == InputKeyOk) bb_go(app, BbSceneBoard);
        return;

    case BbSceneBoard:
    case BbSceneCredits:
        if(key == InputKeyLeft) bb_go(app, BbSceneBoardPick);
        return;

    default:
        return;
    }
}

/* ------------------------------------------------------------------ */
/* Time                                                                */
/* ------------------------------------------------------------------ */

#define BB_SPLASH_MS (BB_SP_HOLD + BB_SP_FADE + BB_SP_GLIDE + BB_SP_FLASH + BB_SP_WIPE)

void bb_tick(BeepbackApp* app, uint32_t dt_ms) {
    app->now += dt_ms;

    switch(app->scene) {
    case BbSceneSplash:
        if(app->now - app->scene_at >= BB_SPLASH_MS)
            bb_go(app, app->set.tutorial_done ? BbSceneMenu : BbSceneTutorial);
        break;
    case BbSceneGame:
        bb_run_tick(app);
        if(app->run.phase == BbPhaseOver) bb_go(app, BbSceneOver);
        break;
    default:
        break;
    }

    /* a flash of confirmation, and any tone the menus started, are brief */
    if(app->flash_at && app->now - app->flash_at >= BB_TONE_MS) {
        app->flash_at = 0;
        if(app->scene != BbSceneGame) {
            app->tone_hz = 0;
            app->led = BbLedOff;
        }
    }
}
