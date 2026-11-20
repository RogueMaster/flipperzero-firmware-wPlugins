#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_input.h>
#include <storage/storage.h>
#include <expansion/expansion.h>
#include <dialogs/dialogs.h>
#include "font_4x6.h"

#define RX_BUF_SIZE 8192
#define WBUF_SIZE   2048
#define DMA_CHUNK   64

/* screen: 4x6 font -> 32 cols, 9 text rows + status row */
#define COLS     32
#define LINES    64
#define ROWS     9
#define HEX_LINE 8

#define MAX_DIRS 32
#define NAME_LEN 32
#define DEFAULT_NAME "uart_logs"

typedef enum {
    SceneConfig,
    SceneLog,
    SceneName,
    SceneNum
} SceneId;
typedef enum {
    ViewConfig,
    ViewLog,
    ViewName
} ViewId;
typedef enum {
    EvtStart,
    EvtEditName,
    EvtNameDone,
    EvtToggleRec
} EvtId;

#define FLAG_STOP (1 << 0)
#define FLAG_RX   (1 << 1)

static const uint32_t bauds[] = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
static const char* baud_names[] =
    {"9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600"};
#define BAUD_COUNT 8

typedef struct {
    char lines[LINES][COLS + 1];
    uint16_t head; // current (last) line
    uint16_t count;
    uint8_t col;
    uint8_t hexcol;
    uint16_t scroll; // 0 = follow bottom
    uint32_t bytes;
    uint8_t state; // 0 pause, 1 rec, 2 error, 3 no file
    bool busy;
} LogModel;

typedef struct {
    Gui* gui;
    ViewDispatcher* vd;
    SceneManager* sm;
    VariableItemList* vil;
    TextInput* ti;
    View* log_view;
    Storage* storage;
    Expansion* expansion;

    uint8_t baud_idx;
    uint8_t pins; // 0 USART, 1 LPUART
    uint8_t hex;

    /* save location: /ext[/<base>]/<name> */
    char bases[MAX_DIRS + 2][NAME_LEN]; // [0] = app data, [1] = SD root
    uint8_t base_count;
    uint8_t base_idx;
    char name[NAME_LEN];
    char name_edit[NAME_LEN];
    VariableItem* name_item;
    FuriString* path;

    FuriHalSerialHandle* serial;
    FuriStreamBuffer* rx;
    FuriThread* worker;
    FuriMutex* mutex; // guards file state

    File* file;
    bool rec;
    bool err;
    uint8_t wbuf[WBUF_SIZE];
    size_t wlen;
    uint8_t file_col;
} App;

/* ---------- serial (ISR) ---------- */
static void rx_cb(FuriHalSerialHandle* h, FuriHalSerialRxEvent ev, size_t size, void* ctx) {
    App* app = ctx;
    if(ev & (FuriHalSerialRxEventData | FuriHalSerialRxEventIdle)) {
        uint8_t d[DMA_CHUNK];
        while(size) {
            size_t n = furi_hal_serial_dma_rx(h, d, size > DMA_CHUNK ? DMA_CHUNK : size);
            if(!n) break;
            furi_stream_buffer_send(app->rx, d, n, 0);
            size -= n;
        }
        furi_thread_flags_set(furi_thread_get_id(app->worker), FLAG_RX);
    }
}

/* ---------- file (call with mutex held) ---------- */
static void file_write_out(App* app) {
    if(app->file && app->wlen && !app->err) {
        if(storage_file_write(app->file, app->wbuf, app->wlen) != app->wlen) {
            app->err = true;
            app->rec = false;
        }
    }
    app->wlen = 0;
}

static void file_put(App* app, const uint8_t* d, size_t n) {
    while(n) {
        if(app->wlen == WBUF_SIZE) file_write_out(app);
        size_t k = WBUF_SIZE - app->wlen;
        if(k > n) k = n;
        memcpy(app->wbuf + app->wlen, d, k);
        app->wlen += k;
        d += k;
        n -= k;
    }
}

static void log_dir(App* app, FuriString* out) {
    if(app->base_idx == 0) {
        /* default: /ext/apps_data/uart_logger */
        furi_string_set(out, APP_DATA_PATH(""));
        furi_string_left(out, furi_string_size(out) - 1);
        storage_common_resolve_path_and_ensure_app_directory(app->storage, out);
    } else if(app->base_idx == 1)
        furi_string_printf(out, "/ext/%s", app->name);
    else
        furi_string_printf(out, "/ext/%s/%s", app->bases[app->base_idx], app->name);
}

