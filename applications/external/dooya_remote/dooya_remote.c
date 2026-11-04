/*
 * Dooya Remote - an app that emulates a Dooya shutter remote.
 * Manage a single remote or a whole group of remote serials.
 * https://github.com/Lechnio/Dooya-Remote
 */

#include <furi.h>
#include <furi_hal_subghz.h>
#include <furi_hal_power.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/text_input.h>
#include <gui/elements.h>
#include <input/input.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include <power/power_service/power.h>
#include <lib/flipper_format/flipper_format.h>
#include <lib/toolbox/level_duration.h>
#include <devices/devices.h>
#include <devices/cc1101_int/cc1101_int_interconnect.h>
#include <applications/drivers/subghz/cc1101_ext/cc1101_ext_interconnect.h>

#include "dooya_remote_icons.h"

#define APP_NAME    "Dooya Remote"
#define APP_VERSION "1.1"
#define APP_REPO    "github.com/Lechnio"

#define APP_FOLDER    EXT_PATH("dooya_remote")
#define SUBGHZ_FOLDER EXT_PATH("subghz")
#define FILE_EXT      ".dr"
#define FILE_TYPE     "Dooya Remote"
#define FILE_VERSION  1

#define DEFAULTS_NAME "Defaults" FILE_EXT
#define DEFAULTS_SRC  APP_ASSETS_PATH(DEFAULTS_NAME)
#define DEFAULTS_DST  APP_FOLDER "/" DEFAULTS_NAME

#define SUBGHZ_FREQ 433920000UL

#define DOOYA_FRAME_MS      50 // one Dooya frame on air
#define PROGRESS_MIN_FRAMES 20 // show the group progress bar past this (~1s)

#define MAX_SERIALS 32
#define SERIAL_LEN  3
#define CH_MAX      15

#define TE_SHORT 366
#define TE_LONG  733

#define REP_P2 3
#define REP_2S 33
#define REP_5S 83

#define BTN_UP_LONG    0x11
#define BTN_UP_SHORT   0x1E
#define BTN_DOWN_LONG  0x33
#define BTN_DOWN_SHORT 0x3C
#define BTN_STOP       0x55
#define BTN_SWAP       0x79
#define BTN_UP_STOP    0x80
#define BTN_DOWN_STOP  0x81
#define BTN_P2         0xCC

#define SCREEN_W 128
#define SCREEN_H 64

typedef enum {
    ViewMain,
    ViewRemote,
    ViewRemoteMenu,
    ViewCreateMenu,
    ViewEditMenu,
    ViewList,
    ViewHex,
    ViewAbout,
    ViewTextInput,
} ViewId;

typedef enum {
    MainLoad,
    MainCreate,
    MainRadio,
    MainAbout
} MainItem;
typedef enum {
    CreateFromFile,
    CreateCustom
} CreateItem;
typedef enum {
    CustomSaveNamed = 1
} CustomEvent; // name entered via Save Remote
typedef enum {
    RemoteMenuSave,
    RemoteMenuManage,
    RemoteMenuTxCount,
    RemoteMenuRename,
    RemoteMenuDelete
} RemoteMenuItem;
typedef enum {
    EditAddFile,
    EditAddCustom,
    EditList
} EditItem;
typedef enum {
    TiSave,
    TiSaveExit,
    TiRename
} TiAction;

typedef enum {
    RmNormal,
    RmMenu,
    RmSending
} RemoteMode;
typedef enum {
    MkTop,
    MkBottom,
    MkStop
} MenuKind;

typedef struct App App;

typedef struct {
    App* app;
    uint8_t pressed; // 0 none, else InputKey + 1
    uint8_t mode;
    uint8_t menu_kind;
    uint8_t menu_sel;
    char sending[20];
} RemoteModel;

typedef struct {
    App* app;
    uint8_t sel;
    uint8_t top;
    bool menu;
    uint8_t menu_sel;
} ListModel;

typedef struct {
    App* app;
    uint8_t nib[6];
    uint8_t col; // 0..5 digits, 6 = channel-mode toggle
    uint8_t mode; // 0 standard, 1 mask
    int index; // -1 new, else serial index being edited
} HexModel;

typedef struct {
    App* app;
    uint8_t sel;
    uint8_t top;
} RemoteMenuModel;

struct App {
    Gui* gui;
    Storage* storage;
    DialogsApp* dialogs;
    ViewDispatcher* vd;

    Submenu* main_menu;
    Submenu* create_menu;
    Submenu* edit_menu;
    Widget* about;
    TextInput* text_input;

    View* remote_view;
    View* remote_menu_view;
    View* list_view;
    View* hex_view;

    uint8_t serials[MAX_SERIALS][SERIAL_LEN];
    uint8_t serial_mode[MAX_SERIALS]; // 0 = standard (0x0n), 1 = mask (0x1n)
    uint8_t serial_wild[MAX_SERIALS]; // bit n set = nibble n is a '*' wildcard
    uint8_t serial_disabled[MAX_SERIALS]; // 1 = kept in list but skipped on send
    uint8_t count;
    uint8_t channel;
    uint8_t tx_count; // frames sent per single tap (1..5, default 2)
    bool radio_ext;

    bool ext_ok;
    bool tilt; // tilt(true)/roller(false) mode of the loaded remote
    bool dirty; // unsaved changes since load/create
    bool creating; // in the Create Remote flow: first serial jumps to the keypad
    uint8_t ti_mode; // TiAction for the text-input result

    FuriString* sub_path; // remembers the last .sub browser folder

    char name[64];
    char name_buf[48];

    char cmd_name[14];
    uint8_t cmd_code; // 0 = no hex line

    // Transmission worker (so long sends can be cancelled with Back)
    FuriThread* tx_thread;
    volatile bool tx_abort;
    volatile bool tx_cancel; // abort was a user Back-cancel (show "Canceled")
    volatile bool tx_busy;
    volatile uint32_t tx_prog_done; // frames put on air so far
    uint32_t tx_prog_total; // frames this send will put on air
    uint32_t tx_combos_total; // wildcard-expanded serials in this send
    uint32_t tx_rep_sum; // frames sent per expanded serial (sum of reps)
    bool tx_show_progress; // this send is long enough for the progress panel
    uint8_t p_btns[3];
    uint16_t p_reps[3];
    uint8_t p_nbtn;
    char p_done[14];
    uint8_t p_done_code;
    bool p_toggle_tilt;
    bool p_overlay; // long service command: show "Canceled" if aborted
    bool p_jog; // continuous round-robin send until aborted (tilt jog)
    bool jogging; // a hold-jog is active; released -> abort

    LevelDuration tx_buf[2 * 40 + 4];
    size_t tx_len;
    size_t tx_idx;
};

static void remote_commit(App* app);
static void rm_set_mode(App* app, uint8_t mode);
static void build_edit_menu(App* app);
static void exit_to_main(App* app);
static void text_input_cb(void* ctx);
static void edit_menu_cb(void* ctx, uint32_t index);
static void main_menu_cb(void* ctx, uint32_t index);
static void build_main_menu(App* app);
static void create_menu_cb(void* ctx, uint32_t index);
static void build_create_menu(App* app);
static void enter_remote(App* app);
static void save_named(App* app);

static uint8_t nibble_at(const uint8_t* b, uint8_t n) {
    return (n & 1) ? (b[n / 2] & 0x0F) : (b[n / 2] >> 4);
}

// "AA BB C*" style, with '*' for wildcard nibbles.
static void serial_pattern_str(const uint8_t* b, uint8_t wild, char* out, size_t n) {
    static const char hx[] = "0123456789ABCDEF";
    char d[6];
    for(uint8_t i = 0; i < 6; i++)
        d[i] = (wild & (1 << i)) ? '*' : hx[nibble_at(b, i)];
    snprintf(out, n, "%c%c %c%c %c%c", d[0], d[1], d[2], d[3], d[4], d[5]);
}

// Wildcard-aware overlap. Returns the index of a colliding pattern or -1.
// *exact is set when the collision is an identical pattern (no wildcards involved).
static int serial_find_overlap(App* app, const uint8_t* b, uint8_t wild, int except, bool* exact) {
    for(int i = 0; i < app->count; i++) {
        if(i == except) continue;
        bool overlap = true;
        for(uint8_t n = 0; n < 6; n++) {
            if((wild & (1 << n)) || (app->serial_wild[i] & (1 << n))) continue;
            if(nibble_at(b, n) != nibble_at(app->serials[i], n)) {
                overlap = false;
                break;
            }
        }
        if(overlap) {
            if(exact)
                *exact =
                    (wild == app->serial_wild[i] && memcmp(b, app->serials[i], SERIAL_LEN) == 0);
            return i;
        }
    }
    return -1;
}

static uint8_t wild_count(uint8_t wild) {
    uint8_t n = 0;
    for(uint8_t i = 0; i < 6; i++)
        if(wild & (1 << i)) n++;
    return n;
}

static bool all_disabled(App* app) {
    if(app->count == 0) return false;
    for(uint8_t i = 0; i < app->count; i++)
        if(!app->serial_disabled[i]) return false;
    return true;
}

// Total number of concrete serials, expanding every wildcard nibble to 16.
static uint32_t effective_count(App* app) {
    uint32_t total = 0;
    for(uint8_t s = 0; s < app->count; s++) {
        if(app->serial_disabled[s]) continue;
        uint32_t e = 1u << (4 * wild_count(app->serial_wild[s]));
        total = (total > 0xFFFFFFFFu - e) ? 0xFFFFFFFFu : total + e;
    }
    return total;
}

// Keeps huge wildcard totals (a ****** serial is 16777216) short enough to fit;
// a leading '>' marks the value as rounded down.
static void fmt_count(char* buf, size_t n, uint32_t v) {
    if(v < 10000)
        snprintf(buf, n, "%lu", (unsigned long)v);
    else if(v < 1000000)
        snprintf(buf, n, ">%luk", (unsigned long)(v / 1000));
    else
        snprintf(buf, n, ">%luM", (unsigned long)(v / 1000000));
}

