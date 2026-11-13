/*
 * RFidentifier v1.0 - created by Marcopiiit
 * Flipper Zero (Momentum firmware) external app.
 *
 *  - Read      : listen for a Sub-GHz signal and check if it is already saved
 *  - Duplicate : find duplicate signals between the saved .sub files
 *  - Settings  : frequency + search mode (Decoded / RAW / Both)
 *  - About
 */

#include <furi.h>
#include <furi_hal.h>

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/modules/text_box.h>

#include <storage/storage.h>
#include <toolbox/stream/stream.h>
#include <toolbox/stream/buffered_file_stream.h>
#include <flipper_format/flipper_format.h>

#include <lib/subghz/types.h>
#include <lib/subghz/environment.h>
#include <lib/subghz/receiver.h>
#include <lib/subghz/subghz_worker.h>
#include <lib/subghz/registry.h>
#include <lib/subghz/protocols/base.h>

/* Exported by the Momentum firmware (declared here because protocol_items.h
 * is not shipped with the uFBT SDK). */
extern const SubGhzProtocolRegistry subghz_protocol_registry;
#include <lib/subghz/devices/devices.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define TAG "RFidentifier"

#define SUBGHZ_DIR "/ext/subghz"

/* Tunables */
#define CAP_MAX        700 /* max captured pulses                      */
#define MIN_PULSES     60 /* min pulses to consider a capture valid   */
#define RAW_MAX        4096 /* max RAW values read from one .sub file   */
#define SIG_LEN        24 /* length of the RAW "fingerprint"          */
#define GAP_US         2500 /* a gap >= this marks the start of a frame */
#define MAX_FILES      400 /* max files loaded for duplicate search    */
#define MAX_DUP_PAIRS  60
#define MAX_MATCHES    30
#define WARMUP_MS      600 /* ignore the radio right after it starts */
#define END_MS         150

typedef enum {
    ViewMenu,
    ViewRead,
    ViewDup,
    ViewSettings,
    ViewAbout,
    ViewTextBox,
    ViewProgress,
    ViewFolder,
} ViewId;

typedef enum {
    MenuRead,
    MenuDuplicate,
    MenuSettings,
    MenuAbout,
} MenuIndex;

typedef enum {
    EvtSignal = 1,
    EvtRetry,
    EvtDupScan,
    EvtReadDone,
    EvtDupDone,
} CustomEvt;

typedef enum {
    ModeDecoded,
    ModeRaw,
    ModeBoth,
} SearchMode;

static const uint32_t freq_values[] = {
    300000000, 303875000, 304250000, 310000000, 315000000, 318000000,
    330000000, 345000000, 390000000, 433075000, 433420000, 433920000,
    434420000, 434775000, 438900000, 868350000, 915000000, 925000000,
};
#define FREQ_COUNT (sizeof(freq_values) / sizeof(freq_values[0]))
#define FREQ_DEFAULT 11 /* 433.92 MHz */
static char freq_text[FREQ_COUNT][16];

static const char* mode_text[] = {"Decoded", "RAW", "Both"};

typedef enum {
    ScopeAll, /* /ext/subghz and every subfolder          */
    ScopeSaved, /* only files directly inside /ext/subghz   */
    ScopeFolder, /* a chosen folder and its subfolders       */
} SearchScope;

static const char* scope_text[] = {"All files", "Saved only", "Folder"};

#define FOLDER_MAX     32
#define FOLDER_NAME    48
#define FOLDER_BASE    100

typedef struct {
    bool has_dec;
    bool has_raw;
    char proto[32];
    uint64_t key;
    size_t raw_n;
} SubInfo;

typedef struct {
    FuriString* path;
    uint8_t flags; /* 1 = decoded, 2 = raw */
    uint32_t proto_hash;
    uint64_t key;
    int16_t sig[SIG_LEN];
} Entry;

typedef struct App App;

typedef bool (*FileCb)(App* app, const char* path, void* ctx);

struct App {
    Gui* gui;
    ViewDispatcher* vd;
    Submenu* menu;
    VariableItemList* settings;
    Widget* w_read;
    Widget* w_about;
    TextBox* textbox;
    FuriString* text;
    Storage* storage;

    /* settings */
    uint8_t freq_idx;
    uint8_t mode;

    /* radio */
    bool radio_on;
    const SubGhzDevice* device;
    SubGhzEnvironment* env;
    SubGhzReceiver* receiver;
    SubGhzWorker* worker;
    FuriTimer* timer;

    /* capture */
    volatile bool capturing;
    volatile bool done;
    volatile uint16_t cap_count;
    uint32_t last_act;
    int16_t cap[CAP_MAX];

    /* decoded result */
    volatile bool dec_ready;
    char dec_proto[32];
    uint64_t dec_key;

    /* scanning helpers */
    int16_t* raw_buf;
    bool scan_stop;

    /* scope / folder selection */
    uint8_t scope;
    char folder[128];
    char folder_label[40];
    VariableItem* folder_item;
    Submenu* folder_menu;
    char browse[128];
    char browse_hdr[48];
    char folder_names[FOLDER_MAX][FOLDER_NAME];

    /* background search */
    FuriThread* thread;
    void* rctx;
    float rssi_start;
    uint8_t sens_idx;
    uint32_t start_tick;

    /* progress */
    View* progress;
    char prog_title[24];
    volatile bool cancelled;
    bool counting;
    uint8_t max_depth;
    uint32_t scan_total;
    uint32_t scan_done;
};

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static uint32_t hash_str(const char* s) {
    uint32_t h = 5381;
    while(*s) h = ((h << 5) + h) + (uint8_t)*s++;
    return h;
}

