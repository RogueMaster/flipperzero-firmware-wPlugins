#include <furi.h>
#include <furi_hal_power.h>
#include <furi_hal_rtc.h>
#include <datetime/datetime.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <storage/storage.h>
#include <math.h>

#include "i2c_worker.h"
#include "chip_db.h"
#include "i2c_notify.h"
#include "i2c_settings.h"

#define TAG "I2CChipId"

#define ANIM_PERIOD_MS 60 // ~16 fps, smooth enough and cheap

typedef enum {
    I2CChipIdViewMenu,
    I2CChipIdViewWiring,
    I2CChipIdViewScan,
    I2CChipIdViewDetail,
    I2CChipIdViewLive,
    I2CChipIdViewSettings,
    I2CChipIdViewAbout,
} I2CChipIdViewId;

typedef enum {
    MenuIndexWiring,
    MenuIndexScan,
    MenuIndexLiveTest,
    MenuIndexSettings,
    MenuIndexAbout,
} MenuIndex;

typedef struct {
    uint32_t frame;
    I2CBusCheck bus;
    bool sensor_seen; // pull-ups detected => something is wired up
} WiringViewModel;

typedef struct {
    bool scanning;
    uint32_t frame;
    uint8_t progress_addr;
    I2CFoundDevice found[I2C_SCAN_MAX_FOUND];
    uint8_t found_count;
    uint8_t selected;
    uint8_t scroll;
    I2CBusCheck bus; // captured before the sweep, drives the failure hints
    char status_msg[20];
} ScanViewModel;

typedef struct {
    I2CFoundDevice device;
} DetailViewModel;

typedef struct {
    I2CLiveData data;
    uint32_t frame;
} LiveViewModel;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;
    Submenu* submenu;
    View* wiring_view;
    View* scan_view;
    View* detail_view;
    View* live_view;
    VariableItemList* settings_list;
    Widget* about_widget;
    I2CWorker* worker;
    FuriThread* anim_thread;
    volatile bool anim_stop;
    I2CSettings settings;
    volatile I2CChipIdViewId current_view;
    uint8_t last_mag_cal; // to chime once when calibration reaches 3
} I2CChipIdApp;

static void app_switch_view(I2CChipIdApp* app, I2CChipIdViewId view_id) {
    app->current_view = view_id;
    view_dispatcher_switch_to_view(app->view_dispatcher, view_id);
}

/* ---------------- Wiring screen ---------------- */

// Compact plug glyph so the boxes read as hardware, not plain rectangles.
static void draw_connector(Canvas* canvas, uint8_t x, uint8_t y) {
    canvas_draw_box(canvas, x, y - 2, 3, 5);
    canvas_draw_line(canvas, x + 3, y, x + 5, y);
}

static void wiring_draw_callback(Canvas* canvas, void* model) {
    WiringViewModel* m = model;
    canvas_clear(canvas);

    canvas_draw_rframe(canvas, 0, 10, 38, 44, 2);
    canvas_draw_rframe(canvas, 92, 10, 36, 44, 2);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 3, 20, "FLIPPER");
    canvas_draw_str(canvas, 95, 20, "SENSOR");

    // Pin numbers verified against furi_hal_resources.c gpio_pins[]:
    // PC0 (SCL) is header pin 16, PC1 (SDA) is header pin 15.
    const char* labels[] = {"16 SCL", "15 SDA", "9  3V3", "8  GND"};
    for(uint8_t i = 0; i < 4; i++) {
        uint8_t y = 26 + i * 8;
        canvas_draw_line(canvas, 38, y, 92, y);
        draw_connector(canvas, 33, y);
        draw_connector(canvas, 87, y);
        canvas_draw_str(canvas, 44, y - 1, labels[i]);

        // A pulse travelling Flipper -> sensor shows the link is live; it
        // speeds up once pull-ups appear on the bus.
        uint8_t speed = m->sensor_seen ? 3 : 1;
        uint8_t span = 92 - 38;
        uint8_t px = 38 + (uint8_t)((m->frame * speed + i * 13) % span);
        canvas_draw_disc(canvas, px, y, 1);
    }

    canvas_set_font(canvas, FontSecondary);
    if(m->sensor_seen) {
        canvas_draw_box(canvas, 0, 55, 128, 9);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(
            canvas, 64, 63, AlignCenter, AlignBottom, "Sensor detected! OK = scan");
        canvas_set_color(canvas, ColorBlack);
    } else if(m->bus.health == I2CBusStuckLow) {
        canvas_draw_str_aligned(
            canvas, 64, 63, AlignCenter, AlignBottom, "Line stuck low - check short");
    } else {
        char buf[32];
        const char* dots = "   ";
        switch((m->frame / 8) % 4) {
        case 1:
            dots = ".  ";
            break;
        case 2:
            dots = ".. ";
            break;
        case 3:
            dots = "...";
            break;
        default:
            break;
        }
        snprintf(buf, sizeof(buf), "Waiting for sensor%s", dots);
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, buf);
    }

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignBottom, "3.3V ONLY - NOT 5V!");
}

