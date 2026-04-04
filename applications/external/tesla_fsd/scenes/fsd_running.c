#include "../tesla_fsd_app.h"
#include "../scenes_config/app_scene_functions.h"
#include <stdio.h>

#define FSD_DISPLAY_REFRESH_MS 250

static void fsd_update_display(TeslaFSDApp* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    FSDState state = app->fsd_state;
    TeslaHWVersion hw = app->hw_version;
    furi_mutex_release(app->mutex);

    const char* hw_str;
    int max_profile;
    switch(hw) {
    case TeslaHW_Legacy: hw_str = "Legacy"; max_profile = 2; break;
    case TeslaHW_HW3:    hw_str = "HW3";    max_profile = 2; break;
    case TeslaHW_HW4:    hw_str = "HW4";    max_profile = 4; break;
    default:             hw_str = "??";      max_profile = 0; break;
    }

    widget_reset(app->widget);

    widget_add_string_element(
        app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary,
        "Tesla FSD Active");

    char line1[40];
    snprintf(line1, sizeof(line1), "HW: %s    Profile: %d/%d", hw_str, state.speed_profile, max_profile);
    widget_add_string_element(
        app->widget, 2, 16, AlignLeft, AlignTop, FontSecondary, line1);

    char line2[40];
    snprintf(line2, sizeof(line2), "FSD: %s  Nag: %s",
        state.fsd_enabled ? "ON" : "WAIT",
        state.nag_suppressed ? "OFF" : "--");
    widget_add_string_element(
        app->widget, 2, 26, AlignLeft, AlignTop, FontSecondary, line2);

    char line3[40];
    snprintf(line3, sizeof(line3), "Frames: %lu", (unsigned long)state.frames_modified);
    widget_add_string_element(
        app->widget, 2, 36, AlignLeft, AlignTop, FontSecondary, line3);

    // show active features
    char line4[40];
    snprintf(line4, sizeof(line4), "%s%s%s",
        state.force_fsd ? "FORCE " : "",
        state.suppress_speed_chime ? "CHIME " : "",
        state.emergency_vehicle_detect ? "EMRG" : "");
    if(line4[0]) {
        widget_add_string_element(
            app->widget, 2, 46, AlignLeft, AlignTop, FontSecondary, line4);
    }

    widget_add_string_element(
        app->widget, 64, 56, AlignCenter, AlignTop, FontSecondary,
        "[BACK] to stop");
}

