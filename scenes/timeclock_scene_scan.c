// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "../timeclock.h"

// =============================================================================
// Scan scene - reads a badge UID (single shot) and routes to "known" or "new"
// badge, or reassigns the chip when in replace mode. The radio work is done by
// the shared TimeclockReader (full NFC protocol coverage via NfcScanner, or LF
// RFID), which reports a UID through its callback.
// =============================================================================

typedef struct {
    TimeclockReader* reader;
} ScanCtx;

// Called by the reader (GUI thread) when a UID has been read.
static void scan_on_uid(const char* uid_hex, const char* tech, void* context) {
    TimeClock* app = context;

    strncpy(app->scanned_uid, uid_hex, TC_UID_STR_MAX - 1);
    app->scanned_uid[TC_UID_STR_MAX - 1] = '\0';
    strncpy(app->scanned_tech, tech, TC_TECH_MAX - 1);
    app->scanned_tech[TC_TECH_MAX - 1] = '\0';
    app->found_index = timeclock_find_badge(app, app->scanned_uid);

    // Replace-chip mode: reassign this chip to the selected collaborator.
    if(app->replace_index >= 0) {
        int tgt = app->replace_index;
        if(app->found_index >= 0 && app->found_index != tgt) {
            timeclock_notify_error(app); // chip already used by someone else
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
        return;
    }

    timeclock_notify_success(app);
    if(app->found_index >= 0) {
        scene_manager_next_scene(app->scene_manager, TimeClockSceneBadgeAction);
    } else {
        scene_manager_next_scene(app->scene_manager, TimeClockSceneNewBadge);
    }
}

void timeclock_scene_scan_on_enter(void* context) {
    TimeClock* app = context;

    ScanCtx* ctx = malloc(sizeof(ScanCtx));
    memset(ctx, 0, sizeof(ScanCtx));
    ctx->reader = timeclock_reader_alloc(app->view_dispatcher);
    timeclock_reader_set_callback(ctx->reader, scan_on_uid, app);
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

    timeclock_reader_start(ctx->reader, app->config.use_lf, false);
}

bool timeclock_scene_scan_on_event(void* context, SceneManagerEvent event) {
    TimeClock* app = context;
    ScanCtx* ctx =
        (ScanCtx*)(uintptr_t)scene_manager_get_scene_state(app->scene_manager, TimeClockSceneScan);

    if(event.type == SceneManagerEventTypeCustom && ctx) {
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
    // Abort any pending chip replacement if the user left without scanning.
    app->replace_index = -1;
    popup_reset(app->popup);
}