static uint64_t parse_key(const char* s) {
    uint64_t key = 0;
    const char* p = s;
    while(*p) {
        while(*p == ' ') p++;
        if(!*p || *p == '\r' || *p == '\n') break;
        char* end;
        unsigned long v = strtoul(p, &end, 16);
        if(end == p) break;
        key = (key << 8) | (v & 0xFF);
        p = end;
    }
    return key;
}

static const char* rel_name(const char* path) {
    size_t n = strlen(SUBGHZ_DIR) + 1;
    return (strlen(path) > n) ? path + n : path;
}

static void append_name(FuriString* out, const char* path) {
    const char* r = rel_name(path);
    size_t len = strlen(r);
    if(len > 4 && strcmp(r + len - 4, ".sub") == 0) len -= 4;
    furi_string_cat_printf(out, "%.*s", (int)len, r);
}

static bool dur_close(int16_t a, int16_t b) {
    if((a < 0) != (b < 0)) return false;
    int x = abs(a), y = abs(b);
    int d = abs(x - y);
    int m = x > y ? x : y;
    return d <= 120 + m / 4;
}

static bool sig_equal(const int16_t* a, const int16_t* b) {
    int ok = 0;
    for(int i = 0; i < SIG_LEN; i++)
        if(dur_close(a[i], b[i])) ok++;
    return ok >= (SIG_LEN * 9) / 10;
}

/* fingerprint = SIG_LEN pulses right after the first long gap */
static bool extract_sig(const int16_t* v, size_t n, int16_t* out) {
    for(size_t i = 0; i + 1 + SIG_LEN <= n; i++) {
        if(v[i] <= -GAP_US) {
            memcpy(out, &v[i + 1], SIG_LEN * sizeof(int16_t));
            return true;
        }
    }
    return false;
}

/* look for the fingerprint anywhere inside a RAW stream */
static bool sig_in_stream(const int16_t* v, size_t n, const int16_t* sig) {
    if(n < SIG_LEN) return false;
    for(size_t i = 0; i + SIG_LEN <= n; i++) {
        if((v[i] < 0) != (sig[0] < 0)) continue;
        if(sig_equal(&v[i], sig)) return true;
    }
    return false;
}

/* ------------------------------------------------------------------ */
/* .sub file parsing / scanning                                        */
/* ------------------------------------------------------------------ */

static bool parse_sub(App* app, const char* path, SubInfo* info, bool want_raw) {
    memset(info, 0, sizeof(SubInfo));
    Stream* s = buffered_file_stream_alloc(app->storage);
    if(!buffered_file_stream_open(s, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        buffered_file_stream_close(s);
        stream_free(s);
        return false;
    }

    FuriString* line = furi_string_alloc();
    bool is_raw_file = false;
    bool has_key = false;

    while(stream_read_line(s, line)) {
        const char* l = furi_string_get_cstr(line);

        if(strncmp(l, "Protocol:", 9) == 0) {
            const char* p = l + 9;
            while(*p == ' ') p++;
            size_t i = 0;
            while(*p && *p != '\r' && *p != '\n' && i < sizeof(info->proto) - 1)
                info->proto[i++] = *p++;
            info->proto[i] = 0;
            is_raw_file = (strcmp(info->proto, "RAW") == 0);
        } else if(strncmp(l, "Key:", 4) == 0) {
            info->key = parse_key(l + 4);
            has_key = true;
        } else if(strncmp(l, "RAW_Data:", 9) == 0) {
            info->has_raw = true;
            if(!want_raw) break;
            const char* p = l + 9;
            while(*p) {
                while(*p == ' ') p++;
                if(!*p || *p == '\r' || *p == '\n') break;
                char* end;
                long v = strtol(p, &end, 10);
                if(end == p) break;
                p = end;
                if(v > 32000) v = 32000;
                if(v < -32000) v = -32000;
                if(info->raw_n < RAW_MAX) app->raw_buf[info->raw_n++] = (int16_t)v;
            }
            if(info->raw_n >= RAW_MAX) break;
        }
    }

    info->has_dec = (!is_raw_file && has_key && info->proto[0]);

    furi_string_free(line);
    buffered_file_stream_close(s);
    stream_free(s);
    return true;
}

typedef struct {
    char title[24];
    char name[32];
    uint32_t done;
    uint32_t total;
} ProgModel;

static void progress_draw_cb(Canvas* canvas, void* model) {
    ProgModel* m = model;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignCenter, m->title);

    canvas_draw_frame(canvas, 4, 20, 120, 12);
    uint32_t w = 0;
    if(m->total) {
        w = (m->done * 116) / m->total;
        if(w > 116) w = 116;
    }
    if(w) canvas_draw_box(canvas, 6, 22, w, 8);

    char buf[40];
    unsigned pct = m->total ? (unsigned)((m->done * 100) / m->total) : 0;
    snprintf(buf, sizeof(buf), "%u%%  (%lu/%lu)", pct, (unsigned long)m->done, (unsigned long)m->total);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignCenter, buf);
    canvas_draw_str_aligned(canvas, 64, 53, AlignCenter, AlignCenter, m->name);
    canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "Back = stop");
}

static bool progress_input_cb(InputEvent* ev, void* ctx) {
    App* app = ctx;
    if(ev->key == InputKeyBack) {
        if(ev->type == InputTypeShort) {
            app->cancelled = true;
            app->scan_stop = true;
        }
        return true;
    }
    return false;
}

