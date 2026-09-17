// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "../timeclock.h"

// =============================================================================
// Scan scene - reads a badge UID (single shot) via the shared reader. What
// happens next depends on app->scan_purpose:
//   Punch    : known chip -> record automatic IN/OUT; unknown -> "Not
//              registered". Never registers here.
//   Register : unknown chip -> go to name input (no punch); known -> "Already
//              registered".
//   Replace  : reassign the scanned chip to the selected collaborator.
// After a result the scene shows a short popup, then returns to the right place.
// The stack stays shallow (menu/badges -> scan -> back), so Back always works.
//
// Like Work mode, the reader is locked to one technology at a time (no
// automatic rotation - see timeclock_reader.h) with Left/Right to switch
// NFC/RFID/iButton. The choice is shared with Work mode (config.work_tech):
// switching it here or there remembers it everywhere. Left/Right only ever
// changes technology; tapping a badge itself needs no button press.
// =============================================================================

#define SCAN_POPUP_DONE 205u

typedef struct {
    TimeclockReader* reader;
    TimeclockReaderTech tech;
    uint32_t result_scene; // scene to return to after the result popup
    bool handled; // first UID consumed (guards against a double punch)
} ScanCtx;

static char scan_msg[64];

static void timeclock_scene_scan_popup_callback(void* context) {
    TimeClock* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, SCAN_POPUP_DONE);
}

// The "waiting for a tap" screen: header/hint depend on scan_purpose, plus
// the active technology and Left/Right to change it. Rebuilt on entry and
// again every time the technology changes.
static void timeclock_scene_scan_show_reading(TimeClock* app, ScanCtx* ctx) {
    const char* header;
    const char* text;
    if(app->scan_purpose == TcScanReplace) {
        header = tc_str(StrNewChip);
        text = tc_str(StrTapNewChip);
    } else if(app->scan_purpose == TcScanRegister) {
        header = tc_str(StrNewBadge);
        text = tc_str(StrTapRegister);
    } else {
        header = tc_str(StrReadingBadge);
        text = tc_str(StrHoldBadge);
    }
    scan_view_set_content(app->scan_view, header, text, timeclock_reader_tech_label(ctx->tech));
    view_dispatcher_switch_to_view(app->view_dispatcher, TimeClockViewScan);
}

// Left/Right (GUI thread, via the view's input callback): switch technology.
static void timeclock_scene_scan_nav_cb(int direction, void* context) {
    TimeClock* app = context;
    ScanCtx* ctx =
        (ScanCtx*)(uintptr_t)scene_manager_get_scene_state(app->scene_manager, TimeClockSceneScan);
    // Ignore once a UID has already been consumed (result screen is about to
    // take over, or already has).
    if(!ctx || ctx->handled) return;

    int next =
        ((int)ctx->tech + direction + TimeclockReaderTechCount) % TimeclockReaderTechCount;
    ctx->tech = (TimeclockReaderTech)next;
    timeclock_reader_start_fixed(ctx->reader, ctx->tech, false);
    timeclock_scene_scan_show_reading(app, ctx);

    // Remember the choice - shared with Work mode - across sessions.
    app->config.work_tech = (uint32_t)ctx->tech;
    tc_config_save(&app->config);
}

static void timeclock_scene_scan_result(
    TimeClock* app,
    ScanCtx* ctx,
    const char* header,
    const char* text,
    uint32_t back_scene) {
    ctx->result_scene = back_scene;
    Popup* popup = app->popup;
    popup_reset(popup);
    popup_set_header(popup, header, 64, 8, AlignCenter, AlignTop);
    popup_set_text(popup, text, 64, 30, AlignCenter, AlignTop);
    popup_set_callback(popup, timeclock_scene_scan_popup_callback);
    popup_set_context(popup, app);
    popup_set_timeout(popup, 1800);
    popup_enable_timeout(popup);
    view_dispatcher_switch_to_view(app->view_dispatcher, TimeClockViewPopup);
}

