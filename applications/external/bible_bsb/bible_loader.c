#include "bible_loader.h"
#include "bible_books.h"
#include "bible_pack.h"
#include "bible_renderer.h"

#include <compress.h>
#include <furi.h>
#include <storage/storage.h>
#include <string.h>

/* ============================================================================
 * Chapter loader — single-pass incremental parsing, NO verse array in RAM
 *
 * Chapter text ships in assets/bsb.pack and is installed to
 * /ext/apps_assets/bible_bsb/bsb.pack. APP_ASSETS_PATH() is the firmware
 * alias for that directory. One chapter is decoded at a time into a buffer
 * no larger than BIBLE_MAX_FILE_SIZE.
 * ============================================================================ */

#define BSB_PACK_PATH           APP_ASSETS_PATH("bsb.pack")
#define BIBLE_PACK_MAX_CHAPTERS 2048

static void bible_sanitize_text(char* text, size_t max_len) {
    size_t len = strlen(text);
    if(len > max_len) {
        text[max_len] = '\0';
        len = max_len;
    }
    for(size_t i = 0; i + 2 < len; i++) {
        /* Em dash -> hyphen */
        if((unsigned char)text[i] == 0xE2 && (unsigned char)text[i + 1] == 0x80 &&
           (unsigned char)text[i + 2] == 0x94) {
            text[i] = '-';
            memmove(&text[i + 1], &text[i + 3], len - i - 2);
            len -= 2;
        }
        /* Right single quote -> apostrophe */
        else if(
            (unsigned char)text[i] == 0xE2 && (unsigned char)text[i + 1] == 0x80 &&
            (unsigned char)text[i + 2] == 0x99) {
            text[i] = '\'';
            memmove(&text[i + 1], &text[i + 3], len - i - 2);
            len -= 2;
        }
        /* Left single quote -> apostrophe */
        else if(
            (unsigned char)text[i] == 0xE2 && (unsigned char)text[i + 1] == 0x80 &&
            (unsigned char)text[i + 2] == 0x98) {
            text[i] = '\'';
            memmove(&text[i + 1], &text[i + 3], len - i - 2);
            len -= 2;
        }
        /* Left double quote -> straight quote */
        else if(
            (unsigned char)text[i] == 0xE2 && (unsigned char)text[i + 1] == 0x80 &&
            (unsigned char)text[i + 2] == 0x9C) {
            text[i] = '"';
            memmove(&text[i + 1], &text[i + 3], len - i - 2);
            len -= 2;
        }
        /* Right double quote -> straight quote */
        else if(
            (unsigned char)text[i] == 0xE2 && (unsigned char)text[i + 1] == 0x80 &&
            (unsigned char)text[i + 2] == 0x9D) {
            text[i] = '"';
            memmove(&text[i + 1], &text[i + 3], len - i - 2);
            len -= 2;
        }
    }
}

bool bible_chapter_data_present(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        return false;
    }
    bool present = storage_file_exists(storage, BSB_PACK_PATH);
    furi_record_close(RECORD_STORAGE);
    return present;
}

typedef enum {
    BibleFileOk,
    BibleFileMissing,
    BibleFileOom,
} BibleFileStatus;

/* Close and free a storage file, then release RECORD_STORAGE.
 * `file` may be NULL (alloc failed after the record was opened). */
static void bible_close_storage_file(File* file) {
    if(file) {
        storage_file_close(file);
        storage_file_free(file);
    }
    furi_record_close(RECORD_STORAGE);
}

/* Stream one compressed chapter out of the asset pack.
 * The heatshrink decoder reads the payload in small chunks. The only large
 * buffer is the decoded chapter, capped at BIBLE_MAX_FILE_SIZE.
 * BibleFileOk: caller must free(*out_buf).
 * Any other status: *out_buf is NULL and nothing is left allocated.
 */
typedef struct {
    File* file;
    uint32_t left;
    uint8_t* out;
    size_t out_cap;
    size_t out_len;
    bool failed;
} BiblePackStream;

static int32_t bible_pack_read_cb(void* context, uint8_t* buffer, size_t size) {
    BiblePackStream* stream = context;
    if(stream->failed || stream->left == 0 || size == 0) return 0;
    if(size > stream->left) size = stream->left;

    size_t n = storage_file_read(stream->file, buffer, size);
    if(n == 0) {
        stream->failed = true;
        return 0;
    }
    stream->left -= (uint32_t)n;
    return (int32_t)n;
}

