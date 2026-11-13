/*
 * TagDuplicate v1.0
 * created by Marcopiiit
 *
 * Flipper Zero app (Momentum firmware) that:
 *  - Read:            reads the UID of an NFC tag and looks for it in saved Mifare .nfc files
 *  - Check duplicate: finds saved Mifare .nfc files that share the same UID
 *  - Log:             list of past searches (one entry per search, auto-saved on the SD card)
 *                     Up/Down = select, OK = full text, Left = delete the selected entry
 *  - About
 *
 * Both Read and Check show a progress bar while the saved files are compared.
 * Read feedback:  found     -> green LED blinks 3 times + vibration + sound
 *                 not found -> red LED + vibration for 1 second
 *
 * Speed: UIDs of saved files are cached (/ext/apps_data/tag_duplicate/cache.bin).
 * A file is re-read only if it is new or its size / modification time changed.
 */

#include <furi.h>
#include <furi_hal.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/text_box.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <toolbox/dir_walk.h>
#include <flipper_format/flipper_format.h>
#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>

#define NFC_DIR     "/ext/nfc"
#define DATA_DIR    "/ext/apps_data/tag_duplicate"
#define LOG_PATH    DATA_DIR "/log.txt"
#define CACHE_PATH  DATA_DIR "/cache.bin"
#define MAX_UID_LEN 10
#define MAX_FILES   400
#define NAME_LEN    64

// Log list
#define LOG_MAX_ENTRIES 40 // oldest entries are dropped beyond this
#define LOG_TIME_LEN    12 // "DD/MM HH:MM" + NUL
#define LOG_MSG_LEN     96
#define LOG_VISIBLE     3 // rows shown on screen
#define LOG_ITEM_H      18 // height of one row (two lines of text)
#define LOG_MAX_FILE    16384 // only the tail of the log file is loaded

/* Only files whose "Device type" contains this text are considered.
 * "Mifare" = Classic, Ultralight, DESFire, Plus.
 * Use "Mifare Classic" to restrict the search to Mifare Classic only.
 * (The cache is rebuilt automatically when this value changes.) */
#define MIFARE_FILTER "Mifare"

#define CACHE_MAGIC 0x54445550UL

/* ------------------------------------------------------------------ */
/* LED / vibration / sound feedback for Read                           */
/* ------------------------------------------------------------------ */

static const NotificationMessage message_beep_found = {
    .type = NotificationMessageTypeSoundOn,
    .data.sound.frequency = 1568.0f,
    .data.sound.volume = 1.0f,
};

// found: green LED blinks 3 times, with a vibration pulse and a beep on each blink
static const NotificationSequence sequence_tag_found = {
    &message_green_255,
    &message_vibro_on,
    &message_beep_found,
    &message_delay_100,
    &message_delay_50,
    &message_green_0,
    &message_vibro_off,
    &message_sound_off,
    &message_delay_100,

    &message_green_255,
    &message_vibro_on,
    &message_beep_found,
    &message_delay_100,
    &message_delay_50,
    &message_green_0,
    &message_vibro_off,
    &message_sound_off,
    &message_delay_100,

    &message_green_255,
    &message_vibro_on,
    &message_beep_found,
    &message_delay_100,
    &message_delay_50,
    &message_green_0,
    &message_vibro_off,
    &message_sound_off,
    NULL,
};

// not found: red LED on and steady vibration for 1 second
static const NotificationSequence sequence_tag_not_found = {
    &message_red_255,
    &message_vibro_on,
    &message_delay_1000,
    &message_vibro_off,
    &message_red_0,
    NULL,
};

/* ------------------------------------------------------------------ */
/* Types                                                               */
/* ------------------------------------------------------------------ */

typedef enum {
    ViewMain,
    ViewMessage, // widget: read result / about
    ViewText, // text box: duplicates / log entry detail
    ViewLog, // custom view: list of log entries
    ViewProgress, // custom view: progress bar
} AppView;

typedef enum {
    MenuRead,
    MenuCheckDuplicate,
    MenuLog,
    MenuAbout,
} MenuItem;

typedef enum {
    EventTagRead = 100,
    EventScanDone,
} AppEvent;

typedef struct {
    char name[NAME_LEN]; // path relative to /ext/nfc
    uint32_t timestamp;
    uint32_t size;
    uint8_t uid_len; // 0 = not a Mifare file / no UID
    uint8_t uid[MAX_UID_LEN];
    uint8_t seen;
} CacheEntry;

typedef struct {
    uint32_t magic;
    uint32_t sig;
    uint32_t count;
} CacheHeader;

typedef struct {
    char time[LOG_TIME_LEN]; // "DD/MM HH:MM"
    char msg[LOG_MSG_LEN];
} LogEntry;

typedef struct App App;

