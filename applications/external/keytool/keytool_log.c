#include "keytool_log.h"
#include <gui/view.h>
#include <gui/canvas.h>
#include <input/input.h>
#include <stream/stream.h>
#include <stream/file_stream.h>
#include <furi_hal_rtc.h>
#include <datetime/datetime.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define KEYTOOL_MAX_LOG_ENTRIES 64
#define KEYTOOL_VISIBLE_ROWS 4
#define KEYTOOL_ROW_HEIGHT 11

typedef struct {
    FuriString* filename; // basename only, inside KEYTOOL_LOGS_FOLDER
    FuriString* card_name;
    FuriString* date_str;
} KeyToolLogEntry;

typedef struct {
    KeyToolLogEntry entries[KEYTOOL_MAX_LOG_ENTRIES];
    size_t count;
    size_t selected;
    size_t scroll;
    bool confirm_delete;
} KeyToolLogListModel;

// ---------------------------------------------------------------------
// Date/time helpers
// ---------------------------------------------------------------------

void keytool_log_datetime_now(FuriString* display_out, FuriString* prefix_out) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    furi_string_printf(
        display_out,
        "%02u/%02u/%04u %02u:%02u",
        (unsigned)dt.day,
        (unsigned)dt.month,
        (unsigned)dt.year,
        (unsigned)dt.hour,
        (unsigned)dt.minute);
    furi_string_printf(
        prefix_out,
        "%04u%02u%02u_%02u%02u%02u",
        (unsigned)dt.year,
        (unsigned)dt.month,
        (unsigned)dt.day,
        (unsigned)dt.hour,
        (unsigned)dt.minute,
        (unsigned)dt.second);
}

// ---------------------------------------------------------------------
// Save / read one log entry
// ---------------------------------------------------------------------

void keytool_log_save_entry(
    Storage* storage,
    const char* card_name,
    const char* date_display,
    const char* filename_prefix,
    FuriString* body) {
    storage_common_mkdir(storage, KEYTOOL_LOGS_FOLDER);

    FuriString* safe_name = furi_string_alloc();
    for(const char* p = card_name; *p && furi_string_size(safe_name) < 40; p++) {
        char c = *p;
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                  c == '-' || c == '_' || c == ' ';
        furi_string_cat_printf(safe_name, "%c", ok ? c : '_');
    }
    if(furi_string_empty(safe_name)) furi_string_set_str(safe_name, "card");

    FuriString* path = furi_string_alloc_printf(
        "%s/%s_%s.txt", KEYTOOL_LOGS_FOLDER, filename_prefix, furi_string_get_cstr(safe_name));

    Stream* stream = file_stream_alloc(storage);
    if(file_stream_open(stream, furi_string_get_cstr(path), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        stream_write_cstring(stream, "Card: ");
        stream_write_cstring(stream, card_name);
        stream_write_cstring(stream, "\n");
        stream_write_cstring(stream, "Date: ");
        stream_write_cstring(stream, date_display);
        stream_write_cstring(stream, "\n\n");
        stream_write_cstring(stream, furi_string_get_cstr(body));
    }
    file_stream_close(stream);
    stream_free(stream);

    furi_string_free(path);
    furi_string_free(safe_name);
}

void keytool_log_read_file_to_string(Storage* storage, const char* path, FuriString* out) {
    furi_string_reset(out);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint8_t buf[128];
        uint16_t read_bytes;
        do {
            read_bytes = storage_file_read(file, buf, sizeof(buf));
            if(read_bytes > 0) {
                furi_string_cat_printf(out, "%.*s", (int)read_bytes, (const char*)buf);
            }
        } while(read_bytes > 0);
    } else {
        furi_string_set_str(out, "Could not read this log file.");
    }
    storage_file_close(file);
    storage_file_free(file);
}

// ---------------------------------------------------------------------
// List view: draw
// ---------------------------------------------------------------------

