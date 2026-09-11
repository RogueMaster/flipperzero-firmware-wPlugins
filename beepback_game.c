/*
 * BEEPBACK - the run, ported from the browser build's enter() and
 * update(). The scene names, the order of the checks and the numbers are
 * all the browser's; what is different is that time arrives through
 * bb_tick() instead of requestAnimationFrame, and sound and light leave
 * through app->tune and app->led instead of WebAudio and a div.
 */
#include "beepback.h"

/* ------------------------------------------------------------------ */
/* The browser's little derived getters                                */
/* ------------------------------------------------------------------ */

bool bb_is_challenge_mode(uint8_t mode) {
    return mode >= BbModeChallenge; /* challenge and daily share the engine */
}
bool bb_audio_on(const BeepbackApp* app) {
    return app->set.volume > 0;
}
/* volume off and assist off leaves nothing to play by, so force shapes */
uint8_t bb_assist(const BeepbackApp* app) {
    return (app->set.volume == 0 && app->set.assist == 0) ? 2 : app->set.assist;
}
bool bb_led_mode(const BeepbackApp* app) {
    return bb_assist(app) == 1;
}
bool bb_shapes_on(const BeepbackApp* app) {
    return bb_assist(app) == 2;
}
bool bb_arrows_on(const BeepbackApp* app) {
    return bb_assist(app) == 3;
}
bool bb_cue_on(const BeepbackApp* app) {
    return bb_assist(app) >= 2; /* something is drawn on screen */
}
bool bb_visual_on(const BeepbackApp* app) {
    return bb_assist(app) > 0; /* any assist at all */
}
uint16_t bb_tone_ms(const BeepbackApp* app) {
    return bb_speed_tone[app->set.speed < BB_SPEED_COUNT ? app->set.speed : 1];
}
uint16_t bb_gap_ms(const BeepbackApp* app) {
    uint16_t g = bb_speed_gap[app->set.speed < BB_SPEED_COUNT ? app->set.speed : 1];
    return (uint16_t)(g + (bb_visual_on(app) ? BB_ASSIST_GAP : 0));
}
uint32_t bb_multiplier_for(const BeepbackApp* app) {
    return bb_multiplier((BbMode)app->mode, app->set.diff, app->set.speed);
}

bool bb_in_game(BbScene s) {
    return s == BbSceneReflexGap || s == BbSceneReflexCue || s == BbSceneRuleCard ||
           s == BbSceneListen || s == BbScenePlayback || s == BbSceneGo || s == BbSceneInput ||
           s == BbSceneHold || s == BbSceneSuccess || s == BbSceneRoundClear ||
           s == BbSceneWrong || s == BbSceneRetry;
}

/* the headline board is your best in that mode and assist, whatever you set */
uint32_t bb_best_for(const BeepbackApp* app, uint8_t mode, uint8_t assist) {
    uint32_t m = 0;
    if(assist >= BB_ASSIST_COUNT) assist = 0;
    if(mode == BbModeDaily) return app->rec.daily_best[assist];
    if(mode == BbModeChallenge) {
        for(uint8_t r = 0; r < BB_RULE_COUNT; r++)
            if(app->rec.ch_best[r][assist] > m) m = app->rec.ch_best[r][assist];
        return m;
    }
    if(mode >= BB_LADDER_MODES) return 0;
    for(uint8_t t = 0; t < BB_DIFF_COUNT; t++)
        for(uint8_t sp = 0; sp < BB_SPEED_COUNT; sp++)
            if(app->rec.best[mode][t][sp][assist] > m) m = app->rec.best[mode][t][sp][assist];
    return m;
}

void bb_daily_refresh(BeepbackApp* app, uint32_t today) {
    if(app->rec.daily_date == today) return;
    memset(app->rec.daily_best, 0, sizeof(app->rec.daily_best));
    app->rec.daily_date = today;
    app->rec.daily_done = false;
}

/* ------------------------------------------------------------------ */
/* Sound and light, requested rather than performed                    */
/* ------------------------------------------------------------------ */

void bb_play(BeepbackApp* app, const BbNote* notes, uint8_t n) {
    if(!bb_audio_on(app) || !n) {
        app->tune = NULL;
        app->tone_hz = 0;
        return;
    }
    app->tune = notes;
    app->tune_n = n;
    app->tune_i = 0;
    app->tone_hz = notes[0].f;
    app->tune_next = app->now + notes[0].ms;
}

