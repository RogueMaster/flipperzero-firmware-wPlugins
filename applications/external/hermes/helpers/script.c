#include "script.h"

#include <storage/storage.h>
#include <string.h>

struct Script {
    char buf[SCRIPT_MAX_BYTES + 1]; // lines carved out of this, NUL-separated
    const char* lines[SCRIPT_MAX_LINES];
    uint16_t count;

    char name[40];

    bool running;
    uint16_t pos;
    uint32_t next_tick;
};

Script* script_alloc(void) {
    Script* sc = malloc(sizeof(Script));
    memset(sc, 0, sizeof(Script));
    return sc;
}

void script_free(Script* sc) {
    furi_assert(sc);
    free(sc);
}

void script_clear(Script* sc) {
    furi_assert(sc);
    sc->count = 0;
    sc->running = false;
    sc->pos = 0;
    sc->name[0] = '\0';
}

/** Carve the loaded buffer into lines in place. */
static void script_split(Script* sc, size_t len) {
    sc->count = 0;
    size_t i = 0;

    while(i < len && sc->count < SCRIPT_MAX_LINES) {
        /* skip any leading line breaks */
        while(i < len && (sc->buf[i] == '\r' || sc->buf[i] == '\n'))
            i++;
        if(i >= len) break;

        char* start = &sc->buf[i];
        while(i < len && sc->buf[i] != '\r' && sc->buf[i] != '\n')
            i++;
        sc->buf[i] = '\0'; // safe: buf has one spare byte past len
        i++;

        /* '#' makes a script self-documenting, which matters when you find one
         * on the card six months later. */
        if(start[0] == '\0' || start[0] == '#') continue;
        sc->lines[sc->count++] = start;
    }
}

bool script_load(Script* sc, const char* path) {
    furi_assert(sc);
    furi_assert(path);
    script_clear(sc);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool ok = false;

    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        const size_t read = storage_file_read(file, sc->buf, SCRIPT_MAX_BYTES);
        if(read > 0) {
            sc->buf[read] = '\0';
            script_split(sc, read);
            ok = sc->count > 0;
        }
        storage_file_close(file);
    }

    if(ok) {
        /* Show just the filename; the full path does not fit and the directory
         * is always the same anyway. */
        const char* slash = strrchr(path, '/');
        const char* base = slash ? slash + 1 : path;
        strncpy(sc->name, base, sizeof(sc->name) - 1);
        sc->name[sizeof(sc->name) - 1] = '\0';
    } else {
        script_clear(sc);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool script_is_loaded(const Script* sc) {
    furi_assert(sc);
    return sc->count > 0;
}

uint16_t script_line_count(const Script* sc) {
    furi_assert(sc);
    return sc->count;
}

const char* script_name(const Script* sc) {
    furi_assert(sc);
    return sc->name;
}

/* ----------------------------------------------------------- playback ----- */

void script_start(Script* sc) {
    furi_assert(sc);
    if(sc->count == 0) return;
    sc->running = true;
    sc->pos = 0;
    sc->next_tick = furi_get_tick(); // first line goes immediately
}

void script_stop(Script* sc) {
    furi_assert(sc);
    sc->running = false;
}

bool script_is_running(const Script* sc) {
    furi_assert(sc);
    return sc->running;
}

uint16_t script_position(const Script* sc) {
    furi_assert(sc);
    return sc->pos;
}

const char* script_next_line(Script* sc) {
    furi_assert(sc);
    if(!sc->running) return NULL;

    if(sc->pos >= sc->count) {
        sc->running = false;
        return NULL;
    }
    if(furi_get_tick() < sc->next_tick) return NULL; // not yet

    const char* line = sc->lines[sc->pos++];
    sc->next_tick = furi_get_tick() + furi_ms_to_ticks(SCRIPT_LINE_GAP_MS);
    return line;
}
