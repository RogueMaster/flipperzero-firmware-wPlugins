/*
 * Disney Infinity base emulator for the Flipper Zero: user interface.
 *
 * Keys, main list: Up/Down pick row  OK add (File/Fav/New) or remove
 *                  Left/Right swap to previous/next favorite
 *                  Hold OK star/unstar the row's figure
 *                  Hold Right save debug log   Back exit
 * Keys, new figure: Up/Down pick figure   Left/Right change group (series and
 *                   characters / play sets / discs)
 *                   OK create (blank, kept in memory only, never saved)   Back cancel
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include <notification/notification_messages.h>
#include <stdio.h>
#include <string.h>

#include "infinity_figures.h"
#include "usb_infinity.h"

#define APP_DATA_DIR "/ext/apps_data"
#define APP_DIR      "/ext/apps_data/flipbase"
#define NAME_LEN     24
#define PATH_LEN     96
#define FAV_MAX      20
#define FAV_FILE     "/ext/apps_data/flipbase/favorites.txt"

typedef enum {
    ModeSlots,
    ModeCreate,
    ModeFavs,
} Mode;

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* queue;
    Storage* storage;
    DialogsApp* dialogs;

    Mode mode;
    bool running;
    uint8_t sel;
    uint8_t create_group; /* GROUPS entries: series 1..3 x characters / play sets / discs */
    uint16_t create_index;

    char slot_name[INF_SLOT_COUNT][NAME_LEN];
    char slot_path[INF_SLOT_COUNT][PATH_LEN];
    char fav_path[FAV_MAX][PATH_LEN];
    uint8_t fav_count;
    uint8_t fav_sel;
    char message[32];
    uint32_t message_until;
} App;

#define ROWS         9
#define ROWS_VISIBLE 4
/* All nine base slots: characters first, then the play set, then every disc slot. */
static const uint8_t ROW_SLOT[ROWS] = {3, 6, 0, 1, 2, 4, 5, 7, 8};
static const char* const ROW_LABEL[ROWS] = {
    "Player 1",
    "Player 2",
    "Play set",
    "Disc 2",
    "Disc 3",
    "P1 disc 1",
    "P1 disc 2",
    "P2 disc 1",
    "P2 disc 2"};

/* ------------------------------------------------------------ helpers */

static void set_message(App* app, const char* text) {
    snprintf(app->message, sizeof(app->message), "%s", text);
    app->message_until = furi_get_tick() + 3000;
}

/* The New list is browsed one group at a time: Infinity 1.0 / 2.0 / 3.0, each split
 * into characters, play sets and power discs. */
#define GROUPS 9
static const char* const KIND_LABEL[3] = {"Characters", "Play sets", "Discs"};

static bool in_group(const InfFigureInfo* info, uint8_t group) {
    return info->series == (group / 3) + 1 && (uint8_t)inf_figure_kind(info->id) == group % 3;
}

static uint16_t group_count(uint8_t group) {
    uint16_t n = 0;
    for(size_t i = 0; i < inf_figure_table_count; i++) {
        if(in_group(&inf_figure_table[i], group)) n++;
    }
    return n;
}

static const InfFigureInfo* group_get(uint8_t group, uint16_t index) {
    for(size_t i = 0; i < inf_figure_table_count; i++) {
        if(in_group(&inf_figure_table[i], group)) {
            if(index == 0) return &inf_figure_table[i];
            index--;
        }
    }
    return NULL;
}

static bool read_file(App* app, const char* path, uint8_t* out) {
    File* file = storage_file_alloc(app->storage);
    bool ok = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING);
    if(ok) ok = (storage_file_size(file) == INF_FIGURE_SIZE);
    if(ok) ok = (storage_file_read(file, out, INF_FIGURE_SIZE) == INF_FIGURE_SIZE);
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

