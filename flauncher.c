#include <furi.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>
#include <storage/storage.h>
#include <loader/loader.h>
#include <toolbox/stream/file_stream.h>

#define FAV_PATH   "/ext/favorites.txt"
#define ICONS_CFG  "/ext/apps_data/flauncher/icons.txt"
#define ICONS_DIR  "/ext/apps_data/flauncher/icons"

#define MAX_ENTRIES 40
#define COLS        5
#define VIS_ROWS    2
#define ICON_W      18
#define ICON_H      18
#define ICON_BYTES  (((ICON_W + 7) / 8) * ICON_H) // 3*22 = 66
#define ICON_FILE   (1 + ICON_BYTES)              // flag byte + XBM

// grid cell geometry on the 128x64 screen
#define CELL_W 25
#define CELL_H 25
#define GRID_X 2 // left margin (5*25 = 125, +2 = 127)
#define GRID_Y 0

typedef struct {
    char path[128];
    char label[28];
    char type[10];
    char app[16];
    bool is_file;
    uint8_t icon[ICON_FILE];
    bool icon_ok;
} Entry;

typedef struct {
    Entry entries[MAX_ENTRIES];
    uint8_t count;
    uint8_t index;
    uint8_t row_offset; // top visible grid row
    FuriMessageQueue* input_queue;
    bool do_launch;
} App;

// ---- data ----------------------------------------------------------------

static const char* ext_of(const char* path) {
    const char* dot = strrchr(path, '.');
    return dot ? dot : "";
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
    // default icon name stored temporarily in icon[] region? no: keep separate below
}

// default icon name by type (used when no override)
static const char* default_icon(const Entry* e) {
    if(!strcmp(e->type, "IR")) return "tv";
    if(!strcmp(e->type, "SubGHz")) return "bolt";
    if(!strcmp(e->type, "NFC")) return "door-open";
    if(!strcmp(e->type, "RFID")) return "building";
    if(!strcmp(e->type, "iBtn")) return "key";
    return "gears";
}

