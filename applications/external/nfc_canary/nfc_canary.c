/*
 * NFC Canary
 *
 * Passive 13.56 MHz reader detector. Alerts the wearer -- vibration first --
 * when a reader interrogates the device.
 *
 * IMPORTANT: a passive NFC tag/card will NOT trigger this. Tags have no power
 * source and only backscatter a reader's energy. Use something that EMITS a
 * carrier: an unlocked Android phone with NFC on, or a terminal. Present it to
 * the BACK of the Flipper, where the antenna is.
 */

#include "nfc_canary_i.h"
#include "alert.h"
#include "eventlog.h"
#include "settings.h"
#include "settings_menu.h"
#include "views.h"

#include <furi_hal_rtc.h>

typedef struct {
    AlerterState state;
    AlerterSettings settings;
    ViewState view;

    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* input_queue;
    NotificationApp* notifications;
    Alert* alert;
    FuriThread* worker;
    volatile bool running;
} NfcCanary;

/* ---------- helpers ---------- */

static uint32_t rtc_seconds(void) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    return (uint32_t)dt.hour * 3600 + (uint32_t)dt.minute * 60 + dt.second;
}

/* Record a tier into the history strip's current time slot, keeping the
 * highest tier seen in that slot. */
static void history_mark(AlerterState* s, ThreatTier tier) {
    uint32_t now = furi_get_tick();
    uint32_t elapsed = now - s->history_base_tick;
    uint32_t slot_ticks = furi_ms_to_ticks(HISTORY_SLOT_MS);

    uint32_t slots_passed = slot_ticks ? (elapsed / slot_ticks) : 0;
    if(slots_passed) {
        /* Scroll the strip left by however many slots elapsed. */
        if(slots_passed >= HISTORY_SLOTS) {
            memset(s->history, 0, sizeof(s->history));
        } else {
            memmove(s->history, s->history + slots_passed, HISTORY_SLOTS - slots_passed);
            memset(s->history + (HISTORY_SLOTS - slots_passed), 0, slots_passed);
        }
        s->history_base_tick += slots_passed * slot_ticks;
    }

    uint8_t* cur = &s->history[HISTORY_SLOTS - 1];
    if(tier > *cur) *cur = (uint8_t)tier;
}

/* ---------- detection worker ---------- */

static int32_t worker_thread(void* context) {
    NfcCanary* app = context;
    AlerterState* st = &app->state;

    if(furi_hal_nfc_acquire() != FuriHalNfcErrorNone) {
        /* Surface this instead of failing silently -- a dead radio and a
         * missing test source look identical from the main screen. */
        FURI_LOG_E(TAG, "failed to acquire NFC");
        furi_mutex_acquire(st->mutex, FuriWaitForever);
        st->nfc_ok = false;
        furi_mutex_release(st->mutex);
        view_port_update(app->view_port);
        return -1;
    }

    furi_mutex_acquire(st->mutex, FuriWaitForever);
    st->nfc_ok = true;
    furi_mutex_release(st->mutex);

    furi_hal_nfc_low_power_mode_stop();
    furi_hal_nfc_field_detect_start();

    bool raw_last = false;
    uint32_t raw_since = furi_get_tick();
    bool latched = false;
    uint32_t last_seen = 0;
    uint32_t start_tick = 0;
    ThreatTier tier = TierNone;

    while(app->running) {
        bool raw = furi_hal_nfc_field_is_present();
        uint32_t now = furi_get_tick();

        if(raw) last_seen = now;

        if(raw != raw_last) {
            raw_last = raw;
            raw_since = now;
            furi_mutex_acquire(st->mutex, FuriWaitForever);
            st->raw_field = raw;
            st->raw_edges++;
            furi_mutex_release(st->mutex);
            view_port_update(app->view_port);
        }

        if(!latched) {
            if(raw && (now - raw_since) >= furi_ms_to_ticks(FIELD_MIN_PULSE_MS)) {
                latched = true;
                start_tick = now;
                tier = TierInfo;

                furi_mutex_acquire(st->mutex, FuriWaitForever);
                st->field_present = true;
                st->current_tier = tier;
                st->field_start_tick = start_tick;
                st->detections++;
                history_mark(st, tier);
                furi_mutex_release(st->mutex);

                FURI_LOG_I(TAG, "field detected");
                view_port_update(app->view_port);
            }
        } else {
            /* Promote Info -> Warn once the carrier has persisted. A brief
             * carrier is a terminal being walked past; a sustained one is
             * pointed at us. */
            if(tier == TierInfo && (now - start_tick) >= furi_ms_to_ticks(TIER_WARN_MS)) {
                tier = TierWarn;
                furi_mutex_acquire(st->mutex, FuriWaitForever);
                st->current_tier = tier;
                history_mark(st, tier);
                furi_mutex_release(st->mutex);
                alert_start(app->alert, tier);
                view_port_update(app->view_port);
            }

            if(!raw && (now - last_seen) >= furi_ms_to_ticks(FIELD_HOLDOFF_MS)) {
                latched = false;

                uint32_t dur_ticks = last_seen - start_tick;
                uint32_t dur_ms = dur_ticks * 1000 / furi_kernel_get_tick_frequency();
                if(dur_ms > 0xFFFF) dur_ms = 0xFFFF;

                AlertEvent ev = {
                    .timestamp = rtc_seconds(),
                    .duration_ms = (uint16_t)dur_ms,
                    .tier = (uint8_t)tier,
                    .mode = app->settings.mode,
                    .protocol = 0xFF,
                    .cmd = 0,
                    .cmd_len = 0,
                    .reserved = 0,
                };

                furi_mutex_acquire(st->mutex, FuriWaitForever);
                st->field_present = false;
                st->current_tier = TierNone;
                furi_mutex_release(st->mutex);

                alert_stop(app->alert);
                eventlog_add(st, &ev);
                if(app->settings.log_enabled) eventlog_persist(&ev);

                FURI_LOG_I(TAG, "field lost after %lums", (unsigned long)dur_ms);
                tier = TierNone;
                view_port_update(app->view_port);
            }
        }

        furi_delay_ms(POLL_INTERVAL_MS);
    }

    /* Leave the radio quiet -- never exit with a field energized. */
    alert_stop(app->alert);
    furi_hal_nfc_field_detect_stop();
    furi_hal_nfc_low_power_mode_start();
    furi_hal_nfc_release();

    return 0;
}

