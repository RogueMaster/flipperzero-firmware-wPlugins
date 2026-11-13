#include "keytool_cache.h"
#include "keytool_log.h"
#include <gui/view.h>
#include <gui/canvas.h>
#include <input/input.h>
#include <stream/stream.h>
#include <stream/file_stream.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define KEYTOOL_MAX_CACHE_ENTRIES 64
#define KEYTOOL_CACHE_VISIBLE_ROWS 4
#define KEYTOOL_CACHE_ROW_HEIGHT 11
#define KEYTOOL_CACHE_HEADER "# Dump: "

typedef struct {
    FuriString* filename; // basename only, inside KEYTOOL_CACHE_FOLDER
    FuriString* dump_name;
    size_t key_count;
} KeyToolCacheEntry;

typedef struct {
    KeyToolCacheEntry entries[KEYTOOL_MAX_CACHE_ENTRIES];
    size_t count;
    size_t selected;
    size_t scroll;
    bool confirm_import;
    bool confirm_delete;
    bool show_notice;
    bool notice_error;
    size_t notice_added;
    size_t notice_skipped;
} KeyToolCacheListModel;

// ---------------------------------------------------------------------
// Key line parsing: "AABBCCDDEEFF" optionally followed by " # comment".
// Lines starting with '#' (or anything not 12 hex digits) are ignored.
// ---------------------------------------------------------------------

static bool keytool_cache_parse_key(const char* line, uint8_t* out) {
    while(*line == ' ' || *line == '\t') line++;
    for(int i = 0; i < 6; i++) {
        int value = 0;
        for(int n = 0; n < 2; n++) {
            char c = *line++;
            int d;
            if(c >= '0' && c <= '9') {
                d = c - '0';
            } else if(c >= 'a' && c <= 'f') {
                d = c - 'a' + 10;
            } else if(c >= 'A' && c <= 'F') {
                d = c - 'A' + 10;
            } else {
                return false;
            }
            value = (value << 4) | d;
        }
        out[i] = (uint8_t)value;
    }
    char c = *line;
    return c == '\0' || c == ' ' || c == '\t' || c == '#' || c == '\r' || c == '\n';
}

// ---------------------------------------------------------------------
// Save (called after every scan)
// ---------------------------------------------------------------------