static void fmt_eta(char* buf, size_t n, uint32_t s) {
    uint32_t d = s / 86400;
    s %= 86400;
    uint32_t h = s / 3600;
    s %= 3600;
    uint32_t m = s / 60;
    s %= 60;
    if(d)
        snprintf(
            buf,
            n,
            "%lud %luh %lum %lus",
            (unsigned long)d,
            (unsigned long)h,
            (unsigned long)m,
            (unsigned long)s);
    else if(h)
        snprintf(buf, n, "%luh %lum %lus", (unsigned long)h, (unsigned long)m, (unsigned long)s);
    else if(m)
        snprintf(buf, n, "%lum %lus", (unsigned long)m, (unsigned long)s);
    else
        snprintf(buf, n, "%lus", (unsigned long)s);
}

// Draw a serial so digits line up regardless of their widths: each hex digit
// is centred in a fixed-width column, with a small gap between byte groups.
static void draw_mono(Canvas* c, int x, int y, const char* s) {
    const int col = 7, gap = 3;
    char ch[2] = {0, 0};
    int cur = x;
    for(int i = 0; s[i]; i++) {
        if(s[i] == ' ') {
            cur += gap;
            continue;
        }
        ch[0] = s[i];
        int w = canvas_string_width(c, ch);
        canvas_draw_str(c, cur + (col - w) / 2, y, ch);
        cur += col;
    }
}

static void serials_sort(App* app) {
    for(int i = 1; i < app->count; i++) {
        uint8_t key[SERIAL_LEN];
        memcpy(key, app->serials[i], SERIAL_LEN);
        uint8_t km = app->serial_mode[i];
        uint8_t kw = app->serial_wild[i];
        uint8_t kd = app->serial_disabled[i];
        int j = i - 1;
        while(j >= 0 &&
              (app->serial_disabled[j] > kd ||
               (app->serial_disabled[j] == kd && memcmp(app->serials[j], key, SERIAL_LEN) > 0))) {
            memcpy(app->serials[j + 1], app->serials[j], SERIAL_LEN);
            app->serial_mode[j + 1] = app->serial_mode[j];
            app->serial_wild[j + 1] = app->serial_wild[j];
            app->serial_disabled[j + 1] = app->serial_disabled[j];
            j--;
        }
        memcpy(app->serials[j + 1], key, SERIAL_LEN);
        app->serial_mode[j + 1] = km;
        app->serial_wild[j + 1] = kw;
        app->serial_disabled[j + 1] = kd;
    }
}

static int serial_find(App* app, const uint8_t* s, int except) {
    for(int i = 0; i < app->count; i++) {
        if(i == except) continue;
        if(memcmp(app->serials[i], s, SERIAL_LEN) == 0) return i;
    }
    return -1;
}

static void info_dialog(App* app, const char* header, const char* text) {
    DialogMessage* m = dialog_message_alloc();
    dialog_message_set_header(m, header, 64, 4, AlignCenter, AlignTop);
    dialog_message_set_text(m, text, 64, 34, AlignCenter, AlignCenter);
    dialog_message_set_buttons(m, NULL, "OK", NULL);
    dialog_message_show(app->dialogs, m);
    dialog_message_free(m);
}

static void set_cmd(App* app, const char* name, uint8_t code) {
    strncpy(app->cmd_name, name, sizeof(app->cmd_name) - 1);
    app->cmd_name[sizeof(app->cmd_name) - 1] = '\0';
    app->cmd_code = code;
}

static LevelDuration tx_yield(void* ctx) {
    App* app = ctx;
    if(app->tx_idx >= app->tx_len) return level_duration_reset();
    return app->tx_buf[app->tx_idx++];
}

static void
    tx_build_frame(App* app, const uint8_t* serial, uint8_t ch, uint8_t btn, uint8_t mode) {
    // Standard remotes put the channel straight in the low nibble (ch2 -> 0x02).
    // Mask-variant remotes set the high nibble (ch2 -> 0x12). Per-serial.
    uint8_t chbyte = (mode ? 0x10 : 0x00) | (ch & 0x0F);
    uint64_t data = ((uint64_t)serial[0] << 32) | ((uint64_t)serial[1] << 24) |
                    ((uint64_t)serial[2] << 16) | ((uint64_t)chbyte << 8) | btn;

    size_t n = 0;
    bool b0 = data & 1;
    app->tx_buf[n++] = level_duration_make(false, TE_LONG * 12 + (b0 ? TE_LONG : TE_SHORT));
    app->tx_buf[n++] = level_duration_make(true, TE_SHORT * 13);
    app->tx_buf[n++] = level_duration_make(false, TE_LONG * 2);
    for(int i = 39; i >= 0; i--) {
        if((data >> i) & 1) {
            app->tx_buf[n++] = level_duration_make(true, TE_LONG);
            app->tx_buf[n++] = level_duration_make(false, TE_SHORT);
        } else {
            app->tx_buf[n++] = level_duration_make(true, TE_SHORT);
            app->tx_buf[n++] = level_duration_make(false, TE_LONG);
        }
    }
    app->tx_len = n;
}

// External CC1101 runs on 5V OTG. We hold that power for the whole time the
// keypad view is open (enter/exit callbacks), not per transmission, so the
// module stays alive between presses.
static void radio_power(bool on) {
    Power* power = furi_record_open(RECORD_POWER);
    power_enable_otg(power, on);
    furi_record_close(RECORD_POWER);
    if(on) {
        for(uint8_t i = 0; i < 5; i++) {
            if(furi_hal_power_is_otg_enabled() || furi_hal_power_get_usb_voltage() >= 4.5f) break;
            furi_delay_ms(100);
        }
    }
}

static const SubGhzDevice* radio_begin(App* app) {
    const char* name = app->radio_ext ? SUBGHZ_DEVICE_CC1101_EXT_NAME :
                                        SUBGHZ_DEVICE_CC1101_INT_NAME;

    const SubGhzDevice* dev = subghz_devices_get_by_name(name);
    if(!dev) return NULL;

    subghz_devices_begin(dev);
    subghz_devices_reset(dev);
    subghz_devices_load_preset(dev, FuriHalSubGhzPresetOok270Async, NULL);
    subghz_devices_set_frequency(dev, SUBGHZ_FREQ);
    return dev;
}

static void radio_end(App* app, const SubGhzDevice* dev) {
    UNUSED(app);
    subghz_devices_idle(dev);
    subghz_devices_sleep(dev);
    subghz_devices_end(dev);
}

// Same detection the stock Sub-GHz app and the scheduler use: power the module,
// init the device registry, then is_connect WITHOUT begin/reset.
static bool ext_present(void) {
    radio_power(true);
    subghz_devices_init();
    const SubGhzDevice* d = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_EXT_NAME);
    bool present = d && subghz_devices_is_connect(d);
    subghz_devices_deinit();
    radio_power(false);
    return present;
}

static void
    tx_emit(App* app, const SubGhzDevice* dev, const uint8_t* bytes, uint8_t mode, uint8_t btn) {
    tx_build_frame(app, bytes, app->channel, btn, mode);
    app->tx_idx = 0;
    if(subghz_devices_start_async_tx(dev, tx_yield, app)) {
        while(!subghz_devices_is_async_complete_tx(dev) && !app->tx_abort) {
            furi_delay_ms(2);
        }
        subghz_devices_stop_async_tx(dev);
    }
}

// Sends one serial pattern, expanding every '*' nibble to all 16 values.
static void tx_frame_once(App* app, const SubGhzDevice* dev, uint8_t idx, uint8_t btn) {
    uint8_t wpos[6], wn = 0;
    for(uint8_t i = 0; i < 6; i++)
        if(app->serial_wild[idx] & (1 << i)) wpos[wn++] = i;

    uint32_t total = 1u << (4 * wn);
    for(uint32_t combo = 0; combo < total && !app->tx_abort; combo++) {
        uint8_t nib[6];
        for(uint8_t i = 0; i < 6; i++)
            nib[i] = nibble_at(app->serials[idx], i);
        for(uint8_t j = 0; j < wn; j++)
            nib[wpos[j]] = (combo >> (4 * j)) & 0x0F;
        uint8_t bytes[SERIAL_LEN] = {
            (nib[0] << 4) | nib[1], (nib[2] << 4) | nib[3], (nib[4] << 4) | nib[5]};
        tx_emit(app, dev, bytes, app->serial_mode[idx], btn);
        app->tx_prog_done++;
    }
}

// Runs on the worker thread. Stops promptly when app->tx_abort is set.
static void tx_run(App* app, const uint8_t* btns, const uint16_t* reps, uint8_t nbtn) {
    const SubGhzDevice* dev = radio_begin(app);
    if(!dev) return;

    furi_hal_power_suppress_charge_enter();
    // Jog: round-robin one short frame per serial, repeating until released.
    while(app->p_jog && !app->tx_abort) {
        for(uint8_t s = 0; s < app->count && !app->tx_abort; s++) {
            if(app->serial_disabled[s]) continue;
            tx_frame_once(app, dev, s, btns[0]);
        }
    }
    for(uint8_t s = 0; !app->p_jog && s < app->count && !app->tx_abort; s++) {
        if(app->serial_disabled[s]) continue;
        for(uint8_t k = 0; k < nbtn && !app->tx_abort; k++) {
            for(uint16_t r = 0; r < reps[k] && !app->tx_abort; r++) {
                tx_frame_once(app, dev, s, btns[k]);
            }
        }
    }
    furi_hal_power_suppress_charge_exit();
    radio_end(app, dev);
}

static int32_t tx_worker(void* ctx) {
    App* app = ctx;
    tx_run(app, app->p_btns, app->p_reps, app->p_nbtn);

    if(app->tx_cancel) {
        set_cmd(app, "Canceled", 0);
    } else if(app->p_toggle_tilt) {
        app->tilt = !app->tilt;
        set_cmd(app, app->tilt ? "Tilt mode" : "Roller mode", 0);
    } else {
        set_cmd(app, app->p_done, app->p_done_code);
    }
    app->tx_busy = false;
    with_view_model(
        app->remote_view,
        RemoteModel * m,
        {
            m->mode = RmNormal;
            m->pressed = 0;
        },
        true);
    return 0;
}

static void tx_join(App* app) {
    if(app->tx_thread) {
        furi_thread_join(app->tx_thread);
        furi_thread_free(app->tx_thread);
        app->tx_thread = NULL;
    }
}