void bb_tone(BeepbackApp* app, uint16_t hz, uint16_t ms) {
    static BbNote one; /* a single tone is a tune of length one */
    one.f = hz;
    one.ms = ms;
    bb_play(app, &one, 1);
}

void bb_led_flash(BeepbackApp* app, uint8_t color, uint32_t ms) {
    app->led = color;
    app->led_until = app->now + ms;
}

static void bb_tune_tick(BeepbackApp* app) {
    if(!app->tune) return;
    while(app->tune && app->now >= app->tune_next) {
        app->tune_i++;
        if(app->tune_i >= app->tune_n) {
            app->tune = NULL;
            app->tone_hz = 0;
            return;
        }
        app->tone_hz = app->tune[app->tune_i].f;
        app->tune_next += app->tune[app->tune_i].ms;
    }
}

/* ------------------------------------------------------------------ */
/* Sequences and rules                                                 */
/* ------------------------------------------------------------------ */

uint8_t bb_rand_button(BeepbackApp* app) {
    return bb_rng_below(&app->rng, BbBtnCount);
}

void bb_new_base(BeepbackApp* app, uint8_t len) {
    if(len > BB_MAX_SEQ) len = BB_MAX_SEQ;
    app->base.len = 0;
    for(uint8_t i = 0; i < len; i++) {
        uint8_t b = bb_rand_button(app), n = app->base.len;
        /* three of the same running is a staring contest, not a memory test */
        if(n >= 2 && app->base.step[n - 1] == b && app->base.step[n - 2] == b)
            b = (uint8_t)((b + 1 + bb_rng_below(&app->rng, BbBtnCount - 1)) % BbBtnCount);
        app->base.step[app->base.len++] = b;
    }
}

void bb_apply_run_rule(const BeepbackApp* app, const BbSeq* seq, BbPresses* out) {
    if((app->mode != BbModeRules && !bb_is_challenge_mode(app->mode)) || app->rule_idx < 0) {
        out->len = seq->len;
        memcpy(out->press, seq->step, seq->len);
        return;
    }
    bb_apply_rule(seq, (BbRule)app->rule_idx, app->rule_a, app->rule_b, out);
}

void bb_set_stage(BeepbackApp* app, uint8_t n) {
    if(n > app->base.len) n = app->base.len;
    app->stage = n;
    BbSeq shown;
    shown.len = n;
    memcpy(shown.step, app->base.step, n);
    bb_apply_run_rule(app, &shown, &app->expected);
}

/* A rule must leave at least one press at every stage of the ladder, and
   it has to actually bite at full length or the round is a freebie. */
bool bb_rule_fits_run(const BeepbackApp* app) {
    BbSeq p;
    BbPresses out;
    for(uint8_t n = 1; n <= app->target && n <= app->base.len; n++) {
        p.len = n;
        memcpy(p.step, app->base.step, n);
        bb_apply_run_rule(app, &p, &out);
        if(out.len == 0) return false;
    }
    bb_apply_run_rule(app, &app->base, &out);
    /* a press list of one repeated button can be mashed, so it is not a round */
    if(app->target >= 3) {
        bool two = false;
        for(uint8_t i = 1; i < out.len; i++)
            if(out.press[i] != out.press[0]) {
                two = true;
                break;
            }
        if(!two) return false;
    }
    if(out.len != app->base.len) return true;
    for(uint8_t i = 0; i < out.len; i++)
        if(out.press[i] != app->base.step[i]) return true;
    return false;
}

static void bb_rule_segs(BeepbackApp* app) {
    uint8_t r = (uint8_t)(app->rule_idx < 0 ? 0 : app->rule_idx);
    app->rule_seg_n = 0;
    if(r == BbRuleSkip || r == BbRuleDouble) {
        app->rule_segs[app->rule_seg_n].text = (r == BbRuleSkip) ? "SKIP" : "DOUBLE";
        app->rule_segs[app->rule_seg_n++].btn = -1;
        app->rule_segs[app->rule_seg_n].text = NULL;
        app->rule_segs[app->rule_seg_n++].btn = (int8_t)app->rule_a;
    } else if(r == BbRuleSwap) {
        app->rule_segs[app->rule_seg_n].text = NULL;
        app->rule_segs[app->rule_seg_n++].btn = (int8_t)app->rule_a;
        app->rule_segs[app->rule_seg_n].text = "IS";
        app->rule_segs[app->rule_seg_n++].btn = -1;
        app->rule_segs[app->rule_seg_n].text = NULL;
        app->rule_segs[app->rule_seg_n++].btn = (int8_t)app->rule_b;
    } else {
        app->rule_segs[app->rule_seg_n].text = bb_rule_label[r];
        app->rule_segs[app->rule_seg_n++].btn = -1;
    }
}

