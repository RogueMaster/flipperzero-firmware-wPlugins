// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Daren Darrow
// https://github.com/darendarrow/flipnCGM

#include <furi.h>
#include <furi_hal_rtc.h>
#include <gui/gui.h>
#include <input/input.h>
#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso15693_3/iso15693_3.h>
#include <nfc/protocols/iso15693_3/iso15693_3_poller.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

// ── Constants ──────────────────────────────────────────────────────────────

#define FLIPNCGM_LOG_DIR      "/ext/apps_data/flipncgm"
#define FLIPNCGM_LOG_FILE     "/ext/apps_data/flipncgm/flipncgm.log"
#define FLIPNCGM_SETTINGS_FILE "/ext/apps_data/flipncgm/settings.ff"
#define FLIPNCGM_LOG_BUF      256

// UTC offset bounds: -12:00 (-720 min) to +14:00 (+840 min), 30-min steps.
#define TZ_OFFSET_MIN_MINUTES (-720)
#define TZ_OFFSET_MAX_MINUTES (840)
#define TZ_OFFSET_STEP_MINUTES (30)

// 32-char alphabet: B, I, O, S omitted to avoid visual ambiguity.
static const char SERIAL_LOOKUP[32] = "0123456789ACDEFGHJKLMNPQRTUVWXYZ";

// ── Notification sequences ─────────────────────────────────────────────────

// Brief two-note ascending chime on successful Libre read (~200 ms).
static const NotificationSequence sequence_cgm_success = {
    &message_note_c5, &message_delay_100,
    &message_note_e5, &message_delay_100,
    &message_sound_off,
    NULL,
};

// Single short low beep for non-Libre tag (~100 ms).
static const NotificationSequence sequence_cgm_error = {
    &message_note_c3, &message_delay_100,
    &message_sound_off,
    NULL,
};

// ── Log level ──────────────────────────────────────────────────────────────

typedef enum {
    AppLogOff = 0, // No logging
    AppLogError,   // Errors only
    AppLogInfo,    // Errors + CGM reads + lifecycle events
    AppLogDebug,   // Everything above + internal state
    AppLogCount,
} AppLogLevel;

static const char* const LOG_LEVEL_NAMES[AppLogCount] = {
    "Off", "Error", "Info", "Debug",
};

static const char* const LOG_LEVEL_TAGS[AppLogCount] = {
    "OFF", "ERR", "INF", "DBG",
};

// ── App state ──────────────────────────────────────────────────────────────

typedef enum {
    AppStateScanning,
    AppStateResult,
    AppStateNotALibre,
} AppState;

typedef struct {
    AppState    state;
    AppLogLevel log_level;
    int32_t     utc_offset_minutes; // UTC offset in minutes, e.g. -300 = UTC-05:00
    char        serial[10];   // 9-char serial + null terminator
    char        uid_str[32];  // "E0:7A:xx:xx:xx:xx:xx:xx\0"
    bool        scan_enabled; // debounce: cleared after read, re-armed on OK
    Gui*              gui;
    ViewPort*         view_port;
    FuriMessageQueue* event_queue;
    NotificationApp*  notifications;
    Nfc*        nfc;
    NfcPoller*  poller;
    Storage*    storage;
    FuriMutex*  mutex;
    FuriMutex*  log_mutex; // separate from UI mutex: file I/O must not block draws
} App;

// ── Serial decode ──────────────────────────────────────────────────────────

// Decode a FreeStyle Libre NFC UID to its 9-character ASCII serial.
// uid: 8 bytes MSB-first (uid[0]==0xE0, uid[1]==0x7A).
// Treats uid[2..7] as a 48-bit big-endian int, extracts 9×5-bit groups
// MSB-first, maps each through SERIAL_LOOKUP.
static void decode_libre_serial(const uint8_t* uid, char* out) {
    uint64_t value = 0;
    for(int i = 2; i <= 7; i++) value = (value << 8) | (uint64_t)uid[i];
    for(int i = 8; i >= 0; i--) out[8 - i] = SERIAL_LOOKUP[(value >> (i * 5)) & 0x1F];
    out[9] = '\0';
}

// ── Settings (UTC offset) ──────────────────────────────────────────────────