// Kick off a transmission on the worker thread. overlay=true shows the
// "sending, wait" box (for the long service commands); taps use overlay=false
// and show their label immediately.
static void tx_start(
    App* app,
    const uint8_t* btns,
    const uint16_t* reps,
    uint8_t nbtn,
    bool overlay,
    const char* overlay_text,
    const char* done,
    uint8_t done_code,
    bool toggle_tilt,
    bool jog) {
    if(app->tx_busy) return;

    if(effective_count(app) == 0) {
        set_cmd(app, "No remotes", 0);
        remote_commit(app);
        return;
    }
    if(app->radio_ext && !app->ext_ok) {
        set_cmd(app, "No ext radio", 0);
        remote_commit(app);
        return;
    }

    tx_join(app);

    for(uint8_t i = 0; i < nbtn; i++) {
        app->p_btns[i] = btns[i];
        app->p_reps[i] = reps[i];
    }
    app->p_nbtn = nbtn;
    strncpy(app->p_done, done, sizeof(app->p_done) - 1);
    app->p_done[sizeof(app->p_done) - 1] = '\0';
    app->p_done_code = done_code;
    app->p_toggle_tilt = toggle_tilt;
    app->p_overlay = overlay;
    app->p_jog = jog;
    app->tx_abort = false;
    app->tx_cancel = false;
    app->tx_busy = true;

    uint32_t rep_sum = 0;
    for(uint8_t i = 0; i < nbtn; i++)
        rep_sum += reps[i];
    uint64_t frames = (uint64_t)effective_count(app) * rep_sum;
    app->tx_prog_total = frames > 0xFFFFFFFFu ? 0xFFFFFFFFu : (uint32_t)frames;
    app->tx_prog_done = 0;
    app->tx_combos_total = effective_count(app);
    app->tx_rep_sum = rep_sum ? rep_sum : 1;
    app->tx_show_progress = !jog && app->tx_prog_total >= PROGRESS_MIN_FRAMES;

    if(overlay) {
        with_view_model(
            app->remote_view,
            RemoteModel * m,
            {
                m->mode = RmSending;
                strncpy(m->sending, overlay_text, sizeof(m->sending) - 1);
                m->sending[sizeof(m->sending) - 1] = '\0';
            },
            true);
    } else {
        set_cmd(app, done, done_code);
        remote_commit(app);
    }

    app->tx_thread = furi_thread_alloc_ex("DooyaTx", 2048, tx_worker, app);
    furi_thread_start(app->tx_thread);
}

static bool remote_load(App* app, const char* path) {
    FlipperFormat* ff = flipper_format_file_alloc(app->storage);
    FuriString* type = furi_string_alloc();
    bool ok = false;

    do {
        if(!flipper_format_file_open_existing(ff, path)) break;
        uint32_t ver = 0;
        if(!flipper_format_read_header(ff, type, &ver)) break;
        if(!furi_string_equal_str(type, FILE_TYPE)) break;
        uint32_t cnt = 0;
        if(!flipper_format_read_uint32(ff, "Count", &cnt, 1)) break;
        if(cnt == 0) break;
        if(cnt > MAX_SERIALS) cnt = MAX_SERIALS;
        if(!flipper_format_read_hex(ff, "Serials", (uint8_t*)app->serials, cnt * SERIAL_LEN))
            break;
        memset(app->serial_mode, 0, sizeof(app->serial_mode));
        memset(app->serial_wild, 0, sizeof(app->serial_wild));
        memset(app->serial_disabled, 0, sizeof(app->serial_disabled));
        if(cnt > 0) flipper_format_read_hex(ff, "Modes", app->serial_mode, cnt); // optional
        if(cnt > 0) flipper_format_read_hex(ff, "Wild", app->serial_wild, cnt); // optional
        if(cnt > 0) flipper_format_read_hex(ff, "Disabled", app->serial_disabled, cnt); // optional
        uint32_t tilt = 0;
        flipper_format_read_uint32(ff, "Tilt", &tilt, 1); // optional
        app->tilt = tilt ? true : false;
        uint32_t txc = 2;
        flipper_format_read_uint32(ff, "TX Count", &txc, 1); // optional
        if(txc < 1) txc = 1;
        if(txc > 5) txc = 5;
        app->tx_count = txc;
        app->count = cnt;
        ok = true;
    } while(false);

    furi_string_free(type);
    flipper_format_free(ff);
    return ok;
}

static bool remote_save(App* app, const char* name) {
    FuriString* path = furi_string_alloc_printf("%s/%s%s", APP_FOLDER, name, FILE_EXT);
    FlipperFormat* ff = flipper_format_file_alloc(app->storage);
    bool ok = false;

    do {
        if(!flipper_format_file_open_always(ff, furi_string_get_cstr(path))) break;
        if(!flipper_format_write_header_cstr(ff, FILE_TYPE, FILE_VERSION)) break;
        uint32_t cnt = app->count;
        if(!flipper_format_write_uint32(ff, "Count", &cnt, 1)) break;
        if(cnt > 0 &&
           !flipper_format_write_hex(ff, "Serials", (uint8_t*)app->serials, cnt * SERIAL_LEN))
            break;
        if(cnt > 0 && !flipper_format_write_hex(ff, "Modes", app->serial_mode, cnt)) break;
        if(cnt > 0 && !flipper_format_write_hex(ff, "Wild", app->serial_wild, cnt)) break;
        if(cnt > 0 && !flipper_format_write_hex(ff, "Disabled", app->serial_disabled, cnt)) break;
        uint32_t tilt = app->tilt ? 1 : 0;
        if(!flipper_format_write_uint32(ff, "Tilt", &tilt, 1)) break;
        uint32_t txc = app->tx_count;
        if(!flipper_format_write_uint32(ff, "TX Count", &txc, 1)) break;
        ok = true;
    } while(false);

    flipper_format_free(ff);
    furi_string_free(path);
    return ok;
}

static void remote_delete_file(App* app, const char* name) {
    FuriString* path = furi_string_alloc_printf("%s/%s%s", APP_FOLDER, name, FILE_EXT);
    storage_common_remove(app->storage, furi_string_get_cstr(path));
    furi_string_free(path);
}

static bool remote_file_exists(App* app, const char* name) {
    FuriString* path = furi_string_alloc_printf("%s/%s%s", APP_FOLDER, name, FILE_EXT);
    FileInfo fi;
    bool e = storage_common_stat(app->storage, furi_string_get_cstr(path), &fi) == FSE_OK;
    furi_string_free(path);
    return e;
}

static bool serial_from_sub(App* app, const char* path, uint8_t* out, uint8_t* out_mode) {
    FlipperFormat* ff = flipper_format_file_alloc(app->storage);
    FuriString* proto = furi_string_alloc();
    bool ok = false;

    do {
        if(!flipper_format_file_open_existing(ff, path)) break;
        if(!flipper_format_read_string(ff, "Protocol", proto)) break;
        if(!furi_string_equal_str(proto, "Dooya")) break;
        uint8_t key[8] = {0};
        if(!flipper_format_read_hex(ff, "Key", key, sizeof(key))) break;
        out[0] = key[3];
        out[1] = key[4];
        out[2] = key[5];
        // key[6] is the channel byte; high nibble 1 => mask variant.
        *out_mode = ((key[6] & 0xF0) == 0x10) ? 1 : 0;
        ok = true;
    } while(false);

    furi_string_free(proto);
    flipper_format_free(ff);
    return ok;
}

static void name_from_path(const char* path, char* out, size_t n) {
    const char* base = strrchr(path, '/');
    base = base ? base + 1 : path;
    strncpy(out, base, n - 1);
    out[n - 1] = '\0';
    char* dot = strstr(out, FILE_EXT);
    if(dot) *dot = '\0';
}

// Solid triangle, base centred at (bx,by), tip H away in direction dir.
static void fill_tri(Canvas* c, int bx, int by, int B, int H, CanvasDirection dir) {
    int h = B / 2;
    for(int k = 0; k <= H; k++) {
        int ext = h - (h * k) / H;
        switch(dir) {
        case CanvasDirectionBottomToTop:
            canvas_draw_line(c, bx - ext, by - k, bx + ext, by - k);
            break;
        case CanvasDirectionTopToBottom:
            canvas_draw_line(c, bx - ext, by + k, bx + ext, by + k);
            break;
        case CanvasDirectionRightToLeft:
            canvas_draw_line(c, bx - k, by - ext, bx - k, by + ext);
            break;
        default:
            canvas_draw_line(c, bx + k, by - ext, bx + k, by + ext);
            break;
        }
    }
}

// Arrow pointing in dir. Pressed = solid; otherwise a clean 2px outline made by
// filling then carving a smaller white triangle (no gaps between the lines).
static void
    draw_arrow(Canvas* c, int bx, int by, int B, int H, CanvasDirection dir, bool pressed) {
    fill_tri(c, bx, by, B, H, dir);
    if(pressed) return;

    int ibx = bx, iby = by;
    switch(dir) {
    case CanvasDirectionBottomToTop:
        iby = by - 2;
        break;
    case CanvasDirectionTopToBottom:
        iby = by + 2;
        break;
    case CanvasDirectionRightToLeft:
        ibx = bx - 2;
        break;
    default:
        ibx = bx + 2;
        break;
    }
    canvas_set_color(c, ColorWhite);
    fill_tri(c, ibx, iby, B - 6, H - 4, dir);
    canvas_set_color(c, ColorBlack);
}

// Small curved "back" arrow (like the Flipper Back key), ~9x8 at top-left (x,y).
static void draw_back_glyph(Canvas* c, int x, int y) {
    int cy = y + 5;
    canvas_draw_line(c, x + 1, cy, x + 7, cy); // shaft
    canvas_draw_line(c, x + 7, cy, x + 7, y + 1); // hook up
    canvas_draw_line(c, x + 1, cy, x + 3, cy - 2); // arrowhead
    canvas_draw_line(c, x + 1, cy, x + 3, cy + 2);
}

// Centred selectable box. title == NULL draws no title (used as an overlay).
static void
    draw_box_menu(Canvas* c, const char* title, const char* const* items, uint8_t n, uint8_t sel) {
    canvas_set_font(c, FontSecondary);
    if(title) canvas_draw_str_aligned(c, 64, 9, AlignCenter, AlignBottom, title);

    int rh = 11;
    int bh = n * rh + 3;
    int bw = 78;
    int bx = 64 - bw / 2;
    int by = 12;

    canvas_set_color(c, ColorWhite);
    canvas_draw_box(c, bx, by, bw, bh);
    canvas_set_color(c, ColorBlack);
    canvas_draw_frame(c, bx, by, bw, bh);

    for(uint8_t i = 0; i < n; i++) {
        int ry = by + 2 + i * rh;
        if(sel == i) {
            canvas_draw_box(c, bx + 2, ry, bw - 4, rh);
            canvas_set_color(c, ColorWhite);
        } else {
            canvas_set_color(c, ColorBlack);
        }
        canvas_draw_str_aligned(c, 64, ry + rh / 2 + 1, AlignCenter, AlignCenter, items[i]);
        canvas_set_color(c, ColorBlack);
    }
}

