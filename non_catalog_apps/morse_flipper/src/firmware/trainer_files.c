/*
 * Purpose: Load custom trainer character sets from storage.
 * Owns: custom set parsing, path override, and host/FAP file access.
 * Depends on: trainer_files.h, trainer.h, and morse_flipper_paths.h.
 * Tests: tests/test_trainer_files.c.
 */

#include "trainer.h"
#include "trainer_files.h"
#include "morse_flipper_paths.h"

#ifdef MORSE_FLIPPER_FAP
#include <storage/storage.h>
#include <furi.h>
#else
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#endif

#include <stdlib.h>
#include <string.h>

static const char* morse_trainer_custom_path_value = MORSE_FLIPPER_CUSTOM_CHARS_PATH;
static const char* morse_trainer_custom_defaults = "numbers=0123456789\n"
                                                   "dits=EISH5\n"
                                                   "more dits=EISH5AUVNDB\n";

const char* morse_trainer_custom_chars_path(void) {
    return morse_trainer_custom_path_value;
}

#ifdef MORSE_FLIPPER_FAP
static bool morse_trainer_write_custom_defaults(Storage* storage) {
    File* file = storage_file_alloc(storage);
    bool ok = false;

    if(file == NULL) {
        return false;
    }

    if(storage_file_open(file, morse_trainer_custom_path_value, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        ok = storage_file_write(
                 file, morse_trainer_custom_defaults, strlen(morse_trainer_custom_defaults)) ==
             strlen(morse_trainer_custom_defaults);
    }
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

void morse_trainer_ensure_custom_chars_file(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    if(storage == NULL) {
        return;
    }

    if(!storage_file_exists(storage, morse_trainer_custom_path_value)) {
        storage_common_mkdir(storage, MORSE_FLIPPER_APP_DATA_DIR);
        morse_trainer_write_custom_defaults(storage);
    }

    furi_record_close(RECORD_STORAGE);
}

static bool morse_trainer_read_custom_text(char* buf, size_t buf_sz, bool create_defaults) {
    Storage* storage;
    File* file;
    uint16_t got = 0U;
    bool opened;
    bool file_needs_close;

    if(buf == NULL || buf_sz == 0U) {
        return false;
    }

    storage = furi_record_open(RECORD_STORAGE);
    if(storage == NULL) {
        buf[0] = '\0';
        return false;
    }

    file = storage_file_alloc(storage);
    if(file == NULL) {
        buf[0] = '\0';
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    opened =
        storage_file_open(file, morse_trainer_custom_path_value, FSAM_READ, FSOM_OPEN_EXISTING);
    file_needs_close = true;
    if(!opened) {
        storage_file_close(file);
        file_needs_close = false;
        if(create_defaults) {
            storage_common_mkdir(storage, MORSE_FLIPPER_APP_DATA_DIR);
            morse_trainer_write_custom_defaults(storage);
            opened = storage_file_open(
                file, morse_trainer_custom_path_value, FSAM_READ, FSOM_OPEN_EXISTING);
            file_needs_close = true;
        }
    }

    if(opened) {
        got = storage_file_read(file, buf, buf_sz - 1U);
    }
    if(file_needs_close) {
        storage_file_close(file);
    }

    buf[got] = '\0';
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return got != 0U;
}
#else
void morse_trainer_ensure_custom_chars_file(void) {
    FILE* f = fopen(morse_trainer_custom_path_value, "rb");

    if(f != NULL) {
        fclose(f);
        return;
    }

    mkdir("ext", 0777);
    mkdir("ext/apps_data", 0777);
    mkdir(MORSE_FLIPPER_APP_DATA_DIR, 0777);
    f = fopen(morse_trainer_custom_path_value, "wb");
    if(f != NULL) {
        fwrite(morse_trainer_custom_defaults, 1, strlen(morse_trainer_custom_defaults), f);
        fclose(f);
    }
}

static bool morse_trainer_read_custom_text(char* buf, size_t buf_sz, bool create_defaults) {
    FILE* f;
    size_t got;

    if(buf == NULL || buf_sz == 0U) {
        return false;
    }

    f = fopen(morse_trainer_custom_path_value, "rb");
    if(f == NULL && create_defaults) {
        mkdir("ext", 0777);
        mkdir("ext/apps_data", 0777);
        mkdir(MORSE_FLIPPER_APP_DATA_DIR, 0777);
        f = fopen(morse_trainer_custom_path_value, "wb");
        if(f != NULL) {
            fwrite(morse_trainer_custom_defaults, 1, strlen(morse_trainer_custom_defaults), f);
            fclose(f);
        }
        f = fopen(morse_trainer_custom_path_value, "rb");
    }

    if(f == NULL) {
        return false;
    }

    got = fread(buf, 1, buf_sz - 1U, f);
    buf[got] = '\0';
    fclose(f);
    return got != 0U;
}
#endif

static bool
    morse_trainer_load_custom_sets_inner(MorseTrainerCustomSets* sets, bool create_defaults) {
    char* buf;
    char* line;
    char* next;
    char* eq;
    bool loaded = false;

    if(sets == NULL) {
        return false;
    }

    memset(sets, 0, sizeof(*sets));
    buf = malloc(MORSE_TRAINER_CUSTOM_TEXT_CAP);
    if(buf == NULL) {
        return false;
    }

    if(!morse_trainer_read_custom_text(buf, MORSE_TRAINER_CUSTOM_TEXT_CAP, create_defaults)) {
        free(buf);
        return false;
    }

    line = buf;
    while(line != NULL && *line != '\0' && sets->count < MORSE_TRAINER_CUSTOM_SET_CAP) {
        next = strchr(line, '\n');
        if(next != NULL) {
            *next++ = '\0';
        }

        eq = strchr(line, '=');
        if(eq != NULL) {
            *eq++ = '\0';
            if(line[0] != '\0' && eq[0] != '\0') {
                strncpy(sets->sets[sets->count].name, line, MORSE_TRAINER_CUSTOM_NAME_CAP - 1U);
                strncpy(sets->sets[sets->count].chars, eq, MORSE_TRAINER_CHARSET_CAP - 1U);
                sets->count++;
            }
        }

        line = next;
    }

    loaded = sets->count != 0U;
    free(buf);
    return loaded;
}

bool morse_trainer_load_custom_sets(MorseTrainerCustomSets* sets) {
    return morse_trainer_load_custom_sets_inner(sets, true);
}

bool morse_trainer_try_load_custom_sets(MorseTrainerCustomSets* sets) {
    return morse_trainer_load_custom_sets_inner(sets, false);
}