static void progress_set(App* app, const char* title, const char* name, uint32_t done, uint32_t total) {
    if(title) snprintf(app->prog_title, sizeof(app->prog_title), "%s", title);
    with_view_model(
        app->progress,
        ProgModel * m,
        {
            snprintf(m->title, sizeof(m->title), "%s", app->prog_title);
            snprintf(m->name, sizeof(m->name), "%.28s", name ? name : "");
            m->done = done;
            m->total = total;
        },
        true);
}

static void scan_dir(App* app, const char* dir_path, FileCb cb, void* ctx, uint8_t depth) {
    if(depth > app->max_depth || app->scan_stop) return;
    File* dir = storage_file_alloc(app->storage);
    if(storage_dir_open(dir, dir_path)) {
        FileInfo fi;
        char name[96];
        while(!app->scan_stop && storage_dir_read(dir, &fi, name, sizeof(name))) {
            if(file_info_is_dir(&fi)) {
                if(depth < app->max_depth) {
                    FuriString* p = furi_string_alloc_printf("%s/%s", dir_path, name);
                    scan_dir(app, furi_string_get_cstr(p), cb, ctx, depth + 1);
                    furi_string_free(p);
                }
            } else {
                size_t len = strlen(name);
                if(len > 4 && strcasecmp(name + len - 4, ".sub") == 0) {
                    if(app->counting) {
                        app->scan_total++;
                    } else {
                        app->scan_done++;
                        progress_set(app, NULL, name, app->scan_done, app->scan_total);
                        FuriString* p = furi_string_alloc_printf("%s/%s", dir_path, name);
                        if(!cb(app, furi_string_get_cstr(p), ctx)) app->scan_stop = true;
                        furi_string_free(p);
                    }
                }
            }
        }
        storage_dir_close(dir);
    }
    storage_file_free(dir);
}

/* Count the files first (for the progress bar), then scan them. */
static void scan_root(App* app, FileCb cb, void* ctx) {
    const char* root = (app->scope == ScopeFolder) ? app->folder : SUBGHZ_DIR;
    app->max_depth = (app->scope == ScopeSaved) ? 0 : 5;
    app->cancelled = false;
    app->scan_stop = false;
    app->scan_total = 0;
    app->scan_done = 0;

    app->counting = true;
    scan_dir(app, root, NULL, NULL, 0);
    app->counting = false;

    if(app->cancelled) return;
    app->scan_stop = false;
    progress_set(app, NULL, "", 0, app->scan_total);
    scan_dir(app, root, cb, ctx, 0);
}

/* ------------------------------------------------------------------ */
/* Radio                                                              */
/* ------------------------------------------------------------------ */

static void rx_capture_cb(bool level, uint32_t duration, void* ctx) {
    App* app = ctx;
    /* feed the protocol decoders */
    if(app->worker) subghz_worker_rx_callback(level, duration, app->worker);
    /* store raw pulses */
    if(app->capturing && app->cap_count < CAP_MAX) {
        int32_t d = duration > 32000 ? 32000 : (int32_t)duration;
        app->cap[app->cap_count++] = level ? (int16_t)d : (int16_t)(-d);
    }
}

static void rx_decoded_cb(SubGhzReceiver* receiver, SubGhzProtocolDecoderBase* base, void* ctx) {
    UNUSED(receiver);
    App* app = ctx;
    if(app->dec_ready || !app->capturing) return;

    FlipperFormat* ff = flipper_format_string_alloc();
    SubGhzRadioPreset preset = {
        .name = furi_string_alloc_set("AM650"),
        .frequency = freq_values[app->freq_idx],
        .data = NULL,
        .data_size = 0,
    };

    if(subghz_protocol_decoder_base_serialize(base, ff, &preset) == SubGhzProtocolStatusOk) {
        flipper_format_rewind(ff);
        FuriString* proto = furi_string_alloc();
        uint32_t cnt = 0;
        if(flipper_format_read_string(ff, "Protocol", proto) &&
           flipper_format_get_value_count(ff, "Key", &cnt) && cnt > 0 && cnt <= 16) {
            uint8_t b[16];
            if(flipper_format_read_hex(ff, "Key", b, cnt)) {
                uint64_t key = 0;
                for(uint32_t i = 0; i < cnt; i++) key = (key << 8) | b[i];
                snprintf(app->dec_proto, sizeof(app->dec_proto), "%s", furi_string_get_cstr(proto));
                app->dec_key = key;
                app->dec_ready = true;
            }
        }
        furi_string_free(proto);
    }

    furi_string_free(preset.name);
    flipper_format_free(ff);
}

static void timer_cb(void* ctx) {
    App* app = ctx;
    if(!app->radio_on || app->done) return;

    uint32_t now = furi_get_tick();

    /* the CC1101 RSSI is unreliable right after start: ignore it for a while */
    if((now - app->start_tick) < WARMUP_MS) {
        app->capturing = false;
        app->cap_count = 0;
        app->dec_ready = false;
        return;
    }

    float rssi = subghz_devices_get_rssi(app->device);
    float hold = app->rssi_start - 5.0f;

    if(!app->capturing) {
        if(rssi > app->rssi_start) {
            app->cap_count = 0;
            app->last_act = now;
            app->capturing = true;
        }
    } else {
        if(rssi > hold) app->last_act = now;
        if((now - app->last_act) > END_MS || app->cap_count >= CAP_MAX) {
            app->capturing = false;
            if(app->cap_count >= MIN_PULSES) {
                app->done = true;
                view_dispatcher_send_custom_event(app->vd, EvtSignal);
            } else {
                app->dec_ready = false; /* too short: it was noise */
            }
        }
    }
}

