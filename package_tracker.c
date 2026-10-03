#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>

#include "tracker_util.h"
#include "http.h"
#include "prompt.h"
#include "menu.h"

#define MAX_PACKAGES 12
#define VISIBLE_ROWS 4
#define ROW_HEIGHT   13

#define PACK_DIR    "/ext/apps_data/package_tracker"
#define PACK_FILE   PACK_DIR "/packages.txt"
#define CONFIG_FILE PACK_DIR "/config.txt"

typedef struct {
    char label[24];
    char carrier[16];
    char tracking[28];
    char last_update[24];
    char location[28];
    PackageStatus status;
} Package;

typedef enum {
    ScreenList,
    ScreenDetail,
    ScreenConfirmDelete,
} Screen;

typedef enum {
    EventTypeInput,
    EventTypeRefreshDone,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} TrackerEvent;

typedef struct {
    Screen screen;
    uint8_t selected;
    uint8_t scroll;
    bool refreshing;
    volatile bool cancel;
    char msg[40];
    FuriMutex* mutex;
    FuriThread* worker;
    ViewPort* view_port;
    FuriMessageQueue* queue;
    Gui* gui;
} TrackerState;

static Package packages[MAX_PACKAGES];
static uint8_t package_count = 0;
static TrackerConfig config;

// --- file loading --------------------------------------------------------

static PackageStatus status_from_str(const char* s) {
    if(!strcmp(s, "delivered")) return StatusDelivered;
    if(!strcmp(s, "out")) return StatusOutForDelivery;
    if(!strcmp(s, "transit")) return StatusInTransit;
    if(!strcmp(s, "exception")) return StatusException;
    return StatusPending;
}

static void copy_field(char* dst, size_t cap, const char* src) {
    while(*src == ' ' || *src == '\t')
        src++;
    size_t len = 0;
    while(src[len])
        len++;
    while(len > 0 && (src[len - 1] == ' ' || src[len - 1] == '\t'))
        len--;
    if(len > cap - 1) len = cap - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

static bool parse_line(char* line, Package* pkg) {
    char* fields[6];
    int n = 0;
    fields[n++] = line;
    for(char* p = line; *p && n < 6; p++) {
        if(*p == '|') {
            *p = '\0';
            fields[n++] = p + 1;
        }
    }
    if(n < 6) return false;
    copy_field(pkg->label, sizeof(pkg->label), fields[0]);
    copy_field(pkg->carrier, sizeof(pkg->carrier), fields[1]);
    copy_field(pkg->tracking, sizeof(pkg->tracking), fields[2]);
    char st[16];
    copy_field(st, sizeof(st), fields[3]);
    pkg->status = status_from_str(st);
    copy_field(pkg->location, sizeof(pkg->location), fields[4]);
    copy_field(pkg->last_update, sizeof(pkg->last_update), fields[5]);
    return pkg->label[0] != '\0';
}

static void parse_packages(char* buf) {
    char* line = buf;
    while(line && *line && package_count < MAX_PACKAGES) {
        char* p = line;
        while(*p && *p != '\n')
            p++;
        char* next = (*p == '\n') ? p + 1 : NULL;
        *p = '\0';
        size_t len = 0;
        while(line[len])
            len++;
        if(len > 0 && line[len - 1] == '\r') line[len - 1] = '\0';

        char* t = line;
        while(*t == ' ' || *t == '\t')
            t++;
        if(*t != '\0' && *t != '#') {
            Package pkg;
            memset(&pkg, 0, sizeof(pkg));
            if(parse_line(line, &pkg)) packages[package_count++] = pkg;
        }
        line = next;
    }
}

static char* read_file(Storage* storage, const char* path) {
    File* f = storage_file_alloc(storage);
    char* buf = NULL;
    if(storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t sz = storage_file_size(f);
        if(sz > 0) {
            if(sz > 8192) sz = 8192;
            buf = malloc((size_t)sz + 1);
            size_t rd = storage_file_read(f, buf, (size_t)sz);
            buf[rd] = '\0';
        }
    }
    storage_file_close(f);
    storage_file_free(f);
    return buf;
}

static void write_file(Storage* storage, const char* path, const char* text) {
    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(f, text, strlen(text));
    }
    storage_file_close(f);
    storage_file_free(f);
}