static bool write_file(App* app, const char* path, const uint8_t* data, size_t size) {
    File* file = storage_file_alloc(app->storage);
    bool ok = storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(ok) ok = (storage_file_write(file, data, size) == size);
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

/* Write back any figure the game changed (progress, unlocks). */
static void flush_dirty(App* app) {
    for(uint8_t slot = 0; slot < INF_SLOT_COUNT; slot++) {
        uint8_t data[INF_FIGURE_SIZE];
        FURI_CRITICAL_ENTER();
        const bool dirty = inf_base_take_dirty(&usb_inf_base, slot, data);
        FURI_CRITICAL_EXIT();
        if(dirty && app->slot_path[slot][0] != '\0') {
            if(!write_file(app, app->slot_path[slot], data, INF_FIGURE_SIZE)) {
                set_message(app, "Save to SD failed");
            }
        }
    }
}

static void put_in_slot(App* app, uint8_t slot, const uint8_t* data, const char* path_in) {
    /* The caller's path may live inside App; work from a private copy. */
    char path[PATH_LEN];
    snprintf(path, PATH_LEN, "%s", path_in);
    flush_dirty(app);

    FURI_CRITICAL_ENTER();
    if(inf_base_slot_present(&usb_inf_base, slot)) {
        inf_base_remove_figure(&usb_inf_base, slot);
    }
    inf_base_load_figure(&usb_inf_base, slot, data);
    FURI_CRITICAL_EXIT();
    usb_infinity_kick();

    const uint32_t id = inf_figure_decode_number(data);
    const InfFigureInfo* info = inf_figure_lookup(id);
    snprintf(app->slot_name[slot], NAME_LEN, "%s", info ? info->name : "Unknown figure");
    snprintf(app->slot_path[slot], PATH_LEN, "%s", path);
}

static void remove_slot(App* app, uint8_t slot) {
    flush_dirty(app);
    FURI_CRITICAL_ENTER();
    const bool removed = inf_base_remove_figure(&usb_inf_base, slot);
    FURI_CRITICAL_EXIT();
    if(removed) {
        usb_infinity_kick();
        app->slot_name[slot][0] = '\0';
        app->slot_path[slot][0] = '\0';
    }
}

/* ------------------------------------------------------------ favorites */

static const char* base_name(const char* path) {
    const char* slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static int fav_find(const App* app, const char* path) {
    for(uint8_t i = 0; i < app->fav_count; i++) {
        if(strcmp(app->fav_path[i], path) == 0) return i;
    }
    return -1;
}

static void fav_save(App* app) {
    File* file = storage_file_alloc(app->storage);
    if(storage_file_open(file, FAV_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        for(uint8_t i = 0; i < app->fav_count; i++) {
            storage_file_write(file, app->fav_path[i], (uint16_t)strlen(app->fav_path[i]));
            storage_file_write(file, "\n", 1);
        }
    }
    storage_file_close(file);
    storage_file_free(file);
}

static void fav_load(App* app) {
    app->fav_count = 0;
    File* file = storage_file_alloc(app->storage);
    if(storage_file_open(file, FAV_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        const uint16_t cap = 2048;
        char* buf = malloc(cap + 1);
        const uint16_t len = storage_file_read(file, buf, cap);
        buf[len] = '\0';
        char* line = buf;
        while(*line != '\0' && app->fav_count < FAV_MAX) {
            char* end = strchr(line, '\n');
            if(end) *end = '\0';
            if(line[0] != '\0' && strlen(line) < PATH_LEN) {
                snprintf(app->fav_path[app->fav_count++], PATH_LEN, "%s", line);
            }
            if(!end) break;
            line = end + 1;
        }
        free(buf);
    }
    storage_file_close(file);
    storage_file_free(file);
}

static void fav_remove_at(App* app, uint8_t index) {
    if(index >= app->fav_count) return;
    for(uint8_t i = index; i + 1 < app->fav_count; i++) {
        memcpy(app->fav_path[i], app->fav_path[i + 1], PATH_LEN);
    }
    app->fav_count--;
    if(app->fav_sel >= app->fav_count && app->fav_sel > 0) app->fav_sel--;
    fav_save(app);
}

/* Star or unstar the figure in the selected row. */
static void fav_toggle_row(App* app) {
    const char* path = app->slot_path[ROW_SLOT[app->sel]];
    if(path[0] == '\0') {
        set_message(app, "Only saved files star");
        return;
    }
    const int found = fav_find(app, path);
    if(found >= 0) {
        fav_remove_at(app, (uint8_t)found);
        set_message(app, "Removed favorite");
    } else if(app->fav_count >= FAV_MAX) {
        set_message(app, "Favorites full (20)");
    } else {
        char copy[PATH_LEN];
        snprintf(copy, PATH_LEN, "%s", path);
        memcpy(app->fav_path[app->fav_count++], copy, PATH_LEN);
        fav_save(app);
        set_message(app, "Added favorite *");
    }
}

static void fav_use(App* app, uint8_t index) {
    uint8_t data[INF_FIGURE_SIZE];
    if(index >= app->fav_count) return;
    if(read_file(app, app->fav_path[index], data)) {
        put_in_slot(app, ROW_SLOT[app->sel], data, app->fav_path[index]);
    } else {
        set_message(app, "Favorite file missing");
    }
}

/* Left/Right on a row: swap in the previous/next favorite. */
static void quick_switch(App* app, int direction) {
    if(app->fav_count == 0) {
        set_message(app, "No favorites: hold OK");
        return;
    }
    const int current = fav_find(app, app->slot_path[ROW_SLOT[app->sel]]);
    int next;
    if(current < 0) {
        next = (direction > 0) ? 0 : app->fav_count - 1;
    } else {
        next = (current + direction + app->fav_count) % app->fav_count;
    }
    fav_use(app, (uint8_t)next);
}

static void fav_label(const char* path, char* out, size_t size) {
    snprintf(out, size, "%s", base_name(path));
    char* dot = strrchr(out, '.');
    if(dot) *dot = '\0';
}

static void pick_file(App* app) {
    FuriString* path = furi_string_alloc_set_str(APP_DIR);
    DialogsFileBrowserOptions options;
    dialog_file_browser_set_basic_options(&options, ".bin", NULL);
    if(dialog_file_browser_show(app->dialogs, path, path, &options)) {
        uint8_t data[INF_FIGURE_SIZE];
        if(read_file(app, furi_string_get_cstr(path), data)) {
            put_in_slot(app, ROW_SLOT[app->sel], data, furi_string_get_cstr(path));
        } else {
            set_message(app, "Need a 320-byte .bin");
        }
    }
    furi_string_free(path);
}

/* Make a blank figure and put it straight on the base. It only exists in RAM: no
 * file is written, so the game's progress on it is gone once it is removed or the
 * app closes. (An empty slot_path is what tells flush_dirty() not to save.) */
static void create_figure(App* app) {
    const InfFigureInfo* info = group_get(app->create_group, app->create_index);
    if(info == NULL) return;

    uint8_t uid[7];
    furi_hal_random_fill_buf(uid, sizeof(uid));
    for(size_t i = 0; i < sizeof(uid); i++)
        uid[i] = (uint8_t)(uid[i] % 255);

    uint8_t data[INF_FIGURE_SIZE];
    if(!inf_figure_create_blank(data, info->id, info->series, uid)) {
        set_message(app, "Cannot create that one");
        return;
    }

    put_in_slot(app, ROW_SLOT[app->sel], data, "");
    app->mode = ModeSlots;
    set_message(app, "Blank, not saved");
}

/* What the console sent, newest last. Hold Left in the slot list to save it. */
static void save_debug(App* app) {
    InfLogEntry log[INF_LOG_ENTRIES];
    uint8_t next;
    uint32_t total, commands, dropped;
    bool handshake;
    FURI_CRITICAL_ENTER();
    memcpy(log, usb_inf_base.log, sizeof(log));
    next = usb_inf_base.log_next;
    total = usb_inf_base.log_total;
    commands = usb_inf_base.commands_seen;
    dropped = usb_inf_base.dropped_packets;
    handshake = usb_inf_base.handshake_seen;
    FURI_CRITICAL_EXIT();

    File* file = storage_file_alloc(app->storage);
    if(!storage_file_open(file, APP_DIR "/debug.txt", FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(file);
        set_message(app, "Log save failed");
        return;
    }
    char line[96];
    int len = snprintf(
        line,
        sizeof(line),
        "cfg=%lu out=%lu in=%lu busy=%lu susp=%lu\n",
        (unsigned long)usb_inf_stats.enum_configs,
        (unsigned long)usb_inf_stats.out_packets,
        (unsigned long)usb_inf_stats.in_packets,
        (unsigned long)usb_inf_stats.in_busy,
        (unsigned long)usb_inf_stats.suspends);
    storage_file_write(file, line, (uint16_t)len);
    len = snprintf(
        line,
        sizeof(line),
        "ms_since_last_rx=%lu ctl=%lu rejected=%lu (last type %02X req %02X)\n",
        (unsigned long)(furi_get_tick() - usb_inf_stats.last_rx_tick),
        (unsigned long)usb_inf_stats.ctrl_requests,
        (unsigned long)usb_inf_stats.ctrl_rejected,
        usb_inf_stats.last_rejected_type,
        usb_inf_stats.last_rejected_req);
    storage_file_write(file, line, (uint16_t)len);
    len = snprintf(
        line,
        sizeof(line),
        "commands=%lu dropped=%lu handshake=%s\n",
        (unsigned long)commands,
        (unsigned long)dropped,
        handshake ? "yes" : "no");
    storage_file_write(file, line, (uint16_t)len);

    const uint32_t count = total < INF_LOG_ENTRIES ? total : INF_LOG_ENTRIES;
    const uint32_t start = (next + INF_LOG_ENTRIES - count) % INF_LOG_ENTRIES;
    for(uint32_t i = 0; i < count; i++) {
        const InfLogEntry* e = &log[(start + i) % INF_LOG_ENTRIES];
        len = snprintf(
            line,
            sizeof(line),
            "cmd %02X seq %02X a %02X b %02X\n",
            e->command,
            e->sequence,
            e->arg0,
            e->arg1);
        storage_file_write(file, line, (uint16_t)len);
    }
    storage_file_close(file);
    storage_file_free(file);
    set_message(app, "Saved debug.txt");
}

/* --------------------------------------------------------------- UI */

/* Link state shown on screen and used for the disconnect alert. */
typedef enum {
    LinkWaiting,
    LinkStarting,
    LinkConnected,
    LinkSilent,
    LinkDisconnected,
} LinkState;

#define LINK_SILENT_MS 10000

static LinkState link_state(void) {
    if(usb_inf_stats.enum_configs == 0) return LinkWaiting;
    if(usb_inf_stats.suspended) return LinkDisconnected;
    if(!usb_inf_base.handshake_seen) return LinkStarting;
    if(furi_get_tick() - usb_inf_stats.last_rx_tick > LINK_SILENT_MS) return LinkSilent;
    return LinkConnected;
}

static const char* link_text(LinkState state) {
    switch(state) {
    case LinkWaiting:
        return "Plug into console";
    case LinkStarting:
        return "Console: starting...";
    case LinkConnected:
        return "Console: connected";
    case LinkSilent:
        return "Console: no signal!";
    default:
        return "DISCONNECTED!";
    }
}

static void draw_callback(Canvas* canvas, void* ctx) {
    App* app = ctx;
    char line[48];
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    if(app->mode == ModeCreate) {
        snprintf(line, sizeof(line), "New figure -> %s", ROW_LABEL[app->sel]);
        canvas_draw_str(canvas, 0, 8, line);
        snprintf(
            line,
            sizeof(line),
            "< %u.0 %s >",
            (unsigned)(app->create_group / 3) + 1,
            KIND_LABEL[app->create_group % 3]);
        canvas_draw_str(canvas, 0, 18, line);
        const InfFigureInfo* info = group_get(app->create_group, app->create_index);
        snprintf(line, sizeof(line), "%.24s", info ? info->name : "-");
        canvas_draw_str(canvas, 0, 34, line);
        snprintf(
            line,
            sizeof(line),
            "%u / %u",
            (unsigned)(app->create_index + 1),
            (unsigned)group_count(app->create_group));
        canvas_draw_str(canvas, 0, 44, line);
        canvas_draw_str(canvas, 0, 62, "Up/Dn pick  OK blank");
        return;
    }

    if(app->mode == ModeFavs) {
        snprintf(line, sizeof(line), "Favorites -> %s", ROW_LABEL[app->sel]);
        canvas_draw_str(canvas, 0, 8, line);
        if(app->fav_count == 0) {
            canvas_draw_str(canvas, 0, 30, "None yet. Hold OK on a");
            canvas_draw_str(canvas, 0, 40, "loaded figure to star it.");
        }
        uint8_t first = (app->fav_sel > 1) ? app->fav_sel - 1 : 0;
        for(uint8_t row = 0; row < 3 && (uint8_t)(first + row) < app->fav_count; row++) {
            char label[PATH_LEN];
            fav_label(app->fav_path[first + row], label, sizeof(label));
            snprintf(
                line, sizeof(line), "%c%.20s", (first + row) == app->fav_sel ? '>' : ' ', label);
            canvas_draw_str(canvas, 0, (uint8_t)(21 + row * 10), line);
        }
        canvas_draw_str(canvas, 0, 62, "OK load  hold OK remove");
        return;
    }

    canvas_draw_str(canvas, 0, 8, link_text(link_state()));
    uint8_t first_row = (app->sel > 1) ? app->sel - 1 : 0;
    if(first_row > ROWS - ROWS_VISIBLE) first_row = ROWS - ROWS_VISIBLE;
    for(uint8_t i = 0; i < ROWS_VISIBLE; i++) {
        const uint8_t row = first_row + i;
        const uint8_t slot = ROW_SLOT[row];
        const bool present = inf_base_slot_present(&usb_inf_base, slot);
        snprintf(
            line,
            sizeof(line),
            "%c%-10.10s%c%.12s",
            row == app->sel ? '>' : ' ',
            ROW_LABEL[row],
            (present && fav_find(app, app->slot_path[slot]) >= 0) ? '*' : ' ',
            present ? (app->slot_name[slot][0] ? app->slot_name[slot] : "figure") : "empty");
        canvas_draw_str(canvas, 0, (uint8_t)(21 + i * 10), line);
    }
    if(furi_get_tick() < app->message_until) {
        canvas_draw_str(canvas, 0, 62, app->message);
    } else {
        canvas_draw_str(canvas, 0, 62, "</> favs  holdOK star");
    }
}

static void input_callback(InputEvent* event, void* ctx) {
    FuriMessageQueue* queue = ctx;
    furi_message_queue_put(queue, event, 0);
}

static void handle_input(App* app, const InputEvent* event) {
    const bool step = (event->type == InputTypeShort || event->type == InputTypeRepeat);

    if(app->mode == ModeCreate) {
        const uint16_t count = group_count(app->create_group);
        if(step && event->key == InputKeyUp && count > 0) {
            app->create_index = (uint16_t)((app->create_index + count - 1) % count);
        } else if(step && event->key == InputKeyDown && count > 0) {
            app->create_index = (uint16_t)((app->create_index + 1) % count);
        } else if(event->type == InputTypeShort && event->key == InputKeyLeft) {
            app->create_group = (app->create_group + GROUPS - 1) % GROUPS;
            app->create_index = 0;
        } else if(event->type == InputTypeShort && event->key == InputKeyRight) {
            app->create_group = (app->create_group + 1) % GROUPS;
            app->create_index = 0;
        } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
            create_figure(app);
        } else if(event->type == InputTypeShort && event->key == InputKeyBack) {
            app->mode = ModeSlots;
        }
        return;
    }

    if(app->mode == ModeFavs) {
        if(step && event->key == InputKeyUp && app->fav_count > 0) {
            app->fav_sel = (uint8_t)((app->fav_sel + app->fav_count - 1) % app->fav_count);
        } else if(step && event->key == InputKeyDown && app->fav_count > 0) {
            app->fav_sel = (uint8_t)((app->fav_sel + 1) % app->fav_count);
        } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
            fav_use(app, app->fav_sel);
            app->mode = ModeSlots;
        } else if(event->type == InputTypeLong && event->key == InputKeyOk) {
            fav_remove_at(app, app->fav_sel);
        } else if(event->type == InputTypeShort && event->key == InputKeyBack) {
            app->mode = ModeSlots;
        }
        return;
    }

    const uint8_t slot = ROW_SLOT[app->sel];
    if(step && event->key == InputKeyUp) {
        app->sel = (uint8_t)((app->sel + ROWS - 1) % ROWS);
    } else if(step && event->key == InputKeyDown) {
        app->sel = (uint8_t)((app->sel + 1) % ROWS);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(inf_base_slot_present(&usb_inf_base, slot)) {
            remove_slot(app, slot);
        } else {
            /* Empty: ask New or File. */
            DialogMessage* m = dialog_message_alloc();
            dialog_message_set_header(m, ROW_LABEL[app->sel], 64, 0, AlignCenter, AlignTop);
            dialog_message_set_text(
                m, "File, Favorite or\nNew figure?", 64, 24, AlignCenter, AlignTop);
            dialog_message_set_buttons(m, "File", "Fav", "New");
            const DialogMessageButton r = dialog_message_show(app->dialogs, m);
            dialog_message_free(m);
            if(r == DialogMessageButtonRight) {
                app->create_index = 0;
                app->mode = ModeCreate;
            } else if(r == DialogMessageButtonCenter) {
                app->fav_sel = 0;
                app->mode = ModeFavs;
            } else if(r == DialogMessageButtonLeft) {
                pick_file(app);
            }
        }
    } else if(event->type == InputTypeLong && event->key == InputKeyOk) {
        fav_toggle_row(app);
    } else if(event->type == InputTypeShort && event->key == InputKeyLeft) {
        quick_switch(app, -1);
    } else if(event->type == InputTypeShort && event->key == InputKeyRight) {
        quick_switch(app, 1);
    } else if(event->type == InputTypeLong && event->key == InputKeyRight) {
        save_debug(app);
    } else if(event->type == InputTypeShort && event->key == InputKeyBack) {
        app->running = false;
    }
}

int32_t flipbase_app(void* p) {
    UNUSED(p);

    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));
    app->running = true;
    app->create_group = 6; /* Infinity 3.0 characters */

    app->storage = furi_record_open(RECORD_STORAGE);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    storage_simply_mkdir(app->storage, APP_DATA_DIR);
    storage_simply_mkdir(app->storage, APP_DIR);

    inf_base_init(&usb_inf_base);
    fav_load(app);

    app->queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app->queue);
    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
    /* Keep the screen on so the Flipper never idles down while the game is running. */
    notification_message_block(notifications, &sequence_display_backlight_enforce_on);

    usb_infinity_start();

    LinkState last_link = LinkWaiting;
    while(app->running) {
        InputEvent event;
        if(furi_message_queue_get(app->queue, &event, 250) == FuriStatusOk) {
            handle_input(app, &event);
        }
        /* Buzz once when a working connection drops. */
        const LinkState now_link = link_state();
        if(now_link != last_link) {
            if((now_link == LinkDisconnected || now_link == LinkSilent) &&
               last_link == LinkConnected) {
                notification_message(notifications, &sequence_single_vibro);
                set_message(app, "Console disconnected");
            } else if(
                now_link == LinkConnected && last_link != LinkWaiting &&
                last_link != LinkStarting) {
                set_message(app, "Console reconnected");
            }
            last_link = now_link;
        }
        usb_infinity_kick(); /* retry a send if the endpoint ever stalled */
        flush_dirty(app);
        view_port_update(app->view_port);
    }

    flush_dirty(app);
    usb_infinity_stop();
    notification_message_block(notifications, &sequence_display_backlight_enforce_auto);
    furi_record_close(RECORD_NOTIFICATION);

    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(app->view_port);
    furi_message_queue_free(app->queue);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_STORAGE);
    free(app);
    return 0;
}
