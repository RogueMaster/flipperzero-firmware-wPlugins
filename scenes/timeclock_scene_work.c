// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "../timeclock.h"

// =============================================================================
// Work mode - a locked kiosk screen showing a live clock and an animated
// mascot. A registered collaborator taps their badge and the app auto-logs
// IN/OUT (alternating), greeting them "Welcome" / "Goodbye". Back is captured
// and leaving Work mode requires the PIN, so collaborators cannot tamper with
// the app. Every punch is appended to punches.csv immediately (nothing lost).
//
// Requires: at least one registered collaborator AND a configured PIN.
// =============================================================================

#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>

#include <lfrfid/lfrfid_worker.h>
#include <lfrfid/protocols/lfrfid_protocols.h>
#include <toolbox/protocols/protocol_dict.h>

#define WORK_GREETING_MS 3000
#define WORK_COOLDOWN_MS 3500
#define WORK_BOUNCE 260

typedef struct {
    TimeClock* app;
    // NFC
    Nfc* nfc;
    NfcPoller* poller;
    // LF RFID
    ProtocolDict* lf_dict;
    LFRFIDWorker* lf_worker;
    // Debounce / greeting timing
    uint32_t last_punch_tick;
    uint32_t greeting_until;
    char last_uid[TC_UID_STR_MAX];
} WorkCtx;

// ---- Shared UID reporting (with debounce) ----------------------------------
static void work_report_uid(WorkCtx* ctx, const uint8_t* uid, size_t len, const char* tech) {
    if(len > 20) len = 20;

    char buf[TC_UID_STR_MAX];
    size_t pos = 0;
    for(size_t i = 0; i < len && pos + 2 < TC_UID_STR_MAX; i++) {
        static const char hexd[] = "0123456789ABCDEF";
        buf[pos++] = hexd[(uid[i] >> 4) & 0xF];
        buf[pos++] = hexd[uid[i] & 0xF];
    }
    buf[pos] = '\0';

    uint32_t now = furi_get_tick();
    if(strcmp(buf, ctx->last_uid) == 0 && (now - ctx->last_punch_tick) < WORK_COOLDOWN_MS) {
        return; // same badge within the cooldown window: ignore
    }
    ctx->last_punch_tick = now;
    strncpy(ctx->last_uid, buf, sizeof(ctx->last_uid) - 1);
    ctx->last_uid[sizeof(ctx->last_uid) - 1] = '\0';

    TimeClock* app = ctx->app;
    strncpy(app->scanned_uid, buf, TC_UID_STR_MAX - 1);
    app->scanned_uid[TC_UID_STR_MAX - 1] = '\0';
    strncpy(app->scanned_tech, tech, TC_TECH_MAX - 1);
    app->scanned_tech[TC_TECH_MAX - 1] = '\0';

    view_dispatcher_send_custom_event(app->view_dispatcher, TimeClockCustomEventBadgeFound);
}

// ---- NFC ----
static NfcCommand work_nfc_poller_callback(NfcGenericEvent event, void* context) {
    WorkCtx* ctx = context;
    Iso14443_3aPollerEvent* iso_event = event.event_data;
    if(iso_event && iso_event->type == Iso14443_3aPollerEventTypeReady) {
        const Iso14443_3aData* data = nfc_poller_get_data(ctx->poller);
        size_t uid_len = 0;
        const uint8_t* uid = iso14443_3a_get_uid(data, &uid_len);
        work_report_uid(ctx, uid, uid_len, "NFC");
    }
    return NfcCommandContinue; // keep scanning
}

// ---- LF ----
static void work_lf_read_callback(LFRFIDWorkerReadResult result, ProtocolId protocol, void* context) {
    WorkCtx* ctx = context;
    if(result != LFRFIDWorkerReadDone) return;
    size_t data_size = protocol_dict_get_data_size(ctx->lf_dict, protocol);
    if(data_size == 0 || data_size > 20) return;
    uint8_t buffer[20];
    protocol_dict_get_data(ctx->lf_dict, protocol, buffer, data_size);
    work_report_uid(ctx, buffer, data_size, "RFID");
}