typedef struct {
    App* app;
    size_t pos; // selected entry (0 = newest)
    size_t top; // first visible entry
} LogViewModel;

typedef struct {
    const char* title;
    uint32_t done;
    uint32_t total; // 0 = still counting files
} ProgressModel;

struct App {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    Widget* widget;
    TextBox* text_box;
    View* log_view;
    View* progress_view;
    Storage* storage;
    NotificationApp* notifications;

    AppView current_view;
    AppView text_parent;

    // NFC reading
    Nfc* nfc;
    NfcPoller* poller;
    bool reading;
    uint8_t read_uid[MAX_UID_LEN];
    uint8_t read_uid_len;

    // cache of saved files
    CacheEntry* cache;
    size_t cache_count;
    bool cache_loaded;
    bool cache_dirty;

    // background scan
    FuriThread* worker;
    volatile bool cancel;
    bool scan_ok;
    int task; // MenuRead or MenuCheckDuplicate
    bool read_found;
    char read_name[NAME_LEN];

    FuriString* text; // text shown in text box

    // log: one entry per search, oldest first
    LogEntry* log;
    size_t log_count;
};

/* ------------------------------------------------------------------ */
/* Log (auto-saved on the SD card, one line per entry)                 */
/* ------------------------------------------------------------------ */

static void ensure_data_dir(App* app) {
    storage_common_mkdir(app->storage, "/ext/apps_data");
    storage_common_mkdir(app->storage, DATA_DIR);
}

static void log_write_line(File* file, const LogEntry* e) {
    char line[LOG_TIME_LEN + LOG_MSG_LEN + 4];
    int n = snprintf(line, sizeof(line), "%s %s\n", e->time, e->msg);
    if(n > 0) storage_file_write(file, line, (uint16_t)strlen(line));
}