/* ---------------- Scan screen ---------------- */

#define SCAN_LIST_ROWS 5

static void app_start_scan(I2CChipIdApp* app) {
    with_view_model(
        app->scan_view,
        ScanViewModel * m,
        {
            m->scanning = true;
            m->frame = 0;
            m->progress_addr = I2C_SCAN_ADDR_FIRST;
            m->found_count = 0;
            m->selected = 0;
            m->scroll = 0;
            m->bus = (I2CBusCheck){0};
            m->status_msg[0] = '\0';
        },
        true);
    app_switch_view(app, I2CChipIdViewScan);
    i2c_worker_start_scan(app->worker, i2c_settings_probe_timeout(&app->settings));
}

// Rotating radar sweep: a scan that visibly moves reads as alive even when
// every address NACKs.
static void draw_scan_spinner(Canvas* canvas, uint8_t cx, uint8_t cy, uint32_t frame) {
    canvas_draw_circle(canvas, cx, cy, 9);
    float a = (float)(frame % 32) / 32.0f * 2.0f * (float)M_PI;
    canvas_draw_line(
        canvas, cx, cy, cx + (int8_t)(sinf(a) * 8.0f), cy - (int8_t)(cosf(a) * 8.0f));
    canvas_draw_disc(canvas, cx, cy, 1);
}

static void scan_draw_callback(Canvas* canvas, void* model) {
    ScanViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    if(m->scanning) {
        canvas_draw_str(canvas, 2, 12, "Scanning bus...");
        draw_scan_spinner(canvas, 112, 20, m->frame);

        uint8_t span = I2C_SCAN_ADDR_LAST - I2C_SCAN_ADDR_FIRST;
        uint8_t done = m->progress_addr - I2C_SCAN_ADDR_FIRST;
        canvas_draw_frame(canvas, 4, 32, 92, 10);
        canvas_draw_box(canvas, 4, 32, (uint16_t)done * 92 / span, 10);

        canvas_set_font(canvas, FontSecondary);
        char buf[28];
        snprintf(buf, sizeof(buf), "addr 0x%02X   found: %u", m->progress_addr, m->found_count);
        canvas_draw_str(canvas, 4, 52, buf);
        return;
    }

    if(m->found_count == 0) {
        canvas_draw_str_aligned(canvas, 64, 11, AlignCenter, AlignBottom, "No devices found");
        canvas_set_font(canvas, FontSecondary);

        // Hints keyed to what the electrical check actually saw
        const char* l1;
        const char* l2;
        const char* l3;
        switch(m->bus.health) {
        case I2CBusStuckLow:
            l1 = m->bus.scl_stuck ?
                     (m->bus.sda_stuck ? "Both lines held LOW" : "SCL (pin 16) held LOW") :
                     "SDA (pin 15) held LOW";
            l2 = "Short to GND or hung chip.";
            l3 = "Unplug, re-seat, try again.";
            break;
        case I2CBusFloating:
            l1 = "No pull-ups on the bus.";
            l2 = "Sensor unpowered or not wired.";
            l3 = "Check 9=3V3, 8=GND, 16, 15.";
            break;
        default:
            l1 = "Bus looks electrically OK.";
            l2 = "Wrong address, or SCL/SDA";
            l3 = "swapped, or the chip is dead.";
            break;
        }
        canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignBottom, l1);
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignBottom, l2);
        canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignBottom, l3);
        canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "OK = rescan");
        return;
    }

    char buf[40];
    if(m->status_msg[0]) {
        snprintf(buf, sizeof(buf), "Found %u  [%s]", m->found_count, m->status_msg);
    } else {
        snprintf(buf, sizeof(buf), "Found %u device(s)", m->found_count);
    }
    canvas_draw_str(canvas, 2, 10, buf);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 126, 10, AlignRight, AlignBottom, "R=save");

    for(uint8_t row = 0; row < SCAN_LIST_ROWS; row++) {
        uint8_t idx = m->scroll + row;
        if(idx >= m->found_count) break;
        uint8_t y = 22 + row * 10;
        const I2CFoundDevice* dev = &m->found[idx];
        const char* name = dev->ident.chip ? dev->ident.chip->name : "UNKNOWN";
        snprintf(
            buf,
            sizeof(buf),
            "0x%02X %s %s",
            dev->addr,
            name,
            chip_verdict_short_str(dev->ident.verdict));
        if(idx == m->selected) {
            canvas_draw_box(canvas, 0, y - 8, 128, 10);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, 4, y, buf);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str(canvas, 4, y, buf);
        }
    }
}

/* Writes a snapshot of the scan results to /ext/apps_data/i2c_chipid/.
 * Takes a copy rather than the live model: SD writes can stall for seconds
 * and must never run while the view-model mutex is held. */
