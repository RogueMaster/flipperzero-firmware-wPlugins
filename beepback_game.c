/*
 * BEEPBACK - a run from start to game over.
 *
 * The engine is a phase machine driven entirely by app->now, which only
 * bb_tick() advances, so a host test can play a whole run in a loop with
 * no device and no waiting. Nothing in here calls a Flipper API: sound
 * and light leave through app->tone_hz and app->led.
 */
#include "beepback.h"

/* Classic is the only mode that plays the sequence back unchanged. The
   browser build had this guard written as `mode != RULES`, which quietly
   let challenge announce a rule and then ignore it, so it lives in one
   named function that every caller goes through. */
bool bb_run_has_rule(BbMode mode) {
    return mode == BbModeRules || bb_is_challenge(mode);
}

static void bb_phase(BeepbackApp* app, BbPhase phase, uint32_t ms) {
    app->run.phase = phase;
    app->run.phase_end = app->now + ms;
}

void bb_run_shift(BeepbackApp* app, uint32_t ms) {
    BbRun* run = &app->run;
    if(!ms) return;
    run->phase_end += ms;
    run->win_end += ms;
    run->rx_at += ms;
    if(run->flash_end) run->flash_end += ms;
}

static void bb_silence(BeepbackApp* app) {
    app->tone_hz = 0;
    app->led = BbLedOff;
}

/* ------------------------------------------------------------------ */
/* Records                                                             */
/* ------------------------------------------------------------------ */

uint32_t* bb_record_slot(BeepbackApp* app, const BbRun* run) {
    uint8_t as = run->assist < BB_ASSIST_COUNT ? run->assist : 0;
    uint8_t d = run->diff < BB_DIFF_COUNT ? run->diff : 1;
    uint8_t s = run->speed < BB_SPEED_COUNT ? run->speed : 1;
    switch(run->mode) {
    case BbModeClassic:
    case BbModeRules:
    case BbModeReflex:
        return &app->rec.best[run->mode][d][s][as];
    case BbModeChallenge:
        return &app->rec.ch_best[run->rule % BB_RULE_COUNT][as];
    case BbModeDaily:
        return &app->rec.daily_best[as];
    default:
        return NULL;
    }
}

uint32_t bb_best_of_mode(const BeepbackApp* app, BbMode mode, uint8_t assist) {
    if(assist >= BB_ASSIST_COUNT) assist = 0;
    uint32_t best = 0;
    if(mode == BbModeDaily) return app->rec.daily_best[assist];
    if(mode == BbModeChallenge) {
        for(uint8_t r = 0; r < BB_RULE_COUNT; r++)
            if(app->rec.ch_best[r][assist] > best) best = app->rec.ch_best[r][assist];
        return best;
    }
    if(mode >= BB_LADDER_MODES) return 0;
    /* the board shows the best across every time and speed for the mode */
    for(uint8_t d = 0; d < BB_DIFF_COUNT; d++)
        for(uint8_t s = 0; s < BB_SPEED_COUNT; s++)
            if(app->rec.best[mode][d][s][assist] > best) best = app->rec.best[mode][d][s][assist];
    return best;
}

void bb_daily_refresh(BeepbackApp* app, uint32_t today) {
    if(app->rec.daily_date == today) return;
    /* a new day is a new run, so last night's bests go with it */
    memset(app->rec.daily_best, 0, sizeof(app->rec.daily_best));
    app->rec.daily_date = today;
    app->rec.daily_done = false;
}

/* ------------------------------------------------------------------ */
/* Building sequences                                                  */
/* ------------------------------------------------------------------ */

void bb_run_build_presses(BbRun* run) {
    BbSeq shown;
    shown.len = run->shown <= run->seq.len ? run->shown : run->seq.len;
    memcpy(shown.step, run->seq.step, shown.len);
    if(bb_run_has_rule(run->mode)) {
        bb_apply_rule(&shown, run->rule, run->ra, run->rb, &run->press);
    } else {
        run->press.len = shown.len;
        memcpy(run->press.press, shown.step, shown.len);
    }
}

/* Never three of the same button running. Costs a second draw, and only
   where the correction is actually needed, which is what keeps the
   sequence in step with the browser build draw for draw. */
static uint8_t bb_no_triple(BbRun* run, uint8_t at, uint8_t btn) {
    if(at >= 2 && run->seq.step[at - 1] == btn && run->seq.step[at - 2] == btn)
        return (uint8_t)((btn + 1 + bb_rng_below(&run->rng, BbBtnCount - 1)) % BbBtnCount);
    return btn;
}