static const char* mk_title(uint8_t k) {
    return k == MkTop ? "Top limit" : k == MkBottom ? "Bottom limit" : "Actions";
}
static uint8_t mk_count(uint8_t k) {
    return k == MkStop ? 4 : 2;
}
static void mk_items(uint8_t k, const char** out) {
    if(k == MkStop) {
        out[0] = "Send P2";
        out[1] = "Swap dir";
        out[2] = "Clear limits";
        out[3] = "Tilt/Roller";
    } else {
        out[0] = "Set limit";
        out[1] = "Delete limit";
    }
}

// Centered, titled panel, matching the limit/action menus. The counter shows
// wildcard-expanded serials handled, so one two-star serial runs 0..256.
static void draw_progress(Canvas* c, App* app, const char* act) {
    canvas_set_font(c, FontSecondary);
    char title[32];
    if(act && act[0])
        snprintf(title, sizeof(title), "Sending %s...", act);
    else
        snprintf(title, sizeof(title), "Sending...");
    canvas_draw_str_aligned(c, 64, 9, AlignCenter, AlignBottom, title);

    const int bx = 14, by = 12, bw = 100, bh = 44;
    canvas_set_color(c, ColorWhite);
    canvas_draw_box(c, bx, by, bw, bh);
    canvas_set_color(c, ColorBlack);
    canvas_draw_frame(c, bx, by, bw, bh);

    uint32_t frames = app->tx_prog_total;
    uint32_t fdone = app->tx_prog_done;
    if(fdone > frames) fdone = frames;
    elements_progress_bar(
        c, bx + 5, by + 6, bw - 10, frames ? (float)fdone / (float)frames : 0.0f);

    uint32_t total = app->tx_combos_total;
    uint32_t done = app->tx_rep_sum ? app->tx_prog_done / app->tx_rep_sum : 0;
    if(done > total) done = total;
    char line[32];
    snprintf(line, sizeof(line), "%lu/%lu", (unsigned long)done, (unsigned long)total);
    canvas_draw_str_aligned(c, 64, by + 26, AlignCenter, AlignBottom, line);

    uint64_t rem_ms = (uint64_t)(frames - fdone) * DOOYA_FRAME_MS;
    char eta[24];
    fmt_eta(eta, sizeof(eta), (uint32_t)((rem_ms + 999) / 1000));
    snprintf(line, sizeof(line), "%s left", eta);
    canvas_draw_str_aligned(c, 64, by + 38, AlignCenter, AlignBottom, line);
}

static void remote_draw(Canvas* c, void* ctx) {
    RemoteModel* m = ctx;
    App* app = m->app;

    canvas_clear(c);
    canvas_set_color(c, ColorBlack);
    canvas_set_font(c, FontSecondary);

    // A titled overlay (menu / progress / sending box) sits where the file name
    // and the right-hand column would be, so both are hidden while one is up.
    bool overlay = (m->mode == RmMenu) || (m->mode == RmSending) ||
                   (app->tx_busy && app->tx_show_progress);

    if(!overlay) {
        char hdr[16]; // caps the shown file name at 15 characters
        strncpy(hdr, app->name[0] ? app->name : "<unsaved>", sizeof(hdr) - 1);
        hdr[sizeof(hdr) - 1] = '\0';
        canvas_draw_str(c, 0, 7, hdr);
        canvas_draw_str_aligned(
            c, SCREEN_W - 1, 7, AlignRight, AlignBottom, app->radio_ext ? "Ext" : "Int");
    }

    // Keypad is always drawn; the action menu / sending box overlay on top of it.
    const int cx = 30, cy = 33, R = 22;
    bool up = m->pressed == InputKeyUp + 1;
    bool dn = m->pressed == InputKeyDown + 1;
    bool lf = m->pressed == InputKeyLeft + 1;
    bool rt = m->pressed == InputKeyRight + 1;
    bool ok = m->pressed == InputKeyOk + 1;

    // 2px ring: solid disc with a white interior carved out.
    canvas_draw_disc(c, cx, cy, R);
    canvas_set_color(c, ColorWhite);
    canvas_draw_disc(c, cx, cy, R - 2);
    canvas_set_color(c, ColorBlack);

    draw_arrow(c, cx, cy - 9, 11, 8, CanvasDirectionBottomToTop, up);
    draw_arrow(c, cx, cy + 9, 11, 8, CanvasDirectionTopToBottom, dn);
    draw_arrow(c, cx - 9, cy, 11, 8, CanvasDirectionRightToLeft, lf);
    draw_arrow(c, cx + 9, cy, 11, 8, CanvasDirectionLeftToRight, rt);
    if(ok) {
        canvas_draw_disc(c, cx, cy, 5);
    } else {
        canvas_draw_disc(c, cx, cy, 5);
        canvas_set_color(c, ColorWhite);
        canvas_draw_disc(c, cx, cy, 3);
        canvas_set_color(c, ColorBlack);
    }

    const int px = 90; // centre for the action name
    const int lx = 60; // left column for the serials / channel lines
    if(!overlay) {
        char cnt[12];
        fmt_count(cnt, sizeof(cnt), effective_count(app));
        char line[28];
        snprintf(line, sizeof(line), "Serials: %s%s", cnt, all_disabled(app) ? " (D)" : "");
        canvas_draw_str(c, lx, 25, line);

        char ch[20];
        if(app->channel == 0)
            snprintf(ch, sizeof(ch), "Channel: 0 (m)");
        else
            snprintf(ch, sizeof(ch), "Channel: %u", app->channel);
        canvas_draw_str(c, lx, 35, ch);

        if(app->cmd_name[0])
            canvas_draw_str_aligned(c, px, 49, AlignCenter, AlignBottom, app->cmd_name);
    }

    canvas_draw_str(c, 1, 63, app->tilt ? "Tilt" : "Roller");
    int we = canvas_string_width(c, ": exit");
    canvas_draw_str_aligned(c, SCREEN_W - 1, 63, AlignRight, AlignBottom, ": exit");
    int gx = SCREEN_W - 1 - we - 9;
    draw_back_glyph(c, gx, 55);
    int wh = canvas_string_width(c, "Hold ");
    int holdx = gx - wh;
    canvas_draw_str(c, holdx, 63, "Hold ");
    if(app->dirty) canvas_draw_str(c, holdx - canvas_string_width(c, "(U) "), 63, "(U)");

    if(m->mode == RmMenu) {
        const char* items[4];
        mk_items(m->menu_kind, items);
        draw_box_menu(c, mk_title(m->menu_kind), items, mk_count(m->menu_kind), m->menu_sel);
    } else if(app->tx_busy && app->tx_show_progress) {
        draw_progress(c, app, m->mode == RmSending ? m->sending : app->cmd_name);
    } else if(m->mode == RmSending) {
        canvas_set_color(c, ColorWhite);
        canvas_draw_box(c, 8, 22, 112, 26);
        canvas_set_color(c, ColorBlack);
        canvas_draw_frame(c, 8, 22, 112, 26);
        canvas_draw_str_aligned(c, 64, 32, AlignCenter, AlignCenter, m->sending);
        canvas_draw_str_aligned(c, 64, 42, AlignCenter, AlignCenter, "sending, wait...");
    }
}

static void rm_set_mode(App* app, uint8_t mode) {
    with_view_model(app->remote_view, RemoteModel * m, { m->mode = mode; }, true);
}

static void remote_commit(App* app) {
    with_view_model(app->remote_view, RemoteModel * m, { UNUSED(m); }, true);
}

static void send_one(App* app, uint8_t btn, uint16_t reps, bool overlay, const char* name) {
    uint8_t b = btn;
    uint16_t r = reps;
    tx_start(app, &b, &r, 1, overlay, name, name, btn, false, false);
}

// Continuous short-frame jog (tilt mode), until the key is released.
static void start_jog(App* app, uint8_t btn, const char* name) {
    uint8_t b = btn;
    uint16_t r = 1;
    app->jogging = true;
    tx_start(app, &b, &r, 1, false, NULL, name, btn, false, true);
}

static void remote_exec_menu(App* app, uint8_t kind, uint8_t sel) {
    if(kind == MkTop) {
        send_one(
            app, BTN_UP_STOP, sel == 0 ? REP_2S : REP_5S, true, sel == 0 ? "Set top" : "Del top");
    } else if(kind == MkBottom) {
        send_one(
            app, BTN_DOWN_STOP, sel == 0 ? REP_2S : REP_5S, true, sel == 0 ? "Set bot" : "Del bot");
    } else if(sel == 0) {
        send_one(app, BTN_P2, REP_P2, true, "P2");
    } else if(sel == 1) {
        send_one(app, BTN_SWAP, REP_2S, true, "Swap dir");
    } else if(sel == 2) {
        uint8_t btns[3] = {BTN_P2, BTN_DOWN_LONG, BTN_P2};
        uint16_t reps[3] = {REP_P2, REP_P2, REP_P2};
        tx_start(app, btns, reps, 3, true, "Clear limits", "Clear lim", 0, false, false);
    } else {
        // Tilt/Roller is a local mode switch only; it transmits nothing.
        app->tilt = !app->tilt;
        app->dirty = true;
        set_cmd(app, app->tilt ? "Tilt mode" : "Roller mode", 0);
        rm_set_mode(app, RmNormal);
    }
}

static void channel_step(App* app, bool up) {
    if(up)
        app->channel = (app->channel == CH_MAX) ? 0 : app->channel + 1;
    else
        app->channel = (app->channel == 0) ? CH_MAX : app->channel - 1;
    set_cmd(app, "", 0);
}

