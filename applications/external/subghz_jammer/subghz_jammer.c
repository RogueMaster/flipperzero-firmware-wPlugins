#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/modules/variable_item_list.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <stdio.h>
#include <string.h>

#include "subghz_jammer.h"
#include "subghz_jammer_worker.h"

#define TAG "SubGhzJammer"

/* ── View IDs ── */
typedef enum {
    JammerViewMain,
    JammerViewSettings,
} JammerViewId;

/* ── Threshold option tables ── */
static const float THRESH_SUSPICIOUS_VALUES[] = {-70.0f, -60.0f, -50.0f};
static const char* const THRESH_SUSPICIOUS_NAMES[] = {"-70 dBm", "-60 dBm", "-50 dBm"};
#define THRESH_SUSPICIOUS_COUNT   3
#define THRESH_SUSPICIOUS_DEFAULT 1 /* -60 dBm */

static const float THRESH_JAMMER_VALUES[] = {-50.0f, -40.0f, -30.0f};
static const char* const THRESH_JAMMER_NAMES[] = {"-50 dBm", "-40 dBm", "-30 dBm"};
#define THRESH_JAMMER_COUNT   3
#define THRESH_JAMMER_DEFAULT 1 /* -40 dBm */

static const char* const ALERT_MODE_NAMES[] = {"Silent", "Blink", "Vibrate"};
#define ALERT_MODE_COUNT   3
#define ALERT_MODE_DEFAULT AlertModeBlink

/* ── App context ── */
typedef struct {
    ViewDispatcher* view_dispatcher;
    View* main_view;
    VariableItemList* settings_list;
    Gui* gui;
    NotificationApp* notifications;

    /* Shared detection state */
    JammerState* state;
    FuriMutex* state_mutex;

    /* Worker */
    JammerWorker* worker;

    /* Settings indices (for VariableItemList sync) */
    uint8_t thresh_suspicious_idx;
    uint8_t thresh_jammer_idx;
    uint8_t alert_mode_idx;

    /* Timer for periodic view refresh */
    FuriTimer* refresh_timer;
} JammerApp;

/* ── RSSI bar rendering ── */

/* Map rssi in [-110, -20] to a bar width in [0, bar_max_w] pixels */
static uint8_t rssi_to_bar_width(float rssi, uint8_t bar_max_w) {
    const float RSSI_MIN = -110.0f;
    const float RSSI_MAX = -20.0f;
    float norm = (rssi - RSSI_MIN) / (RSSI_MAX - RSSI_MIN);
    if(norm < 0.0f) norm = 0.0f;
    if(norm > 1.0f) norm = 1.0f;
    return (uint8_t)(norm * (float)bar_max_w);
}

/* ── Main view draw callback ── */

static void jammer_main_draw_callback(Canvas* canvas, void* model) {
    JammerViewModel* vm = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    /* ── Title bar ── */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 9, "SubGHz Jammer Detect");
    canvas_draw_line(canvas, 0, 11, 127, 11);

    /* ── Frequency rows ──
   * Layout per row (row height = 10 px, starting at y=12):
   *   col 0..28  : frequency name (FontSecondary)
   *   col 30..85 : RSSI bar (55 px wide)
   *   col 88..109: RSSI value
   *   col 111..127: status label
   */
    canvas_set_font(canvas, FontSecondary);

    const uint8_t ROW_START_Y = 22;
    const uint8_t ROW_STEP = 11;
    const uint8_t BAR_X = 30;
    const uint8_t BAR_W = 55;
    const uint8_t BAR_H = 7;

    for(uint8_t i = 0; i < MONITOR_FREQ_COUNT; i++) {
        uint8_t row_y = ROW_START_Y + i * ROW_STEP;

        /* Frequency name */
        canvas_draw_str(canvas, 0, row_y, MONITOR_NAMES[i]);

        /* RSSI bar outline */
        canvas_draw_frame(canvas, BAR_X, row_y - BAR_H + 1, BAR_W, BAR_H);

        /* RSSI bar fill */
        uint8_t fill = rssi_to_bar_width(vm->window_max[i], BAR_W - 2);
        if(fill > 0) {
            canvas_draw_box(canvas, BAR_X + 1, row_y - BAR_H + 2, fill, BAR_H - 2);
        }

        /* RSSI value label (integer dBm) */
        char rssi_str[8];
        int32_t rssi_i = (int32_t)vm->rssi[i];
        int ret = snprintf(rssi_str, sizeof(rssi_str), "%ld", (long)rssi_i);
        if(ret < 0 || (size_t)ret >= sizeof(rssi_str)) rssi_str[sizeof(rssi_str) - 1] = '\0';
        canvas_draw_str(canvas, 87, row_y, rssi_str);

        /* Status label */
        const char* status_str;
        switch(vm->status[i]) {
        case FreqStatusJammer:
            status_str = "JAM!";
            break;
        case FreqStatusSuspicious:
            status_str = "WARN";
            break;
        default:
            status_str = "OK";
            break;
        }
        canvas_draw_str(canvas, 112, row_y, status_str);
    }

    /* ── Bottom status bar ── */
    canvas_draw_line(canvas, 0, 54, 127, 54);
    canvas_set_font(canvas, FontSecondary);

    if(vm->alert_freq_idx >= 0 && vm->alert_freq_idx < MONITOR_FREQ_COUNT) {
        uint8_t idx = (uint8_t)vm->alert_freq_idx;
        char alert_str[32];
        const char* severity = (vm->status[idx] == FreqStatusJammer) ? "JAMMER" : "SUSPICIOUS";
        int ret =
            snprintf(alert_str, sizeof(alert_str), "!! %s %s !!", severity, MONITOR_NAMES[idx]);
        if(ret < 0 || (size_t)ret >= sizeof(alert_str)) alert_str[sizeof(alert_str) - 1] = '\0';
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, alert_str);
    } else {
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "SCANNING...");
    }
}

