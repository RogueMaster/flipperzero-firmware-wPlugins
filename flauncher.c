#include <furi.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>
#include <storage/storage.h>
#include <loader/loader.h>
#include <toolbox/stream/file_stream.h>

#define FAV_PATH   "/ext/favorites.txt"
#define ICONS_CFG  "/ext/apps_data/flauncher/icons.txt"
#define ICONS_TMP  "/ext/apps_data/flauncher/icons.tmp"
#define ICONS_DIR  "/ext/apps_data/flauncher/icons"
#define IRPAD_FAP  "/ext/apps/Infrared/irpad.fap" // IR favorites open here if present
#define STATE_PATH "/ext/apps_data/flauncher/state.txt"

#define MAX_ENTRIES 40
#define MAX_ICONS   64
#define COLS        5
#define VIS_ROWS    2
#define VIS_CELLS   (COLS * VIS_ROWS)
#define ICON_W      18
#define ICON_H      18
#define ICON_BYTES  (((ICON_W + 7) / 8) * ICON_H) // 3*18 = 54
#define ICON_FILE   (1 + ICON_BYTES)              // flag byte + XBM

#define CELL_W 25
#define CELL_H 25
#define GRID_X 2
#define BAR_Y  52
#define GRID_AREA (BAR_Y - 1)

typedef enum {
    ModeGrid,
    ModePicker,
} Mode;

typedef struct {
    char path[128];
    char label[28];
    char type[10];
    char app[16];
    char icon_name[24];
    bool is_file;
    uint8_t icon[ICON_FILE];
    bool icon_ok;
} Entry;

typedef struct {
    Entry entries[MAX_ENTRIES];
    uint8_t count;
    uint8_t index;
    uint8_t row_offset;

    Mode mode;
    // picker state
    char icon_names[MAX_ICONS][24];
    uint8_t icon_count;
    uint8_t pick_index;
    uint8_t pick_row_offset;
    uint8_t pick_buf[VIS_CELLS][ICON_FILE]; // visible picker icons
    bool pick_ok[VIS_CELLS];

    FuriMessageQueue* input_queue;
    bool do_launch;
} App;

// ---- data ----------------------------------------------------------------

static const char* ext_of(const char* path) {
    const char* dot = strrchr(path, '.');
    return dot ? dot : "";
}

static const char* default_icon(const char* type) {
    if(!strcmp(type, "IR")) return "tv";
    if(!strcmp(type, "SubGHz")) return "radio";
    if(!strcmp(type, "NFC")) return "door-open";
    if(!strcmp(type, "RFID")) return "building";
    if(!strcmp(type, "iBtn")) return "key";
    return "gears";
}

static void classify(Entry* e) {
    const char* ext = ext_of(e->path);
    e->is_file = true;
    if(!strcasecmp(ext, ".ir")) {
        strlcpy(e->app, "Infrared", sizeof(e->app));
        strlcpy(e->type, "IR", sizeof(e->type));
    } else if(!strcasecmp(ext, ".sub")) {
        strlcpy(e->app, "Sub-GHz", sizeof(e->app));
        strlcpy(e->type, "SubGHz", sizeof(e->type));
    } else if(!strcasecmp(ext, ".nfc")) {
        strlcpy(e->app, "NFC", sizeof(e->app));
        strlcpy(e->type, "NFC", sizeof(e->type));
    } else if(!strcasecmp(ext, ".rfid")) {
        strlcpy(e->app, "125 kHz RFID", sizeof(e->app));
        strlcpy(e->type, "RFID", sizeof(e->type));
    } else if(!strcasecmp(ext, ".ibtn")) {
        strlcpy(e->app, "iButton", sizeof(e->app));
        strlcpy(e->type, "iBtn", sizeof(e->type));
    } else {
        e->is_file = false;
        e->app[0] = '\0';
        strlcpy(e->type, "App", sizeof(e->type));
    }
    const char* base = strrchr(e->path, '/');
    base = base ? base + 1 : e->path;
    strlcpy(e->label, base, sizeof(e->label));
    char* dot = strrchr(e->label, '.');
    if(dot) *dot = '\0';
    strlcpy(e->icon_name, default_icon(e->type), sizeof(e->icon_name));
}

