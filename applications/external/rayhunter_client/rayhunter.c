/**
 * rayhunter.c — Main application, scene management, and entry point.
 *
 * View hierarchy
 * ~~~~~~~~~~~~~~
 *   RhViewMain      View (custom)     — live threat status display
 *   RhViewSettings  VariableItemList  — host, port, poll interval
 *   RhViewAbout     Widget            — description and instructions
 *
 * Navigation
 * ~~~~~~~~~~
 *   Main → [OK or Left] → Settings → [Back] → Main
 *   Main → [Right]      → About    → [Back] → Main
 *   Main → [Back]                            → exit
 *
 * The poll timer fires every N seconds (per config) and sends
 * "rayhunter_poll\n" to the ESP32 via UART.  The worker parses responses
 * and updates the main view model; the GUI thread redraws on
 * RhEventStatusUpdate.  A RhEventHighThreat causes a vibration + red blink.
 */

#include "rayhunter.h"
#include "rayhunter_worker.h"

#include <stdio.h>
#include <string.h>

#define TAG "RhApp"

/* --------------------------------------------------------------------------
 * Forward declarations
 * -------------------------------------------------------------------------- */
static bool rh_navigation_cb(void* ctx);
static bool rh_custom_event_cb(void* ctx, uint32_t event);
static void rh_main_draw_cb(Canvas* canvas, void* model);
static bool rh_main_input_cb(InputEvent* event, void* ctx);
static void rh_poll_timer_cb(void* ctx);
static void rh_settings_setup(RhApp* app);
static void rh_about_setup(RhApp* app);

/* --------------------------------------------------------------------------
 * Navigation callback
 * -------------------------------------------------------------------------- */
static bool rh_navigation_cb(void* ctx) {
    RhApp* app = (RhApp*)ctx;
    if(app->current_view == RhViewMain) {
        /* Back from main view — exit the application. */
        view_dispatcher_stop(app->view_dispatcher);
        return false;
    }
    /* Other views navigate back to main. */
    app->current_view = RhViewMain;
    view_dispatcher_switch_to_view(app->view_dispatcher, RhViewMain);
    return true;
}

/* --------------------------------------------------------------------------
 * Custom event callback — called on the GUI thread
 * -------------------------------------------------------------------------- */
static bool rh_custom_event_cb(void* ctx, uint32_t event) {
    RhApp* app = (RhApp*)ctx;

    switch((RhCustomEvent)event) {
    case RhEventStatusUpdate:
        /* Model was already updated under lock by the worker thread.
     * Force a view redraw by touching the model with update=true. */
        with_view_model(app->main_view, RhMainModel * m, { (void)m; }, true);
        return true;

    case RhEventHighThreat:
        /* Notify the user: single vibration + sustained red blink. */
        notification_message(app->notifications, &sequence_single_vibro);
        notification_message(app->notifications, &sequence_blink_red_100);
        return true;
    }

    return false;
}

/* --------------------------------------------------------------------------
 * Threat level helpers
 * -------------------------------------------------------------------------- */
static const char* rh_threat_label(RhThreatLevel t) {
    switch(t) {
    case RhThreatHigh:
        return "!! HIGH THREAT !!";
    case RhThreatMedium:
        return "MEDIUM THREAT";
    case RhThreatLow:
        return "LOW THREAT";
    case RhThreatClean:
        return "CLEAN";
    default:
        return "Waiting...";
    }
}

/* --------------------------------------------------------------------------
 * Main view — draw callback
 *
 * Layout (128 x 64):
 *   y=0-10:  title bar  "Ray Hunter"  [connected indicator right-aligned]
 *   y=11:    separator line
 *   y=12-22: status row  "Status: CONNECTED" or "Waiting for ESP32..."
 *   y=25-38: threat box  (inverted when High/Medium)
 *   y=47:    last alert message (if any)
 *   y=55:    packet/warning counters
 *   y=63:    host:port footer
 * -------------------------------------------------------------------------- */