/* ---------- gui plumbing ---------- */

static void draw_callback(Canvas* canvas, void* context) {
    NfcCanary* app = context;
    views_draw(canvas, &app->view, &app->state, &app->settings);
}

static void input_callback(InputEvent* event, void* context) {
    NfcCanary* app = context;
    furi_message_queue_put(app->input_queue, event, FuriWaitForever);
}

/* ---------- input handling ---------- */

/* Returns true to exit the app. */
static bool handle_input(NfcCanary* app, InputEvent* event) {
    ViewState* v = &app->view;

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    switch(v->screen) {
    case ScreenIntro:
        /* Any key dismisses; only shown once. */
        app->settings.seen_intro = true;
        settings_save(&app->settings);
        v->screen = ScreenStatus;
        break;

    case ScreenStatus:
        if(event->key == InputKeyBack) return true;
        if(event->key == InputKeyRight) v->screen = ScreenEvents;
        if(event->key == InputKeyLeft) v->screen = ScreenSession;
        if(event->key == InputKeyOk) {
            v->screen = ScreenSettings;
            v->settings_cursor = 0;
            v->settings_top = 0;
        }
        if(event->key == InputKeyUp) v->screen = ScreenDiag;
        break;

    case ScreenEvents: {
        uint16_t count = eventlog_count(&app->state);
        if(event->key == InputKeyBack) v->screen = ScreenStatus;
        if(event->key == InputKeyUp && v->event_cursor > 0) v->event_cursor--;
        if(event->key == InputKeyDown && v->event_cursor + 1 < count) v->event_cursor++;
        if(event->key == InputKeyOk && count > 0) v->screen = ScreenEventDetail;
        break;
    }

    case ScreenEventDetail:
        if(event->key == InputKeyBack || event->key == InputKeyOk) v->screen = ScreenEvents;
        break;

    case ScreenSession:
        if(event->key == InputKeyBack) v->screen = ScreenStatus;
        if(event->key == InputKeyRight) v->screen = ScreenStatus;
        if(event->key == InputKeyOk) {
            eventlog_clear(&app->state);
            v->event_cursor = 0;
        }
        break;

    case ScreenSettings: {
        SettingRow rows[16];
        uint8_t n = settings_menu_build(&app->settings, rows, 16);

        if(event->key == InputKeyBack) {
            settings_save(&app->settings);
            v->screen = ScreenStatus;
            break;
        }
        if(event->key == InputKeyUp && v->settings_cursor > 0) v->settings_cursor--;
        if(event->key == InputKeyDown && v->settings_cursor + 1 < n) v->settings_cursor++;

        if(v->settings_cursor < n) {
            SettingRow* r = &rows[v->settings_cursor];
            if(event->key == InputKeyRight && *r->field < r->max) (*r->field)++;
            if(event->key == InputKeyLeft && *r->field > 0) (*r->field)--;
        }
        if(event->key == InputKeyOk) {
            /* Audition the current alert config without needing a reader. */
            alert_test(app->alert);
        }
        break;
    }

    case ScreenDiag:
        if(event->key == InputKeyBack || event->key == InputKeyDown) v->screen = ScreenStatus;
        if(event->key == InputKeyOk) alert_test(app->alert);
        break;
    }

    return false;
}

/* ---------- lifecycle ---------- */

static NfcCanary* app_alloc(void) {
    NfcCanary* app = malloc(sizeof(NfcCanary));
    memset(app, 0, sizeof(NfcCanary));

    settings_load(&app->settings);

    app->state.mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->state.last_protocol = 0xFF;
    app->state.session_start = rtc_seconds();
    app->state.history_base_tick = furi_get_tick();
    app->running = true;

    app->view.screen = app->settings.seen_intro ? ScreenStatus : ScreenIntro;

    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->alert = alert_alloc(app->notifications, &app->settings);

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    return app;
}

static void app_free(NfcCanary* app) {
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    alert_free(app->alert);
    furi_record_close(RECORD_NOTIFICATION);
    furi_message_queue_free(app->input_queue);
    furi_mutex_free(app->state.mutex);
    free(app);
}

int32_t nfc_canary_app(void* p) {
    UNUSED(p);

    NfcCanary* app = app_alloc();

    app->worker = furi_thread_alloc_ex("NfcCanaryWorker", 2048, worker_thread, app);
    furi_thread_start(app->worker);

    InputEvent event;
    while(true) {
        if(furi_message_queue_get(app->input_queue, &event, FuriWaitForever) != FuriStatusOk) {
            continue;
        }
        if(handle_input(app, &event)) break;
        view_port_update(app->view_port);
    }

    app->running = false;
    furi_thread_join(app->worker);
    furi_thread_free(app->worker);

    settings_save(&app->settings);
    app_free(app);
    return 0;
}
