/* Host-side check of the endless modes: challenge and the daily. One
   rule, one sequence, no rounds and no resets, and the growth guarded so
   the rule can never make the next step unpressable. */
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

static void start(BeepbackApp* app, BbMode mode, uint32_t seed, uint8_t rule_cur) {
    bb_app_init(app);
    app->seed = seed;
    app->rule_cur = rule_cur;
    app->run.mode = mode;
    bb_go(app, BbSceneSetup);
    app->setup_cur = 2;
    bb_input(app, InputKeyOk);
}

static bool wait_input(BeepbackApp* app) {
    for(int i = 0; i < 40000; i++) {
        if(app->run.phase == BbPhaseInput) return true;
        if(app->run.phase == BbPhaseOver) return false;
        bb_tick(app, BB_TICK_MS);
    }
    return false;
}
static void wait_phase_change(BeepbackApp* app, BbPhase from) {
    for(int i = 0; i < 40000 && app->run.phase == from; i++) bb_tick(app, BB_TICK_MS);
}
static void play_stage(BeepbackApp* app) {
    uint8_t n = app->run.press.len;
    for(uint8_t i = 0; i < n; i++) bb_input(app, key_of(app->run.press.press[i]));
}

int main(void) {
    char msg[200];
    BeepbackApp app;

    /* ---- growth, three hundred steps of it ---- */
    {
        int unpressable = 0, one_button = 0, over_cap = 0;
        uint8_t longest = 0;
        for(uint32_t seed = 1; seed <= 12; seed++) {
            start(&app, BbModeChallenge, seed * 2654435761u, (uint8_t)(seed % BB_RULE_COUNT));
            BbRun* run = &app.run;
            for(int i = 0; i < 300; i++) {
                bb_run_grow(run);
                if(run->seq.len > BB_MAX_SEQ) over_cap++;
                if(run->press.len == 0) unpressable++;
                if(run->press.len > BB_MAX_PRESS) over_cap++;
                /* three of the same button running is not a memory test,
                   it is a staring contest, so growth corrects it */
                for(uint8_t k = 2; k < run->seq.len; k++)
                    if(run->seq.step[k] == run->seq.step[k - 1] &&
                       run->seq.step[k] == run->seq.step[k - 2])
                        one_button++;
                if(run->seq.len > longest) longest = run->seq.len;
            }
        }
        sprintf(msg, "%d empty, %d triples, %d overrun, longest %u", unpressable, one_button, over_cap, longest);
        check("three hundred steps of growth always leave something to press",
              unpressable == 0 && one_button == 0 && over_cap == 0, msg);
        check("and stop at the sequence cap", longest == BB_MAX_SEQ, msg);
    }

    /* ---- the rule is chosen once and never moves ---- */
    start(&app, BbModeChallenge, 0xC0FFEEu, BbRuleSwap);
    check("an explicit rule is the one you picked", app.run.rule == BbRuleSwap, "");
    check("and is not marked random", !app.run.rule_random, "");
    {
        BbRule rule = app.run.rule;
        uint8_t ra = app.run.ra, rb = app.run.rb;
        int rounds = 0, changed = 0;
        for(int stage = 0; stage < 20; stage++) {
            if(!wait_input(&app)) break;
            play_stage(&app);
            wait_phase_change(&app, BbPhaseHold);
            if(app.run.rule != rule || app.run.ra != ra || app.run.rb != rb) changed++;
            if(app.run.round != 1) rounds++;
            if(app.run.phase == BbPhaseRound) rounds++;
        }
        sprintf(msg, "%d changes, %d rounds, len %u", changed, rounds, app.run.seq.len);
        check("twenty stages later the rule is still the same one", changed == 0, msg);
        check("and no round ever cleared", rounds == 0, msg);
        check("with no target to clear either", app.run.target == 0, msg);
        check("the sequence only ever grew", app.run.seq.len >= 20, msg);
    }

    /* RANDOM rolls one and then holds it */
    start(&app, BbModeChallenge, 0xABCDEFu, BB_RULE_COUNT);
    check("RANDOM rolls a real rule", app.run.rule < BB_RULE_COUNT, "");
    check("and says that it rolled it", app.run.rule_random, "");
    {
        BbRule rule = app.run.rule;
        for(int stage = 0; stage < 6; stage++) {
            if(!wait_input(&app)) break;
            play_stage(&app);
            wait_phase_change(&app, BbPhaseHold);
        }
        check("which it then keeps for the run", app.run.rule == rule, "");
    }

    /* ---- a mistake does not reset the sequence ---- */
    start(&app, BbModeChallenge, 0x1234u, BbRuleBackwards);
    for(int stage = 0; stage < 4; stage++) {
        wait_input(&app);
        play_stage(&app);
        wait_phase_change(&app, BbPhaseHold);
    }
    wait_input(&app);
    {
        BbSeq before = app.run.seq;
        uint8_t shown = app.run.shown;
        uint32_t score = app.run.score;
        uint8_t want = app.run.press.press[0];
        bb_input(&app, key_of((uint8_t)((want + 1) % BbBtnCount)));
        check("a mistake in a challenge costs a life", app.run.lives == BB_LIVES - 1, "");
        wait_input(&app);
        check("but the sequence is the same one",
              app.run.seq.len == before.len && memcmp(app.run.seq.step, before.step, before.len) == 0, "");
        check("at the same length", app.run.shown == shown, "");
        check("and the score is untouched", app.run.score == score, "");
    }

    /* ---- what a challenge stage pays ---- */
    start(&app, BbModeChallenge, 0x77u, BbRuleLastTwice);
    {
        uint32_t before = app.run.score;
        uint8_t len = app.run.shown;
        wait_input(&app);
        play_stage(&app);
        wait_phase_change(&app, BbPhaseHold);
        sprintf(msg, "len %u paid %lu", len, (unsigned long)(app.run.score - before));
        check("a challenge stage pays ten a step",
              app.run.score - before == bb_apply_mult(10u * len, app.run.mult), msg);
        check("and never a round bonus", app.run.bonus == 0, "");
    }

    /* ---- the daily is the same run everywhere ---- */
    {
        BbRun a, b;
        memset(&a, 0, sizeof(a));
        memset(&b, 0, sizeof(b));
        a.mode = b.mode = BbModeDaily;
        bb_daily_setup(&a, 20260910u);
        bb_daily_setup(&b, 20260910u);
        check("one date gives one rule", a.rule == b.rule && a.ra == b.ra && a.rb == b.rb, "");
        check("locked to NORMAL time and speed", a.diff == 1 && a.speed == 1, "");
        bb_run_new_round(&a, 12);
        bb_run_new_round(&b, 12);
        check("and one sequence", memcmp(a.seq.step, b.seq.step, 12) == 0, "");

        BbRun c;
        memset(&c, 0, sizeof(c));
        c.mode = BbModeDaily;
        bb_daily_setup(&c, 20260911u);
        bb_run_new_round(&c, 12);
        int differs = (c.rule != a.rule) || memcmp(c.seq.step, a.seq.step, 12) != 0;
        check("the next day is a different run", differs, "");
        sprintf(msg, "rule %u, %u becomes %u", a.rule, a.ra, a.rb);
        check("X IS Y never maps a button to itself", a.ra != a.rb, msg);
    }

    /* ---- the daily, pinned step for step ---- */
    /* This is the whole of what a date produces: the rule, its two
       buttons, and the sequence the run grows. It is the only thing
       standing between the device and the browser generating the same
       daily, and the twelve-value generator test would not notice it
       drifting, because the generator can be identical while the
       procedure built on it is not.
     *
     * PROVISIONAL. These are this build's values. The browser build
     * reports rule EVERY OTHER and 033221042032412034102302402014 for
     * the same date, which this build cannot reproduce at any offset,
     * under any rule, with either correction variant. Until the two
     * agree, this pin catches drift on our side only - it does not
     * certify parity. */
    {
        bb_app_init(&app);
        app.seed = 1;
        bb_run_start(&app, BbModeDaily);
        char got[64];
        int at = 0;
        for(int i = 0; i < 29; i++) bb_run_grow(&app.run);
        for(uint8_t i = 0; i < 30 && i < app.run.seq.len; i++)
            at += sprintf(got + at, "%u", app.run.seq.step[i]);
        got[at] = 0;

        sprintf(msg, "rule %u, %u becomes %u", app.run.rule, app.run.ra, app.run.rb);
        check("the daily for 2026-09-10 picks the rule it always has",
              app.run.rule == BbRuleNoDoubles && app.run.ra == 2 && app.run.rb == 0, msg);
        check("and grows the sequence it always has",
              strcmp(got, "102114334400314231030040221432") == 0, got);

        /* the same date twice is the same run, procedure and all */
        BeepbackApp again;
        bb_app_init(&again);
        again.seed = 999;
        bb_run_start(&again, BbModeDaily);
        for(int i = 0; i < 29; i++) bb_run_grow(&again.run);
        check("and the hardware seed cannot touch any of it",
              again.run.rule == app.run.rule && again.run.ra == app.run.ra &&
                  memcmp(again.run.seq.step, app.run.seq.step, 30) == 0, "");
    }

    /* every date has to produce a rule whose buttons make sense */
    {
        int bad = 0;
        for(uint32_t day = 20260101u; day <= 20260131u; day++) {
            BbRun r;
            memset(&r, 0, sizeof(r));
            r.mode = BbModeDaily;
            bb_daily_setup(&r, day);
            if(r.rule >= BB_RULE_COUNT || r.ra >= BbBtnCount || r.rb >= BbBtnCount || r.ra == r.rb)
                bad++;
        }
        check("a month of dailies all make sense", bad == 0, "");
    }

    /* ---- one attempt a day ---- */
    bb_app_init(&app);
    app.seed = 1;
    bb_daily_refresh(&app, bb_today_seed());
    app.run.mode = BbModeDaily;
    bb_go(&app, BbSceneSetup);
    bb_input(&app, InputKeyOk);
    check("the daily starts when it has not been played", app.scene == BbSceneGame, "");
    bb_run_end(&app, true);
    check("and is marked spent when it ends", app.rec.daily_done, "");
    check("against today's date", app.rec.daily_date == bb_today_seed(), "");
    bb_go(&app, BbSceneSetup);
    bb_input(&app, InputKeyOk);
    check("a second attempt the same day does not start", app.scene == BbSceneSetup, "");

    /* opening the daily setup checks the date itself, so a session left
       running past midnight does not keep yesterday's attempt spent */
    app.rec.daily_date = bb_today_seed() - 1;
    app.rec.daily_done = true;
    app.rec.daily_best[BbAssistShapes] = 4321;
    bb_go(&app, BbSceneSetup);
    check("opening the daily past midnight rolls it over",
          !app.rec.daily_done && app.rec.daily_best[BbAssistShapes] == 0, "");
    bb_input(&app, InputKeyOk);
    check("and the new day's attempt starts", app.scene == BbSceneGame, "");
    bb_run_end(&app, true);

    /* a new day clears the day's records and gives the attempt back */
    app.rec.daily_best[BbAssistShapes] = 4321;
    bb_daily_refresh(&app, bb_today_seed() + 1);
    check("a new day clears yesterday's daily best", app.rec.daily_best[BbAssistShapes] == 0, "");
    check("and hands back the attempt", !app.rec.daily_done, "");
    app.rec.daily_best[BbAssistShapes] = 99;
    bb_daily_refresh(&app, bb_today_seed() + 1);
    check("but the same day again changes nothing", app.rec.daily_best[BbAssistShapes] == 99, "");

    /* ---- the daily keeps a best per assist, not per setting ---- */
    bb_app_init(&app);
    app.rec.daily_best[BbAssistArrows] = 555;
    check("the daily board reads its own store",
          bb_best_of_mode(&app, BbModeDaily, BbAssistArrows) == 555, "");
    app.rec.ch_best[BbRuleSwap][BbAssistLed] = 800;
    app.rec.ch_best[BbRuleSkip][BbAssistLed] = 900;
    check("the challenge board takes the best rule",
          bb_best_of_mode(&app, BbModeChallenge, BbAssistLed) == 900, "");

    start(&app, BbModeChallenge, 42u, BbRuleSkip);
    app.run.score = 1234;
    bb_run_end(&app, false);
    check("a challenge best is filed under its rule",
          app.rec.ch_best[BbRuleSkip][bb_effective_assist(&app)] == 1234, "");

    printf(fails ? "\n%d FAILURES\n" : "\nall challenge checks passed\n", fails);
    return fails ? 1 : 0;
}
