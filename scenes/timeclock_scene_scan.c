// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "../timeclock.h"

// =============================================================================
// Scan scene - reads a badge UID and routes to "known" or "new" badge.
//
// Reader is chosen in Settings (config.use_lf):
//   - NFC (13.56 MHz): ISO14443-3A poller (covers MIFARE Classic/Ultralight,
//     NTAG, DESFire and other ISO14443A cards - the vast majority of office
//     NFC badges).
//   - LF RFID (125 kHz): the lfrfid worker in auto mode (EM4100, HID, Indala,
//     ... - whatever the firmware's protocol dictionary supports).
//
// !!! FIRMWARE NOTE ------------------------------------------------------------
// The radio APIs are the part most likely to differ between firmware versions.
// The NFC ISO14443-3A path and the LF path below follow the official-firmware
// shape, but the exact symbol names (event types, get_uid / protocol_dict
// helpers) may need a small adjustment on your first `ufbt build`. Everything
// outside this file (storage, PIN, menus, history) does not depend on it.
// Extra NFC protocols (FeliCa, ISO15693/NFC-V, ISO14443-3B) can be added by
// allocating additional pollers here.
// -----------------------------------------------------------------------------

#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>

#include <lfrfid/lfrfid_worker.h>
#include <lfrfid/protocols/lfrfid_protocols.h>
#include <toolbox/protocols/protocol_dict.h>

typedef struct {
    TimeClock* app;
    // NFC
    Nfc* nfc;
    NfcPoller* poller;
    // LF RFID
    ProtocolDict* lf_dict;
    LFRFIDWorker* lf_worker;
    bool reported; // guard against multiple reports
} ScanCtx;

// Format up to 20 raw UID bytes as uppercase hex ("04A1B2C3D4"), store into the
// app, look the badge up and emit the matching custom event.
static void scan_report_uid(ScanCtx* ctx, const uint8_t* uid, size_t len, const char* tech) {
    if(ctx->reported) return;
    ctx->reported = true;

    TimeClock* app = ctx->app;
    if(len > 20) len = 20;

    char* out = app->scanned_uid;
    size_t pos = 0;
    for(size_t i = 0; i < len && pos + 2 < TC_UID_STR_MAX; i++) {
        static const char hexd[] = "0123456789ABCDEF";
        out[pos++] = hexd[(uid[i] >> 4) & 0xF];
        out[pos++] = hexd[uid[i] & 0xF];
    }
    out[pos] = '\0';

    strncpy(app->scanned_tech, tech, TC_TECH_MAX - 1);
    app->scanned_tech[TC_TECH_MAX - 1] = '\0';

    int idx = timeclock_find_badge(app, app->scanned_uid);
    app->found_index = idx;

    view_dispatcher_send_custom_event(
        app->view_dispatcher,
        idx >= 0 ? TimeClockCustomEventBadgeFound : TimeClockCustomEventBadgeUnknown);
}

// ---- NFC poller callback ---------------------------------------------------
static NfcCommand scan_nfc_poller_callback(NfcGenericEvent event, void* context) {
    ScanCtx* ctx = context;
    NfcCommand command = NfcCommandContinue;

    Iso14443_3aPollerEvent* iso_event = event.event_data;
    if(iso_event && iso_event->type == Iso14443_3aPollerEventTypeReady) {
        const Iso14443_3aData* data = nfc_poller_get_data(ctx->poller);
        size_t uid_len = 0;
        const uint8_t* uid = iso14443_3a_get_uid(data, &uid_len);
        scan_report_uid(ctx, uid, uid_len, "NFC");
        command = NfcCommandStop;
    }
    return command;
}

// ---- LF RFID read callback -------------------------------------------------
// The worker passes the matched protocol; read its data bytes as the UID.
static void scan_lf_read_callback(
    LFRFIDWorkerReadResult result,
    ProtocolId protocol,
    void* context) {
    ScanCtx* ctx = context;
    if(result != LFRFIDWorkerReadDone) return;

    size_t data_size = protocol_dict_get_data_size(ctx->lf_dict, protocol);
    if(data_size == 0 || data_size > 20) return;

    uint8_t buffer[20];
    protocol_dict_get_data(ctx->lf_dict, protocol, buffer, data_size);
    scan_report_uid(ctx, buffer, data_size, "RFID");
}

static void scan_start_nfc(ScanCtx* ctx) {
    ctx->nfc = nfc_alloc();
    ctx->poller = nfc_poller_alloc(ctx->nfc, NfcProtocolIso14443_3a);
    nfc_poller_start(ctx->poller, scan_nfc_poller_callback, ctx);
}