static void file_open(App* app) {
    log_dir(app, app->path);
    storage_simply_mkdir(app->storage, furi_string_get_cstr(app->path));
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    furi_string_cat_printf(
        app->path,
        "/log_%04u%02u%02u_%02u%02u%02u.txt",
        dt.year,
        dt.month,
        dt.day,
        dt.hour,
        dt.minute,
        dt.second);
    app->wlen = 0;
    app->file_col = 0;
    app->err = false;
    app->file = storage_file_alloc(app->storage);
    if(storage_file_open(
           app->file, furi_string_get_cstr(app->path), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        app->rec = true;
    } else {
        storage_file_free(app->file);
        app->file = NULL;
        app->rec = false;
        furi_string_reset(app->path);
    }
}

static void file_close(App* app) {
    if(app->file) {
        file_write_out(app);
        storage_file_close(app->file);
        storage_file_free(app->file);
        app->file = NULL;
    }
    app->rec = false;
}

static void scan_bases(App* app) {
    strcpy(app->bases[0], "App data");
    strcpy(app->bases[1], "SD root");
    app->base_count = 2;
    File* dir = storage_file_alloc(app->storage);
    if(storage_dir_open(dir, "/ext")) {
        FileInfo fi;
        char nm[NAME_LEN];
        while(app->base_count < MAX_DIRS + 2 && storage_dir_read(dir, &fi, nm, sizeof(nm))) {
            if(!file_info_is_dir(&fi) || nm[0] == '.') continue;
                        strlcpy(app->bases[app->base_count++], nm, NAME_LEN);
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
}

/* ---------- screen buffer ---------- */
static void scr_newline(LogModel* m) {
    m->head = (m->head + 1) % LINES;
    m->lines[m->head][0] = 0;
    m->col = 0;
    if(m->count < LINES) m->count++;
    if(m->scroll && m->scroll < m->count - ROWS) m->scroll++; // keep view still while scrolled
}

static inline void scr_putc(LogModel* m, char c) {
    if(m->col >= COLS) scr_newline(m);
    m->lines[m->head][m->col++] = c;
    m->lines[m->head][m->col] = 0;
}

/* ---------- worker ---------- */
static const char hx[] = "0123456789ABCDEF";

static void process(App* app, LogModel* m, const uint8_t* d, size_t n) {
    m->bytes += n;
    bool rec = app->rec;
    for(size_t i = 0; i < n; i++) {
        uint8_t b = d[i];
        if(app->hex) {
            if(m->hexcol == HEX_LINE) {
                scr_newline(m);
                m->hexcol = 0;
            }
            scr_putc(m, hx[b >> 4]);
            scr_putc(m, hx[b & 0xF]);
            if(++m->hexcol < HEX_LINE) scr_putc(m, ' ');
            if(rec) {
                uint8_t s[3] = {hx[b >> 4], hx[b & 0xF], ' '};
                if(++app->file_col >= 16) {
                    s[2] = '\n';
                    app->file_col = 0;
                }
                file_put(app, s, 3);
            }
        } else {
            if(b == '\n')
                scr_newline(m);
            else if(b >= 0x20 && b < 0x7F)
                scr_putc(m, b);
            else if(b == '\t')
                scr_putc(m, ' ');
            else if(b != '\r')
                scr_putc(m, '.');
        }
    }
    if(rec && !app->hex) file_put(app, d, n);
}

static uint8_t rec_state(App* app) {
    return app->err ? 2 : (app->file ? (app->rec ? 1 : 0) : 3);
}

static int32_t worker(void* ctx) {
    App* app = ctx;
    uint8_t buf[256];
    uint32_t last_flush = furi_get_tick();
    uint32_t last_ui = 0;
    bool dirty = false;
    for(;;) {
        uint32_t f = furi_thread_flags_wait(FLAG_STOP | FLAG_RX, FuriFlagWaitAny, 100);
        if(!(f & FuriFlagError) && (f & FLAG_STOP)) break;

        size_t n;
        while((n = furi_stream_buffer_receive(app->rx, buf, sizeof(buf), 0)) > 0) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            LogModel* m = view_get_model(app->log_view);
            process(app, m, buf, n);
            m->state = rec_state(app);
            view_commit_model(app->log_view, false);
            furi_mutex_release(app->mutex);
            dirty = true;
            if(furi_thread_flags_get() & FLAG_STOP) break;
        }

        uint32_t now = furi_get_tick();
        if(now - last_flush >= 1000) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            if(app->rec) {
                file_write_out(app);
                if(app->file && !app->err) storage_file_sync(app->file);
            }
            furi_mutex_release(app->mutex);
            last_flush = now;
        }
        if(dirty && now - last_ui >= 100) {
            with_view_model(app->log_view, LogModel * m, { UNUSED(m); }, true);
            last_ui = now;
            dirty = false;
        }
    }
    return 0;
}

/* ---------- log view ---------- */
static void log_draw(Canvas* canvas, void* model) {
    LogModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_custom_u8g2_font(canvas, u8g2_font_tom_thumb_4x6_tr);

    uint16_t last = (m->head + LINES - m->scroll) % LINES;
    uint16_t shown = m->count - m->scroll;
    if(shown > ROWS) shown = ROWS;
    for(uint16_t i = 0; i < shown; i++) {
        uint16_t idx = (last + LINES - (shown - 1 - i)) % LINES;
        canvas_draw_str(canvas, 0, 6 * i + 5, m->lines[idx]);
    }

    canvas_draw_line(canvas, 0, 56, 127, 56);
    static const char* st[] = {"PAUSE", "REC", "SD ERR", "NO FILE"};
    char s[COLS + 1];
    if(m->busy)
        snprintf(s, sizeof(s), "UART BUSY");
    else if(m->scroll)
        snprintf(s, sizeof(s), "%lu B %s ^%u", (unsigned long)m->bytes, st[m->state], m->scroll);
    else
        snprintf(s, sizeof(s), "%lu B %s", (unsigned long)m->bytes, st[m->state]);
    canvas_draw_str(canvas, 0, 63, s);
}

static bool log_input(InputEvent* ev, void* ctx) {
    App* app = ctx;
    if(ev->key == InputKeyOk) {
        if(ev->type == InputTypeShort) view_dispatcher_send_custom_event(app->vd, EvtToggleRec);
        return true;
    }
    if(ev->key == InputKeyUp || ev->key == InputKeyDown) {
        if(ev->type == InputTypeShort || ev->type == InputTypeRepeat) {
            with_view_model(
                app->log_view,
                LogModel * m,
                {
                    uint16_t max = m->count > ROWS ? m->count - ROWS : 0;
                    if(ev->key == InputKeyUp && m->scroll < max) m->scroll++;
                    if(ev->key == InputKeyDown && m->scroll) m->scroll--;
                },
                true);
        }
        return true;
    }
    return false;
}

static void serial_stop(App* app) {
    if(app->serial) {
        furi_hal_serial_dma_rx_stop(app->serial);
        furi_hal_serial_deinit(app->serial);
        furi_hal_serial_control_release(app->serial);
        app->serial = NULL;
    }
}

static void worker_stop(App* app) {
    if(app->worker) {
        furi_thread_flags_set(furi_thread_get_id(app->worker), FLAG_STOP);
        furi_thread_join(app->worker);
        furi_thread_free(app->worker);
        app->worker = NULL;
    }
}

static void scene_log_enter(void* ctx) {
    App* app = ctx;
    furi_stream_buffer_reset(app->rx);
    file_open(app);

    with_view_model(
        app->log_view,
        LogModel * m,
        {
            memset(m, 0, sizeof(LogModel));
            m->count = 1;
            m->state = rec_state(app);
        },
        true);

    app->worker = furi_thread_alloc_ex("UartLogWorker", 2048, worker, app);
    furi_thread_start(app->worker);

    app->serial = furi_hal_serial_control_acquire(
        app->pins ? FuriHalSerialIdLpuart : FuriHalSerialIdUsart);
    if(app->serial) {
        furi_hal_serial_init(app->serial, bauds[app->baud_idx]);
        furi_hal_serial_dma_rx_start(app->serial, rx_cb, app, false);
    } else {
        with_view_model(app->log_view, LogModel * m, { m->busy = true; }, true);
    }
    view_dispatcher_switch_to_view(app->vd, ViewLog);
}

static bool scene_log_event(void* ctx, SceneManagerEvent ev) {
    App* app = ctx;
    if(ev.type == SceneManagerEventTypeBack) {
        serial_stop(app);
        worker_stop(app);

        bool had = app->file != NULL;
        file_close(app);
        if(!had) return false;

        DialogsApp* dlg = furi_record_open(RECORD_DIALOGS);
        DialogMessage* m = dialog_message_alloc();
        dialog_message_set_header(m, "Save log?", 64, 2, AlignCenter, AlignTop);
        dialog_message_set_text(
            m, furi_string_get_cstr(app->path), 64, 32, AlignCenter, AlignCenter);
        dialog_message_set_buttons(m, "No", NULL, "Yes");
        if(dialog_message_show(dlg, m) == DialogMessageButtonRight) {
            dialog_message_set_header(m, "Saved to:", 64, 2, AlignCenter, AlignTop);
            dialog_message_set_buttons(m, NULL, "OK", NULL);
            dialog_message_show(dlg, m);
        } else {
            storage_simply_remove(app->storage, furi_string_get_cstr(app->path));
        }
        dialog_message_free(m);
        furi_record_close(RECORD_DIALOGS);
        return false;
    }
    if(ev.type == SceneManagerEventTypeCustom && ev.event == EvtToggleRec) {
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        if(app->file && !app->err) {
            if(app->rec) file_write_out(app);
            app->rec = !app->rec;
        }
        uint8_t st = rec_state(app);
        furi_mutex_release(app->mutex);
        with_view_model(app->log_view, LogModel * m, { m->state = st; }, true);
        return true;
    }
    return false;
}

static void scene_log_exit(void* ctx) {
    App* app = ctx;
    serial_stop(app);
    worker_stop(app);
    file_close(app);
}

/* ---------- name scene ---------- */
static void name_done_cb(void* ctx) {
    App* app = ctx;
    view_dispatcher_send_custom_event(app->vd, EvtNameDone);
}

static void scene_name_enter(void* ctx) {
    App* app = ctx;
    strlcpy(app->name_edit, app->name, NAME_LEN);
    text_input_reset(app->ti);
    text_input_set_header_text(app->ti, "Log folder name");
    text_input_set_result_callback(app->ti, name_done_cb, app, app->name_edit, NAME_LEN, false);
    view_dispatcher_switch_to_view(app->vd, ViewName);
}

static bool scene_name_event(void* ctx, SceneManagerEvent ev) {
    App* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom && ev.event == EvtNameDone) {
        if(app->name_edit[0]) strlcpy(app->name, app->name_edit, NAME_LEN);
        scene_manager_previous_scene(app->sm);
        return true;
    }
    return false;
}