static bool radio_start(App* app) {
    uint32_t freq = freq_values[app->freq_idx];

    subghz_devices_init();
    app->device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    if(!app->device) {
        subghz_devices_deinit();
        return false;
    }

    app->env = subghz_environment_alloc();
    subghz_environment_set_protocol_registry(app->env, (void*)&subghz_protocol_registry);
    subghz_environment_load_keystore(app->env, EXT_PATH("subghz/assets/keeloq_mfcodes"));

    app->receiver = subghz_receiver_alloc_init(app->env);
    subghz_receiver_set_filter(app->receiver, SubGhzProtocolFlag_Decodable);
    subghz_receiver_set_rx_callback(app->receiver, rx_decoded_cb, app);

    app->worker = subghz_worker_alloc();
    subghz_worker_set_overrun_callback(app->worker, (SubGhzWorkerOverrunCallback)subghz_receiver_reset);
    subghz_worker_set_pair_callback(app->worker, (SubGhzWorkerPairCallback)subghz_receiver_decode);
    subghz_worker_set_context(app->worker, app->receiver);

    app->dec_ready = false;
    app->capturing = false;
    app->done = false;
    app->cap_count = 0;

    subghz_devices_begin(app->device);
    subghz_devices_reset(app->device);
    subghz_devices_idle(app->device);
    subghz_devices_load_preset(app->device, FuriHalSubGhzPresetOok650Async, NULL);
    if(!subghz_devices_is_frequency_valid(app->device, freq)) freq = 433920000;
    subghz_devices_set_frequency(app->device, freq);
    subghz_devices_flush_rx(app->device);
    subghz_devices_set_rx(app->device);

    subghz_worker_start(app->worker);
    subghz_devices_start_async_rx(app->device, rx_capture_cb, app);

    app->start_tick = furi_get_tick();
    app->radio_on = true;
    app->timer = furi_timer_alloc(timer_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, furi_ms_to_ticks(10));
    return true;
}

static void radio_stop(App* app) {
    if(!app->radio_on) return;
    app->radio_on = false;

    furi_timer_stop(app->timer);
    furi_timer_free(app->timer);
    app->timer = NULL;

    subghz_devices_stop_async_rx(app->device);
    subghz_devices_idle(app->device);
    subghz_worker_stop(app->worker);
    subghz_devices_sleep(app->device);
    subghz_devices_end(app->device);
    subghz_devices_deinit();

    SubGhzWorker* w = app->worker;
    app->worker = NULL;
    subghz_worker_free(w);
    subghz_receiver_free(app->receiver);
    subghz_environment_free(app->env);
    app->receiver = NULL;
    app->env = NULL;
    app->device = NULL;
}

/* ------------------------------------------------------------------ */
/* READ                                                               */
/* ------------------------------------------------------------------ */

typedef struct {
    bool have_dec;
    bool have_sig;
    char proto[32];
    uint64_t key;
    int16_t sig[SIG_LEN];
    FuriString* matches;
    uint16_t found;
    bool searched;
} ReadCtx;

static bool read_cb(App* app, const char* path, void* ctx) {
    ReadCtx* c = ctx;
    SubInfo info;
    if(!parse_sub(app, path, &info, c->have_sig)) return true;

    bool dec = c->have_dec && info.has_dec && info.key == c->key &&
               strcmp(info.proto, c->proto) == 0;
    bool raw = c->have_sig && info.has_raw && sig_in_stream(app->raw_buf, info.raw_n, c->sig);

    if(dec || raw) {
        c->found++;
        if(c->found <= MAX_MATCHES) {
            furi_string_cat_str(c->matches, "- ");
            append_name(c->matches, path);
            furi_string_cat_printf(
                c->matches, "\n   [%s%s%s]\n", dec ? "DEC" : "", (dec && raw) ? "+" : "", raw ? "RAW" : "");
        }
    }
    return true;
}

static void read_show_listening(App* app) {
    widget_reset(app->w_read);
    FuriString* s = furi_string_alloc_printf(
        "\e#Listening...\e#\n%s MHz\nMode: %s\n\nPress a button on\nthe remote.",
        freq_text[app->freq_idx],
        mode_text[app->mode]);
    widget_add_text_scroll_element(app->w_read, 0, 0, 128, 64, furi_string_get_cstr(s));
    furi_string_free(s);
}

static void widget_btn_cb(GuiButtonType result, InputType type, void* ctx) {
    App* app = ctx;
    UNUSED(result);
    if(type == InputTypeShort) view_dispatcher_send_custom_event(app->vd, EvtRetry);
}

static void thread_join(App* app) {
    if(app->thread) {
        furi_thread_join(app->thread);
        furi_thread_free(app->thread);
        app->thread = NULL;
    }
}

static void thread_start(App* app, FuriThreadCallback cb) {
    thread_join(app);
    app->thread = furi_thread_alloc_ex("RFidentScan", 5 * 1024, cb, app);
    furi_thread_start(app->thread);
}

static int32_t read_thread(void* ctx) {
    App* app = ctx;
    scan_root(app, read_cb, app->rctx);
    view_dispatcher_send_custom_event(app->vd, EvtReadDone);
    return 0;
}