// Create the file with a template if it doesn't already exist.
static void ensure_file(Storage* storage, const char* path, const char* tmpl) {
    FileInfo info;
    bool exists = (storage_common_stat(storage, path, &info) == FSE_OK);
    if(!exists) write_file(storage, path, tmpl);
}

static const char* status_to_str(PackageStatus s) {
    switch(s) {
    case StatusDelivered:
        return "delivered";
    case StatusOutForDelivery:
        return "out";
    case StatusInTransit:
        return "transit";
    case StatusException:
        return "exception";
    default:
        return "pending";
    }
}

// The pipe and newline are field and record separators, so they can never
// appear inside a value the user typed.
static void sanitize_field(char* s) {
    for(; *s; s++)
        if(*s == '|' || *s == '\n' || *s == '\r') *s = ' ';
}

// Rewrite packages.txt from what is currently in memory.
static void save_packages(void) {
    size_t cap = 4096;
    char* buf = malloc(cap);
    int n = snprintf(
        buf,
        cap,
        "# Pack Track - one package per line:\n"
        "#   Label | Carrier | Tracking | Status | Location | Updated\n"
        "# Status: pending, transit, out, delivered, exception\n");
    if(n < 0) n = 0;

    for(uint8_t i = 0; i < package_count && (size_t)n < cap; i++) {
        int w = snprintf(
            buf + n,
            cap - (size_t)n,
            "%s | %s | %s | %s | %s | %s\n",
            packages[i].label,
            packages[i].carrier,
            packages[i].tracking,
            status_to_str(packages[i].status),
            packages[i].location[0] ? packages[i].location : "-",
            packages[i].last_update[0] ? packages[i].last_update : "-");
        if(w < 0 || (size_t)w >= cap - (size_t)n) break;
        n += w;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, PACK_DIR);
    write_file(storage, PACK_FILE, buf);
    furi_record_close(RECORD_STORAGE);
    free(buf);
}

static void load_all(void) {
    package_count = 0;
    memset(&config, 0, sizeof(config));

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, PACK_DIR);

    ensure_file(
        storage,
        PACK_FILE,
        // Comments only: the app adds packages itself, and nobody should have
        // to delete example data before using their own. The header documents
        // the format for anyone who edits this file from a computer.
        "# Pack Track - one package per line:\n"
        "#   Label | Carrier | Tracking | Status | Location | Updated\n"
        "# Status: pending, transit, out, delivered, exception\n"
        "# Packages added in the app are written here automatically.\n");
    ensure_file(
        storage,
        CONFIG_FILE,
        "# Pack Track live-tracking config (optional).\n"
        "# Fill this in to fetch real status with a WiFi devboard + your own\n"
        "# tracking API key. Press RIGHT in the app to refresh.\n"
        "WIFI_SSID = \n"
        "WIFI_PASS = \n"
        "# {tracking} and {carrier} are replaced per package:\n"
        "URL = \n"
        "# Optional headers (repeatable), e.g. your API key:\n"
        "# HEADER = Authorization: Bearer YOUR_KEY\n"
        "# JSON field paths in the response (dot keys, numbers = array index):\n"
        "FIELD_STATUS = \n"
        "FIELD_LOCATION = \n"
        "FIELD_UPDATED = \n");

    char* pkgbuf = read_file(storage, PACK_FILE);
    if(pkgbuf) {
        parse_packages(pkgbuf);
        free(pkgbuf);
    }
    char* cfgbuf = read_file(storage, CONFIG_FILE);
    if(cfgbuf) {
        config_parse(cfgbuf, &config);
        free(cfgbuf);
    }
    furi_record_close(RECORD_STORAGE);
}

// --- refresh worker ------------------------------------------------------

static void refresh_msg(TrackerState* s, const char* m) {
    furi_mutex_acquire(s->mutex, FuriWaitForever);
    strncpy(s->msg, m, sizeof(s->msg) - 1);
    s->msg[sizeof(s->msg) - 1] = '\0';
    furi_mutex_release(s->mutex);
    view_port_update(s->view_port);
}

