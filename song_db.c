#include "song_db.h"

#include <storage/storage.h>
#include <toolbox/stream/stream.h>
#include <toolbox/stream/file_stream.h>

#define TAG "SongDb"

/* Compact source form: name | comma-separated chord names | bpm
 * Every chord name here must exist in the chord library, or the practice
 * screen shows "?" for that step. */
static const char* const BUILTIN[] = {
    // two chords - drilling a single transition
    "Em - G|Em,G|80",
    "Am - F|Am,F|75",
    "G - D|G,D|85",
    "C - F|C,F|85",
    "Am - Dm|Am,Dm|75",
    // three chords
    "I-IV-V|G,C,D|90",
    "ii-V-I|Dm7,G7,Cmaj7|75",
    "12-Bar Blues|A7,D7,E7|90",
    "Folk Three|C,Am,G|85",
    // four chords
    "I-V-vi-IV|G,D,Em,C|85",
    "50s Doo-Wop|C,Am,F,G|80",
    "Pop Punk|C,G,Am,F|100",
    "Andalusian|Am,G,F,E|75",
    "Minor Fall|Am,F,C,G|80",
};

/* ------------------------------------------------------------------ */
/* Storage                                                             */
/* ------------------------------------------------------------------ */

SongDb* song_db_alloc(void) {
    SongDb* db = malloc(sizeof(SongDb));
    db->items = NULL;
    db->count = 0;
    db->capacity = 0;
    return db;
}

void song_db_free(SongDb* db) {
    if(!db) return;
    if(db->items) free(db->items);
    free(db);
}

static void song_db_reset(SongDb* db) {
    db->count = 0;
}

static void song_db_grow(SongDb* db) {
    size_t new_cap = db->capacity ? db->capacity * 2 : 16;
    Song* items = malloc(sizeof(Song) * new_cap);
    if(db->items) {
        memcpy(items, db->items, sizeof(Song) * db->count);
        free(db->items);
    }
    db->items = items;
    db->capacity = new_cap;
}

static void song_db_push(SongDb* db, const Song* s) {
    if(db->count == db->capacity) song_db_grow(db);
    db->items[db->count++] = *s;
}

/* ------------------------------------------------------------------ */
/* Parsing                                                             */
/* ------------------------------------------------------------------ */

/* Split "A|B|C" in place. Returns number of fields found (max `max`). */
static size_t split_pipe(char* line, char** fields, size_t max) {
    size_t n = 0;
    char* p = line;
    fields[n++] = p;
    while(*p && n < max) {
        if(*p == '|') {
            *p = '\0';
            fields[n++] = p + 1;
        }
        p++;
    }
    return n;
}

/** Parse "NAME|C,G,Am,F|BPM". BPM may be omitted. */
static bool song_parse(const char* record, Song* out) {
    char buf[128];
    strncpy(buf, record, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char* f[3] = {0};
    size_t n = split_pipe(buf, f, 3);
    if(n < 2) return false;

    memset(out, 0, sizeof(Song));
    strncpy(out->name, f[0], SONG_NAME_LEN - 1);

    /* chord list */
    char* p = f[1];
    while(*p && out->count < SONG_MAX_CHORDS) {
        while(*p == ' ')
            p++;
        char* start = p;
        while(*p && *p != ',')
            p++;
        size_t len = (size_t)(p - start);
        while(len > 0 && start[len - 1] == ' ')
            len--; // trim trailing space
        if(len > 0) {
            if(len > CHORD_NAME_LEN - 1) len = CHORD_NAME_LEN - 1;
            memcpy(out->chords[out->count], start, len);
            out->chords[out->count][len] = '\0';
            out->count++;
        }
        if(*p == ',') p++;
    }
    if(out->count < 2) return false;

    out->bpm = 80;
    if(n >= 3 && f[2] && f[2][0]) {
        int b = atoi(f[2]);
        if(b >= SONG_BPM_MIN && b <= SONG_BPM_MAX) out->bpm = (uint16_t)b;
    }
    return true;
}

/* ------------------------------------------------------------------ */
/* Loading                                                             */
/* ------------------------------------------------------------------ */

void song_db_load_builtin(SongDb* db) {
    song_db_reset(db);
    size_t total = sizeof(BUILTIN) / sizeof(BUILTIN[0]);
    for(size_t i = 0; i < total; i++) {
        Song s;
        if(song_parse(BUILTIN[i], &s)) song_db_push(db, &s);
    }
}

bool song_db_load_csv(SongDb* db, const char* path) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = file_stream_alloc(storage);
    bool ok = false;

    if(file_stream_open(stream, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        song_db_reset(db);
        FuriString* line = furi_string_alloc();
        while(stream_read_line(stream, line)) {
            furi_string_trim(line);
            if(furi_string_empty(line)) continue;
            if(furi_string_get_char(line, 0) == '#') continue;

            Song s;
            if(song_parse(furi_string_get_cstr(line), &s)) {
                song_db_push(db, &s);
            } else {
                FURI_LOG_W(TAG, "bad line: %s", furi_string_get_cstr(line));
            }
        }
        furi_string_free(line);
        ok = db->count > 0;
    }

    file_stream_close(stream);
    stream_free(stream);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool song_db_write_default_csv(const char* path) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = file_stream_alloc(storage);
    bool ok = false;

    if(file_stream_open(stream, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        stream_write_cstring(stream, "# Guitar Chords practice progressions\n");
        stream_write_cstring(stream, "# NAME|CHORDS|BPM\n");
        stream_write_cstring(stream, "# CHORDS: 2-8 names separated by commas.\n");
        stream_write_cstring(stream, "#         Each must match a NAME in chords.csv.\n");
        stream_write_cstring(stream, "# BPM:    40-200. Optional, defaults to 80.\n");
        stream_write_cstring(stream, "#         Each chord lasts one bar (4 beats).\n");
        stream_write_cstring(stream, "# Lines starting with # are ignored.\n\n");
        size_t total = sizeof(BUILTIN) / sizeof(BUILTIN[0]);
        for(size_t i = 0; i < total; i++) {
            stream_write_cstring(stream, BUILTIN[i]);
            stream_write_char(stream, '\n');
        }
        ok = true;
    }

    file_stream_close(stream);
    stream_free(stream);
    furi_record_close(RECORD_STORAGE);
    return ok;
}
