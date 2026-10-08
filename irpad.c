#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/view.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <dialogs/dialogs.h>
#include <input/input.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>
#include <infrared.h>
#include <infrared_transmit.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <ctype.h>

#define APP_DIR    "/ext/apps_data/irpad"
#define ICONS_DIR  "/ext/apps_data/irpad/icons"
#define REMOTE_EXT ".irr"
#define LAYOUT_FILETYPE "IR Remote Layout"

#define MAX_REMOTES 16
#define MAX_BUTTONS 64
#define MAX_ICONS   64
#define ICON_W 14
#define ICON_H 14
#define ICON_BYTES (((ICON_W + 7) / 8) * ICON_H) // 54
#define ICON_FILE  (1 + ICON_BYTES)              // 55

#define ROW_H         20
#define ROWS_PER_PAGE 6 // 6 * 20 = 120, leaves a footer for the page indicator

typedef enum {
    ViewIdRemotes,
    ViewIdLayout,
    ViewIdEdit,
    ViewIdSignals,
    ViewIdIcons,
    ViewIdText,
} ViewId;

typedef enum {
    SizeLong,
    SizeShort,
} BtnSize;

typedef struct {
    BtnSize size;
    char icon[24];   // "" = none
    char text[24];   // used if no icon
    char file[128];  // .ir path
    char signal[32]; // signal name
    uint8_t icon_bm[ICON_FILE];
    bool icon_ok;
} Button;

typedef struct {
    char name[32];
    char path[160];
} RemoteRef;

typedef struct {
    uint8_t b[2];
    uint8_t n;
} Row;

typedef struct App {
    Gui* gui;
    Storage* storage;
    NotificationApp* notif;
    ViewDispatcher* vd;
    Submenu* remotes_menu;
    View* layout_view;
    Submenu* edit_menu;
    Submenu* signals_menu;
    View* icon_view;
    TextInput* text_input;
    DialogsApp* dialogs;

    RemoteRef remotes[MAX_REMOTES];
    uint8_t remote_count;

    Button buttons[MAX_BUTTONS];
    uint8_t button_count;
    uint8_t index;
    Row rows[MAX_BUTTONS];
    uint8_t row_count;
    uint8_t page;
    uint8_t page_count;
    bool direct; // opened via launch argument (Back exits instead of showing the list)
    bool move_mode; // rearranging the focused button with the d-pad
    char remote_name[32];
    char remote_path[160]; // .irr to save edits to
    char text_buf[24];     // text_input buffer
    char pending_file[160]; // .ir chosen in the browser, awaiting signal pick
    char icon_list[MAX_ICONS][24];
    uint8_t icon_list_count;
    uint8_t icon_bms[MAX_ICONS][ICON_FILE]; // preloaded for the grid
    bool icon_bm_ok[MAX_ICONS];
    uint8_t icon_sel;
    uint8_t icon_off; // grid scroll (row)
    char sig_names[64][32];
    uint8_t sig_count;
} App;

// layout view model just references the App
typedef struct {
    App* app;
} LayoutModel;

static void open_edit(App* app); // defined in the editor section
static void save_remote(App* app);
static void swap_buttons(App* app, uint8_t a, uint8_t b);

// ---- icons ---------------------------------------------------------------