void keytool_cache_save(
    Storage* storage,
    const char* dump_name,
    const KeyToolKeyEntry* keys,
    size_t key_count,
    const bool* found_matrix,
    size_t dict_count) {
    FuriString* safe_name = furi_string_alloc();
    for(const char* p = dump_name; *p && furi_string_size(safe_name) < 40; p++) {
        char c = *p;
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                  c == '-' || c == '_' || c == ' ';
        furi_string_cat_printf(safe_name, "%c", ok ? c : '_');
    }
    if(furi_string_empty(safe_name)) furi_string_set_str(safe_name, "card");

    FuriString* path = furi_string_alloc_printf(
        "%s/%s.txt", KEYTOOL_CACHE_FOLDER, furi_string_get_cstr(safe_name));

    bool unfound[KEYTOOL_MAX_KEYS];
    size_t unfound_count = 0;
    for(size_t k = 0; k < key_count && k < KEYTOOL_MAX_KEYS; k++) {
        bool any = false;
        for(size_t d = 0; d < dict_count; d++) {
            if(found_matrix[k * dict_count + d]) {
                any = true;
                break;
            }
        }
        unfound[k] = !any;
        if(!any) unfound_count++;
    }

    if(unfound_count == 0) {
        // Everything is already in a dictionary: nothing to keep.
        storage_common_remove(storage, furi_string_get_cstr(path));
    } else {
        storage_simply_mkdir(storage, KEYTOOL_CACHE_FOLDER);

        Stream* stream = file_stream_alloc(storage);
        if(file_stream_open(
               stream, furi_string_get_cstr(path), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            stream_write_cstring(stream, KEYTOOL_CACHE_HEADER);
            stream_write_cstring(stream, dump_name);
            stream_write_cstring(stream, "\n");

            FuriString* line = furi_string_alloc();
            for(size_t k = 0; k < key_count && k < KEYTOOL_MAX_KEYS; k++) {
                if(!unfound[k]) continue;

                // Same key used by several sectors: write it only once.
                bool duplicate = false;
                for(size_t j = 0; j < k; j++) {
                    if(unfound[j] && memcmp(keys[j].bytes, keys[k].bytes, 6) == 0) {
                        duplicate = true;
                        break;
                    }
                }
                if(duplicate) continue;

                furi_string_reset(line);
                for(int b = 0; b < 6; b++) {
                    furi_string_cat_printf(line, "%02X", keys[k].bytes[b]);
                }
                furi_string_cat_str(line, " #");
                for(size_t j = k; j < key_count && j < KEYTOOL_MAX_KEYS; j++) {
                    if(unfound[j] && memcmp(keys[j].bytes, keys[k].bytes, 6) == 0) {
                        furi_string_cat_printf(
                            line, " S%lu-%c", (unsigned long)keys[j].sector, keys[j].type);
                    }
                }
                furi_string_cat_str(line, "\n");
                stream_write_cstring(stream, furi_string_get_cstr(line));
            }
            furi_string_free(line);
        }
        file_stream_close(stream);
        stream_free(stream);
    }

    furi_string_free(path);
    furi_string_free(safe_name);
}

// ---------------------------------------------------------------------
// Import one cache file into the user dictionary
// ---------------------------------------------------------------------

static bool keytool_cache_import_file(
    Storage* storage,
    const char* cache_path,
    size_t* added_out,
    size_t* skipped_out) {
    *added_out = 0;
    *skipped_out = 0;

    uint8_t keys[KEYTOOL_MAX_KEYS][6];
    bool present[KEYTOOL_MAX_KEYS];
    size_t n = 0;

    // 1) Read the keys out of the cache file.
    Stream* stream = file_stream_alloc(storage);
    if(!file_stream_open(stream, cache_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        stream_free(stream);
        return false;
    }
    FuriString* line = furi_string_alloc();
    while(n < KEYTOOL_MAX_KEYS && stream_read_line(stream, line)) {
        if(keytool_cache_parse_key(furi_string_get_cstr(line), keys[n])) {
            present[n] = false;
            n++;
        }
    }
    file_stream_close(stream);
    stream_free(stream);

    // 2) Mark the ones already in the user dictionary.
    storage_simply_mkdir(storage, "/ext/nfc/assets");
    bool needs_newline = false;

    stream = file_stream_alloc(storage);
    if(file_stream_open(stream, KEYTOOL_USER_DICT_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint8_t existing[6];
        while(stream_read_line(stream, line)) {
            if(!keytool_cache_parse_key(furi_string_get_cstr(line), existing)) continue;
            for(size_t i = 0; i < n; i++) {
                if(memcmp(keys[i], existing, 6) == 0) present[i] = true;
            }
        }
    }
    file_stream_close(stream);
    stream_free(stream);
    furi_string_free(line);

    // 3) If the user dictionary doesn't end with a newline, add one first
    //    so the first imported key doesn't get glued to the last line.
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, KEYTOOL_USER_DICT_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t size = storage_file_size(file);
        if(size > 0 && storage_file_seek(file, (uint32_t)(size - 1), true)) {
            char last = 0;
            if(storage_file_read(file, &last, 1) == 1 && last != '\n') needs_newline = true;
        }
    }
    storage_file_close(file);
    storage_file_free(file);

    // 4) Append the missing keys.
    stream = file_stream_alloc(storage);
    if(!file_stream_open(stream, KEYTOOL_USER_DICT_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        file_stream_close(stream);
        stream_free(stream);
        return false;
    }
    if(needs_newline) stream_write_cstring(stream, "\n");

    FuriString* out = furi_string_alloc();
    for(size_t i = 0; i < n; i++) {
        if(present[i]) {
            (*skipped_out)++;
            continue;
        }
        furi_string_reset(out);
        for(int b = 0; b < 6; b++) furi_string_cat_printf(out, "%02X", keys[i][b]);
        furi_string_cat_str(out, "\n");
        stream_write_cstring(stream, furi_string_get_cstr(out));
        (*added_out)++;
    }
    furi_string_free(out);

    file_stream_close(stream);
    stream_free(stream);
    return true;
}

// ---------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------

static void keytool_cache_list_draw_callback(Canvas* canvas, void* model_ptr) {
    KeyToolCacheListModel* model = model_ptr;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(model->show_notice) {
        canvas_set_font(canvas, FontPrimary);
        if(model->notice_error) {
            canvas_draw_str(canvas, 4, 12, "Import failed");
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 4, 28, "Could not write the");
            canvas_draw_str(canvas, 4, 40, "user dictionary.");
        } else {
            canvas_draw_str(canvas, 4, 12, "Import done");
            canvas_set_font(canvas, FontSecondary);
            FuriString* a = furi_string_alloc_printf("%zu key(s) added", model->notice_added);
            FuriString* s =
                furi_string_alloc_printf("%zu already present", model->notice_skipped);
            canvas_draw_str(canvas, 4, 28, furi_string_get_cstr(a));
            canvas_draw_str(canvas, 4, 40, furi_string_get_cstr(s));
            furi_string_free(a);
            furi_string_free(s);
        }
        canvas_draw_str(canvas, 4, 58, "Press any key");
        return;
    }

    if(model->confirm_delete && model->count > 0 && model->selected < model->count) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 4, 12, "Delete this cache?");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(
            canvas, 4, 26, furi_string_get_cstr(model->entries[model->selected].dump_name));
        FuriString* info = furi_string_alloc_printf(
            "%zu key(s) will be lost", model->entries[model->selected].key_count);
        canvas_draw_str(canvas, 4, 38, furi_string_get_cstr(info));
        furi_string_free(info);
        canvas_draw_str(canvas, 4, 56, "OK = Yes      BACK = No");
        return;
    }

    if(model->confirm_import && model->count > 0 && model->selected < model->count) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 4, 12, "Import these keys?");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(
            canvas, 4, 26, furi_string_get_cstr(model->entries[model->selected].dump_name));
        FuriString* info = furi_string_alloc_printf(
            "%zu key(s) -> user dict", model->entries[model->selected].key_count);
        canvas_draw_str(canvas, 4, 38, furi_string_get_cstr(info));
        furi_string_free(info);
        canvas_draw_str(canvas, 4, 56, "OK = Yes      BACK = No");
        return;
    }

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Cache");
    canvas_set_font(canvas, FontSecondary);

    if(model->count == 0) {
        canvas_draw_str(canvas, 4, 28, "Cache is empty.");
        canvas_draw_str(canvas, 4, 40, "Missing keys show up");
        canvas_draw_str(canvas, 4, 50, "here after a scan.");
        return;
    }

    size_t y = 22;
    for(size_t i = model->scroll;
        i < model->count && i < model->scroll + KEYTOOL_CACHE_VISIBLE_ROWS;
        i++) {
        bool selected = (i == model->selected);
        if(selected) {
            canvas_draw_box(canvas, 0, y - 9, 128, KEYTOOL_CACHE_ROW_HEIGHT);
            canvas_set_color(canvas, ColorWhite);
        }
        FuriString* row = furi_string_alloc_printf(
            "%s (%zu)", furi_string_get_cstr(model->entries[i].dump_name), model->entries[i].key_count);
        canvas_draw_str(canvas, 3, y, furi_string_get_cstr(row));
        furi_string_free(row);
        if(selected) canvas_set_color(canvas, ColorBlack);
        y += KEYTOOL_CACHE_ROW_HEIGHT;
    }

    canvas_draw_str(canvas, 2, 63, "OK view  L del  Hold imp");
}