/* main thread: build the result screen from app->rctx */
static void read_present(App* app) {
    ReadCtx* c = app->rctx;
    app->rctx = NULL;
    if(!c) return;

    FuriString* out = furi_string_alloc();

    if(!c->searched) {
        furi_string_cat_str(out, "\e#No usable signal\e#\n");
        if(app->mode == ModeDecoded)
            furi_string_cat_str(out, "Signal not decoded.\nTry mode RAW/Both.");
        else if(app->mode == ModeRaw)
            furi_string_cat_str(out, "Signal too short or\nnoisy. Try again.");
        else
            furi_string_cat_str(out, "Not decoded and too\nnoisy for RAW. Retry.");
    } else {
        if(c->found) {
            furi_string_cat_printf(out, "\e#ALREADY SAVED (%u)\e#\n", c->found);
            furi_string_cat(out, c->matches);
            if(c->found > MAX_MATCHES) furi_string_cat_str(out, "...and more\n");
        } else {
            furi_string_cat_str(out, "\e#NOT SAVED\e#\nNo matching file found.\n");
            if(app->scan_total == 0) furi_string_cat_str(out, "(no .sub files in scope)\n");
        }
        if(c->have_dec) {
            furi_string_cat_printf(
                out,
                "\nProto: %s\nKey: %08lX%08lX\n",
                c->proto,
                (uint32_t)(c->key >> 32),
                (uint32_t)(c->key & 0xFFFFFFFF));
        }
    }

    furi_string_cat_printf(out, "\nScope: %s\n", scope_text[app->scope]);
    if(app->cancelled) furi_string_cat_str(out, "(search stopped)\n");

    widget_reset(app->w_read);
    widget_add_text_scroll_element(app->w_read, 0, 0, 128, 52, furi_string_get_cstr(out));
    widget_add_button_element(app->w_read, GuiButtonTypeCenter, "Retry", widget_btn_cb, app);
    view_dispatcher_switch_to_view(app->vd, ViewRead);

    furi_string_free(out);
    furi_string_free(c->matches);
    free(c);
}

/* main thread: a signal was captured -> start the background search */
static void read_show_result(App* app) {
    ReadCtx* c = malloc(sizeof(ReadCtx));
    memset(c, 0, sizeof(ReadCtx));
    c->matches = furi_string_alloc();

    if(app->mode != ModeRaw && app->dec_ready) {
        c->have_dec = true;
        snprintf(c->proto, sizeof(c->proto), "%s", app->dec_proto);
        c->key = app->dec_key;
    }
    if(app->mode != ModeDecoded && app->cap_count >= MIN_PULSES) {
        c->have_sig = extract_sig(app->cap, app->cap_count, c->sig);
    }
    app->rctx = c;

    if(!c->have_dec && !c->have_sig) {
        c->searched = false;
        read_present(app);
        return;
    }

    c->searched = true;
    progress_set(app, "Searching...", "", 0, 0);
    view_dispatcher_switch_to_view(app->vd, ViewProgress);
    thread_start(app, read_thread);
}

static void read_start(App* app) {
    read_show_listening(app);
    view_dispatcher_switch_to_view(app->vd, ViewRead);
    if(!radio_start(app)) {
        widget_reset(app->w_read);
        widget_add_text_scroll_element(
            app->w_read, 0, 0, 128, 64, "\e#Radio error\e#\nCannot start the\nSub-GHz radio.");
    }
}

/* ------------------------------------------------------------------ */
/* DUPLICATES                                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    Entry* list;
    size_t count;
    bool truncated;
} DupCtx;

static bool dup_cb(App* app, const char* path, void* ctx) {
    DupCtx* d = ctx;
    if(d->count >= MAX_FILES) {
        d->truncated = true;
        return false;
    }
    SubInfo info;
    bool want_raw = (app->mode != ModeDecoded);
    if(!parse_sub(app, path, &info, want_raw)) return true;

    Entry e;
    memset(&e, 0, sizeof(e));
    if(app->mode != ModeRaw && info.has_dec) {
        e.flags |= 1;
        e.proto_hash = hash_str(info.proto);
        e.key = info.key;
    }
    if(want_raw && info.has_raw && extract_sig(app->raw_buf, info.raw_n, e.sig)) {
        e.flags |= 2;
    }
    if(!e.flags) return true;

    e.path = furi_string_alloc_set(path);
    d->list[d->count++] = e;
    return true;
}

static void dup_run(App* app) {
    DupCtx d = {.list = malloc(sizeof(Entry) * MAX_FILES), .count = 0, .truncated = false};
    scan_root(app, dup_cb, &d);

    furi_string_reset(app->text);
    size_t pairs = 0;
    FuriString* body = furi_string_alloc();

    for(size_t i = 0; i < d.count && pairs < MAX_DUP_PAIRS; i++) {
        for(size_t j = i + 1; j < d.count && pairs < MAX_DUP_PAIRS; j++) {
            bool dec = (d.list[i].flags & 1) && (d.list[j].flags & 1) &&
                       d.list[i].key == d.list[j].key &&
                       d.list[i].proto_hash == d.list[j].proto_hash;
            bool raw = (d.list[i].flags & 2) && (d.list[j].flags & 2) &&
                       sig_equal(d.list[i].sig, d.list[j].sig);
            if(dec || raw) {
                pairs++;
                append_name(body, furi_string_get_cstr(d.list[i].path));
                furi_string_cat_str(body, "\n  = ");
                append_name(body, furi_string_get_cstr(d.list[j].path));
                furi_string_cat_printf(
                    body, "\n  [%s%s%s]\n\n", dec ? "DEC" : "", (dec && raw) ? "+" : "", raw ? "RAW" : "");
            }
        }
    }

    furi_string_printf(
        app->text,
        "Files checked: %u\nType: %s\nScope: %s\nDuplicates found: %u%s\n\n",
        (unsigned)d.count,
        mode_text[app->mode],
        scope_text[app->scope],
        (unsigned)pairs,
        pairs >= MAX_DUP_PAIRS ? "+" : "");
    if(d.truncated) furi_string_cat_str(app->text, "(limit reached, only the first files were checked)\n\n");
    if(app->cancelled) furi_string_cat_str(app->text, "(scan stopped)\n\n");
    if(!pairs) furi_string_cat_str(app->text, "No duplicates.\n");
    furi_string_cat(app->text, body);

    furi_string_free(body);
    for(size_t i = 0; i < d.count; i++) furi_string_free(d.list[i].path);
    free(d.list);

}

static void dup_present(App* app) {
    text_box_reset(app->textbox);
    text_box_set_font(app->textbox, TextBoxFontText);
    text_box_set_text(app->textbox, furi_string_get_cstr(app->text));
    text_box_set_focus(app->textbox, TextBoxFocusStart);
    view_dispatcher_switch_to_view(app->vd, ViewTextBox);
}

static int32_t dup_thread(void* ctx) {
    App* app = ctx;
    dup_run(app);
    view_dispatcher_send_custom_event(app->vd, EvtDupDone);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Settings                                                           */
