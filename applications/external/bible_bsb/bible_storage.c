#include "bible_storage.h"
#include "bible_renderer.h"
#include "bible_loader.h"
#include "bible_books.h"
#include "bible_time.h"

#include <furi.h>
#include <furi_hal_rtc.h>
#include <storage/storage.h>
#include <string.h>

#define COLLECTION_PATH APP_DATA_PATH("collection.json")
#define LAST_READ_PATH  APP_DATA_PATH("last_read.bin")
#define LAST_READ_MAGIC 0xB5

static void bible_stamp_now(char* out, size_t out_len) {
    DateTime dt = {0};
    furi_hal_rtc_get_datetime(&dt);
    if(!bible_format_iso8601(
           out, out_len, dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second)) {
        if(out && out_len > 0) {
            out[0] = '\0';
        }
    }
}

/* ============================================================================
 * Streaming JSON writer — 512-byte fixed buffer, writes directly to SD file
 * ============================================================================ */

typedef struct {
    File* file;
    uint8_t buf[512];
    uint16_t pos;
    uint32_t total_written;
    bool failed;
} JsonWriter;

static void json_writer_init(JsonWriter* w, File* file) {
    furi_check(w);
    furi_check(file);
    memset(w, 0, sizeof(JsonWriter));
    w->file = file;
}

static void json_writer_flush(JsonWriter* w) {
    if(w->pos == 0 || w->failed) return;
    size_t written = storage_file_write(w->file, w->buf, w->pos);
    if(written != w->pos) {
        w->failed = true;
    } else {
        w->total_written += written;
    }
    w->pos = 0;
}

static void json_writer_push_byte(JsonWriter* w, uint8_t b) {
    if(w->pos >= sizeof(w->buf)) {
        json_writer_flush(w);
    }
    w->buf[w->pos++] = b;
}

static void json_writer_push_bytes(JsonWriter* w, const uint8_t* bytes, size_t len) {
    for(size_t i = 0; i < len; i++) {
        json_writer_push_byte(w, bytes[i]);
    }
}

static void json_writer_push_str(JsonWriter* w, const char* s) {
    json_writer_push_bytes(w, (const uint8_t*)s, strlen(s));
}

static void json_writer_push_u16(JsonWriter* w, uint16_t n) {
    char tmp[6];
    uint8_t i = 0;
    if(n == 0) {
        tmp[i++] = '0';
    } else {
        while(n > 0) {
            tmp[i++] = '0' + (n % 10);
            n /= 10;
        }
    }
    for(int j = i - 1; j >= 0; j--) {
        json_writer_push_byte(w, (uint8_t)tmp[j]);
    }
}

static void json_writer_push_escaped(JsonWriter* w, const char* s) {
    const char* p = s;
    while(*p != '\0') {
        char c = *p;
        if(c == '"') {
            json_writer_push_str(w, "\\\"");
        } else if(c == '\\') {
            json_writer_push_str(w, "\\\\");
        } else if(c == '\n') {
            json_writer_push_str(w, "\\n");
        } else if(c == '\r') {
            json_writer_push_str(w, "\\r");
        } else if(c == '\t') {
            json_writer_push_str(w, "\\t");
        } else {
            json_writer_push_byte(w, (uint8_t)c);
        }
        p++;
    }
}

static uint32_t json_writer_finish(JsonWriter* w) {
    json_writer_flush(w);
    return w->total_written;
}

static bool json_writer_ok(const JsonWriter* w) {
    return !w->failed;
}

/* Close and free `file` when non-NULL, then release RECORD_STORAGE.
 * Call only after furi_record_open(RECORD_STORAGE) succeeded. */
static void bible_close_storage_file(File* file) {
    if(file) {
        storage_file_close(file);
        storage_file_free(file);
    }
    furi_record_close(RECORD_STORAGE);
}

/* ============================================================================
 * Collection JSON write — streaming writer
 * ============================================================================ */