static void work_start_scanner(WorkCtx* ctx) {
    if(ctx->app->config.use_lf) {
        ctx->lf_dict = protocol_dict_alloc(lfrfid_protocols, LFRFIDProtocolMax);
        ctx->lf_worker = lfrfid_worker_alloc(ctx->lf_dict);
        lfrfid_worker_start_thread(ctx->lf_worker);
        lfrfid_worker_read_start(
            ctx->lf_worker, LFRFIDWorkerReadTypeAuto, work_lf_read_callback, ctx);
    } else {
        ctx->nfc = nfc_alloc();
        ctx->poller = nfc_poller_alloc(ctx->nfc, NfcProtocolIso14443_3a);
        nfc_poller_start(ctx->poller, work_nfc_poller_callback, ctx);
    }
}

static void work_stop_scanner(WorkCtx* ctx) {
    if(ctx->poller) {
        nfc_poller_stop(ctx->poller);
        nfc_poller_free(ctx->poller);
        ctx->poller = NULL;
    }
    if(ctx->nfc) {
        nfc_free(ctx->nfc);
        ctx->nfc = NULL;
    }
    if(ctx->lf_worker) {
        lfrfid_worker_stop(ctx->lf_worker);
        lfrfid_worker_stop_thread(ctx->lf_worker);
        lfrfid_worker_free(ctx->lf_worker);
        ctx->lf_worker = NULL;
    }
    if(ctx->lf_dict) {
        protocol_dict_free(ctx->lf_dict);
        ctx->lf_dict = NULL;
    }
}

// ---- View callbacks --------------------------------------------------------
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

// ---- Scene -----------------------------------------------------------------
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
        scene_manager_set_scene_state(
            app->scene_manager, TimeClockSceneWork, (uint32_t)(uintptr_t)ctx);
    }

    work_view_set_exit_callback(app->work_view, work_view_exit_cb, app);
    work_view_set_greeting(app->work_view, NULL);
    work_update_clock(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, TimeClockViewWork);

    work_start_scanner(ctx);
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
        switch(event.event) {
        case TimeClockCustomEventBadgeFound: {
            consumed = true;
            int idx = timeclock_find_badge(app, app->scanned_uid);
            char msg[40];
            if(idx >= 0) {
                Badge* b = &app->badges[idx];
                TcEventType type = (b->last_event == TcEventIn) ? TcEventOut : TcEventIn;

                char date[TC_DT_MAX], time[8], dt[TC_DT_MAX];
                tc_now_date(date, sizeof(date));
                tc_now_time(time, sizeof(time));
                tc_now_datetime(dt, sizeof(dt));
                tc_history_append(date, time, b->name, b->uid, type);
                b->last_event = type;
                strncpy(b->last_used, dt, sizeof(b->last_used) - 1);
                b->last_used[sizeof(b->last_used) - 1] = '\0';
                tc_badges_save(app->badges, app->badge_count);

                snprintf(
                    msg,
                    sizeof(msg),
                    "%s, %s",
                    (type == TcEventIn) ? "Welcome" : "Goodbye",
                    b->name);
                timeclock_notify_punch(app, type);
            } else {
                snprintf(msg, sizeof(msg), "Unknown badge");
                timeclock_notify_error(app);
            }
            work_view_set_greeting(app->work_view, msg);
            if(ctx) ctx->greeting_until = furi_get_tick() + WORK_GREETING_MS;
            break;
        }
        case TimeClockCustomEventWorkExit:
            consumed = true;
            // Stop the radio while the PIN is entered, then ask for the PIN.
            if(ctx) work_stop_scanner(ctx);
            app->pin_mode = TcPinModeVerifyExitWork;
            scene_manager_next_scene(app->scene_manager, TimeClockScenePinSet);
            break;
        case WORK_BOUNCE:
            consumed = true;
            scene_manager_previous_scene(app->scene_manager);
            break;
        default:
            break;
        }
    }

    return consumed;
}

void timeclock_scene_work_on_exit(void* context) {
    TimeClock* app = context;
    WorkCtx* ctx =
        (WorkCtx*)(uintptr_t)scene_manager_get_scene_state(app->scene_manager, TimeClockSceneWork);
    if(ctx) {
        work_stop_scanner(ctx);
        free(ctx);
        scene_manager_set_scene_state(app->scene_manager, TimeClockSceneWork, 0);
    }
    work_view_set_greeting(app->work_view, NULL);
    popup_reset(app->popup);
}