/* ── Main view input callback ── */

static bool jammer_main_input_callback(InputEvent* event, void* context) {
    JammerApp* app = context;

    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        /* OK key: switch to settings */
        view_dispatcher_switch_to_view(app->view_dispatcher, JammerViewSettings);
        return true;
    }
    /* Back is handled by view_dispatcher navigation callback */
    return false;
}

/* ── Timer callback: copy state snapshot into view model ── */

static void jammer_refresh_timer_callback(void* context) {
    JammerApp* app = context;

    JammerViewModel snapshot;
    if(furi_mutex_acquire(app->state_mutex, 50) == FuriStatusOk) {
        JammerState* s = app->state;
        for(uint8_t i = 0; i < MONITOR_FREQ_COUNT; i++) {
            snapshot.rssi[i] = s->rssi[i];
            snapshot.window_max[i] = s->window_max[i];
            snapshot.status[i] = s->status[i];
        }
        snapshot.alert_freq_idx = s->alert_freq_idx;
        snapshot.threshold_suspicious = s->threshold_suspicious;
        snapshot.threshold_jammer = s->threshold_jammer;
        furi_mutex_release(app->state_mutex);
    } else {
        return;
    }

    with_view_model(
        app->main_view,
        JammerViewModel * vm,
        { memcpy(vm, &snapshot, sizeof(JammerViewModel)); },
        true);
}

/* ── Worker alert callback ── */

static void jammer_alert_callback(void* context) {
    /* Extra blink on confirmed alert — LED is already handled in the worker
   * for mode-specific sequences. This is a hook for future extensions. */
    UNUSED(context);
}

/* ── Settings callbacks ── */

static void jammer_thresh_suspicious_cb(VariableItem* item) {
    JammerApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->thresh_suspicious_idx = idx;
    variable_item_set_current_value_text(item, THRESH_SUSPICIOUS_NAMES[idx]);

    if(furi_mutex_acquire(app->state_mutex, FuriWaitForever) == FuriStatusOk) {
        app->state->threshold_suspicious = THRESH_SUSPICIOUS_VALUES[idx];
        furi_mutex_release(app->state_mutex);
    }
}

static void jammer_thresh_jammer_cb(VariableItem* item) {
    JammerApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->thresh_jammer_idx = idx;
    variable_item_set_current_value_text(item, THRESH_JAMMER_NAMES[idx]);

    if(furi_mutex_acquire(app->state_mutex, FuriWaitForever) == FuriStatusOk) {
        app->state->threshold_jammer = THRESH_JAMMER_VALUES[idx];
        furi_mutex_release(app->state_mutex);
    }
}

static void jammer_alert_mode_cb(VariableItem* item) {
    JammerApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->alert_mode_idx = idx;
    variable_item_set_current_value_text(item, ALERT_MODE_NAMES[idx]);

    if(furi_mutex_acquire(app->state_mutex, FuriWaitForever) == FuriStatusOk) {
        app->state->alert_mode = (AlertMode)idx;
        furi_mutex_release(app->state_mutex);
    }
}

/* ── Navigation callbacks ── */