bool bible_storage_save_collection(const BibleAppState* state) {
    furi_check(state);

    if(state->collection_count > 0 && state->collection == NULL) {
        return false;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        return false;
    }

    File* file = storage_file_alloc(storage);
    if(!file) {
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    bool opened = storage_file_open(file, COLLECTION_PATH, FSAM_WRITE, FSOM_OPEN_ALWAYS);
    if(!opened) {
        bible_close_storage_file(file);
        return false;
    }

    storage_file_truncate(file);

    JsonWriter w;
    json_writer_init(&w, file);

    json_writer_push_str(&w, "{\"version\":\"kindled-flipper-v2\",\"passages\":[");
    for(uint8_t i = 0; i < state->collection_count; i++) {
        if(i > 0) {
            json_writer_push_str(&w, ",");
        }
        const BibleCollectionEntry* e = &state->collection[i];
        json_writer_push_str(&w, "{\"scripture_ref\":\"");
        json_writer_push_escaped(&w, e->scripture_ref);
        json_writer_push_str(&w, "\",\"scripture_display_ref\":\"");
        json_writer_push_escaped(&w, e->scripture_display_ref);
        json_writer_push_str(&w, "\",\"scripture_translation\":\"");
        json_writer_push_escaped(&w, e->scripture_translation);
        json_writer_push_str(&w, "\",\"book_index\":");
        json_writer_push_u16(&w, e->book_index);
        json_writer_push_str(&w, ",\"chapter\":");
        json_writer_push_u16(&w, e->chapter);
        json_writer_push_str(&w, ",\"start_verse\":");
        json_writer_push_u16(&w, e->start_verse);
        json_writer_push_str(&w, ",\"end_verse\":");
        json_writer_push_u16(&w, e->end_verse);
        json_writer_push_str(&w, ",\"captured_at\":\"");
        json_writer_push_escaped(&w, e->captured_at);
        json_writer_push_str(&w, "\",\"note\":\"");
        json_writer_push_escaped(&w, e->note);
        json_writer_push_str(&w, "\"}");
    }
    json_writer_push_str(&w, "]}");

    uint32_t total = json_writer_finish(&w);
    bool ok = json_writer_ok(&w) && total > 0;

    bible_close_storage_file(file);

    return ok;
}

/* ============================================================================
 * Collection JSON read — minimal byte scanner, chunked 1KB reads, max 2KB
 * ============================================================================ */

/* Find "key" starting from position start, return position of value start.
 * For string values, returns position after opening quote.
 * For numeric values, returns position of first digit.
 * Returns -1 if not found.
 */
static int32_t find_key_value(
    const uint8_t* data,
    size_t len,
    size_t start,
    const char* key,
    bool* out_is_string) {
    size_t key_len = strlen(key);
    size_t i = start;

    while(i + key_len + 3 < len) {
        if(data[i] == '"' && memcmp(&data[i + 1], key, key_len) == 0 &&
           data[i + 1 + key_len] == '"') {
            i += key_len + 2; /* skip past "key" */
            /* skip colon and whitespace */
            while(i < len && (data[i] == ':' || data[i] == ' ' || data[i] == '\t' ||
                              data[i] == '\n' || data[i] == '\r')) {
                i++;
            }
            if(i >= len) return -1;

            if(data[i] == '"') {
                *out_is_string = true;
                return (int32_t)(i + 1); /* position after opening quote */
            } else {
                *out_is_string = false;
                return (int32_t)i; /* position of first digit or value char */
            }
        }
        i++;
    }
    return -1;
}

/* Extract string value (between quotes) into out buffer */
static bool extract_string_value(
    const uint8_t* data,
    size_t len,
    int32_t val_start,
    char* out,
    size_t out_len) {
    if(val_start < 0 || (size_t)val_start >= len) return false;
    size_t i = (size_t)val_start;
    size_t j = 0;
    while(i < len && data[i] != '"') {
        if(j + 1 < out_len) {
            out[j++] = (char)data[i];
        }
        i++;
    }
    if(j < out_len) {
        out[j] = '\0';
    }
    return true;
}

/* Extract numeric value (digits only) */
static uint16_t extract_u16_value(const uint8_t* data, size_t len, int32_t val_start) {
    if(val_start < 0 || (size_t)val_start >= len) return 0;
    uint16_t n = 0;
    size_t i = (size_t)val_start;
    while(i < len && data[i] >= '0' && data[i] <= '9') {
        n = n * 10 + (data[i] - '0');
        i++;
    }
    return n;
}

bool bible_storage_load_collection(BibleAppState* state) {
    furi_check(state);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        bible_toast_set(&state->toast, "OOM");
        return false;
    }

    File* file = storage_file_alloc(storage);
    if(!file) {
        furi_record_close(RECORD_STORAGE);
        bible_toast_set(&state->toast, "OOM");
        return false;
    }

    bool opened = storage_file_open(file, COLLECTION_PATH, FSAM_READ, FSOM_OPEN_EXISTING);

    if(!opened) {
        bible_close_storage_file(file);
        /* File doesn't exist yet — not an error, just empty collection */
        state->collection_count = 0;
        state->collection_loaded = true;
        return true;
    }

    /* Read in 1KB chunks, max 2KB total */
    uint8_t chunk[BIBLE_READ_CHUNK];
    uint8_t* buf = NULL;
    size_t total = 0;
    size_t capacity = 0;
    bool oom = false;

    while(total < BIBLE_MAX_COLLECTION_SIZE) {
        size_t n = storage_file_read(file, chunk, sizeof(chunk));
        if(n == 0) break;

        if(total + n > BIBLE_MAX_COLLECTION_SIZE) {
            n = BIBLE_MAX_COLLECTION_SIZE - total;
        }

        if(total + n > capacity) {
            size_t new_cap = capacity + BIBLE_READ_CHUNK;
            if(new_cap > BIBLE_MAX_COLLECTION_SIZE) {
                new_cap = BIBLE_MAX_COLLECTION_SIZE;
            }
            size_t old_bytes = buf ? capacity + 1 : 0;
            uint8_t* new_buf = bible_heap_grow(buf, old_bytes, new_cap + 1);
            if(!new_buf) {
                oom = true;
                break;
            }
            buf = new_buf;
            capacity = new_cap;
        }

        if(!buf) {
            oom = true;
            break;
        }

        memcpy(buf + total, chunk, n);
        total += n;
    }

    bible_close_storage_file(file);

    if(oom) {
        free(buf);
        bible_toast_set(&state->toast, "OOM");
        return false;
    }

    if(total == 0 || buf == NULL) {
        free(buf);
        state->collection_count = 0;
        state->collection_loaded = true;
        return true;
    }

    buf[total] = '\0';

    /* Parse JSON — find passages array entries */
    state->collection_count = 0;
    size_t i = 0;

    while(i < total && state->collection_count < BIBLE_MAX_COLLECTION) {
        bool is_string = false;

        int32_t ref_start = find_key_value(buf, total, i, "scripture_ref", &is_string);
        if(ref_start < 0) break;

        /* Ensure collection array has room. Do not write through a NULL buffer. */
        if(!bible_collection_ensure(state, state->collection_count + 1) || !state->collection) {
            free(buf);
            bible_toast_set(&state->toast, "OOM");
            return false;
        }

        BibleCollectionEntry* e = &state->collection[state->collection_count];
        memset(e, 0, sizeof(BibleCollectionEntry));

        extract_string_value(buf, total, ref_start, e->scripture_ref, sizeof(e->scripture_ref));

        int32_t display_ref_start =
            find_key_value(buf, total, (size_t)ref_start, "scripture_display_ref", &is_string);
        if(display_ref_start >= 0) {
            extract_string_value(
                buf,
                total,
                display_ref_start,
                e->scripture_display_ref,
                sizeof(e->scripture_display_ref));
        }

        int32_t trans_start =
            find_key_value(buf, total, (size_t)ref_start, "scripture_translation", &is_string);
        if(trans_start >= 0) {
            extract_string_value(
                buf,
                total,
                trans_start,
                e->scripture_translation,
                sizeof(e->scripture_translation));
        }

        int32_t book_idx_start =
            find_key_value(buf, total, (size_t)ref_start, "book_index", &is_string);
        e->book_index = (uint8_t)extract_u16_value(buf, total, book_idx_start);

        int32_t chapter_start =
            find_key_value(buf, total, (size_t)ref_start, "chapter", &is_string);
        e->chapter = extract_u16_value(buf, total, chapter_start);

        int32_t start_verse_start =
            find_key_value(buf, total, (size_t)ref_start, "start_verse", &is_string);
        e->start_verse = extract_u16_value(buf, total, start_verse_start);

        int32_t end_verse_start =
            find_key_value(buf, total, (size_t)ref_start, "end_verse", &is_string);
        e->end_verse = extract_u16_value(buf, total, end_verse_start);

        int32_t captured_start =
            find_key_value(buf, total, (size_t)ref_start, "captured_at", &is_string);
        if(captured_start >= 0) {
            extract_string_value(
                buf, total, captured_start, e->captured_at, sizeof(e->captured_at));
        }

        int32_t note_start = find_key_value(buf, total, (size_t)ref_start, "note", &is_string);
        if(note_start >= 0) {
            extract_string_value(buf, total, note_start, e->note, sizeof(e->note));
        }

        state->collection_count++;
        i = (size_t)ref_start + 1; /* advance past this entry */
    }

    free(buf);
    state->collection_loaded = true;
    return true;
}