static void keytool_log_list_draw_callback(Canvas* canvas, void* model_ptr) {
    KeyToolLogListModel* model = model_ptr;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(model->confirm_delete && model->count > 0 && model->selected < model->count) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 4, 12, "Delete this log?");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(
            canvas, 4, 26, furi_string_get_cstr(model->entries[model->selected].card_name));
        canvas_draw_str(
            canvas, 4, 38, furi_string_get_cstr(model->entries[model->selected].date_str));
        canvas_draw_str(canvas, 4, 56, "OK = Yes      BACK = No");
        return;
    }

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Saved logs");
    canvas_set_font(canvas, FontSecondary);

    if(model->count == 0) {
        canvas_draw_str(canvas, 4, 28, "No saved logs yet.");
        canvas_draw_str(canvas, 4, 40, "Run Start first.");
        return;
    }

    size_t y = 22;
    for(size_t i = model->scroll;
        i < model->count && i < model->scroll + KEYTOOL_VISIBLE_ROWS;
        i++) {
        bool selected = (i == model->selected);
        if(selected) {
            canvas_draw_box(canvas, 0, y - 9, 128, KEYTOOL_ROW_HEIGHT);
            canvas_set_color(canvas, ColorWhite);
        }
        FuriString* row = furi_string_alloc_printf(
            "%s - %s",
            furi_string_get_cstr(model->entries[i].card_name),
            furi_string_get_cstr(model->entries[i].date_str));
        canvas_draw_str(canvas, 3, y, furi_string_get_cstr(row));
        furi_string_free(row);
        if(selected) canvas_set_color(canvas, ColorBlack);
        y += KEYTOOL_ROW_HEIGHT;
    }

    canvas_draw_str(canvas, 2, 63, "OK open  LEFT delete");
}

// ---------------------------------------------------------------------
// List view: input
// ---------------------------------------------------------------------

static void keytool_log_list_clamp_scroll(KeyToolLogListModel* model) {
    if(model->selected < model->scroll) model->scroll = model->selected;
    if(model->count > 0 && model->selected >= model->scroll + KEYTOOL_VISIBLE_ROWS) {
        model->scroll = model->selected - KEYTOOL_VISIBLE_ROWS + 1;
    }
}