static void load_icon(App* app, const char* name, uint8_t* dst, bool* ok) {
    *ok = false;
    if(!name || !name[0]) return;
    char full[200];
    snprintf(full, sizeof(full), "%s/%s.bm", ICONS_DIR, name);
    File* f = storage_file_alloc(app->storage);
    if(storage_file_open(f, full, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(storage_file_size(f) == ICON_FILE && storage_file_read(f, dst, ICON_FILE) == ICON_FILE)
            *ok = true;
    }
    storage_file_close(f);
    storage_file_free(f);
}

// ---- remote config (FlipperFormat) --------------------------------------

static void compute_rows(App* app) {
    app->row_count = 0;
    for(uint8_t i = 0; i < app->button_count; i++) {
        Button* b = &app->buttons[i];
        if(b->size == SizeShort && app->row_count > 0 &&
           app->rows[app->row_count - 1].n == 1 &&
           app->buttons[app->rows[app->row_count - 1].b[0]].size == SizeShort) {
            // pair with the open short slot in the previous row
            app->rows[app->row_count - 1].b[1] = i;
            app->rows[app->row_count - 1].n = 2;
        } else {
            app->rows[app->row_count].b[0] = i;
            app->rows[app->row_count].n = 1;
            app->row_count++;
        }
    }
    app->page_count = (app->row_count + ROWS_PER_PAGE - 1) / ROWS_PER_PAGE;
    if(app->page_count == 0) app->page_count = 1;
}

static void load_remote(App* app, const char* path) {
    app->button_count = 0;
    app->index = 0;
    app->page = 0;
    strlcpy(app->remote_name, "", sizeof(app->remote_name));
    strlcpy(app->remote_path, path, sizeof(app->remote_path));

    FlipperFormat* ff = flipper_format_file_alloc(app->storage);
    FuriString* tmp = furi_string_alloc();
    uint32_t ver = 0;
    if(flipper_format_file_open_existing(ff, path) && flipper_format_read_header(ff, tmp, &ver)) {
        if(flipper_format_read_string(ff, "Name", tmp))
            strlcpy(app->remote_name, furi_string_get_cstr(tmp), sizeof(app->remote_name));
        while(app->button_count < MAX_BUTTONS) {
            // each button is a fixed sequence of keys
            if(!flipper_format_read_string(ff, "Size", tmp)) break;
            Button* b = &app->buttons[app->button_count];
            memset(b, 0, sizeof(Button));
            b->size = (furi_string_cmpi_str(tmp, "short") == 0) ? SizeShort : SizeLong;
            if(flipper_format_read_string(ff, "Icon", tmp) && furi_string_cmp_str(tmp, "-"))
                strlcpy(b->icon, furi_string_get_cstr(tmp), sizeof(b->icon));
            if(flipper_format_read_string(ff, "Text", tmp) && furi_string_cmp_str(tmp, "-"))
                strlcpy(b->text, furi_string_get_cstr(tmp), sizeof(b->text));
            if(flipper_format_read_string(ff, "File", tmp) && furi_string_cmp_str(tmp, "-"))
                strlcpy(b->file, furi_string_get_cstr(tmp), sizeof(b->file));
            if(flipper_format_read_string(ff, "Signal", tmp) && furi_string_cmp_str(tmp, "-"))
                strlcpy(b->signal, furi_string_get_cstr(tmp), sizeof(b->signal));
            load_icon(app, b->icon, b->icon_bm, &b->icon_ok);
            app->button_count++;
        }
    }
    furi_string_free(tmp);
    flipper_format_free(ff);
    compute_rows(app);
}

// derive a short label (+ optional icon, size) for a raw signal name, so the
// auto-built remote looks tidy when there is no dedicated .irr layout
static void auto_label(const char* sig, char* text, size_t tn, char* icon, size_t in, BtnSize* size) {
    char lo[48];
    size_t i = 0;
    for(; sig[i] && i < sizeof(lo) - 1; i++) lo[i] = (char)tolower((unsigned char)sig[i]);
    lo[i] = '\0';
    icon[0] = '\0';
    *size = SizeShort;
    bool up = strstr(lo, "up") || strstr(lo, "next") || strstr(lo, "inc") || strchr(sig, '+');
    bool dn = strstr(lo, "down") || strstr(lo, "dwn") || strstr(lo, "prev") ||
              strstr(lo, "dec") || strchr(sig, '-');

#define SET(ic, tx)                   \
    do {                              \
        strlcpy(icon, ic, in);        \
        strlcpy(text, tx, tn);        \
        return;                       \
    } while(0)

    if(!strcmp(lo, "off")) SET("power-off", "Off");
    if(strstr(lo, "power") || !strcmp(lo, "on")) {
        *size = SizeLong;
        SET("power-off", "Power");
    }
    if(strstr(lo, "vol") && up) SET("volume-up", "Vol+");
    if(strstr(lo, "vol") && dn) SET("volume-down", "Vol-");
    if(strstr(lo, "ch") && up) SET("", "CH+");
    if(strstr(lo, "ch") && dn) SET("", "CH-");
    if(strstr(lo, "bright") && up) SET("", "Br+");
    if(strstr(lo, "bright") && dn) SET("", "Br-");
    if(strstr(lo, "mute")) SET("", "Mute");
    if(!strcmp(lo, "up")) SET("arrow-up", "Up");
    if(!strcmp(lo, "down") || !strcmp(lo, "dwn")) SET("arrow-down", "Down");
    if(!strcmp(lo, "left")) SET("arrow-left", "Left");
    if(!strcmp(lo, "right")) SET("arrow-right", "Right");
    if(!strcmp(lo, "ok") || strstr(lo, "select") || strstr(lo, "enter")) SET("", "OK");
    if(strstr(lo, "back")) SET("backward", "Back");
    if(strstr(lo, "home") || strstr(lo, "menu")) SET("house", "Home");
    if(strstr(lo, "play")) SET("play", "Play");
#undef SET

    // fallback: abbreviate to fit a short button (e.g. "Dark Orange" -> "DaOr",
    // "Light Green" -> "LiGr"); single words are kept (draw trims if needed)
    char tmp[48];
    strlcpy(tmp, sig, sizeof(tmp));
    char* words[5];
    int wc = 0;
    char* tok = strtok(tmp, " _");
    while(tok && wc < 5) {
        words[wc++] = tok;
        tok = strtok(NULL, " _");
    }
    size_t j = 0;
    if(wc >= 2) {
        int take = (wc == 2) ? 2 : 1; // 2 chars per word for two words, else initials
        for(int w = 0; w < wc && j < tn - 1; w++) {
            for(int k = 0; k < take && words[w][k] && j < tn - 1; k++) {
                char c = words[w][k];
                text[j++] = (k == 0) ? (char)toupper((unsigned char)c) :
                                       (char)tolower((unsigned char)c);
            }
        }
        text[j] = '\0';
    } else {
        strlcpy(text, wc ? words[0] : sig, tn);
    }
}

// build an ad-hoc remote from a raw .ir file: smart label/icon per signal
static void load_ir_as_remote(App* app, const char* path) {
    app->button_count = 0;
    app->index = 0;
    app->page = 0;
    const char* base = strrchr(path, '/');
    base = base ? base + 1 : path;
    strlcpy(app->remote_name, base, sizeof(app->remote_name));
    char* dot = strrchr(app->remote_name, '.');
    if(dot) *dot = '\0';
    // edits to an auto remote persist as a same-named .irr next to the .ir mapping
    snprintf(app->remote_path, sizeof(app->remote_path), "%s/%s%s", APP_DIR, app->remote_name,
             REMOTE_EXT);

    FlipperFormat* ff = flipper_format_file_alloc(app->storage);
    FuriString* nm = furi_string_alloc();
    FuriString* ft = furi_string_alloc();
    uint32_t ver = 0;
    if(flipper_format_file_open_existing(ff, path) && flipper_format_read_header(ff, ft, &ver)) {
        while(app->button_count < MAX_BUTTONS && flipper_format_read_string(ff, "name", nm)) {
            Button* b = &app->buttons[app->button_count];
            memset(b, 0, sizeof(Button));
            const char* sig = furi_string_get_cstr(nm);
            strlcpy(b->file, path, sizeof(b->file));
            strlcpy(b->signal, sig, sizeof(b->signal));
            auto_label(sig, b->text, sizeof(b->text), b->icon, sizeof(b->icon), &b->size);
            load_icon(app, b->icon, b->icon_bm, &b->icon_ok);
            app->button_count++;
        }
    }
    furi_string_free(nm);
    furi_string_free(ft);
    flipper_format_free(ff);
    compute_rows(app);
}

static void scan_remotes(App* app) {
    app->remote_count = 0;
    File* dir = storage_file_alloc(app->storage);
    char name[128];
    if(storage_dir_open(dir, APP_DIR)) {
        FileInfo info;
        while(app->remote_count < MAX_REMOTES && storage_dir_read(dir, &info, name, sizeof(name))) {
            if(info.flags & FSF_DIRECTORY) continue;
            char* dot = strrchr(name, '.');
            if(!dot || strcmp(dot, REMOTE_EXT)) continue;
            RemoteRef* r = &app->remotes[app->remote_count];
            snprintf(r->path, sizeof(r->path), "%s/%s", APP_DIR, name);
            *dot = '\0';
            strlcpy(r->name, name, sizeof(r->name)); // fallback = filename
            // prefer the human "Name" field for the list
            FlipperFormat* ff = flipper_format_file_alloc(app->storage);
            FuriString* v = furi_string_alloc();
            FuriString* ft = furi_string_alloc();
            uint32_t ver = 0;
            if(flipper_format_file_open_existing(ff, r->path) &&
               flipper_format_read_header(ff, ft, &ver) &&
               flipper_format_read_string(ff, "Name", v)) {
                strlcpy(r->name, furi_string_get_cstr(v), sizeof(r->name));
            }
            furi_string_free(v);
            furi_string_free(ft);
            flipper_format_free(ff);
            app->remote_count++;
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
}

// ---- IR transmit ---------------------------------------------------------

// parse the .ir file ourselves (infrared_signal API is disabled for faps) and
// transmit the named signal via the low-level infrared_send / _raw_ext
static void transmit(App* app, Button* b) {
    if(!b->file[0] || !b->signal[0]) return;
    FlipperFormat* ff = flipper_format_file_alloc(app->storage);
    FuriString* nm = furi_string_alloc();
    FuriString* ty = furi_string_alloc();
    FuriString* ftype = furi_string_alloc();
    FuriString* proto = furi_string_alloc();
    uint32_t ver = 0;
    bool sent = false;
    if(flipper_format_file_open_existing(ff, b->file) &&
       flipper_format_read_header(ff, ftype, &ver)) {
        while(flipper_format_read_string(ff, "name", nm)) {
            if(furi_string_cmp_str(nm, b->signal) != 0) continue; // seeks to next "name"
            if(!flipper_format_read_string(ff, "type", ty)) break;
            if(furi_string_cmp_str(ty, "raw") == 0) {
                uint32_t freq = 38000, count = 0;
                float duty = 0.33f;
                flipper_format_read_uint32(ff, "frequency", &freq, 1);
                flipper_format_read_float(ff, "duty_cycle", &duty, 1);
                if(flipper_format_get_value_count(ff, "data", &count) && count > 0) {
                    uint32_t* timings = malloc(sizeof(uint32_t) * count);
                    if(flipper_format_read_uint32(ff, "data", timings, count)) {
                        infrared_send_raw_ext(timings, count, true, freq, duty);
                        sent = true;
                    }
                    free(timings);
                }
            } else { // parsed
                uint8_t addr[4] = {0}, cmd[4] = {0};
                flipper_format_read_string(ff, "protocol", proto);
                flipper_format_read_hex(ff, "address", addr, 4);
                flipper_format_read_hex(ff, "command", cmd, 4);
                InfraredMessage msg;
                msg.protocol = infrared_get_protocol_by_name(furi_string_get_cstr(proto));
                msg.address = addr[0] | (addr[1] << 8) | (addr[2] << 16) | ((uint32_t)addr[3] << 24);
                msg.command = cmd[0] | (cmd[1] << 8) | (cmd[2] << 16) | ((uint32_t)cmd[3] << 24);
                msg.repeat = false;
                if(msg.protocol != InfraredProtocolUnknown) {
                    infrared_send(&msg, 1);
                    sent = true;
                }
            }
            break;
        }
    }
    if(sent) notification_message(app->notif, &sequence_blink_blue_100);
    furi_string_free(nm);
    furi_string_free(ty);
    furi_string_free(ftype);
    furi_string_free(proto);
    flipper_format_free(ff);
}

// ---- layout view ---------------------------------------------------------

static uint8_t row_of(App* app, uint8_t index) {
    for(uint8_t r = 0; r < app->row_count; r++)
        if(app->rows[r].b[0] == index || (app->rows[r].n == 2 && app->rows[r].b[1] == index))
            return r;
    return 0;
}

static void draw_button(Canvas* c, int x, int y, int w, const Button* b, bool sel) {
    if(sel)
        canvas_draw_rbox(c, x, y, w, ROW_H - 2, 3);
    else
        canvas_draw_rframe(c, x, y, w, ROW_H - 2, 3);
    if(sel) canvas_set_color(c, ColorWhite);
    if(b->icon_ok) {
        canvas_draw_xbm(c, x + (w - ICON_W) / 2, y + (ROW_H - 2 - ICON_H) / 2, ICON_W, ICON_H,
                        b->icon_bm + 1);
    } else {
        canvas_set_font(c, FontSecondary);
        char t[24];
        strlcpy(t, b->text[0] ? b->text : "?", sizeof(t));
        size_t n = strlen(t); // trim to fit the button width
        while(n > 1 && (int)canvas_string_width(c, t) > w - 4) t[--n] = '\0';
        canvas_draw_str_aligned(c, x + w / 2, y + (ROW_H - 2) / 2, AlignCenter, AlignCenter, t);
    }
    if(sel) canvas_set_color(c, ColorBlack);
}

static void layout_draw(Canvas* c, void* model) {
    LayoutModel* m = model;
    App* app = m->app;
    canvas_clear(c);
    int W = canvas_width(c);  // 64 in portrait
    int H = canvas_height(c); // 128 in portrait
    if(app->button_count == 0) {
        canvas_set_font(c, FontPrimary);
        canvas_draw_str_aligned(c, W / 2, H / 3, AlignCenter, AlignCenter, app->remote_name);
        canvas_set_font(c, FontSecondary);
        canvas_draw_str_aligned(c, W / 2, H / 3 + 16, AlignCenter, AlignCenter, "Empty remote");
        return;
    }
    int sw = (W - 6) / 2; // short button width
    uint8_t base = app->page * ROWS_PER_PAGE;
    for(uint8_t vr = 0; vr < ROWS_PER_PAGE; vr++) {
        uint8_t r = base + vr;
        if(r >= app->row_count) break;
        int y = vr * ROW_H + 1;
        Row* row = &app->rows[r];
        if(row->n == 1 && app->buttons[row->b[0]].size == SizeLong) {
            draw_button(c, 2, y, W - 4, &app->buttons[row->b[0]], row->b[0] == app->index);
        } else {
            draw_button(c, 2, y, sw, &app->buttons[row->b[0]], row->b[0] == app->index);
            if(row->n == 2)
                draw_button(c, 4 + sw, y, sw, &app->buttons[row->b[1]], row->b[1] == app->index);
        }
    }
    // footer: move-mode hint, else page dots
    if(app->move_mode) {
        canvas_set_font(c, FontSecondary);
        canvas_draw_str_aligned(c, W / 2, H - 2, AlignCenter, AlignBottom, "OK=done");
    } else if(app->page_count > 1) {
        int gap = 7;
        int x0 = W / 2 - (app->page_count - 1) * gap / 2;
        int y = H - 4;
        for(uint8_t i = 0; i < app->page_count; i++) {
            int cx = x0 + i * gap;
            if(i == app->page)
                canvas_draw_disc(c, cx, y, 2); // current: filled
            else
                canvas_draw_circle(c, cx, y, 2); // others: hollow ring
        }
    }
}

static void goto_page(App* app, uint8_t page) {
    if(app->page_count == 0) return;
    app->page = page % app->page_count;
    app->index = app->rows[app->page * ROWS_PER_PAGE].b[0]; // focus first button of page
}

static bool layout_input(InputEvent* e, void* ctx) {
    App* app = ctx;
    bool handled = false;

    if(e->type == InputTypeLong && e->key == InputKeyOk && app->button_count) {
        open_edit(app); // long-press OK -> edit the focused button
        return true;
    }

    // move mode: rearrange the focused button through the whole sequence (crosses pages)
    if(app->move_mode && (e->type == InputTypeShort || e->type == InputTypeRepeat)) {
        if((e->key == InputKeyUp || e->key == InputKeyLeft) && app->index > 0) {
            swap_buttons(app, app->index, app->index - 1);
            app->index--;
            compute_rows(app);
        } else if(
            (e->key == InputKeyDown || e->key == InputKeyRight) &&
            app->index + 1 < app->button_count) {
            swap_buttons(app, app->index, app->index + 1);
            app->index++;
            compute_rows(app);
        } else if(e->key == InputKeyOk || e->key == InputKeyBack) {
            app->move_mode = false;
            save_remote(app);
        }
        if(app->button_count) app->page = row_of(app, app->index) / ROWS_PER_PAGE;
        with_view_model(app->layout_view, LayoutModel * m, { m->app = app; }, true);
        return true;
    }

    if((e->type == InputTypeShort || e->type == InputTypeRepeat) && app->button_count) {
        uint8_t base = app->page * ROWS_PER_PAGE;
        uint8_t last = base + ROWS_PER_PAGE - 1;
        if(last >= app->row_count) last = app->row_count - 1;
        uint8_t r = row_of(app, app->index);
        Row* row = &app->rows[r];
        if(e->key == InputKeyUp || e->key == InputKeyDown) {
            bool right = (row->n == 2 && row->b[1] == app->index);
            if(e->key == InputKeyUp && r > base) r--;
            else if(e->key == InputKeyDown && r < last) r++;
            Row* nr = &app->rows[r];
            app->index = (right && nr->n == 2) ? nr->b[1] : nr->b[0];
            handled = true;
        } else if(e->key == InputKeyLeft) {
            if(row->n == 2 && row->b[1] == app->index) {
                app->index = row->b[0]; // move within the row
            } else if(app->page_count > 1) {
                goto_page(app, app->page + app->page_count - 1); // edge -> prev page
            }
            handled = true;
        } else if(e->key == InputKeyRight) {
            if(row->n == 2 && row->b[0] == app->index) {
                app->index = row->b[1]; // move within the row
            } else if(app->page_count > 1) {
                goto_page(app, app->page + 1); // edge -> next page
            }
            handled = true;
        } else if(e->key == InputKeyOk && e->type == InputTypeShort) {
            transmit(app, &app->buttons[app->index]);
            handled = true;
        } else if(e->key == InputKeyBack && e->type == InputTypeShort) {
            if(app->direct)
                view_dispatcher_stop(app->vd); // opened directly -> exit app
            else
                view_dispatcher_switch_to_view(app->vd, ViewIdRemotes);
            handled = true;
        }
    }

    if(handled)
        with_view_model(app->layout_view, LayoutModel * mdl, { mdl->app = app; }, true);
    return handled;
}

// ---- remotes submenu -----------------------------------------------------

static void remotes_cb(void* ctx, uint32_t idx) {
    App* app = ctx;
    if(idx < app->remote_count) {
        load_remote(app, app->remotes[idx].path);
        with_view_model(app->layout_view, LayoutModel * m, { m->app = app; }, true);
        view_dispatcher_switch_to_view(app->vd, ViewIdLayout);
    }
}

static void build_remotes_menu(App* app) {
    submenu_reset(app->remotes_menu);
    submenu_set_header(app->remotes_menu, "IR Pad - remotes");
    scan_remotes(app);
    for(uint8_t i = 0; i < app->remote_count; i++)
        submenu_add_item(app->remotes_menu, app->remotes[i].name, i, remotes_cb, app);
}

static uint32_t exit_cb(void* ctx) {
    UNUSED(ctx);
    return VIEW_NONE;
}

static uint32_t ret_layout(void* ctx) {
    UNUSED(ctx);
    return ViewIdLayout;
}
static uint32_t ret_edit(void* ctx) {
    UNUSED(ctx);
    return ViewIdEdit;
}

// ---- editor --------------------------------------------------------------

static void save_remote(App* app) {
    if(app->remote_path[0] == '\0') return;
    FlipperFormat* ff = flipper_format_file_alloc(app->storage);
    FuriString* v = furi_string_alloc();
    if(flipper_format_file_open_always(ff, app->remote_path)) {
        flipper_format_write_header_cstr(ff, LAYOUT_FILETYPE, 1);
        furi_string_set(v, app->remote_name[0] ? app->remote_name : "Remote");
        flipper_format_write_string(ff, "Name", v);
        for(uint8_t i = 0; i < app->button_count; i++) {
            Button* b = &app->buttons[i];
            furi_string_set(v, b->size == SizeLong ? "long" : "short");
            flipper_format_write_string(ff, "Size", v);
            furi_string_set(v, b->icon[0] ? b->icon : "-");
            flipper_format_write_string(ff, "Icon", v);
            furi_string_set(v, b->text[0] ? b->text : "-");
            flipper_format_write_string(ff, "Text", v);
            furi_string_set(v, b->file[0] ? b->file : "-");
            flipper_format_write_string(ff, "File", v);
            furi_string_set(v, b->signal[0] ? b->signal : "-");
            flipper_format_write_string(ff, "Signal", v);
        }
    }
    furi_string_free(v);
    flipper_format_free(ff);
}

static void back_to_layout(App* app) {
    if(app->page_count == 0 || app->button_count == 0)
        app->page = 0;
    else
        app->page = row_of(app, app->index) / ROWS_PER_PAGE;
    with_view_model(app->layout_view, LayoutModel * m, { m->app = app; }, true);
    view_dispatcher_switch_to_view(app->vd, ViewIdLayout);
}

static void signals_cb(void* ctx, uint32_t idx) {
    App* app = ctx;
    if(idx < app->sig_count) {
        Button* b = &app->buttons[app->index];
        strlcpy(b->file, app->pending_file, sizeof(b->file));
        strlcpy(b->signal, app->sig_names[idx], sizeof(b->signal));
        save_remote(app);
    }
    back_to_layout(app);
}

static void build_signals_menu(App* app) {
    app->sig_count = 0;
    submenu_reset(app->signals_menu);
    submenu_set_header(app->signals_menu, "Pick signal");
    FlipperFormat* ff = flipper_format_file_alloc(app->storage);
    FuriString* nm = furi_string_alloc();
    FuriString* ft = furi_string_alloc();
    uint32_t ver = 0;
    if(flipper_format_file_open_existing(ff, app->pending_file) &&
       flipper_format_read_header(ff, ft, &ver)) {
        while(app->sig_count < 64 && flipper_format_read_string(ff, "name", nm)) {
            strlcpy(app->sig_names[app->sig_count], furi_string_get_cstr(nm), 32);
            submenu_add_item(
                app->signals_menu, app->sig_names[app->sig_count], app->sig_count, signals_cb, app);
            app->sig_count++;
        }
    }
    furi_string_free(nm);
    furi_string_free(ft);
    flipper_format_free(ff);
}

// ---- icon picker grid ----
#define IPICK_COLS 2
#define IPICK_CW   31
#define IPICK_CH   30
#define IPICK_ROWS 4

static void build_icons(App* app) {
    app->icon_list_count = 0;
    File* dir = storage_file_alloc(app->storage);
    char name[64];
    if(storage_dir_open(dir, ICONS_DIR)) {
        FileInfo info;
        while(app->icon_list_count < MAX_ICONS && storage_dir_read(dir, &info, name, sizeof(name))) {
            if(info.flags & FSF_DIRECTORY) continue;
            char* dot = strrchr(name, '.');
            if(!dot || strcasecmp(dot, ".bm")) continue;
            *dot = '\0';
            uint8_t n = app->icon_list_count;
            strlcpy(app->icon_list[n], name, 24);
            load_icon(app, app->icon_list[n], app->icon_bms[n], &app->icon_bm_ok[n]);
            app->icon_list_count++;
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    // start on the button's current icon, else the trailing "none" cell
    app->icon_sel = app->icon_list_count;
    for(uint8_t i = 0; i < app->icon_list_count; i++)
        if(!strcmp(app->icon_list[i], app->buttons[app->index].icon)) {
            app->icon_sel = i;
            break;
        }
    uint8_t row = app->icon_sel / IPICK_COLS;
    app->icon_off = (row >= IPICK_ROWS) ? row - (IPICK_ROWS - 1) : 0;
}

static void icon_draw(Canvas* c, void* model) {
    LayoutModel* m = model;
    App* app = m->app;
    canvas_clear(c);
    uint8_t total = app->icon_list_count + 1; // trailing cell = "none"
    for(uint8_t vr = 0; vr < IPICK_ROWS; vr++) {
        for(uint8_t col = 0; col < IPICK_COLS; col++) {
            uint8_t k = (app->icon_off + vr) * IPICK_COLS + col;
            if(k >= total) continue;
            int x = 2 + col * IPICK_CW;
            int y = vr * IPICK_CH + 1;
            bool sel = (k == app->icon_sel);
            if(sel) canvas_draw_rbox(c, x, y, IPICK_CW - 2, IPICK_CH - 2, 3);
            if(sel) canvas_set_color(c, ColorWhite);
            if(k < app->icon_list_count && app->icon_bm_ok[k]) {
                canvas_draw_xbm(c, x + (IPICK_CW - 2 - ICON_W) / 2, y + (IPICK_CH - 2 - ICON_H) / 2,
                                ICON_W, ICON_H, app->icon_bms[k] + 1);
            } else {
                canvas_set_font(c, FontSecondary);
                canvas_draw_str_aligned(c, x + IPICK_CW / 2, y + IPICK_CH / 2, AlignCenter,
                                        AlignCenter, (k < app->icon_list_count) ? "?" : "none");
            }
            if(sel) canvas_set_color(c, ColorBlack);
        }
    }
}

static bool icon_input(InputEvent* e, void* ctx) {
    App* app = ctx;
    if(e->type != InputTypeShort && e->type != InputTypeRepeat) return false;
    uint8_t total = app->icon_list_count + 1;
    uint8_t s = app->icon_sel;
    if(e->key == InputKeyOk) {
        Button* b = &app->buttons[app->index];
        if(app->icon_sel >= app->icon_list_count)
            b->icon[0] = '\0';
        else
            strlcpy(b->icon, app->icon_list[app->icon_sel], sizeof(b->icon));
        load_icon(app, b->icon, b->icon_bm, &b->icon_ok);
        save_remote(app);
        back_to_layout(app);
        return true;
    }
    if(e->key == InputKeyBack) {
        view_dispatcher_switch_to_view(app->vd, ViewIdEdit);
        return true;
    }
    if(e->key == InputKeyUp && s >= IPICK_COLS)
        s -= IPICK_COLS;
    else if(e->key == InputKeyDown && s + IPICK_COLS < total)
        s += IPICK_COLS;
    else if(e->key == InputKeyLeft && (s % IPICK_COLS))
        s--;
    else if(e->key == InputKeyRight && (s % IPICK_COLS) == 0 && s + 1 < total)
        s++;
    app->icon_sel = s;
    uint8_t row = s / IPICK_COLS;
    if(row < app->icon_off) app->icon_off = row;
    if(row > app->icon_off + (IPICK_ROWS - 1)) app->icon_off = row - (IPICK_ROWS - 1);
    with_view_model(app->icon_view, LayoutModel * mm, { mm->app = app; }, true);
    return true;
}

static void text_done_cb(void* ctx) {
    App* app = ctx;
    strlcpy(app->buttons[app->index].text, app->text_buf, sizeof(app->buttons[app->index].text));
    save_remote(app);
    back_to_layout(app);
}

static void swap_buttons(App* app, uint8_t a, uint8_t b) {
    Button t = app->buttons[a];
    app->buttons[a] = app->buttons[b];
    app->buttons[b] = t;
}

static void edit_cb(void* ctx, uint32_t idx) {
    App* app = ctx;
    Button* b = &app->buttons[app->index];
    switch(idx) {
    case 0: { // assign IR via file browser, then pick a signal
        FuriString* res = furi_string_alloc();
        FuriString* start = furi_string_alloc_set("/ext/infrared");
        DialogsFileBrowserOptions opt;
        memset(&opt, 0, sizeof(opt));
        opt.extension = ".ir";
        opt.base_path = "/ext/infrared";
        opt.hide_dot_files = true;
        opt.skip_assets = true;
        bool picked = dialog_file_browser_show(app->dialogs, res, start, &opt);
        if(picked) {
            strlcpy(app->pending_file, furi_string_get_cstr(res), sizeof(app->pending_file));
            build_signals_menu(app);
            view_dispatcher_switch_to_view(app->vd, ViewIdSignals);
        } else {
            view_dispatcher_switch_to_view(app->vd, ViewIdEdit);
        }
        furi_string_free(res);
        furi_string_free(start);
        break;
    }
    case 1: // set text
        text_input_reset(app->text_input);
        text_input_set_header_text(app->text_input, "Button text");
        strlcpy(app->text_buf, b->text, sizeof(app->text_buf));
        text_input_set_result_callback(
            app->text_input, text_done_cb, app, app->text_buf, sizeof(app->text_buf), true);
        view_dispatcher_switch_to_view(app->vd, ViewIdText);
        break;
    case 2: // set icon (grid picker)
        build_icons(app);
        with_view_model(app->icon_view, LayoutModel * m, { m->app = app; }, true);
        view_dispatcher_switch_to_view(app->vd, ViewIdIcons);
        break;
    case 3: // toggle size
        b->size = (b->size == SizeLong) ? SizeShort : SizeLong;
        compute_rows(app);
        save_remote(app);
        back_to_layout(app);
        break;
    case 4: // enter move mode (rearrange with the d-pad on the layout)
        app->move_mode = true;
        back_to_layout(app);
        break;
    case 5: // add a blank button after current
        if(app->button_count < MAX_BUTTONS) {
            uint8_t at = app->index + 1;
            for(int i = app->button_count; i > at; i--) app->buttons[i] = app->buttons[i - 1];
            memset(&app->buttons[at], 0, sizeof(Button));
            app->buttons[at].size = SizeShort;
            strlcpy(app->buttons[at].text, "New", sizeof(app->buttons[at].text));
            app->button_count++;
            app->index = at;
            compute_rows(app);
            save_remote(app);
        }
        back_to_layout(app);
        break;
    case 6: { // delete (with confirmation)
        DialogMessage* m = dialog_message_alloc();
        dialog_message_set_text(m, "Delete this button?", 64, 28, AlignCenter, AlignCenter);
        dialog_message_set_buttons(m, "Cancel", NULL, "Delete");
        DialogMessageButton res = dialog_message_show(app->dialogs, m);
        dialog_message_free(m);
        if(res == DialogMessageButtonRight && app->button_count > 0) {
            for(int i = app->index; i + 1 < app->button_count; i++)
                app->buttons[i] = app->buttons[i + 1];
            app->button_count--;
            if(app->index >= app->button_count && app->index > 0) app->index--;
            compute_rows(app);
            save_remote(app);
            back_to_layout(app);
        } else {
            view_dispatcher_switch_to_view(app->vd, ViewIdEdit);
        }
        break;
    }
    }
}

static void open_edit(App* app) {
    if(app->button_count == 0) return;
    Button* b = &app->buttons[app->index];
    submenu_reset(app->edit_menu);
    submenu_set_header(app->edit_menu, "Edit button");
    submenu_add_item(app->edit_menu, "Assign IR", 0, edit_cb, app);
    submenu_add_item(app->edit_menu, "Set text", 1, edit_cb, app);
    submenu_add_item(app->edit_menu, "Set icon", 2, edit_cb, app);
    submenu_add_item(app->edit_menu, b->size == SizeLong ? "Size: Long" : "Size: Short", 3, edit_cb, app);
    submenu_add_item(app->edit_menu, "Move (d-pad)", 4, edit_cb, app);
    submenu_add_item(app->edit_menu, "Add button", 5, edit_cb, app);
    submenu_add_item(app->edit_menu, "Delete button", 6, edit_cb, app);
    view_dispatcher_switch_to_view(app->vd, ViewIdEdit);
}

// ---- entry ---------------------------------------------------------------

int32_t irpad_app(void* p) {
    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));
    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->notif = furi_record_open(RECORD_NOTIFICATION);
    storage_common_mkdir(app->storage, APP_DIR);

    app->vd = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->vd, app->gui, ViewDispatcherTypeFullscreen);

    app->remotes_menu = submenu_alloc();
    view_set_orientation(submenu_get_view(app->remotes_menu), ViewOrientationVertical);
    view_set_previous_callback(submenu_get_view(app->remotes_menu), exit_cb);
    view_dispatcher_add_view(app->vd, ViewIdRemotes, submenu_get_view(app->remotes_menu));

    app->layout_view = view_alloc();
    view_set_orientation(app->layout_view, ViewOrientationVertical); // portrait remote
    view_allocate_model(app->layout_view, ViewModelTypeLocking, sizeof(LayoutModel));
    with_view_model(app->layout_view, LayoutModel * m, { m->app = app; }, false);
    view_set_context(app->layout_view, app);
    view_set_draw_callback(app->layout_view, layout_draw);
    view_set_input_callback(app->layout_view, layout_input);
    view_dispatcher_add_view(app->vd, ViewIdLayout, app->layout_view);

    app->edit_menu = submenu_alloc();
    view_set_orientation(submenu_get_view(app->edit_menu), ViewOrientationVertical);
    view_set_previous_callback(submenu_get_view(app->edit_menu), ret_layout);
    view_dispatcher_add_view(app->vd, ViewIdEdit, submenu_get_view(app->edit_menu));

    app->signals_menu = submenu_alloc();
    view_set_orientation(submenu_get_view(app->signals_menu), ViewOrientationVertical);
    view_set_previous_callback(submenu_get_view(app->signals_menu), ret_edit);
    view_dispatcher_add_view(app->vd, ViewIdSignals, submenu_get_view(app->signals_menu));

    app->icon_view = view_alloc();
    view_set_orientation(app->icon_view, ViewOrientationVertical);
    view_allocate_model(app->icon_view, ViewModelTypeLocking, sizeof(LayoutModel));
    with_view_model(app->icon_view, LayoutModel * m, { m->app = app; }, false);
    view_set_context(app->icon_view, app);
    view_set_draw_callback(app->icon_view, icon_draw);
    view_set_input_callback(app->icon_view, icon_input);
    view_dispatcher_add_view(app->vd, ViewIdIcons, app->icon_view);

    app->text_input = text_input_alloc();
    // keyboard stays landscape (it needs the full 128px width); user rotates to type
    view_set_previous_callback(text_input_get_view(app->text_input), ret_edit);
    view_dispatcher_add_view(app->vd, ViewIdText, text_input_get_view(app->text_input));

    app->dialogs = furi_record_open(RECORD_DIALOGS);

    const char* arg = (const char*)p;
    if(arg && arg[0]) {
        // launched with a file argument: open it directly (skip the remote list)
        app->direct = true;
        size_t len = strlen(arg);
        if(len > 4 && strcmp(arg + len - 4, REMOTE_EXT) == 0) {
            load_remote(app, arg); // a .irr layout
        } else {
            // exact 1:1 name match: /ext/infrared/Foo.ir -> apps_data/irpad/Foo.irr
            const char* base = strrchr(arg, '/');
            base = base ? base + 1 : arg;
            char stem[128];
            strlcpy(stem, base, sizeof(stem));
            char* d = strrchr(stem, '.');
            if(d) *d = '\0';
            char cand[220];
            snprintf(cand, sizeof(cand), "%s/%s%s", APP_DIR, stem, REMOTE_EXT);
            if(storage_file_exists(app->storage, cand))
                load_remote(app, cand); // the dedicated layout for exactly this .ir
            else
                load_ir_as_remote(app, arg); // fallback: auto remote
        }
        with_view_model(app->layout_view, LayoutModel * m, { m->app = app; }, true);
        view_dispatcher_switch_to_view(app->vd, ViewIdLayout);
    } else {
        build_remotes_menu(app);
        view_dispatcher_switch_to_view(app->vd, ViewIdRemotes);
    }
    view_dispatcher_run(app->vd);

    view_dispatcher_remove_view(app->vd, ViewIdRemotes);
    view_dispatcher_remove_view(app->vd, ViewIdLayout);
    view_dispatcher_remove_view(app->vd, ViewIdEdit);
    view_dispatcher_remove_view(app->vd, ViewIdSignals);
    view_dispatcher_remove_view(app->vd, ViewIdIcons);
    view_dispatcher_remove_view(app->vd, ViewIdText);
    submenu_free(app->remotes_menu);
    submenu_free(app->edit_menu);
    submenu_free(app->signals_menu);
    text_input_free(app->text_input);
    view_free(app->icon_view);
    view_free(app->layout_view);
    view_dispatcher_free(app->vd);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_NOTIFICATION);
    free(app);
    return 0;
}