// appends a single entry to the log file
static void log_append_file(App* app, const LogEntry* e) {
    ensure_data_dir(app);
    File* file = storage_file_alloc(app->storage);
    if(storage_file_open(file, LOG_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        log_write_line(file, e);
    }
    storage_file_close(file);
    storage_file_free(file);
}

// rewrites the whole log file from memory (after a delete / overflow)
static void log_save_all(App* app) {
    ensure_data_dir(app);
    File* file = storage_file_alloc(app->storage);
    if(storage_file_open(file, LOG_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        for(size_t i = 0; i < app->log_count; i++) {
            log_write_line(file, &app->log[i]);
        }
    }
    storage_file_close(file);
    storage_file_free(file);
}

// pushes a new entry (drops the oldest one if the list is full)
static LogEntry* log_push(App* app) {
    if(app->log_count >= LOG_MAX_ENTRIES) {
        memmove(&app->log[0], &app->log[1], sizeof(LogEntry) * (LOG_MAX_ENTRIES - 1));
        app->log_count = LOG_MAX_ENTRIES - 1;
    }
    return &app->log[app->log_count++];
}

static void log_add(App* app, const char* message) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    bool overflow = (app->log_count >= LOG_MAX_ENTRIES);
    LogEntry* e = log_push(app);
    snprintf(
        e->time,
        sizeof(e->time),
        "%02u/%02u %02u:%02u",
        (unsigned)(dt.day % 100),
        (unsigned)(dt.month % 100),
        (unsigned)(dt.hour % 100),
        (unsigned)(dt.minute % 100));
    strncpy(e->msg, message, LOG_MSG_LEN - 1);
    e->msg[LOG_MSG_LEN - 1] = '\0';

    if(overflow) {
        log_save_all(app);
    } else {
        log_append_file(app, e);
    }
}

// line format: "DD/MM HH:MM message"
static void log_parse_line(App* app, const char* line, size_t len) {
    if(len <= LOG_TIME_LEN || line[LOG_TIME_LEN - 1] != ' ') return;
    LogEntry* e = log_push(app);
    memcpy(e->time, line, LOG_TIME_LEN - 1);
    e->time[LOG_TIME_LEN - 1] = '\0';
    strncpy(e->msg, line + LOG_TIME_LEN, LOG_MSG_LEN - 1);
    e->msg[LOG_MSG_LEN - 1] = '\0';
}

static void log_load(App* app) {
    app->log_count = 0;
    File* file = storage_file_alloc(app->storage);
    if(storage_file_open(file, LOG_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t size = storage_file_size(file);
        bool skip_partial = false;
        if(size > LOG_MAX_FILE) {
            storage_file_seek(file, (uint32_t)(size - LOG_MAX_FILE), true);
            skip_partial = true; // we probably landed in the middle of a line
        }

        char line[LOG_TIME_LEN + LOG_MSG_LEN + 4];
        size_t line_len = 0;
        char buf[64];
        uint16_t read;
        while((read = storage_file_read(file, buf, sizeof(buf))) > 0) {
            for(uint16_t i = 0; i < read; i++) {
                char c = buf[i];
                if(c == '\n') {
                    if(!skip_partial) {
                        line[line_len] = '\0';
                        log_parse_line(app, line, line_len);
                    }
                    skip_partial = false;
                    line_len = 0;
                } else if(c != '\r' && line_len < sizeof(line) - 1) {
                    line[line_len++] = c;
                }
            }
        }
    }
    storage_file_close(file);
    storage_file_free(file);
}

// removes one entry from memory; index 0 = newest. Caller saves the file.
static void log_remove(App* app, size_t display_index) {
    if(display_index >= app->log_count) return;
    size_t idx = app->log_count - 1 - display_index;
    memmove(&app->log[idx], &app->log[idx + 1], sizeof(LogEntry) * (app->log_count - 1 - idx));
    app->log_count--;
}

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static void uid_to_string(const uint8_t* uid, uint8_t len, FuriString* out) {
    furi_string_reset(out);
    for(uint8_t i = 0; i < len; i++) {
        furi_string_cat_printf(out, "%02X%s", uid[i], (i + 1 < len) ? " " : "");
    }
}

static bool uid_equal(const uint8_t* a, uint8_t a_len, const uint8_t* b, uint8_t b_len) {
    return (a_len == b_len) && (memcmp(a, b, a_len) == 0);
}

static bool str_contains_nocase(const char* haystack, const char* needle) {
    size_t hl = strlen(haystack), nl = strlen(needle);
    if(nl > hl) return false;
    for(size_t i = 0; i + nl <= hl; i++) {
        size_t k = 0;
        while(k < nl &&
              tolower((unsigned char)haystack[i + k]) == tolower((unsigned char)needle[k]))
            k++;
        if(k == nl) return true;
    }
    return false;
}

static int hex_val(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static uint32_t cache_signature(void) {
    uint32_t h = 2166136261UL ^ 3; // 3 = cache format version
    for(const char* c = MIFARE_FILTER; *c; c++) {
        h = (h ^ (uint8_t)*c) * 16777619UL;
    }
    return h;
}

// draws text on one line, cutting it with "..." if wider than max_w pixels
static void draw_str_fit(Canvas* canvas, uint8_t x, uint8_t y, const char* text, uint8_t max_w) {
    if(canvas_string_width(canvas, text) <= max_w) {
        canvas_draw_str(canvas, x, y, text);
        return;
    }
    char buf[LOG_MSG_LEN + 4];
    size_t len = strlen(text);
    if(len > LOG_MSG_LEN - 1) len = LOG_MSG_LEN - 1;
    while(len > 0) {
        len--;
        memcpy(buf, text, len);
        memcpy(buf + len, "...", 4);
        if(canvas_string_width(canvas, buf) <= max_w) break;
    }
    canvas_draw_str(canvas, x, y, buf);
}

/* ------------------------------------------------------------------ */
/* Progress bar                                                        */
/* ------------------------------------------------------------------ */

static void progress_draw(Canvas* canvas, void* model) {
    ProgressModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 4, AlignCenter, AlignTop, m->title ? m->title : "");

    canvas_set_font(canvas, FontSecondary);
    if(m->total == 0) {
        canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignTop, "Preparing...");
        elements_progress_bar(canvas, 8, 38, 112, 0.0f);
    } else {
        float p = (float)m->done / (float)m->total;
        if(p > 1.0f) p = 1.0f;
        canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignTop, "Comparing tags...");
        elements_progress_bar(canvas, 8, 38, 112, p);

        char buf[40];
        snprintf(
            buf,
            sizeof(buf),
            "%u / %u  (%u%%)",
            (unsigned)m->done,
            (unsigned)m->total,
            (unsigned)(p * 100.0f));
        canvas_draw_str_aligned(canvas, 64, 52, AlignCenter, AlignTop, buf);
    }
}

// thread-safe: can be called from the scan worker
static void progress_set(App* app, uint32_t done, uint32_t total) {
    ProgressModel* m = view_get_model(app->progress_view);
    m->done = done;
    m->total = total;
    view_commit_model(app->progress_view, true);
}

/* ------------------------------------------------------------------ */
/* Cache                                                               */
/* ------------------------------------------------------------------ */