static void settings_load(App* app) {
    FlipperFormat* ff = flipper_format_file_alloc(app->storage);
    if(flipper_format_file_open_existing(ff, FLIPNCGM_SETTINGS_FILE)) {
        int32_t val = 0;
        if(flipper_format_read_int32(ff, "UTC offset minutes", &val, 1)) {
            // Clamp to valid range
            if(val < TZ_OFFSET_MIN_MINUTES) val = TZ_OFFSET_MIN_MINUTES;
            if(val > TZ_OFFSET_MAX_MINUTES) val = TZ_OFFSET_MAX_MINUTES;
            app->utc_offset_minutes = val;
        }
        flipper_format_file_close(ff);
    }
    flipper_format_free(ff);
}

static void settings_save(App* app) {
    FlipperFormat* ff = flipper_format_file_alloc(app->storage);
    if(flipper_format_file_open_always(ff, FLIPNCGM_SETTINGS_FILE)) {
        flipper_format_write_header_cstr(ff, "flipnCGM Settings", 1);
        flipper_format_write_int32(ff, "UTC offset minutes", &app->utc_offset_minutes, 1);
        flipper_format_file_close(ff);
    }
    flipper_format_free(ff);
}

// Format a UTC offset as "+HH:MM" or "-HH:MM" into buf (must be >= 16 bytes).
static void format_tz(char* buf, size_t size, int32_t offset_minutes) {
    char sign = offset_minutes >= 0 ? '+' : '-';
    int32_t abs_min = offset_minutes >= 0 ? offset_minutes : -offset_minutes;
    // Hours 0-14, minutes 0 or 30 — use int to satisfy -Wformat-truncation.
    int hh = (int)(abs_min / 60);
    int mm = (int)(abs_min % 60);
    snprintf(buf, size, "%c%02d:%02d", sign, hh, mm);
}

// ── Logging ────────────────────────────────────────────────────────────────

// Write a timestamped entry to the SD-card log file.
// level must be <= app->log_level (already checked by callers).
static void app_log_write(App* app, AppLogLevel level, const char* fmt, ...) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    char tz_str[16];
    format_tz(tz_str, sizeof(tz_str), app->utc_offset_minutes);

    char buf[FLIPNCGM_LOG_BUF];
    int hdr = snprintf(
        buf, sizeof(buf),
        "%04u-%02u-%02u %02u:%02u:%02u%s [%s] ",
        dt.year, dt.month, dt.day,
        dt.hour, dt.minute, dt.second,
        tz_str,
        LOG_LEVEL_TAGS[level]);

    if(hdr <= 0 || hdr >= (int)sizeof(buf) - 2) return;

    va_list args;
    va_start(args, fmt);
    int body = vsnprintf(buf + hdr, sizeof(buf) - (size_t)hdr - 2, fmt, args);
    va_end(args);

    if(body < 0) return;

    // Clamp body length to what actually fits, then append '\n'
    int end = hdr + (body < (int)(sizeof(buf) - (size_t)hdr - 2)
                         ? body
                         : (int)(sizeof(buf) - (size_t)hdr - 3));
    buf[end++] = '\n';
    buf[end]   = '\0';

    furi_mutex_acquire(app->log_mutex, FuriWaitForever);
    File* file = storage_file_alloc(app->storage);
    if(storage_file_open(file, FLIPNCGM_LOG_FILE, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        storage_file_write(file, buf, (uint16_t)end);
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_mutex_release(app->log_mutex);
}

// Convenience wrappers — silently skip when level is disabled.
#define LOG_ERROR(app, ...) \
    do { if((app)->log_level >= AppLogError) app_log_write((app), AppLogError, __VA_ARGS__); } while(0)
#define LOG_INFO(app, ...) \
    do { if((app)->log_level >= AppLogInfo)  app_log_write((app), AppLogInfo,  __VA_ARGS__); } while(0)
#define LOG_DEBUG(app, ...) \
    do { if((app)->log_level >= AppLogDebug) app_log_write((app), AppLogDebug, __VA_ARGS__); } while(0)

// ── GUI ────────────────────────────────────────────────────────────────────

static void draw_callback(Canvas* canvas, void* context) {
    App* app = (App*)context;
    if(furi_mutex_acquire(app->mutex, 25) != FuriStatusOk) return;

    canvas_clear(canvas);

    switch(app->state) {
    case AppStateScanning:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignCenter, "flipnCGM");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, "Hold a FreeStyle Libre");
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignCenter, "sensor to back of Flipper");
        // Status row: log level (left) and UTC offset (right)
        {
            char log_str[16];
            char tz_str[16];
            char utc_str[24];
            snprintf(log_str, sizeof(log_str), "Log: %s", LOG_LEVEL_NAMES[app->log_level]);
            format_tz(tz_str, sizeof(tz_str), app->utc_offset_minutes);
            snprintf(utc_str, sizeof(utc_str), "UTC%s", tz_str);
            canvas_draw_str_aligned(canvas, 0,   46, AlignLeft,  AlignCenter, log_str);
            canvas_draw_str_aligned(canvas, 127, 46, AlignRight, AlignCenter, utc_str);
        }
        // Button hints
        canvas_draw_str_aligned(canvas, 0,   57, AlignLeft,  AlignCenter, "^:log  <>:UTC");
        canvas_draw_str_aligned(canvas, 127, 57, AlignRight, AlignCenter, "Back:exit");
        break;

    case AppStateResult:
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 8,  AlignCenter, AlignCenter, "FreeStyle Libre Serial:");
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignCenter, app->serial);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignCenter, app->uid_str);
        canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignCenter, "OK: scan again  Back: exit");
        break;

    case AppStateNotALibre:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignCenter, "Not a Libre sensor");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "Tap a FreeStyle Libre CGM");
        canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignCenter, "OK: try again  Back: exit");
        break;
    }

    furi_mutex_release(app->mutex);
}