static bool scan_save_log(const I2CFoundDevice* found, uint8_t count) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool ok = false;

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    FuriString* path = furi_string_alloc_printf(
        APP_DATA_PATH("scan_%04u%02u%02u_%02u%02u%02u.txt"),
        dt.year,
        dt.month,
        dt.day,
        dt.hour,
        dt.minute,
        dt.second);
    storage_common_resolve_path_and_ensure_app_directory(storage, path);

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, furi_string_get_cstr(path), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FuriString* line = furi_string_alloc();
        furi_string_printf(
            line,
            "I2C Chip ID scan %04u-%02u-%02u %02u:%02u:%02u\n"
            "Bus: external, 100 kHz, pin 16 SCL / pin 15 SDA\n\n",
            dt.year,
            dt.month,
            dt.day,
            dt.hour,
            dt.minute,
            dt.second);
        for(uint8_t i = 0; i < count; i++) {
            const I2CFoundDevice* dev = &found[i];
            furi_string_cat_printf(
                line,
                "addr 0x%02X  %s  %s\n",
                dev->addr,
                dev->ident.chip ? dev->ident.chip->name : "UNKNOWN",
                chip_verdict_str(dev->ident.verdict));
            for(uint8_t r = 0; r < dev->ident.read_count; r++) {
                const IdReadResult* rr = &dev->ident.reads[r];
                uint8_t digits = rr->wide ? 4 : 2;
                if(!rr->read_ok) {
                    furi_string_cat_printf(line, "  reg 0x%02X read FAILED\n", rr->reg);
                } else if(rr->has_expected) {
                    furi_string_cat_printf(
                        line,
                        "  reg 0x%02X = 0x%0*X (exp 0x%0*X) %s\n",
                        rr->reg,
                        digits,
                        rr->actual,
                        digits,
                        rr->expected,
                        rr->match ? "OK" : "MISMATCH");
                } else {
                    furi_string_cat_printf(line, "  reg 0x%02X = 0x%02X\n", rr->reg, rr->actual);
                }
            }
            if(dev->ident.chip && dev->ident.chip->note) {
                furi_string_cat_printf(line, "  note: %s\n", dev->ident.chip->note);
            }
        }
        if(count == 0) furi_string_cat_str(line, "No devices found\n");
        size_t len = furi_string_size(line);
        ok = storage_file_write(file, furi_string_get_cstr(line), len) == len;
        furi_string_free(line);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_string_free(path);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

/* Snapshots the result list and writes it to SD outside the model lock.
 * The snapshot lives on the heap only for the duration of the write — as a
 * static buffer it would cost 800+ bytes of permanent RAM for something that
 * runs once per scan. */
static void app_save_log(I2CChipIdApp* app) {
    I2CFoundDevice* snapshot = malloc(sizeof(I2CFoundDevice) * I2C_SCAN_MAX_FOUND);
    uint8_t count = 0;

    with_view_model(
        app->scan_view,
        ScanViewModel * m,
        {
            count = m->found_count;
            if(count > I2C_SCAN_MAX_FOUND) count = I2C_SCAN_MAX_FOUND;
            memcpy(snapshot, m->found, count * sizeof(I2CFoundDevice));
        },
        false);

    bool saved = scan_save_log(snapshot, count);
    free(snapshot);

    with_view_model(
        app->scan_view,
        ScanViewModel * m,
        { snprintf(m->status_msg, sizeof(m->status_msg), "%s", saved ? "saved" : "SD error"); },
        true);
}

static I2CNotifyKind verdict_notify_kind(ChipVerdict verdict) {
    switch(verdict) {
    case VerdictGenuine:
        return I2CNotifyGenuine;
    case VerdictWrongChip:
    case VerdictNoAnswer:
        return I2CNotifyBad;
    case VerdictUnknown:
        return I2CNotifyAttention;
    default:
        return I2CNotifyNeutral;
    }
}

static bool scan_input_callback(InputEvent* event, void* context) {
    I2CChipIdApp* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    bool consumed = false;
    bool rescan = false;
    bool open_detail = false;
    bool do_save = false;

    with_view_model(
        app->scan_view,
        ScanViewModel * m,
        {
            if(!m->scanning) { // no navigation while scanning
                if(event->key == InputKeyOk && event->type == InputTypeShort) {
                    if(m->found_count == 0) {
                        rescan = true;
                    } else {
                        open_detail = true;
                    }
                    consumed = true;
                } else if(event->key == InputKeyUp && m->found_count > 0) {
                    if(m->selected > 0) m->selected--;
                    if(m->selected < m->scroll) m->scroll = m->selected;
                    consumed = true;
                } else if(event->key == InputKeyDown && m->found_count > 0) {
                    if(m->selected + 1 < m->found_count) m->selected++;
                    if(m->selected >= m->scroll + SCAN_LIST_ROWS)
                        m->scroll = m->selected - SCAN_LIST_ROWS + 1;
                    consumed = true;
                } else if(
                    event->key == InputKeyRight && m->found_count > 0 &&
                    event->type == InputTypeShort) {
                    do_save = true;
                    consumed = true;
                }
            }
        },
        consumed);

    if(do_save) {
        app_save_log(app);
        i2c_notify_play(app->notifications, I2CNotifyNeutral);
    }

    if(rescan) app_start_scan(app);

    if(open_detail) {
        I2CFoundDevice selected_dev;
        bool have = false;
        with_view_model(
            app->scan_view,
            ScanViewModel * m,
            {
                if(m->selected < m->found_count) {
                    selected_dev = m->found[m->selected];
                    have = true;
                }
            },
            false);
        if(have) {
            with_view_model(
                app->detail_view, DetailViewModel * dm, { dm->device = selected_dev; }, true);
            app_switch_view(app, I2CChipIdViewDetail);
            i2c_notify_play(app->notifications, verdict_notify_kind(selected_dev.ident.verdict));
        }
    }
    return consumed;
}