static void cache_load(App* app) {
    if(app->cache_loaded) return;
    app->cache_loaded = true;
    app->cache_count = 0;

    File* file = storage_file_alloc(app->storage);
    if(storage_file_open(file, CACHE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        CacheHeader hdr;
        if(storage_file_read(file, &hdr, sizeof(hdr)) == sizeof(hdr) &&
           hdr.magic == CACHE_MAGIC && hdr.sig == cache_signature() && hdr.count <= MAX_FILES &&
           storage_file_size(file) == sizeof(hdr) + (uint64_t)hdr.count * sizeof(CacheEntry)) {
            uint16_t bytes = (uint16_t)(hdr.count * sizeof(CacheEntry));
            if(bytes == 0 || storage_file_read(file, app->cache, bytes) == bytes) {
                app->cache_count = hdr.count;
            }
        }
    }
    storage_file_close(file);
    storage_file_free(file);
}

static void cache_save(App* app) {
    if(!app->cache_dirty) return;
    ensure_data_dir(app);
    File* file = storage_file_alloc(app->storage);
    if(storage_file_open(file, CACHE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        CacheHeader hdr = {
            .magic = CACHE_MAGIC, .sig = cache_signature(), .count = (uint32_t)app->cache_count};
        storage_file_write(file, &hdr, sizeof(hdr));
        if(app->cache_count) {
            storage_file_write(file, app->cache, app->cache_count * sizeof(CacheEntry));
        }
        app->cache_dirty = false;
    }
    storage_file_close(file);
    storage_file_free(file);
}

static int cache_find(App* app, const char* name, size_t hint) {
    if(hint < app->cache_count && strcmp(app->cache[hint].name, name) == 0) return (int)hint;
    for(size_t i = 0; i < app->cache_count; i++) {
        if(strcmp(app->cache[i].name, name) == 0) return (int)i;
    }
    return -1;
}

/* Reads only the header of a saved .nfc file (single pass, no rewind).
 * Returns true only for Mifare files with a valid UID. */
static bool read_file_uid(
    FlipperFormat* ff,
    FuriString* tmp,
    const char* path,
    uint8_t* uid,
    uint8_t* uid_len) {
    bool ok = false;
    if(flipper_format_file_open_existing(ff, path)) {
        if(flipper_format_read_string(ff, "Device type", tmp) &&
           str_contains_nocase(furi_string_get_cstr(tmp), MIFARE_FILTER) &&
           flipper_format_read_string(ff, "UID", tmp)) {
            const char* c = furi_string_get_cstr(tmp);
            uint8_t n = 0;
            while(*c && n < MAX_UID_LEN) {
                int hi = hex_val(*c);
                if(hi < 0) {
                    c++;
                    continue;
                }
                int lo = hex_val(*(c + 1));
                if(lo < 0) break;
                uid[n++] = (uint8_t)((hi << 4) | lo);
                c += 2;
            }
            if(n > 0) {
                *uid_len = n;
                ok = true;
            }
        }
    }
    flipper_format_file_close(ff);
    return ok;
}

static bool walk_filter(const char* name, FileInfo* info, void* context) {
    UNUSED(context);
    if(file_info_is_dir(info)) return true;
    size_t l = strlen(name);
    return (l > 4) && (strcmp(name + l - 4, ".nfc") == 0);
}

/* Counts the .nfc files in /ext/nfc (only used when there is no cache yet,
 * to know the total for the progress bar). */
static uint32_t count_nfc_files(App* app) {
    DirWalk* walk = dir_walk_alloc(app->storage);
    dir_walk_set_filter_cb(walk, walk_filter, NULL);
    FuriString* path = furi_string_alloc();
    FileInfo info;
    uint32_t n = 0;

    if(dir_walk_open(walk, NFC_DIR)) {
        while(dir_walk_read(walk, path, &info) == DirWalkOK) {
            if(app->cancel) break;
            if(!file_info_is_dir(&info)) n++;
        }
    }

    furi_string_free(path);
    dir_walk_free(walk);
    return n;
}

/* Read fast path: if the tag UID is in the cache and that file is unchanged
 * (same size and modification time) the answer is immediate, no scan needed. */
static bool read_fast_path(App* app) {
    FuriString* path = furi_string_alloc();
    bool found = false;
    for(size_t i = 0; i < app->cache_count && !found; i++) {
        CacheEntry* e = &app->cache[i];
        if(e->uid_len == 0 || !uid_equal(e->uid, e->uid_len, app->read_uid, app->read_uid_len))
            continue;
        furi_string_printf(path, NFC_DIR "/%s", e->name);
        FileInfo fi;
        uint32_t ts = 0;
        if(storage_common_stat(app->storage, furi_string_get_cstr(path), &fi) == FSE_OK &&
           storage_common_timestamp(app->storage, furi_string_get_cstr(path), &ts) == FSE_OK &&
           fi.size == e->size && ts == e->timestamp) {
            strncpy(app->read_name, e->name, NAME_LEN - 1);
            app->read_name[NAME_LEN - 1] = '\0';
            found = true;
        }
    }
    furi_string_free(path);
    return found;
}

/* Walks /ext/nfc and refreshes the cache. Unchanged files are not opened.
 * In Read mode the walk stops at the first file matching the tag just read.
 * Updates the progress bar while walking. */
static bool scan_nfc_files(App* app) {
    if(!storage_dir_exists(app->storage, NFC_DIR)) return false;

    // total for the progress bar: previous cache size, or a quick count if there is no cache
    uint32_t total = (uint32_t)app->cache_count;
    if(total == 0) total = count_nfc_files(app);
    uint32_t done = 0;
    uint32_t last_pct = 101; // forces the first update
    progress_set(app, 0, total);

    for(size_t i = 0; i < app->cache_count; i++) app->cache[i].seen = 0;

    DirWalk* walk = dir_walk_alloc(app->storage);
    dir_walk_set_filter_cb(walk, walk_filter, NULL);
    FuriString* path = furi_string_alloc();
    FuriString* tmp = furi_string_alloc();
    FlipperFormat* ff = flipper_format_file_alloc(app->storage);
    FileInfo info;
    size_t hint = 0;
    bool completed = false;

    if(dir_walk_open(walk, NFC_DIR)) {
        DirWalkResult res;
        while((res = dir_walk_read(walk, path, &info)) == DirWalkOK) {
            if(app->cancel) break;
            if(file_info_is_dir(&info)) continue;

            // progress: files completed so far (the estimate can be too low if new files appeared)
            if(done >= total) total = done + 1;
            uint32_t pct = (uint32_t)(((uint64_t)done * 100) / total);
            if(pct != last_pct) {
                last_pct = pct;
                progress_set(app, done, total);
            }
            done++;

            const char* full = furi_string_get_cstr(path);
            const char* rel = full + strlen(NFC_DIR);
            if(*rel == '/') rel++;

            uint32_t ts = 0;
            storage_common_timestamp(app->storage, full, &ts);

            int idx = cache_find(app, rel, hint);
            CacheEntry* e;
            if(idx >= 0 && app->cache[idx].timestamp == ts &&
               app->cache[idx].size == (uint32_t)info.size) {
                e = &app->cache[idx]; // unchanged: nothing to read
            } else {
                if(idx < 0) {
                    if(app->cache_count >= MAX_FILES) continue;
                    idx = (int)app->cache_count++;
                    strncpy(app->cache[idx].name, rel, NAME_LEN - 1);
                    app->cache[idx].name[NAME_LEN - 1] = '\0';
                }
                e = &app->cache[idx];
                e->uid_len = 0;
                uint8_t len = 0;
                if(read_file_uid(ff, tmp, full, e->uid, &len)) e->uid_len = len;
                e->timestamp = ts;
                e->size = (uint32_t)info.size;
                app->cache_dirty = true;
            }
            e->seen = 1;
            hint = (size_t)idx + 1;

            if(app->task == MenuRead && e->uid_len &&
               uid_equal(e->uid, e->uid_len, app->read_uid, app->read_uid_len)) {
                strncpy(app->read_name, e->name, NAME_LEN - 1);
                app->read_name[NAME_LEN - 1] = '\0';
                app->read_found = true;
                break; // found, no need to scan further
            }
        }
        completed = (res == DirWalkLast);
    }

    if(!app->cancel) progress_set(app, total, total); // 100%

    // drop cache entries of files that no longer exist (only after a full walk)
    if(completed && !app->cancel) {
        size_t w = 0;
        for(size_t i = 0; i < app->cache_count; i++) {
            if(app->cache[i].seen) {
                if(w != i) app->cache[w] = app->cache[i];
                w++;
            }
        }
        if(w != app->cache_count) {
            app->cache_count = w;
            app->cache_dirty = true;
        }
    }

    flipper_format_free(ff);
    furi_string_free(tmp);
    furi_string_free(path);
    dir_walk_free(walk);
    return true;
}

/* ------------------------------------------------------------------ */
/* View switching                                                      */
/* ------------------------------------------------------------------ */

static void switch_view(App* app, AppView view) {
    app->current_view = view;
    view_dispatcher_switch_to_view(app->view_dispatcher, view);
}

static void show_message(App* app, const char* title, const char* body) {
    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, title);
    widget_add_string_multiline_element(
        app->widget, 64, 18, AlignCenter, AlignTop, FontSecondary, body);
    switch_view(app, ViewMessage);
}

static void show_text(App* app, const char* text_content, TextBoxFocus focus, AppView parent) {
    furi_string_set(app->text, text_content);
    text_box_reset(app->text_box);
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text));
    text_box_set_focus(app->text_box, focus);
    app->text_parent = parent;
    switch_view(app, ViewText);
}

