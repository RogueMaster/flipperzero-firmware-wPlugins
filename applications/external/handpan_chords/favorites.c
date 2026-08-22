#include "handpan.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HP_FAV_DIR  EXT_PATH("apps_data/handpan")
#define HP_FAV_PATH HP_FAV_DIR "/favorites.txt"

/* 64 favorites * "9,11,17\n" plus the header comfortably fits. */
#define HP_FAV_BUF_SIZE 1536

static const char* const hp_fav_header = "# Handpan Chords favorites\n"
                                         "# scale,root_pc,formula\n";

/* Parse one "scale,root_pc,formula" line. Every field is validated against the
 * current table bounds so a hand-edited or truncated file can't take the app
 * down -- bad lines are simply dropped. */
static bool hp_fav_parse_line(const char* line, HpFavorite* out) {
    const char* p = line;
    while(*p == ' ' || *p == '\t')
        p++;
    if(*p == '\0' || *p == '#') return false;

    char* end = NULL;
    long values[3];

    for(size_t i = 0; i < 3; i++) {
        values[i] = strtol(p, &end, 10);
        if(end == p) return false;
        p = end;
        if(i < 2) {
            while(*p == ' ' || *p == '\t')
                p++;
            if(*p != ',') return false;
            p++;
        }
    }

    /* Trailing junk after the third field means the line is malformed. */
    while(*p == ' ' || *p == '\t' || *p == '\r')
        p++;
    if(*p != '\0') return false;

    if(values[0] < 0 || (size_t)values[0] >= hp_scale_count) return false;
    if(values[1] < 0 || values[1] >= 12) return false;
    if(values[2] < 0 || (size_t)values[2] >= hp_formula_count) return false;

    out->scale = (uint8_t)values[0];
    out->root_pc = (uint8_t)values[1];
    out->formula = (uint8_t)values[2];
    return true;
}

size_t hp_fav_load(Storage* storage, HpFavorite* out, size_t max) {
    if(!storage || !out || max == 0) return 0;

    File* file = storage_file_alloc(storage);
    size_t count = 0;
    char* buf = NULL;

    if(storage_file_open(file, HP_FAV_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        buf = malloc(HP_FAV_BUF_SIZE);
        if(buf) {
            size_t read = storage_file_read(file, buf, HP_FAV_BUF_SIZE - 1);
            buf[read] = '\0';

            char* p = buf;
            while(*p && count < max) {
                char* eol = strchr(p, '\n');
                if(eol) *eol = '\0';

                HpFavorite fav;
                if(hp_fav_parse_line(p, &fav)) {
                    out[count++] = fav;
                }

                if(!eol) break;
                p = eol + 1;
            }
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    if(buf) free(buf);

    return count;
}

bool hp_fav_save(Storage* storage, const HpFavorite* favs, size_t count) {
    if(!storage || (!favs && count > 0)) return false;
    if(count > HP_MAX_FAVORITES) count = HP_MAX_FAVORITES;

    storage_simply_mkdir(storage, HP_FAV_DIR);

    File* file = storage_file_alloc(storage);
    bool ok = false;

    if(storage_file_open(file, HP_FAV_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        size_t header_len = strlen(hp_fav_header);
        ok = storage_file_write(file, hp_fav_header, header_len) == header_len;

        for(size_t i = 0; ok && i < count; i++) {
            char line[32];
            int len = snprintf(
                line,
                sizeof(line),
                "%u,%u,%u\n",
                (unsigned)favs[i].scale,
                (unsigned)favs[i].root_pc,
                (unsigned)favs[i].formula);
            if(len <= 0 || (size_t)len >= sizeof(line)) {
                ok = false;
                break;
            }
            ok = storage_file_write(file, line, (size_t)len) == (size_t)len;
        }
    }

    storage_file_close(file);
    storage_file_free(file);

    return ok;
}