static void scan_stop_nfc(ScanCtx* ctx) {
    if(ctx->poller) {
        nfc_poller_stop(ctx->poller);
        nfc_poller_free(ctx->poller);
        ctx->poller = NULL;
    }
    if(ctx->nfc) {
        nfc_free(ctx->nfc);
        ctx->nfc = NULL;
    }
}

static void scan_start_lf(ScanCtx* ctx) {
    ctx->lf_dict = protocol_dict_alloc(lfrfid_protocols, LFRFIDProtocolMax);
    ctx->lf_worker = lfrfid_worker_alloc(ctx->lf_dict);
    lfrfid_worker_start_thread(ctx->lf_worker);
    lfrfid_worker_read_start(ctx->lf_worker, LFRFIDWorkerReadTypeAuto, scan_lf_read_callback, ctx);
}

static void scan_stop_lf(ScanCtx* ctx) {
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

void timeclock_scene_scan_on_enter(void* context) {
    TimeClock* app = context;

    ScanCtx* ctx = malloc(sizeof(ScanCtx));
    memset(ctx, 0, sizeof(ScanCtx));
    ctx->app = app;
    scene_manager_set_scene_state(app->scene_manager, TimeClockSceneScan, (uint32_t)(uintptr_t)ctx);

    bool replacing = app->replace_index >= 0;
    Popup* popup = app->popup;
    popup_reset(popup);
    popup_set_header(
        popup,
        replacing ? "New chip" : (app->config.use_lf ? "Reading RFID" : "Reading NFC"),
        64,
        10,
        AlignCenter,
        AlignTop);
    popup_set_text(
        popup,
        replacing ? "Tap the new chip\nfor this person" : "Hold the badge\nnear the Flipper",
        64,
        34,
        AlignCenter,
        AlignTop);
    view_dispatcher_switch_to_view(app->view_dispatcher, TimeClockViewPopup);

    if(app->config.use_lf) {
        scan_start_lf(ctx);
    } else {
        scan_start_nfc(ctx);
    }
}

bool timeclock_scene_scan_on_event(void* context, SceneManagerEvent event) {
    TimeClock* app = context;
    bool consumed = false;

    // Replace-chip mode: a scanned chip is reassigned to the selected
    // collaborator (keeping their name and history) instead of punching.
    if(event.type == SceneManagerEventTypeCustom && app->replace_index >= 0 &&
       (event.event == TimeClockCustomEventBadgeFound ||
        event.event == TimeClockCustomEventBadgeUnknown)) {
        int tgt = app->replace_index;
        if(app->found_index >= 0 && app->found_index != tgt) {
            // The chip already belongs to another collaborator: refuse.
            timeclock_notify_error(app);
        } else {
            Badge* b = &app->badges[tgt];
            strncpy(b->uid, app->scanned_uid, TC_UID_STR_MAX - 1);
            b->uid[TC_UID_STR_MAX - 1] = '\0';
            strncpy(b->tech, app->scanned_tech, TC_TECH_MAX - 1);
            b->tech[TC_TECH_MAX - 1] = '\0';
            tc_badges_save(app->badges, app->badge_count);
            timeclock_notify_success(app);
        }
        app->replace_index = -1;
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, TimeClockSceneBadgeDetail);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case TimeClockCustomEventBadgeFound:
            timeclock_notify_success(app);
            scene_manager_next_scene(app->scene_manager, TimeClockSceneBadgeAction);
            consumed = true;
            break;
        case TimeClockCustomEventBadgeUnknown:
            timeclock_notify_success(app);
            scene_manager_next_scene(app->scene_manager, TimeClockSceneNewBadge);
            consumed = true;
            break;
        case TimeClockCustomEventScanError:
            timeclock_notify_error(app);
            consumed = true;
            break;
        default:
            break;
        }
    }

    return consumed;
}

void timeclock_scene_scan_on_exit(void* context) {
    TimeClock* app = context;
    ScanCtx* ctx =
        (ScanCtx*)(uintptr_t)scene_manager_get_scene_state(app->scene_manager, TimeClockSceneScan);
    if(ctx) {
        scan_stop_nfc(ctx);
        scan_stop_lf(ctx);
        free(ctx);
        scene_manager_set_scene_state(app->scene_manager, TimeClockSceneScan, 0);
    }
    // Abort any pending chip replacement if the user left without scanning.
    app->replace_index = -1;
    popup_reset(app->popup);
}