static bool remote_menu_input(App* app, InputEvent* e) {
    uint8_t kind = 0, sel = 0, n = 0;
    with_view_model(
        app->remote_view,
        RemoteModel * m,
        {
            kind = m->menu_kind;
            sel = m->menu_sel;
        },
        false);
    n = mk_count(kind);

    if(e->key == InputKeyBack && e->type == InputTypeShort) {
        rm_set_mode(app, RmNormal);
        return true;
    }
    if((e->type == InputTypeShort || e->type == InputTypeRepeat) &&
       (e->key == InputKeyUp || e->key == InputKeyDown)) {
        uint8_t ns = (e->key == InputKeyUp) ? (sel == 0 ? n - 1 : sel - 1) : (sel + 1) % n;
        with_view_model(app->remote_view, RemoteModel * m, { m->menu_sel = ns; }, true);
        return true;
    }
    if(e->type == InputTypeShort && e->key == InputKeyOk) {
        remote_exec_menu(app, kind, sel);
        return true;
    }
    return true;
}

static bool remote_input(InputEvent* e, void* ctx) {
    App* app = ctx;
    uint8_t mode = 0;
    with_view_model(app->remote_view, RemoteModel * m, { mode = m->mode; }, false);

    // Back cancels any transmission in progress and keeps us on the keypad.
    if(app->tx_busy && e->key == InputKeyBack) {
        if(e->type == InputTypeShort || e->type == InputTypeLong) {
            app->tx_cancel = true;
            app->tx_abort = true;
        }
        return true;
    }
    if(mode == RmSending) return true;
    if(mode == RmMenu) return remote_menu_input(app, e);

    if(e->key == InputKeyBack) {
        if(e->type == InputTypeLong) {
            exit_to_main(app);
            return true;
        }
        if(e->type == InputTypeShort) return false; // -> Remote menu
        return true; // swallow Back press/release so a trailing release cannot navigate
    }

    if(e->type == InputTypePress) {
        if(e->key == InputKeyLeft || e->key == InputKeyRight)
            channel_step(app, e->key == InputKeyRight);
        with_view_model(app->remote_view, RemoteModel * m, { m->pressed = e->key + 1; }, true);
        return true;
    }
    if(e->type == InputTypeRepeat) {
        if(e->key == InputKeyLeft || e->key == InputKeyRight) {
            channel_step(app, e->key == InputKeyRight);
            remote_commit(app);
            return true;
        }
        return true;
    }
    if(e->type == InputTypeRelease) {
        if(app->jogging) {
            app->tx_abort = true; // stop the hold-jog
            app->jogging = false;
        }
        if(!app->tx_busy) // keep the key highlighted until the worker finishes

            with_view_model(app->remote_view, RemoteModel * m, { m->pressed = 0; }, true);
        return true;
    }
    if(e->type == InputTypeShort) {
        // Tilt mode taps send short frames; roller mode sends long.
        if(e->key == InputKeyUp)
            send_one(
                app,
                app->tilt ? BTN_UP_SHORT : BTN_UP_LONG,
                app->tx_count,
                false,
                app->tilt ? "Up Short" : "Up Long");
        else if(e->key == InputKeyDown)
            send_one(
                app,
                app->tilt ? BTN_DOWN_SHORT : BTN_DOWN_LONG,
                app->tx_count,
                false,
                app->tilt ? "Down Short" : "Down Long");
        else if(e->key == InputKeyOk)
            send_one(app, BTN_STOP, app->tx_count, false, "Stop");
        return true;
    }
    if(e->type == InputTypeLong) {
        if(app->tilt && e->key == InputKeyUp) {
            start_jog(app, BTN_UP_SHORT, "Up Short");
        } else if(app->tilt && e->key == InputKeyDown) {
            start_jog(app, BTN_DOWN_SHORT, "Down Short");
        } else {
            // roller: Up/Down open the limit menus; OK opens the actions menu (both modes)
            uint8_t kind = (e->key == InputKeyUp)   ? MkTop :
                           (e->key == InputKeyDown) ? MkBottom :
                           (e->key == InputKeyOk)   ? MkStop :
                                                      0xFF;
            if(kind != 0xFF) {
                with_view_model(
                    app->remote_view,
                    RemoteModel * m,
                    {
                        m->mode = RmMenu;
                        m->menu_kind = kind;
                        m->menu_sel = 0;
                        m->pressed = 0;
                    },
                    true);
            }
        }
        return true;
    }
    return false;
}

static void list_draw(Canvas* c, void* ctx) {
    ListModel* m = ctx;
    App* app = m->app;

    canvas_clear(c);
    canvas_set_color(c, ColorBlack);
    canvas_set_font(c, FontPrimary);
    char hdr[24];
    snprintf(hdr, sizeof(hdr), "Serials (%lu)", (unsigned long)effective_count(app));
    canvas_draw_str(c, 2, 10, hdr);
    canvas_draw_line(c, 0, 12, SCREEN_W - 1, 12);

    canvas_set_font(c, FontSecondary);
    if(app->count == 0) {
        canvas_draw_str_aligned(c, 64, 36, AlignCenter, AlignCenter, "Empty - back to add");
        return;
    }

    const int rows = 4, rh = 12;
    for(int i = 0; i < rows && (m->top + i) < app->count; i++) {
        int idx = m->top + i;
        int y = 14 + i * rh;
        char s[16];
        serial_pattern_str(app->serials[idx], app->serial_wild[idx], s, sizeof(s));
        const char* modetag = app->serial_mode[idx] ? "mask" : "std";
        if(idx == m->sel) {
            canvas_draw_box(c, 0, y, SCREEN_W, rh);
            canvas_set_color(c, ColorWhite);
        }
        draw_mono(c, 4, y + 9, s);
        canvas_draw_str_aligned(c, SCREEN_W - 4, y + 9, AlignRight, AlignBottom, modetag);
        if(app->serial_disabled[idx]) canvas_draw_line(c, 2, y + rh / 2, SCREEN_W - 3, y + rh / 2);
        canvas_set_color(c, ColorBlack);
    }

    if(m->menu) {
        const char* items[3] = {
            "Edit", "Delete", app->serial_disabled[m->sel] ? "Enable" : "Disable"};
        draw_box_menu(c, NULL, items, 3, m->menu_sel);
    }
}

static void hex_enter_from_list(App* app, uint8_t idx);

static bool list_input(InputEvent* e, void* ctx) {
    App* app = ctx;
    bool menu = false;
    with_view_model(app->list_view, ListModel * m, { menu = m->menu; }, false);

    if(menu) {
        if(e->type != InputTypeShort && e->type != InputTypeRepeat) return true;
        if(e->key == InputKeyBack) {
            with_view_model(app->list_view, ListModel * m, { m->menu = false; }, true);
            return true;
        }
        if(e->key == InputKeyUp || e->key == InputKeyDown) {
            with_view_model(
                app->list_view,
                ListModel * m,
                {
                    m->menu_sel = (e->key == InputKeyUp) ?
                                      (m->menu_sel == 0 ? 2 : m->menu_sel - 1) :
                                      (m->menu_sel + 1) % 3;
                },
                true);
            return true;
        }
        if(e->key == InputKeyOk) {
            uint8_t sel = 0, msel = 0;
            with_view_model(
                app->list_view,
                ListModel * m,
                {
                    sel = m->sel;
                    msel = m->menu_sel;
                    m->menu = false;
                },
                true);
            if(msel == 2) {
                uint8_t key[SERIAL_LEN];
                memcpy(key, app->serials[sel], SERIAL_LEN);
                app->serial_disabled[sel] = !app->serial_disabled[sel];
                app->dirty = true;
                serials_sort(app);
                int pos = serial_find(app, key, -1);
                with_view_model(
                    app->list_view,
                    ListModel * m,
                    {
                        if(pos >= 0) m->sel = pos;
                        if(m->sel < m->top) m->top = m->sel;
                        if(m->sel >= m->top + 4) m->top = m->sel - 3;
                    },
                    true);
            } else if(msel == 1 && app->count <= 1) {
                info_dialog(app, APP_NAME, "Keep at least\none serial");
            } else if(msel == 1) {
                char s[16];
                serial_pattern_str(app->serials[sel], app->serial_wild[sel], s, sizeof(s));
                DialogMessage* dm = dialog_message_alloc();
                dialog_message_set_header(dm, "Delete serial?", 64, 4, AlignCenter, AlignTop);
                dialog_message_set_text(dm, s, 64, 32, AlignCenter, AlignCenter);
                dialog_message_set_buttons(dm, NULL, "Delete", NULL);
                DialogMessageButton b = dialog_message_show(app->dialogs, dm);
                dialog_message_free(dm);
                if(b == DialogMessageButtonCenter) {
                    for(int i = sel; i < app->count - 1; i++) {
                        memcpy(app->serials[i], app->serials[i + 1], SERIAL_LEN);
                        app->serial_mode[i] = app->serial_mode[i + 1];
                        app->serial_wild[i] = app->serial_wild[i + 1];
                        app->serial_disabled[i] = app->serial_disabled[i + 1];
                    }
                    app->count--;
                    app->dirty = true;
                    with_view_model(
                        app->list_view,
                        ListModel * m,
                        {
                            if(m->sel >= app->count && m->sel > 0) m->sel = app->count - 1;
                            if(app->count == 0) m->sel = 0;
                            if(m->top > m->sel) m->top = m->sel;
                        },
                        true);
                }
            } else {
                hex_enter_from_list(app, sel);
            }
            return true;
        }
        return true;
    }

    if(e->type != InputTypeShort && e->type != InputTypeRepeat) return false;
    if(e->key == InputKeyBack) return false;

    if(e->key == InputKeyUp || e->key == InputKeyDown) {
        with_view_model(
            app->list_view,
            ListModel * m,
            {
                if(app->count > 0) {
                    m->sel = (e->key == InputKeyUp) ? (m->sel == 0 ? app->count - 1 : m->sel - 1) :
                                                      (m->sel + 1) % app->count;
                    if(m->sel < m->top) m->top = m->sel;
                    if(m->sel >= m->top + 4) m->top = m->sel - 3;
                }
            },
            true);
        return true;
    }
    if(e->key == InputKeyOk && app->count > 0) {
        with_view_model(
            app->list_view,
            ListModel * m,
            {
                m->menu = true;
                m->menu_sel = 0;
            },
            true);
        return true;
    }
    return false;
}