static void worker_event_callback(I2CWorkerEvent event, void* context) {
    I2CChipIdApp* app = context;

    if(event == I2CWorkerEventScanProgress || event == I2CWorkerEventScanDone) {
        bool done = (event == I2CWorkerEventScanDone);
        uint8_t count = 0;
        bool any_genuine = false, any_bad = false;
        I2CBusCheck bus;
        i2c_worker_get_bus(app->worker, &bus);

        with_view_model(
            app->scan_view,
            ScanViewModel * m,
            {
                m->progress_addr = i2c_worker_get_progress(app->worker);
                m->found_count = i2c_worker_get_found(app->worker, m->found, I2C_SCAN_MAX_FOUND);
                m->bus = bus;
                if(done) {
                    m->scanning = false;
                    m->selected = 0;
                    m->scroll = 0;
                    count = m->found_count;
                    for(uint8_t i = 0; i < count; i++) {
                        ChipVerdict v = m->found[i].ident.verdict;
                        if(v == VerdictGenuine) any_genuine = true;
                        if(v == VerdictWrongChip || v == VerdictNoAnswer) any_bad = true;
                    }
                }
            },
            true);

        if(done) {
            I2CNotifyKind kind;
            if(count == 0) {
                kind = I2CNotifyAttention; // nothing to check — the user must act
            } else if(any_bad) {
                kind = I2CNotifyBad;
            } else if(any_genuine) {
                kind = I2CNotifyGenuine;
            } else {
                kind = I2CNotifyNeutral;
            }
            i2c_notify_play(app->notifications, kind);

            if(app->settings.autosave && count > 0) app_save_log(app);
        }
    } else if(event == I2CWorkerEventLiveUpdate) {
        uint8_t cal = 0;
        bool running = false;
        with_view_model(
            app->live_view,
            LiveViewModel * m,
            {
                i2c_worker_get_live(app->worker, &m->data);
                cal = m->data.mag_cal;
                running = (m->data.status == I2CLiveStatusRunning);
            },
            true);
        // Chime once, on the transition to fully calibrated
        if(running && cal == 3 && app->last_mag_cal != 3) {
            i2c_notify_play(app->notifications, I2CNotifyCalibrated);
        }
        if(running) app->last_mag_cal = cal;
    } else if(event == I2CWorkerEventBusUpdate) {
        I2CBusCheck bus;
        i2c_worker_get_bus(app->worker, &bus);
        bool became_connected = false;
        with_view_model(
            app->wiring_view,
            WiringViewModel * m,
            {
                bool seen = (bus.health == I2CBusOk);
                if(seen && !m->sensor_seen) became_connected = true;
                m->bus = bus;
                m->sensor_seen = seen;
            },
            true);
        if(became_connected) i2c_notify_play(app->notifications, I2CNotifyNeutral);
    }
}

/* ---------------- Wiring enter/exit ---------------- */

static void wiring_enter_callback(void* context) {
    I2CChipIdApp* app = context;
    with_view_model(
        app->wiring_view,
        WiringViewModel * m,
        {
            m->frame = 0;
            m->sensor_seen = false;
            m->bus = (I2CBusCheck){0};
        },
        true);
    i2c_worker_watch_start(app->worker);
}

static void wiring_exit_callback(void* context) {
    I2CChipIdApp* app = context;
    i2c_worker_watch_stop(app->worker);
}

/* ---------------- Detail screen ---------------- */

