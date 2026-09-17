/*
 * A save file with records already in it, for screenshots.
 *
 * The records tables and the stats pages are the screens worth showing
 * and the ones that look broken empty: a grid of dashes says the feature
 * is missing rather than unused. This writes a plausible save so those
 * screens can be photographed off a real device without playing for an
 * hour first.
 *
 * It packs through bb_save_pack(), the same function the firmware writes
 * with, so the version byte, the field order and the checksum cannot
 * drift from what the app will accept.
 */
#define BB_HOST_TEST 1
#include <stdio.h>
#include <string.h>
#include "beepback.h"
#include "beepback_save.c"

int main(int argc, char** argv) {
    const char* out = argc > 1 ? argv[1] : "beepback.save";
    BeepbackApp app;
    memset(&app, 0, sizeof(app));

    /* what the game opens on, so nothing has to be set up by hand */
    app.set.volume = 2; /* MID */
    app.set.assist = 2; /* SHAPES */
    app.set.speed = 1; /* NORMAL */
    app.set.diff = 1; /* NORMAL */
    app.set.haptic = true;
    app.set.tutorial_done = true; /* or the first launch opens the guide */

    /* CLASSIC on shapes, the table the screenshot is of. A spread rather
       than a full grid: the dashes are worth showing too, because that is
       what an unplayed setting looks like. */
    static const uint32_t classic[BB_DIFF_COUNT][BB_SPEED_COUNT] = {
        {1240, 1580, 0},
        {960, 2210, 430},
        {0, 1120, 0},
        {0, 0, 0},
    };
    for(uint8_t t = 0; t < BB_DIFF_COUNT; t++)
        for(uint8_t s = 0; s < BB_SPEED_COUNT; s++)
            app.rec.best[BbModeClassic][t][s][BbAssistShapes] = classic[t][s];

    /* enough elsewhere that the other tables are not bare */
    app.rec.best[BbModeClassic][1][1][BbAssistLed] = 880;
    app.rec.best[BbModeClassic][1][1][BbAssistOff] = 340;
    app.rec.best[BbModeRules][1][1][BbAssistShapes] = 1460;
    app.rec.best[BbModeRules][2][1][BbAssistShapes] = 720;
    app.rec.best[BbModeRules][1][0][BbAssistShapes] = 980;
    app.rec.best[BbModeReflex][1][1][BbAssistShapes] = 310;
    app.rec.best[BbModeReflex][2][2][BbAssistShapes] = 180;

    app.rec.ch_best[BbRuleSkip][BbAssistShapes] = 640;
    app.rec.ch_best[BbRuleDouble][BbAssistShapes] = 410;
    app.rec.ch_best[BbRuleNoDoubles][BbAssistShapes] = 890;
    app.rec.ch_best[BbRuleEveryOther][BbAssistShapes] = 1120;
    app.rec.ch_best[BbRuleBackwards][BbAssistShapes] = 350;

    /* the daily belongs to a date, and any date but today is cleared on
       load - so leave it alone and let the day it is set it */
    app.rec.daily_date = 0;
    app.rec.daily_done = false;

    app.stats.runs = 47;
    app.stats.play_ms = 4520000u; /* 1H 15M */
    app.stats.rounds = 96;
    app.stats.notes = 1284;
    app.stats.best_ever = 2210;
    app.stats.longest = 11;
    app.stats.by_mode[BbModeClassic] = 21;
    app.stats.by_mode[BbModeRules] = 13;
    app.stats.by_mode[BbModeReflex] = 7;
    app.stats.by_mode[BbModeChallenge] = 4;
    app.stats.by_mode[BbModeDaily] = 2;
    app.stats.by_assist[BbAssistShapes] = 33;
    app.stats.by_assist[BbAssistLed] = 9;
    app.stats.by_assist[BbAssistOff] = 3;
    app.stats.by_assist[BbAssistArrows] = 2;

    uint8_t buf[BB_SAVE_BYTES];
    size_t n = bb_save_pack(&app, buf, sizeof(buf));
    if(n != BB_SAVE_BYTES) {
        fprintf(stderr, "packed %zu bytes, expected %d\n", n, BB_SAVE_BYTES);
        return 1;
    }

    /* read it straight back, so a file that the app would reject never
       leaves this program */
    BeepbackApp check;
    memset(&check, 0, sizeof(check));
    if(!bb_save_unpack(&check, buf, n)) {
        fprintf(stderr, "the file this wrote does not load\n");
        return 1;
    }
    if(check.rec.best[BbModeClassic][1][1][BbAssistShapes] != 2210 || check.stats.runs != 47 ||
       check.set.assist != 2) {
        fprintf(stderr, "it loads, but not as what was written\n");
        return 1;
    }

    FILE* f = fopen(out, "wb");
    if(!f) {
        perror(out);
        return 1;
    }
    fwrite(buf, 1, n, f);
    fclose(f);
    printf("%s: %zu bytes, save version %d, reads back clean\n", out, n, BB_SAVE_VERSION);
    printf("copy to the Flipper at %s\n", "/ext/apps_data/beepback/beepback.save");
    return 0;
}