static void hex_draw(Canvas* c, void* ctx) {
    HexModel* m = ctx;

    canvas_clear(c);
    canvas_set_color(c, ColorBlack);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(
        c, 64, 9, AlignCenter, AlignBottom, m->index < 0 ? "Add Serial" : "Edit Serial");

    const int x0[6] = {6, 24, 48, 66, 90, 108};
    const int by = 18, bh = 15;
    for(int i = 0; i < 6; i++) {
        int x = x0[i], w = 16, mcx = x + w / 2;
        char d[4];
        if(m->nib[i] == 16)
            snprintf(d, sizeof(d), "*");
        else
            snprintf(d, sizeof(d), "%X", m->nib[i] & 0x0F);

        // mini up/down arrows above and below every box
        canvas_draw_dot(c, mcx, by - 5);
        canvas_draw_line(c, mcx - 1, by - 4, mcx + 1, by - 4);
        canvas_draw_line(c, mcx - 2, by - 3, mcx + 2, by - 3);
        canvas_draw_line(c, mcx - 2, by + bh + 3, mcx + 2, by + bh + 3);
        canvas_draw_line(c, mcx - 1, by + bh + 4, mcx + 1, by + bh + 4);
        canvas_draw_dot(c, mcx, by + bh + 5);

        if(m->col == i) {
            canvas_draw_rbox(c, x, by, w, bh, 2);
            canvas_set_color(c, ColorWhite);
        } else {
            canvas_draw_rframe(c, x, by, w, bh, 2);
        }
        canvas_draw_str_aligned(c, mcx, by + bh / 2 + 1, AlignCenter, AlignCenter, d);
        canvas_set_color(c, ColorBlack);
    }

    // channel-mode field (col 6), with its own up/down arrows on the right
    canvas_set_font(c, FontSecondary);
    char mstr[20];
    snprintf(mstr, sizeof(mstr), "Mode: %s", m->mode ? "Mask 0x1n" : "Std 0x0n");
    int mx = 14, my = 42, mw = 100, mh = 13;
    if(m->col == 6) {
        canvas_draw_rbox(c, mx, my, mw, mh, 2);
        canvas_set_color(c, ColorWhite);
    } else {
        canvas_draw_rframe(c, mx, my, mw, mh, 2);
    }
    canvas_draw_str(c, mx + 5, my + 10, mstr);
    int ax = mx + mw - 8;
    canvas_draw_dot(c, ax, my + 3);
    canvas_draw_line(c, ax - 1, my + 4, ax + 1, my + 4);
    canvas_draw_line(c, ax - 1, my + mh - 5, ax + 1, my + mh - 5);
    canvas_draw_dot(c, ax, my + mh - 4);
    canvas_set_color(c, ColorBlack);

    canvas_draw_str_aligned(c, 64, 63, AlignCenter, AlignBottom, "OK: save");
}

static bool hex_input(InputEvent* e, void* ctx) {
    App* app = ctx;
    if(e->type != InputTypeShort && e->type != InputTypeRepeat) return false;

    if(e->key == InputKeyBack) return false;

    if(e->key == InputKeyLeft || e->key == InputKeyRight) {
        with_view_model(
            app->hex_view,
            HexModel * m,
            {
                if(e->key == InputKeyLeft) m->col = (m->col == 0) ? 6 : m->col - 1;
                if(e->key == InputKeyRight) m->col = (m->col == 6) ? 0 : m->col + 1;
            },
            true);
        return true;
    }
    if(e->key == InputKeyUp || e->key == InputKeyDown) {
        with_view_model(
            app->hex_view,
            HexModel * m,
            {
                if(m->col == 6)
                    m->mode = !m->mode;
                else if(e->key == InputKeyUp)
                    m->nib[m->col] = (m->nib[m->col] >= 16) ? 0 : m->nib[m->col] + 1;
                else
                    m->nib[m->col] = (m->nib[m->col] == 0) ? 16 : m->nib[m->col] - 1;
            },
            true);
        return true;
    }
    if(e->key == InputKeyOk) {
        uint8_t nib[6];
        int index = -1;
        uint8_t mode = 0;
        with_view_model(
            app->hex_view,
            HexModel * m,
            {
                memcpy(nib, m->nib, sizeof(nib));
                index = m->index;
                mode = m->mode;
            },
            false);

        uint8_t bytes[SERIAL_LEN] = {0, 0, 0};
        uint8_t wild = 0;
        for(uint8_t n = 0; n < 6; n++) {
            uint8_t v = nib[n];
            if(v == 16) {
                wild |= (1 << n);
                v = 0;
            }
            if(n & 1)
                bytes[n / 2] |= v;
            else
                bytes[n / 2] |= v << 4;
        }

        bool exact = false;
        if(serial_find_overlap(app, bytes, wild, index, &exact) >= 0) {
            info_dialog(
                app,
                APP_NAME,
                exact ? "Serial already\nin this remote" : "Overlaps a mask\nalready added");
            return true;
        }
        if(index < 0) {
            if(app->count >= MAX_SERIALS) {
                info_dialog(app, APP_NAME, "Remote is full");
                return true;
            }
            memcpy(app->serials[app->count], bytes, SERIAL_LEN);
            app->serial_mode[app->count] = mode;
            app->serial_wild[app->count] = wild;
            app->serial_disabled[app->count] = 0;
            app->count++;
            serials_sort(app);
            app->dirty = true;
            if(app->creating) {
                app->creating = false;
                enter_remote(app);
            } else {
                build_edit_menu(app);
                view_dispatcher_switch_to_view(app->vd, ViewEditMenu);
            }
        } else {
            memcpy(app->serials[index], bytes, SERIAL_LEN);
            app->serial_mode[index] = mode;
            app->serial_wild[index] = wild;
            app->dirty = true;
            serials_sort(app);
            int pos = serial_find(app, bytes, -1);
            with_view_model(
                app->list_view, ListModel * m, { m->sel = (pos >= 0) ? pos : 0; }, false);
            view_dispatcher_switch_to_view(app->vd, ViewList);
        }
        return true;
    }
    return false;
}

static void hex_set(App* app, const uint8_t* bytes, int index, uint8_t mode, uint8_t wild) {
    uint8_t b[3] = {0, 0, 0};
    if(bytes) memcpy(b, bytes, SERIAL_LEN);
    with_view_model(
        app->hex_view,
        HexModel * m,
        {
            for(uint8_t i = 0; i < 6; i++)
                m->nib[i] = (wild & (1 << i)) ? 16 : nibble_at(b, i);
            m->col = 0;
            m->mode = mode;
            m->index = index;
        },
        true);
}

static void hex_enter_from_list(App* app, uint8_t idx) {
    hex_set(app, app->serials[idx], idx, app->serial_mode[idx], app->serial_wild[idx]);
    view_dispatcher_switch_to_view(app->vd, ViewHex);
}

static uint32_t prev_main(void* ctx) {
    UNUSED(ctx);
    return VIEW_NONE;
}
static uint32_t prev_to_main(void* ctx) {
    UNUSED(ctx);
    return ViewMain;
}
static uint32_t prev_to_remote(void* ctx) {
    UNUSED(ctx);
    return ViewRemote;
}
static uint32_t prev_to_remote_menu(void* ctx) {
    UNUSED(ctx);
    return ViewRemoteMenu;
}
static uint32_t prev_to_edit(void* ctx) {
    UNUSED(ctx);
    return ViewEditMenu;
}
static uint32_t prev_hex(void* ctx) {
    App* app = ctx;
    int index = -1;
    with_view_model(app->hex_view, HexModel * m, { index = m->index; }, false);
    if(index >= 0) return ViewList;
    return app->creating ? ViewCreateMenu : ViewEditMenu;
}

static void remote_view_enter(void* ctx) {
    App* app = ctx;
    if(app->radio_ext) radio_power(true);
    subghz_devices_init();
    app->ext_ok = true;
    if(app->radio_ext) {
        const SubGhzDevice* d = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_EXT_NAME);
        app->ext_ok = d && subghz_devices_is_connect(d);
    }
}

static void remote_view_exit(void* ctx) {
    App* app = ctx;
    app->tx_abort = true;
    tx_join(app); // wait for any in-flight transmission before closing the radio
    app->tx_busy = false;
    subghz_devices_deinit();
    radio_power(false);
}

static void enter_remote(App* app) {
    app->creating = false;
    with_view_model(
        app->remote_view,
        RemoteModel * m,
        {
            m->mode = RmNormal;
            m->pressed = 0;
        },
        true);
    with_view_model(
        app->remote_menu_view,
        RemoteMenuModel * m,
        {
            m->sel = 0;
            m->top = 0;
        },
        false);
    view_dispatcher_switch_to_view(app->vd, ViewRemote);
}

static bool browse(App* app, const char* ext, const char* base, FuriString* path) {
    DialogsFileBrowserOptions opt;
    dialog_file_browser_set_basic_options(&opt, ext, &I_icon);
    opt.base_path = base;
    return dialog_file_browser_show(app->dialogs, path, path, &opt);
}

static bool add_from_file(App* app) {
    bool added = false;
    if(browse(app, ".sub", SUBGHZ_FOLDER, app->sub_path)) {
        uint8_t serial[SERIAL_LEN];
        uint8_t mode = 0;
        bool exact = false;
        if(!serial_from_sub(app, furi_string_get_cstr(app->sub_path), serial, &mode))
            info_dialog(app, APP_NAME, "Not a Dooya\n.sub file");
        else if(serial_find_overlap(app, serial, 0, -1, &exact) >= 0)
            info_dialog(
                app,
                APP_NAME,
                exact ? "Serial already\nin this remote" : "Overlaps a mask\nalready added");
        else if(app->count >= MAX_SERIALS)
            info_dialog(app, APP_NAME, "Remote is full");
        else {
            memcpy(app->serials[app->count], serial, SERIAL_LEN);
            app->serial_mode[app->count] = mode;
            app->serial_wild[app->count] = 0;
            app->serial_disabled[app->count] = 0;
            app->count++;
            serials_sort(app);
            app->dirty = true;
            added = true;
        }
    }
    return added;
}

static void build_main_menu(App* app) {
    submenu_reset(app->main_menu);
    submenu_set_header(app->main_menu, APP_NAME);
    submenu_add_item(app->main_menu, "Load Remote", MainLoad, main_menu_cb, app);
    submenu_add_item(app->main_menu, "Create Remote", MainCreate, main_menu_cb, app);
    char radio[20];
    snprintf(radio, sizeof(radio), "Radio: %s", app->radio_ext ? "External" : "Internal");
    submenu_add_item(app->main_menu, radio, MainRadio, main_menu_cb, app);
    submenu_add_item(app->main_menu, "About", MainAbout, main_menu_cb, app);
}