static void detail_draw_callback(Canvas* canvas, void* model) {
    DetailViewModel* m = model;
    const I2CFoundDevice* dev = &m->device;
    canvas_clear(canvas);

    char buf[48];
    canvas_set_font(canvas, FontPrimary);
    snprintf(
        buf,
        sizeof(buf),
        "0x%02X  %s",
        dev->addr,
        dev->ident.chip ? dev->ident.chip->name : "UNKNOWN");
    canvas_draw_str(canvas, 2, 10, buf);

    canvas_set_font(canvas, FontSecondary);
    bool any_read_failed = false;
    uint8_t y = 20;
    for(uint8_t r = 0; r < dev->ident.read_count && r < CHIP_MAX_CHECKS; r++) {
        const IdReadResult* rr = &dev->ident.reads[r];
        uint8_t digits = rr->wide ? 4 : 2;
        if(!rr->read_ok) {
            any_read_failed = true;
            snprintf(buf, sizeof(buf), "0x%02X: read FAILED", rr->reg);
        } else if(rr->has_expected) {
            snprintf(
                buf,
                sizeof(buf),
                "0x%02X: %0*X exp %0*X %s",
                rr->reg,
                digits,
                rr->actual,
                digits,
                rr->expected,
                rr->match ? "OK" : "BAD");
        } else {
            snprintf(buf, sizeof(buf), "0x%02X: %02X", rr->reg, rr->actual);
        }
        canvas_draw_str(canvas, 2, y, buf);
        y += 9;
    }

    if(dev->ident.read_count == 0) {
        canvas_draw_str(canvas, 2, 20, "No ID register on this chip:");
        canvas_draw_str(canvas, 2, 29, "presence is all we can prove.");
        y = 38;
    }
    if(any_read_failed && y <= 45) {
        canvas_draw_str(canvas, 2, y, "ACK but no data: check pull-ups");
        y += 9;
    }
    if(dev->ident.chip && dev->ident.chip->note && y <= 51) {
        snprintf(buf, sizeof(buf), "! %s", dev->ident.chip->note);
        canvas_draw_str(canvas, 2, y, buf);
    }

    canvas_draw_box(canvas, 0, 54, 128, 10);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str_aligned(
        canvas, 64, 62, AlignCenter, AlignBottom, chip_verdict_str(dev->ident.verdict));
    canvas_set_color(canvas, ColorBlack);
}

// Navigation callbacks receive the *view's* context. For Widget and
// VariableItemList that is the module instance, not the app, so these must
// never dereference it — state changes belong in the exit callbacks of the
// views we own.
static uint32_t nav_to_scan(void* context) {
    UNUSED(context);
    return I2CChipIdViewScan;
}

/* ---------------- BNO055 live test ---------------- */

// Lemniscate traced by a moving dot: the figure-8 motion the magnetometer
// needs for calibration, shown instead of described.
static void draw_figure8(Canvas* canvas, uint8_t cx, uint8_t cy, uint32_t frame) {
    const float rx = 20.0f, ry = 8.0f;
    for(uint8_t i = 0; i < 32; i++) {
        float t = (float)i / 32.0f * 2.0f * (float)M_PI;
        canvas_draw_dot(canvas, cx + (int8_t)(rx * sinf(t)), cy + (int8_t)(ry * sinf(2 * t)));
    }
    float t = (float)(frame % 48) / 48.0f * 2.0f * (float)M_PI;
    canvas_draw_disc(
        canvas, cx + (int8_t)(rx * sinf(t)), cy + (int8_t)(ry * sinf(2 * t)), 2);
}

static void live_draw_callback(Canvas* canvas, void* model) {
    LiveViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    switch(m->data.status) {
    case I2CLiveStatusSearching:
        canvas_draw_str_aligned(canvas, 64, 12, AlignCenter, AlignBottom, "Looking for BNO055");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignBottom, "Probing 0x28 and 0x29");
        draw_scan_spinner(canvas, 64, 40, m->frame);
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "Plug it in - I'll wait");
        return;
    case I2CLiveStatusInit: {
        canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignBottom, "Starting NDOF mode");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignBottom, "Sensor fusion warm-up");
        uint8_t w = (uint8_t)((m->frame % 20) * 100 / 20);
        canvas_draw_frame(canvas, 14, 34, 100, 8);
        canvas_draw_box(canvas, 14, 34, w, 8);
        return;
    }
    case I2CLiveStatusLost:
        canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignBottom, "Sensor dropped off!");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, 64, 28, AlignCenter, AlignBottom, "It answered, then went quiet.");
        canvas_draw_str_aligned(
            canvas, 64, 38, AlignCenter, AlignBottom, "Check 3V3 and loose wires.");
        canvas_draw_str_aligned(canvas, 64, 52, AlignCenter, AlignBottom, "Retrying...");
        return;
    case I2CLiveStatusRunning:
        break;
    }

    // Heading arrives in 1/16 degree steps; integer math keeps printf light.
    int32_t raw = m->data.heading_raw;
    if(raw < 0) raw += 360 * 16;
    int32_t deg = raw / 16;
    int32_t frac = (raw % 16) * 10 / 16;

    char buf[24];
    canvas_set_font(canvas, FontBigNumbers);
    snprintf(buf, sizeof(buf), "%ld.%ld", (long)deg, (long)frac);
    canvas_draw_str(canvas, 2, 26, buf);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4, 36, "deg");

    // Compass: 0 deg = north = up, needle follows the sensor
    const uint8_t cx = 100, cy = 24, r = 18;
    canvas_draw_circle(canvas, cx, cy, r);
    canvas_draw_str_aligned(canvas, cx, cy - r + 6, AlignCenter, AlignBottom, "N");
    float a = (float)raw / 16.0f * ((float)M_PI / 180.0f);
    int8_t dx = (int8_t)(sinf(a) * (r - 4));
    int8_t dy = (int8_t)(-cosf(a) * (r - 4));
    canvas_draw_line(canvas, cx, cy, cx + dx, cy + dy);
    canvas_draw_disc(canvas, cx + dx, cy + dy, 2);

    snprintf(buf, sizeof(buf), "MAG CAL %u/3", m->data.mag_cal);
    canvas_draw_str(canvas, 2, 47, buf);
    for(uint8_t i = 0; i < 3; i++) {
        uint8_t bx = 58 + i * 9;
        if(i < m->data.mag_cal) {
            canvas_draw_box(canvas, bx, 40, 7, 7);
        } else {
            canvas_draw_frame(canvas, bx, 40, 7, 7);
        }
    }

    if(m->data.mag_cal < 3) {
        draw_figure8(canvas, 24, 57, m->frame);
        canvas_draw_str(canvas, 50, 60, "Rotate in figure-8");
    } else {
        canvas_draw_box(canvas, 0, 51, 128, 13);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(
            canvas, 64, 61, AlignCenter, AlignBottom, "CALIBRATED - spin a full turn");
        canvas_set_color(canvas, ColorBlack);
    }
}

