/* Host-side check of the state machine: what is reachable, where BACK
   goes, and that nothing anywhere wraps. */
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

static const char* const scene_name[BbSceneCount] = {
    "launcher", "splash",   "tutorial", "menu",     "modeselect", "rulepick", "setup",
    "game",     "pause",    "over",     "howtopick", "howto",     "soundtest", "settings",
    "sounds",   "scores",   "detail",   "reset",    "boardpick",  "board",    "credits",
};

static const InputKey keys[6] = {
    InputKeyUp, InputKeyDown, InputKeyLeft, InputKeyRight, InputKeyOk, InputKeyBack};

static void boot(BeepbackApp* app) {
    bb_app_init(app);
    app->seed = 0xB33FBACCu;
}

/* Walk the machine at random, which is the only honest way to ask what a
   player can actually get to. Time advances a tick a step so the timed
   scenes resolve on their own. */
static void walk(BeepbackApp* app, uint32_t steps, bool* seen, BbRng* rng) {
    for(uint32_t i = 0; i < steps; i++) {
        bb_tick(app, BB_TICK_MS);
        bb_input(app, keys[bb_rng_below(rng, 6)]);
        if(!app->running) { /* BACK off the launcher leaves; come back in */
            app->running = true;
            bb_go(app, BbSceneLauncher);
        }
        seen[app->scene] = true;
    }
}

