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
        snprintf(buf_a, bufn, "%s %uMS", bb_diff_short[diff], bb_rx_shrink[diff]);
        snprintf(buf_b, bufn, "%s %uMS", bb_speed_short[speed], bb_rx_gap[speed]);
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

/* Where this screen goes when you leave it, or BbSceneCount for a screen
   that has nowhere to go (the menu, and anything mid-run). BACK and LEFT
   both read this, and so does the arrow that says LEFT will work, so the
   three cannot drift apart. */
BbScene bb_back_target(const BeepbackApp* app) {
    switch(app->scene) {
    case BbSceneMenu:
        return BbSceneCount; /* the root: BACK leaves the app entirely */
    case BbSceneScorePick:
    case BbSceneCredits:
        return BbSceneStats; /* the stats screen is the front of this section */
    case BbSceneTable:
        return BbSceneScorePick;
    case BbSceneSetup:
        return app->mode == BbModeChallenge ? BbSceneChPick : BbSceneMode;
    case BbSceneChPick:
        return BbSceneMode;
    case BbSceneReset:
    case BbSceneNoCue: /* BACK is "let me fix it", so back to the rows */
        return BbSceneSettings;
    case BbSceneSoundTest:
        return app->test_from;
    case BbSceneTutorial:
        return app->help_from;
    case BbSceneRulesGuide:
    case BbSceneReflexGuide:
        return BbSceneHelp;
    case BbSceneRuleList:
        return BbSceneRulesGuide;
    case BbSceneRuleInfo:
        return BbSceneRuleList;
    default:
        return bb_in_game(app->scene) ? BbSceneCount : BbSceneMenu;
    }
}