static void live_enter_callback(void* context) {
    I2CChipIdApp* app = context;
    app->last_mag_cal = 0;
    with_view_model(
        app->live_view,
        LiveViewModel * m,
        {
            m->data = (I2CLiveData){.status = I2CLiveStatusSearching};
            m->frame = 0;
        },
        true);
    i2c_worker_live_start(app->worker);
}

static void live_exit_callback(void* context) {
    I2CChipIdApp* app = context;
    i2c_worker_live_stop(app->worker);
}

/* ---------------- Settings ---------------- */

static const char* const on_off_names[] = {"OFF", "ON"};

static void settings_apply(I2CChipIdApp* app) {
    i2c_notify_apply_settings(&app->settings);
    i2c_settings_save(&app->settings);
}

static void settings_sound_changed(VariableItem* item) {
    I2CChipIdApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, on_off_names[idx]);
    app->settings.sound = idx;
    settings_apply(app);
}

static void settings_vibro_changed(VariableItem* item) {
    I2CChipIdApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, on_off_names[idx]);
    app->settings.vibro = idx;
    settings_apply(app);
    // Buzz on enable so the setting demonstrates itself
    if(idx) i2c_notify_play(app->notifications, I2CNotifyNeutral);
}

static void settings_led_changed(VariableItem* item) {
    I2CChipIdApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, on_off_names[idx]);
    app->settings.led = idx;
    settings_apply(app);
    if(idx) i2c_notify_play(app->notifications, I2CNotifyNeutral);
}

static void settings_backlight_changed(VariableItem* item) {
    I2CChipIdApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, on_off_names[idx]);
    app->settings.backlight = idx;
    notification_message(
        app->notifications,
        idx ? &sequence_display_backlight_enforce_on :
              &sequence_display_backlight_enforce_auto);
    settings_apply(app);
}

static void settings_timeout_changed(VariableItem* item) {
    I2CChipIdApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, i2c_settings_timeout_names[idx]);
    app->settings.probe_timeout_idx = idx;
    settings_apply(app);
}

static void settings_autosave_changed(VariableItem* item) {
    I2CChipIdApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, on_off_names[idx]);
    app->settings.autosave = idx;
    settings_apply(app);
}

static void settings_build(I2CChipIdApp* app) {
    VariableItemList* list = app->settings_list;
    VariableItem* item;

    item = variable_item_list_add(list, "Sound", 2, settings_sound_changed, app);
    variable_item_set_current_value_index(item, app->settings.sound);
    variable_item_set_current_value_text(item, on_off_names[app->settings.sound]);

    item = variable_item_list_add(list, "Vibration", 2, settings_vibro_changed, app);
    variable_item_set_current_value_index(item, app->settings.vibro);
    variable_item_set_current_value_text(item, on_off_names[app->settings.vibro]);

    item = variable_item_list_add(list, "LED", 2, settings_led_changed, app);
    variable_item_set_current_value_index(item, app->settings.led);
    variable_item_set_current_value_text(item, on_off_names[app->settings.led]);

    item = variable_item_list_add(list, "Keep backlight", 2, settings_backlight_changed, app);
    variable_item_set_current_value_index(item, app->settings.backlight);
    variable_item_set_current_value_text(item, on_off_names[app->settings.backlight]);

    item = variable_item_list_add(
        list, "Probe speed", I2C_SETTINGS_TIMEOUT_COUNT, settings_timeout_changed, app);
    variable_item_set_current_value_index(item, app->settings.probe_timeout_idx);
    variable_item_set_current_value_text(
        item, i2c_settings_timeout_names[app->settings.probe_timeout_idx]);

    item = variable_item_list_add(list, "Auto-save log", 2, settings_autosave_changed, app);
    variable_item_set_current_value_index(item, app->settings.autosave);
    variable_item_set_current_value_text(item, on_off_names[app->settings.autosave]);
}