/* ============================================================================
 * save_passage — full defrag / load / check / write / reload / toast flow
 * ============================================================================ */

static void build_canonical_ref(const BiblePassage* passage, char* out, size_t out_len) {
    const char* osis = OSIS_BOOK_CODES[passage->book_index];
    if(passage->start_verse > 0 && passage->end_verse > passage->start_verse) {
        snprintf(
            out,
            out_len,
            "%s.%u.%u-%s.%u.%u",
            osis,
            (unsigned int)passage->chapter,
            (unsigned int)passage->start_verse,
            osis,
            (unsigned int)passage->chapter,
            (unsigned int)passage->end_verse);
    } else if(passage->start_verse > 0) {
        snprintf(
            out,
            out_len,
            "%s.%u.%u",
            osis,
            (unsigned int)passage->chapter,
            (unsigned int)passage->start_verse);
    } else {
        /* All verses: canonical ref has no verse number */
        snprintf(out, out_len, "%s.%u", osis, (unsigned int)passage->chapter);
    }
}

static void build_display_ref(const BiblePassage* passage, char* out, size_t out_len) {
    const char* book_name = OSIS_BOOK_NAMES[passage->book_index];
    if(passage->start_verse > 0 && passage->end_verse > passage->start_verse) {
        snprintf(
            out,
            out_len,
            "%s %u:%u-%u",
            book_name,
            (unsigned int)passage->chapter,
            (unsigned int)passage->start_verse,
            (unsigned int)passage->end_verse);
    } else if(passage->start_verse > 0) {
        snprintf(
            out,
            out_len,
            "%s %u:%u",
            book_name,
            (unsigned int)passage->chapter,
            (unsigned int)passage->start_verse);
    } else {
        snprintf(out, out_len, "%s %u", book_name, (unsigned int)passage->chapter);
    }
}