/* When a run of re-rolls has not landed on a sequence the rule can live
   with, walk each position through the five buttons in turn. It is
   deterministic, so two devices given the same unlucky draw repair it
   the same way, and it converges where thirty-two random tries did not. */
static bool bb_repair(BbSeq* seq, BbRule rule, uint8_t a, uint8_t b) {
    for(uint8_t at = seq->len; at-- > 0;) {
        uint8_t was = seq->step[at];
        for(uint8_t btn = 0; btn < BbBtnCount; btn++) {
            seq->step[at] = btn;
            if(bb_rule_fits(seq, rule, a, b)) return true;
        }
        seq->step[at] = was;
    }
    return false;
}

void bb_run_new_round(BbRun* run, uint8_t target) {
    if(target > BB_MAX_SEQ) target = BB_MAX_SEQ;
    if(target == 0) target = 1;
    run->target = target;
    bool ruled = bb_run_has_rule(run->mode);
    bool ok = !ruled;
    /* Re-roll the sequence, never the rule: the rule is what the round
       announced and what the player is being asked to hold. */
    for(uint8_t tries = 0; tries < 60 && !ok; tries++) {
        run->seq.len = target;
        for(uint8_t i = 0; i < target; i++)
            run->seq.step[i] = bb_no_triple(run, i, bb_rng_below(&run->rng, BbBtnCount));
        ok = bb_rule_fits(&run->seq, run->rule, run->ra, run->rb);
    }
    if(!ruled) {
        run->seq.len = target;
        for(uint8_t i = 0; i < target; i++)
            run->seq.step[i] = bb_no_triple(run, i, bb_rng_below(&run->rng, BbBtnCount));
    } else if(!ok) {
        /* sixty sequences and the rule would not take any of them, so fall
           back to the one rule that fits anything: NO DOUBLES needs only a
           repeat somewhere, and bb_repair guarantees the rest */
        run->rule = BbRuleNoDoubles;
        if(!bb_rule_fits(&run->seq, run->rule, run->ra, run->rb))
            bb_repair(&run->seq, run->rule, run->ra, run->rb);
    }
    /* one step at a time, from one: check 1 of bb_rule_fits() has already
       guaranteed no stage along the way leaves nothing to press */
    run->shown = 1;
    run->idx = 0;
    bb_run_build_presses(run);
}

/* One more step on the end.
 *
 * The acceptance test here is only that the player is left something to
 * press. The rule proved it had something to say when the run started,
 * so re-proving it on every step would be stricter than the browser
 * build and would reject steps it accepts - and the two have to grow the
 * same sequence from the same seed or the daily is not one run. */
void bb_run_grow(BbRun* run) {
    if(run->seq.len >= BB_MAX_SEQ) {
        bb_run_build_presses(run);
        return;
    }
    uint8_t at = run->seq.len;
    bool ruled = bb_run_has_rule(run->mode);

    for(uint8_t tries = 0; tries < 40; tries++) {
        run->seq.step[at] = bb_no_triple(run, at, bb_rng_below(&run->rng, BbBtnCount));
        run->seq.len = (uint8_t)(at + 1);
        if(!ruled) break; /* nothing can empty a sequence that has no rule */
        BbPresses p;
        bb_apply_rule(&run->seq, run->rule, run->ra, run->rb, &p);
        if(p.len > 0) break; /* accept */
        run->seq.len = at; /* reject, and go round again */
    }
    if(run->seq.len == at) {
        /* forty tries and none of them stuck: take one and move on */
        run->seq.step[at] = bb_rng_below(&run->rng, BbBtnCount);
        run->seq.len = (uint8_t)(at + 1);
    }

    if(run->shown < run->seq.len) run->shown = run->seq.len;
    bb_run_build_presses(run);
}

/* ------------------------------------------------------------------ */
/* Choosing a rule                                                     */
/* ------------------------------------------------------------------ */

static void bb_roll_rule_params(BbRun* run) {
    run->ra = bb_rng_below(&run->rng, BbBtnCount);
    /* rb must differ from ra or X IS Y says nothing */
    run->rb = (uint8_t)((run->ra + 1 + bb_rng_below(&run->rng, BbBtnCount - 1)) % BbBtnCount);
}

void bb_daily_setup(BbRun* run, uint32_t seed) {
    /* The order of these draws is part of the daily: change it and the
       browser build stops producing the same run from the same date. */
    bb_rng_seed(&run->rng, seed);
    run->rule = (BbRule)bb_rng_below(&run->rng, BB_RULE_COUNT);
    bb_roll_rule_params(run);
    run->diff = 1; /* TIME and SPEED are the day's, not the player's */
    run->speed = 1;
}