/* ---------------- Animation tick ---------------- */

// Animation runs on its own thread rather than a FuriTimer: a timer callback
// executes on the shared FreeRTOS timer daemon, and taking the view-model
// mutex plus queueing a GUI redraw from there can stall every timer in the
// firmware — including the ones the USB and storage services depend on.
static int32_t anim_thread_worker(void* context) {
    I2CChipIdApp* app = context;

    while(!app->anim_stop) {
        furi_delay_ms(ANIM_PERIOD_MS);
        if(app->anim_stop) break;

        switch(app->current_view) {
        case I2CChipIdViewWiring:
            with_view_model(app->wiring_view, WiringViewModel * m, { m->frame++; }, true);
            break;
        case I2CChipIdViewScan:
            with_view_model(app->scan_view, ScanViewModel * m, { m->frame++; }, true);
            break;
        case I2CChipIdViewLive:
            with_view_model(app->live_view, LiveViewModel * m, { m->frame++; }, true);
            break;
        default:
            break; // menus and static screens need no ticks
        }
    }
    return 0;
}

/* ---------------- Menu ---------------- */

static void menu_callback(void* context, uint32_t index) {
    I2CChipIdApp* app = context;
    switch(index) {
    case MenuIndexWiring:
        app_switch_view(app, I2CChipIdViewWiring);
        break;
    case MenuIndexScan:
        app_start_scan(app);
        break;
    case MenuIndexLiveTest:
        app_switch_view(app, I2CChipIdViewLive);
        break;
    case MenuIndexSettings:
        app_switch_view(app, I2CChipIdViewSettings);
        break;
    case MenuIndexAbout:
        app_switch_view(app, I2CChipIdViewAbout);
        break;
    }
}

static bool wiring_input_callback(InputEvent* event, void* context) {
    I2CChipIdApp* app = context;
    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        app_start_scan(app);
        return true;
    }
    return false;
}

static uint32_t nav_to_menu(void* context) {
    UNUSED(context);
    return I2CChipIdViewMenu;
}

static void scan_exit_callback(void* context) {
    I2CChipIdApp* app = context;
    app->current_view = I2CChipIdViewMenu;
    i2c_worker_abort_scan(app->worker); // never leave a sweep running behind us
}

static void detail_enter_callback(void* context) {
    I2CChipIdApp* app = context;
    app->current_view = I2CChipIdViewDetail;
}