static void scene_name_exit(void* ctx) {
    App* app = ctx;
    text_input_reset(app->ti);
}

/* ---------- config scene ---------- */
static void baud_changed(VariableItem* it) {
    App* app = variable_item_get_context(it);
    app->baud_idx = variable_item_get_current_value_index(it);
    variable_item_set_current_value_text(it, baud_names[app->baud_idx]);
}

static void pins_changed(VariableItem* it) {
    App* app = variable_item_get_context(it);
    app->pins = variable_item_get_current_value_index(it);
    variable_item_set_current_value_text(it, app->pins ? "15/16 LP" : "13/14");
}

static void mode_changed(VariableItem* it) {
    App* app = variable_item_get_context(it);
    app->hex = variable_item_get_current_value_index(it);
    variable_item_set_current_value_text(it, app->hex ? "Hex" : "Text");
}

static void base_changed(VariableItem* it) {
    App* app = variable_item_get_context(it);
    app->base_idx = variable_item_get_current_value_index(it);
    variable_item_set_current_value_text(it, app->bases[app->base_idx]);
}

static void cfg_enter_cb(void* ctx, uint32_t index) {
    App* app = ctx;
    if(index == 4) view_dispatcher_send_custom_event(app->vd, EvtEditName);
    if(index == 5) view_dispatcher_send_custom_event(app->vd, EvtStart);
}