// ---------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------

static void keytool_cache_list_clamp_scroll(KeyToolCacheListModel* model) {
    if(model->selected < model->scroll) model->scroll = model->selected;
    if(model->count > 0 && model->selected >= model->scroll + KEYTOOL_CACHE_VISIBLE_ROWS) {
        model->scroll = model->selected - KEYTOOL_CACHE_VISIBLE_ROWS + 1;
    }
}

static bool keytool_cache_list_input_callback(InputEvent* event, void* context) {
    KeyToolApp* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeLong) return false;

    View* view = app->cache_view;
    KeyToolCacheListModel* model = view_get_model(view);

    // Result screen after an import: any key closes it.
    if(model->show_notice) {
        model->show_notice = false;
        view_commit_model(view, true);
        return true;
    }

    // Delete confirmation: OK removes this cache file, BACK cancels.
    if(model->confirm_delete) {
        if(event->type == InputTypeShort && event->key == InputKeyOk) {
            if(model->count > 0 && model->selected < model->count) {
                FuriString* full_path = furi_string_alloc_printf(
                    "%s/%s",
                    KEYTOOL_CACHE_FOLDER,
                    furi_string_get_cstr(model->entries[model->selected].filename));
                storage_common_remove(app->storage, furi_string_get_cstr(full_path));
                furi_string_free(full_path);
            }
            model->confirm_delete = false;
            view_commit_model(view, false);
            keytool_cache_list_refresh(app);
            return true;
        }
        if(event->type == InputTypeShort && event->key == InputKeyBack) {
            model->confirm_delete = false;
            view_commit_model(view, true);
            return true;
        }
        view_commit_model(view, false);
        return true;
    }

    // Confirmation screen: OK imports, BACK cancels.
    if(model->confirm_import) {
        if(event->type == InputTypeShort && event->key == InputKeyOk) {
            FuriString* full_path = furi_string_alloc();
            if(model->count > 0 && model->selected < model->count) {
                furi_string_printf(
                    full_path,
                    "%s/%s",
                    KEYTOOL_CACHE_FOLDER,
                    furi_string_get_cstr(model->entries[model->selected].filename));
            }
            model->confirm_import = false;
            view_commit_model(view, false);

            size_t added = 0, skipped = 0;
            bool ok = false;
            if(!furi_string_empty(full_path)) {
                ok = keytool_cache_import_file(
                    app->storage, furi_string_get_cstr(full_path), &added, &skipped);
                // Keys now live in the dictionary: drop them from the cache.
                if(ok) storage_common_remove(app->storage, furi_string_get_cstr(full_path));
            }
            furi_string_free(full_path);

            keytool_cache_list_refresh(app);

            model = view_get_model(view);
            model->show_notice = true;
            model->notice_error = !ok;
            model->notice_added = added;
            model->notice_skipped = skipped;
            view_commit_model(view, true);
            return true;
        }
        if(event->type == InputTypeShort && event->key == InputKeyBack) {
            model->confirm_import = false;
            view_commit_model(view, true);
            return true;
        }
        view_commit_model(view, false);
        return true;
    }

    // Long press on OK: ask before importing.
    if(event->type == InputTypeLong) {
        if(event->key == InputKeyOk && model->count > 0 && model->selected < model->count) {
            model->confirm_import = true;
            view_commit_model(view, true);
            return true;
        }
        view_commit_model(view, false);
        return false;
    }

    switch(event->key) {
    case InputKeyUp:
        if(model->count > 0) {
            model->selected = (model->selected == 0) ? model->count - 1 : model->selected - 1;
            keytool_cache_list_clamp_scroll(model);
        }
        view_commit_model(view, true);
        return true;

    case InputKeyDown:
        if(model->count > 0) {
            model->selected = (model->selected + 1) % model->count;
            keytool_cache_list_clamp_scroll(model);
        }
        view_commit_model(view, true);
        return true;

    case InputKeyLeft:
        if(model->count > 0) model->confirm_delete = true;
        view_commit_model(view, true);
        return true;

    case InputKeyOk:
        if(model->count > 0 && model->selected < model->count) {
            FuriString* full_path = furi_string_alloc_printf(
                "%s/%s",
                KEYTOOL_CACHE_FOLDER,
                furi_string_get_cstr(model->entries[model->selected].filename));
            view_commit_model(view, false);

            keytool_log_read_file_to_string(
                app->storage, furi_string_get_cstr(full_path), app->log_text);
            furi_string_free(full_path);

            app->text_box_return_view = KeyToolViewCacheList;
            text_box_set_text(app->text_box, furi_string_get_cstr(app->log_text));
            view_dispatcher_switch_to_view(app->view_dispatcher, KeyToolViewLogDetail);
            return true;
        }
        view_commit_model(view, false);
        return true;

    default:
        break;
    }

    view_commit_model(view, false);
    return false; // BACK propagates to the previous_callback (-> main menu)
}