static bool read_icon(Storage* storage, const char* name, uint8_t* dst) {
    char full[180];
    snprintf(full, sizeof(full), "%s/%s.bm", ICONS_DIR, name);
    bool ok = false;
    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, full, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(storage_file_size(f) == ICON_FILE &&
           storage_file_read(f, dst, ICON_FILE) == ICON_FILE) {
            ok = true;
        }
    }
    storage_file_close(f);
    storage_file_free(f);
    return ok;
}

static void load_favorites(App* app) {
    app->count = 0;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* s = file_stream_alloc(storage);
    FuriString* line = furi_string_alloc();
    if(file_stream_open(s, FAV_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        while(app->count < MAX_ENTRIES && stream_read_line(s, line)) {
            furi_string_trim(line);
            if(furi_string_empty(line)) continue;
            Entry* e = &app->entries[app->count];
            memset(e, 0, sizeof(Entry));
            strlcpy(e->path, furi_string_get_cstr(line), sizeof(e->path));
            classify(e);
            app->count++;
        }
    }
    furi_string_free(line);
    file_stream_close(s);
    stream_free(s);
    furi_record_close(RECORD_STORAGE);
}

// apply icons.txt overrides onto entry icon names, then preload entry bitmaps
static void load_icons(App* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* s = file_stream_alloc(storage);
    FuriString* line = furi_string_alloc();
    if(file_stream_open(s, ICONS_CFG, FSAM_READ, FSOM_OPEN_EXISTING)) {
        while(stream_read_line(s, line)) {
            furi_string_trim(line);
            if(furi_string_empty(line) || furi_string_get_char(line, 0) == '#') continue;
            size_t eq = furi_string_search_char(line, '=');
            if(eq == FURI_STRING_FAILURE) continue;
            FuriString* key = furi_string_alloc();
            FuriString* val = furi_string_alloc();
            furi_string_set_n(key, line, 0, eq);
            furi_string_set_n(val, line, eq + 1, furi_string_size(line) - eq - 1);
            furi_string_trim(key);
            furi_string_trim(val);
            for(uint8_t i = 0; i < app->count; i++) {
                if(!strcmp(app->entries[i].path, furi_string_get_cstr(key))) {
                    strlcpy(app->entries[i].icon_name, furi_string_get_cstr(val),
                            sizeof(app->entries[i].icon_name));
                    break;
                }
            }
            furi_string_free(key);
            furi_string_free(val);
        }
    }
    furi_string_free(line);
    file_stream_close(s);
    stream_free(s);
    for(uint8_t i = 0; i < app->count; i++) {
        app->entries[i].icon_ok = read_icon(storage, app->entries[i].icon_name, app->entries[i].icon);
    }
    furi_record_close(RECORD_STORAGE);
}

// remember the selected favorite across relaunches (store its path, not index,
// so it survives favorites.txt reordering)
static void save_state(App* app) {
    if(app->count == 0) return;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, STATE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        const char* path = app->entries[app->index].path;
        storage_file_write(f, path, strlen(path));
    }
    storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

static void load_state(App* app) {
    if(app->count == 0) return;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* s = file_stream_alloc(storage);
    FuriString* line = furi_string_alloc();
    if(file_stream_open(s, STATE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(stream_read_line(s, line)) {
            furi_string_trim(line);
            for(uint8_t i = 0; i < app->count; i++) {
                if(!strcmp(app->entries[i].path, furi_string_get_cstr(line))) {
                    app->index = i;
                    break;
                }
            }
        }
    }
    furi_string_free(line);
    file_stream_close(s);
    stream_free(s);
    furi_record_close(RECORD_STORAGE);
    // keep the restored selection within the viewport
    uint8_t srow = app->index / COLS;
    app->row_offset = (srow >= VIS_ROWS) ? (srow - (VIS_ROWS - 1)) : 0;
}

// scan the icons dir for *.bm names (called lazily when the picker opens)
static void scan_icons(App* app) {
    app->icon_count = 0;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* dir = storage_file_alloc(storage);
    char name[64];
    if(storage_dir_open(dir, ICONS_DIR)) {
        FileInfo info;
        while(app->icon_count < MAX_ICONS && storage_dir_read(dir, &info, name, sizeof(name))) {
            if(info.flags & FSF_DIRECTORY) continue;
            char* dot = strrchr(name, '.');
            if(!dot || strcasecmp(dot, ".bm")) continue;
            *dot = '\0';
            strlcpy(app->icon_names[app->icon_count], name, sizeof(app->icon_names[0]));
            app->icon_count++;
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);
}

// load the currently visible picker icons (<=10) into pick_buf
static void load_pick_visible(App* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    for(uint8_t slot = 0; slot < VIS_CELLS; slot++) {
        uint8_t idx = app->pick_row_offset * COLS + slot;
        app->pick_ok[slot] = (idx < app->icon_count) &&
                             read_icon(storage, app->icon_names[idx], app->pick_buf[slot]);
    }
    furi_record_close(RECORD_STORAGE);
}

// rewrite icons.txt: keep every line except the one for `path`, then append path=icon
static void write_mapping(const char* path, const char* icon) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FuriString* out = furi_string_alloc();
    // read existing, dropping any prior mapping for this path
    Stream* rs = file_stream_alloc(storage);
    FuriString* line = furi_string_alloc();
    size_t plen = strlen(path);
    if(file_stream_open(rs, ICONS_CFG, FSAM_READ, FSOM_OPEN_EXISTING)) {
        while(stream_read_line(rs, line)) {
            // compare against "<path>=" ignoring surrounding spaces
            FuriString* t = furi_string_alloc_set(line);
            furi_string_trim(t);
            bool is_this = (furi_string_size(t) > plen) &&
                           (strncmp(furi_string_get_cstr(t), path, plen) == 0) &&
                           (furi_string_get_char(t, plen) == '=');
            furi_string_free(t);
            if(is_this) continue; // drop old mapping
            // normalise: trim and re-add exactly one newline (stream_read_line
            // newline handling varies, so don't rely on it)
            furi_string_trim(line);
            if(furi_string_empty(line)) continue;
            furi_string_cat(out, line);
            furi_string_push_back(out, '\n');
        }
    }
    furi_string_free(line);
    file_stream_close(rs);
    stream_free(rs);
    furi_string_cat_printf(out, "%s=%s\n", path, icon);
    // write in place (truncate)
    File* wf = storage_file_alloc(storage);
    if(storage_file_open(wf, ICONS_CFG, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(wf, furi_string_get_cstr(out), furi_string_size(out));
    }
    storage_file_close(wf);
    storage_file_free(wf);
    furi_string_free(out);
    furi_record_close(RECORD_STORAGE);
}

// ---- gui helpers ---------------------------------------------------------

static void draw_cell_icon(Canvas* canvas, int cx, int cy, const uint8_t* bm, bool ok, bool sel) {
    if(sel) canvas_draw_rbox(canvas, cx, cy, CELL_W - 1, CELL_H - 1, 3);
    int ix = cx + (CELL_W - 1 - ICON_W) / 2;
    int iy = cy + (CELL_H - 1 - ICON_H) / 2;
    if(sel) canvas_set_color(canvas, ColorWhite);
    if(ok) {
        canvas_draw_xbm(canvas, ix, iy, ICON_W, ICON_H, bm + 1);
    } else {
        canvas_draw_str_aligned(
            canvas, cx + CELL_W / 2, cy + CELL_H / 2, AlignCenter, AlignCenter, "?");
    }
    if(sel) canvas_set_color(canvas, ColorBlack);
}

static void draw_bottom(Canvas* canvas, const char* left, const char* right) {
    canvas_draw_line(canvas, 0, BAR_Y - 1, 128, BAR_Y - 1);
    canvas_set_font(canvas, FontSecondary);
    uint16_t w_r = canvas_string_width(canvas, right);
    canvas_draw_str_aligned(canvas, 127, 62, AlignRight, AlignBottom, right);
    int avail = 128 - 4 - (int)w_r - 4;
    char name[40];
    strlcpy(name, left, sizeof(name));
    size_t n = strlen(name);
    while(n > 1 && canvas_string_width(canvas, name) > avail) {
        name[--n] = '\0';
        if(n > 1) name[n - 1] = '.';
    }
    canvas_draw_str(canvas, 2, 62, name);
}

static void draw_cb(Canvas* canvas, void* ctx) {
    App* app = ctx;
    canvas_clear(canvas);

    if(app->count == 0) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, "No favorites");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "Pin items in Archive");
        return;
    }

    if(app->mode == ModePicker) {
        uint8_t total_rows = (app->icon_count + COLS - 1) / COLS;
        uint8_t rows_vis = total_rows < VIS_ROWS ? total_rows : VIS_ROWS;
        int top = (total_rows <= VIS_ROWS) ? (GRID_AREA - rows_vis * CELL_H) / 2 : 0;
        if(top < 0) top = 0;
        for(uint8_t vr = 0; vr < VIS_ROWS; vr++) {
            for(uint8_t col = 0; col < COLS; col++) {
                uint8_t slot = vr * COLS + col;
                uint8_t idx = (app->pick_row_offset + vr) * COLS + col;
                if(idx >= app->icon_count) continue;
                draw_cell_icon(
                    canvas, GRID_X + col * CELL_W, top + vr * CELL_H, app->pick_buf[slot],
                    app->pick_ok[slot], idx == app->pick_index);
            }
        }
        char pos[10];
        snprintf(pos, sizeof(pos), "%d/%d", app->pick_index + 1, app->icon_count);
        draw_bottom(canvas, app->icon_names[app->pick_index], pos);
        return;
    }

    // ModeGrid
    uint8_t total_rows = (app->count + COLS - 1) / COLS;
    uint8_t rows_vis = total_rows < VIS_ROWS ? total_rows : VIS_ROWS;
    int top = (total_rows <= VIS_ROWS) ? (GRID_AREA - rows_vis * CELL_H) / 2 : 0;
    if(top < 0) top = 0;
    for(uint8_t vr = 0; vr < VIS_ROWS; vr++) {
        for(uint8_t col = 0; col < COLS; col++) {
            uint8_t idx = (app->row_offset + vr) * COLS + col;
            if(idx >= app->count) continue;
            draw_cell_icon(
                canvas, GRID_X + col * CELL_W, top + vr * CELL_H, app->entries[idx].icon,
                app->entries[idx].icon_ok, idx == app->index);
        }
    }
    Entry* e = &app->entries[app->index];
    char right[12];
    snprintf(right, sizeof(right), "%s %d/%d", e->type, app->index + 1, app->count);
    draw_bottom(canvas, e->label, right);
}