/* Reload full chapter display lines for the current passage metadata.
 * Used after heap-defrag operations during save. */
static bool bible_reload_passage_lines(BibleAppState* state) {
    state->line_count = 0;
    return bible_load_chapter(state->passage.book_index, state->passage.chapter, 0, 0, state);
}

/* Restore the reader after a save-path detour.
 * If the reload already toasted "OOM", keep that instead of hiding it. */
static void bible_finish_with_toast(BibleAppState* state, uint16_t saved_scroll, const char* msg) {
    bool reloaded = bible_reload_passage_lines(state);
    state->scroll_offset = saved_scroll;
    if(reloaded || !bible_toast_active(&state->toast)) {
        bible_toast_set(&state->toast, msg);
    }
}

void bible_save_passage(BibleAppState* state) {
    furi_check(state);

    if(state->passage.verse_count == 0) {
        bible_toast_set(&state->toast, "No passage");
        return;
    }

    /* 1. Save scroll offset so we can restore it */
    uint16_t saved_scroll = state->scroll_offset;

    /* 2. Clear lines (heap defrag) */
    state->line_count = 0;

    /* 3. Load collection from SD if not already loaded */
    if(!state->collection_loaded) {
        if(!bible_storage_load_collection(state)) {
            bible_finish_with_toast(state, saved_scroll, "OOM");
            return;
        }
    }

    if(state->collection_count > 0 && state->collection == NULL) {
        bible_finish_with_toast(state, saved_scroll, "OOM");
        return;
    }

    /* 4. Build canonical ref for duplicate check */
    char new_ref[32];
    build_canonical_ref(&state->passage, new_ref, sizeof(new_ref));

    /* 5. Check for duplicate scripture_ref */
    for(uint8_t i = 0; i < state->collection_count; i++) {
        if(strcmp(state->collection[i].scripture_ref, new_ref) == 0) {
            /* Duplicate found — reload lines and show toast */
            bible_finish_with_toast(state, saved_scroll, "Already saved");
            return;
        }
    }

    /* 6. Check capacity */
    if(state->collection_count >= BIBLE_MAX_COLLECTION) {
        bible_finish_with_toast(state, saved_scroll, "Collection full");
        return;
    }

    /* 7. Build display ref */
    char display_ref[32];
    build_display_ref(&state->passage, display_ref, sizeof(display_ref));

    /* 8. Ensure collection capacity and append new entry */
    if(!bible_collection_ensure(state, state->collection_count + 1) || !state->collection) {
        bible_finish_with_toast(state, saved_scroll, "OOM");
        return;
    }
    BibleCollectionEntry* e = &state->collection[state->collection_count];
    memset(e, 0, sizeof(BibleCollectionEntry));
    strlcpy(e->scripture_ref, new_ref, sizeof(e->scripture_ref));
    strlcpy(e->scripture_display_ref, display_ref, sizeof(e->scripture_display_ref));
    strlcpy(e->scripture_translation, "BSB", sizeof(e->scripture_translation));
    e->book_index = state->passage.book_index;
    e->chapter = state->passage.chapter;
    e->start_verse = state->passage.start_verse;
    e->end_verse = state->passage.end_verse;
    bible_stamp_now(e->captured_at, sizeof(e->captured_at));
    e->note[0] = '\0';

    state->collection_count++;

    /* 9. Write collection to SD */
    bool saved = bible_storage_save_collection(state);
    if(!saved) {
        /* Rollback: remove the entry we just added */
        state->collection_count--;
    }

    /* 10. Reload lines (restore reader display) and toast.
     * An OOM toast from the reload wins over Saved / Save failed. */
    bible_finish_with_toast(state, saved_scroll, saved ? "Saved!" : "Save failed");
}

