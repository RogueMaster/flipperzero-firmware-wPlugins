#include "../tesla_fsd_app.h"
#include "../scenes_config/app_scene_functions.h"
#include <stdio.h>

#define FSD_DISPLAY_REFRESH_MS 250
#define WIRING_WARN_TIMEOUT_MS 5000

static void fsd_update_display(TeslaFSDApp* app, uint32_t uptime_ms) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    FSDState state = app->fsd_state;
    TeslaHWVersion hw = app->hw_version;
    furi_mutex_release(app->mutex);

    widget_reset(app->widget);

    // Wiring sanity check: nothing on the bus after 5s == bad wiring
    if(state.rx_count == 0 && uptime_ms > WIRING_WARN_TIMEOUT_MS) {
        widget_add_string_element(
            app->widget, 64, 6, AlignCenter, AlignTop, FontPrimary, "No CAN traffic");
        widget_add_string_multiline_element(
            app->widget,
            64,
            22,
            AlignCenter,
            AlignTop,
            FontSecondary,
            "Check wiring.\n"
            "Try swapping CAN-H/L.\n"
            "Termination cut?");
        widget_add_string_element(
            app->widget, 64, 56, AlignCenter, AlignTop, FontSecondary, "[BACK] to stop");
        return;
    }

    const char* hw_str;
    int max_profile;
    switch(hw) {
    case TeslaHW_Legacy:
        hw_str = "Legacy";
        max_profile = 2;
        break;
    case TeslaHW_HW3:
        hw_str = "HW3";
        max_profile = 2;
        break;
    case TeslaHW_HW4:
        hw_str = "HW4";
        max_profile = 4;
        break;
    default:
        hw_str = "??";
        max_profile = 0;
        break;
    }

    widget_add_string_element(
        app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "Tesla FSD Active");

    char line1[40];
    snprintf(
        line1, sizeof(line1), "HW: %s    Profile: %d/%d", hw_str, state.speed_profile, max_profile);
    widget_add_string_element(app->widget, 2, 16, AlignLeft, AlignTop, FontSecondary, line1);

    const char* mode_str = "ACT";
    if(state.op_mode == OpMode_ListenOnly)
        mode_str = "LSN";
    else if(state.op_mode == OpMode_Service)
        mode_str = "SVC";

    char line2[44];
    if(state.tesla_ota_in_progress) {
        snprintf(line2, sizeof(line2), "OTA — TX paused [%s]", mode_str);
    } else {
        snprintf(
            line2,
            sizeof(line2),
            "FSD: %s  Nag: %s [%s]",
            state.fsd_enabled ? "ON" : "WAIT",
            state.nag_suppressed ? "OFF" : "--",
            mode_str);
    }
    widget_add_string_element(app->widget, 2, 26, AlignLeft, AlignTop, FontSecondary, line2);

    char line3[44];
    snprintf(
        line3,
        sizeof(line3),
        "TX:%lu RX:%lu Err:%lu",
        (unsigned long)state.frames_modified,
        (unsigned long)state.rx_count,
        (unsigned long)state.crc_err_count);
    widget_add_string_element(app->widget, 2, 36, AlignLeft, AlignTop, FontSecondary, line3);

    // show active features
    char line4[48];
    snprintf(
        line4,
        sizeof(line4),
        "%s%s%s%s",
        state.force_fsd ? "FORCE " : "",
        state.suppress_speed_chime ? "CHIME " : "",
        state.emergency_vehicle_detect ? "EMRG " : "",
        state.nag_killer ? "NAG" : "");
    if(line4[0]) {
        widget_add_string_element(app->widget, 2, 46, AlignLeft, AlignTop, FontSecondary, line4);
    }

    widget_add_string_element(
        app->widget, 64, 56, AlignCenter, AlignTop, FontSecondary, "[BACK] to stop");
}