// Reader callback (GUI thread): a UID was read.
static void timeclock_scene_scan_on_uid(const char* uid_hex, const char* tech, void* context) {
    TimeClock* app = context;
    ScanCtx* ctx =
        (ScanCtx*)(uintptr_t)scene_manager_get_scene_state(app->scene_manager, TimeClockSceneScan);
    if(!ctx) return;

    // Single-shot: all three radios run at once, so a second UID can already be
    // queued. Consume only the first and stop every radio immediately, so the
    // same tap can never be recorded twice.
    if(ctx->handled) return;
    ctx->handled = true;
    timeclock_reader_stop(ctx->reader);

    strncpy(app->scanned_uid, uid_hex, TC_UID_STR_MAX - 1);
    app->scanned_uid[TC_UID_STR_MAX - 1] = '\0';
    strncpy(app->scanned_tech, tech, TC_TECH_MAX - 1);
    app->scanned_tech[TC_TECH_MAX - 1] = '\0';
    int found = timeclock_find_badge(app, app->scanned_uid);

    if(app->scan_purpose == TcScanReplace) {
        int tgt = app->replace_index;
        if(tgt < 0) {
            timeclock_scene_scan_result(
                app, ctx, tc_str(StrChip), tc_str(StrUnknownBadge), TimeClockSceneMenu);
        } else if(found >= 0 && found != tgt) {
            timeclock_notify_error(app);
            snprintf(
                scan_msg, sizeof(scan_msg), "%s\n%s", tc_str(StrChipUsed), app->badges[found].name);
            timeclock_scene_scan_result(
                app, ctx, tc_str(StrChip), scan_msg, TimeClockSceneBadgeDetail);
        } else {
            Badge* b = &app->badges[tgt];
            strncpy(b->uid, app->scanned_uid, TC_UID_STR_MAX - 1);
            b->uid[TC_UID_STR_MAX - 1] = '\0';
            strncpy(b->tech, app->scanned_tech, TC_TECH_MAX - 1);
            b->tech[TC_TECH_MAX - 1] = '\0';
            tc_badges_save(app->badges, app->badge_count);
            timeclock_notify_success(app);
            snprintf(scan_msg, sizeof(scan_msg), "%s\n%s", tc_str(StrChipSet), b->name);
            timeclock_scene_scan_result(
                app, ctx, tc_str(StrChip), scan_msg, TimeClockSceneBadgeDetail);
        }
        app->replace_index = -1;
        return;
    }

    if(app->scan_purpose == TcScanRegister) {
        if(found >= 0) {
            timeclock_notify_error(app);
            snprintf(
                scan_msg, sizeof(scan_msg), "%s\n%s", tc_str(StrAlreadyReg), app->badges[found].name);
            timeclock_scene_scan_result(
                app, ctx, tc_str(StrBadge), scan_msg, TimeClockSceneBadgeList);
        } else {
            // New chip: confirm it was read, then go straight to name entry.
            timeclock_notify_detected(app);
            scene_manager_set_scene_state(app->scene_manager, TimeClockSceneNameInput, 0);
            scene_manager_next_scene(app->scene_manager, TimeClockSceneNameInput);
        }
        return;
    }

    // TcScanPunch
    if(found >= 0) {
        TcEventType type = timeclock_record_punch(app, found);
        char t[8];
        tc_now_time(t, sizeof(t));
        snprintf(
            scan_msg,
            sizeof(scan_msg),
            "%s\n%s at %s",
            app->badges[found].name,
            tc_event_str(type),
            t);
        timeclock_scene_scan_result(app, ctx, tc_str(StrSaved), scan_msg, TimeClockSceneMenu);
    } else {
        timeclock_notify_error(app);
        snprintf(scan_msg, sizeof(scan_msg), "%s", tc_str(StrNotRegistered));
        timeclock_scene_scan_result(
            app, ctx, tc_str(StrUnknownBadge), scan_msg, TimeClockSceneMenu);
    }
}

void timeclock_scene_scan_on_enter(void* context) {
    TimeClock* app = context;

    ScanCtx* ctx = malloc(sizeof(ScanCtx));
    memset(ctx, 0, sizeof(ScanCtx));
    ctx->tech = (app->config.work_tech < TimeclockReaderTechCount) ?
                    (TimeclockReaderTech)app->config.work_tech :
                    TimeclockReaderTechNfc;
    ctx->reader = timeclock_reader_alloc(app->view_dispatcher);
    timeclock_reader_set_callback(ctx->reader, timeclock_scene_scan_on_uid, app);
    scene_manager_set_scene_state(app->scene_manager, TimeClockSceneScan, (uint32_t)(uintptr_t)ctx);

    scan_view_set_nav_callback(app->scan_view, timeclock_scene_scan_nav_cb, app);
    timeclock_scene_scan_show_reading(app, ctx);
    timeclock_reader_start_fixed(ctx->reader, ctx->tech, false);
}

bool timeclock_scene_scan_on_event(void* context, SceneManagerEvent event) {
    TimeClock* app = context;
    ScanCtx* ctx =
        (ScanCtx*)(uintptr_t)scene_manager_get_scene_state(app->scene_manager, TimeClockSceneScan);

    if(event.type == SceneManagerEventTypeCustom && ctx) {
        if(event.event == SCAN_POPUP_DONE) {
            scene_manager_search_and_switch_to_previous_scene(app->scene_manager, ctx->result_scene);
            return true;
        }
        return timeclock_reader_handle_event(ctx->reader, event.event);
    }
    return false;
}

void timeclock_scene_scan_on_exit(void* context) {
    TimeClock* app = context;
    ScanCtx* ctx =
        (ScanCtx*)(uintptr_t)scene_manager_get_scene_state(app->scene_manager, TimeClockSceneScan);
    if(ctx) {
        timeclock_reader_free(ctx->reader);
        free(ctx);
        scene_manager_set_scene_state(app->scene_manager, TimeClockSceneScan, 0);
    }
    app->replace_index = -1;
    popup_reset(app->popup);
}