static void show_progress(App* app, const char* title) {
    ProgressModel* m = view_get_model(app->progress_view);
    m->title = title;
    m->done = 0;
    m->total = 0;
    view_commit_model(app->progress_view, true);
    switch_view(app, ViewProgress);
}

/* ------------------------------------------------------------------ */
/* Log view (list of searches)                                         */
/* ------------------------------------------------------------------ */

static void log_view_draw(Canvas* canvas, void* model) {
    LogViewModel* m = model;
    App* app = m->app;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    if(app->log_count == 0) {
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignCenter, "Log is empty");
        return;
    }

    for(size_t row = 0; row < LOG_VISIBLE; row++) {
        size_t i = m->top + row;
        if(i >= app->log_count) break;
        const LogEntry* e = &app->log[app->log_count - 1 - i]; // newest first
        uint8_t y = (uint8_t)(row * LOG_ITEM_H);

        if(i == m->pos) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_rbox(canvas, 0, y, 122, LOG_ITEM_H, 2);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }
        canvas_draw_str(canvas, 3, y + 8, e->time);
        draw_str_fit(canvas, 3, y + 16, e->msg, 116);
    }

    canvas_set_color(canvas, ColorBlack);
    elements_scrollbar_pos(canvas, 128, 0, LOG_VISIBLE * LOG_ITEM_H, m->pos, app->log_count);
    canvas_draw_str(canvas, 0, 63, "< Delete");
    const char* hint = "OK: View";
    canvas_draw_str(canvas, 128 - canvas_string_width(canvas, hint), 63, hint);
}