static uint32_t keytool_cache_list_back_callback(void* context) {
    UNUSED(context);
    return KeyToolViewSubmenu;
}

// ---------------------------------------------------------------------
// Refresh (rescan KEYTOOL_CACHE_FOLDER)
// ---------------------------------------------------------------------

static bool keytool_cache_is_txt(const char* name) {
    size_t len = strlen(name);
    return len > 4 && strcasecmp(name + (len - 4), ".txt") == 0;
}

void keytool_cache_list_refresh(KeyToolApp* app) {
    Storage* storage = app->storage;
    storage_simply_mkdir(storage, KEYTOOL_CACHE_FOLDER);

    FuriString* names[KEYTOOL_MAX_CACHE_ENTRIES];
    for(size_t i = 0; i < KEYTOOL_MAX_CACHE_ENTRIES; i++) names[i] = furi_string_alloc();
    size_t count = 0;

    File* dir = storage_file_alloc(storage);
    if(storage_dir_open(dir, KEYTOOL_CACHE_FOLDER)) {
        FileInfo info;
        char name_buf[256];
        while(count < KEYTOOL_MAX_CACHE_ENTRIES &&
              storage_dir_read(dir, &info, name_buf, sizeof(name_buf))) {
            if(info.flags & FSF_DIRECTORY) continue;
            if(!keytool_cache_is_txt(name_buf)) continue;
            furi_string_set_str(names[count], name_buf);
            count++;
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);

    // Alphabetical order by dump name (file name).
    for(size_t i = 1; i < count; i++) {
        for(size_t j = i;
            j > 0 &&
            strcasecmp(furi_string_get_cstr(names[j]), furi_string_get_cstr(names[j - 1])) < 0;
            j--) {
            FuriString* tmp = names[j];
            names[j] = names[j - 1];
            names[j - 1] = tmp;
        }
    }

    KeyToolCacheListModel* model = view_get_model(app->cache_view);
    model->count = count;
    model->selected = 0;
    model->scroll = 0;
    model->confirm_import = false;
    model->confirm_delete = false;
    model->show_notice = false;

    for(size_t i = 0; i < count; i++) {
        furi_string_set(model->entries[i].filename, names[i]);
        model->entries[i].key_count = 0;

        // Default label = file name without ".txt"; the "# Dump: " header
        // (written by keytool_cache_save) holds the original dump name.
        furi_string_set(model->entries[i].dump_name, names[i]);
        furi_string_left(model->entries[i].dump_name, furi_string_size(names[i]) - 4);

        FuriString* full_path = furi_string_alloc_printf(
            "%s/%s", KEYTOOL_CACHE_FOLDER, furi_string_get_cstr(names[i]));
        Stream* stream = file_stream_alloc(storage);
        if(file_stream_open(
               stream, furi_string_get_cstr(full_path), FSAM_READ, FSOM_OPEN_EXISTING)) {
            FuriString* line = furi_string_alloc();
            while(stream_read_line(stream, line)) {
                const char* cline = furi_string_get_cstr(line);
                if(strncmp(cline, KEYTOOL_CACHE_HEADER, strlen(KEYTOOL_CACHE_HEADER)) == 0) {
                    furi_string_set_str(
                        model->entries[i].dump_name, cline + strlen(KEYTOOL_CACHE_HEADER));
                    furi_string_trim(model->entries[i].dump_name);
                    continue;
                }
                uint8_t tmp_key[6];
                if(keytool_cache_parse_key(cline, tmp_key)) model->entries[i].key_count++;
            }
            furi_string_free(line);
        }
        file_stream_close(stream);
        stream_free(stream);
        furi_string_free(full_path);
    }

    view_commit_model(app->cache_view, true);

    for(size_t i = 0; i < KEYTOOL_MAX_CACHE_ENTRIES; i++) furi_string_free(names[i]);
}

// ---------------------------------------------------------------------
// Alloc / free
// ---------------------------------------------------------------------

View* keytool_cache_list_alloc(KeyToolApp* app) {
    View* view = view_alloc();
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(KeyToolCacheListModel));
    view_set_draw_callback(view, keytool_cache_list_draw_callback);
    view_set_input_callback(view, keytool_cache_list_input_callback);
    view_set_previous_callback(view, keytool_cache_list_back_callback);

    KeyToolCacheListModel* model = view_get_model(view);
    for(size_t i = 0; i < KEYTOOL_MAX_CACHE_ENTRIES; i++) {
        model->entries[i].filename = furi_string_alloc();
        model->entries[i].dump_name = furi_string_alloc();
        model->entries[i].key_count = 0;
    }
    model->count = 0;
    model->selected = 0;
    model->scroll = 0;
    model->confirm_import = false;
    model->confirm_delete = false;
    model->show_notice = false;
    model->notice_error = false;
    model->notice_added = 0;
    model->notice_skipped = 0;
    view_commit_model(view, false);

    return view;
}

void keytool_cache_list_free(View* view) {
    KeyToolCacheListModel* model = view_get_model(view);
    for(size_t i = 0; i < KEYTOOL_MAX_CACHE_ENTRIES; i++) {
        furi_string_free(model->entries[i].filename);
        furi_string_free(model->entries[i].dump_name);
    }
    view_commit_model(view, false);
    view_free(view);
}