void bb_press(BeepbackApp* app, InputKey key) {
    if(app->scene == BbSceneGameOver && app->now < app->lock_until) return;
    int8_t btn = btn_of(key);
    bool in_game = bb_in_game(app->scene);

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

    /* BACK is the only way back. LEFT means whatever the screen says it
       means and nothing where a screen says nothing, because an exit
       nothing points at is one you find by accident. */
    if(key == InputKeyBack) {
        /* You may play deaf and blind if you mean to, but not by accident:
           leaving SETTINGS that way says so once and asks. */
        if(app->scene == BbSceneSettings && bb_no_cue(app)) {
            bb_enter(app, BbSceneNoCue);
            return;
        }
        BbScene to = bb_back_target(app);
        if(to != BbSceneCount) {
            bb_enter(app, to);
            return;
        }
        if(app->scene == BbSceneMenu) {
            /* the way out of the app, and where the save is written */
            app->running = false;
            return;
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
            bb_hush(app); /* nothing should still be sounding behind it */
            return;
        }
        bb_enter(app, BbSceneMenu);
        return;
    }

    switch(app->scene) {
    case BbSceneMenu:
        if(key == InputKeyUp) app->menu_idx = clamp8((int16_t)app->menu_idx - 1, 0, 2);
        if(key == InputKeyDown) app->menu_idx = clamp8((int16_t)app->menu_idx + 1, 0, 2);
        if(key == InputKeyRight) bb_enter(app, BbSceneStats);
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
        if(app->set_idx == 2 && step) {
            app->set.haptic = step > 0;
            if(app->set.haptic) bb_buzz(app); /* feel it as you turn it on */
        }
        if(app->set_idx == 3 && (key == InputKeyOk || key == InputKeyRight)) {
            app->test_from = BbSceneSettings;
            bb_enter(app, BbSceneSoundTest);
        }
        if(app->set_idx == 4 && (key == InputKeyOk || key == InputKeyRight)) {
            app->confirm_until = 0;
            app->reset_idx = 0;
            bb_enter(app, BbSceneReset);
        }
        break;
    }

    case BbSceneReset: {
        int8_t move = key == InputKeyUp ? -1 : key == InputKeyDown ? 1 : 0;
        if(move) {
            app->reset_idx =
                clamp8((int16_t)app->reset_idx + move, 0, BB_RESET_ROWS - 1);
            app->confirm_until = 0; /* moving off a row disarms it */
        }
        if(key != InputKeyOk) break;

        /* Ask once, then listen for a second. A single press is never a
           reset, and an armed row cannot be left armed. */
        bool armed = app->confirm_until && app->now < app->confirm_until &&
                     app->confirm_row == app->reset_idx;
        if(!armed) {
            app->confirm_row = app->reset_idx;
            app->confirm_until = app->now + BB_CONFIRM_MS;
            break;
        }
        app->confirm_until = 0;
        switch(app->reset_idx) {
        case BbResetRecords:
            memset(&app->rec, 0, sizeof(app->rec));
            break;
        case BbResetStats:
            memset(&app->stats, 0, sizeof(app->stats));
            break;
        case BbResetTutorial:
            app->first_run = true;
            app->set.tutorial_done = false;
            break;
        default:
            /* everything, and out: the save is written on the way past the
               menu, so a fresh file is what the next launch reads */
            memset(&app->rec, 0, sizeof(app->rec));
            memset(&app->stats, 0, sizeof(app->stats));
            app->set.volume = 2;
            app->set.assist = 2;
            app->set.speed = 1;
            app->set.diff = 1;
            app->first_run = true;
            app->set.tutorial_done = false;
            app->running = false;
            return;
        }
        app->set_flash = app->now + 900; /* the row says DONE for a moment */
        break;
    }

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

    case BbSceneNoCue:
        /* OK keeps them: the warning is a warning, not a veto */
        if(key == InputKeyOk) bb_enter(app, BbSceneMenu);
        break;

    case BbSceneStats: {
        const uint8_t VIS = 4;
        if(key == InputKeyDown && app->stat_scroll + VIS < BB_STAT_ROWS) app->stat_scroll++;
        if(key == InputKeyUp && app->stat_scroll > 0) app->stat_scroll--;
        if(key == InputKeyOk) {
            app->score_mode = 0;
            bb_enter(app, BbSceneScorePick);
        }
        if(key == InputKeyRight) bb_enter(app, BbSceneCredits);
        break;
    }

    case BbSceneScorePick:
        if(key == InputKeyUp) app->score_mode = clamp8((int16_t)app->score_mode - 1, 0, BB_MODE_COUNT - 1);
        if(key == InputKeyDown) app->score_mode = clamp8((int16_t)app->score_mode + 1, 0, BB_MODE_COUNT - 1);
        if(key == InputKeyOk) {
            app->tbl_assist = 0;
            app->tbl_scroll = 0;
            bb_enter(app, BbSceneTable);
        }
        break;

    case BbSceneTable: {
        /* OK turns the table to the next assist, whatever shape it is. UP
           and DOWN are free on the grids and scroll the rules on
           challenge. */
        if(key == InputKeyOk)
            app->tbl_assist = (uint8_t)((app->tbl_assist + 1) % BB_ASSIST_COUNT);
        if(app->score_mode == BbModeChallenge) {
            const uint8_t VIS = 5;
            if(key == InputKeyDown && app->tbl_scroll + VIS < BB_RULE_COUNT) app->tbl_scroll++;
            if(key == InputKeyUp && app->tbl_scroll > 0) app->tbl_scroll--;
        }
        break;
    }

    case BbSceneCredits:
        break;

    case BbSceneGameOver:
        /* One attempt a day, from here as much as from the setup screen.
           The browser guards this on setup only, so its game over hands
           the daily back for as many goes as you like. */
        if(key == InputKeyOk) {
            if(app->mode == BbModeDaily && app->rec.daily_date == bb_daily_seed() &&
               app->rec.daily_done)
                break;
            bb_start_game(app);
        }
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

/* ------------------------------------------------------------------ */
/* One press, one action                                               */
/*                                                                     */
/* The device reports a press twice: InputTypePress when it goes down,
 * then InputTypeShort when it comes back up. In the game the first is
 * what counts, because waiting for the release would cost the player a
 * slice of a window they are being judged on. Everywhere else the
 * release is what counts, so holding a key does not fire it.
 *
 * The two rules meet badly when a press changes which of them applies.
 * BACK in a running game is exactly that: the press pauses, and then the
 * release arrives at a screen where BACK means quit, so one press paused
 * and left. The latch below is what stops a press being read twice.
 * ------------------------------------------------------------------ */
void bb_input_event(BeepbackApp* app, InputKey key, InputType type) {
    if(key >= InputKeyMAX) return;
    uint8_t bit = (uint8_t)(1u << key);

    switch(type) {
    case InputTypePress:
        if(bb_in_game(app->scene) && !app->paused) {
            app->press_latch |= bit;
            bb_press(app, key);
        }
        return;

    case InputTypeShort:
        /* the second half of a press already acted on */
        if(app->press_latch & bit) {
            app->press_latch &= (uint8_t)~bit;
            return;
        }
        bb_press(app, key);
        return;

    case InputTypeRelease:
        /* a long hold never sends Short, so the latch clears here too */
        app->press_latch &= (uint8_t)~bit;
        return;

    case InputTypeRepeat:
        /* holding a direction walks a list, but never repeats an action */
        if(key != InputKeyOk && key != InputKeyBack && !bb_in_game(app->scene))
            bb_press(app, key);
        return;

    default:
        return;
    }
}
