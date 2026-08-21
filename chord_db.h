#pragma once

#include <furi.h>
#include <storage/storage.h>

#define CHORD_STRINGS     6
#define CHORD_NAME_LEN    14
#define CHORD_ROOT_LEN    3
#define CHORD_QUALITY_LEN 12

#define CHORD_FRET_MUTED (-1)
#define CHORD_FRET_OPEN  (0)

#define CHORD_DB_DIR  EXT_PATH("apps_data/guitar_chords")
#define CHORD_DB_PATH CHORD_DB_DIR "/chords.csv"

typedef struct {
    char name[CHORD_NAME_LEN]; // "Am7"
    char root[CHORD_ROOT_LEN]; // "A", "Bb", "F#"
    char quality[CHORD_QUALITY_LEN]; // "", "m", "7", "sus4"
    int8_t frets[CHORD_STRINGS]; // low E -> high e; -1 muted, 0 open, else fret offset
    uint8_t base_fret; // 1 == at the nut
    int8_t fingers[CHORD_STRINGS]; // 0 = none, 1..4
} Chord;

typedef struct {
    Chord* items;
    size_t count;
    size_t capacity;
} ChordDb;

ChordDb* chord_db_alloc(void);
void chord_db_free(ChordDb* db);

/** Wipe and refill with the compiled-in starter set. */
void chord_db_load_builtin(ChordDb* db);

/** Wipe and refill from a pipe-delimited CSV. Returns false if unreadable/empty. */
bool chord_db_load_csv(ChordDb* db, const char* path);

/** Write the builtin set to `path` so the user has something to edit. */
bool chord_db_write_default_csv(const char* path);

/** Unique roots, in chromatic order. Returns count; fills `out` with pointers into a static table. */
size_t chord_db_roots(ChordDb* db, const char** out, size_t out_max);

/** Distinct chord names sharing `root`, first-seen order. Returns count. */
size_t chord_db_names_for_root(ChordDb* db, const char* root, const char** out, size_t out_max);

/** Indices of every voicing whose name matches `name`. Returns count. */
size_t chord_db_voicings(ChordDb* db, const char* name, size_t* out, size_t out_max);