/* ------------------------------------------------------------------ */
/* Starting and ending                                                 */
/* ------------------------------------------------------------------ */

void bb_run_start(BeepbackApp* app, BbMode mode) {
    BbRun* run = &app->run;
    memset(run, 0, sizeof(*run));
    run->mode = mode;
    run->assist = bb_effective_assist(app);
    run->lives = (mode == BbModeReflex) ? 1 : BB_LIVES;

    if(mode == BbModeDaily) {
        bb_daily_setup(run, bb_today_seed());
    } else {
        run->diff = app->set.diff;
        run->speed = app->set.speed;
        bb_rng_seed(&run->rng, app->seed);
        app->seed = bb_rng_next(&run->rng); /* a different run next time */
        if(mode == BbModeChallenge) {
            run->rule_random = (app->rule_cur >= BB_RULE_COUNT);
            run->rule = run->rule_random ? (BbRule)bb_rng_below(&run->rng, BB_RULE_COUNT) :
                                           (BbRule)app->rule_cur;
            bb_roll_rule_params(run);
        } else if(mode == BbModeRules) {
            run->rule = (BbRule)bb_rng_below(&run->rng, BB_RULE_COUNT);
            bb_roll_rule_params(run);
        }
    }

    /* captured once, so moving a dial mid-run cannot revalue what you
       have already earned */
    run->mult = bb_multiplier(mode, run->diff, run->speed);
    run->round = 1;

    if(mode == BbModeReflex) {
        run->rx_win = BB_RX_START;
        bb_phase(app, BbPhaseRxReady, BB_LISTEN_MS + BB_GO_MS);
        return;
    }

    if(bb_is_challenge(mode)) {
        /* one sequence that only ever grows; the first step is a growth
           from nothing, taken through the same guard as all the rest */
        run->seq.len = 0;
        bb_run_grow(run);
        run->target = 0; /* no target: challenge shows LEN, not stage/target */
        run->idx = 0;
    } else {
        bb_run_new_round(run, BB_START_LEN);
    }

    if(bb_run_has_rule(mode)) {
        bb_phase(app, BbPhaseRuleCard, BB_RULECARD_MS);
    } else {
        bb_phase(app, BbPhaseListen, BB_LISTEN_MS);
    }
}

void bb_run_end(BeepbackApp* app, bool quit) {
    BbRun* run = &app->run;
    if(run->phase == BbPhaseOver) return;
    run->quit = quit;
    run->phase = BbPhaseOver;
    run->phase_end = app->now;
    bb_silence(app);

    uint32_t* slot = bb_record_slot(app, run);
    /* what the run was trying to beat, captured before it is overwritten */
    run->prev_best = slot ? *slot : 0;
    if(slot && run->score > *slot) {
        *slot = run->score;
        run->record = true;
    }
    if(run->mode == BbModeDaily) {
        app->rec.daily_date = bb_today_seed();
        app->rec.daily_done = true; /* spent, whether or not it went well */
    }
}

/* ------------------------------------------------------------------ */
/* Playback                                                            */
/* ------------------------------------------------------------------ */

static uint16_t bb_tone_ms(const BbRun* run) {
    return bb_speed_tone[run->speed < BB_SPEED_COUNT ? run->speed : 1];
}

static uint16_t bb_gap_ms(const BbRun* run) {
    uint16_t gap = bb_speed_gap[run->speed < BB_SPEED_COUNT ? run->speed : 1];
    /* a shape or an arrow needs a beat of its own to read as separate */
    if(run->assist != BbAssistOff) gap = (uint16_t)(gap + BB_ASSIST_GAP);
    return gap;
}

static void bb_show_step(BeepbackApp* app, uint8_t button) {
    app->tone_hz = bb_button_hz[button];
    app->led = bb_button_led[button];
}

static void bb_begin_playback(BeepbackApp* app) {
    BbRun* run = &app->run;
    run->play_i = 0;
    run->play_on = false;
    bb_silence(app);
    bb_phase(app, BbPhasePlayback, 0); /* the next tick lights step one */
}

static void bb_begin_input(BeepbackApp* app) {
    BbRun* run = &app->run;
    run->idx = 0;
    run->win_ms = bb_window_ms(run->mode, run->diff);
    run->win_end = app->now + run->win_ms;
    bb_silence(app);
    bb_phase(app, BbPhaseInput, run->win_ms);
}