static bool log_view_input(InputEvent* event, void* context) {
    App* app = context;
    if(event->key == InputKeyBack) return false; // handled by navigation callback

    bool short_or_repeat = (event->type == InputTypeShort || event->type == InputTypeRepeat);
    bool consumed = false;
    bool save_file = false;
    bool open_detail = false;
    size_t detail_index = 0;

    LogViewModel* m = view_get_model(app->log_view);
    size_t count = app->log_count;

    if(count > 0) {
        if(event->key == InputKeyUp && short_or_repeat) {
            m->pos = m->pos ? m->pos - 1 : count - 1;
            consumed = true;
        } else if(event->key == InputKeyDown && short_or_repeat) {
            m->pos = (m->pos + 1 < count) ? m->pos + 1 : 0;
            consumed = true;
        } else if(event->key == InputKeyLeft && event->type == InputTypeShort) {
            log_remove(app, m->pos); // deletes only the selected search
            save_file = true;
            consumed = true;
            count = app->log_count;
            if(count == 0) {
                m->pos = 0;
            } else if(m->pos >= count) {
                m->pos = count - 1;
            }
        } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
            open_detail = true;
            detail_index = m->pos;
            consumed = true;
        }

        // keep the selection inside the visible window
        if(m->pos < m->top) m->top = m->pos;
        if(m->pos >= m->top + LOG_VISIBLE) m->top = m->pos - LOG_VISIBLE + 1;
        if(count <= LOG_VISIBLE) {
            m->top = 0;
        } else if(m->top > count - LOG_VISIBLE) {
            m->top = count - LOG_VISIBLE;
        }
    }
    view_commit_model(app->log_view, consumed);

    if(save_file) log_save_all(app);

    if(open_detail && detail_index < app->log_count) {
        const LogEntry* e = &app->log[app->log_count - 1 - detail_index];
        FuriString* s = furi_string_alloc_printf("%s\n%s", e->time, e->msg);
        show_text(app, furi_string_get_cstr(s), TextBoxFocusStart, ViewLog);
        furi_string_free(s);
    }
    return consumed;
}

static void show_log(App* app) {
    LogViewModel* m = view_get_model(app->log_view);
    m->pos = 0;
    m->top = 0;
    view_commit_model(app->log_view, true);
    switch_view(app, ViewLog);
}

/* ------------------------------------------------------------------ */
/* Background scan                                                     */
/* ------------------------------------------------------------------ */

static int32_t scan_worker(void* context) {
    App* app = context;
    cache_load(app);

    app->read_found = false;
    if(app->task == MenuRead && read_fast_path(app)) {
        app->read_found = true;
        app->scan_ok = true;
    } else {
        app->scan_ok = scan_nfc_files(app);
        cache_save(app);
    }

    if(!app->cancel) {
        view_dispatcher_send_custom_event(app->view_dispatcher, EventScanDone);
    }
    return 0;
}

static void stop_scan(App* app) {
    if(app->worker) {
        app->cancel = true;
        furi_thread_join(app->worker);
        furi_thread_free(app->worker);
        app->worker = NULL;
    }
}

static void start_scan(App* app, int task) {
    stop_scan(app);
    app->task = task;
    app->cancel = false;
    app->scan_ok = false;
    app->worker = furi_thread_alloc_ex("TagDupScan", 4 * 1024, scan_worker, app);
    furi_thread_start(app->worker);
}

/* ------------------------------------------------------------------ */
/* NFC reading                                                         */
/* ------------------------------------------------------------------ */