static void
    apply_field(char* dst, size_t cap, FuriMutex* mtx, const char* body, const char* path) {
    if(!path || !path[0]) return;
    char val[64];
    if(json_extract(body, path, val, sizeof(val)) && val[0]) {
        furi_mutex_acquire(mtx, FuriWaitForever);
        strncpy(dst, val, cap - 1);
        dst[cap - 1] = '\0';
        furi_mutex_release(mtx);
    }
}

// Ask for the three fields on the device keyboard, then append and save.
// Runs on the app thread with the state mutex released, because the keyboard
// blocks and the render callback needs that mutex to draw.
static void add_package_flow(TrackerState* state) {
    if(package_count >= MAX_PACKAGES) {
        refresh_msg(state, "List is full");
        return;
    }

    char tracking[28] = "";
    char label[24] = "Package";
    char carrier[16] = "UPS";

    view_port_enabled_set(state->view_port, false);
    bool ok = prompt_text(state->gui, "Tracking number", tracking, sizeof(tracking), 1) &&
              prompt_text(state->gui, "Label", label, sizeof(label), 1) &&
              prompt_text(state->gui, "Carrier", carrier, sizeof(carrier), 1);
    view_port_enabled_set(state->view_port, true);

    if(!ok) {
        refresh_msg(state, "");
        return;
    }

    sanitize_field(tracking);
    sanitize_field(label);
    sanitize_field(carrier);

    furi_mutex_acquire(state->mutex, FuriWaitForever);
    Package* pkg = &packages[package_count];
    memset(pkg, 0, sizeof(*pkg));
    copy_field(pkg->tracking, sizeof(pkg->tracking), tracking);
    copy_field(pkg->label, sizeof(pkg->label), label);
    copy_field(pkg->carrier, sizeof(pkg->carrier), carrier);
    pkg->status = StatusPending;
    copy_field(pkg->location, sizeof(pkg->location), "-");
    copy_field(pkg->last_update, sizeof(pkg->last_update), "-");
    package_count++;

    state->selected = package_count - 1;
    if(state->selected >= state->scroll + VISIBLE_ROWS)
        state->scroll = state->selected - VISIBLE_ROWS + 1;
    furi_mutex_release(state->mutex);

    save_packages();
    refresh_msg(state, "Added");
}

// Scan from the board, pick a network, type the password once. The board
// stores the credentials itself, so they never touch the SD card.
static void wifi_setup_flow(TrackerState* state) {
    refresh_msg(state, "Connecting board...");
    FhttpClient* http = fhttp_alloc();

    if(!fhttp_open(http)) {
        refresh_msg(state, "No board found");
        fhttp_free(http);
        return;
    }
    if(!fhttp_ping(http)) {
        refresh_msg(state, "Board not responding");
        fhttp_close(http);
        fhttp_free(http);
        return;
    }

    refresh_msg(state, "Scanning WiFi...");
    static char ssids[12][TU_SSID_LEN];
    char* body = malloc(2048);
    bool scanned = fhttp_scan(http, body, 2048);
    int n = scanned ? ssid_list_parse(body, ssids, 12) : 0;
    free(body);

    if(n <= 0) {
        refresh_msg(state, scanned ? "No networks found" : "Scan failed");
        fhttp_close(http);
        fhttp_free(http);
        return;
    }

    const char* items[12];
    for(int i = 0; i < n; i++)
        items[i] = ssids[i];

    char pass[64] = "";
    view_port_enabled_set(state->view_port, false);
    int32_t pick = menu_pick(state->gui, "Select network", items, (size_t)n);
    // An open network is legitimate, so an empty password is allowed.
    bool confirmed = (pick >= 0) &&
                     prompt_text(state->gui, "WiFi password", pass, sizeof(pass), 0);
    view_port_enabled_set(state->view_port, true);

    if(!confirmed) {
        refresh_msg(state, "");
        fhttp_close(http);
        fhttp_free(http);
        return;
    }

    refresh_msg(state, "Joining WiFi...");
    bool joined = fhttp_wifi(http, ssids[pick], pass);
    refresh_msg(state, joined ? "WiFi saved to board" : "WiFi failed");

    fhttp_close(http);
    fhttp_free(http);
}

