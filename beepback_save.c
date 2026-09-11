/*
 * BEEPBACK - the save file.
 *
 * Settings, all three record stores and the daily's date and flag,
 * written as a flat little-endian block rather than a struct dump, so
 * the layout is the same whatever the compiler decides about padding
 * and the browser build can read it if it ever needs to.
 *
 * The version byte is what protects an old file: v3 kept a different
 * record layout, and a v3 file read as a v4 one would look like a set
 * of very good scores nobody earned. Anything that is not this exact
 * version, length and checksum is discarded and the defaults stand.
 */
#include "beepback.h"

/* ------------------------------------------------------------------ */
/* A flat block, written by hand                                       */
/* ------------------------------------------------------------------ */

static void bb_put32(uint8_t* buf, size_t* at, uint32_t v) {
    buf[(*at)++] = (uint8_t)(v & 0xFFu);
    buf[(*at)++] = (uint8_t)((v >> 8) & 0xFFu);
    buf[(*at)++] = (uint8_t)((v >> 16) & 0xFFu);
    buf[(*at)++] = (uint8_t)((v >> 24) & 0xFFu);
}

static uint32_t bb_get32(const uint8_t* buf, size_t* at) {
    uint32_t v = (uint32_t)buf[*at] | ((uint32_t)buf[*at + 1] << 8) |
                 ((uint32_t)buf[*at + 2] << 16) | ((uint32_t)buf[*at + 3] << 24);
    *at += 4;
    return v;
}

/* Enough to notice a truncated or half-written file. Not a hash. */
static uint32_t bb_checksum(const uint8_t* buf, size_t n) {
    uint32_t sum = 0x9E3779B9u;
    for(size_t i = 0; i < n; i++) sum = (sum << 3) ^ (sum >> 29) ^ buf[i];
    return sum;
}

size_t bb_save_pack(const BeepbackApp* app, uint8_t* buf, size_t n) {
    if(n < BB_SAVE_BYTES) return 0;
    memset(buf, 0, BB_SAVE_BYTES);
    size_t at = 0;

    buf[at++] = 'B';
    buf[at++] = 'B';
    buf[at++] = 'K';
    buf[at++] = '!';
    buf[at++] = BB_SAVE_VERSION;

    buf[at++] = app->set.volume;
    buf[at++] = app->set.assist;
    buf[at++] = app->set.diff;
    buf[at++] = app->set.speed;
    buf[at++] = app->set.tutorial_done ? 1u : 0u;

    for(uint8_t m = 0; m < BB_LADDER_MODES; m++)
        for(uint8_t d = 0; d < BB_DIFF_COUNT; d++)
            for(uint8_t s = 0; s < BB_SPEED_COUNT; s++)
                for(uint8_t a = 0; a < BB_ASSIST_COUNT; a++)
                    bb_put32(buf, &at, app->rec.best[m][d][s][a]);

    for(uint8_t r = 0; r < BB_RULE_COUNT; r++)
        for(uint8_t a = 0; a < BB_ASSIST_COUNT; a++) bb_put32(buf, &at, app->rec.ch_best[r][a]);

    for(uint8_t a = 0; a < BB_ASSIST_COUNT; a++) bb_put32(buf, &at, app->rec.daily_best[a]);
    bb_put32(buf, &at, app->rec.daily_date);
    buf[at++] = app->rec.daily_done ? 1u : 0u;

    bb_put32(buf, &at, bb_checksum(buf, at));
    return at;
}