static uint32_t nav_exit(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

/* ---------------- App lifecycle ---------------- */

static I2CChipIdApp* i2c_chipid_app_alloc(void) {
    I2CChipIdApp* app = malloc(sizeof(I2CChipIdApp));
    memset(app, 0, sizeof(I2CChipIdApp));

    i2c_settings_load(&app->settings);
    i2c_notify_apply_settings(&app->settings);

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    app->current_view = I2CChipIdViewMenu;

    if(app->settings.backlight) {
        notification_message(app->notifications, &sequence_display_backlight_enforce_on);
    }

    app->worker = i2c_worker_alloc();
    i2c_worker_set_callback(app->worker, worker_event_callback, app);

    app->submenu = submenu_alloc();
    submenu_set_header(app->submenu, "I2C Chip ID");
    submenu_add_item(app->submenu, "How to wire", MenuIndexWiring, menu_callback, app);
    submenu_add_item(app->submenu, "Scan bus", MenuIndexScan, menu_callback, app);
    submenu_add_item(app->submenu, "BNO055 live test", MenuIndexLiveTest, menu_callback, app);
    submenu_add_item(app->submenu, "Settings", MenuIndexSettings, menu_callback, app);
    submenu_add_item(app->submenu, "About", MenuIndexAbout, menu_callback, app);
    view_set_previous_callback(submenu_get_view(app->submenu), nav_exit);
    view_dispatcher_add_view(
        app->view_dispatcher, I2CChipIdViewMenu, submenu_get_view(app->submenu));

    app->wiring_view = view_alloc();
    view_set_context(app->wiring_view, app);
    view_allocate_model(app->wiring_view, ViewModelTypeLocking, sizeof(WiringViewModel));
    view_set_draw_callback(app->wiring_view, wiring_draw_callback);
    view_set_input_callback(app->wiring_view, wiring_input_callback);
    view_set_enter_callback(app->wiring_view, wiring_enter_callback);
    view_set_exit_callback(app->wiring_view, wiring_exit_callback);
    view_set_previous_callback(app->wiring_view, nav_to_menu);
    view_dispatcher_add_view(app->view_dispatcher, I2CChipIdViewWiring, app->wiring_view);

    app->scan_view = view_alloc();
    view_set_context(app->scan_view, app);
    view_allocate_model(app->scan_view, ViewModelTypeLocking, sizeof(ScanViewModel));
    view_set_draw_callback(app->scan_view, scan_draw_callback);
    view_set_input_callback(app->scan_view, scan_input_callback);
    view_set_exit_callback(app->scan_view, scan_exit_callback);
    view_set_previous_callback(app->scan_view, nav_to_menu);
    view_dispatcher_add_view(app->view_dispatcher, I2CChipIdViewScan, app->scan_view);

    app->detail_view = view_alloc();
    view_set_context(app->detail_view, app);
    view_allocate_model(app->detail_view, ViewModelTypeLocking, sizeof(DetailViewModel));
    view_set_draw_callback(app->detail_view, detail_draw_callback);
    view_set_enter_callback(app->detail_view, detail_enter_callback);
    view_set_previous_callback(app->detail_view, nav_to_scan);
    view_dispatcher_add_view(app->view_dispatcher, I2CChipIdViewDetail, app->detail_view);

    app->live_view = view_alloc();
    view_set_context(app->live_view, app);
    view_allocate_model(app->live_view, ViewModelTypeLocking, sizeof(LiveViewModel));
    view_set_draw_callback(app->live_view, live_draw_callback);
    view_set_enter_callback(app->live_view, live_enter_callback);
    view_set_exit_callback(app->live_view, live_exit_callback);
    view_set_previous_callback(app->live_view, nav_to_menu);
    view_dispatcher_add_view(app->view_dispatcher, I2CChipIdViewLive, app->live_view);

    app->settings_list = variable_item_list_alloc();
    settings_build(app);
    view_set_previous_callback(variable_item_list_get_view(app->settings_list), nav_to_menu);
    view_dispatcher_add_view(
        app->view_dispatcher,
        I2CChipIdViewSettings,
        variable_item_list_get_view(app->settings_list));

    app->about_widget = widget_alloc();
    widget_add_string_element(
        app->about_widget, 64, 6, AlignCenter, AlignTop, FontPrimary, "I2C Chip ID");
    widget_add_string_element(
        app->about_widget, 64, 20, AlignCenter, AlignTop, FontSecondary, "Spot fake I2C sensors");
    widget_add_string_element(
        app->about_widget, 64, 30, AlignCenter, AlignTop, FontSecondary, "by their ID registers.");
    {
        static char db_line[32];
        snprintf(db_line, sizeof(db_line), "%u chips known", (unsigned)chip_db_count());
        widget_add_string_element(
            app->about_widget, 64, 42, AlignCenter, AlignTop, FontSecondary, db_line);
    }
    widget_add_string_element(
        app->about_widget,
        64,
        52,
        AlignCenter,
        AlignTop,
        FontSecondary,
        "pin16 SCL/15 SDA - MIT");
    view_set_previous_callback(widget_get_view(app->about_widget), nav_to_menu);
    view_dispatcher_add_view(
        app->view_dispatcher, I2CChipIdViewAbout, widget_get_view(app->about_widget));

    app->anim_stop = false;
    app->anim_thread = furi_thread_alloc_ex("I2CChipIdAnim", 1024, anim_thread_worker, app);
    furi_thread_start(app->anim_thread);

    view_dispatcher_switch_to_view(app->view_dispatcher, I2CChipIdViewMenu);
    return app;
}

static void i2c_chipid_app_free(I2CChipIdApp* app) {
    // Animation thread first: it touches the view models
    app->anim_stop = true;
    furi_thread_join(app->anim_thread);
    furi_thread_free(app->anim_thread);
    // Then the worker: joining it guarantees no more callbacks into the views
    i2c_worker_free(app->worker);

    notification_message(app->notifications, &sequence_display_backlight_enforce_auto);

    view_dispatcher_remove_view(app->view_dispatcher, I2CChipIdViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, I2CChipIdViewWiring);
    view_dispatcher_remove_view(app->view_dispatcher, I2CChipIdViewScan);
    view_dispatcher_remove_view(app->view_dispatcher, I2CChipIdViewDetail);
    view_dispatcher_remove_view(app->view_dispatcher, I2CChipIdViewLive);
    view_dispatcher_remove_view(app->view_dispatcher, I2CChipIdViewSettings);
    view_dispatcher_remove_view(app->view_dispatcher, I2CChipIdViewAbout);
    submenu_free(app->submenu);
    view_free(app->wiring_view);
    view_free(app->scan_view);
    view_free(app->detail_view);
    view_free(app->live_view);
    variable_item_list_free(app->settings_list);
    widget_free(app->about_widget);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t i2c_chipid_app(void* p) {
    UNUSED(p);
    // The sensor is powered from pin 9: make sure the external 3.3V rail is on.
    // It is on by default after boot, so we leave it enabled on exit.
    furi_hal_power_enable_external_3_3v();

    I2CChipIdApp* app = i2c_chipid_app_alloc();
    view_dispatcher_run(app->view_dispatcher);
    i2c_chipid_app_free(app);
    return 0;
}
