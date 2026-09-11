/* Host-side check of the game loop: what you press, what it pays, and
   what a mistake costs. A whole run plays out in a loop here because the
   engine takes its time from bb_tick() and nothing else. */
#define BB_HOST_TEST 1
#include <stdio.h>
#include <string.h>
#include "beepback.h"
#include "../beepback_rules.c"
#include "../beepback_nav.c"
#include "../beepback_game.c"

static int fails = 0;
static void check(const char* name, int ok, const char* extra) {
    printf("%s %s%s%s\n", ok ? "ok  " : "FAIL", name, extra && *extra ? "  -> " : "", extra ? extra : "");
    if(!ok) fails++;
}

/* the rule names, kept here rather than dragging the whole canvas in
   for a printf */
static const char* const rule_label[BB_RULE_COUNT] =
    {"SKIP", "DOUBLE", "NODBL", "EVERY2", "LAST2", "XISY", "BACK"};

static InputKey key_of(uint8_t btn) {
    switch(btn) {
    case BbBtnUp:
        return InputKeyUp;
    case BbBtnDown:
        return InputKeyDown;
    case BbBtnLeft:
        return InputKeyLeft;
    case BbBtnRight:
        return InputKeyRight;
    default:
        return InputKeyOk;
    }
}

/* Start a run the way a player does: through the setup screen. */
static void start(BeepbackApp* app, BbMode mode, uint8_t diff, uint8_t speed, uint8_t assist) {
    bb_app_init(app);
    app->seed = 0x5EEDC0DEu;
    app->set.diff = diff;
    app->set.speed = speed;
    app->set.assist = assist;
    app->run.mode = mode;
    bb_go(app, BbSceneSetup);
    app->setup_cur = 2;
    bb_input(app, InputKeyOk);
}

/* Run the clock until the engine is waiting for a press, or the run ends. */
static bool wait_input(BeepbackApp* app) {
    for(int i = 0; i < 8000; i++) {
        if(app->run.phase == BbPhaseInput) return true;
        if(app->run.phase == BbPhaseOver) return false;
        bb_tick(app, BB_TICK_MS);
    }
    return false;
}

static void wait_phase_change(BeepbackApp* app, BbPhase from) {
    for(int i = 0; i < 8000 && app->run.phase == from; i++) bb_tick(app, BB_TICK_MS);
}

static void play_stage(BeepbackApp* app) {
    uint8_t n = app->run.press.len;
    for(uint8_t i = 0; i < n; i++) bb_input(app, key_of(app->run.press.press[i]));
}

static void miss_stage(BeepbackApp* app) {
    uint8_t want = app->run.press.press[app->run.idx];
    bb_input(app, key_of((uint8_t)((want + 1) % BbBtnCount)));
}

