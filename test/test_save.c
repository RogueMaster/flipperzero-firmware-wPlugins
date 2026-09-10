/* Host-side check of the save file: that it round-trips, and that
   anything which is not exactly ours is discarded rather than read. */
#define BB_HOST_TEST 1
#include <stdio.h>
#include <string.h>
#include "beepback.h"
#include "../beepback_rules.c"
#include "../beepback_nav.c"
#include "../beepback_game.c"
#include "../beepback_save.c"

static int fails = 0;
static void check(const char* name, int ok, const char* extra) {
    printf("%s %s%s%s\n", ok ? "ok  " : "FAIL", name, extra && *extra ? "  -> " : "", extra ? extra : "");
    if(!ok) fails++;
}

/* Fill every record store with something distinguishable. */
static void populate(BeepbackApp* app) {
    bb_app_init(app);
    app->set.volume = 4;
    app->set.assist = BbAssistArrows;
    app->set.diff = 3;
    app->set.speed = 0;
    app->set.tutorial_done = true;
    for(uint8_t m = 0; m < BB_LADDER_MODES; m++)
        for(uint8_t d = 0; d < BB_DIFF_COUNT; d++)
            for(uint8_t s = 0; s < BB_SPEED_COUNT; s++)
                for(uint8_t a = 0; a < BB_ASSIST_COUNT; a++)
                    app->rec.best[m][d][s][a] = 1000000u + m * 10000u + d * 1000u + s * 100u + a;
    for(uint8_t r = 0; r < BB_RULE_COUNT; r++)
        for(uint8_t a = 0; a < BB_ASSIST_COUNT; a++) app->rec.ch_best[r][a] = 50000u + r * 10u + a;
    for(uint8_t a = 0; a < BB_ASSIST_COUNT; a++) app->rec.daily_best[a] = 700u + a;
    app->rec.daily_date = 20260910u;
    app->rec.daily_done = true;
}

int main(void) {
    char msg[200];
    BeepbackApp a, b;
    uint8_t buf[BB_SAVE_BYTES + 8];

    /* ---- everything goes out and comes back ---- */
    populate(&a);
    size_t n = bb_save_pack(&a, buf, sizeof(buf));
    sprintf(msg, "%u bytes", (unsigned)n);
    check("the block is the size the header says", n == BB_SAVE_BYTES, msg);

    bb_app_init(&b);
    check("and reads back", bb_save_unpack(&b, buf, n), "");
    check("with the settings intact",
          memcmp(&a.set, &b.set, sizeof(BbSettings)) == 0, "");
    check("and every record store intact",
          memcmp(&a.rec, &b.rec, sizeof(BbRecords)) == 0, "");
    check("the daily date came with it", b.rec.daily_date == 20260910u, "");
    check("and its spent flag", b.rec.daily_done, "");

    /* the three stores are really three: no aliasing between them */
    check("ladder records survive", b.rec.best[2][3][1][2] == 1000000u + 20000u + 3000u + 100u + 2, "");
    check("challenge records survive", b.rec.ch_best[6][3] == 50000u + 63u, "");
    check("daily records survive", b.rec.daily_best[3] == 703u, "");

    /* ---- a file that is not ours is left alone ---- */
    populate(&a);
    bb_save_pack(&a, buf, sizeof(buf));

    {
        uint8_t bad[BB_SAVE_BYTES];
        memcpy(bad, buf, BB_SAVE_BYTES);
        bad[0] = 'X';
        bb_app_init(&b);
        BeepbackApp fresh = b;
        check("a file with the wrong magic is refused", !bb_save_unpack(&b, bad, BB_SAVE_BYTES), "");
        check("and nothing of it is kept", memcmp(&fresh.rec, &b.rec, sizeof(BbRecords)) == 0, "");
    }

    /* a v3 file is the case that matters: same shape, different meaning */
    {
        uint8_t old[BB_SAVE_BYTES];
        memcpy(old, buf, BB_SAVE_BYTES);
        old[4] = 3;
        bb_app_init(&b);
        b.set.volume = 2;
        check("a version 3 file is discarded, not misread",
              !bb_save_unpack(&b, old, BB_SAVE_BYTES), "");
        check("so the defaults stand", b.set.volume == 2 && b.rec.best[0][0][0][0] == 0, "");
    }

    {
        bb_app_init(&b);
        check("a short file is refused", !bb_save_unpack(&b, buf, BB_SAVE_BYTES - 1), "");
        check("a long one too", !bb_save_unpack(&b, buf, BB_SAVE_BYTES + 1), "");
        check("and an empty one", !bb_save_unpack(&b, buf, 0), "");
    }

    /* ---- a single flipped bit anywhere is caught ---- */
    {
        int missed = 0;
        for(size_t i = 0; i < BB_SAVE_BYTES; i++) {
            uint8_t corrupt[BB_SAVE_BYTES];
            memcpy(corrupt, buf, BB_SAVE_BYTES);
            corrupt[i] ^= 0x40u;
            bb_app_init(&b);
            if(bb_save_unpack(&b, corrupt, BB_SAVE_BYTES)) missed++;
        }
        sprintf(msg, "%d of %u bytes slipped through", missed, BB_SAVE_BYTES);
        check("a bit flipped anywhere in the file is caught", missed == 0, msg);
    }

    /* ---- a file that passes its checksum can still be nonsense ---- */
    {
        BeepbackApp weird;
        bb_app_init(&weird);
        weird.set.volume = 200;
        weird.set.assist = 90;
        weird.set.diff = 40;
        weird.set.speed = 7;
        uint8_t odd[BB_SAVE_BYTES];
        bb_save_pack(&weird, odd, sizeof(odd));
        bb_app_init(&b);
        check("an out-of-range setting still loads", bb_save_unpack(&b, odd, BB_SAVE_BYTES), "");
        sprintf(msg, "vol %u assist %u time %u speed %u", b.set.volume, b.set.assist, b.set.diff, b.set.speed);
        check("but is clamped to something this build has",
              b.set.volume < BB_VOL_COUNT && b.set.assist < BB_ASSIST_COUNT &&
                  b.set.diff < BB_DIFF_COUNT && b.set.speed < BB_SPEED_COUNT, msg);
    }

    /* ---- the version really is bumped past v3 ---- */
    check("the save version is past the one v3 wrote", BB_SAVE_VERSION > 3, "");
    populate(&a);
    bb_save_pack(&a, buf, sizeof(buf));
    check("and the file says so", buf[4] == BB_SAVE_VERSION, "");

    /* ---- a run's record survives a save and load ---- */
    bb_app_init(&a);
    a.seed = 5;
    a.set.assist = BbAssistLed;
    bb_run_start(&a, BbModeChallenge);
    a.run.score = 8888;
    bb_run_end(&a, false);
    bb_save_pack(&a, buf, sizeof(buf));
    bb_app_init(&b);
    bb_save_unpack(&b, buf, BB_SAVE_BYTES);
    check("a challenge best written by a run reads back",
          b.rec.ch_best[a.run.rule][BbAssistLed] == 8888, "");

    printf(fails ? "\n%d FAILURES\n" : "\nall save file checks passed\n", fails);
    return fails ? 1 : 0;
}