/* ------------------------------------------------------------------ */

static void freq_change_cb(VariableItem* item) {
    App* app = variable_item_get_context(item);
    app->freq_idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, freq_text[app->freq_idx]);
}

static void mode_change_cb(VariableItem* item) {
    App* app = variable_item_get_context(item);
    app->mode = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, mode_text[app->mode]);
}

static void set_folder_label(App* app) {
    const char* last = strrchr(app->folder, '/');
    last = last ? last + 1 : app->folder;
    if(strcmp(app->folder, SUBGHZ_DIR) == 0) last = "subghz (root)";
    snprintf(app->folder_label, sizeof(app->folder_label), "%.38s", last);
    if(app->folder_item) variable_item_set_current_value_text(app->folder_item, app->folder_label);
}

static const char* sens_text[] = {"Low (-60)", "Medium (-70)", "High (-80)", "Max (-90)"};
static const float sens_values[] = {-60.0f, -70.0f, -80.0f, -90.0f};

static void sens_change_cb(VariableItem* item) {
    App* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->sens_idx = i;
    app->rssi_start = sens_values[i];
    variable_item_set_current_value_text(item, sens_text[i]);
}

static void noop_change_cb(VariableItem* item) {
    UNUSED(item);
}

static void scope_change_cb(VariableItem* item) {
    App* app = variable_item_get_context(item);
    app->scope = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, scope_text[app->scope]);
}

static void folder_menu_cb(void* ctx, uint32_t index);

static void folder_populate(App* app) {
    submenu_reset(app->folder_menu);

    const char* rel = (strlen(app->browse) > strlen(SUBGHZ_DIR)) ? app->browse + strlen(SUBGHZ_DIR) : "/";
    snprintf(app->browse_hdr, sizeof(app->browse_hdr), "%.40s", rel);
    submenu_set_header(app->folder_menu, app->browse_hdr);

    submenu_add_item(app->folder_menu, "[Use this folder]", 0, folder_menu_cb, app);
    if(strcmp(app->browse, SUBGHZ_DIR) != 0) {
        submenu_add_item(app->folder_menu, "..", 1, folder_menu_cb, app);
    }

    uint8_t count = 0;
    File* dir = storage_file_alloc(app->storage);
    if(storage_dir_open(dir, app->browse)) {
        FileInfo fi;
        char name[96];
        while(count < FOLDER_MAX && storage_dir_read(dir, &fi, name, sizeof(name))) {
            size_t nlen = strlen(name);
            if(file_info_is_dir(&fi) && nlen < FOLDER_NAME) {
                memcpy(app->folder_names[count], name, nlen + 1);
                submenu_add_item(app->folder_menu, name, FOLDER_BASE + count, folder_menu_cb, app);
                count++;
            }
        }
        storage_dir_close(dir);
    }
    storage_file_free(dir);
    submenu_set_selected_item(app->folder_menu, 0);
}

static void folder_menu_cb(void* ctx, uint32_t index) {
    App* app = ctx;
    if(index == 0) {
        snprintf(app->folder, sizeof(app->folder), "%s", app->browse);
        set_folder_label(app);
        view_dispatcher_switch_to_view(app->vd, ViewSettings);
    } else if(index == 1) {
        char* sl = strrchr(app->browse, '/');
        if(sl && strlen(app->browse) > strlen(SUBGHZ_DIR)) *sl = 0;
        folder_populate(app);
    } else if(index >= FOLDER_BASE) {
        const char* n = app->folder_names[index - FOLDER_BASE];
        size_t l = strlen(app->browse);
        if(l + 1 + strlen(n) < sizeof(app->browse)) {
            snprintf(app->browse + l, sizeof(app->browse) - l, "/%s", n);
            folder_populate(app);
        }
    }
}

static void settings_enter_cb(void* ctx, uint32_t index) {
    App* app = ctx;
    if(index == 3) { /* Folder item */
        snprintf(app->browse, sizeof(app->browse), "%s", app->folder);
        folder_populate(app);
        view_dispatcher_switch_to_view(app->vd, ViewFolder);
    }
}