static void input_cb(InputEvent* event, void* ctx) {
    App* app = ctx;
    furi_message_queue_put(app->input_queue, event, FuriWaitForever);
}

// grid navigation with wrap-around; handles a partial last row by skipping to
// the next/prev row that actually has the current column populated
static uint8_t grid_nav(uint8_t index, uint8_t count, uint16_t key) {
    if(count == 0) return 0;
    uint8_t col = index % COLS;
    uint8_t row = index / COLS;
    uint8_t rows = (count + COLS - 1) / COLS;
    if(key == InputKeyLeft) return (index + count - 1) % count;
    if(key == InputKeyRight) return (index + 1) % count;
    if(key == InputKeyDown) {
        for(uint8_t k = 1; k <= rows; k++) {
            uint8_t ni = ((row + k) % rows) * COLS + col;
            if(ni < count) return ni;
        }
    } else if(key == InputKeyUp) {
        for(uint8_t k = 1; k <= rows; k++) {
            uint8_t ni = ((row + rows - k) % rows) * COLS + col;
            if(ni < count) return ni;
        }
    }
    return index;
}

// ---- entry ---------------------------------------------------------------

int32_t flauncher_app(void* p) {
    UNUSED(p);
    App* app = malloc(sizeof(App));
    if(!app) return -1;
    memset(app, 0, sizeof(App));
    app->mode = ModeGrid;
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    load_favorites(app);
    load_icons(app);
    load_state(app); // restore last-selected favorite

    ViewPort* vp = view_port_alloc();
    view_port_draw_callback_set(vp, draw_cb, app);
    view_port_input_callback_set(vp, input_cb, app);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, vp, GuiLayerFullscreen);

    bool running = true;
    InputEvent event;
    while(running) {
        if(furi_message_queue_get(app->input_queue, &event, FuriWaitForever) != FuriStatusOk)
            continue;
        bool move = (event.type == InputTypeShort || event.type == InputTypeRepeat);

        if(app->mode == ModePicker) {
            if(move && app->icon_count) {
                app->pick_index = grid_nav(app->pick_index, app->icon_count, event.key);
                uint8_t prow = app->pick_index / COLS;
                if(prow < app->pick_row_offset) app->pick_row_offset = prow;
                if(prow > app->pick_row_offset + (VIS_ROWS - 1))
                    app->pick_row_offset = prow - (VIS_ROWS - 1);
                load_pick_visible(app);
                view_port_update(vp);
            }
            if(event.type == InputTypeShort) {
                if(event.key == InputKeyOk && app->icon_count) {
                    // assign the chosen icon to the current favorite
                    Entry* e = &app->entries[app->index];
                    strlcpy(e->icon_name, app->icon_names[app->pick_index], sizeof(e->icon_name));
                    write_mapping(e->path, e->icon_name);
                    Storage* st = furi_record_open(RECORD_STORAGE);
                    e->icon_ok = read_icon(st, e->icon_name, e->icon);
                    furi_record_close(RECORD_STORAGE);
                    app->mode = ModeGrid;
                    view_port_update(vp);
                } else if(event.key == InputKeyBack) {
                    app->mode = ModeGrid; // cancel picker
                    view_port_update(vp);
                }
            }
            continue;
        }

        // ModeGrid
        if(move && app->count) {
            app->index = grid_nav(app->index, app->count, event.key);
            uint8_t srow = app->index / COLS;
            if(srow < app->row_offset) app->row_offset = srow;
            if(srow > app->row_offset + (VIS_ROWS - 1)) app->row_offset = srow - (VIS_ROWS - 1);
            view_port_update(vp);
        }
        if(event.type == InputTypeLong && event.key == InputKeyOk && app->count) {
            // open icon picker for the selected favorite
            scan_icons(app);
            app->pick_index = 0;
            for(uint8_t i = 0; i < app->icon_count; i++) {
                if(!strcmp(app->icon_names[i], app->entries[app->index].icon_name)) {
                    app->pick_index = i;
                    break;
                }
            }
            uint8_t prow = app->pick_index / COLS;
            app->pick_row_offset = (prow >= VIS_ROWS) ? (prow - (VIS_ROWS - 1)) : 0;
            load_pick_visible(app);
            app->mode = ModePicker;
            view_port_update(vp);
        } else if(event.type == InputTypeShort) {
            if(event.key == InputKeyOk && app->count) {
                app->do_launch = true;
                running = false;
            } else if(event.key == InputKeyBack) {
                running = false;
            }
        }
    }

    gui_remove_view_port(gui, vp);
    furi_record_close(RECORD_GUI);
    view_port_free(vp);

    save_state(app); // remember where we were, for the next open / post-launch return

    if(app->do_launch && app->count) {
        Entry* e = &app->entries[app->index];
        Loader* loader = furi_record_open(RECORD_LOADER);
        if(e->is_file) {
            // IR remotes open in the IR Pad app (if installed), else the stock Infrared app
            const char* name = e->app;
            if(!strcmp(e->type, "IR")) {
                Storage* st = furi_record_open(RECORD_STORAGE);
                if(storage_file_exists(st, IRPAD_FAP)) name = IRPAD_FAP;
                furi_record_close(RECORD_STORAGE);
            }
            loader_enqueue_launch(loader, name, e->path, LoaderDeferredLaunchFlagNone);
        } else {
            loader_enqueue_launch(loader, e->path, NULL, LoaderDeferredLaunchFlagNone);
        }
        FuriString* self = furi_string_alloc();
        if(loader_get_application_launch_path(loader, self)) {
            loader_enqueue_launch(
                loader, furi_string_get_cstr(self), NULL, LoaderDeferredLaunchFlagNone);
        }
        furi_string_free(self);
        furi_record_close(RECORD_LOADER);
    }

    furi_message_queue_free(app->input_queue);
    free(app);
    return 0;
}