static NfcCommand poller_callback(NfcGenericEvent event, void* context) {
    App* app = context;
    NfcCommand command = NfcCommandContinue;

    if(event.protocol == NfcProtocolIso14443_3a) {
        const Iso14443_3aPollerEvent* ev = event.event_data;
        if(ev->type == Iso14443_3aPollerEventTypeReady) {
            const Iso14443_3aData* data = nfc_poller_get_data(app->poller);
            size_t len = 0;
            const uint8_t* uid = iso14443_3a_get_uid(data, &len);
            if(uid && len > 0 && len <= MAX_UID_LEN) {
                memcpy(app->read_uid, uid, len);
                app->read_uid_len = (uint8_t)len;
                view_dispatcher_send_custom_event(app->view_dispatcher, EventTagRead);
                command = NfcCommandStop;
            }
        }
    }
    return command;
}

static void stop_reading(App* app) {
    if(app->reading) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
        app->poller = NULL;
        app->reading = false;
    }
}

static void start_reading(App* app) {
    show_message(app, "Read", "Place the tag on the\nback of the Flipper...");
    app->poller = nfc_poller_alloc(app->nfc, NfcProtocolIso14443_3a);
    app->reading = true;
    nfc_poller_start(app->poller, poller_callback, app);
}

static void handle_tag_read(App* app) {
    stop_reading(app);
    show_progress(app, "Read");
    start_scan(app, MenuRead);
}

static void finish_read(App* app) {
    FuriString* uid_str = furi_string_alloc();
    uid_to_string(app->read_uid, app->read_uid_len, uid_str);

    FuriString* body = furi_string_alloc();
    FuriString* logmsg = furi_string_alloc();

    bool found = false;
    if(!app->scan_ok) {
        furi_string_printf(body, "UID: %s\nNFC folder not found", furi_string_get_cstr(uid_str));
        furi_string_printf(logmsg, "Read %s: no NFC folder", furi_string_get_cstr(uid_str));
    } else if(app->read_found) {
        found = true;
        furi_string_printf(
            body, "UID: %s\n\nSaved as:\n%s", furi_string_get_cstr(uid_str), app->read_name);
        furi_string_printf(logmsg, "Read %s: %s", furi_string_get_cstr(uid_str), app->read_name);
    } else {
        furi_string_printf(body, "UID: %s\n\nNot found", furi_string_get_cstr(uid_str));
        furi_string_printf(logmsg, "Read %s: not found", furi_string_get_cstr(uid_str));
    }

    log_add(app, furi_string_get_cstr(logmsg));
    show_message(app, "Result", furi_string_get_cstr(body));

    // found: green blinks + vibration + sound / not found: red LED + 1 s vibration
    if(found) {
        notification_message(app->notifications, &sequence_tag_found);
    } else {
        notification_message(app->notifications, &sequence_tag_not_found);
    }

    furi_string_free(logmsg);
    furi_string_free(body);
    furi_string_free(uid_str);
}

/* ------------------------------------------------------------------ */
/* Check duplicate                                                     */
/* ------------------------------------------------------------------ */

static void finish_check(App* app) {
    if(!app->scan_ok) {
        log_add(app, "Check: no NFC folder");
        show_message(app, "Error", "NFC folder not found");
        return;
    }

    size_t n = app->cache_count;
    FuriString* out = furi_string_alloc();
    FuriString* uid_str = furi_string_alloc();
    bool* done = malloc(n ? n : 1);
    memset(done, 0, n ? n : 1);
    size_t groups = 0, mifare_files = 0;

    for(size_t i = 0; i < n; i++) {
        CacheEntry* a = &app->cache[i];
        if(a->uid_len == 0) continue;
        mifare_files++;
        if(done[i]) continue;
        bool header = false;

        for(size_t j = i + 1; j < n; j++) {
            CacheEntry* b = &app->cache[j];
            if(done[j] || b->uid_len == 0) continue;
            if(!uid_equal(a->uid, a->uid_len, b->uid, b->uid_len)) continue;

            if(!header) {
                uid_to_string(a->uid, a->uid_len, uid_str);
                furi_string_cat_printf(
                    out, "UID %s\n- %s\n", furi_string_get_cstr(uid_str), a->name);
                header = true;
                groups++;
            }
            furi_string_cat_printf(out, "- %s\n", b->name);
            done[j] = true;
        }
        if(header) furi_string_cat(out, "\n");
    }

    FuriString* logmsg = furi_string_alloc();
    if(groups == 0) {
        furi_string_printf(
            out, "No duplicates found.\n\nFiles checked: %u", (unsigned)mifare_files);
        furi_string_printf(logmsg, "Check: no duplicates (%u files)", (unsigned)mifare_files);
    } else {
        furi_string_printf(
            logmsg,
            "Check: %u duplicate group(s) in %u files",
            (unsigned)groups,
            (unsigned)mifare_files);
    }
    log_add(app, furi_string_get_cstr(logmsg));
    show_text(app, furi_string_get_cstr(out), TextBoxFocusStart, ViewMain);

    furi_string_free(logmsg);
    free(done);
    furi_string_free(uid_str);
    furi_string_free(out);
}