static bool keytool_log_list_input_callback(InputEvent* event, void* context) {
    KeyToolApp* app = context;
    if(event->type != InputTypeShort) return false;

    View* view = app->log_list_view;
    KeyToolLogListModel* model = view_get_model(view);

    if(model->confirm_delete) {
        if(event->key == InputKeyOk) {
            if(model->count > 0 && model->selected < model->count) {
                FuriString* full_path = furi_string_alloc_printf(
                    "%s/%s",
                    KEYTOOL_LOGS_FOLDER,
                    furi_string_get_cstr(model->entries[model->selected].filename));
                storage_common_remove(app->storage, furi_string_get_cstr(full_path));
                furi_string_free(full_path);
            }
            model->confirm_delete = false;
            view_commit_model(view, false);
            keytool_log_list_refresh(app);
            return true;
        }
        if(event->key == InputKeyBack) {
            model->confirm_delete = false;
            view_commit_model(view, true);
            return true;
        }
        view_commit_model(view, false);
        return true;
    }

    switch(event->key) {
    case InputKeyUp:
        if(model->count > 0) {
            model->selected = (model->selected == 0) ? model->count - 1 : model->selected - 1;
            keytool_log_list_clamp_scroll(model);
        }
        view_commit_model(view, true);
        return true;

    case InputKeyDown:
        if(model->count > 0) {
            model->selected = (model->selected + 1) % model->count;
            keytool_log_list_clamp_scroll(model);
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
                KEYTOOL_LOGS_FOLDER,
                furi_string_get_cstr(model->entries[model->selected].filename));
            keytool_log_read_file_to_string(
                app->storage, furi_string_get_cstr(full_path), app->log_text);
            furi_string_free(full_path);
            view_commit_model(view, false);

            app->text_box_return_view = KeyToolViewLogList;
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
    return false; // let BACK propagate to the previous_callback (-> main menu)
}

static uint32_t keytool_log_list_back_callback(void* context) {
    UNUSED(context);
    return KeyToolViewSubmenu;
}

// ---------------------------------------------------------------------
// Refresh (rescan KEYTOOL_LOGS_FOLDER)
// ---------------------------------------------------------------------

static bool keytool_log_is_txt(const char* name) {
    size_t len = strlen(name);
    return len > 4 && strcasecmp(name + (len - 4), ".txt") == 0;
}

void keytool_log_list_refresh(KeyToolApp* app) {
    Storage* storage = app->storage;
    storage_common_mkdir(storage, KEYTOOL_LOGS_FOLDER);

    FuriString* names[KEYTOOL_MAX_LOG_ENTRIES];
    for(size_t i = 0; i < KEYTOOL_MAX_LOG_ENTRIES; i++) names[i] = furi_string_alloc();
    size_t count = 0;

    File* dir = storage_file_alloc(storage);
    if(storage_dir_open(dir, KEYTOOL_LOGS_FOLDER)) {
        FileInfo info;
        char name_buf[256];
        while(count < KEYTOOL_MAX_LOG_ENTRIES &&
              storage_dir_read(dir, &info, name_buf, sizeof(name_buf))) {
            if(info.flags & FSF_DIRECTORY) continue;
            if(!keytool_log_is_txt(name_buf)) continue;
            furi_string_set_str(names[count], name_buf);
            count++;
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);

    // Filenames start with a sortable "YYYYMMDD_HHMMSS" prefix, so a
    // plain descending string sort puts the most recent scan first.
    for(size_t i = 1; i < count; i++) {
        for(size_t j = i;
            j > 0 &&
            strcmp(furi_string_get_cstr(names[j]), furi_string_get_cstr(names[j - 1])) > 0;
            j--) {
            FuriString* tmp = names[j];
            names[j] = names[j - 1];
            names[j - 1] = tmp;
        }
    }

    KeyToolLogListModel* model = view_get_model(app->log_list_view);
    model->count = count;
    model->selected = 0;
    model->scroll = 0;
    model->confirm_delete = false;

    for(size_t i = 0; i < count; i++) {
        furi_string_set(model->entries[i].filename, names[i]);
        furi_string_set_str(model->entries[i].card_name, "?");
        furi_string_set_str(model->entries[i].date_str, "?");

        FuriString* full_path = furi_string_alloc_printf(
            "%s/%s", KEYTOOL_LOGS_FOLDER, furi_string_get_cstr(names[i]));
        Stream* stream = file_stream_alloc(storage);
        if(file_stream_open(
               stream, furi_string_get_cstr(full_path), FSAM_READ, FSOM_OPEN_EXISTING)) {
            FuriString* line = furi_string_alloc();
            if(stream_read_line(stream, line)) {
                const char* cline = furi_string_get_cstr(line);
                if(strncmp(cline, "Card: ", 6) == 0) {
                    furi_string_set_str(model->entries[i].card_name, cline + 6);
                    furi_string_trim(model->entries[i].card_name);
                }
            }
            if(stream_read_line(stream, line)) {
                const char* cline = furi_string_get_cstr(line);
                if(strncmp(cline, "Date: ", 6) == 0) {
                    furi_string_set_str(model->entries[i].date_str, cline + 6);
                    furi_string_trim(model->entries[i].date_str);
                }
            }
            furi_string_free(line);
        }
        file_stream_close(stream);
        stream_free(stream);
        furi_string_free(full_path);
    }

    view_commit_model(app->log_list_view, true);

    for(size_t i = 0; i < KEYTOOL_MAX_LOG_ENTRIES; i++) furi_string_free(names[i]);
}

// ---------------------------------------------------------------------
// Alloc / free
// ---------------------------------------------------------------------

View* keytool_log_list_alloc(KeyToolApp* app) {
    View* view = view_alloc();
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(KeyToolLogListModel));
    view_set_draw_callback(view, keytool_log_list_draw_callback);
    view_set_input_callback(view, keytool_log_list_input_callback);
    view_set_previous_callback(view, keytool_log_list_back_callback);

    KeyToolLogListModel* model = view_get_model(view);
    for(size_t i = 0; i < KEYTOOL_MAX_LOG_ENTRIES; i++) {
        model->entries[i].filename = furi_string_alloc();
        model->entries[i].card_name = furi_string_alloc();
        model->entries[i].date_str = furi_string_alloc();
    }
    model->count = 0;
    model->selected = 0;
    model->scroll = 0;
    model->confirm_delete = false;
    view_commit_model(view, false);

    return view;
}

void keytool_log_list_free(View* view) {
    KeyToolLogListModel* model = view_get_model(view);
    for(size_t i = 0; i < KEYTOOL_MAX_LOG_ENTRIES; i++) {
        furi_string_free(model->entries[i].filename);
        furi_string_free(model->entries[i].card_name);
        furi_string_free(model->entries[i].date_str);
    }
    view_commit_model(view, false);
    view_free(view);
}
