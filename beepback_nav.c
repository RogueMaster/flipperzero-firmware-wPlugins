/*
 * BEEPBACK - input, ported from the browser build's press().
 *
 * Same scenes in the same order, same clamps, same BACK table. Pause is
 * a flag over whatever scene is running rather than a scene of its own,
 * exactly as it is in the browser.
 */
#include "beepback.h"
#include <stdio.h>

static uint8_t clamp8(int16_t v, int16_t lo, int16_t hi) {
    if(v < lo) return (uint8_t)lo;
    if(v > hi) return (uint8_t)hi;
    return (uint8_t)v;
}

/* which of the five buttons this key is, or -1 for BACK */
static int8_t btn_of(InputKey key) {
    switch(key) {
    case InputKeyUp:
        return BbBtnUp;
    case InputKeyDown:
        return BbBtnDown;
    case InputKeyLeft:
        return BbBtnLeft;
    case InputKeyRight:
        return BbBtnRight;
    case InputKeyOk:
        return BbBtnOk;
    default:
        return -1;
    }
}

/* setupRows(): each mode names its own rows. The caller owns the two
   scratch buffers, because two of the values are built at draw time. */
uint8_t bb_setup_rows(
    const BeepbackApp* app,
    const char* label[4],
    const char* value[4],
    uint8_t kind[4],
    char* buf_a,
    char* buf_b,
    size_t bufn) {
    uint8_t n = 0;
    uint8_t diff = app->set.diff < BB_DIFF_COUNT ? app->set.diff : 1;
    uint8_t speed = app->set.speed < BB_SPEED_COUNT ? app->set.speed : 1;
    label[0] = label[1] = label[2] = label[3] = NULL;
    value[0] = value[1] = value[2] = value[3] = NULL;
    kind[0] = kind[1] = kind[2] = kind[3] = BbRowStart;

    if(app->mode == BbModeDaily) {
        bool played = (app->rec.daily_date == bb_daily_seed()) && app->rec.daily_done;
        snprintf(buf_a, bufn, "%s %uS", bb_diff_name[1], bb_time_ms[1] / 1000u);
        kind[n] = BbRowToday;
        label[n] = "TODAY";
        value[n++] = bb_rule_label[bb_daily_rule()];
        kind[n] = BbRowTime;
        label[n] = "TIME";
        value[n++] = buf_a;
        kind[n] = BbRowSpeed;
        label[n] = "SPEED";
        value[n++] = bb_speed_name[1];
        kind[n] = BbRowStart;
        label[n++] = played ? "PLAYED" : "START";
        return n;
    }
    if(app->mode == BbModeChallenge) {
        snprintf(buf_a, bufn, "%s %uS", bb_diff_name[diff], bb_time_ms[diff] / 1000u);
        kind[n] = BbRowTime;
        label[n] = "TIME";
        value[n++] = buf_a;
        kind[n] = BbRowSpeed;
        label[n] = "SPEED";
        value[n++] = bb_speed_name[speed];
        kind[n] = BbRowStart;
        label[n++] = "START";
        return n;
    }
    if(app->mode == BbModeReflex) {
        snprintf(buf_a, bufn, "%s %uMS", bb_diff_name[diff], bb_rx_shrink[diff]);
        snprintf(buf_b, bufn, "%s %uMS", bb_speed_name[speed], bb_rx_gap[speed]);
        kind[n] = BbRowRamp;
        label[n] = "RAMP";
        value[n++] = buf_a;
        kind[n] = BbRowSpeed;
        label[n] = "SPEED";
        value[n++] = buf_b;
        kind[n] = BbRowStart;
        label[n++] = "START";
        return n;
    }
    snprintf(buf_a, bufn, "%s %uS", bb_diff_name[diff], bb_time_ms[diff] / 1000u);
    label[n] = "TIME";
    value[n++] = buf_a;
    label[n] = "SPEED";
    value[n++] = bb_speed_name[speed];
    label[n++] = "START";
    return n;
}

static uint8_t setup_row_count(const BeepbackApp* app) {
    const char* l[4];
    const char* v[4];
    uint8_t k[4];
    char a[24], b[24];
    return bb_setup_rows(app, l, v, k, a, b, sizeof(a));
}

/* what the row the cursor is on is for, which is what LEFT and RIGHT act
   upon. A kind rather than a label, so nothing compares strings. */
static uint8_t setup_row_kind(const BeepbackApp* app) {
    const char* l[4];
    const char* v[4];
    uint8_t k[4];
    char a[24], b[24];
    uint8_t n = bb_setup_rows(app, l, v, k, a, b, sizeof(a));
    return app->setup_idx < n ? k[app->setup_idx] : BbRowStart;
}