static void add_item(
    App* app,
    const char* label,
    uint8_t count,
    VariableItemChangeCallback cb,
    uint8_t idx,
    const char* text) {
    VariableItem* it = variable_item_list_add(app->vil, label, count, cb, app);
    variable_item_set_current_value_index(it, idx);
    variable_item_set_current_value_text(it, text);
}

static void scene_config_enter(void* ctx) {
    App* app = ctx;
    VariableItemList* l = app->vil;
    variable_item_list_reset(l);
    add_item(app, "Baudrate", BAUD_COUNT, baud_changed, app->baud_idx, baud_names[app->baud_idx]);
    add_item(app, "Pins TX/RX", 2, pins_changed, app->pins, app->pins ? "15/16 LP" : "13/14");
    add_item(app, "Mode", 2, mode_changed, app->hex, app->hex ? "Hex" : "Text");
    add_item(app, "Save in", app->base_count, base_changed, app->base_idx, app->bases[app->base_idx]);
    VariableItem* it = variable_item_list_add(l, "Folder", 1, NULL, app);
    variable_item_set_current_value_text(it, app->name);
    variable_item_list_add(l, "Start", 0, NULL, app);
    variable_item_list_set_enter_callback(l, cfg_enter_cb, app);
    variable_item_list_set_selected_item(
        l, scene_manager_get_scene_state(app->sm, SceneConfig));
    view_dispatcher_switch_to_view(app->vd, ViewConfig);
}