static void rh_main_draw_cb(Canvas* canvas, void* model) {
    const RhMainModel* mm = (const RhMainModel*)model;
    const RhStatus* s = &mm->status;
    const RhConfig* c = &mm->config;

    canvas_clear(canvas);

    /* ---- Title bar ---- */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Ray Hunter");

    /* Connection dot: filled = connected, outline = not. */
    if(s->connected) {
        canvas_draw_box(canvas, 120, 3, 6, 6);
    } else {
        canvas_draw_frame(canvas, 120, 3, 6, 6);
    }

    canvas_draw_line(canvas, 0, 12, 127, 12);

    canvas_set_font(canvas, FontSecondary);

    /* ---- Status row ---- */
    if(!s->connected) {
        canvas_draw_str(canvas, 2, 22, "Waiting for RH bridge...");
    } else {
        /* Show recording status if known, otherwise "Connected".
     * Buffer = "Status: " (8) + RH_STATUS_LEN (64) + NUL = 73 bytes. */
        char status_row[8 + RH_STATUS_LEN];
        if(s->status_text[0] != '\0') {
            snprintf(status_row, sizeof(status_row), "Status: %s", s->status_text);
        } else {
            snprintf(status_row, sizeof(status_row), "Status: Connected");
        }
        canvas_draw_str(canvas, 2, 22, status_row);
    }

    /* ---- Threat box (y=25 to y=38, 14 px tall) ---- */
    const bool is_high = (s->threat == RhThreatHigh);
    const bool is_medium = (s->threat == RhThreatMedium);
    const bool is_alert = (is_high || is_medium);

    if(is_alert) {
        /* Inverted block for High/Medium — grabs attention. */
        canvas_draw_box(canvas, 0, 25, 128, 14);
        canvas_set_color(canvas, ColorWhite);
    }

    canvas_set_font(canvas, FontPrimary);
    const char* label = rh_threat_label(s->threat);
    /* Center the label: FontPrimary chars are ~8px wide. */
    int16_t lx = (int16_t)((128 - (int16_t)(strlen(label) * 7)) / 2);
    if(lx < 2) lx = 2;
    canvas_draw_str(canvas, lx, 37, label);

    if(is_alert) {
        canvas_set_color(canvas, ColorBlack);
    }

    canvas_set_font(canvas, FontSecondary);

    /* ---- Alert message ---- */
    if(s->last_message[0] != '\0') {
        /* Truncate to fit 128px at ~5px/char = 25 chars. */
        char msg_buf[26];
        strncpy(msg_buf, s->last_message, sizeof(msg_buf) - 1);
        msg_buf[sizeof(msg_buf) - 1] = '\0';
        canvas_draw_str(canvas, 2, 47, msg_buf);
    } else if(s->threat == RhThreatClean) {
        canvas_draw_str(canvas, 2, 47, "No threats detected");
    } else if(s->threat == RhThreatNone && s->connected) {
        canvas_draw_str(canvas, 2, 47, "Polling...");
    }

    /* ---- Packet / warning counters ---- */
    char counters[32];
    snprintf(
        counters,
        sizeof(counters),
        "Pkts:%lu Warn:%lu",
        (unsigned long)s->packet_count,
        (unsigned long)s->warning_count);
    canvas_draw_str(canvas, 2, 55, counters);

    /* ---- Footer: host:port ---- */
    char footer[48];
    snprintf(footer, sizeof(footer), "%s:%u", c->host, (unsigned)c->port);
    int16_t fx = (int16_t)(128 - (int16_t)(strlen(footer) * 5));
    if(fx < 2) fx = 2;
    canvas_draw_str(canvas, fx, 63, footer);
}

/* --------------------------------------------------------------------------
 * Main view — input callback
 *
 * OK / Left    → Settings
 * Right        → About
 * Back         → navigation_cb handles exit
 * -------------------------------------------------------------------------- */