static void main_menu_cb(void* ctx, uint32_t index) {
    App* app = ctx;
    if(index == MainLoad) {
        FuriString* path = furi_string_alloc_set(APP_FOLDER);
        if(browse(app, FILE_EXT, APP_FOLDER, path)) {
            app->count = 0;
            if(remote_load(app, furi_string_get_cstr(path))) {
                name_from_path(furi_string_get_cstr(path), app->name, sizeof(app->name));
                app->channel = 0;
                set_cmd(app, "", 0);
                app->dirty = false;
                enter_remote(app);
            } else {
                info_dialog(app, APP_NAME, "Could not load\nremote file");
            }
        }
        furi_string_free(path);
    } else if(index == MainCreate) {
        app->count = 0;
        app->channel = 0;
        app->name[0] = '\0';
        set_cmd(app, "", 0);
        app->tilt = false;
        app->tx_count = 2;
        app->dirty = false;
        app->creating = true;
        build_create_menu(app);
        view_dispatcher_switch_to_view(app->vd, ViewCreateMenu);
    } else if(index == MainRadio) {
        app->radio_ext = !app->radio_ext;
        build_main_menu(app);
        submenu_set_selected_item(app->main_menu, MainRadio);
    } else if(index == MainAbout) {
        view_dispatcher_switch_to_view(app->vd, ViewAbout);
    }
}

static void build_create_menu(App* app) {
    submenu_reset(app->create_menu);
    submenu_set_header(app->create_menu, "Create Remote");
    submenu_add_item(app->create_menu, "Create From File", CreateFromFile, create_menu_cb, app);
    submenu_add_item(app->create_menu, "Create Custom", CreateCustom, create_menu_cb, app);
}

static void create_menu_cb(void* ctx, uint32_t index) {
    App* app = ctx;
    if(index == CreateFromFile) {
        if(add_from_file(app)) {
            app->creating = false;
            enter_remote(app);
        }
    } else if(index == CreateCustom) {
        hex_set(app, NULL, -1, 0, 0);
        view_dispatcher_switch_to_view(app->vd, ViewHex);
    }
}

static void draw_adjust_arrows(Canvas* c, int rx, int cy) {
    fill_tri(c, rx - 7, cy, 5, 4, CanvasDirectionRightToLeft); // left-pointing
    fill_tri(c, rx - 4, cy, 5, 4, CanvasDirectionLeftToRight); // right-pointing
}

static uint8_t
    remote_menu_build(App* app, const char** labels, uint8_t* acts, char* txbuf, size_t txn) {
    uint8_t n = 0;
    labels[n] = "Manage Serials";
    acts[n++] = RemoteMenuManage;
    snprintf(txbuf, txn, "TX Count: %u", app->tx_count);
    labels[n] = txbuf;
    acts[n++] = RemoteMenuTxCount;
    if(app->dirty) {
        labels[n] = "Save Remote";
        acts[n++] = RemoteMenuSave;
    }
    if(app->name[0]) {
        labels[n] = "Rename Remote";
        acts[n++] = RemoteMenuRename;
        labels[n] = "Delete Remote";
        acts[n++] = RemoteMenuDelete;
    }
    return n;
}

static void do_rename(App* app) {
    app->ti_mode = TiRename;
    strncpy(app->name_buf, app->name, sizeof(app->name_buf) - 1);
    app->name_buf[sizeof(app->name_buf) - 1] = '\0';
    text_input_set_header_text(app->text_input, "Rename remote");
    text_input_set_result_callback(
        app->text_input, text_input_cb, app, app->name_buf, sizeof(app->name_buf), false);
    view_dispatcher_switch_to_view(app->vd, ViewTextInput);
}

static void do_delete(App* app) {
    DialogMessage* dm = dialog_message_alloc();
    dialog_message_set_header(dm, "Delete remote?", 64, 4, AlignCenter, AlignTop);
    dialog_message_set_text(dm, app->name, 64, 32, AlignCenter, AlignCenter);
    dialog_message_set_buttons(dm, NULL, "Delete", NULL);
    DialogMessageButton b = dialog_message_show(app->dialogs, dm);
    dialog_message_free(dm);
    if(b == DialogMessageButtonCenter) {
        remote_delete_file(app, app->name);
        app->count = 0;
        app->name[0] = '\0';
        app->dirty = false;
        view_dispatcher_switch_to_view(app->vd, ViewMain);
    }
}

#define RMENU_IH      16
#define RMENU_HDR     16
#define RMENU_VISIBLE 3

static void remote_menu_draw(Canvas* c, void* ctx) {
    RemoteMenuModel* m = ctx;
    App* app = m->app;
    canvas_clear(c);

    const char* labels[5];
    uint8_t acts[5];
    char txbuf[18];
    uint8_t n = remote_menu_build(app, labels, acts, txbuf, sizeof(txbuf));

    canvas_set_color(c, ColorBlack);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 4, 12, "Remote");

    canvas_set_font(c, FontSecondary);
    for(int i = 0; i < RMENU_VISIBLE && (m->top + i) < n; i++) {
        int idx = m->top + i;
        int y = RMENU_HDR + i * RMENU_IH;
        if(idx == m->sel) {
            canvas_set_color(c, ColorBlack);
            elements_slightly_rounded_box(c, 0, y, 122, RMENU_IH - 1);
            canvas_set_color(c, ColorWhite);
        } else {
            canvas_set_color(c, ColorBlack);
        }
        canvas_draw_str(c, 6, y + 11, labels[idx]);
        if(acts[idx] == RemoteMenuTxCount) draw_adjust_arrows(c, 112, y + RMENU_IH / 2);
        canvas_set_color(c, ColorBlack);
    }
    elements_scrollbar(c, m->sel, n);
}

static bool remote_menu_view_input(InputEvent* e, void* ctx) {
    App* app = ctx;
    if(e->key == InputKeyBack) {
        if(e->type == InputTypeLong) {
            exit_to_main(app);
            return true;
        }
        return false; // short back -> previous callback -> keypad
    }
    if(e->type != InputTypeShort && e->type != InputTypeRepeat) return false;

    const char* labels[5];
    uint8_t acts[5];
    char txbuf[18];
    uint8_t n = remote_menu_build(app, labels, acts, txbuf, sizeof(txbuf));

    if(e->key == InputKeyUp || e->key == InputKeyDown) {
        with_view_model(
            app->remote_menu_view,
            RemoteMenuModel * m,
            {
                m->sel = (e->key == InputKeyUp) ? (m->sel == 0 ? n - 1 : m->sel - 1) :
                                                  (m->sel + 1) % n;
                if(m->sel < m->top) m->top = m->sel;
                if(m->sel >= m->top + RMENU_VISIBLE) m->top = m->sel - (RMENU_VISIBLE - 1);
            },
            true);
        return true;
    }

    uint8_t sel = 0;
    with_view_model(app->remote_menu_view, RemoteMenuModel * m, { sel = m->sel; }, false);
    if(sel >= n) sel = n - 1;
    uint8_t act = acts[sel];

    // Left/Right adjust TX Count (1..5, no wrap); arrows are the only way to change it.
    if((e->key == InputKeyLeft || e->key == InputKeyRight) && act == RemoteMenuTxCount) {
        if(e->key == InputKeyRight && app->tx_count < 5) {
            app->tx_count++;
            app->dirty = true;
        } else if(e->key == InputKeyLeft && app->tx_count > 1) {
            app->tx_count--;
            app->dirty = true;
        }
        with_view_model(app->remote_menu_view, RemoteMenuModel * m, { UNUSED(m); }, true);
        return true;
    }

    if(e->key == InputKeyOk && e->type == InputTypeShort) {
        if(act == RemoteMenuSave) {
            app->ti_mode = TiSave;
            strncpy(app->name_buf, app->name[0] ? app->name : "dooya", sizeof(app->name_buf) - 1);
            app->name_buf[sizeof(app->name_buf) - 1] = '\0';
            text_input_set_header_text(app->text_input, "Save remote as");
            text_input_set_result_callback(
                app->text_input, text_input_cb, app, app->name_buf, sizeof(app->name_buf), false);
            view_dispatcher_switch_to_view(app->vd, ViewTextInput);
        } else if(act == RemoteMenuManage) {
            build_edit_menu(app);
            view_dispatcher_switch_to_view(app->vd, ViewEditMenu);
        } else if(act == RemoteMenuRename) {
            do_rename(app);
        } else if(act == RemoteMenuDelete) {
            do_delete(app);
        }
        return true;
    }
    return false;
}

static void build_edit_menu(App* app) {
    submenu_reset(app->edit_menu);
    submenu_set_header(app->edit_menu, "Manage Serials");
    submenu_add_item(app->edit_menu, "Add From File", EditAddFile, edit_menu_cb, app);
    submenu_add_item(app->edit_menu, "Add Custom", EditAddCustom, edit_menu_cb, app);
    submenu_add_item(app->edit_menu, "Edit List", EditList, edit_menu_cb, app);
}

static void edit_menu_cb(void* ctx, uint32_t index) {
    App* app = ctx;
    if(index == EditAddFile) {
        add_from_file(app);
    } else if(index == EditAddCustom) {
        hex_set(app, NULL, -1, 0, 0);
        view_dispatcher_switch_to_view(app->vd, ViewHex);
    } else if(index == EditList) {
        serials_sort(app);
        with_view_model(
            app->list_view,
            ListModel * m,
            {
                m->sel = 0;
                m->top = 0;
                m->menu = false;
            },
            true);
        view_dispatcher_switch_to_view(app->vd, ViewList);
    }
}

static void show_remote_menu(App* app) {
    with_view_model(
        app->remote_menu_view,
        RemoteMenuModel * m,
        {
            m->sel = 0;
            m->top = 0;
        },
        false);
    view_dispatcher_switch_to_view(app->vd, ViewRemoteMenu);
}

static void save_named(App* app) {
    const char* newname = app->name_buf;

    if(remote_file_exists(app, newname)) {
        DialogMessage* dm = dialog_message_alloc();
        dialog_message_set_header(dm, "Save remote?", 64, 4, AlignCenter, AlignTop);
        dialog_message_set_text(dm, "Overwrite the file?", 64, 32, AlignCenter, AlignCenter);
        dialog_message_set_buttons(dm, "Discard", NULL, "Save");
        DialogMessageButton b = dialog_message_show(app->dialogs, dm);
        dialog_message_free(dm);
        if(b != DialogMessageButtonRight) {
            show_remote_menu(app);
            return;
        }
    }

    bool keep_identity = (app->name[0] == '\0') || (strcmp(app->name, newname) == 0);
    bool ok = remote_save(app, newname);
    if(ok && keep_identity) {
        strncpy(app->name, newname, sizeof(app->name) - 1);
        app->name[sizeof(app->name) - 1] = '\0';
        app->dirty = false;
    }
    info_dialog(app, APP_NAME, ok ? "Remote saved" : "Save failed");
    show_remote_menu(app);
}