#define DATA_DIR      EXT_PATH("apps_data/rfidentifier")
#define SETTINGS_FILE DATA_DIR "/settings.conf"

static void settings_save(App* app) {
    storage_common_mkdir(app->storage, EXT_PATH("apps_data"));
    storage_common_mkdir(app->storage, DATA_DIR);

    FlipperFormat* ff = flipper_format_file_alloc(app->storage);
    if(flipper_format_file_open_always(ff, SETTINGS_FILE)) {
        uint32_t v;
        flipper_format_write_header_cstr(ff, "RFidentifier settings", 1);
        v = freq_values[app->freq_idx];
        flipper_format_write_uint32(ff, "Frequency", &v, 1);
        v = app->mode;
        flipper_format_write_uint32(ff, "SignalType", &v, 1);
        v = app->scope;
        flipper_format_write_uint32(ff, "SearchIn", &v, 1);
        v = app->sens_idx;
        flipper_format_write_uint32(ff, "Sensitivity", &v, 1);
        flipper_format_write_string_cstr(ff, "Folder", app->folder);
    }
    flipper_format_free(ff);
}

static void settings_load(App* app) {
    FlipperFormat* ff = flipper_format_file_alloc(app->storage);
    FuriString* tmp = furi_string_alloc();

    if(flipper_format_file_open_existing(ff, SETTINGS_FILE)) {
        uint32_t ver = 0, v = 0;
        if(flipper_format_read_header(ff, tmp, &ver)) {
            flipper_format_rewind(ff);
            if(flipper_format_read_uint32(ff, "Frequency", &v, 1)) {
                for(size_t i = 0; i < FREQ_COUNT; i++)
                    if(freq_values[i] == v) app->freq_idx = i;
            }
            flipper_format_rewind(ff);
            if(flipper_format_read_uint32(ff, "SignalType", &v, 1) && v <= ModeBoth) app->mode = v;
            flipper_format_rewind(ff);
            if(flipper_format_read_uint32(ff, "SearchIn", &v, 1) && v <= ScopeFolder) app->scope = v;
            flipper_format_rewind(ff);
            if(flipper_format_read_uint32(ff, "Sensitivity", &v, 1) && v < 4) {
                app->sens_idx = v;
                app->rssi_start = sens_values[v];
            }
            flipper_format_rewind(ff);
            if(flipper_format_read_string(ff, "Folder", tmp)) {
                const char* f = furi_string_get_cstr(tmp);
                if(strncmp(f, SUBGHZ_DIR, strlen(SUBGHZ_DIR)) == 0 &&
                   strlen(f) < sizeof(app->folder) && storage_dir_exists(app->storage, f)) {
                    snprintf(app->folder, sizeof(app->folder), "%s", f);
                }
            }
        }
    }

    furi_string_free(tmp);
    flipper_format_free(ff);
}

/* ------------------------------------------------------------------ */
/* Navigation / menu                                                  */
/* ------------------------------------------------------------------ */

static void menu_cb(void* ctx, uint32_t index) {
    App* app = ctx;
    switch(index) {
    case MenuRead:
        read_start(app);
        break;
    case MenuDuplicate:
        progress_set(app, "Scanning...", "", 0, 0);
        view_dispatcher_switch_to_view(app->vd, ViewProgress);
        thread_start(app, dup_thread);
        break;
    case MenuSettings:
        view_dispatcher_switch_to_view(app->vd, ViewSettings);
        break;
    case MenuAbout:
        view_dispatcher_switch_to_view(app->vd, ViewAbout);
        break;
    }
}

static bool custom_event_cb(void* ctx, uint32_t event) {
    App* app = ctx;
    switch(event) {
    case EvtSignal:
        radio_stop(app);
        read_show_result(app);
        return true;
    case EvtReadDone:
        thread_join(app);
        read_present(app);
        return true;
    case EvtDupDone:
        thread_join(app);
        dup_present(app);
        return true;
    case EvtRetry:
        radio_stop(app);
        read_start(app);
        return true;
    }
    return false;
}

static App* g_app = NULL;

static uint32_t nav_to_menu_cb(void* ctx) {
    UNUSED(ctx);
    App* app = g_app;
    if(!app) return ViewMenu;
    radio_stop(app);
    app->scan_stop = true;
    return ViewMenu;
}

static uint32_t nav_settings_exit_cb(void* ctx) {
    UNUSED(ctx);
    if(g_app) settings_save(g_app);
    return ViewMenu;
}

static uint32_t nav_to_settings_cb(void* ctx) {
    UNUSED(ctx);
    return ViewSettings;
}

static uint32_t nav_exit_cb(void* ctx) {
    UNUSED(ctx);
    return VIEW_NONE;
}

/* ------------------------------------------------------------------ */
/* Main                                                               */
/* ------------------------------------------------------------------ */