static void bb_playback_tick(BeepbackApp* app) {
    BbRun* run = &app->run;
    if(run->play_on) {
        /* the tone is over; clear it and move on */
        run->play_on = false;
        bb_silence(app);
        run->play_i++;
        if(run->play_i >= run->shown) {
            bb_phase(app, BbPhaseGo, BB_GO_MS);
        } else {
            bb_phase(app, BbPhasePlayback, bb_gap_ms(run));
        }
    } else {
        run->play_on = true;
        bb_show_step(app, run->seq.step[run->play_i]);
        bb_phase(app, BbPhasePlayback, bb_tone_ms(run));
    }
}

/* ------------------------------------------------------------------ */
/* Reflex                                                              */
/*                                                                     */
/* No memory and no second chance. One cue at a time, a window that     */
/* tightens with every hit, and a single miss - including a press in    */
/* the gap between cues - ends the run. There is no pause: freezing a   */
/* live cue would be a cheat, so BACK ends it too, which the ready      */
/* screen says before the first cue goes up.                            */
/* ------------------------------------------------------------------ */

static void bb_rx_wait(BeepbackApp* app) {
    BbRun* run = &app->run;
    bb_silence(app);
    /* the gap is the beat, and the beat is what SPEED sets */
    bb_phase(app, BbPhaseRxWait, bb_rx_gap[run->speed < BB_SPEED_COUNT ? run->speed : 1]);
}

static void bb_rx_cue(BeepbackApp* app) {
    BbRun* run = &app->run;
    run->rx_cue = bb_rng_below(&run->rng, BbBtnCount);
    run->rx_at = app->now;
    bb_show_step(app, run->rx_cue);
    /* the bar the screen draws is the same deadline the engine judges by */
    run->win_ms = run->rx_win;
    run->win_end = app->now + run->rx_win;
    bb_phase(app, BbPhaseRxCue, run->rx_win);
}

static void bb_rx_hit(BeepbackApp* app) {
    BbRun* run = &app->run;
    /* floored at 1ms: zero would read as "no reaction recorded yet" and
       never be beaten */
    uint32_t reaction = app->now > run->rx_at ? app->now - run->rx_at : 1;
    if(reaction > 0xFFFFu) reaction = 0xFFFFu;
    if(!run->rx_fastest || reaction < run->rx_fastest)
        run->rx_fastest = (uint16_t)reaction;
    /* paid at the window it was taken at, then the window tightens */
    run->award = bb_apply_mult(bb_rx_hit_value(run->rx_win), run->mult);
    run->bonus = 0;
    run->score += run->award;
    run->hits++;
    if(run->hits < 0xFFu) run->longest = (uint8_t)run->hits;

    uint16_t shrink = bb_rx_shrink[run->diff < BB_DIFF_COUNT ? run->diff : 1];
    run->rx_win = (run->rx_win > BB_RX_FLOOR + shrink) ? (uint16_t)(run->rx_win - shrink) :
                                                         BB_RX_FLOOR;
    run->flash_btn = run->rx_cue;
    run->flash_end = app->now + BB_PRESS_LED_MS;
    bb_rx_wait(app);
}

static void bb_rx_miss(BeepbackApp* app) {
    app->run.lives = 0;
    app->run.longest = 0; /* reflex measures itself in hits and milliseconds */
    app->tone_hz = 0;
    app->led = BbLedRed;
    bb_run_end(app, false);
}

/* ------------------------------------------------------------------ */
/* Resolving a stage                                                    */
/* ------------------------------------------------------------------ */

/* Both awards are multiplied here, once, and stored as the exact amount
   that went into the score. The browser build showed the flat award and
   banked the multiplied one, so a +40 arrived as +76 and nobody could
   reconcile the total. */
static void bb_stage_clear(BeepbackApp* app) {
    BbRun* run = &app->run;
    if(run->shown > run->longest) run->longest = run->shown;
    /* deliberately not run->rng: the daily's sequence must not depend on
       which line of praise came up */
    app->praise = (uint8_t)((app->praise + 1 + (app->now >> 5)) % BB_PRAISE_COUNT);
    run->award = bb_apply_mult(10u * run->shown, run->mult);
    run->bonus = 0;
    run->score += run->award;

    if(bb_is_challenge(run->mode)) {
        bb_phase(app, BbPhaseSuccess, BB_SUCCESS_MS); /* no rounds to clear */
        return;
    }
    if(run->shown >= run->target) {
        run->bonus = bb_apply_mult(50u * run->round, run->mult);
        run->score += run->bonus;
        bb_phase(app, BbPhaseRound, BB_ROUND_MS);
    } else {
        bb_phase(app, BbPhaseSuccess, BB_SUCCESS_MS);
    }
}