void bb_pick_rule(BeepbackApp* app) {
    /* Draw the rule once and re-roll the sequence around it. Re-drawing
       the rule on every failed attempt quietly skewed which rules you
       ever saw: the fussy ones got replaced instead of retried. */
    app->rule_idx = (int8_t)bb_rng_below(&app->rng, BB_RULE_COUNT);
    app->rule_a = bb_rand_button(app);
    app->rule_b = (uint8_t)((app->rule_a + 1 + bb_rng_below(&app->rng, BbBtnCount - 1)) % BbBtnCount);
    for(uint8_t tries = 0; tries < 60; tries++) {
        bb_new_base(app, app->target);
        if(bb_rule_fits_run(app)) {
            bb_rule_segs(app);
            return;
        }
    }
    app->rule_idx = BbRuleNoDoubles;
    bb_rule_segs(app);
}

void bb_new_round(BeepbackApp* app) {
    if(app->mode == BbModeRules) {
        bb_pick_rule(app);
    } else if(bb_is_challenge_mode(app->mode)) {
        /* the rule is already fixed for the whole run */
    } else {
        app->rule_idx = -1;
        app->rule_seg_n = 0;
        bb_new_base(app, app->target);
    }
    bb_set_stage(app, 1);
}

/* Challenge: one sequence that never resets. Each win adds a step, and
   the step is re-rolled if it would leave the rule nothing to press. */
void bb_challenge_grow(BeepbackApp* app) {
    if(app->base.len >= BB_MAX_SEQ) {
        bb_set_stage(app, app->base.len);
        return;
    }
    for(uint8_t tries = 0; tries < 40; tries++) {
        uint8_t before = app->base.len, n = before;
        uint8_t b = bb_rand_button(app);
        if(n >= 2 && app->base.step[n - 1] == b && app->base.step[n - 2] == b)
            b = (uint8_t)((b + 1 + bb_rng_below(&app->rng, BbBtnCount - 1)) % BbBtnCount);
        app->base.step[app->base.len++] = b;
        BbPresses out;
        bb_apply_run_rule(app, &app->base, &out);
        if(out.len > 0) {
            bb_set_stage(app, app->base.len);
            return;
        }
        app->base.len = before;
    }
    app->base.step[app->base.len++] = bb_rand_button(app);
    bb_set_stage(app, app->base.len);
}

uint32_t bb_daily_seed(void) {
    return bb_today_seed();
}

uint8_t bb_daily_rule(void) {
    /* today's rule, worked out without disturbing the live generator */
    BbRng r;
    bb_rng_seed(&r, bb_daily_seed());
    return bb_rng_below(&r, BB_RULE_COUNT);
}

void bb_start_challenge(BeepbackApp* app) {
    if(app->mode == BbModeDaily) {
        /* Seeded from the date, so every device that knows what day it is
           generates the identical run. No server involved. */
        bb_rng_seed(&app->rng, bb_daily_seed());
        app->rng_seeded = true;
        app->set.diff = 1;
        app->set.speed = 1; /* locked, or nobody is comparable */
        app->ch_rule = bb_rng_below(&app->rng, BB_RULE_COUNT);
        bb_daily_refresh(app, bb_daily_seed());
    } else {
        bb_rng_seed(&app->rng, app->seed);
        app->seed = bb_rng_next(&app->rng);
        app->rng_seeded = false;
        app->ch_rule = (app->ch_idx == BB_RULE_COUNT) ?
                           bb_rng_below(&app->rng, BB_RULE_COUNT) :
                           app->ch_idx;
    }
    app->rule_idx = (int8_t)app->ch_rule;
    app->rule_a = bb_rand_button(app);
    app->rule_b = (uint8_t)((app->rule_a + 1 + bb_rng_below(&app->rng, BbBtnCount - 1)) % BbBtnCount);
    bb_rule_segs(app);
    app->base.len = 0;
    bb_challenge_grow(app);
}

/* ------------------------------------------------------------------ */
/* enter()                                                             */
/* ------------------------------------------------------------------ */

static const uint8_t BB_LED_ORDER[BbBtnCount] = {BbBtnDown, BbBtnLeft, BbBtnOk, BbBtnRight, BbBtnUp};