// Write a ready-made config for Trace (traceapi.dev) with the user's own key.
// Everything the service needs is known except the key, so this is the whole
// of live-tracking setup: no file editing, and the key stays on the device.
static void tracking_setup_flow(TrackerState* state) {
    char key[72] = "";

    view_port_enabled_set(state->view_port, false);
    bool got = prompt_text(state->gui, "Trace API key (trc_...)", key, sizeof(key), 4);
    view_port_enabled_set(state->view_port, true);
    if(!got) {
        refresh_msg(state, "");
        return;
    }

    // A stray quote or newline would corrupt the header line.
    sanitize_field(key);
    for(char* q = key; *q; q++)
        if(*q == '"') *q = ' ';

    char* text = malloc(768);
    snprintf(
        text,
        768,
        "# Pack Track live tracking, written by the app.\n"
        "# Edit by hand only if you want a different service.\n"
        "METHOD = POST\n"
        "URL = https://api.traceapi.dev/v1/track\n"
        "HEADER = Authorization: Bearer %s\n"
        "HEADER = Content-Type: application/json\n"
        "BODY = {\"tracking_number\":\"{tracking}\"}\n"
        "FIELD_STATUS = status\n"
        "FIELD_LOCATION = events.last.location\n"
        "FIELD_UPDATED = events.last.timestamp\n",
        key);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, PACK_DIR);
    write_file(storage, CONFIG_FILE, text);
    furi_record_close(RECORD_STORAGE);
    free(text);

    load_all(); // pick the new config up immediately
    refresh_msg(state, "Tracking configured");
}

// One request for a package, using whichever method the config asks for.
static bool tracker_fetch(
    FhttpClient* http,
    const char* tracking,
    const char* carrier,
    char* out,
    size_t out_cap) {
    const char* hdrs[TU_HDR_MAX];
    for(int i = 0; i < config.header_count; i++)
        hdrs[i] = config.headers[i];

    char url[TU_URL_MAX + 96];
    url_build(config.url, tracking, carrier, url, sizeof(url));

    if(config.is_post) {
        char body[TU_BODY_MAX + 96];
        url_build(config.body, tracking, carrier, body, sizeof(body));
        return fhttp_post(http, url, hdrs, config.header_count, body, out, out_cap);
    }
    return fhttp_get(http, url, hdrs, config.header_count, out, out_cap);
}

static void delete_selected(TrackerState* state) {
    furi_mutex_acquire(state->mutex, FuriWaitForever);
    if(package_count > 0) {
        for(uint8_t i = state->selected; i + 1 < package_count; i++)
            packages[i] = packages[i + 1];
        package_count--;

        if(package_count == 0) {
            state->selected = 0;
            state->scroll = 0;
        } else {
            if(state->selected >= package_count) state->selected = package_count - 1;
            if(state->scroll > 0 && state->scroll + VISIBLE_ROWS > package_count)
                state->scroll =
                    (package_count > VISIBLE_ROWS) ? (uint8_t)(package_count - VISIBLE_ROWS) : 0;
        }
    }
    state->screen = ScreenList;
    furi_mutex_release(state->mutex);

    save_packages();
    refresh_msg(state, "Deleted");
}