static void input_callback(InputEvent* input_event, void* context) {
    App* app = (App*)context;
    furi_message_queue_put(app->event_queue, input_event, FuriWaitForever);
}

// ── NFC poller ─────────────────────────────────────────────────────────────

static NfcCommand poller_callback(NfcGenericEvent event, void* context) {
    App* app = (App*)context;
    Iso15693_3PollerEvent* poller_event = (Iso15693_3PollerEvent*)event.event_data;

    if(poller_event->type == Iso15693_3PollerEventTypeReady) {
        // Debounce: only process one scan per OK press.
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        bool should_process = app->scan_enabled;
        if(should_process) app->scan_enabled = false;
        furi_mutex_release(app->mutex);

        LOG_DEBUG(app, "Tag detected (scan_enabled was %s)", should_process ? "true" : "false");

        if(should_process) {
            const Iso15693_3Data* iso_data =
                (const Iso15693_3Data*)nfc_poller_get_data(app->poller);
            const uint8_t* uid = iso_data->uid;

            furi_mutex_acquire(app->mutex, FuriWaitForever);
            if(uid[0] == 0xE0 && uid[1] == 0x7A) {
                decode_libre_serial(uid, app->serial);
                snprintf(
                    app->uid_str, sizeof(app->uid_str),
                    "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
                    uid[0], uid[1], uid[2], uid[3],
                    uid[4], uid[5], uid[6], uid[7]);
                app->state = AppStateResult;
                notification_message(app->notifications, &sequence_cgm_success);

                // Capture for logging outside the mutex
                char serial_copy[10];
                char uid_copy[32];
                memcpy(serial_copy, app->serial, sizeof(serial_copy));
                memcpy(uid_copy, app->uid_str, sizeof(uid_copy));
                furi_mutex_release(app->mutex);

                LOG_INFO(app, "CGM read OK - Serial: %s  UID: %s", serial_copy, uid_copy);
            } else {
                app->state = AppStateNotALibre;
                char uid_hex[32];
                snprintf(
                    uid_hex, sizeof(uid_hex),
                    "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
                    uid[0], uid[1], uid[2], uid[3],
                    uid[4], uid[5], uid[6], uid[7]);
                furi_mutex_release(app->mutex);

                notification_message(app->notifications, &sequence_cgm_error);
                LOG_INFO(app, "Non-Libre ISO15693 tag - UID: %s", uid_hex);
            }
            view_port_update(app->view_port);
        }

        return NfcCommandReset; // keep poller running; scan_enabled debounces it
    }

    if(poller_event->type == Iso15693_3PollerEventTypeError) {
        LOG_ERROR(app, "NFC poller error during activation");
    }

    return NfcCommandContinue;
}