void bb_enter(BeepbackApp* app, BbScene scene) {
    app->scene = scene;

    if(scene == BbSceneListen) app->phase = app->now + BB_LISTEN_MS;
    if(scene == BbScenePlayback) {
        app->play_idx = 0;
        app->tone_on = true;
        app->step_start = app->now;
        app->phase = app->now + bb_tone_ms(app);
        bb_tone(app, bb_button_hz[app->base.step[0]], bb_tone_ms(app));
        if(bb_led_mode(app)) bb_led_flash(app, bb_button_led[app->base.step[0]], bb_tone_ms(app));
    }
    if(scene == BbSceneGo) {
        app->phase = app->now + BB_GO_MS;
        bb_play(app, bb_jingle_go, 3);
        bb_led_flash(app, BbLedBlue, 160);
    }
    if(scene == BbSceneInput) {
        app->input_idx = 0;
        app->last_press = -1;
        app->input_start = app->now;
        app->input_end = app->now + bb_window_ms((BbMode)app->mode, app->set.diff);
    }
    if(scene == BbSceneHold) app->phase = app->now + bb_tone_ms(app) + bb_gap_ms(app);
    if(scene == BbSceneSuccess) {
        /* not the run's generator: the daily must not depend on praise */
        app->praise = (uint8_t)((app->praise + 1 + (app->now >> 5)) % BB_PRAISE_COUNT);
        app->score += bb_apply_mult(10u * app->stage, app->run_mult);
        if(app->stage > app->run_best) app->run_best = app->stage;
        app->phase = app->now + BB_SUCCESS_MS;
        bb_play(app, bb_jingle_win, 3);
        bb_led_flash(app, BbLedGreen, 220);
    }
    if(scene == BbSceneRoundClear) {
        app->sweep_idx = -1;
        app->score += bb_apply_mult(50u * app->round, app->run_mult);
        app->phase = app->now + BB_ROUND_MS;
        bb_play(app, bb_jingle_round, 4);
        bb_led_flash(app, BbLedGreen, 600);
    }
    if(scene == BbSceneWrong) {
        if(app->lives > 0) app->lives--;
        app->phase = app->now + BB_WRONG_MS;
        bb_play(app, bb_jingle_fail, 3);
        bb_led_flash(app, BbLedRed, 400);
    }
    if(scene == BbSceneRetry) app->phase = app->now + BB_RETRY_MS;
    if(scene == BbSceneRuleCard) app->phase = app->now + BB_RULECARD_MS;
    if(scene == BbSceneReflexGap)
        app->phase = app->now + bb_rx_gap[app->set.speed < BB_SPEED_COUNT ? app->set.speed : 1];
    if(scene == BbSceneReflexCue) {
        app->rx_cue = bb_rand_button(app);
        app->rx_at = app->now;
        app->phase = app->now + app->rx_window;
        uint16_t ring = app->rx_window < 160 ? app->rx_window : 160;
        bb_tone(app, bb_button_hz[app->rx_cue], ring);
        if(bb_led_mode(app)) bb_led_flash(app, bb_button_led[app->rx_cue], ring);
    }
    if(scene == BbSceneGameOver) {
        app->go_page = 0;
        app->lock_until = app->now + BB_OVER_LOCK; /* a press in flight must not retry */
        app->prev_best = bb_best_for(app, app->run_game_mode, app->run_mode);
        app->new_best = app->score > app->prev_best;
        if(app->run_game_mode == BbModeDaily) {
            if(app->score > app->rec.daily_best[app->run_mode])
                app->rec.daily_best[app->run_mode] = app->score;
            app->rec.daily_date = bb_daily_seed();
            app->rec.daily_done = true; /* one a day, and that was it */
        } else if(app->run_game_mode == BbModeChallenge) {
            uint8_t r = app->ch_rule % BB_RULE_COUNT;
            if(app->score > app->rec.ch_best[r][app->run_mode])
                app->rec.ch_best[r][app->run_mode] = app->score;
        } else {
            uint32_t* cell =
                &app->rec.best[app->run_game_mode][app->run_diff][app->run_speed][app->run_mode];
            if(app->score > *cell) *cell = app->score;
        }
        bb_play(app, bb_jingle_over, 4);
        bb_led_flash(app, BbLedRed, 900);
    }
    if(scene == BbSceneMode) app->mode_idx = app->mode;
    if(scene == BbSceneSoundTest) app->test_btn = -1;
}