static int32_t refresh_worker(void* ctx) {
    TrackerState* s = ctx;
    FhttpClient* http = fhttp_alloc();

    refresh_msg(s, "Connecting board...");
    if(!fhttp_open(http)) {
        refresh_msg(s, "No board found");
        fhttp_free(http);
        goto done;
    }
    if(!fhttp_ping(http)) {
        refresh_msg(s, "Board not responding");
        fhttp_close(http);
        fhttp_free(http);
        goto done;
    }
    if(config.has_wifi) {
        refresh_msg(s, "Connecting WiFi...");
        if(!fhttp_wifi(http, config.wifi_ssid, config.wifi_pass)) {
            refresh_msg(s, "WiFi failed");
            fhttp_close(http);
            fhttp_free(http);
            goto done;
        }
    }

    char* body = malloc(4096);
    uint8_t failed = 0;
    for(uint8_t i = 0; i < package_count && !s->cancel; i++) {
        char m[40];
        snprintf(m, sizeof(m), "Refreshing %d/%d...", i + 1, package_count);
        refresh_msg(s, m);

        if(!tracker_fetch(http, packages[i].tracking, packages[i].carrier, body, 4096)) {
            failed++;
        } else {
            char stbuf[64];
            if(config.field_status[0] &&
               json_extract(body, config.field_status, stbuf, sizeof(stbuf)) && stbuf[0]) {
                furi_mutex_acquire(s->mutex, FuriWaitForever);
                packages[i].status = status_from_text(stbuf);
                furi_mutex_release(s->mutex);
            }
            apply_field(
                packages[i].location,
                sizeof(packages[i].location),
                s->mutex,
                body,
                config.field_location);
            // Services report ISO timestamps; shorten them to fit the screen.
            if(config.field_updated[0]) {
                char raw[64];
                if(json_extract(body, config.field_updated, raw, sizeof(raw)) && raw[0]) {
                    char pretty[32];
                    iso_to_short(raw, pretty, sizeof(pretty));
                    furi_mutex_acquire(s->mutex, FuriWaitForever);
                    strncpy(packages[i].last_update, pretty, sizeof(packages[i].last_update) - 1);
                    packages[i].last_update[sizeof(packages[i].last_update) - 1] = '\0';
                    furi_mutex_release(s->mutex);
                }
            }
        }
        view_port_update(s->view_port);
    }
    free(body);
    fhttp_close(http);
    fhttp_free(http);

    // Saying "Updated" after every lookup failed hides the problem; a silent
    // failure here is what makes a misconfigured service impossible to debug.
    if(s->cancel) {
        refresh_msg(s, "Cancelled");
    } else if(failed == 0) {
        refresh_msg(s, "Updated");
    } else if(failed == package_count) {
        refresh_msg(s, "No data - check setup");
    } else {
        char m[40];
        snprintf(m, sizeof(m), "Updated, %d failed", failed);
        refresh_msg(s, m);
    }

done:;
    TrackerEvent ev = {.type = EventTypeRefreshDone};
    furi_message_queue_put(s->queue, &ev, FuriWaitForever);
    return 0;
}

static void start_refresh(TrackerState* s) {
    if(s->refreshing) return;
    if(!config.has_url) {
        refresh_msg(s, "No URL in config.txt");
        return;
    }
    s->cancel = false;
    s->refreshing = true;
    s->worker = furi_thread_alloc_ex("PackRefresh", 4096, refresh_worker, s);
    furi_thread_start(s->worker);
}

// --- drawing -------------------------------------------------------------

static const char* status_short(PackageStatus s) {
    switch(s) {
    case StatusPending:
        return "Pending";
    case StatusInTransit:
        return "In Transit";
    case StatusOutForDelivery:
        return "Out Delivery";
    case StatusDelivered:
        return "Delivered";
    case StatusException:
        return "Exception";
    }
    return "?";
}

static const char* status_full(PackageStatus s) {
    switch(s) {
    case StatusPending:
        return "Pending pickup";
    case StatusInTransit:
        return "In Transit";
    case StatusOutForDelivery:
        return "Out for Delivery";
    case StatusDelivered:
        return "Delivered";
    case StatusException:
        return "Delivery Exception";
    }
    return "Unknown";
}

static void draw_status_icon(Canvas* canvas, int x, int y, PackageStatus s) {
    switch(s) {
    case StatusDelivered:
        canvas_draw_disc(canvas, x + 3, y + 3, 3);
        break;
    case StatusOutForDelivery:
        canvas_draw_circle(canvas, x + 3, y + 3, 3);
        canvas_draw_dot(canvas, x + 3, y + 3);
        break;
    case StatusInTransit:
        canvas_draw_circle(canvas, x + 3, y + 3, 3);
        break;
    case StatusPending:
        canvas_draw_line(canvas, x, y + 3, x + 6, y + 3);
        break;
    case StatusException:
        canvas_draw_line(canvas, x, y, x + 6, y + 6);
        canvas_draw_line(canvas, x + 6, y, x, y + 6);
        break;
    }
}

static void draw_empty(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 18, AlignCenter, AlignCenter, "No packages yet");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, "LEFT: add one, or set up");
    canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignCenter, "live tracking");
    canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignCenter, "BACK: exit");
}