int main(void) {
    char msg[200];
    int p2;
    BeepbackApp app;

    /* ---- one stage, one award, and the award is what went in ---- */
    start(&app, BbModeClassic, 1, 1, BbAssistShapes);
    check("a run opens on its first stage", app.run.shown == 1 && app.run.target == BB_START_LEN, "");
    check("with three lives", app.run.lives == BB_LIVES, "");
    check("and its multiplier captured", app.run.mult == bb_multiplier(BbModeClassic, 1, 1), "");
    check("classic plays back exactly what it played", !bb_run_has_rule(BbModeClassic), "");

    wait_input(&app);
    check("the first stage is one press long", app.run.press.len == 1, "");
    play_stage(&app);
    wait_phase_change(&app, BbPhaseHold);
    sprintf(msg, "score %lu award %lu", (unsigned long)app.run.score, (unsigned long)app.run.award);
    check("a stage of one pays ten at x1.00", app.run.score == 10 && app.run.award == 10, msg);
    check("and the amount shown is the amount banked", app.run.score == app.run.award, "");

    /* ---- a whole round, against the formulas ---- */
    start(&app, BbModeClassic, 1, 1, BbAssistShapes);
    for(int stage = 0; stage < BB_START_LEN; stage++) {
        if(!wait_input(&app)) break;
        play_stage(&app);
        wait_phase_change(&app, BbPhaseHold);
    }
    sprintf(msg, "%lu", (unsigned long)app.run.score);
    check("round one at x1.00 pays 10+20+30+40 and 50", app.run.score == 150, msg);
    check("its round bonus is separate from the stage award", app.run.bonus == 50 && app.run.award == 40, "");
    wait_phase_change(&app, BbPhaseRound);
    check("clearing a round starts the next", app.run.round == 2, "");
    sprintf(msg, "target %u", app.run.target);
    check("and generates a longer sequence", app.run.target == BB_START_LEN + 1, msg);
    check("built one step at a time again", app.run.shown == 1, "");

    /* the same round with a multiplier, to catch a flat award slipping in */
    start(&app, BbModeClassic, 2, 1, BbAssistShapes); /* HARD, x1.55 */
    check("hard on normal is x1.55", app.run.mult == 155, "");
    for(int stage = 0; stage < BB_START_LEN; stage++) {
        if(!wait_input(&app)) break;
        play_stage(&app);
        wait_phase_change(&app, BbPhaseHold);
    }
    sprintf(msg, "%lu", (unsigned long)app.run.score);
    check("the same round at x1.55 pays 16+31+47+62 and 78", app.run.score == 234, msg);

    /* ---- a mistake replays the same sequence at the same stage ---- */
    start(&app, BbModeClassic, 1, 1, BbAssistShapes);
    for(int stage = 0; stage < 2; stage++) {
        wait_input(&app);
        play_stage(&app);
        wait_phase_change(&app, BbPhaseHold);
    }
    wait_input(&app);
    BbSeq before = app.run.seq;
    uint8_t shown_before = app.run.shown;
    uint32_t score_before = app.run.score;
    miss_stage(&app);
    check("a wrong button is wrong straight away", app.run.phase == BbPhaseWrong, "");
    check("and costs a life", app.run.lives == BB_LIVES - 1, "");
    wait_input(&app);
    check("the retry replays the same sequence",
          memcmp(before.step, app.run.seq.step, before.len) == 0 && before.len == app.run.seq.len, "");
    check("at the same stage", app.run.shown == shown_before, "");
    check("with the score untouched", app.run.score == score_before, "");
    check("and the presses counted from the start again", app.run.idx == 0, "");

    /* ---- three mistakes end it ---- */
    start(&app, BbModeClassic, 1, 1, BbAssistShapes);
    for(int life = 0; life < BB_LIVES; life++) {
        if(!wait_input(&app)) break;
        miss_stage(&app);
        wait_phase_change(&app, BbPhaseWrong);
    }
    check("three mistakes end the run", app.run.phase == BbPhaseOver, "");
    check("which takes you to the game over screen", app.scene == BbSceneOver, "");
    check("not counted as a quit", !app.run.quit, "");

    /* ---- running out of time is a mistake too ---- */
    start(&app, BbModeClassic, 3, 1, BbAssistShapes); /* INSANE, 2000ms */
    wait_input(&app);
    check("the window is the one TIME asked for", app.run.win_ms == bb_window_ms(BbModeClassic, 3), "");
    for(int i = 0; i < 200 && app.run.phase == BbPhaseInput; i++) bb_tick(&app, BB_TICK_MS);
    check("letting the window empty costs a life", app.run.phase == BbPhaseWrong && app.run.lives == BB_LIVES - 1, "");

    /* ---- presses outside the window are not heard ---- */
    start(&app, BbModeClassic, 1, 1, BbAssistShapes);
    bb_tick(&app, BB_TICK_MS);
    check("the run opens in listen", app.run.phase == BbPhaseListen, "");
    bb_input(&app, InputKeyUp);
    bb_input(&app, InputKeyDown);
    check("mashing during playback does nothing", app.run.phase != BbPhaseWrong && app.run.lives == BB_LIVES, "");

    /* ---- rules really does change what you press ---- */
    /* The browser build announced a rule and then played the sequence
       straight, so the check that matters is that the press list the
       engine hands the player is the rule's output, in every ruled mode
       and at every stage - not just that it looks different once. */
    {
        int applied = 0, checked = 0, transformed = 0;
        int bites[BB_RULE_COUNT] = {0};
        static const BbMode ruled[3] = {BbModeRules, BbModeChallenge, BbModeDaily};
        for(uint32_t s = 1; s <= 40; s++) {
            for(int m = 0; m < 3; m++) {
                bb_app_init(&app);
                app.seed = s * 2654435761u;
                app.rule_cur = (uint8_t)(s % (BB_RULE_COUNT + 1));
                app.run.mode = ruled[m];
                bb_go(&app, BbSceneSetup);
                app.setup_cur = 2;
                bb_input(&app, InputKeyOk);
                for(int stage = 0; stage < 3; stage++) {
                    BbSeq shown;
                    shown.len = app.run.shown;
                    memcpy(shown.step, app.run.seq.step, shown.len);
                    BbPresses want;
                    bb_apply_rule(&shown, app.run.rule, app.run.ra, app.run.rb, &want);
                    checked++;
                    if(want.len == app.run.press.len &&
                       memcmp(want.press, app.run.press.press, want.len) == 0)
                        applied++;
                    if(want.len != shown.len ||
                       memcmp(want.press, shown.step, want.len) != 0) {
                        transformed++;
                        bites[app.run.rule]++;
                    }
                    if(!wait_input(&app)) break;
                    play_stage(&app);
                    wait_phase_change(&app, BbPhaseHold);
                    wait_input(&app);
                }
            }
        }
        sprintf(msg, "%d of %d stages", applied, checked);
        check("every ruled mode presses the rule's output, not the sequence",
              applied == checked && checked > 300, msg);
        /* A rule can honestly be a no-op on a given sequence - SKIP of a
           button that never came up - so the sharp check is that every
           one of the seven is seen to bite in real play, not that each
           individual stage differs. */
        int mute = 0;
        p2 = 0;
        msg[0] = 0;
        for(int r = 0; r < BB_RULE_COUNT; r++) {
            if(!bites[r]) mute++;
            p2 += sprintf(msg + p2, "%d ", bites[r]);
        }
        check("all seven rules are seen changing the presses in real runs", mute == 0, msg);
        sprintf(msg, "%d of %d stages", transformed, checked);
        check("which is most stages, the short ones aside", transformed * 5 > checked * 2, msg);
    }

    /* every rule, given a sequence it has something to say about */
    {
        BbSeq s2 = {{BbBtnUp, BbBtnOk, BbBtnOk, BbBtnLeft, BbBtnUp}, 5};
        int silent = 0;
        for(int rule = 0; rule < BB_RULE_COUNT; rule++) {
            BbPresses out;
            bb_apply_rule(&s2, (BbRule)rule, BbBtnOk, BbBtnRight, &out);
            if(out.len == s2.len && memcmp(out.press, s2.step, out.len) == 0) silent++;
        }
        sprintf(msg, "%d silent", silent);
        check("all seven rules change that sequence", silent == 0, msg);
    }

    /* ---- the guard, over four thousand generated rounds ---- */
    /* A round whose rule leaves the full sequence untouched is a dead
       round, and from the player's side it is indistinguishable from the
       rule being ignored. bb_rule_fits() is what stops one being dealt:
       check 1 keeps every stage of the ladder pressable, so the ladder
       can start at one, and check 3 makes the rule bite by the last
       stage. It is meant to do nothing on some early stages - that is
       the rule kicking in as the sequence grows. */
    {
        const int rounds = 4000;
        int dead = 0, empty = 0;
        int bite[BB_START_LEN] = {0};
        int rule_bite[BB_RULE_COUNT] = {0};
        int rule_seen[BB_RULE_COUNT] = {0};
        for(int i = 0; i < rounds; i++) {
            BbRun run;
            memset(&run, 0, sizeof(run));
            run.mode = BbModeRules;
            bb_rng_seed(&run.rng, (uint32_t)(i + 1) * 2654435761u);
            run.rule = (BbRule)bb_rng_below(&run.rng, BB_RULE_COUNT);
            run.ra = bb_rng_below(&run.rng, BbBtnCount);
            run.rb = (uint8_t)((run.ra + 1 + bb_rng_below(&run.rng, BbBtnCount - 1)) % BbBtnCount);
            bb_run_new_round(&run, BB_START_LEN);

            if(run.shown != 1) dead++; /* the ladder must start at one */
            for(uint8_t n = 1; n <= BB_START_LEN; n++) {
                run.shown = n;
                bb_run_build_presses(&run);
                if(run.press.len == 0) empty++;
                BbSeq shown;
                shown.len = n;
                memcpy(shown.step, run.seq.step, n);
                if(run.press.len != n || memcmp(run.press.press, shown.step, n) != 0) {
                    bite[n - 1]++;
                    if(n == 1) rule_bite[run.rule]++;
                }
                if(n == 1) rule_seen[run.rule]++;
            }
            if(bite[BB_START_LEN - 1] != i + 1) dead++; /* the last stage always bites */
        }
        sprintf(msg, "%d dead rounds", dead);
        check("no generated round leaves its rule with nothing to do", dead == 0, msg);
        sprintf(msg, "%d empty press lists", empty);
        check("and no stage of one hands the player nothing to press", empty == 0, msg);
        sprintf(msg, "%d %d %d %d of %d", bite[0], bite[1], bite[2], bite[3], rounds);
        check("the rule kicks in as the sequence grows",
              bite[0] < bite[3] && bite[3] == rounds, msg);

        /* Broken down per rule, so the stage-one rate can be compared
           against the browser build a rule at a time rather than as one
           number. Only three rules can say anything about a one-step
           sequence: LAST TWICE always, DOUBLE and X IS Y when the step
           happens to be the button the rule names. */
        p2 = 0;
        msg[0] = 0;
        for(int r = 0; r < BB_RULE_COUNT; r++)
            p2 += sprintf(msg + p2, "%s %d/%d ", rule_label[r], rule_bite[r], rule_seen[r]);
        check("and only three rules can bite a one-step stage at all",
              rule_bite[BbRuleSkip] == 0 && rule_bite[BbRuleNoDoubles] == 0 &&
                  rule_bite[BbRuleEveryOther] == 0 && rule_bite[BbRuleBackwards] == 0 &&
                  rule_bite[BbRuleLastTwice] == rule_seen[BbRuleLastTwice],
              msg);
    }

    /* the ladder starts at one now, in every ruled mode */
    {
        int late = 0;
        for(uint32_t s = 1; s <= 200; s++) {
            bb_app_init(&app);
            app.seed = s * 40503u;
            app.run.mode = BbModeRules;
            bb_go(&app, BbSceneSetup);
            app.setup_cur = 2;
            bb_input(&app, InputKeyOk);
            if(app.run.shown != 1) late++;
        }
        sprintf(msg, "%d starting late", late);
        check("a ruled ladder starts at one step, like a classic one", late == 0, msg);
    }

    /* the guard the browser build got wrong: challenge has a rule too */
    check("classic has no rule", !bb_run_has_rule(BbModeClassic), "");
    check("rules has one", bb_run_has_rule(BbModeRules), "");
    check("challenge has one", bb_run_has_rule(BbModeChallenge), "");
    check("and so does the daily", bb_run_has_rule(BbModeDaily), "");

    /* ---- a dial moved mid-run cannot revalue what is already earned ---- */
    start(&app, BbModeClassic, 1, 1, BbAssistShapes);
    wait_input(&app);
    play_stage(&app);
    wait_phase_change(&app, BbPhaseHold);
    uint32_t earned = app.run.score;
    app.set.diff = 3; /* the player fiddles with settings from the pause screen */
    app.set.speed = 2;
    check("the captured multiplier does not move", app.run.mult == 100, "");
    wait_input(&app);
    play_stage(&app);
    wait_phase_change(&app, BbPhaseHold);
    sprintf(msg, "%lu then %lu", (unsigned long)earned, (unsigned long)app.run.score);
    check("so the next award is still at the run's rate", app.run.score == earned + 20, msg);

    /* ---- game over goes straight back in ---- */
    start(&app, BbModeClassic, 2, 0, BbAssistArrows);
    wait_input(&app);
    play_stage(&app);
    wait_phase_change(&app, BbPhaseHold);
    {
        uint32_t earned = app.run.score;
        uint16_t mult = app.run.mult;
        bb_run_end(&app, false);
        bb_go(&app, BbSceneOver);
        bb_input(&app, InputKeyOk);
        check("game over ignores OK during the wipe", app.scene == BbSceneOver, "");
        bb_tick(&app, BB_OVER_LOCK);
        bb_input(&app, InputKeyOk);
        check("and then OK starts another run there and then", app.scene == BbSceneGame,
              app.scene == BbSceneSetup ? "went back to setup" : "");
        check("on the same mode", app.run.mode == BbModeClassic, "");
        sprintf(msg, "time %u speed %u", app.run.diff, app.run.speed);
        check("with the same settings", app.run.diff == 2 && app.run.speed == 0, msg);
        check("and the same multiplier", app.run.mult == mult, "");
        sprintf(msg, "%lu", (unsigned long)app.run.score);
        check("but a fresh score and fresh lives",
              app.run.score == 0 && app.run.lives == BB_LIVES && earned > 0, msg);
    }

    /* the daily has no second attempt to offer */
    bb_app_init(&app);
    app.seed = 9;
    bb_daily_refresh(&app, bb_today_seed());
    app.run.mode = BbModeDaily;
    bb_go(&app, BbSceneSetup);
    bb_input(&app, InputKeyOk);
    bb_run_end(&app, true);
    bb_go(&app, BbSceneOver);
    bb_tick(&app, BB_OVER_LOCK);
    bb_input(&app, InputKeyOk);
    check("OK on a spent daily's game over starts nothing", app.scene == BbSceneOver, "");
    bb_input(&app, InputKeyBack);
    check("but BACK still leaves for the menu", app.scene == BbSceneMenu, "");

    /* ---- the record is written when the run ends ---- */
    start(&app, BbModeClassic, 1, 1, BbAssistShapes);
    wait_input(&app);
    play_stage(&app);
    wait_phase_change(&app, BbPhaseHold);
    bb_run_end(&app, false);
    check("a finished run writes its best", app.rec.best[BbModeClassic][1][1][BbAssistShapes] == 10, "");
    check("and knows it was one", app.run.record, "");
    app.rec.best[BbModeClassic][1][1][BbAssistShapes] = 5000;
    start(&app, BbModeClassic, 1, 1, BbAssistShapes);
    app.rec.best[BbModeClassic][1][1][BbAssistShapes] = 5000;
    bb_run_end(&app, false);
    check("a worse run leaves the best alone", app.rec.best[BbModeClassic][1][1][BbAssistShapes] == 5000, "");
    check("and says so", !app.run.record, "");

    /* each assist keeps its own */
    bb_app_init(&app);
    app.rec.best[BbModeClassic][1][1][BbAssistOff] = 700;
    app.rec.best[BbModeClassic][3][2][BbAssistArrows] = 900;
    check("the board takes the best across time and speed",
          bb_best_of_mode(&app, BbModeClassic, BbAssistArrows) == 900, "");
    check("without mixing the assists up",
          bb_best_of_mode(&app, BbModeClassic, BbAssistOff) == 700, "");
    check("and reports nothing where nothing was played",
          bb_best_of_mode(&app, BbModeClassic, BbAssistLed) == 0, "");

    /* ---- speed changes how long playback takes ---- */
    uint32_t took[BB_SPEED_COUNT];
    for(uint8_t sp = 0; sp < BB_SPEED_COUNT; sp++) {
        start(&app, BbModeClassic, 1, sp, BbAssistOff);
        wait_phase_change(&app, BbPhaseListen);
        uint32_t t0 = app.now;
        wait_phase_change(&app, BbPhasePlayback);
        took[sp] = app.now - t0;
    }
    sprintf(msg, "%lu %lu %lu ms", (unsigned long)took[0], (unsigned long)took[1], (unsigned long)took[2]);
    check("slow plays back slower than fast", took[0] > took[1] && took[1] > took[2], msg);

    start(&app, BbModeClassic, 1, 1, BbAssistOff);
    wait_phase_change(&app, BbPhaseListen);
    uint32_t t0 = app.now;
    wait_phase_change(&app, BbPhasePlayback);
    uint32_t ears = app.now - t0;
    start(&app, BbModeClassic, 1, 1, BbAssistShapes);
    wait_phase_change(&app, BbPhaseListen);
    t0 = app.now;
    wait_phase_change(&app, BbPhasePlayback);
    check("a one-step stage is one tone either way", app.now - t0 == ears, "");

    printf(fails ? "\n%d FAILURES\n" : "\nall game loop checks passed\n", fails);
    return fails ? 1 : 0;
}