// ── Entry point ────────────────────────────────────────────────────────────

int32_t flipncgm_app(void* p) {
    UNUSED(p);

    App* app = malloc(sizeof(App));
    furi_assert(app);
    memset(app, 0, sizeof(App));
    app->state        = AppStateScanning;
    app->log_level    = AppLogOff; // logging off by default; UP cycles it on
    app->scan_enabled = true;

    app->mutex     = furi_mutex_alloc(FuriMutexTypeNormal);
    app->log_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    // Storage — create log directory (no-op if it already exists).
    app->storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(app->storage, FLIPNCGM_LOG_DIR);
    settings_load(app); // restore persisted UTC offset

    app->nfc    = nfc_alloc();
    app->poller = nfc_poller_alloc(app->nfc, NfcProtocolIso15693_3);
    nfc_poller_start(app->poller, poller_callback, app);

    LOG_INFO(app, "flipnCGM started");

    InputEvent event;
    bool running = true;

    while(running) {
        if(furi_message_queue_get(app->event_queue, &event, 100) == FuriStatusOk) {
            // Short or long Back always exits.
            if(event.key == InputKeyBack &&
               (event.type == InputTypeShort || event.type == InputTypeLong)) {
                LOG_INFO(app, "flipnCGM exiting");
                running = false;

            } else if(event.key == InputKeyOk && event.type == InputTypeShort) {
                furi_mutex_acquire(app->mutex, FuriWaitForever);
                if(app->state != AppStateScanning) {
                    app->state        = AppStateScanning;
                    app->scan_enabled = true;
                    furi_mutex_release(app->mutex);
                    LOG_DEBUG(app, "Scan re-armed by OK press");
                } else {
                    furi_mutex_release(app->mutex);
                }
                view_port_update(app->view_port);

            } else if(event.key == InputKeyUp && event.type == InputTypeShort) {
                // Cycle log level: Off → Error → Info → Debug → Off → …
                furi_mutex_acquire(app->mutex, FuriWaitForever);
                AppLogLevel prev = app->log_level;
                app->log_level   = (AppLogLevel)((app->log_level + 1) % AppLogCount);
                AppLogLevel next = app->log_level;
                furi_mutex_release(app->mutex);

                // If turning off: write farewell while still enabled (prev level).
                // If turning on or changing: write at new level.
                if(prev != AppLogOff && next == AppLogOff) {
                    app_log_write(app, AppLogInfo,
                        "Logging disabled (was %s)", LOG_LEVEL_NAMES[prev]);
                } else if(next != AppLogOff) {
                    app_log_write(app, AppLogInfo,
                        "Log level: %s -> %s", LOG_LEVEL_NAMES[prev], LOG_LEVEL_NAMES[next]);
                }
                view_port_update(app->view_port);

            } else if((event.key == InputKeyLeft || event.key == InputKeyRight) &&
                      event.type == InputTypeShort) {
                // Adjust UTC offset in 30-minute steps.
                furi_mutex_acquire(app->mutex, FuriWaitForever);
                int32_t delta = (event.key == InputKeyRight)
                                    ? TZ_OFFSET_STEP_MINUTES
                                    : -TZ_OFFSET_STEP_MINUTES;
                app->utc_offset_minutes += delta;
                if(app->utc_offset_minutes < TZ_OFFSET_MIN_MINUTES)
                    app->utc_offset_minutes = TZ_OFFSET_MIN_MINUTES;
                if(app->utc_offset_minutes > TZ_OFFSET_MAX_MINUTES)
                    app->utc_offset_minutes = TZ_OFFSET_MAX_MINUTES;
                furi_mutex_release(app->mutex);

                settings_save(app); // persist immediately

                char tz_str[16];
                format_tz(tz_str, sizeof(tz_str), app->utc_offset_minutes);
                LOG_INFO(app, "UTC offset set to %s", tz_str);
                view_port_update(app->view_port);
            }
        }
    }

    // Poller is always running — stop exactly once.
    nfc_poller_stop(app->poller);
    nfc_poller_free(app->poller);
    nfc_free(app->nfc);

    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_NOTIFICATION);

    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(app->view_port);

    furi_message_queue_free(app->event_queue);
    furi_mutex_free(app->log_mutex);
    furi_mutex_free(app->mutex);
    free(app);

    return 0;
}
