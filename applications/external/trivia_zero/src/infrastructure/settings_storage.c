#include "include/infrastructure/settings_storage.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#ifdef __has_include
#if __has_include(<furi.h>)
#include <furi.h>
#include <storage/storage.h>
#define TZ_HAVE_FURI 1
#endif
#endif

#define APPS_DATA_DIR "/ext/apps_data/flipper_trivia_zero"
#define SETTINGS_PATH APPS_DATA_DIR "/settings"
#define SETTINGS_HEADER "# trivia v1\n"

/* ---- Pure parsing ---- */

Settings settings_default(void) {
    return (Settings){.lang = LangEn, .last_id = 0u, .last_id_valid = false};
}

static char *trim_inplace(char *s) {
    if (!s)
        return s;
    while (*s && isspace((unsigned char)*s))
        s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1]))
        end--;
    *end = '\0';
    return s;
}

bool settings_split_line(const char *line, char *key, size_t key_size, char *value,
                         size_t value_size) {
    if (!line || !key || !value || key_size == 0 || value_size == 0) {
        return false;
    }

    char buf[160];
    strncpy(buf, line, sizeof(buf) - 1u);
    buf[sizeof(buf) - 1u] = '\0';
    char *nl = strchr(buf, '\n');
    if (nl)
        *nl = '\0';

    char *t = trim_inplace(buf);
    if (*t == '\0' || *t == '#') {
        return false;
    }

    char *eq = strchr(t, '=');
    if (!eq) {
        return false;
    }
    *eq = '\0';
    const char *k = trim_inplace(t);
    const char *v = trim_inplace(eq + 1);

    if (strlen(k) >= key_size || strlen(v) >= value_size) {
        return false;
    }
    strcpy(key, k);
    strcpy(value, v);
    return true;
}

bool settings_apply_kv(Settings *s, const char *key, const char *value) {
    if (!s || !key || !value) {
        return false;
    }
    if (strcmp(key, "lang") == 0) {
        if (strcmp(value, "es") == 0) {
            s->lang = LangEs;
            return true;
        }
        if (strcmp(value, "en") == 0) {
            s->lang = LangEn;
            return true;
        }
        return false;
    }
    if (strcmp(key, "last_id") == 0) {
        char *end = NULL;
        const unsigned long parsed = strtoul(value, &end, 10);
        if (end == value || *end != '\0' || parsed > 0xFFFFFFFFul) {
            return false;
        }
        s->last_id = (uint32_t)parsed;
        s->last_id_valid = true;
        return true;
    }
    return false;
}

/* ---- Furi I/O ---- */

#ifdef TZ_HAVE_FURI

bool settings_load(Settings *out) {
    if (!out)
        return false;
    *out = settings_default();

    Storage *storage = furi_record_open(RECORD_STORAGE);
    File *file = storage_file_alloc(storage);
    bool ok = false;

    if (storage_file_open(file, SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buf[160];
        size_t pos = 0u;
        char ch;
        while (storage_file_read(file, &ch, 1) == 1) {
            if (ch == '\n' || pos == sizeof(buf) - 1u) {
                buf[pos] = '\0';
                char k[32];
                char v[96];
                if (settings_split_line(buf, k, sizeof(k), v, sizeof(v))) {
                    settings_apply_kv(out, k, v);
                }
                pos = 0u;
                if (ch != '\n') {
                    /* line was truncated; reset and skip until next '\n' */
                    while (storage_file_read(file, &ch, 1) == 1 && ch != '\n') {
                        /* drain */
                    }
                }
            } else {
                buf[pos++] = ch;
            }
        }
        if (pos > 0u) {
            buf[pos] = '\0';
            char k[32];
            char v[96];
            if (settings_split_line(buf, k, sizeof(k), v, sizeof(v))) {
                settings_apply_kv(out, k, v);
            }
        }
        ok = true;
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool settings_save(const Settings *s) {
    if (!s)
        return false;
    Storage *storage = furi_record_open(RECORD_STORAGE);
    File *file = storage_file_alloc(storage);
    bool ok = false;

    storage_common_mkdir(storage, APPS_DATA_DIR);
    if (storage_file_open(file, SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, SETTINGS_HEADER, sizeof(SETTINGS_HEADER) - 1u);
        const char *lang = (s->lang == LangEs) ? "lang=es\n" : "lang=en\n";
        storage_file_write(file, lang, strlen(lang));
        if (s->last_id_valid) {
            char line[40];
            const int n = snprintf(line, sizeof(line), "last_id=%lu\n", (unsigned long)s->last_id);
            if (n > 0 && (size_t)n < sizeof(line)) {
                storage_file_write(file, line, (size_t)n);
            }
        }
        ok = true;
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

#endif /* TZ_HAVE_FURI */