static uint32_t jammer_nav_exit(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static uint32_t jammer_nav_to_main(void* context) {
    UNUSED(context);
    return JammerViewMain;
}

/* ── App lifecycle ── */

static JammerApp* jammer_app_alloc(void) {
    JammerApp* app = malloc(sizeof(JammerApp));
    memset(app, 0, sizeof(JammerApp));

    /* Shared detection state — heap-allocated, never on the FAP stack */
    app->state = malloc(sizeof(JammerState));
    memset(app->state, 0, sizeof(JammerState));

    /* Initialise RSSI windows to a floor value so the bar starts at zero */
    for(uint8_t i = 0; i < MONITOR_FREQ_COUNT; i++) {
        app->state->rssi[i] = -110.0f;
        app->state->window_max[i] = -110.0f;
        for(uint8_t j = 0; j < RSSI_WINDOW_SIZE; j++) {
            app->state->rssi_window[i][j] = -110.0f;
        }
        app->state->status[i] = FreqStatusOk;
        app->state->consecutive[i] = 0;
    }
    app->state->alert_freq_idx = -1;
    app->state->threshold_suspicious = THRESH_SUSPICIOUS_VALUES[THRESH_SUSPICIOUS_DEFAULT];
    app->state->threshold_jammer = THRESH_JAMMER_VALUES[THRESH_JAMMER_DEFAULT];
    app->state->alert_mode = ALERT_MODE_DEFAULT;

    app->state_mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    /* System services */
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    /* View dispatcher */
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    /* Main monitoring view */
    app->main_view = view_alloc();
    view_allocate_model(app->main_view, ViewModelTypeLocking, sizeof(JammerViewModel));
    view_set_draw_callback(app->main_view, jammer_main_draw_callback);
    view_set_input_callback(app->main_view, jammer_main_input_callback);
    view_set_context(app->main_view, app);
    view_set_previous_callback(app->main_view, jammer_nav_exit);
    view_dispatcher_add_view(app->view_dispatcher, JammerViewMain, app->main_view);

    /* Populate the main view model with initial zeros */
    with_view_model(
        app->main_view,
        JammerViewModel * vm,
        {
            for(uint8_t i = 0; i < MONITOR_FREQ_COUNT; i++) {
                vm->rssi[i] = -110.0f;
                vm->window_max[i] = -110.0f;
                vm->status[i] = FreqStatusOk;
            }
            vm->alert_freq_idx = -1;
            vm->threshold_suspicious = app->state->threshold_suspicious;
            vm->threshold_jammer = app->state->threshold_jammer;
        },
        false);

    /* Settings view */
    app->settings_list = variable_item_list_alloc();

    app->thresh_suspicious_idx = THRESH_SUSPICIOUS_DEFAULT;
    app->thresh_jammer_idx = THRESH_JAMMER_DEFAULT;
    app->alert_mode_idx = (uint8_t)ALERT_MODE_DEFAULT;

    VariableItem* item;
    item = variable_item_list_add(
        app->settings_list,
        "Suspicious RSSI",
        THRESH_SUSPICIOUS_COUNT,
        jammer_thresh_suspicious_cb,
        app);
    variable_item_set_current_value_index(item, app->thresh_suspicious_idx);
    variable_item_set_current_value_text(
        item, THRESH_SUSPICIOUS_NAMES[app->thresh_suspicious_idx]);

    item = variable_item_list_add(
        app->settings_list, "Jammer RSSI", THRESH_JAMMER_COUNT, jammer_thresh_jammer_cb, app);
    variable_item_set_current_value_index(item, app->thresh_jammer_idx);
    variable_item_set_current_value_text(item, THRESH_JAMMER_NAMES[app->thresh_jammer_idx]);

    item = variable_item_list_add(
        app->settings_list, "Alert Mode", ALERT_MODE_COUNT, jammer_alert_mode_cb, app);
    variable_item_set_current_value_index(item, app->alert_mode_idx);
    variable_item_set_current_value_text(item, ALERT_MODE_NAMES[app->alert_mode_idx]);

    view_set_previous_callback(
        variable_item_list_get_view(app->settings_list), jammer_nav_to_main);
    view_dispatcher_add_view(
        app->view_dispatcher, JammerViewSettings, variable_item_list_get_view(app->settings_list));

    /* Periodic refresh timer */
    app->refresh_timer =
        furi_timer_alloc(jammer_refresh_timer_callback, FuriTimerTypePeriodic, app);

    /* Worker */
    app->worker = jammer_worker_alloc(app->state, app->state_mutex);
    jammer_worker_set_callback(app->worker, jammer_alert_callback, app);

    return app;
}

static void jammer_app_free(JammerApp* app) {
    /* Stop timer and worker first */
    furi_timer_stop(app->refresh_timer);
    furi_timer_free(app->refresh_timer);

    if(jammer_worker_is_running(app->worker)) {
        jammer_worker_stop(app->worker);
    }
    jammer_worker_free(app->worker);

    /* Views */
    view_dispatcher_remove_view(app->view_dispatcher, JammerViewMain);
    view_dispatcher_remove_view(app->view_dispatcher, JammerViewSettings);
    view_free(app->main_view);
    variable_item_list_free(app->settings_list);
    view_dispatcher_free(app->view_dispatcher);

    /* State */
    furi_mutex_free(app->state_mutex);
    free(app->state);

    /* System services */
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

/* ── Entry point ── */

int32_t subghz_jammer_app(void* p) {
    UNUSED(p);

    JammerApp* app = jammer_app_alloc();

    /* Start scanning and refresh timer before entering the event loop */
    jammer_worker_start(app->worker);
    furi_timer_start(app->refresh_timer, UI_REFRESH_MS);

    view_dispatcher_switch_to_view(app->view_dispatcher, JammerViewMain);
    view_dispatcher_run(app->view_dispatcher);

    jammer_app_free(app);
    return 0;
}
