/* Host-side check of reflex: the beat, the ramp, and the single miss. */
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

static void start(BeepbackApp* app, uint8_t diff, uint8_t speed) {
    bb_app_init(app);
    app->seed = 0x2EF1E4u;
    app->set.diff = diff;
    app->set.speed = speed;
    app->run.mode = BbModeReflex;
    bb_go(app, BbSceneSetup);
    app->setup_cur = 2;
    bb_input(app, InputKeyOk);
}

static bool wait_for(BeepbackApp* app, BbPhase want) {
    for(int i = 0; i < 4000; i++) {
        if(app->run.phase == want) return true;
        if(app->run.phase == BbPhaseOver) return false;
        bb_tick(app, BB_TICK_MS);
    }
    return false;
}

int main(void) {
    char msg[200];
    BeepbackApp app;

    /* ---- one life, and it says so before the first cue ---- */
    start(&app, 1, 1);
    check("reflex gets one life, not three", app.run.lives == 1, "");
    check("and holds a ready screen before the first cue", app.run.phase == BbPhaseRxReady, "");
    check("with the window at its start", app.run.rx_win == BB_RX_START, "");
    check("on reflex's own multiplier table", app.run.mult == bb_multiplier(BbModeReflex, 1, 1), "");

    /* ---- the gap between cues is the speed setting ---- */
    for(uint8_t sp = 0; sp < BB_SPEED_COUNT; sp++) {
        start(&app, 1, sp);
        wait_for(&app, BbPhaseRxWait);
        uint32_t t0 = app.now;
        wait_for(&app, BbPhaseRxCue);
        uint32_t gap = app.now - t0;
        sprintf(msg, "%lu ms, wanted %u", (unsigned long)gap, bb_rx_gap[sp]);
        /* deadlines land on a tick, so it can overshoot by one */
        check("the gap between cues matches the speed setting",
              gap >= bb_rx_gap[sp] && gap < (uint32_t)(bb_rx_gap[sp] + BB_TICK_MS), msg);
    }

    /* ---- the window tightens by the amount TIME asks for ---- */
    for(uint8_t d = 0; d < BB_DIFF_COUNT; d++) {
        start(&app, d, 2);
        uint16_t seen[6];
        int n = 0;
        for(int hit = 0; hit < 6; hit++) {
            if(!wait_for(&app, BbPhaseRxCue)) break;
            seen[n++] = app.run.rx_win;
            bb_input(&app, key_of(app.run.rx_cue));
        }
        int bad = 0;
        for(int i = 1; i < n; i++)
            if(seen[i] != seen[i - 1] - bb_rx_shrink[d]) bad++;
        sprintf(msg, "%u %u %u %u, step %u", seen[0], seen[1], seen[2], seen[3], bb_rx_shrink[d]);
        check("the window tightens by the ramp for that TIME", n == 6 && bad == 0, msg);
    }
    check("and every ramp starts everyone at the same window", BB_RX_START == 1000, "");

    /* the ramp has a floor, because a run at zero could never end */
    start(&app, 3, 2);
    for(int hit = 0; hit < 400; hit++) {
        if(!wait_for(&app, BbPhaseRxCue)) break;
        bb_input(&app, key_of(app.run.rx_cue));
    }
    sprintf(msg, "window %u after %lu hits", app.run.rx_win, (unsigned long)app.run.hits);
    check("the window never goes below its floor", app.run.rx_win >= BB_RX_FLOOR, msg);
    check("and stops there rather than at zero", app.run.rx_win == BB_RX_FLOOR, msg);

    /* ---- what a hit pays ---- */
    start(&app, 1, 1);
    wait_for(&app, BbPhaseRxCue);
    {
        uint16_t win = app.run.rx_win;
        bb_input(&app, key_of(app.run.rx_cue));
        sprintf(msg, "%lu at %u ms", (unsigned long)app.run.score, win);
        check("a hit pays its window's value",
              app.run.score == bb_apply_mult(bb_rx_hit_value(win), app.run.mult), msg);
        check("and counts", app.run.hits == 1, "");
        check("the first one being worth ten flat", app.run.score == 10, msg);
    }
    /* tighter windows pay more, which is the whole ramp */
    {
        uint32_t first = app.run.score;
        wait_for(&app, BbPhaseRxCue);
        bb_input(&app, key_of(app.run.rx_cue));
        sprintf(msg, "%lu then %lu", (unsigned long)first, (unsigned long)(app.run.score - first));
        check("and a tighter one pays more", app.run.score - first >= first, msg);
    }

    /* ---- reflex measures how fast, not just how many ---- */
    start(&app, 1, 1);
    wait_for(&app, BbPhaseRxCue);
    for(int i = 0; i < 6; i++) bb_tick(&app, BB_TICK_MS); /* take a beat, then hit */
    bb_input(&app, key_of(app.run.rx_cue));
    sprintf(msg, "%u ms", app.run.rx_fastest);
    check("a hit records the reaction time", app.run.rx_fastest > 0, msg);
    check("and it is the time actually taken", app.run.rx_fastest >= 100, msg);
    {
        uint16_t slow = app.run.rx_fastest;
        wait_for(&app, BbPhaseRxCue);
        bb_input(&app, key_of(app.run.rx_cue)); /* instantly */
        sprintf(msg, "%u then %u", slow, app.run.rx_fastest);
        check("a quicker one replaces it", app.run.rx_fastest < slow, msg);
        wait_for(&app, BbPhaseRxCue);
        for(int i = 0; i < 10; i++) bb_tick(&app, BB_TICK_MS);
        uint16_t best = app.run.rx_fastest;
        bb_input(&app, key_of(app.run.rx_cue));
        check("but a slower one does not", app.run.rx_fastest == best, "");
    }

    /* ---- one miss and it is over ---- */
    start(&app, 1, 1);
    wait_for(&app, BbPhaseRxCue);
    bb_input(&app, key_of((uint8_t)((app.run.rx_cue + 1) % BbBtnCount)));
    check("a wrong button ends the run outright", app.run.phase == BbPhaseOver, "");
    check("with no lives left over", app.run.lives == 0, "");
    check("and it was not a quit", !app.run.quit, "");

    start(&app, 1, 1);
    wait_for(&app, BbPhaseRxCue);
    for(int i = 0; i < 200 && app.run.phase == BbPhaseRxCue; i++) bb_tick(&app, BB_TICK_MS);
    check("letting the window empty ends it too", app.run.phase == BbPhaseOver, "");

    /* pressing in the gap is a miss, not a free go */
    start(&app, 1, 0);
    wait_for(&app, BbPhaseRxWait);
    bb_input(&app, InputKeyOk);
    check("a press in the gap between cues is a miss", app.run.phase == BbPhaseOver, "");

    /* ---- there is no pause ---- */
    start(&app, 1, 1);
    wait_for(&app, BbPhaseRxCue);
    bb_input(&app, InputKeyBack);
    check("BACK never reaches a pause screen", app.scene != BbScenePause, "");
    check("it ends the run", app.run.phase == BbPhaseOver, "");
    check("and says the run was quit", app.run.quit, "");
    check("landing on game over", app.scene == BbSceneOver, "");

    /* ---- a reflex run files its best like the ladder modes ---- */
    start(&app, 2, 0);
    wait_for(&app, BbPhaseRxCue);
    bb_input(&app, key_of(app.run.rx_cue));
    {
        uint32_t score = app.run.score;
        bb_input(&app, InputKeyBack);
        sprintf(msg, "%lu", (unsigned long)score);
        check("the best goes in under its own time and speed",
              app.rec.best[BbModeReflex][2][0][bb_effective_assist(&app)] == score, msg);
    }

    /* reflex has no sequence, so nothing about it should look like one */
    start(&app, 1, 1);
    check("reflex builds no sequence", app.run.seq.len == 0 && app.run.press.len == 0, "");
    check("and has no target to show", app.run.target == 0, "");

    printf(fails ? "\n%d FAILURES\n" : "\nall reflex checks passed\n", fails);
    return fails ? 1 : 0;
}