int main(void) {
    char msg[256];
    BeepbackApp app;

    /* ---- cursors clamp, they never wrap ---- */
    uint8_t cur = 0;
    check("a cursor at the top will not step up", !bb_list_move(&cur, 5, -1) && cur == 0, "");
    cur = 4;
    check("nor off the bottom", !bb_list_move(&cur, 5, 1) && cur == 4, "");
    cur = 2;
    check("but it moves in the middle", bb_list_move(&cur, 5, 1) && cur == 3, "");
    check("an empty list moves nowhere", !bb_list_move(&cur, 0, 1), "");

    /* ---- every scene is reachable from a cold start ---- */
    bool seen[BbSceneCount];
    memset(seen, 0, sizeof(seen));
    boot(&app);
    seen[app.scene] = true;
    BbRng rng;
    bb_rng_seed(&rng, 4242u);
    walk(&app, 400000, seen, &rng);
    int missing = 0;
    int p = 0;
    msg[0] = 0;
    for(int s = 0; s < BbSceneCount; s++)
        if(!seen[s]) {
            missing++;
            p += sprintf(msg + p, "%s ", scene_name[s]);
        }
    check("every scene can be reached by pressing buttons", missing == 0, missing ? msg : "all 21");

    /* ---- BACK goes where the map says, from every scene ---- */
    int bad = 0;
    p = 0;
    msg[0] = 0;
    for(int s = 0; s < BbSceneCount; s++) {
        boot(&app);
        app.scene = (BbScene)s;
        app.now = 100000; /* past the game over lock */
        app.scene_at = 0;
        if(s == BbSceneGame || s == BbScenePause) {
            bb_run_start(&app, BbModeClassic);
            app.scene = (BbScene)s;
        }
        BbScene want = bb_back_target(&app);
        bb_input(&app, InputKeyBack);
        BbScene got = app.running ? app.scene : BbSceneCount;
        if(got != want) {
            bad++;
            p += sprintf(msg + p, "%s->%d not %d ", scene_name[s], got, want);
        }
    }
    check("BACK follows the navigation map everywhere", bad == 0, bad ? msg : "21 scenes");

    /* ---- the exits the map calls out by name ---- */
    boot(&app);
    app.scene = BbSceneLauncher;
    bb_input(&app, InputKeyBack);
    check("BACK on the launcher leaves the app", !app.running, "");

    boot(&app);
    bb_go(&app, BbSceneMenu);
    bb_input(&app, InputKeyBack);
    check("BACK on the menu returns to the launcher", app.scene == BbSceneLauncher, "");

    /* challenge reached setup through the rule picker, so back goes there */
    boot(&app);
    app.run.mode = BbModeChallenge;
    bb_go(&app, BbSceneSetup);
    bb_input(&app, InputKeyBack);
    check("BACK from a challenge setup lands on the rule picker", app.scene == BbSceneRulePick, "");
    boot(&app);
    app.run.mode = BbModeClassic;
    bb_go(&app, BbSceneSetup);
    bb_input(&app, InputKeyBack);
    check("BACK from a classic setup lands on mode select", app.scene == BbSceneModeSelect, "");

    /* ---- reflex refuses to pause ---- */
    boot(&app);
    bb_run_start(&app, BbModeReflex);
    bb_go(&app, BbSceneGame);
    bb_input(&app, InputKeyBack);
    check("BACK in reflex ends the run instead of pausing", app.scene == BbSceneOver, scene_name[app.scene]);
    check("and the run really is over", app.run.phase == BbPhaseOver && app.run.quit, "");

    boot(&app);
    bb_run_start(&app, BbModeClassic);
    bb_go(&app, BbSceneGame);
    bb_input(&app, InputKeyBack);
    check("BACK in classic pauses", app.scene == BbScenePause, scene_name[app.scene]);
    check("and the run is still alive", app.run.phase != BbPhaseOver, "");
    bb_input(&app, InputKeyOk);
    check("OK resumes it", app.scene == BbSceneGame, scene_name[app.scene]);
    bb_input(&app, InputKeyBack);
    bb_input(&app, InputKeyBack);
    check("BACK twice quits to game over", app.scene == BbSceneOver && app.run.quit, "");

    /* ---- game over locks its input while the screen wipes ---- */
    boot(&app);
    bb_go(&app, BbSceneOver);
    bb_input(&app, InputKeyBack);
    check("game over ignores input during the wipe", app.scene == BbSceneOver, "");
    bb_tick(&app, BB_OVER_LOCK);
    bb_input(&app, InputKeyBack);
    check("and accepts it once the wipe is done", app.scene == BbSceneMenu, "");

    /* ---- nothing wraps, on any list ---- */
    static const BbScene lists[] = {
        BbSceneMenu, BbSceneRulePick, BbSceneSetup, BbSceneSettings, BbSceneSounds,
        BbSceneScores, BbSceneReset, BbSceneBoardPick, BbSceneHowToPick, BbSceneSoundTest};
    bad = 0;
    p = 0;
    msg[0] = 0;
    for(size_t i = 0; i < COUNT_OF(lists); i++) {
        boot(&app);
        bb_go(&app, lists[i]);
        uint8_t count = bb_list_count(&app, lists[i]);
        if(count == 0) {
            bad++;
            p += sprintf(msg + p, "%s has no rows ", scene_name[lists[i]]);
            continue;
        }
        /* mash past both ends; a wrap would show up as a jump */
        for(int k = 0; k < 20; k++) bb_input(&app, InputKeyDown);
        if(bb_can_adjust(&app, 0)) bad++; /* a zero step never moves anything */
        for(int k = 0; k < 20; k++) bb_input(&app, InputKeyUp);
        if(app.scene != lists[i]) {
            bad++;
            p += sprintf(msg + p, "%s wandered off ", scene_name[lists[i]]);
        }
    }
    check("every list has rows and holds its ends", bad == 0, bad ? msg : "10 lists");

    boot(&app);
    bb_go(&app, BbSceneMenu);
    for(int k = 0; k < 9; k++) bb_input(&app, InputKeyDown);
    check("the menu stops at its last row", app.menu_cur == 2, "");
    for(int k = 0; k < 9; k++) bb_input(&app, InputKeyUp);
    check("and at its first", app.menu_cur == 0, "");

    /* ---- values clamp too, and the arrows follow ---- */
    boot(&app);
    bb_go(&app, BbSceneSettings);
    app.settings_cur = 0;
    for(int k = 0; k < 9; k++) bb_input(&app, InputKeyLeft);
    check("volume bottoms out at zero", app.set.volume == 0, "");
    check("and shows no left arrow there", !bb_can_adjust(&app, -1), "");
    check("but does show a right one", bb_can_adjust(&app, 1), "");
    for(int k = 0; k < 9; k++) bb_input(&app, InputKeyRight);
    sprintf(msg, "volume %u", app.set.volume);
    check("volume tops out at its last step", app.set.volume == BB_VOL_COUNT - 1, msg);
    check("and shows no right arrow there", !bb_can_adjust(&app, 1), "");

    boot(&app);
    app.run.mode = BbModeClassic;
    bb_go(&app, BbSceneSetup);
    for(int k = 0; k < 9; k++) bb_input(&app, InputKeyRight);
    check("TIME clamps at INSANE", app.set.diff == BB_DIFF_COUNT - 1, "");
    bb_input(&app, InputKeyDown);
    for(int k = 0; k < 9; k++) bb_input(&app, InputKeyLeft);
    check("SPEED clamps at its slowest", app.set.speed == 0, "");

    /* ---- mode select: three pages, and the short one keeps the cursor in ---- */
    boot(&app);
    bb_go(&app, BbSceneModeSelect);
    for(int k = 0; k < 9; k++) bb_input(&app, InputKeyRight);
    check("mode select stops on the last page", app.mode_page == 2, "");
    check("whose single row holds the cursor", app.mode_row == 0, "");
    bb_input(&app, InputKeyOk);
    check("and that row is the daily", app.run.mode == BbModeDaily, "");
    for(int k = 0; k < 9; k++) bb_input(&app, InputKeyLeft);
    boot(&app);
    bb_go(&app, BbSceneModeSelect);
    bb_input(&app, InputKeyDown); /* RULES */
    bb_input(&app, InputKeyRight); /* a page change lands on that page's first */
    check("changing page lands on the first mode of it", app.mode_row == 0, "");
    bb_input(&app, InputKeyOk);
    check("which on page two is reflex", app.run.mode == BbModeReflex, "");
    bb_go(&app, BbSceneModeSelect);
    app.mode_page = 1;
    bb_input(&app, InputKeyDown); /* and the row below it is challenge */
    bb_input(&app, InputKeyOk);
    check("page two row two is challenge", app.run.mode == BbModeChallenge, "");
    check("which asks for a rule first", app.scene == BbSceneRulePick, scene_name[app.scene]);

    /* ---- the daily setup does not let you change the run ---- */
    boot(&app);
    app.run.mode = BbModeDaily;
    bb_go(&app, BbSceneSetup);
    check("the daily pins the cursor to START", app.setup_cur == 2, "");
    bb_input(&app, InputKeyUp);
    check("which will not move", app.setup_cur == 2, "");
    uint8_t was = app.set.diff;
    bb_input(&app, InputKeyLeft);
    bb_input(&app, InputKeyRight);
    check("and TIME is locked", app.set.diff == was, "");
    check("with no arrows offered", !bb_can_adjust(&app, -1) && !bb_can_adjust(&app, 1), "");

    /* ---- assist has to leave you something to go on ---- */
    boot(&app);
    app.set.volume = 0;
    app.set.assist = BbAssistOff;
    check("silence plus ears only becomes shapes", bb_effective_assist(&app) == BbAssistShapes, "");
    app.set.volume = 2;
    check("with sound it stays ears only", bb_effective_assist(&app) == BbAssistOff, "");

    /* ---- the splash runs itself out, and any key skips it ---- */
    boot(&app);
    bb_go(&app, BbSceneSplash);
    app.set.tutorial_done = true;
    for(int k = 0; k < 400 && app.scene == BbSceneSplash; k++) bb_tick(&app, BB_TICK_MS);
    check("the splash ends on its own", app.scene == BbSceneMenu, scene_name[app.scene]);
    boot(&app);
    bb_go(&app, BbSceneSplash);
    bb_input(&app, InputKeyRight);
    check("and a first run goes to the tutorial", app.scene == BbSceneTutorial, scene_name[app.scene]);
    bb_input(&app, InputKeyBack);
    check("leaving the tutorial marks it seen", app.set.tutorial_done, "");
    bb_go(&app, BbSceneSplash);
    bb_input(&app, InputKeyOk);
    check("so the next launch goes straight to the menu", app.scene == BbSceneMenu, "");

    /* ---- reset really resets ---- */
    boot(&app);
    app.rec.best[BbModeClassic][1][1][2] = 999;
    app.set.tutorial_done = true;
    bb_go(&app, BbSceneReset);
    bb_input(&app, InputKeyOk);
    check("RESET SCORES clears the records", app.rec.best[BbModeClassic][1][1][2] == 0, "");
    check("and leaves the tutorial alone", app.set.tutorial_done, "");
    bb_input(&app, InputKeyDown);
    bb_input(&app, InputKeyOk);
    check("RESET TUTORIAL brings it back", !app.set.tutorial_done, "");

    printf(fails ? "\n%d FAILURES\n" : "\nall state machine checks passed\n", fails);
    return fails ? 1 : 0;
}