int32_t rfidentifier_app(void* p) {
    UNUSED(p);

    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));
    g_app = app;
    app->freq_idx = FREQ_DEFAULT;
    app->mode = ModeBoth;
    app->scope = ScopeAll;
    app->sens_idx = 1;
    app->rssi_start = -70.0f;
    snprintf(app->folder, sizeof(app->folder), "%s", SUBGHZ_DIR);
    app->raw_buf = malloc(RAW_MAX * sizeof(int16_t));
    app->text = furi_string_alloc();

    for(size_t i = 0; i < FREQ_COUNT; i++) {
        snprintf(
            freq_text[i],
            sizeof(freq_text[i]),
            "%lu.%02lu",
            freq_values[i] / 1000000,
            (freq_values[i] % 1000000) / 10000);
    }

    app->storage = furi_record_open(RECORD_STORAGE);
    settings_load(app);
    app->gui = furi_record_open(RECORD_GUI);
    app->vd = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->vd, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->vd, app);
    view_dispatcher_set_custom_event_callback(app->vd, custom_event_cb);

    /* Main menu */
    app->menu = submenu_alloc();
    submenu_set_header(app->menu, "RFidentifier");
    submenu_add_item(app->menu, "Read", MenuRead, menu_cb, app);
    submenu_add_item(app->menu, "Duplicate", MenuDuplicate, menu_cb, app);
    submenu_add_item(app->menu, "Settings", MenuSettings, menu_cb, app);
    submenu_add_item(app->menu, "About", MenuAbout, menu_cb, app);
    view_set_previous_callback(submenu_get_view(app->menu), nav_exit_cb);
    view_dispatcher_add_view(app->vd, ViewMenu, submenu_get_view(app->menu));

    /* Read / scanning widget */
    app->w_read = widget_alloc();
    view_set_previous_callback(widget_get_view(app->w_read), nav_to_menu_cb);
    view_dispatcher_add_view(app->vd, ViewRead, widget_get_view(app->w_read));

    /* Duplicate result */
    app->textbox = text_box_alloc();
    view_set_previous_callback(text_box_get_view(app->textbox), nav_to_menu_cb);
    view_dispatcher_add_view(app->vd, ViewTextBox, text_box_get_view(app->textbox));

    /* Settings */
    app->settings = variable_item_list_alloc();
    VariableItem* it = variable_item_list_add(app->settings, "Frequency", FREQ_COUNT, freq_change_cb, app);
    variable_item_set_current_value_index(it, app->freq_idx);
    variable_item_set_current_value_text(it, freq_text[app->freq_idx]);
    it = variable_item_list_add(app->settings, "Signal type", 3, mode_change_cb, app);
    variable_item_set_current_value_index(it, app->mode);
    variable_item_set_current_value_text(it, mode_text[app->mode]);
    it = variable_item_list_add(app->settings, "Search in", 3, scope_change_cb, app);
    variable_item_set_current_value_index(it, app->scope);
    variable_item_set_current_value_text(it, scope_text[app->scope]);
    app->folder_item = variable_item_list_add(app->settings, "Folder (OK)", 1, noop_change_cb, app);
    set_folder_label(app);
    it = variable_item_list_add(app->settings, "Sensitivity", 4, sens_change_cb, app);
    variable_item_set_current_value_index(it, app->sens_idx);
    variable_item_set_current_value_text(it, sens_text[app->sens_idx]);
    variable_item_list_set_enter_callback(app->settings, settings_enter_cb, app);
    view_set_previous_callback(variable_item_list_get_view(app->settings), nav_settings_exit_cb);
    view_dispatcher_add_view(app->vd, ViewSettings, variable_item_list_get_view(app->settings));

    /* Progress bar */
    app->progress = view_alloc();
    view_allocate_model(app->progress, ViewModelTypeLocking, sizeof(ProgModel));
    view_set_draw_callback(app->progress, progress_draw_cb);
    view_set_input_callback(app->progress, progress_input_cb);
    view_set_context(app->progress, app);
    view_set_previous_callback(app->progress, nav_to_menu_cb);
    view_dispatcher_add_view(app->vd, ViewProgress, app->progress);

    /* Folder picker */
    app->folder_menu = submenu_alloc();
    view_set_previous_callback(submenu_get_view(app->folder_menu), nav_to_settings_cb);
    view_dispatcher_add_view(app->vd, ViewFolder, submenu_get_view(app->folder_menu));

    /* About */
    app->w_about = widget_alloc();
    widget_add_string_element(app->w_about, 64, 10, AlignCenter, AlignCenter, FontPrimary, "RFidentifier");
    widget_add_string_multiline_element(
        app->w_about, 64, 38, AlignCenter, AlignCenter, FontSecondary, "v1.0\nCreated by Marcopiiit");
    view_set_previous_callback(widget_get_view(app->w_about), nav_to_menu_cb);
    view_dispatcher_add_view(app->vd, ViewAbout, widget_get_view(app->w_about));

    view_dispatcher_switch_to_view(app->vd, ViewMenu);
    view_dispatcher_run(app->vd);

    /* Cleanup */
    settings_save(app);
    app->scan_stop = true;
    thread_join(app);
    if(app->rctx) {
        ReadCtx* c = app->rctx;
        furi_string_free(c->matches);
        free(c);
        app->rctx = NULL;
    }
    radio_stop(app);

    view_dispatcher_remove_view(app->vd, ViewMenu);
    view_dispatcher_remove_view(app->vd, ViewRead);
    view_dispatcher_remove_view(app->vd, ViewTextBox);
    view_dispatcher_remove_view(app->vd, ViewSettings);
    view_dispatcher_remove_view(app->vd, ViewAbout);
    view_dispatcher_remove_view(app->vd, ViewProgress);
    view_dispatcher_remove_view(app->vd, ViewFolder);

    submenu_free(app->menu);
    widget_free(app->w_read);
    text_box_free(app->textbox);
    variable_item_list_free(app->settings);
    widget_free(app->w_about);
    view_free(app->progress);
    submenu_free(app->folder_menu);
    view_dispatcher_free(app->vd);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);

    g_app = NULL;
    furi_string_free(app->text);
    free(app->raw_buf);
    free(app);
    return 0;
}
