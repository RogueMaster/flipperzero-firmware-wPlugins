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

/* The shortest stage this rule can survive. SKIP can empty a short
   sequence out and EVERY OTHER can reduce it to one button, neither of
   which can be played, so the ladder starts above that. Adding a step
   never removes a distinct button from the press list, so every longer
   stage in the round is safe once this one is. */
static uint8_t bb_start_len(const BbRun* run) {
    if(!bb_run_has_rule(run->mode)) return 1;
    BbSeq p;
    for(uint8_t len = 1; len <= run->seq.len; len++) {
        p.len = len;
        memcpy(p.step, run->seq.step, len);
        if(bb_rule_fits(&p, run->rule, run->ra, run->rb)) return len;
    }
    return run->seq.len;
}

void bb_run_new_round(BbRun* run, uint8_t target) {
    if(target > BB_MAX_SEQ) target = BB_MAX_SEQ;
    if(target == 0) target = 1;
    run->target = target;
    for(uint8_t tries = 0; tries < 32; tries++) {
        run->seq.len = target;
        for(uint8_t i = 0; i < target; i++) run->seq.step[i] = bb_rng_below(&run->rng, BbBtnCount);
        if(!bb_run_has_rule(run->mode)) break;
        if(bb_rule_fits(&run->seq, run->rule, run->ra, run->rb)) break;
    }
    run->start_len = bb_start_len(run);
    run->shown = run->start_len;
    run->idx = 0;
    bb_run_build_presses(run);
}

void bb_run_grow(BbRun* run) {
    if(run->seq.len >= BB_MAX_SEQ) {
        bb_run_build_presses(run);
        return;
    }
    uint8_t at = run->seq.len;
    run->seq.len = at + 1;
    bool ruled = bb_run_has_rule(run->mode);
    for(uint8_t tries = 0; tries < 32; tries++) {
        run->seq.step[at] = bb_rng_below(&run->rng, BbBtnCount);
        /* re-roll the new step rather than let the rule make it unpressable */
        if(!ruled || bb_rule_fits(&run->seq, run->rule, run->ra, run->rb)) {
            if(run->shown < run->seq.len) run->shown = run->seq.len;
            bb_run_build_presses(run);
            return;
        }
    }
    /* a run this unlucky still has to be playable, so walk the five */
    for(uint8_t b = 0; b < BbBtnCount; b++) {
        run->seq.step[at] = b;
        if(bb_rule_fits(&run->seq, run->rule, run->ra, run->rb)) break;
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
        /* one sequence, grown from a single step until the rule fits */
        run->seq.len = 1;
        run->seq.step[0] = bb_rng_below(&run->rng, BbBtnCount);
        run->shown = 1;
        while(run->seq.len < BB_MAX_SEQ &&
              !bb_rule_fits(&run->seq, run->rule, run->ra, run->rb))
            bb_run_grow(run);
        run->start_len = run->seq.len;
        run->shown = run->seq.len;
        run->target = 0; /* no target: challenge shows LEN, not stage/target */
        run->idx = 0;
        bb_run_build_presses(run);
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
    case BbPhaseRetry:
        bb_begin_playback(app);
        break;
    default:
        break;
    }
}

void bb_run_press(BeepbackApp* app, BbButton btn) {
    UNUSED(app);
    UNUSED(btn);
    /* resolution lands with the game loop */
}