static int32_t fsd_running_worker(void* context) {
    TeslaFSDApp* app = context;
    MCP2515* mcp = app->mcp_can;
    CANFRAME frame;

    mcp->mode = MCP_NORMAL;
    mcp->bitRate = MCP_500KBPS;
    mcp->clck = MCP_16MHZ;

    if(mcp2515_init(mcp) != ERROR_OK) {
        view_dispatcher_send_custom_event(app->view_dispatcher, TeslaFSDEventNoDevice);
        return 0;
    }

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    FSDState state = app->fsd_state;
    state.force_fsd = app->force_fsd;
    state.suppress_speed_chime = app->suppress_speed_chime;
    state.emergency_vehicle_detect = app->emergency_vehicle_detect;
    furi_mutex_release(app->mutex);

    // configure MCP2515 filters based on mode
    if(state.hw_version == TeslaHW_Legacy) {
        init_mask(mcp, 0, 0x7FF);
        init_filter(mcp, 0, CAN_ID_AP_LEGACY);
        init_filter(mcp, 1, CAN_ID_AP_LEGACY);
        init_mask(mcp, 1, 0x7FF);
        init_filter(mcp, 2, CAN_ID_STW_ACTN_RQ);
        init_filter(mcp, 3, CAN_ID_STW_ACTN_RQ);
        init_filter(mcp, 4, CAN_ID_STW_ACTN_RQ);
        init_filter(mcp, 5, CAN_ID_STW_ACTN_RQ);
    } else if(state.hw_version == TeslaHW_HW4 && state.suppress_speed_chime) {
        // need 3 IDs: 0x3FD, 0x3F8, 0x399
        // RXB0: exact match 0x3FD
        init_mask(mcp, 0, 0x7FF);
        init_filter(mcp, 0, CAN_ID_AP_CONTROL);
        init_filter(mcp, 1, CAN_ID_AP_CONTROL);
        // RXB1: relaxed mask to accept both 0x3F8 and 0x399
        // 0x3F8 ^ 0x399 = 0x061 → mask = 0x7FF & ~0x061 = 0x79E
        init_mask(mcp, 1, 0x79E);
        init_filter(mcp, 2, CAN_ID_FOLLOW_DIST);
        init_filter(mcp, 3, CAN_ID_FOLLOW_DIST);
        init_filter(mcp, 4, CAN_ID_FOLLOW_DIST);
        init_filter(mcp, 5, CAN_ID_FOLLOW_DIST);
    } else {
        // HW3 or HW4 without chime suppress
        init_mask(mcp, 0, 0x7FF);
        init_filter(mcp, 0, CAN_ID_AP_CONTROL);
        init_filter(mcp, 1, CAN_ID_AP_CONTROL);
        init_mask(mcp, 1, 0x7FF);
        init_filter(mcp, 2, CAN_ID_FOLLOW_DIST);
        init_filter(mcp, 3, CAN_ID_FOLLOW_DIST);
        init_filter(mcp, 4, CAN_ID_FOLLOW_DIST);
        init_filter(mcp, 5, CAN_ID_FOLLOW_DIST);
    }

    uint32_t last_display = 0;

    while(true) {
        uint32_t flags = furi_thread_flags_get();
        if(flags & WorkerFlagStop) break;

        if(check_receive(mcp) == ERROR_OK) {
            if(read_can_message(mcp, &frame) == ERROR_OK) {
                // dispatch by CAN ID
                if(frame.canId == CAN_ID_STW_ACTN_RQ && state.hw_version == TeslaHW_Legacy) {
                    fsd_handle_legacy_stalk(&state, &frame);
                } else if(frame.canId == CAN_ID_AP_LEGACY && state.hw_version == TeslaHW_Legacy) {
                    if(fsd_handle_legacy_autopilot(&state, &frame)) {
                        send_can_frame(mcp, &frame);
                    }
                } else if(frame.canId == CAN_ID_ISA_SPEED && state.suppress_speed_chime) {
                    if(fsd_handle_isa_speed_chime(&frame)) {
                        send_can_frame(mcp, &frame);
                    }
                } else if(frame.canId == CAN_ID_FOLLOW_DIST) {
                    fsd_handle_follow_distance(&state, &frame);
                } else if(frame.canId == CAN_ID_AP_CONTROL) {
                    if(fsd_handle_autopilot_frame(&state, &frame)) {
                        send_can_frame(mcp, &frame);
                    }
                }

                uint32_t now = furi_get_tick();
                if((now - last_display) >= furi_ms_to_ticks(FSD_DISPLAY_REFRESH_MS)) {
                    furi_mutex_acquire(app->mutex, FuriWaitForever);
                    app->fsd_state = state;
                    furi_mutex_release(app->mutex);
                    fsd_update_display(app);
                    last_display = now;
                }
            }
        } else {
            furi_delay_ms(1);
        }
    }

    deinit_mcp2515(mcp);
    return 0;
}

void tesla_fsd_scene_fsd_running_on_enter(void* context) {
    TeslaFSDApp* app = context;

    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 28, AlignCenter, AlignCenter, FontPrimary,
        "Starting...");
    view_dispatcher_switch_to_view(app->view_dispatcher, TeslaFSDViewWidget);

    app->worker_thread = furi_thread_alloc_ex("TeslaFSD", 2048, fsd_running_worker, app);
    furi_thread_start(app->worker_thread);
}

bool tesla_fsd_scene_fsd_running_on_event(void* context, SceneManagerEvent event) {
    TeslaFSDApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == TeslaFSDEventNoDevice) {
            widget_reset(app->widget);
            widget_add_string_multiline_element(
                app->widget, 64, 28, AlignCenter, AlignCenter, FontPrimary,
                "CAN Module\nNot Found");
            consumed = true;
        }
    }
    return consumed;
}

void tesla_fsd_scene_fsd_running_on_exit(void* context) {
    TeslaFSDApp* app = context;

    if(app->worker_thread) {
        furi_thread_flags_set(furi_thread_get_id(app->worker_thread), WorkerFlagStop);
        furi_thread_join(app->worker_thread);
        furi_thread_free(app->worker_thread);
        app->worker_thread = NULL;
    }
    widget_reset(app->widget);
}