static bool rh_main_input_cb(InputEvent* event, void* ctx) {
    RhApp* app = (RhApp*)ctx;

    if(event->type != InputTypeShort) return false;

    if(event->key == InputKeyOk || event->key == InputKeyLeft) {
        app->current_view = RhViewSettings;
        view_dispatcher_switch_to_view(app->view_dispatcher, RhViewSettings);
        return true;
    }

    if(event->key == InputKeyRight) {
        app->current_view = RhViewAbout;
        view_dispatcher_switch_to_view(app->view_dispatcher, RhViewAbout);
        return true;
    }

    return false;
}

/* --------------------------------------------------------------------------
 * Poll timer callback — fires on the timer service thread.
 *
 * Sends "rayhunter_poll\n" to ESP32 and reads config safely.
 * -------------------------------------------------------------------------- */
static void rh_poll_timer_cb(void* ctx) {
    RhApp* app = (RhApp*)ctx;
    rh_worker_poll(app);
}

/* --------------------------------------------------------------------------
 * Settings — VariableItemList
 *
 * Items:
 *   0 — Host (cycling through preset IPs; user edits not supported on Flipper
 *              without a full TextInput — we offer common presets instead)
 *   1 — Port  (stepping through common values: 8080, 80, 8000, 8888)
 *   2 — Poll interval (2/5/10/30/60 s)
 *
 * All changes are applied immediately and the poll timer is restarted.
 * -------------------------------------------------------------------------- */

/* Preset host addresses for the settings list. */
#define RH_HOST_PRESETS_COUNT 4
static const char* const RH_HOST_PRESETS[RH_HOST_PRESETS_COUNT] = {
    "192.168.1.1",
    "192.168.0.1",
    "10.0.0.1",
    "172.16.0.1",
};

/* Preset port values. */
#define RH_PORT_PRESETS_COUNT 4
static const uint16_t RH_PORT_PRESETS[RH_PORT_PRESETS_COUNT] = {8080, 80, 8000, 8888};

typedef enum {
    RhSettingHost = 0,
    RhSettingPort,
    RhSettingInterval,
    RhSettingCount,
} RhSetting;

/* Find index of current host preset; default to 0 if not found. */
static uint8_t rh_host_preset_index(const char* host) {
    for(int i = 0; i < RH_HOST_PRESETS_COUNT; i++) {
        if(strcmp(host, RH_HOST_PRESETS[i]) == 0) return (uint8_t)i;
    }
    return 0;
}

static uint8_t rh_port_preset_index(uint16_t port) {
    for(int i = 0; i < RH_PORT_PRESETS_COUNT; i++) {
        if(RH_PORT_PRESETS[i] == port) return (uint8_t)i;
    }
    return 0;
}

static uint8_t rh_poll_interval_index(uint32_t secs) {
    for(int i = 0; i < RH_POLL_INTERVALS_COUNT; i++) {
        if(RH_POLL_INTERVALS[i] == secs) return (uint8_t)i;
    }
    return 1; /* default 5 s */
}

static void rh_settings_host_cb(VariableItem* item) {
    RhApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    if(idx >= RH_HOST_PRESETS_COUNT) idx = 0;

    furi_mutex_acquire(app->config_mutex, FuriWaitForever);
    strncpy(app->config.host, RH_HOST_PRESETS[idx], RH_HOST_LEN - 1);
    app->config.host[RH_HOST_LEN - 1] = '\0';
    furi_mutex_release(app->config_mutex);

    variable_item_set_current_value_text(item, RH_HOST_PRESETS[idx]);

    /* Sync config into the view model so the footer updates. */
    with_view_model(
        app->main_view,
        RhMainModel * m,
        { strncpy(m->config.host, app->config.host, RH_HOST_LEN - 1); },
        true);
}