void bb_start_game(BeepbackApp* app) {
    app->lives = BB_LIVES;
    app->score = 0;
    app->round = 1;
    app->run_best = 0;
    app->new_best = false;
    app->run_mode = bb_assist(app);
    app->run_speed = app->set.speed;
    app->run_game_mode = app->mode;
    app->run_mult = bb_multiplier_for(app);
    app->run_diff = app->set.diff;

    if(app->mode == BbModeReflex) {
        app->rx_hits = 0;
        app->rx_fastest = 0;
        app->rx_window = BB_RX_START;
        app->lives = 1; /* one miss ends it */
        bb_enter(app, BbSceneReflexGap);
        return;
    }
    if(bb_is_challenge_mode(app->mode)) {
        bb_start_challenge(app);
        /* the daily locks time and speed, so the multiplier is recaptured */
        app->run_mult = bb_multiplier_for(app);
        app->run_diff = app->set.diff;
        app->run_speed = app->set.speed;
        bb_enter(app, BbSceneRuleCard);
        return;
    }
    bb_rng_seed(&app->rng, app->seed);
    app->seed = bb_rng_next(&app->rng);
    app->rng_seeded = false;
    app->target = BB_START_LEN;
    bb_new_round(app);
    bb_enter(app, app->mode == BbModeRules ? BbSceneRuleCard : BbSceneListen);
}

/* ------------------------------------------------------------------ */
/* Presses                                                             */
/* ------------------------------------------------------------------ */

void bb_pause_shift(BeepbackApp* app, uint32_t ms) {
    if(!ms) return;
    app->phase += ms;
    app->input_start += ms;
    app->input_end += ms;
    app->step_start += ms;
    app->press_flash += ms;
    app->rx_at += ms;
    app->tune_next += ms;
    app->led_until += ms;
}

void bb_reflex_miss(BeepbackApp* app) {
    app->lives = 0;
    app->run_best = app->rx_fastest;
    bb_led_flash(app, BbLedRed, 400);
    bb_play(app, bb_jingle_fail, 3);
    bb_enter(app, BbSceneGameOver);
}

void bb_reflex_hit(BeepbackApp* app, uint8_t btn) {
    /* floor at 1ms: zero would read as "no record yet" and never update */
    uint32_t reaction = app->now > app->rx_at ? app->now - app->rx_at : 1;
    if(reaction < 1) reaction = 1;
    if(reaction > 0xFFFFu) reaction = 0xFFFFu;
    if(!app->rx_fastest || reaction < app->rx_fastest) app->rx_fastest = (uint16_t)reaction;
    app->rx_hits++;
    app->score += bb_apply_mult(bb_rx_hit_value(app->rx_window), app->run_mult);
    /* no real floor: the ramp has to end for everyone, however quick */
    uint16_t shrink = bb_rx_shrink[app->set.diff < BB_DIFF_COUNT ? app->set.diff : 1];
    app->rx_window = (app->rx_window > BB_RX_FLOOR + shrink) ?
                         (uint16_t)(app->rx_window - shrink) :
                         BB_RX_FLOOR;
    bb_led_flash(app, bb_button_led[btn], 90);
    bb_enter(app, BbSceneReflexGap);
}

void bb_take_press(BeepbackApp* app, uint8_t btn) {
    app->last_press = (int8_t)btn;
    app->press_flash = app->now + bb_tone_ms(app); /* the cue lasts as long as the tone */
    bb_tone(app, bb_button_hz[btn], bb_tone_ms(app));
    bb_led_flash(app, bb_button_led[btn], BB_PRESS_LED_MS); /* your press echoes its colour */
    if(app->input_idx < app->expected.len && btn == app->expected.press[app->input_idx]) {
        app->input_idx++;
        if(app->input_idx >= app->expected.len) bb_enter(app, BbSceneHold);
    } else {
        bb_enter(app, BbSceneWrong);
    }
}

/* ------------------------------------------------------------------ */
/* update()                                                            */
/* ------------------------------------------------------------------ */

