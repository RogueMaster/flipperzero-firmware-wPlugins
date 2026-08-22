#pragma once

#include <furi.h>
#include "chord_db.h"

#define SONG_NAME_LEN   22
#define SONG_MAX_CHORDS 8

#define SONG_DB_PATH CHORD_DB_DIR "/songs.csv"

#define SONG_BPM_MIN         40
#define SONG_BPM_MAX         200
#define SONG_BPM_STEP        5
#define SONG_BEATS_PER_CHORD 4

typedef struct {
    char name[SONG_NAME_LEN]; // "I-V-vi-IV"
    char chords[SONG_MAX_CHORDS][CHORD_NAME_LEN]; // "G", "D", "Em", "C"
    uint8_t count;
    uint16_t bpm;
} Song;

typedef struct {
    Song* items;
    size_t count;
    size_t capacity;
} SongDb;

SongDb* song_db_alloc(void);
void song_db_free(SongDb* db);

/** Wipe and refill with the compiled-in progressions. */
void song_db_load_builtin(SongDb* db);

/** Wipe and refill from a pipe-delimited CSV. Returns false if unreadable/empty. */
bool song_db_load_csv(SongDb* db, const char* path);

/** Write the builtin progressions to `path` so the user has something to edit. */
bool song_db_write_default_csv(const char* path);