/* ============================================================================
 * load_collection_for_read — load a saved passage back into the reader
 * ============================================================================ */

bool bible_storage_load_verses_for_entry(BibleAppState* state, uint8_t entry_index) {
    furi_check(state);
    if(entry_index >= state->collection_count) return false;
    if(state->collection == NULL) {
        bible_toast_set(&state->toast, "OOM");
        return false;
    }

    const BibleCollectionEntry* e = &state->collection[entry_index];

    /* 1. Save scroll and clear lines (defrag) */
    uint16_t saved_scroll = state->scroll_offset;
    state->line_count = 0;

    /* 2. Load the full chapter from SD and jump to the saved start_verse */
    bool loaded = bible_load_chapter(e->book_index, e->chapter, 0, 0, state);

    if(!loaded) {
        /* Restore previous display state if possible */
        state->scroll_offset = saved_scroll;
        return false;
    }

    /* 3. Set navigation state and position scroll at e->start_verse */
    state->selected_book = e->book_index;
    if(e->book_index < BIBLE_BOOK_COUNT && OSIS_BOOK_NAMES[e->book_index]) {
        strlcpy(
            state->selected_book_name,
            OSIS_BOOK_NAMES[e->book_index],
            sizeof(state->selected_book_name));
    }
    state->selected_chapter = e->chapter;
    state->selected_start_verse = (e->start_verse > 0) ? e->start_verse : 1;
    state->selected_end_verse =
        (e->end_verse >= state->selected_start_verse) ? e->end_verse : state->selected_start_verse;
    state->scroll_offset = 0;
    if(e->start_verse > 1 && state->lines) {
        for(uint16_t i = 0; i < state->line_count; i++) {
            if(state->lines[i].verse_number >= e->start_verse && state->lines[i].is_verse_number) {
                state->scroll_offset = i;
                break;
            }
        }
    }
    state->reader_on_chapter_page = false;
    state->reader_came_from_collection = true;

    return true;
}