static void draw_list(Canvas* canvas, TrackerState* state) {
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 9, state->msg[0] ? state->msg : "Pack Track");
    canvas_draw_line(canvas, 0, 11, 127, 11);

    if(package_count == 0) {
        draw_empty(canvas);
        return;
    }

    if(!state->refreshing) {
        char count[16];
        snprintf(count, sizeof(count), "%d/%d", state->selected + 1, package_count);
        canvas_draw_str_aligned(canvas, 126, 9, AlignRight, AlignBottom, count);
    }

    for(int i = 0; i < VISIBLE_ROWS && (i + state->scroll) < package_count; i++) {
        int idx = i + state->scroll;
        int y = 13 + i * ROW_HEIGHT;

        if(idx == state->selected) {
            canvas_draw_box(canvas, 0, y, 128, ROW_HEIGHT);
            canvas_invert_color(canvas);
        }

        draw_status_icon(canvas, 3, y + 3, packages[idx].status);
        canvas_draw_str(canvas, 13, y + 9, packages[idx].label);

        const char* st = status_short(packages[idx].status);
        canvas_draw_str_aligned(canvas, 125, y + 9, AlignRight, AlignBottom, st);

        if(idx == state->selected) {
            canvas_invert_color(canvas);
        }
    }

    // Room to spare means room to say how to add one.
    if(package_count < VISIBLE_ROWS) {
        canvas_draw_str(canvas, 13, 13 + package_count * ROW_HEIGHT + 9, "LEFT: menu");
    }
}

static void draw_detail(Canvas* canvas, TrackerState* state) {
    const Package* p = &packages[state->selected];

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, p->label);
    canvas_draw_line(canvas, 0, 12, 127, 12);

    canvas_set_font(canvas, FontSecondary);

    char line[48];
    snprintf(line, sizeof(line), "%s  %s", p->carrier, status_full(p->status));
    canvas_draw_str(canvas, 2, 22, line);

    canvas_draw_str(canvas, 2, 32, "Track:");
    char trunc[20];
    strncpy(trunc, p->tracking, sizeof(trunc) - 1);
    trunc[sizeof(trunc) - 1] = '\0';
    canvas_draw_str(canvas, 30, 32, trunc);

    canvas_draw_str(canvas, 2, 42, "Where:");
    canvas_draw_str(canvas, 30, 42, p->location);

    canvas_draw_str(canvas, 2, 52, "When:");
    canvas_draw_str(canvas, 30, 52, p->last_update);

    canvas_draw_line(canvas, 0, 54, 127, 54);
    canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "BACK: list  Hold OK: del");
}

static void draw_confirm(Canvas* canvas, TrackerState* state) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignCenter, "Delete package?");

    canvas_set_font(canvas, FontSecondary);
    if(package_count > 0) {
        canvas_draw_str_aligned(
            canvas, 64, 30, AlignCenter, AlignCenter, packages[state->selected].label);
    }
    canvas_draw_line(canvas, 0, 40, 127, 40);
    canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignCenter, "OK: delete");
    canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignCenter, "BACK: keep it");
}

static void render_callback(Canvas* canvas, void* ctx) {
    furi_assert(ctx);
    TrackerState* state = ctx;
    furi_mutex_acquire(state->mutex, FuriWaitForever);
    canvas_clear(canvas);

    if(state->screen == ScreenList) {
        draw_list(canvas, state);
    } else if(state->screen == ScreenConfirmDelete) {
        draw_confirm(canvas, state);
    } else {
        draw_detail(canvas, state);
    }

    furi_mutex_release(state->mutex);
}

static void input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* queue = ctx;
    TrackerEvent event = {.type = EventTypeInput, .input = *input_event};
    furi_message_queue_put(queue, &event, FuriWaitForever);
}