// load one icon file into dst (ICON_FILE bytes). returns true on success.
static bool read_icon(Storage* storage, const char* name, uint8_t* dst) {
    char full[160];
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

// resolve each entry's icon name (override from icons.txt, else default) and
// preload its bitmap into the entry (count <= MAX_ENTRIES, ~67 bytes each).
static void load_icons(App* app) {
    // start with per-type default names
    char names[MAX_ENTRIES][24];
    for(uint8_t i = 0; i < app->count; i++) {
        strlcpy(names[i], default_icon(&app->entries[i]), sizeof(names[i]));
    }
    // apply overrides
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
                    strlcpy(names[i], furi_string_get_cstr(val), sizeof(names[i]));
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
    // read bitmaps
    for(uint8_t i = 0; i < app->count; i++) {
        app->entries[i].icon_ok = read_icon(storage, names[i], app->entries[i].icon);
    }
    furi_record_close(RECORD_STORAGE);
}

// keep the selected cell inside the 2-row viewport
static void clamp_viewport(App* app) {
    uint8_t sel_row = app->index / COLS;
    if(sel_row < app->row_offset) app->row_offset = sel_row;
    if(sel_row > app->row_offset + (VIS_ROWS - 1)) app->row_offset = sel_row - (VIS_ROWS - 1);
}

// ---- gui -----------------------------------------------------------------

#define BAR_Y     52 // bottom info bar top (line at BAR_Y-1)
#define GRID_AREA (BAR_Y - 1)

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

    uint8_t total_rows = (app->count + COLS - 1) / COLS;
    uint8_t rows_vis = total_rows < VIS_ROWS ? total_rows : VIS_ROWS;
    // vertically center the grid when it fits without scrolling, else pin to top
    int top = (total_rows <= VIS_ROWS) ? (GRID_AREA - rows_vis * CELL_H) / 2 : 0;
    if(top < 0) top = 0;

    for(uint8_t vr = 0; vr < VIS_ROWS; vr++) {
        uint8_t grid_row = app->row_offset + vr;
        for(uint8_t col = 0; col < COLS; col++) {
            uint8_t idx = grid_row * COLS + col;
            if(idx >= app->count) continue;
            int cx = GRID_X + col * CELL_W;
            int cy = top + vr * CELL_H;
            bool sel = (idx == app->index);
            if(sel) canvas_draw_rbox(canvas, cx, cy, CELL_W - 1, CELL_H - 1, 3);

            int ix = cx + (CELL_W - 1 - ICON_W) / 2;
            int iy = cy + (CELL_H - 1 - ICON_H) / 2;
            if(app->entries[idx].icon_ok) {
                if(sel) canvas_set_color(canvas, ColorWhite); // inverted icon on filled box
                canvas_draw_xbm(canvas, ix, iy, ICON_W, ICON_H, app->entries[idx].icon + 1);
                if(sel) canvas_set_color(canvas, ColorBlack);
            } else {
                if(sel) canvas_set_color(canvas, ColorWhite);
                canvas_draw_str_aligned(
                    canvas, cx + CELL_W / 2, cy + CELL_H / 2, AlignCenter, AlignCenter, "?");
                if(sel) canvas_set_color(canvas, ColorBlack);
            }
        }
    }

    // bottom info bar: short name (left, truncated) + "TYPE n/N" (right)
    Entry* e = &app->entries[app->index];
    canvas_draw_line(canvas, 0, BAR_Y - 1, 128, BAR_Y - 1);
    canvas_set_font(canvas, FontSecondary);

    char right[12];
    snprintf(right, sizeof(right), "%s %d/%d", e->type, app->index + 1, app->count);
    uint16_t w_r = canvas_string_width(canvas, right);
    canvas_draw_str_aligned(canvas, 127, 62, AlignRight, AlignBottom, right);

    // truncate label to the remaining left width
    int avail = 128 - 4 - (int)w_r - 4;
    char name[sizeof(e->label) + 1];
    strlcpy(name, e->label, sizeof(name));
    size_t n = strlen(name);
    while(n > 1 && canvas_string_width(canvas, name) > avail) {
        name[--n] = '\0';
        if(n > 1) name[n - 1] = '.'; // show it was cut
    }
    canvas_draw_str(canvas, 2, 62, name);
}

static void input_cb(InputEvent* event, void* ctx) {
    App* app = ctx;
    furi_message_queue_put(app->input_queue, event, FuriWaitForever);
}

// ---- entry ---------------------------------------------------------------

int32_t flauncher_app(void* p) {
    UNUSED(p);
    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    load_favorites(app);
    load_icons(app);

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
        bool is_move = (event.type == InputTypeShort || event.type == InputTypeRepeat);
        if(is_move && app->count) {
            uint8_t i = app->index;
            if(event.key == InputKeyLeft && i > 0) {
                app->index = i - 1;
            } else if(event.key == InputKeyRight && i + 1 < app->count) {
                app->index = i + 1;
            } else if(event.key == InputKeyUp && i >= COLS) {
                app->index = i - COLS;
            } else if(event.key == InputKeyDown && i + COLS < app->count) {
                app->index = i + COLS;
            }
            clamp_viewport(app);
            view_port_update(vp);
        }
        if(event.type == InputTypeShort) {
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

    if(app->do_launch && app->count) {
        Entry* e = &app->entries[app->index];
        Loader* loader = furi_record_open(RECORD_LOADER);
        if(e->is_file) {
            loader_enqueue_launch(loader, e->app, e->path, LoaderDeferredLaunchFlagNone);
        } else {
            loader_enqueue_launch(loader, e->path, NULL, LoaderDeferredLaunchFlagNone);
        }
        furi_record_close(RECORD_LOADER);
    }

    furi_message_queue_free(app->input_queue);
    free(app);
    return 0;
}