static void text_input_cb(void* ctx) {
    App* app = ctx;
    if(app->ti_mode == TiSave) {
        // a dialog here would freeze the GUI, so run the save on the dispatcher loop
        view_dispatcher_send_custom_event(app->vd, CustomSaveNamed);
        return;
    }

    char old[64];
    strncpy(old, app->name, sizeof(old) - 1);
    old[sizeof(old) - 1] = '\0';

    bool ok = remote_save(app, app->name_buf);
    if(ok) {
        if(app->ti_mode == TiRename && old[0] && strcmp(old, app->name_buf) != 0)
            remote_delete_file(app, old);
        strncpy(app->name, app->name_buf, sizeof(app->name) - 1);
        app->name[sizeof(app->name) - 1] = '\0';
        app->dirty = false;
        set_cmd(app, app->ti_mode == TiRename ? "Renamed" : "Saved", 0);
    } else {
        set_cmd(app, "Save failed", 0);
    }

    if(ok && app->ti_mode == TiSaveExit)
        view_dispatcher_switch_to_view(app->vd, ViewMain);
    else
        enter_remote(app);
}

static bool custom_event_cb(void* ctx, uint32_t event) {
    App* app = ctx;
    if(event == CustomSaveNamed) {
        save_named(app);
        return true;
    }
    return false;
}

// Redraws the keypad while a long group send runs, on the GUI thread, so the
// progress panel advances without the worker ever touching the display.
static void tick_cb(void* ctx) {
    App* app = ctx;
    if(app->tx_busy && app->tx_show_progress) remote_commit(app);
}

static void exit_to_main(App* app) {
    if(!app->dirty) {
        view_dispatcher_switch_to_view(app->vd, ViewMain);
        return;
    }
    DialogMessage* dm = dialog_message_alloc();
    dialog_message_set_header(dm, "Save changes?", 64, 4, AlignCenter, AlignTop);
    dialog_message_set_text(
        dm,
        app->name[0] ? "Overwrite the file?" : "Name and save?",
        64,
        32,
        AlignCenter,
        AlignCenter);
    dialog_message_set_buttons(dm, "Discard", NULL, "Save");
    DialogMessageButton b = dialog_message_show(app->dialogs, dm);
    dialog_message_free(dm);

    if(b == DialogMessageButtonRight) {
        if(app->name[0]) {
            remote_save(app, app->name);
            app->dirty = false;
            view_dispatcher_switch_to_view(app->vd, ViewMain);
        } else {
            app->ti_mode = TiSaveExit;
            strncpy(app->name_buf, "dooya", sizeof(app->name_buf) - 1);
            app->name_buf[sizeof(app->name_buf) - 1] = '\0';
            text_input_set_header_text(app->text_input, "Save remote as");
            text_input_set_result_callback(
                app->text_input, text_input_cb, app, app->name_buf, sizeof(app->name_buf), false);
            view_dispatcher_switch_to_view(app->vd, ViewTextInput);
        }
    } else if(b == DialogMessageButtonLeft) {
        app->dirty = false;
        view_dispatcher_switch_to_view(app->vd, ViewMain);
    }
    // Back on the dialog: stay where we are.
}

static void build_about(App* app) {
    widget_add_text_box_element(
        app->about,
        0,
        0,
        SCREEN_W,
        14,
        AlignCenter,
        AlignBottom,
        "\e#\e!  Dooya Remote  \e!\n",
        false);
    widget_add_text_scroll_element(
        app->about,
        0,
        15,
        SCREEN_W,
        SCREEN_H - 15,
        "\e#Version " APP_VERSION "\n"
        "" APP_REPO "\n"
        "\n"
        "Emulate a Dooya shutter\n"
        "remote. Store one serial or\n"
        "a whole group, pick a\n"
        "channel, then send.\n"
        "\n"
        "\e#Keys (tap)\n"
        "Up: Up (long)\n"
        "Down: Down (long)\n"
        "OK: Stop\n"
        "Left/Right: channel\n"
        "Ch 0 = master (all)\n"
        "\n"
        "\e#Keys (hold)\n"
        "Hold OK: P2 / Swap /\n"
        "clear limits / tilt-roller\n"
        "\n"
        "Roller mode: hold Up/Down\n"
        "opens the limit menus.\n"
        "Tilt mode: tap sends short,\n"
        "hold jogs short until let go,\n"
        "no limit menus.\n"
        "\n"
        "A group sends to every\n"
        "stored serial on the\n"
        "chosen channel.\n"
        "\n"
        "Radio Int/Ext toggles in\n"
        "the Save/Edit menu. Ext\n"
        "uses the CC1101 add-on\n"
        "and its 5V OTG power.\n"
        "\n"
        "\e#Real remote combos\n"
        "Add remote: P2, P2, then\n"
        "P2 on the new remote.\n"
        "Set limit: Up+Stop or\n"
        "Down+Stop held 2s (5s to\n"
        "delete). Reverse: Up+Down.\n"
        "Sleep mode is a button on\n"
        "the motor head.");
}

static View* make_view(
    App* app,
    ViewDrawCallback draw,
    ViewInputCallback input,
    size_t msize,
    ViewNavigationCallback prev) {
    View* v = view_alloc();
    view_set_context(v, app);
    view_set_draw_callback(v, draw);
    view_set_input_callback(v, input);
    view_allocate_model(v, ViewModelTypeLocking, msize);
    view_set_previous_callback(v, prev);
    return v;
}

int32_t dooya_remote_app(void* p) {
    UNUSED(p);

    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));

    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    storage_simply_mkdir(app->storage, APP_FOLDER);
    if(!storage_common_exists(app->storage, DEFAULTS_DST) &&
       storage_common_exists(app->storage, DEFAULTS_SRC))
        storage_common_copy(app->storage, DEFAULTS_SRC, DEFAULTS_DST);
    app->sub_path = furi_string_alloc_set(SUBGHZ_FOLDER);
    app->radio_ext = ext_present();
    app->tx_count = 2;

    app->vd = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->vd, app);
    view_dispatcher_set_custom_event_callback(app->vd, custom_event_cb);
    view_dispatcher_set_tick_event_callback(app->vd, tick_cb, 250);

    app->main_menu = submenu_alloc();
    build_main_menu(app);
    view_set_previous_callback(submenu_get_view(app->main_menu), prev_main);

    app->remote_menu_view = make_view(
        app, remote_menu_draw, remote_menu_view_input, sizeof(RemoteMenuModel), prev_to_remote);

    app->create_menu = submenu_alloc();
    build_create_menu(app);
    view_set_previous_callback(submenu_get_view(app->create_menu), prev_to_main);

    app->edit_menu = submenu_alloc();
    build_edit_menu(app);
    view_set_previous_callback(submenu_get_view(app->edit_menu), prev_to_remote);

    app->about = widget_alloc();
    build_about(app);
    view_set_previous_callback(widget_get_view(app->about), prev_to_main);

    app->text_input = text_input_alloc();
    view_set_previous_callback(text_input_get_view(app->text_input), prev_to_remote_menu);

    app->remote_view =
        make_view(app, remote_draw, remote_input, sizeof(RemoteModel), prev_to_remote_menu);
    view_set_enter_callback(app->remote_view, remote_view_enter);
    view_set_exit_callback(app->remote_view, remote_view_exit);
    app->list_view = make_view(app, list_draw, list_input, sizeof(ListModel), prev_to_edit);
    app->hex_view = make_view(app, hex_draw, hex_input, sizeof(HexModel), prev_hex);

    with_view_model(app->remote_view, RemoteModel * m, { m->app = app; }, false);
    with_view_model(
        app->remote_menu_view,
        RemoteMenuModel * m,
        {
            m->app = app;
            m->sel = 0;
            m->top = 0;
        },
        false);
    with_view_model(app->list_view, ListModel * m, { m->app = app; }, false);
    with_view_model(
        app->hex_view,
        HexModel * m,
        {
            m->app = app;
            m->index = -1;
        },
        false);

    view_dispatcher_attach_to_gui(app->vd, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_add_view(app->vd, ViewMain, submenu_get_view(app->main_menu));
    view_dispatcher_add_view(app->vd, ViewRemote, app->remote_view);
    view_dispatcher_add_view(app->vd, ViewRemoteMenu, app->remote_menu_view);
    view_dispatcher_add_view(app->vd, ViewCreateMenu, submenu_get_view(app->create_menu));
    view_dispatcher_add_view(app->vd, ViewEditMenu, submenu_get_view(app->edit_menu));
    view_dispatcher_add_view(app->vd, ViewList, app->list_view);
    view_dispatcher_add_view(app->vd, ViewHex, app->hex_view);
    view_dispatcher_add_view(app->vd, ViewAbout, widget_get_view(app->about));
    view_dispatcher_add_view(app->vd, ViewTextInput, text_input_get_view(app->text_input));

    view_dispatcher_switch_to_view(app->vd, ViewMain);
    view_dispatcher_run(app->vd);

    view_dispatcher_remove_view(app->vd, ViewMain);
    view_dispatcher_remove_view(app->vd, ViewRemote);
    view_dispatcher_remove_view(app->vd, ViewRemoteMenu);
    view_dispatcher_remove_view(app->vd, ViewCreateMenu);
    view_dispatcher_remove_view(app->vd, ViewEditMenu);
    view_dispatcher_remove_view(app->vd, ViewList);
    view_dispatcher_remove_view(app->vd, ViewHex);
    view_dispatcher_remove_view(app->vd, ViewAbout);
    view_dispatcher_remove_view(app->vd, ViewTextInput);

    submenu_free(app->main_menu);
    submenu_free(app->create_menu);
    submenu_free(app->edit_menu);
    widget_free(app->about);
    text_input_free(app->text_input);
    view_free(app->remote_view);
    view_free(app->remote_menu_view);
    view_free(app->list_view);
    view_free(app->hex_view);
    view_dispatcher_free(app->vd);

    tx_join(app);
    radio_power(false);
    furi_string_free(app->sub_path);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);

    free(app);
    return 0;
}