void bb_press(BeepbackApp* app, InputKey key) {
    if(app->scene == BbSceneGameOver && app->now < app->lock_until) return;
    int8_t btn = btn_of(key);
    bool in_game = bb_in_game(app->scene);

    if(app->scene == BbSceneLauncher) {
        app->sp_start = 0;
        app->sp_phase = 0;
        app->sp_flash_idx = -1;
        bb_enter(app, BbSceneSplash);
        return;
    }
    if(app->scene == BbSceneSplash) {
        bb_splash_done(app); /* skippable */
        return;
    }

    if(app->paused) {
        if(key == InputKeyOk) {
            app->paused = false;
            bb_pause_shift(app, app->now - app->pause_at);
        }
        if(key == InputKeyBack) {
            app->paused = false;
            bb_enter(app, BbSceneMenu);
        }
        return;
    }

    if(key == InputKeyBack) {
        /* leaving the game drops you back in the apps list, which is where
           the firmware writes settings and scores to the SD card */
        switch(app->scene) {
        case BbSceneMenu:
            bb_enter(app, BbSceneLauncher);
            return;
        case BbSceneCredits:
        case BbSceneScores:
            bb_enter(app, BbSceneScorePick);
            return;
        case BbSceneSetup:
            bb_enter(app, app->mode == BbModeChallenge ? BbSceneChPick : BbSceneMode);
            return;
        case BbSceneChPick:
            bb_enter(app, BbSceneMode);
            return;
        case BbSceneReset:
        case BbSceneDetail:
            bb_enter(app, BbSceneSettings);
            return;
        case BbSceneSoundTest:
            bb_enter(app, app->test_from);
            return;
        case BbSceneTutorial:
            bb_enter(app, app->help_from);
            return;
        case BbSceneRulesGuide:
        case BbSceneReflexGuide:
            bb_enter(app, BbSceneHelp);
            return;
        case BbSceneRuleList:
            bb_enter(app, BbSceneRulesGuide);
            return;
        case BbSceneRuleInfo:
            bb_enter(app, BbSceneRuleList);
            return;
        default:
            break;
        }
        if(in_game) {
            /* No pausing a reaction test: freezing a live cue would let you
               take all the time you like and then answer. BACK ends the run
               instead, which is what a miss does, so the score still stands. */
            if(app->mode == BbModeReflex) {
                bb_reflex_miss(app);
                return;
            }
            app->paused = true;
            app->pause_at = app->now;
            return;
        }
        bb_enter(app, BbSceneMenu);
        return;
    }

    switch(app->scene) {
    case BbSceneMenu:
        if(key == InputKeyUp) app->menu_idx = clamp8((int16_t)app->menu_idx - 1, 0, 2);
        if(key == InputKeyDown) app->menu_idx = clamp8((int16_t)app->menu_idx + 1, 0, 2);
        if(key == InputKeyRight) bb_enter(app, BbSceneScorePick);
        if(key == InputKeyOk) {
            if(app->menu_idx == 0) {
                bb_enter(app, BbSceneMode);
            } else if(app->menu_idx == 1) {
                app->help_idx = 0;
                bb_enter(app, BbSceneHelp);
            } else {
                app->set_idx = 0;
                bb_enter(app, BbSceneSettings);
            }
        }
        break;

    case BbSceneMode: {
        /* three pages: classic and rules, reflex and challenge, then daily */
        uint8_t page = (uint8_t)(app->mode_idx >> 1);
        int16_t first = page * 2;
        int16_t last = first + 1 < BB_MODE_COUNT ? first + 1 : BB_MODE_COUNT - 1;
        if(key == InputKeyUp) app->mode_idx = clamp8((int16_t)app->mode_idx - 1, first, last);
        if(key == InputKeyDown) app->mode_idx = clamp8((int16_t)app->mode_idx + 1, first, last);
        if(key == InputKeyLeft && page > 0) app->mode_idx = (uint8_t)((page - 1) * 2);
        if(key == InputKeyRight && page < 2) app->mode_idx = (uint8_t)((page + 1) * 2);
        if(key == InputKeyOk) {
            app->mode = app->mode_idx;
            if(app->mode == BbModeChallenge) {
                bb_enter(app, BbSceneChPick); /* pick the rule on its own screen */
                break;
            }
            app->setup_idx = (uint8_t)(setup_row_count(app) - 1);
            bb_enter(app, BbSceneSetup);
        }
        break;
    }

    case BbSceneChPick: {
        const uint8_t n = BB_RULE_COUNT + 1, VIS = 4;
        if(key == InputKeyUp) app->ch_idx = clamp8((int16_t)app->ch_idx - 1, 0, n - 1);
        if(key == InputKeyDown) app->ch_idx = clamp8((int16_t)app->ch_idx + 1, 0, n - 1);
        if(app->ch_idx < app->ch_scroll) app->ch_scroll = app->ch_idx;
        if(app->ch_idx > app->ch_scroll + VIS - 1)
            app->ch_scroll = (uint8_t)(app->ch_idx - VIS + 1);
        if(key == InputKeyOk || key == InputKeyRight) {
            app->setup_idx = (uint8_t)(setup_row_count(app) - 1);
            bb_enter(app, BbSceneSetup);
        }
        break;
    }

    case BbSceneSetup: {
        uint8_t n = setup_row_count(app);
        uint8_t last = (uint8_t)(n - 1);
        if(app->mode == BbModeDaily)
            app->setup_idx = last; /* nothing else on the daily is yours to change */
        if(key == InputKeyUp) app->setup_idx = clamp8((int16_t)app->setup_idx - 1, 0, n - 1);
        if(key == InputKeyDown) app->setup_idx = clamp8((int16_t)app->setup_idx + 1, 0, n - 1);
        if(app->mode == BbModeDaily) app->setup_idx = last;
        int8_t d = key == InputKeyRight ? 1 : key == InputKeyLeft ? -1 : 0;
        uint8_t row = setup_row_kind(app);
        if(d) {
            if(row == BbRowTime || row == BbRowRamp)
                app->set.diff = clamp8((int16_t)app->set.diff + d, 0, BB_DIFF_COUNT - 1);
            if(row == BbRowSpeed)
                app->set.speed = clamp8((int16_t)app->set.speed + d, 0, BB_SPEED_COUNT - 1);
        }
        if(key == InputKeyOk && app->setup_idx == last) {
            if(app->mode == BbModeDaily && app->rec.daily_date == bb_daily_seed() &&
               app->rec.daily_done)
                break; /* one a day */
            bb_start_game(app);
        }
        break;
    }

    case BbSceneSettings: {
        if(key == InputKeyUp) app->set_idx = clamp8((int16_t)app->set_idx - 1, 0, 4);
        if(key == InputKeyDown) app->set_idx = clamp8((int16_t)app->set_idx + 1, 0, 4);
        int8_t step = key == InputKeyRight ? 1 : key == InputKeyLeft ? -1 : 0;
        if(app->set_idx == 0 && step) {
            app->set.volume = clamp8((int16_t)app->set.volume + step, 0, BB_VOL_COUNT - 1);
            if(app->set.volume > 0) bb_tone(app, bb_button_hz[BbBtnOk], 140);
        }
        if(app->set_idx == 1 && step)
            app->set.assist = clamp8((int16_t)app->set.assist + step, 0, BB_ASSIST_COUNT - 1);
        if(app->set_idx == 2 && (key == InputKeyOk || key == InputKeyRight)) {
            app->test_from = BbSceneSettings;
            bb_enter(app, BbSceneSoundTest);
        }
        if(app->set_idx == 3 && (key == InputKeyOk || key == InputKeyRight)) {
            app->det_row = 0;
            bb_enter(app, BbSceneDetail);
        }
        if(app->set_idx == 4 && (key == InputKeyOk || key == InputKeyRight)) {
            app->reset_idx = 0;
            bb_enter(app, BbSceneReset);
        }
        break;
    }

    case BbSceneDetail: {
        if(key == InputKeyUp) app->det_row = clamp8((int16_t)app->det_row - 1, 0, 2);
        if(key == InputKeyDown) app->det_row = clamp8((int16_t)app->det_row + 1, 0, 2);
        int8_t d = key == InputKeyRight ? 1 : key == InputKeyLeft ? -1 : 0;
        if(d) {
            if(app->det_row == 0) app->det_mode = clamp8((int16_t)app->det_mode + d, 0, 2);
            if(app->det_row == 1) app->det_time = clamp8((int16_t)app->det_time + d, 0, 3);
            if(app->det_row == 2) app->det_speed = clamp8((int16_t)app->det_speed + d, 0, 2);
        }
        break;
    }

    case BbSceneReset:
        if(key == InputKeyUp) app->reset_idx = clamp8((int16_t)app->reset_idx - 1, 0, 1);
        if(key == InputKeyDown) app->reset_idx = clamp8((int16_t)app->reset_idx + 1, 0, 1);
        if(key == InputKeyLeft) bb_enter(app, BbSceneSettings);
        if(key == InputKeyOk) {
            if(app->reset_idx == 0) {
                memset(&app->rec, 0, sizeof(app->rec));
                app->set_flash = app->now + 900;
            } else {
                /* the guide opens itself on the next launch; scores are
                   deliberately untouched, that is the other row */
                app->first_run = true;
                app->set.tutorial_done = false;
                bb_enter(app, BbSceneLauncher);
            }
        }
        break;

    case BbSceneHelp:
        if(key == InputKeyUp) app->help_idx = clamp8((int16_t)app->help_idx - 1, 0, 2);
        if(key == InputKeyDown) app->help_idx = clamp8((int16_t)app->help_idx + 1, 0, 2);
        if(key == InputKeyOk) {
            app->tut_page = 0;
            app->help_from = BbSceneHelp;
            bb_enter(app, app->help_idx == 0 ? BbSceneTutorial :
                          app->help_idx == 1 ? BbSceneRulesGuide :
                                               BbSceneReflexGuide);
        }
        break;

    case BbSceneReflexGuide:
        if(key == InputKeyRight || key == InputKeyOk) {
            if(app->tut_page + 1 < 2) {
                app->tut_page++;
            } else {
                app->test_from = BbSceneHelp;
                bb_enter(app, BbSceneSoundTest);
            }
        }
        if(key == InputKeyLeft && app->tut_page > 0) app->tut_page--;
        break;

    case BbSceneRulesGuide:
        if(key == InputKeyRight || key == InputKeyOk) {
            if(app->tut_page + 1 < 2) {
                app->tut_page++;
            } else {
                app->rule_sel = 0;
                app->rule_scroll = 0;
                bb_enter(app, BbSceneRuleList);
            }
        }
        if(key == InputKeyLeft && app->tut_page > 0) app->tut_page--;
        break;

    case BbSceneTutorial: {
        uint8_t total = (uint8_t)(4 + (bb_visual_on(app) ? 1 : 0));
        if(key == InputKeyRight || key == InputKeyOk) {
            if(app->tut_page + 1 < total) {
                app->tut_page++;
            } else {
                app->test_from = app->help_from;
                bb_enter(app, BbSceneSoundTest);
            }
        }
        if(key == InputKeyLeft && app->tut_page > 0) app->tut_page--;
        break;
    }

    case BbSceneRuleList: {
        const uint8_t VIS = 4;
        if(key == InputKeyUp) app->rule_sel = clamp8((int16_t)app->rule_sel - 1, 0, BB_RULE_COUNT - 1);
        if(key == InputKeyDown) app->rule_sel = clamp8((int16_t)app->rule_sel + 1, 0, BB_RULE_COUNT - 1);
        if(app->rule_sel < app->rule_scroll) app->rule_scroll = app->rule_sel;
        if(app->rule_sel > app->rule_scroll + VIS - 1)
            app->rule_scroll = (uint8_t)(app->rule_sel - VIS + 1);
        if(key == InputKeyOk || key == InputKeyRight) bb_enter(app, BbSceneRuleInfo);
        break;
    }

    case BbSceneRuleInfo:
        if(key == InputKeyLeft) bb_enter(app, BbSceneRuleList);
        if(key == InputKeyDown) app->rule_sel = clamp8((int16_t)app->rule_sel + 1, 0, BB_RULE_COUNT - 1);
        if(key == InputKeyUp) app->rule_sel = clamp8((int16_t)app->rule_sel - 1, 0, BB_RULE_COUNT - 1);
        break;

    case BbSceneSoundTest:
        if(btn >= 0) {
            app->test_btn = btn;
            bb_tone(app, bb_button_hz[btn], bb_tone_ms(app));
            bb_led_flash(app, bb_button_led[btn], bb_tone_ms(app));
        }
        break;

    case BbSceneScorePick:
        if(key == InputKeyUp) app->score_mode = clamp8((int16_t)app->score_mode - 1, 0, BB_MODE_COUNT - 1);
        if(key == InputKeyDown) app->score_mode = clamp8((int16_t)app->score_mode + 1, 0, BB_MODE_COUNT - 1);
        if(key == InputKeyOk) bb_enter(app, BbSceneScores);
        if(key == InputKeyLeft) bb_enter(app, BbSceneMenu);
        if(key == InputKeyRight) bb_enter(app, BbSceneCredits);
        break;

    case BbSceneScores:
    case BbSceneCredits:
        if(key == InputKeyLeft) bb_enter(app, BbSceneScorePick);
        break;

    case BbSceneGameOver:
        if(key == InputKeyOk) bb_start_game(app);
        if(key == InputKeyRight) app->go_page = 1;
        if(key == InputKeyLeft) app->go_page = 0;
        break;

    case BbSceneGo:
        /* jumping the gun is allowed: skip the banner and count the press */
        if(btn < 0) return;
        bb_enter(app, BbSceneInput);
        bb_take_press(app, (uint8_t)btn);
        break;

    case BbSceneInput:
        if(btn < 0) return;
        bb_take_press(app, (uint8_t)btn);
        break;

    case BbSceneReflexGap:
        if(btn < 0) return;
        bb_reflex_miss(app); /* pressing before the cue is a miss */
        break;

    case BbSceneReflexCue:
        if(btn < 0) return;
        if(btn == (int8_t)app->rx_cue) {
            bb_reflex_hit(app, (uint8_t)btn);
        } else {
            bb_reflex_miss(app);
        }
        break;

    default:
        break;
    }
}