static int32_t fsd_running_worker(void* context) {
    TeslaFSDApp* app = context;
    MCP2515* mcp = app->mcp_can;
    CANFRAME frame;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    FSDState state = app->fsd_state;
    state.force_fsd = app->force_fsd;
    state.suppress_speed_chime = app->suppress_speed_chime;
    state.emergency_vehicle_detect = app->emergency_vehicle_detect;
    state.nag_killer = app->nag_killer;
    state.op_mode = app->op_mode;
    furi_mutex_release(app->mutex);

    // Listen-only mode → MCP2515 hardware listen-only register
    // Active / Service → normal mode (TX permitted)
    mcp->mode = (state.op_mode == OpMode_ListenOnly) ? MCP_LISTENONLY : MCP_NORMAL;
    mcp->bitRate = MCP_500KBPS;
    mcp->clck = MCP_16MHZ;

    if(mcp2515_init(mcp) != ERROR_OK) {
        view_dispatcher_send_custom_event(app->view_dispatcher, TeslaFSDEventNoDevice);
        return 0;
    }

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
    } else {
        // HW3 / HW4 — keep autopilot control on RXB0, leave RXB1 wide open so
        // we always see 0x318 (OTA detect), 0x370 (nag killer), 0x399 (chime),
        // 0x3F8 (follow distance). Software dispatch filters by canId.
        init_mask(mcp, 0, 0x7FF);
        init_filter(mcp, 0, CAN_ID_AP_CONTROL);
        init_filter(mcp, 1, CAN_ID_AP_CONTROL);
        init_mask(mcp, 1, 0x000);
        init_filter(mcp, 2, 0x000);
        init_filter(mcp, 3, 0x000);
        init_filter(mcp, 4, 0x000);
        init_filter(mcp, 5, 0x000);
    }

    uint32_t last_display = 0;
    uint32_t last_err_check = 0;
    uint32_t worker_start = furi_get_tick();

    while(true) {
        uint32_t flags = furi_thread_flags_get();
        if(flags & WorkerFlagStop) break;

        // Periodic CAN error register sample (~every 250ms)
        uint32_t now = furi_get_tick();
        if((now - last_err_check) >= furi_ms_to_ticks(250)) {
            uint8_t eflg = get_error(mcp);
            // EFLG bits 0/1 = RX0/RX1 overflow, bit 4 = receive error warn,
            // bit 5 = transmit error warn — any of these = bus health issue
            if(eflg) state.crc_err_count++;
            last_err_check = now;
        }

        if(check_receive(mcp) == ERROR_OK) {
            if(read_can_message(mcp, &frame) == ERROR_OK) {
                state.rx_count++;

                bool tx_allowed = fsd_can_transmit(&state);

                // Always handle OTA monitoring regardless of mode
                if(frame.canId == CAN_ID_GTW_CAR_STATE) {
                    fsd_handle_gtw_car_state(&state, &frame);
                }

                if(frame.canId == CAN_ID_EPAS_STATUS && state.nag_killer) {
                    CANFRAME echo;
                    if(fsd_handle_nag_killer(&state, &frame, &echo) && tx_allowed) {
                        send_can_frame(mcp, &echo);
                    }
                } else if(frame.canId == CAN_ID_STW_ACTN_RQ && state.hw_version == TeslaHW_Legacy) {
                    fsd_handle_legacy_stalk(&state, &frame);
                } else if(frame.canId == CAN_ID_AP_LEGACY && state.hw_version == TeslaHW_Legacy) {
                    if(fsd_handle_legacy_autopilot(&state, &frame) && tx_allowed) {
                        send_can_frame(mcp, &frame);
                    }
                } else if(frame.canId == CAN_ID_ISA_SPEED && state.suppress_speed_chime) {
                    if(fsd_handle_isa_speed_chime(&frame) && tx_allowed) {
                        send_can_frame(mcp, &frame);
                    }
                } else if(frame.canId == CAN_ID_FOLLOW_DIST) {
                    fsd_handle_follow_distance(&state, &frame);
                } else if(frame.canId == CAN_ID_AP_CONTROL) {
                    if(fsd_handle_autopilot_frame(&state, &frame) && tx_allowed) {
                        send_can_frame(mcp, &frame);
                    }
                }

                if((now - last_display) >= furi_ms_to_ticks(FSD_DISPLAY_REFRESH_MS)) {
                    furi_mutex_acquire(app->mutex, FuriWaitForever);
                    app->fsd_state = state;
                    furi_mutex_release(app->mutex);
                    uint32_t uptime_ms =
                        (now - worker_start) * 1000 / furi_kernel_get_tick_frequency();
                    fsd_update_display(app, uptime_ms);
                    last_display = now;
                }
            }
        } else {
            // Even when no frame arrived, refresh the display so the wiring
            // warning can show after the timeout.
            if((now - last_display) >= furi_ms_to_ticks(FSD_DISPLAY_REFRESH_MS)) {
                furi_mutex_acquire(app->mutex, FuriWaitForever);
                app->fsd_state = state;
                furi_mutex_release(app->mutex);
                uint32_t uptime_ms =
                    (now - worker_start) * 1000 / furi_kernel_get_tick_frequency();
                fsd_update_display(app, uptime_ms);
                last_display = now;
            }
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
        app->widget, 64, 28, AlignCenter, AlignCenter, FontPrimary, "Starting...");
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
                app->widget,
                64,
                28,
                AlignCenter,
                AlignCenter,
                FontPrimary,
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