/* ------------------------------------------------------------------ */
/* Callbacks                                                           */
/* ------------------------------------------------------------------ */

static void submenu_callback(void* context, uint32_t index) {
    App* app = context;
    switch(index) {
    case MenuRead:
        start_reading(app);
        break;
    case MenuCheckDuplicate:
        show_progress(app, "Check duplicate");
        start_scan(app, MenuCheckDuplicate);
        break;
    case MenuLog:
        show_log(app);
        break;
    case MenuAbout:
        show_message(app, "TagDuplicate", "v1.0\ncreated by Marcopiiit");
        break;
    default:
        break;
    }
}

static bool custom_event_callback(void* context, uint32_t event) {
    App* app = context;
    if(event == EventTagRead) {
        handle_tag_read(app);
        return true;
    }
    if(event == EventScanDone) {
        if(!app->worker) return true; // cancelled
        stop_scan(app);
        if(app->task == MenuRead) {
            finish_read(app);
        } else {
            finish_check(app);
        }
        return true;
    }
    return false;
}

static bool navigation_callback(void* context) {
    App* app = context;
    switch(app->current_view) {
    case ViewMain:
        view_dispatcher_stop(app->view_dispatcher);
        break;
    case ViewText:
        switch_view(app, app->text_parent);
        break;
    default: // ViewMessage, ViewLog, ViewProgress
        stop_reading(app);
        stop_scan(app);
        switch_view(app, ViewMain);
        break;
    }
    return true;
}

/* ------------------------------------------------------------------ */
/* Entry point                                                         */
/* ------------------------------------------------------------------ */

int32_t tag_duplicate_app(void* p) {
    UNUSED(p);

    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));
    app->cache = malloc(sizeof(CacheEntry) * MAX_FILES);
    app->log = malloc(sizeof(LogEntry) * LOG_MAX_ENTRIES);
    app->text = furi_string_alloc();
    app->storage = furi_record_open(RECORD_STORAGE);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->nfc = nfc_alloc();

    log_load(app);

    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, navigation_callback);

    app->submenu = submenu_alloc();
    submenu_set_header(app->submenu, "TagDuplicate");
    submenu_add_item(app->submenu, "Read", MenuRead, submenu_callback, app);
    submenu_add_item(app->submenu, "Check duplicate", MenuCheckDuplicate, submenu_callback, app);
    submenu_add_item(app->submenu, "Log", MenuLog, submenu_callback, app);
    submenu_add_item(app->submenu, "About", MenuAbout, submenu_callback, app);

    app->widget = widget_alloc();
    app->text_box = text_box_alloc();

    app->log_view = view_alloc();
    view_allocate_model(app->log_view, ViewModelTypeLocking, sizeof(LogViewModel));
    view_set_context(app->log_view, app);
    view_set_draw_callback(app->log_view, log_view_draw);
    view_set_input_callback(app->log_view, log_view_input);
    LogViewModel* lm = view_get_model(app->log_view);
    lm->app = app;
    lm->pos = 0;
    lm->top = 0;
    view_commit_model(app->log_view, false);

    app->progress_view = view_alloc();
    view_allocate_model(app->progress_view, ViewModelTypeLocking, sizeof(ProgressModel));
    view_set_draw_callback(app->progress_view, progress_draw);

    view_dispatcher_add_view(app->view_dispatcher, ViewMain, submenu_get_view(app->submenu));
    view_dispatcher_add_view(app->view_dispatcher, ViewMessage, widget_get_view(app->widget));
    view_dispatcher_add_view(app->view_dispatcher, ViewText, text_box_get_view(app->text_box));
    view_dispatcher_add_view(app->view_dispatcher, ViewLog, app->log_view);
    view_dispatcher_add_view(app->view_dispatcher, ViewProgress, app->progress_view);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    switch_view(app, ViewMain);

    view_dispatcher_run(app->view_dispatcher);

    // cleanup
    stop_reading(app);
    stop_scan(app);
    view_dispatcher_remove_view(app->view_dispatcher, ViewMain);
    view_dispatcher_remove_view(app->view_dispatcher, ViewMessage);
    view_dispatcher_remove_view(app->view_dispatcher, ViewText);
    view_dispatcher_remove_view(app->view_dispatcher, ViewLog);
    view_dispatcher_remove_view(app->view_dispatcher, ViewProgress);
    submenu_free(app->submenu);
    widget_free(app->widget);
    text_box_free(app->text_box);
    view_free(app->log_view);
    view_free(app->progress_view);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);

    nfc_free(app->nfc);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(app->text);
    free(app->log);
    free(app->cache);
    free(app);

    return 0;
}