/* ============================================================================
 * build_kindled_json — build NFC export JSON into caller-provided buffer
 * ============================================================================ */

static bool json_buf_append(char* buf, size_t buf_len, size_t* pos, const char* s);
static bool json_buf_append_escaped(char* buf, size_t buf_len, size_t* pos, const char* s);

typedef struct {
    char* buf;
    size_t len;
    size_t* pos;
    bool first_verse;
} EmitCtx;

static bool emit_verse_cb(uint16_t verse_num, const char* text, void* ctx) {
    EmitCtx* ectx = (EmitCtx*)ctx;
    char tmp[64];

    if(!ectx->first_verse) {
        if(!json_buf_append(ectx->buf, ectx->len, ectx->pos, ",")) return false;
    }
    ectx->first_verse = false;

    snprintf(tmp, sizeof(tmp), "{\"number\":%u,\"text\":\"", (unsigned int)verse_num);
    if(!json_buf_append(ectx->buf, ectx->len, ectx->pos, tmp)) return false;
    if(!json_buf_append_escaped(ectx->buf, ectx->len, ectx->pos, text)) return false;
    if(!json_buf_append(ectx->buf, ectx->len, ectx->pos, "\"}")) return false;
    return true;
}

static bool json_buf_append(char* buf, size_t buf_len, size_t* pos, const char* s) {
    size_t s_len = strlen(s);
    if(*pos + s_len >= buf_len) return false;
    memcpy(buf + *pos, s, s_len);
    *pos += s_len;
    buf[*pos] = '\0';
    return true;
}

static bool json_buf_append_escaped(char* buf, size_t buf_len, size_t* pos, const char* s) {
    const char* p = s;
    while(*p != '\0') {
        char c = *p;
        const char* esc = NULL;
        if(c == '"') {
            esc = "\\\"";
        } else if(c == '\\') {
            esc = "\\\\";
        } else if(c == '\n') {
            esc = "\\n";
        } else if(c == '\r') {
            esc = "\\r";
        } else if(c == '\t') {
            esc = "\\t";
        }
        if(esc != NULL) {
            size_t esc_len = strlen(esc);
            if(*pos + esc_len >= buf_len) return false;
            memcpy(buf + *pos, esc, esc_len);
            *pos += esc_len;
        } else {
            if(*pos + 1 >= buf_len) return false;
            buf[(*pos)++] = c;
        }
        p++;
    }
    buf[*pos] = '\0';
    return true;
}