static void rh_settings_port_cb(VariableItem* item) {
    RhApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    if(idx >= RH_PORT_PRESETS_COUNT) idx = 0;

    furi_mutex_acquire(app->config_mutex, FuriWaitForever);
    app->config.port = RH_PORT_PRESETS[idx];
    furi_mutex_release(app->config_mutex);

    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%u", (unsigned)RH_PORT_PRESETS[idx]);
    variable_item_set_current_value_text(item, port_str);

    with_view_model(app->main_view, RhMainModel * m, { m->config.port = app->config.port; }, true);
}

static void rh_settings_interval_cb(VariableItem* item) {
    RhApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    if(idx >= RH_POLL_INTERVALS_COUNT) idx = 1;

    furi_mutex_acquire(app->config_mutex, FuriWaitForever);
    app->config.poll_interval_s = RH_POLL_INTERVALS[idx];
    furi_mutex_release(app->config_mutex);

    char secs_str[8];
    snprintf(secs_str, sizeof(secs_str), "%lus", (unsigned long)RH_POLL_INTERVALS[idx]);
    variable_item_set_current_value_text(item, secs_str);

    /* Restart the timer with the new interval. */
    furi_timer_stop(app->poll_timer);
    furi_timer_start(app->poll_timer, furi_ms_to_ticks(RH_POLL_INTERVALS[idx] * 1000));
}

static void rh_settings_setup(RhApp* app) {
    variable_item_list_reset(app->settings_view);
    variable_item_list_set_header(app->settings_view, "Settings");

    /* --- Host --- */
    VariableItem* host_item = variable_item_list_add(
        app->settings_view, "Host", RH_HOST_PRESETS_COUNT, rh_settings_host_cb, app);
    uint8_t hi = rh_host_preset_index(app->config.host);
    variable_item_set_current_value_index(host_item, hi);
    variable_item_set_current_value_text(host_item, RH_HOST_PRESETS[hi]);

    /* --- Port --- */
    VariableItem* port_item = variable_item_list_add(
        app->settings_view, "Port", RH_PORT_PRESETS_COUNT, rh_settings_port_cb, app);
    uint8_t pi = rh_port_preset_index(app->config.port);
    variable_item_set_current_value_index(port_item, pi);
    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%u", (unsigned)RH_PORT_PRESETS[pi]);
    variable_item_set_current_value_text(port_item, port_str);

    /* --- Poll interval --- */
    VariableItem* ivl_item = variable_item_list_add(
        app->settings_view, "Poll every", RH_POLL_INTERVALS_COUNT, rh_settings_interval_cb, app);
    uint8_t ii = rh_poll_interval_index(app->config.poll_interval_s);
    variable_item_set_current_value_index(ivl_item, ii);
    char secs_str[8];
    snprintf(secs_str, sizeof(secs_str), "%lus", (unsigned long)RH_POLL_INTERVALS[ii]);
    variable_item_set_current_value_text(ivl_item, secs_str);
}

/* --------------------------------------------------------------------------
 * About widget
 * -------------------------------------------------------------------------- */
static void rh_about_setup(RhApp* app) {
    widget_reset(app->about_widget);

    widget_add_string_element(
        app->about_widget, 64, 0, AlignCenter, AlignTop, FontPrimary, "Ray Hunter Client");

    widget_add_text_scroll_element(
        app->about_widget,
        0,
        14,
        128,
        36,
        "EFF Ray Hunter detects IMSI catchers\n"
        "(fake cell towers) on a tethered\n"
        "Orbic RC400L hotspot.\n\n"
        "Requires an ESP32 WiFi Dev Board\n"
        "running the rayhunter_poll sketch\n"
        "connected to the Flipper GPIO.\n\n"
        "Source: github.com/EFForg/rayhunter\n"
        "Default: http://192.168.1.1:8080");

    /* Back navigation is handled by rh_navigation_cb — no button needed. */
}

