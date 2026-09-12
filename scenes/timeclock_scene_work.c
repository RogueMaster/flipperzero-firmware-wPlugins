// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "../timeclock.h"

// =============================================================================
// Work mode - a locked kiosk screen with a live clock. A registered
// collaborator taps their badge and the app auto-logs IN/OUT (alternating),
// greeting them "Welcome" / "Goodbye" with sound/vibro/LED feedback. Back is
// captured and leaving Work mode requires the PIN. Every punch is appended to
// punches.csv immediately (nothing lost).
//
// Reading is done by the shared TimeclockReader in continuous mode (full NFC
// protocol coverage via NfcScanner, or LF RFID).
//
// Requires: at least one registered collaborator AND a configured PIN.
// =============================================================================

#define WORK_GREETING_MS 3000
#define WORK_COOLDOWN_MS 3500
#define WORK_BOUNCE 260

typedef struct {
    TimeClock* app;
    TimeclockReader* reader;
    uint32_t last_punch_tick;
    uint32_t greeting_until;
    char last_uid[TC_UID_STR_MAX];
} WorkCtx;

// Reader callback (GUI thread): debounce, then log the punch and greet.
static void work_on_uid(const char* uid_hex, const char* tech, void* context) {
    UNUSED(tech);
    WorkCtx* ctx = context;
    TimeClock* app = ctx->app;

    uint32_t now = furi_get_tick();
    if(strcmp(uid_hex, ctx->last_uid) == 0 && (now - ctx->last_punch_tick) < WORK_COOLDOWN_MS) {
        return; // same badge within the cooldown window: ignore
    }
    ctx->last_punch_tick = now;
    strncpy(ctx->last_uid, uid_hex, sizeof(ctx->last_uid) - 1);
    ctx->last_uid[sizeof(ctx->last_uid) - 1] = '\0';

    int idx = timeclock_find_badge(app, uid_hex);
    char msg[40];
    if(idx >= 0) {
        TcEventType type = timeclock_record_punch(app, idx);
        snprintf(
            msg,
            sizeof(msg),
            "%s, %s",
            (type == TcEventIn) ? "Welcome" : "Goodbye",
            app->badges[idx].name);
    } else {
        snprintf(msg, sizeof(msg), "Not registered");
        timeclock_notify_error(app);
    }
    work_view_set_greeting(app->work_view, msg);
    ctx->greeting_until = now + WORK_GREETING_MS;
}

static void work_view_exit_cb(void* context) {
    TimeClock* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, TimeClockCustomEventWorkExit);
}

static void work_bounce_popup_cb(void* context) {
    TimeClock* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, WORK_BOUNCE);
}

static void work_update_clock(TimeClock* app) {
    char date[TC_DT_MAX];
    char time[8];
    tc_now_date(date, sizeof(date));
    tc_now_time(time, sizeof(time));
    work_view_set_clock(app->work_view, date, time);
}

void timeclock_scene_work_on_enter(void* context) {
    TimeClock* app = context;

    WorkCtx* ctx =
        (WorkCtx*)(uintptr_t)scene_manager_get_scene_state(app->scene_manager, TimeClockSceneWork);

    if(!ctx) {
        // Fresh entry: validate prerequisites first.
        if(app->badge_count == 0 || !app->config.pin_enabled) {
            Popup* popup = app->popup;
            popup_reset(popup);
            popup_set_header(popup, "Work mode", 64, 8, AlignCenter, AlignTop);
            popup_set_text(
                popup,
                app->badge_count == 0 ? "Register a\ncollaborator first" :
                                        "Set a PIN first\n(Settings)",
                64,
                30,
                AlignCenter,
                AlignTop);
            popup_set_callback(popup, work_bounce_popup_cb);
            popup_set_context(popup, app);
            popup_set_timeout(popup, 2500);
            popup_enable_timeout(popup);
            view_dispatcher_switch_to_view(app->view_dispatcher, TimeClockViewPopup);
            return;
        }

        ctx = malloc(sizeof(WorkCtx));
        memset(ctx, 0, sizeof(WorkCtx));
        ctx->app = app;
        ctx->reader = timeclock_reader_alloc(app->view_dispatcher);
        timeclock_reader_set_callback(ctx->reader, work_on_uid, ctx);
        scene_manager_set_scene_state(
            app->scene_manager, TimeClockSceneWork, (uint32_t)(uintptr_t)ctx);
    }

    work_view_set_exit_callback(app->work_view, work_view_exit_cb, app);
    work_view_set_greeting(app->work_view, NULL);
    work_update_clock(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, TimeClockViewWork);

    timeclock_reader_start(ctx->reader, app->config.use_lf, true);
}

bool timeclock_scene_work_on_event(void* context, SceneManagerEvent event) {
    TimeClock* app = context;
    WorkCtx* ctx =
        (WorkCtx*)(uintptr_t)scene_manager_get_scene_state(app->scene_manager, TimeClockSceneWork);
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        consumed = true;
        work_update_clock(app);
        if(ctx && ctx->greeting_until != 0 && furi_get_tick() > ctx->greeting_until) {
            ctx->greeting_until = 0;
            work_view_set_greeting(app->work_view, NULL);
        }
    } else if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == TimeClockCustomEventWorkExit) {
            // Stop the radio while the PIN is entered, then ask for the PIN.
            if(ctx) timeclock_reader_stop(ctx->reader);
            app->pin_mode = TcPinModeVerifyExitWork;
            scene_manager_next_scene(app->scene_manager, TimeClockScenePinSet);
            consumed = true;
        } else if(event.event == WORK_BOUNCE) {
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        } else if(ctx) {
            consumed = timeclock_reader_handle_event(ctx->reader, event.event);
        }
    }

    return consumed;
}

void timeclock_scene_work_on_exit(void* context) {
    TimeClock* app = context;
    WorkCtx* ctx =
        (WorkCtx*)(uintptr_t)scene_manager_get_scene_state(app->scene_manager, TimeClockSceneWork);
    if(ctx) {
        timeclock_reader_free(ctx->reader);
        free(ctx);
        scene_manager_set_scene_state(app->scene_manager, TimeClockSceneWork, 0);
    }
    work_view_set_greeting(app->work_view, NULL);
    popup_reset(app->popup);
}