int32_t package_tracker_app(void* p) {
    UNUSED(p);

    load_all();

    TrackerState* state = malloc(sizeof(TrackerState));
    memset(state, 0, sizeof(*state));
    state->screen = ScreenList;
    state->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!state->mutex) {
        free(state);
        return 255;
    }

    FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(TrackerEvent));
    state->queue = queue;

    ViewPort* view_port = view_port_alloc();
    state->view_port = view_port;
    view_port_draw_callback_set(view_port, render_callback, state);
    view_port_input_callback_set(view_port, input_callback, queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    state->gui = gui;
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    // Fetch on open: the point of live tracking is not having to ask for it.
    if(config.has_url && package_count > 0) start_refresh(state);

    bool running = true;
    TrackerEvent event;

    while(running) {
        if(furi_message_queue_get(queue, &event, FuriWaitForever) != FuriStatusOk) continue;

        if(event.type == EventTypeRefreshDone) {
            if(state->worker) {
                furi_thread_join(state->worker);
                furi_thread_free(state->worker);
                state->worker = NULL;
            }
            state->refreshing = false;
            view_port_update(view_port);
            continue;
        }

        InputEvent* in = &event.input;
        if(in->type != InputTypeShort && in->type != InputTypeLong && in->type != InputTypeRepeat)
            continue;

        // Deferred so they run with the mutex released: the keyboard blocks,
        // and the render callback takes the same mutex.
        bool do_menu = false;
        bool do_delete = false;
        bool do_refresh = false;

        furi_mutex_acquire(state->mutex, FuriWaitForever);

        if(in->type == InputTypeLong && in->key == InputKeyBack) {
            if(state->refreshing)
                state->cancel = true;
            else
                running = false;
        } else if(state->refreshing) {
            if(in->key == InputKeyBack && in->type == InputTypeShort) state->cancel = true;
        } else if(state->screen == ScreenList) {
            if(package_count == 0) {
                if(in->key == InputKeyBack && in->type == InputTypeShort)
                    running = false;
                else if(in->key == InputKeyLeft && in->type == InputTypeShort)
                    do_menu = true;
                else if(in->key == InputKeyRight && in->type == InputTypeShort)
                    do_refresh = true;
            } else if(in->key == InputKeyLeft && in->type == InputTypeShort) {
                do_menu = true;
            } else if(in->key == InputKeyDown) {
                if(state->selected < package_count - 1) {
                    state->selected++;
                    if(state->selected >= state->scroll + VISIBLE_ROWS) state->scroll++;
                }
            } else if(in->key == InputKeyUp) {
                if(state->selected > 0) {
                    state->selected--;
                    if(state->selected < state->scroll) state->scroll--;
                }
            } else if(in->key == InputKeyRight && in->type == InputTypeShort) {
                do_refresh = true;
            } else if(in->key == InputKeyOk && in->type == InputTypeShort) {
                state->screen = ScreenDetail;
            } else if(in->key == InputKeyBack && in->type == InputTypeShort) {
                running = false;
            }
        } else if(state->screen == ScreenConfirmDelete) {
            if(in->key == InputKeyOk && in->type == InputTypeShort) {
                do_delete = true;
            } else if(in->key == InputKeyBack && in->type == InputTypeShort) {
                state->screen = ScreenDetail;
            }
        } else {
            if(in->key == InputKeyBack && in->type == InputTypeShort) {
                state->screen = ScreenList;
            } else if(in->key == InputKeyOk && in->type == InputTypeLong) {
                state->screen = ScreenConfirmDelete;
            } else if(in->key == InputKeyLeft && in->type == InputTypeShort) {
                if(state->selected > 0) state->selected--;
            } else if(in->key == InputKeyRight && in->type == InputTypeShort) {
                if(state->selected < package_count - 1) state->selected++;
            }
        }

        furi_mutex_release(state->mutex);

        if(do_menu) {
            static const char* const menu_items[] = {
                "Add package",
                "WiFi setup",
                "Refresh now",
                "Tracking setup",
            };
            view_port_enabled_set(view_port, false);
            int32_t choice = menu_pick(state->gui, "Pack Track", menu_items, 4);
            view_port_enabled_set(view_port, true);

            if(choice == 0)
                add_package_flow(state);
            else if(choice == 1)
                wifi_setup_flow(state);
            else if(choice == 2)
                start_refresh(state);
            else if(choice == 3)
                tracking_setup_flow(state);
        } else if(do_delete)
            delete_selected(state);
        else if(do_refresh)
            start_refresh(state);

        view_port_update(view_port);
    }

    if(state->worker) {
        state->cancel = true;
        furi_thread_join(state->worker);
        furi_thread_free(state->worker);
    }
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    furi_message_queue_free(queue);
    furi_mutex_free(state->mutex);
    free(state);
    return 0;
}