/* --------------------------------------------------------------------------
 * App alloc
 * -------------------------------------------------------------------------- */
static RhApp* rh_app_alloc(void) {
    RhApp* app = malloc(sizeof(RhApp));
    furi_assert(app);
    memset(app, 0, sizeof(RhApp));

    /* ---- Default config ---- */
    strncpy(app->config.host, "192.168.1.1", RH_HOST_LEN - 1);
    app->config.port = 8080;
    app->config.poll_interval_s = 5;

    /* ---- Services ---- */
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    /* ---- Config mutex ---- */
    app->config_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    furi_assert(app->config_mutex);

    /* ---- UART layer ---- */
    app->uart = rh_uart_alloc();
    furi_assert(app->uart);

    /* ---- View dispatcher ---- */
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, rh_navigation_cb);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, rh_custom_event_cb);

    /* ---- Main view ---- */
    app->main_view = view_alloc();
    view_set_context(app->main_view, app);
    view_set_draw_callback(app->main_view, rh_main_draw_cb);
    view_set_input_callback(app->main_view, rh_main_input_cb);
    view_allocate_model(app->main_view, ViewModelTypeLocking, sizeof(RhMainModel));
    with_view_model(
        app->main_view,
        RhMainModel * m,
        {
            memset(m, 0, sizeof(RhMainModel));
            /* Embed config into model for lock-free read in draw callback. */
            m->config = app->config;
        },
        false);
    view_dispatcher_add_view(app->view_dispatcher, RhViewMain, app->main_view);

    /* ---- Settings view ---- */
    app->settings_view = variable_item_list_alloc();
    rh_settings_setup(app);
    view_dispatcher_add_view(
        app->view_dispatcher, RhViewSettings, variable_item_list_get_view(app->settings_view));

    /* ---- About widget ---- */
    app->about_widget = widget_alloc();
    rh_about_setup(app);
    view_dispatcher_add_view(
        app->view_dispatcher, RhViewAbout, widget_get_view(app->about_widget));

    /* ---- Worker (registers UART rx callback) ---- */
    rh_worker_start(app);

    /* ---- Poll timer (periodic, starts immediately) ---- */
    app->poll_timer = furi_timer_alloc(rh_poll_timer_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(app->poll_timer, furi_ms_to_ticks(app->config.poll_interval_s * 1000));

    return app;
}

/* --------------------------------------------------------------------------
 * App free
 * -------------------------------------------------------------------------- */
static void rh_app_free(RhApp* app) {
    furi_assert(app);

    /* Stop timer first so no polls fire during teardown. */
    furi_timer_stop(app->poll_timer);
    furi_timer_free(app->poll_timer);
    app->poll_timer = NULL;

    /* Stop worker (unregisters UART callback). */
    rh_worker_stop(app);

    /* Remove views before freeing underlying objects. */
    view_dispatcher_remove_view(app->view_dispatcher, RhViewMain);
    view_dispatcher_remove_view(app->view_dispatcher, RhViewSettings);
    view_dispatcher_remove_view(app->view_dispatcher, RhViewAbout);

    view_dispatcher_free(app->view_dispatcher);

    view_free(app->main_view);
    variable_item_list_free(app->settings_view);
    widget_free(app->about_widget);

    /* Free UART after removing all views (no callbacks can fire now). */
    rh_uart_free(app->uart);
    app->uart = NULL;

    furi_mutex_free(app->config_mutex);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

/* --------------------------------------------------------------------------
 * Entry point
 * -------------------------------------------------------------------------- */
int32_t rayhunter_client_app(void* p) {
    UNUSED(p);

    RhApp* app = rh_app_alloc();

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app->view_dispatcher, RhViewMain);

    /* Blocks until view_dispatcher_stop() is called (Back from main view). */
    view_dispatcher_run(app->view_dispatcher);

    rh_app_free(app);

    return 0;
}