static void bb_next_stage(BeepbackApp* app) {
    BbRun* run = &app->run;
    if(bb_is_challenge(run->mode)) {
        bb_run_grow(run); /* one longer, forever, guarded by the rule */
    } else {
        if(run->shown < run->seq.len) run->shown++;
        bb_run_build_presses(run);
    }
    run->idx = 0;
    bb_phase(app, BbPhaseListen, BB_LISTEN_MS);
}

static void bb_next_round(BeepbackApp* app) {
    BbRun* run = &app->run;
    run->round++;
    /* RULES changes the rule every round; the card announces the new one */
    if(run->mode == BbModeRules) {
        run->rule = (BbRule)bb_rng_below(&run->rng, BB_RULE_COUNT);
        bb_roll_rule_params(run);
    }
    /* round r builds to 3 + r, which is BB_START_LEN on the first */
    bb_run_new_round(run, (uint8_t)(BB_START_LEN + run->round - 1));
    if(bb_run_has_rule(run->mode)) {
        bb_phase(app, BbPhaseRuleCard, BB_RULECARD_MS);
    } else {
        bb_phase(app, BbPhaseListen, BB_LISTEN_MS);
    }
}

static void bb_wrong(BeepbackApp* app) {
    BbRun* run = &app->run;
    if(run->lives) run->lives--;
    run->flash_end = 0;
    app->tone_hz = 0;
    app->led = BbLedRed; /* every mistake is red, in every mode */
    bb_phase(app, BbPhaseWrong, BB_WRONG_MS);
}

void bb_run_tick(BeepbackApp* app) {
    BbRun* run = &app->run;
    if(run->phase == BbPhaseOver) return;

    /* a pressed button stays lit briefly, independently of the phase */
    if(run->flash_end && app->now >= run->flash_end) {
        run->flash_end = 0;
        if(run->phase == BbPhaseInput) bb_silence(app);
    }

    if(app->now < run->phase_end) return;

    switch(run->phase) {
    case BbPhaseRuleCard:
        bb_phase(app, BbPhaseListen, BB_LISTEN_MS);
        break;
    case BbPhaseListen:
        bb_begin_playback(app);
        break;
    case BbPhasePlayback:
        bb_playback_tick(app);
        break;
    case BbPhaseGo:
        bb_begin_input(app);
        break;
    case BbPhaseInput:
        /* the window ran out, which costs the same as a wrong button */
        bb_wrong(app);
        break;
    case BbPhaseHold:
        bb_stage_clear(app);
        break;
    case BbPhaseSuccess:
        bb_next_stage(app);
        break;
    case BbPhaseRound:
        bb_next_round(app);
        break;
    case BbPhaseWrong:
        if(run->lives == 0) {
            bb_run_end(app, false);
        } else {
            /* the same sequence, the same stage: nothing is regenerated */
            bb_silence(app);
            bb_phase(app, BbPhaseRetry, BB_RETRY_MS);
        }
        break;
    case BbPhaseRetry:
        bb_begin_playback(app);
        break;
    case BbPhaseRxReady:
        bb_rx_wait(app);
        break;
    case BbPhaseRxWait:
        bb_rx_cue(app);
        break;
    case BbPhaseRxCue:
        bb_rx_miss(app); /* the window emptied */
        break;
    default:
        break;
    }
}

void bb_run_press(BeepbackApp* app, BbButton btn) {
    BbRun* run = &app->run;
    if(btn >= BbBtnCount) return;

    if(run->mode == BbModeReflex) {
        if(run->phase == BbPhaseRxCue) {
            if(btn == run->rx_cue) {
                bb_rx_hit(app);
            } else {
                bb_rx_miss(app);
            }
        } else if(run->phase == BbPhaseRxWait) {
            bb_rx_miss(app); /* jumping the gap is a miss, not a free go */
        }
        return;
    }

    /* anything pressed outside the response window is simply not heard */
    if(run->phase != BbPhaseInput) return;
    if(run->idx >= run->press.len) return;

    if(btn != run->press.press[run->idx]) {
        bb_wrong(app);
        return;
    }

    run->idx++;
    run->flash_btn = btn;
    run->flash_end = app->now + BB_PRESS_LED_MS;
    app->tone_hz = bb_button_hz[btn];
    app->led = bb_button_led[btn];

    if(run->idx >= run->press.len) {
        /* let the last press ring before the stage resolves */
        bb_phase(app, BbPhaseHold, BB_PRESS_LED_MS);
    }
}