static bool scene_config_event(void* ctx, SceneManagerEvent ev) {
    App* app = ctx;
    if(ev.type != SceneManagerEventTypeCustom) return false;
    if(ev.event == EvtStart) {
        scene_manager_set_scene_state(app->sm, SceneConfig, 5);
        scene_manager_next_scene(app->sm, SceneLog);
        return true;
    }
    if(ev.event == EvtEditName) {
        scene_manager_set_scene_state(app->sm, SceneConfig, 4);
        scene_manager_next_scene(app->sm, SceneName);
        return true;
    }
    return false;
}

static void scene_config_exit(void* ctx) {
    UNUSED(ctx);
}

static void (*const scn_enter[])(void*) = {scene_config_enter, scene_log_enter, scene_name_enter};
static bool (*const scn_event[])(void*, SceneManagerEvent) = {
    scene_config_event,
    scene_log_event,
    scene_name_event};
static void (*const scn_exit[])(void*) = {scene_config_exit, scene_log_exit, scene_name_exit};
static const SceneManagerHandlers scene_handlers = {
    .on_enter_handlers = scn_enter,
    .on_event_handlers = scn_event,
    .on_exit_handlers = scn_exit,
    .scene_num = SceneNum,
};

/* ---------- dispatcher ---------- */
static bool custom_cb(void* ctx, uint32_t ev) {
    App* app = ctx;
    return scene_manager_handle_custom_event(app->sm, ev);
}

static bool back_cb(void* ctx) {
    App* app = ctx;
    return scene_manager_handle_back_event(app->sm);
}

int32_t uart_logger_app(void* p) {
    UNUSED(p);
    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));
    app->baud_idx = 4; // 115200
    strlcpy(app->name, DEFAULT_NAME, NAME_LEN);

    app->expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(app->expansion);

    app->storage = furi_record_open(RECORD_STORAGE);
    app->gui = furi_record_open(RECORD_GUI);
    app->rx = furi_stream_buffer_alloc(RX_BUF_SIZE, 1);
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->path = furi_string_alloc();
    scan_bases(app);

    app->vd = view_dispatcher_alloc();
    app->sm = scene_manager_alloc(&scene_handlers, app);
    view_dispatcher_set_event_callback_context(app->vd, app);
    view_dispatcher_set_custom_event_callback(app->vd, custom_cb);
    view_dispatcher_set_navigation_event_callback(app->vd, back_cb);

    app->vil = variable_item_list_alloc();
    view_dispatcher_add_view(app->vd, ViewConfig, variable_item_list_get_view(app->vil));

    app->ti = text_input_alloc();
    view_dispatcher_add_view(app->vd, ViewName, text_input_get_view(app->ti));

    app->log_view = view_alloc();
    view_allocate_model(app->log_view, ViewModelTypeLocking, sizeof(LogModel));
    view_set_context(app->log_view, app);
    view_set_draw_callback(app->log_view, log_draw);
    view_set_input_callback(app->log_view, log_input);
    view_dispatcher_add_view(app->vd, ViewLog, app->log_view);

    view_dispatcher_attach_to_gui(app->vd, app->gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->sm, SceneConfig);
    view_dispatcher_run(app->vd);

    view_dispatcher_remove_view(app->vd, ViewConfig);
    view_dispatcher_remove_view(app->vd, ViewName);
    view_dispatcher_remove_view(app->vd, ViewLog);
    variable_item_list_free(app->vil);
    text_input_free(app->ti);
    view_free(app->log_view);
    scene_manager_free(app->sm);
    view_dispatcher_free(app->vd);

    furi_string_free(app->path);
    furi_mutex_free(app->mutex);
    furi_stream_buffer_free(app->rx);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);
    expansion_enable(app->expansion);
    furi_record_close(RECORD_EXPANSION);
    free(app);
    return 0;
}