static int32_t bible_pack_write_cb(void* context, uint8_t* buffer, size_t size) {
    BiblePackStream* stream = context;
    if(stream->out_len + size > stream->out_cap) {
        stream->failed = true;
        return 0;
    }
    if(size == 0) return 0;
    memcpy(stream->out + stream->out_len, buffer, size);
    stream->out_len += size;
    return (int32_t)size;
}

static bool bible_read_fully(File* file, uint8_t* dst, size_t n) {
    size_t got = 0;
    while(got < n) {
        size_t k = storage_file_read(file, dst + got, n - got);
        if(k == 0) return false;
        got += k;
    }
    return true;
}

static BibleFileStatus bible_read_chapter_file(
    uint8_t book_index,
    uint16_t chapter,
    uint8_t** out_buf,
    size_t* out_len) {
    furi_check(out_buf);
    furi_check(out_len);
    *out_buf = NULL;
    *out_len = 0;

    if(chapter == 0 || chapter > 255) return BibleFileMissing;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return BibleFileOom;

    File* file = storage_file_alloc(storage);
    if(!file) {
        furi_record_close(RECORD_STORAGE);
        return BibleFileOom;
    }

    if(!storage_file_open(file, BSB_PACK_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        bible_close_storage_file(file);
        return BibleFileMissing;
    }

    uint8_t header[BIBLE_PACK_HEADER_SIZE];
    if(!bible_read_fully(file, header, sizeof(header))) {
        bible_close_storage_file(file);
        return BibleFileMissing;
    }

    uint16_t count = (uint16_t)header[4] | ((uint16_t)header[5] << 8);
    if(count == 0 || count > BIBLE_PACK_MAX_CHAPTERS) {
        bible_close_storage_file(file);
        return BibleFileMissing;
    }

    size_t dir_len = BIBLE_PACK_HEADER_SIZE + (size_t)count * BIBLE_PACK_RECORD_SIZE;
    uint8_t* directory = malloc(dir_len);
    if(!directory) {
        bible_close_storage_file(file);
        return BibleFileOom;
    }
    memcpy(directory, header, sizeof(header));
    bool got_directory =
        bible_read_fully(file, directory + sizeof(header), dir_len - sizeof(header));

    BiblePackEntry entry = {0};
    bool found = false;
    if(got_directory) {
        found = bible_pack_find(directory, dir_len, book_index, (uint8_t)chapter, &entry);
    }
    uint8_t window = directory[8];
    uint8_t lookahead = directory[9];
    free(directory);

    if(!found || entry.raw_size > BIBLE_MAX_FILE_SIZE || entry.data_offset < dir_len) {
        bible_close_storage_file(file);
        return BibleFileMissing;
    }

    uint64_t file_size = storage_file_size(file);
    if((uint64_t)entry.data_offset + entry.comp_size > file_size) {
        bible_close_storage_file(file);
        return BibleFileMissing;
    }

    /* Drop the directory before the chapter buffer so the two never coexist. */
    uint8_t* buf = malloc((size_t)entry.raw_size + 1);
    if(!buf) {
        bible_close_storage_file(file);
        return BibleFileOom;
    }

    if(!storage_file_seek(file, entry.data_offset, true)) {
        free(buf);
        bible_close_storage_file(file);
        return BibleFileMissing;
    }

    BiblePackStream stream = {
        .file = file,
        .left = entry.comp_size,
        .out = buf,
        .out_cap = entry.raw_size,
        .out_len = 0,
        .failed = false,
    };
    CompressConfigHeatshrink heatshrink_config = {
        .window_sz2 = window,
        .lookahead_sz2 = lookahead,
        .input_buffer_sz = BIBLE_PACK_HS_INPUT,
    };
    Compress* compress = compress_alloc(CompressTypeHeatshrink, &heatshrink_config);
    if(!compress) {
        free(buf);
        bible_close_storage_file(file);
        return BibleFileOom;
    }

    bool decoded = compress_decode_streamed(
        compress, bible_pack_read_cb, &stream, bible_pack_write_cb, &stream);
    compress_free(compress);
    bible_close_storage_file(file);

    if(!decoded || stream.failed || stream.left != 0 || stream.out_len != entry.raw_size) {
        free(buf);
        return BibleFileMissing;
    }

    buf[entry.raw_size] = '\0';
    *out_buf = buf;
    *out_len = entry.raw_size;
    return BibleFileOk;
}

/* ============================================================================
 * Incremental JSON verse scanner: calls a callback for each matching verse.
 * Only ONE verse's text is in memory at a time.
 * ============================================================================ */

typedef void (*BibleHeadingCallback)(uint16_t verse_num, const char* heading, void* ctx);

static bool bible_scan_verses_from_json(
    const char* json,
    size_t json_len,
    uint16_t start_verse,
    uint16_t end_verse,
    BibleVerseCallback callback,
    BibleHeadingCallback heading_cb,
    void* ctx,
    uint16_t* out_max_verse) {
    const char* p = json;
    uint16_t count = 0;
    uint16_t max_verse = 0;

    while(*p != '\0') {
        /* Look for "n": */
        size_t offset = (size_t)(p - json);
        if(offset + 4 > json_len) break;

        if(*p == '"' && *(p + 1) == 'n' && *(p + 2) == '"' && *(p + 3) == ':') {
            p += 4;
            while(*p == ' ' || *p == '\t')
                p++;

            uint16_t verse_num = 0;
            while(*p >= '0' && *p <= '9') {
                verse_num = verse_num * 10 + (*p - '0');
                p++;
            }
            if(verse_num > max_verse) {
                max_verse = verse_num;
            }

            static char heading_buf[128];
            heading_buf[0] = '\0';

            /* Look for optional "h":" and required "t":" */
            while(*p != '\0') {
                offset = (size_t)(p - json);
                if(offset + 5 > json_len) break;

                if(*p == '"' && *(p + 1) == 'h' && *(p + 2) == '"' && *(p + 3) == ':' &&
                   *(p + 4) == '"') {
                    p += 5;
                    size_t h_len = 0;
                    bool h_esc = false;
                    while(*p != '\0' && h_len < sizeof(heading_buf) - 1) {
                        if(h_esc) {
                            heading_buf[h_len++] = *p;
                            h_esc = false;
                        } else if(*p == '\\') {
                            h_esc = true;
                        } else if(*p == '"') {
                            break;
                        } else {
                            heading_buf[h_len++] = *p;
                        }
                        p++;
                    }
                    heading_buf[h_len] = '\0';
                    bible_sanitize_text(heading_buf, sizeof(heading_buf) - 1);
                    if(*p == '"') p++;
                    continue;
                }

                if(*p == '"' && *(p + 1) == 't' && *(p + 2) == '"' && *(p + 3) == ':' &&
                   *(p + 4) == '"') {
                    p += 5;

                    /* Static so chapter load does not eat the app stack. */
                    static char text_buf[384];
                    size_t text_len = 0;
                    bool escaped = false;

                    while(*p != '\0' && text_len < sizeof(text_buf) - 1) {
                        if(escaped) {
                            if(*p == 'n') {
                                text_buf[text_len++] = '\n';
                            } else if(*p == 't') {
                                text_buf[text_len++] = '\t';
                            } else if(*p == 'r') {
                                text_buf[text_len++] = '\r';
                            } else {
                                text_buf[text_len++] = *p;
                            }
                            escaped = false;
                        } else if(*p == '\\') {
                            escaped = true;
                        } else if(*p == '"') {
                            break;
                        } else {
                            text_buf[text_len++] = *p;
                        }
                        p++;
                    }
                    text_buf[text_len] = '\0';
                    /* Apply verse range filter */
                    if(start_verse == 0 || (verse_num >= start_verse && verse_num <= end_verse)) {
                        if(heading_cb && heading_buf[0] != '\0') {
                            heading_cb(verse_num, heading_buf, ctx);
                        }
                        bible_sanitize_text(text_buf, sizeof(text_buf) - 1);
                        count++;
                        if(!callback(verse_num, text_buf, ctx)) {
                            if(out_max_verse) *out_max_verse = max_verse;
                            return true; /* caller requested early stop */
                        }
                    }
                    break;
                }
                p++;
            }
        } else {
            p++;
        }
    }

    if(out_max_verse) *out_max_verse = max_verse;
    return count > 0;
}

/* ============================================================================
 * Context structure for bible_load_chapter callback
 * ============================================================================ */

typedef struct {
    BibleAppState* state;
    bool oom;
} WrapContext;

static void bible_heading_callback(uint16_t verse_num, const char* heading, void* ctx) {
    WrapContext* wctx = (WrapContext*)ctx;
    BibleAppState* state = wctx->state;

    if(state->passage.section_count < BIBLE_MAX_CHAPTER_SECTIONS) {
        BibleSectionHeading* sec = &state->passage.sections[state->passage.section_count++];
        sec->start_verse = verse_num;
        strlcpy(sec->title, heading, sizeof(sec->title));
    }
}

static bool bible_wrap_callback(uint16_t verse_num, const char* text, void* ctx) {
    WrapContext* wctx = (WrapContext*)ctx;
    BibleAppState* state = wctx->state;

    /* The line buffer is allocated once before parsing. Stop when it is full
     * instead of growing it while the chapter file is still on the heap. */
    if(!state->lines || state->line_count >= state->lines_capacity) {
        return false;
    }

    bible_word_wrap(
        text,
        verse_num,
        BIBLE_MAX_CHARS_PER_LINE,
        state->lines,
        &state->line_count,
        state->lines_capacity);
    return true;
}

/* ============================================================================
 * bible_load_chapter — single pass, no verse array in RAM
 * ============================================================================ */

bool bible_load_chapter(
    uint8_t book_index,
    uint16_t chapter,
    uint16_t start_verse,
    uint16_t end_verse,
    BibleAppState* state) {
    furi_check(state);

    state->show_data_help = false;
    if(book_index >= BIBLE_BOOK_COUNT || chapter == 0) {
        return false;
    }

    /* One contiguous line buffer, before the chapter file is read. Growing it
     * later, while that file is still allocated, HardFaults on long chapters. */
    if(!bible_lines_ensure(state, BIBLE_MAX_LINES) || !state->lines) {
        bible_toast_set(&state->toast, "OOM");
        return false;
    }

    /* Read JSON file into a temporary buffer */
    uint8_t* buf = NULL;
    size_t total = 0;
    BibleFileStatus file_status = bible_read_chapter_file(book_index, chapter, &buf, &total);
    if(file_status == BibleFileOom) {
        bible_toast_set(&state->toast, "OOM");
        return false;
    }
    if(file_status == BibleFileMissing) {
        state->show_data_help = true;
        return false;
    }
    if(file_status != BibleFileOk) {
        return false;
    }

    /* Clear previous lines and passage. The line buffer itself stays allocated
     * so a retry does not need a fresh contiguous block. */
    state->line_count = 0;
    memset(&state->passage, 0, sizeof(BiblePassage));
    state->header_scroll_ticks = 0;
    state->header_scroll_key = NULL;

    WrapContext ctx = {.state = state, .oom = false};
    uint16_t max_verse = 0;

    /* Scan verses and wrap each one into lines immediately */
    bool ok = bible_scan_verses_from_json(
        (const char*)buf,
        total,
        start_verse,
        end_verse,
        bible_wrap_callback,
        bible_heading_callback,
        &ctx,
        &max_verse);

    free(buf);

    if(ctx.oom || (state->line_count > 0 && !state->lines)) {
        state->line_count = 0;
        memset(&state->passage, 0, sizeof(BiblePassage));
        bible_toast_set(&state->toast, "OOM");
        return false;
    }

    if(!ok || state->line_count == 0) {
        state->line_count = 0;
        return false;
    }

    /* Populate passage metadata */
    state->passage.book_index = book_index;
    state->passage.chapter = chapter;
    state->passage.start_verse = start_verse;
    state->passage.end_verse = end_verse;

    /* Derive verse_count from the highest verse_number in the chapter */
    for(uint16_t i = 0; i < state->line_count; i++) {
        if(state->lines[i].verse_number > max_verse) {
            max_verse = state->lines[i].verse_number;
        }
    }
    state->passage.verse_count = max_verse;

    return true;
}

/* ============================================================================
 * bible_load_chapter_verse_scan — stream verses via callback (no RAM storage)
 * ============================================================================ */

bool bible_load_chapter_verse_scan(
    uint8_t book_index,
    uint16_t chapter,
    uint16_t start_verse,
    uint16_t end_verse,
    BibleVerseCallback callback,
    void* ctx) {
    uint8_t* buf = NULL;
    size_t total = 0;
    if(bible_read_chapter_file(book_index, chapter, &buf, &total) != BibleFileOk) {
        return false;
    }

    bool ok = bible_scan_verses_from_json(
        (const char*)buf, total, start_verse, end_verse, callback, NULL, ctx, NULL);

    free(buf);
    return ok;
}
