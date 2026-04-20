#include "notes_cache.h"

#include <furi.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define NOTES_CACHE_FILE "/ext/apps_data/osm_logger/notes_cache.txt"
#define NOTES_CACHE_MAX  4096

// Format : une ligne par preset, séparée par '\t' :
//   amenity=bench<TAB>wooden bench
//   amenity=drinking_water<TAB>public fountain

void notes_cache_load(const char* primary_tag, char* out, size_t out_size) {
    if(out_size == 0) return;
    out[0] = '\0';
    if(!primary_tag || !*primary_tag) return;

    Storage* s = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(s);
    if(f && storage_file_open(f, NOTES_CACHE_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t sz = storage_file_size(f);
        if(sz > 0 && sz <= NOTES_CACHE_MAX) {
            char* buf = malloc((size_t)sz + 1);
            if(buf) {
                uint16_t read = storage_file_read(f, buf, (uint16_t)sz);
                buf[read] = '\0';

                size_t pt_len = strlen(primary_tag);
                char* p = buf;
                char* end = buf + read;
                while(p < end) {
                    char* eol = memchr(p, '\n', (size_t)(end - p));
                    char* line_end = eol ? eol : end;
                    if((size_t)(line_end - p) > pt_len &&
                       !memcmp(p, primary_tag, pt_len) && p[pt_len] == '\t') {
                        char* note_start = p + pt_len + 1;
                        size_t note_len = (size_t)(line_end - note_start);
                        if(note_len >= out_size) note_len = out_size - 1;
                        memcpy(out, note_start, note_len);
                        out[note_len] = '\0';
                        break;
                    }
                    if(!eol) break;
                    p = eol + 1;
                }
                free(buf);
            }
        }
        storage_file_close(f);
    }
    if(f) storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

void notes_cache_save(const char* primary_tag, const char* note) {
    if(!primary_tag || !*primary_tag) return;
    FURI_LOG_D("OSM", "notes_cache_save: start");

    Storage* s = furi_record_open(RECORD_STORAGE);

    FileInfo fi;
    if(storage_common_stat(s, "/ext/apps_data/osm_logger", &fi) != FSE_OK) {
        storage_common_mkdir(s, "/ext/apps_data/osm_logger");
    }
    FURI_LOG_D("OSM", "notes_cache_save: dir ok");

    // Étape 1 — lecture du contenu existant (dans son propre File*)
    char* existing = NULL;
    uint32_t existing_size = 0;
    {
        File* rf = storage_file_alloc(s);
        if(rf && storage_file_open(rf, NOTES_CACHE_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
            uint64_t sz = storage_file_size(rf);
            if(sz > 0 && sz <= NOTES_CACHE_MAX) {
                existing = malloc((size_t)sz + 1);
                if(existing) {
                    existing_size = storage_file_read(rf, existing, (uint16_t)sz);
                    existing[existing_size] = '\0';
                }
            }
            storage_file_close(rf);
        }
        if(rf) storage_file_free(rf);
    }
    FURI_LOG_D("OSM", "notes_cache_save: read ok (%lu B)", (unsigned long)existing_size);

    // Étape 2 — construction du nouveau buffer (sans la ligne du primary_tag + append)
    char* new_buf = malloc(NOTES_CACHE_MAX);
    if(!new_buf) {
        if(existing) free(existing);
        furi_record_close(RECORD_STORAGE);
        FURI_LOG_E("OSM", "notes_cache_save: malloc failed");
        return;
    }
    size_t new_size = 0;

    if(existing) {
        size_t pt_len = strlen(primary_tag);
        char* p = existing;
        char* end = existing + existing_size;
        while(p < end) {
            char* eol = memchr(p, '\n', (size_t)(end - p));
            char* line_end = eol ? eol : end;
            size_t line_with_nl = (size_t)(line_end - p) + (eol ? 1 : 0);

            bool skip = ((size_t)(line_end - p) > pt_len &&
                         !memcmp(p, primary_tag, pt_len) && p[pt_len] == '\t');
            if(!skip && new_size + line_with_nl < NOTES_CACHE_MAX) {
                memcpy(new_buf + new_size, p, line_with_nl);
                new_size += line_with_nl;
            }
            if(!eol) break;
            p = eol + 1;
        }
        free(existing);
    }

    if(note && *note) {
        int n = snprintf(
            new_buf + new_size,
            NOTES_CACHE_MAX - new_size,
            "%s\t%s\n",
            primary_tag,
            note);
        if(n > 0 && (size_t)n < NOTES_CACHE_MAX - new_size) {
            new_size += (size_t)n;
        }
    }
    FURI_LOG_D("OSM", "notes_cache_save: rebuilt (%lu B)", (unsigned long)new_size);

    // Étape 3 — écriture du fichier (dans son propre File*, séparé du read)
    {
        File* wf = storage_file_alloc(s);
        if(wf && storage_file_open(wf, NOTES_CACHE_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            if(new_size > 0) storage_file_write(wf, new_buf, new_size);
            storage_file_close(wf);
            FURI_LOG_D("OSM", "notes_cache_save: write ok");
        } else {
            FURI_LOG_E("OSM", "notes_cache_save: write open FAILED");
        }
        if(wf) storage_file_free(wf);
    }
    free(new_buf);

    furi_record_close(RECORD_STORAGE);
    FURI_LOG_D("OSM", "notes_cache_save: done");
}