bool bb_save_unpack(BeepbackApp* app, const uint8_t* buf, size_t n) {
    if(n != BB_SAVE_BYTES) return false;
    if(buf[0] != 'B' || buf[1] != 'B' || buf[2] != 'K' || buf[3] != '!') return false;
    if(buf[4] != BB_SAVE_VERSION) return false; /* an older file is not ours */
    if(bb_checksum(buf, BB_SAVE_BYTES - 4) !=
       (uint32_t)((uint32_t)buf[BB_SAVE_BYTES - 4] | ((uint32_t)buf[BB_SAVE_BYTES - 3] << 8) |
                  ((uint32_t)buf[BB_SAVE_BYTES - 2] << 16) |
                  ((uint32_t)buf[BB_SAVE_BYTES - 1] << 24)))
        return false;

    size_t at = 5;
    BbSettings set;
    set.volume = buf[at++];
    set.assist = buf[at++];
    set.diff = buf[at++];
    set.speed = buf[at++];
    set.tutorial_done = buf[at++] != 0;

    /* A file can pass its checksum and still hold a value this build has
       no room for, so every setting is clamped on the way in. */
    if(set.volume >= BB_VOL_COUNT) set.volume = BB_VOL_COUNT - 1;
    if(set.assist >= BB_ASSIST_COUNT) set.assist = BbAssistShapes;
    if(set.diff >= BB_DIFF_COUNT) set.diff = 1;
    if(set.speed >= BB_SPEED_COUNT) set.speed = 1;
    app->set = set;
    /* the browser's firstRun is this flag, the other way up */
    app->first_run = !set.tutorial_done;

    for(uint8_t m = 0; m < BB_LADDER_MODES; m++)
        for(uint8_t d = 0; d < BB_DIFF_COUNT; d++)
            for(uint8_t s = 0; s < BB_SPEED_COUNT; s++)
                for(uint8_t a = 0; a < BB_ASSIST_COUNT; a++)
                    app->rec.best[m][d][s][a] = bb_get32(buf, &at);

    for(uint8_t r = 0; r < BB_RULE_COUNT; r++)
        for(uint8_t a = 0; a < BB_ASSIST_COUNT; a++) app->rec.ch_best[r][a] = bb_get32(buf, &at);

    for(uint8_t a = 0; a < BB_ASSIST_COUNT; a++) app->rec.daily_best[a] = bb_get32(buf, &at);
    app->rec.daily_date = bb_get32(buf, &at);
    app->rec.daily_done = buf[at++] != 0;
    return true;
}

/* ------------------------------------------------------------------ */
/* Storage                                                             */
/* ------------------------------------------------------------------ */

#ifndef BB_HOST_TEST

void bb_save_load(BeepbackApp* app) {
    /* 723 bytes is most of a 4K stack, so the block goes on the heap */
    uint8_t* buf = malloc(BB_SAVE_BYTES);
    if(!buf) return;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, BB_SAVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t got = storage_file_read(file, buf, BB_SAVE_BYTES);
        storage_file_close(file);
        /* anything that is not exactly ours leaves the defaults standing,
           a short read included */
        bb_save_unpack(app, buf, got);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    free(buf);
    /* the daily belongs to a date, and this may be a different one */
    bb_daily_refresh(app, bb_today_seed());
}

void bb_save_store(BeepbackApp* app) {
    uint8_t* buf = malloc(BB_SAVE_BYTES);
    if(!buf) return;
    size_t n = bb_save_pack(app, buf, BB_SAVE_BYTES);
    if(n == 0) {
        free(buf);
        return;
    }
    Storage* storage = furi_record_open(RECORD_STORAGE);
    /* storage_common_mkdir answers with an FS_Error, and FSE_OK is zero,
       so this is a comparison and never a truthiness test. A directory
       that is already there is the normal case, not a failure. */
    FS_Error made = storage_common_mkdir(storage, BB_SAVE_DIR);
    if(made == FSE_OK || made == FSE_EXIST) {
        File* file = storage_file_alloc(storage);
        if(storage_file_open(file, BB_SAVE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            /* a short write leaves a file the checksum will reject, which
               is the right outcome: last session's records beat half of
               this session's */
            storage_file_write(file, buf, n);
            storage_file_close(file);
        }
        storage_file_free(file);
    }
    furi_record_close(RECORD_STORAGE);
    free(buf);
}

#endif
