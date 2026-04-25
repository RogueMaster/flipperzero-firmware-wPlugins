#include "../tesla_fsd_app.h"
#include "../scenes_config/app_scene_functions.h"
#include <stdio.h>

#define FSD_DISPLAY_REFRESH_MS 250
#define WIRING_WARN_TIMEOUT_MS 5000
#define PRECOND_INTERVAL_MS    500

static void fsd_update_display(TeslaFSDApp* app, uint32_t uptime_ms) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    FSDState state = app->fsd_state;
    TeslaHWVersion hw = app->hw_version;
    furi_mutex_release(app->mutex);

    widget_reset(app->widget);

    // Wiring sanity check: nothing on the bus after 5s == bad wiring
    if(state.rx_count == 0 && uptime_ms > WIRING_WARN_TIMEOUT_MS) {
        widget_add_string_element(
            app->widget, 64, 6, AlignCenter, AlignTop, FontPrimary,
            "No CAN traffic");
        widget_add_string_multiline_element(
            app->widget, 64, 22, AlignCenter, AlignTop, FontSecondary,
            "Check wiring.\n"
            "Try swapping CAN-H/L.\n"
            "Termination cut?");
        widget_add_string_element(
            app->widget, 64, 56, AlignCenter, AlignTop, FontSecondary,
            "[BACK] to stop");
        return;
    }

    const char* hw_str;
    int max_profile;
    switch(hw) {
    case TeslaHW_Legacy: hw_str = "Legacy"; max_profile = 2; break;
    case TeslaHW_HW3:    hw_str = "HW3";    max_profile = 2; break;
    case TeslaHW_HW4:    hw_str = "HW4";    max_profile = 4; break;
    default:             hw_str = "??";      max_profile = 0; break;
    }

    widget_add_string_element(
        app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary,
        "Tesla FSD Active");

    char line1[40];
    snprintf(line1, sizeof(line1), "HW: %s    Profile: %d/%d", hw_str, state.speed_profile, max_profile);
    widget_add_string_element(
        app->widget, 2, 16, AlignLeft, AlignTop, FontSecondary, line1);

    const char* mode_str = "ACT";
    if(state.op_mode == OpMode_ListenOnly) mode_str = "LSN";
    else if(state.op_mode == OpMode_Service) mode_str = "SVC";

    char line2[44];
    if(state.tesla_ota_in_progress) {
        snprintf(line2, sizeof(line2), "OTA — TX paused [%s]", mode_str);
    } else {
        snprintf(line2, sizeof(line2), "FSD: %s  Nag: %s [%s]",
            state.fsd_enabled ? "ON" : "WAIT",
            state.nag_suppressed ? "OFF" : "--",
            mode_str);
    }
    widget_add_string_element(
        app->widget, 2, 26, AlignLeft, AlignTop, FontSecondary, line2);

    char line3[44];
    snprintf(line3, sizeof(line3), "TX:%lu RX:%lu Err:%lu",
        (unsigned long)state.frames_modified,
        (unsigned long)state.rx_count,
        (unsigned long)state.crc_err_count);
    widget_add_string_element(
        app->widget, 2, 36, AlignLeft, AlignTop, FontSecondary, line3);

    // Line 4: live BMS readout if we've seen any BMS frames, else feature flags
    char line4[48];
    if(state.bms_seen) {
        float kw = state.pack_voltage_v * state.pack_current_a / 1000.0f;
        snprintf(line4, sizeof(line4), "SoC:%.0f%% %.0fkW %d-%dC",
            (double)state.soc_percent, (double)kw,
            state.batt_temp_min_c, state.batt_temp_max_c);
    } else {
        snprintf(line4, sizeof(line4), "%s%s%s%s%s%s",
            state.force_fsd ? "FORCE " : "",
            state.suppress_speed_chime ? "CHIME " : "",
            state.emergency_vehicle_detect ? "EMRG " : "",
            state.nag_killer ? "NAG " : "",
            state.precondition ? "PRECOND " : "",
            state.tlssc_restore ? "TLSSC" : "");
    }
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

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    FSDState state = app->fsd_state;
    state.force_fsd = app->force_fsd;
    state.suppress_speed_chime = app->suppress_speed_chime;
    state.emergency_vehicle_detect = app->emergency_vehicle_detect;
    state.nag_killer = app->nag_killer;
    state.precondition = app->precondition;
    state.op_mode = app->op_mode;
    // extras
    state.extra_hazard_lights = app->extra_hazard_lights;
    state.extra_wiper_off = app->extra_auto_wipers_off;
    state.extra_park_inject = false;
    state.extra_steering_mode = app->extra_steering_mode;
    state.extra_highbeam_strobe = app->extra_highbeam_strobe;
    state.extra_turn_left = app->extra_turn_left;
    state.extra_turn_right = app->extra_turn_right;
    // Ban Shield: don't arm immediately — learn healthy state first.
    // gtw_shield_armed starts false; fsd_handle_gtw_shield() auto-arms
    // after all 8 mux snapshots are captured.
    bool shield_enabled = app->gtw_shield;
    state.gtw_shield_armed = false;
    state.tlssc_restore = app->tlssc_restore;
    state.gtw_tier_override = app->gtw_tier_override;
    state.assist_nav_enable = app->assist_nav_enable;
    state.assist_hands_off = app->assist_hands_off;
    state.assist_dev_mode = app->assist_dev_mode;
    state.assist_lhd_override = app->assist_lhd_override;
    state.assist_show_lane_graph = app->assist_show_lane_graph;
    state.assist_tlssc_bit38 = app->assist_tlssc_bit38;
    furi_mutex_release(app->mutex);

    // Listen-only mode → MCP2515 hardware listen-only register
    // Active / Service → normal mode (TX permitted)
    mcp->mode = (state.op_mode == OpMode_ListenOnly) ? MCP_LISTENONLY : MCP_NORMAL;
    mcp->bitRate = MCP_500KBPS;
    // 0=16MHz (default), 1=8MHz, 2=12MHz
    switch(app->mcp_clock) {
    case 1:  mcp->clck = MCP_8MHZ;  break;
    case 2:  mcp->clck = MCP_12MHZ; break;
    default: mcp->clck = MCP_16MHZ; break;
    }

    if(mcp2515_init(mcp) != ERROR_OK) {
        view_dispatcher_send_custom_event(app->view_dispatcher, TeslaFSDEventNoDevice);
        return 0;
    }

    // MCP2515 filters: wide-open on RXB1 for all modes. RXB0 prioritizes
    // the main autopilot frame for the detected HW. Legacy also needs
    // wide-open to see 0x3FD for the Legacy→HW3 auto-upgrade trigger.
    {
        uint16_t primary_id = (state.hw_version == TeslaHW_Legacy)
            ? CAN_ID_AP_LEGACY : CAN_ID_AP_CONTROL;
        init_mask(mcp, 0, 0x7FF);
        init_filter(mcp, 0, primary_id);
        init_filter(mcp, 1, primary_id);
        init_mask(mcp, 1, 0x000);
        init_filter(mcp, 2, 0x000);
        init_filter(mcp, 3, 0x000);
        init_filter(mcp, 4, 0x000);
        init_filter(mcp, 5, 0x000);
    }

    uint32_t last_display = 0;
    uint32_t last_err_check = 0;
    uint32_t last_precond = 0;
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

        // High beam strobe: inject SCCM_leftStalk every 200ms, alternating flash on/off
        {
            static uint32_t last_strobe = 0;
            static uint8_t strobe_counter = 0;
            static bool strobe_phase = false;
            if(state.extra_highbeam_strobe && state.op_mode == OpMode_Service &&
               fsd_can_transmit(&state) && (now - last_strobe) >= furi_ms_to_ticks(200)) {
                CANFRAME sf;
                fsd_build_highbeam_flash(&sf, strobe_counter, strobe_phase);
                send_can_frame(mcp, &sf);
                strobe_counter = (strobe_counter + 1) & 0x0F;
                strobe_phase = !strobe_phase;
                last_strobe = now;
            }
        }

        // Turn signal injection: inject SCCM_leftStalk every 300ms while toggle is on
        {
            static uint32_t last_turn = 0;
            static uint8_t turn_counter = 0;
            if((state.extra_turn_left || state.extra_turn_right) &&
               state.op_mode == OpMode_Service && fsd_can_transmit(&state) &&
               (now - last_turn) >= furi_ms_to_ticks(300)) {
                CANFRAME tf;
                uint8_t dir = state.extra_turn_left ? 3 : 1; // 3=DOWN_1(left) 1=UP_1(right)
                fsd_build_turn_signal(&tf, turn_counter, dir);
                send_can_frame(mcp, &tf);
                turn_counter = (turn_counter + 1) & 0x0F;
                last_turn = now;
            }
        }

        // Precondition trigger: inject 0x082 every 500ms while toggle is on
        if(state.precondition && fsd_can_transmit(&state) &&
           (now - last_precond) >= furi_ms_to_ticks(PRECOND_INTERVAL_MS)) {
            CANFRAME pf;
            fsd_build_precondition_frame(&pf);
            send_can_frame(mcp, &pf);
            last_precond = now;
        }

        if(check_receive(mcp) == ERROR_OK) {
            if(read_can_message(mcp, &frame) == ERROR_OK) {
                state.rx_count++;

                bool tx_allowed = fsd_can_transmit(&state);

                // Auto-upgrade Legacy→HW3 if 0x3FD is seen on the bus.
                // Palladium S/X with HW3 reports das_hw=0 (→Legacy) but
                // actually uses 0x3FD, not 0x3EE. True Legacy cars never
                // broadcast 0x3FD.
                if(state.hw_version == TeslaHW_Legacy &&
                   frame.canId == CAN_ID_AP_CONTROL) {
                    state.hw_version = TeslaHW_HW3;
                    state.speed_profile = 2;
                    // Reprogram RXB0 filter from 0x3EE → 0x3FD for HW3
                    init_mask(mcp, 0, 0x7FF);
                    init_filter(mcp, 0, CAN_ID_AP_CONTROL);
                    init_filter(mcp, 1, CAN_ID_AP_CONTROL);
                    // Update app-level HW for UI display
                    furi_mutex_acquire(app->mutex, FuriWaitForever);
                    app->hw_version = TeslaHW_HW3;
                    app->fsd_state.hw_version = TeslaHW_HW3;
                    furi_mutex_release(app->mutex);
                }

                // Always handle OTA monitoring regardless of mode
                if(frame.canId == CAN_ID_GTW_CAR_STATE) {
                    fsd_handle_gtw_car_state(&state, &frame);
                }
                // Live BMS sniff (read-only, mode-independent)
                else if(frame.canId == CAN_ID_BMS_HV_BUS) {
                    fsd_handle_bms_hv(&state, &frame);
                }
                else if(frame.canId == CAN_ID_BMS_SOC) {
                    fsd_handle_bms_soc(&state, &frame);
                }
                else if(frame.canId == CAN_ID_BMS_THERMAL) {
                    fsd_handle_bms_thermal(&state, &frame);
                }
                // Extras: read-only vehicle state parsers (mode-independent)
                else if(frame.canId == CAN_ID_DI_SYS_STATUS) {
                    fsd_handle_di_system_status(&state, &frame);
                }
                else if(frame.canId == CAN_ID_VCRIGHT_STATUS) {
                    fsd_handle_vcright_status(&state, &frame);
                }
                else if(frame.canId == CAN_ID_DI_SPEED) {
                    fsd_handle_di_speed(&state, &frame);
                }
                else if(frame.canId == CAN_ID_ESP_STATUS) {
                    fsd_handle_esp_status(&state, &frame);
                }
                else if(frame.canId == CAN_ID_DAS_STATUS) {
                    fsd_handle_das_status(&state, &frame);
                }
                else if(frame.canId == CAN_ID_DAS_STATUS2) {
                    fsd_handle_das_status2(&state, &frame);
                }
                else if(frame.canId == CAN_ID_DAS_SETTINGS) {
                    fsd_handle_das_settings(&state, &frame);
                }
                else if(frame.canId == CAN_ID_DAS_AP_CONFIG) {
                    if(fsd_handle_tlssc_restore(&state, &frame) && tx_allowed) {
                        send_can_frame(mcp, &frame);
                    }
                }
                else if(frame.canId == CAN_ID_ENERGY_CONS) {
                    fsd_handle_energy_consumption(&state, &frame);
                }
                else if(frame.canId == CAN_ID_GTW_CONFIG_ETH) {
                    fsd_handle_gtw_autopilot_tier(&state, &frame);
                    // Shield and tier override are mutually exclusive on the
                    // same frame — shield freezes existing state, override
                    // forces tier=3. Don't send two conflicting copies.
                    if(shield_enabled) {
                        if(fsd_handle_gtw_shield(&state, &frame) && tx_allowed) {
                            send_can_frame(mcp, &frame);
                        }
                    } else if(fsd_handle_gtw_tier_override(&state, &frame) && tx_allowed) {
                        send_can_frame(mcp, &frame);
                    }
                }

                // Track Mode inject (Service mode only, 0x313)
                if(frame.canId == CAN_ID_TRACK_MODE_SET) {
                    if(fsd_handle_track_mode_inject(&state, &frame) && tx_allowed) {
                        send_can_frame(mcp, &frame);
                    }
                }
                else if(frame.canId == CAN_ID_DAS_CONTROL) {
                    fsd_handle_das_control(&state, &frame);
                }
                else if(frame.canId == CAN_ID_DI_STATE) {
                    fsd_handle_di_state(&state, &frame);
                }
                else if(frame.canId == CAN_ID_DI_TORQUE) {
                    fsd_handle_di_torque(&state, &frame);
                }
                else if(frame.canId == CAN_ID_UI_WARNING) {
                    fsd_handle_ui_warning(&state, &frame);
                }
                else if(frame.canId == CAN_ID_STEER_ANGLE) {
                    fsd_handle_steering_angle(&state, &frame);
                }
                else if(frame.canId == CAN_ID_DAS_STEER) {
                    fsd_handle_das_steering(&state, &frame);
                }

                // Extras: write handlers (Service mode only, gated inside each handler)
                if(frame.canId == CAN_ID_VCFRONT_LIGHT) {
                    if(fsd_handle_hazard_inject(&state, &frame) && tx_allowed) {
                        send_can_frame(mcp, &frame);
                    }
                    if(fsd_handle_wiper_off(&state, &frame) && tx_allowed) {
                        send_can_frame(mcp, &frame);
                    }
                }

                if(frame.canId == CAN_ID_EPAS_STATUS) {
                    // Always parse steering mode from 0x370 (read-only)
                    fsd_handle_epas_steering_mode(&state, &frame);
                    // Nag killer TX (if enabled)
                    if(state.nag_killer) {
                        CANFRAME echo;
                        if(fsd_handle_nag_killer(&state, &frame, &echo) && tx_allowed) {
                            send_can_frame(mcp, &echo);
                        }
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
                    if(fsd_handle_driver_assist_override(&state, &frame) && tx_allowed) {
                        send_can_frame(mcp, &frame);
                    }
                } else if(frame.canId == CAN_ID_AP_CONTROL) {
                    if(fsd_handle_autopilot_frame(&state, &frame) && tx_allowed) {
                        send_can_frame(mcp, &frame);
                    }
                }

                if((now - last_display) >= furi_ms_to_ticks(FSD_DISPLAY_REFRESH_MS)) {
                    furi_mutex_acquire(app->mutex, FuriWaitForever);
                    app->fsd_state = state;
                    furi_mutex_release(app->mutex);
                    uint32_t uptime_ms = (now - worker_start) * 1000 / furi_kernel_get_tick_frequency();
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
                uint32_t uptime_ms = (now - worker_start) * 1000 / furi_kernel_get_tick_frequency();
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
        app->widget, 64, 28, AlignCenter, AlignCenter, FontPrimary,
        "Starting...");
    view_dispatcher_switch_to_view(app->view_dispatcher, TeslaFSDViewWidget);

    app->worker_thread = furi_thread_alloc_ex("TeslaFSD", 4096, fsd_running_worker, app);
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
