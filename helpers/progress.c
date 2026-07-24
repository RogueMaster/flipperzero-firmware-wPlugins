#include "progress.h"

#include <furi.h>
#include <storage/storage.h>

#define PROGRESS_DIR     EXT_PATH("apps_data/ear_trainer")
#define PROGRESS_PATH    PROGRESS_DIR "/progress.bin"
#define SETTINGS_PATH    PROGRESS_DIR "/settings.bin"
#define PROGRESS_MAGIC   0x5445 /* "ET" */
#define PROGRESS_VERSION 2
#define SETTINGS_MAGIC   0x5345 /* "ES" */
#define SETTINGS_VERSION 1

/* v1 shipped with three modes (the interval directions) before chords and
 * scales existed. Its stars array has a different row length, so a v1 file
 * cannot be read as a prefix of the current struct - it gets copied across
 * field by field instead. */
#define V1_MODE_COUNT 3
typedef struct __attribute__((packed)) {
    uint8_t unlocked[V1_MODE_COUNT];
    uint8_t stars[V1_MODE_COUNT][LEVEL_COUNT_MAX];
    uint32_t answered;
    uint32_t correct;
    uint16_t best_streak;
} EarProgressV1;

typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint8_t version;
} FileHeader;

static void progress_defaults(EarProgress* progress) {
    memset(progress, 0, sizeof(EarProgress));
    for(uint8_t m = 0; m < MODE_COUNT; m++)
        progress->unlocked[m] = 1;
}

static bool progress_valid(const EarProgress* progress) {
    for(uint8_t m = 0; m < MODE_COUNT; m++) {
        if(progress->unlocked[m] < 1 || progress->unlocked[m] > LEVEL_COUNT_MAX) return false;
        for(uint8_t l = 0; l < LEVEL_COUNT_MAX; l++) {
            if(progress->stars[m][l] > 3) return false;
        }
    }
    return progress->correct <= progress->answered;
}

/* Carry a v1 file forward: the three interval modes keep their unlocked level
 * and stars, the new chord and scale modes start fresh, and the lifetime
 * counters survive. */
static void migrate_v1(const EarProgressV1* old, EarProgress* out) {
    progress_defaults(out);
    for(uint8_t m = 0; m < V1_MODE_COUNT; m++) {
        out->unlocked[m] = old->unlocked[m];
        memcpy(out->stars[m], old->stars[m], LEVEL_COUNT_MAX);
    }
    out->answered = old->answered;
    out->correct = old->correct;
    out->best_streak = old->best_streak;
}

void ear_progress_load(EarProgress* progress) {
    progress_defaults(progress);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, PROGRESS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FileHeader header;
        EarProgress loaded;
        memset(&loaded, 0, sizeof(loaded));
        bool ok = false;
        /* Bad magic/version/values means start fresh rather than crash. */
        if(storage_file_read(file, &header, sizeof(header)) == sizeof(header) &&
           header.magic == PROGRESS_MAGIC) {
            if(header.version == PROGRESS_VERSION) {
                ok = storage_file_read(file, &loaded, sizeof(loaded)) == (uint16_t)sizeof(loaded);
            } else if(header.version == 1) {
                EarProgressV1 old;
                if(storage_file_read(file, &old, sizeof(old)) == (uint16_t)sizeof(old)) {
                    migrate_v1(&old, &loaded);
                    ok = true;
                }
            }
        }
        if(ok && progress_valid(&loaded)) *progress = loaded;
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void ear_progress_save(const EarProgress* progress) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, PROGRESS_DIR); /* fine if it already exists */
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, PROGRESS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FileHeader header = {.magic = PROGRESS_MAGIC, .version = PROGRESS_VERSION};
        storage_file_write(file, &header, sizeof(header));
        storage_file_write(file, progress, sizeof(EarProgress));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void ear_settings_load(EarSettings* settings) {
    settings->note_ms = 1; /* medium */
    settings->random_root = 1; /* relative pitch is the point, so vary the root */
    settings->vibro = 1;
    settings->led = 1;
    settings->show_mnemonic = 1;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FileHeader header;
        EarSettings loaded = *settings;
        bool ok = false;
        if(storage_file_read(file, &header, sizeof(header)) == sizeof(header) &&
           header.magic == SETTINGS_MAGIC && header.version == SETTINGS_VERSION) {
            ok = storage_file_read(file, &loaded, sizeof(loaded)) == (uint16_t)sizeof(loaded);
        }
        if(ok && loaded.note_ms <= 2 && loaded.random_root <= 1 && loaded.vibro <= 1 &&
           loaded.led <= 1 && loaded.show_mnemonic <= 1) {
            *settings = loaded;
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void ear_settings_save(const EarSettings* settings) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, PROGRESS_DIR);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FileHeader header = {.magic = SETTINGS_MAGIC, .version = SETTINGS_VERSION};
        storage_file_write(file, &header, sizeof(header));
        storage_file_write(file, settings, sizeof(EarSettings));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

uint16_t ear_note_duration_ms(uint8_t note_ms_index) {
    static const uint16_t durations[3] = {350, 550, 800};
    return durations[note_ms_index <= 2 ? note_ms_index : 1];
}