bool bible_storage_build_kindled_json(BibleAppState* state, char* out_buf, size_t out_len) {
    furi_check(state);
    furi_check(out_buf);
    furi_check(out_len > 0);

    if(state->collection_count == 0 || state->collection == NULL) {
        out_buf[0] = '\0';
        if(state->collection_count > 0 && state->collection == NULL) {
            bible_toast_set(&state->toast, "OOM");
        }
        return false;
    }

    size_t pos = 0;
    out_buf[0] = '\0';

    char exported_at[32];
    bible_stamp_now(exported_at, sizeof(exported_at));
    char export_header[160];
    if(exported_at[0] != '\0') {
        snprintf(
            export_header,
            sizeof(export_header),
            "{\"format\":\"kindled\",\"version\":1,\"exported_at\":\"%s\","
            "\"schema_version\":1,\"counts\":{\"blocks\":",
            exported_at);
    } else {
        snprintf(
            export_header,
            sizeof(export_header),
            "{\"format\":\"kindled\",\"version\":1,"
            "\"schema_version\":1,\"counts\":{\"blocks\":");
    }
    if(!json_buf_append(out_buf, out_len, &pos, export_header)) return false;

    char num_buf[8];
    snprintf(num_buf, sizeof(num_buf), "%u", (unsigned int)state->collection_count);
    if(!json_buf_append(out_buf, out_len, &pos, num_buf)) return false;

    if(!json_buf_append(
           out_buf,
           out_len,
           &pos,
           ",\"entities\":0,\"links\":0,\"reflections\":0,\"life_stages\":0},"
           "\"data\":{\"blocks\":["))
        return false;

    /* Save original passage metadata so we can restore it after using the scanner.
     * Lines are not touched by this function (caller is on Collection screen). */
    BiblePassage saved_passage = state->passage;

    for(uint8_t i = 0; i < state->collection_count; i++) {
        if(i > 0) {
            if(!json_buf_append(out_buf, out_len, &pos, ",")) {
                goto build_fail;
            }
        }

        const BibleCollectionEntry* e = &state->collection[i];

        char block_buf[256];
        snprintf(
            block_buf,
            sizeof(block_buf),
            "{\"id\":\"block-%u\",\"type\":\"scripture\",\"content\":\"",
            (unsigned int)i);
        if(!json_buf_append(out_buf, out_len, &pos, block_buf)) {
            goto build_fail;
        }
        if(!json_buf_append_escaped(out_buf, out_len, &pos, e->scripture_display_ref)) {
            goto build_fail;
        }

        snprintf(
            block_buf,
            sizeof(block_buf),
            "\",\"scripture_ref\":\"%s\",\"scripture_display_ref\":\"",
            e->scripture_ref);
        if(!json_buf_append(out_buf, out_len, &pos, block_buf)) {
            goto build_fail;
        }
        if(!json_buf_append_escaped(out_buf, out_len, &pos, e->scripture_display_ref)) {
            goto build_fail;
        }

        snprintf(
            block_buf,
            sizeof(block_buf),
            "\",\"scripture_translation\":\"%s\",\"scripture_verses\":[",
            e->scripture_translation);
        if(!json_buf_append(out_buf, out_len, &pos, block_buf)) {
            goto build_fail;
        }

        /* Stream verses from SD via callback — no RAM verse array */
        typedef struct {
            char* buf;
            size_t len;
            size_t* pos;
            bool first_verse;
        } EmitCtx;

        BibleVerseCallback emit_verse = &emit_verse_cb;
        EmitCtx cb_ctx = {out_buf, out_len, &pos, true};
        bool scanned = bible_load_chapter_verse_scan(
            e->book_index, e->chapter, e->start_verse, e->end_verse, emit_verse, &cb_ctx);

        if(!scanned) {
            /* No verses scanned — write empty array (already opened [) */
        }

        if(!json_buf_append(out_buf, out_len, &pos, "],\"source\":\"manual\",\"captured_at\":\"")) {
            goto build_fail;
        }
        if(!json_buf_append_escaped(out_buf, out_len, &pos, e->captured_at)) {
            goto build_fail;
        }
        if(!json_buf_append(out_buf, out_len, &pos, "\",\"modified_at\":\"")) {
            goto build_fail;
        }
        if(!json_buf_append_escaped(out_buf, out_len, &pos, e->captured_at)) {
            goto build_fail;
        }
        if(!json_buf_append(out_buf, out_len, &pos, "\",\"tags\":[]}")) {
            goto build_fail;
        }
    }

    if(!json_buf_append(
           out_buf,
           out_len,
           &pos,
           "],\"entities\":[],\"links\":[],\"reflections\":[],\"life_stages\":[]}}")) {
        goto build_fail;
    }

    /* Restore original passage metadata */
    state->passage = saved_passage;
    return true;

build_fail:
    /* Restore original passage metadata on failure too */
    state->passage = saved_passage;
    return false;
}