void bb_update(BeepbackApp* app) {
    if(app->paused) return;
    const uint32_t now = app->now;

    if(app->scene == BbSceneListen && now >= app->phase) {
        bb_enter(app, BbScenePlayback);
    } else if(app->scene == BbScenePlayback && now >= app->phase) {
        if(app->tone_on) {
            app->tone_on = false;
            app->phase = now + bb_gap_ms(app);
        } else {
            app->play_idx++;
            if(app->play_idx >= app->stage) {
                bb_enter(app, BbSceneGo);
            } else {
                app->tone_on = true;
                app->step_start = now;
                app->phase = now + bb_tone_ms(app);
                bb_tone(app, bb_button_hz[app->base.step[app->play_idx]], bb_tone_ms(app));
                if(bb_led_mode(app))
                    bb_led_flash(app, bb_button_led[app->base.step[app->play_idx]],
                                 bb_tone_ms(app));
            }
        }
    } else if(app->scene == BbSceneGo && now >= app->phase) {
        bb_enter(app, BbSceneInput);
    } else if(app->scene == BbSceneInput) {
        if(now >= app->input_end) {
            bb_enter(app, BbSceneWrong);
        } else {
            /* the last quarter of the bar pulses red, faster as it drains */
            uint32_t total = app->input_end - app->input_start;
            uint32_t left = app->input_end - now;
            if(total && left * 4 < total && now >= app->led_warn) {
                bb_led_flash(app, BbLedRed, 70);
                app->led_warn = now + 120 + (400u * left * 4) / total;
            }
        }
    } else if(app->scene == BbSceneHold && now >= app->phase) {
        /* a challenge has no rounds to clear, it just keeps growing */
        bb_enter(app, (bb_is_challenge_mode(app->mode) || app->stage < app->target) ?
                          BbSceneSuccess :
                          BbSceneRoundClear);
    } else if(app->scene == BbSceneSuccess && now >= app->phase) {
        if(bb_is_challenge_mode(app->mode)) {
            bb_challenge_grow(app);
        } else {
            bb_set_stage(app, (uint8_t)(app->stage + 1));
        }
        bb_enter(app, BbSceneListen);
    } else if(app->scene == BbSceneRuleCard && now >= app->phase) {
        bb_enter(app, BbSceneListen);
    } else if(app->scene == BbSceneReflexGap) {
        if(now >= app->phase) bb_enter(app, BbSceneReflexCue);
    } else if(app->scene == BbSceneReflexCue) {
        if(now >= app->phase) bb_reflex_miss(app);
    } else if(app->scene == BbSceneRoundClear) {
        /* one sweep up the colour ladder, red to violet, then move on */
        uint32_t slot = BB_ROUND_MS / 5;
        uint32_t elapsed = app->phase > now ? BB_ROUND_MS - (app->phase - now) : BB_ROUND_MS;
        int8_t idx = (int8_t)(elapsed / slot);
        if(idx > 4) idx = 4;
        if(idx != app->sweep_idx) {
            app->sweep_idx = idx;
            bb_led_flash(app, bb_button_led[BB_LED_ORDER[idx]], (slot * 7) / 10);
        }
        if(now >= app->phase) {
            app->round++;
            app->target++;
            bb_new_round(app);
            bb_enter(app, app->mode == BbModeRules ? BbSceneRuleCard : BbSceneListen);
        }
    } else if(app->scene == BbSceneWrong && now >= app->phase) {
        bb_enter(app, app->lives == 0 ? BbSceneGameOver : BbSceneRetry);
    } else if(app->scene == BbSceneRetry && now >= app->phase) {
        bb_enter(app, BbSceneListen); /* same sequence, same stage */
    }
}

void bb_tick(BeepbackApp* app, uint32_t dt_ms) {
    app->now += dt_ms;
    if(app->scene == BbSceneSplash) {
        bb_update_splash(app);
    } else {
        bb_update(app);
    }
    if(!app->paused) bb_tune_tick(app);
    if(app->led_until && app->now >= app->led_until) {
        app->led_until = 0;
        app->led = BbLedOff;
    }
}

void bb_app_init(BeepbackApp* app) {
    memset(app, 0, sizeof(*app));
    app->running = true;
    app->set.volume = 2; /* SET = { volume:2, assist:2, speed:1 } */
    app->set.assist = 2;
    app->set.speed = 1;
    app->set.diff = 1;
    app->first_run = true;
    app->rule_idx = -1;
    app->test_btn = -1;
    app->det_time = 1;
    app->det_speed = 1;
    app->target = BB_START_LEN;
    app->stage = 1;
    app->lives = BB_LIVES;
    app->round = 1;
    app->run_mult = 10000;
    app->scene = BbSceneSplash;
    app->test_from = BbSceneMenu;
    app->help_from = BbSceneHelp;
}