bool bible_storage_save_last_read(const BibleAppState* state) {
    if(!state || state->passage.verse_count == 0 ||
       state->passage.book_index >= BIBLE_BOOK_COUNT) {
        return false;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;

    File* file = storage_file_alloc(storage);
    if(!file) {
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    if(!storage_file_open(file, LAST_READ_PATH, FSAM_WRITE, FSOM_OPEN_ALWAYS)) {
        bible_close_storage_file(file);
        return false;
    }

    storage_file_truncate(file);

    uint8_t buf[6];
    buf[0] = LAST_READ_MAGIC;
    buf[1] = state->passage.book_index;
    buf[2] = (uint8_t)(state->passage.chapter & 0xFF);
    buf[3] = (uint8_t)((state->passage.chapter >> 8) & 0xFF);
    buf[4] = (uint8_t)(state->scroll_offset & 0xFF);
    buf[5] = (uint8_t)((state->scroll_offset >> 8) & 0xFF);

    size_t written = storage_file_write(file, buf, sizeof(buf));
    bible_close_storage_file(file);
    return written == sizeof(buf);
}

bool bible_storage_load_last_read(BibleAppState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;

    File* file = storage_file_alloc(storage);
    if(!file) {
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    if(!storage_file_open(file, LAST_READ_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        bible_close_storage_file(file);
        return false;
    }

    uint8_t buf[6];
    size_t n = storage_file_read(file, buf, sizeof(buf));
    bible_close_storage_file(file);

    if(n != sizeof(buf) || buf[0] != LAST_READ_MAGIC) {
        return false;
    }

    uint8_t book = buf[1];
    uint16_t chapter = (uint16_t)buf[2] | ((uint16_t)buf[3] << 8);
    uint16_t scroll = (uint16_t)buf[4] | ((uint16_t)buf[5] << 8);

    if(book >= BIBLE_BOOK_COUNT || chapter == 0 || chapter > BOOK_CHAPTER_COUNTS[book]) {
        return false;
    }

    if(!bible_load_chapter(book, chapter, 0, 0, state) || state->line_count == 0) {
        return false;
    }

    state->selected_book = book;
    if(OSIS_BOOK_NAMES[book]) {
        strlcpy(
            state->selected_book_name, OSIS_BOOK_NAMES[book], sizeof(state->selected_book_name));
    }
    state->selected_chapter = chapter;
    state->selected_genre = get_genre_for_book(book);
    state->selected_testament = BIBLE_GENRES[state->selected_genre].testament;
    state->book_scroll = (book >= 5) ? (book - 4) : 0;
    state->scroll_offset = (scroll < state->line_count) ? scroll : 0;
    state->reader_on_chapter_page = false;
    state->reader_came_from_collection = false;
    return true;
}
