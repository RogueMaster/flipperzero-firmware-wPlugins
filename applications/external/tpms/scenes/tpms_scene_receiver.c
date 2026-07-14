#include "../tpms_app_i.h"
#include "../views/tpms_receiver.h"
#include "../protocols/schrader_gg4.h"
#include "../protocols/schrader_smd3ma4.h"
#include "../protocols/schrader_eg53ma4.h"
#include "../protocols/abarth_124.h"
#include <string.h>

// Sweep mode configuration
// Ticks per combo is calculated from app->sweep_seconds_per_combo * 10
// Max cycles is from app->sweep_max_cycles

static const uint32_t sweep_frequencies[] = {
    433920000, // 433.92 MHz - most common TPMS frequency
    315000000, // 315 MHz - US TPMS frequency
};
#define SWEEP_FREQUENCY_COUNT (sizeof(sweep_frequencies) / sizeof(sweep_frequencies[0]))

static const char* sweep_presets[] = {
    "AM650",
    "AM270",
    "FM238",
    "FM12K",
    "FM476",
};
#define SWEEP_PRESET_COUNT (sizeof(sweep_presets) / sizeof(sweep_presets[0]))

// Forward declaration
static void tpms_scene_receiver_sweep_next_combo(TPMSApp* app);

// Check if the decoded protocol matches the current filter
static bool tpms_protocol_filter_match(TPMSApp* app, SubGhzProtocolDecoderBase* decoder_base) {
    if(app->protocol_filter == TPMSProtocolFilterAll) {
        return true;
    }

    const char* protocol_name = decoder_base->protocol->name;

    switch(app->protocol_filter) {
    case TPMSProtocolFilterSchraderGG4:
        return strcmp(protocol_name, TPMS_PROTOCOL_SCHRADER_GG4_NAME) == 0;
    case TPMSProtocolFilterSchraderSMD3MA4:
        return strcmp(protocol_name, TPMS_PROTOCOL_SCHRADER_SMD3MA4_NAME) == 0;
    case TPMSProtocolFilterSchraderEG53MA4:
        return strcmp(protocol_name, TPMS_PROTOCOL_SCHRADER_EG53MA4_NAME) == 0;
    case TPMSProtocolFilterAbarth124:
        return strcmp(protocol_name, TPMS_PROTOCOL_ABARTH_124_NAME) == 0;
    default:
        return true;
    }
}

// Advance to next sweep combination
static void tpms_scene_receiver_sweep_next_combo(TPMSApp* app) {
    // Move to next preset
    app->sweep_preset_index++;

    // If we've tried all presets for this frequency, move to next frequency
    if(app->sweep_preset_index >= SWEEP_PRESET_COUNT) {
        app->sweep_preset_index = 0;
        app->sweep_frequency_index++;

        // If we've tried all frequencies, increment cycle count
        if(app->sweep_frequency_index >= SWEEP_FREQUENCY_COUNT) {
            app->sweep_frequency_index = 0;
            app->sweep_cycle_count++;
        }
    }

    app->sweep_tick_count = 0;
}

// Apply current sweep settings and start receiving
static void tpms_scene_receiver_sweep_apply(TPMSApp* app) {
    // Stop current RX
    if(app->txrx->txrx_state == TPMSTxRxStateRx) {
        tpms_rx_end(app);
    }

    // Get current sweep settings
    uint32_t frequency = sweep_frequencies[app->sweep_frequency_index];
    const char* preset = sweep_presets[app->sweep_preset_index];

    // Initialize with new preset and frequency
    tpms_preset_init(app, preset, frequency, NULL, 0);

    // Start receiving with new settings
    if((app->txrx->txrx_state == TPMSTxRxStateIDLE) ||
       (app->txrx->txrx_state == TPMSTxRxStateSleep)) {
        tpms_begin(
            app,
            subghz_setting_get_preset_data_by_name(
                app->setting, furi_string_get_cstr(app->txrx->preset->name)));
        tpms_rx(app, app->txrx->preset->frequency);
    }
}

static const NotificationSequence subghz_sequence_rx = {
    &message_green_255,

    &message_vibro_on,
    &message_note_c6,
    &message_delay_50,
    &message_sound_off,
    &message_vibro_off,

    &message_delay_50,
    NULL,
};

static const NotificationSequence subghz_sequence_rx_locked = {
    &message_green_255,

    &message_display_backlight_on,

    &message_vibro_on,
    &message_note_c6,
    &message_delay_50,
    &message_sound_off,
    &message_vibro_off,

    &message_delay_500,

    &message_display_backlight_off,
    NULL,
};

static void tpms_scene_receiver_update_statusbar(void* context) {
    TPMSApp* app = context;
    FuriString* history_stat_str;
    history_stat_str = furi_string_alloc();
    if(!tpms_history_get_text_space_left(app->txrx->history, history_stat_str)) {
        FuriString* frequency_str;
        FuriString* modulation_str;

        frequency_str = furi_string_alloc();
        modulation_str = furi_string_alloc();

        tpms_get_frequency_modulation(app, frequency_str, modulation_str);

        tpms_view_receiver_add_data_statusbar(
            app->tpms_receiver,
            furi_string_get_cstr(frequency_str),
            furi_string_get_cstr(modulation_str),
            furi_string_get_cstr(history_stat_str),
            radio_device_loader_is_external(app->txrx->radio_device));

        furi_string_free(frequency_str);
        furi_string_free(modulation_str);
    } else {
        tpms_view_receiver_add_data_statusbar(
            app->tpms_receiver,
            furi_string_get_cstr(history_stat_str),
            "",
            "",
            radio_device_loader_is_external(app->txrx->radio_device));
    }
    furi_string_free(history_stat_str);
}

void tpms_scene_receiver_callback(TPMSCustomEvent event, void* context) {
    furi_assert(context);
    TPMSApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

static void tpms_scene_receiver_add_to_history_callback(
    SubGhzReceiver* receiver,
    SubGhzProtocolDecoderBase* decoder_base,
    void* context) {
    furi_assert(context);
    TPMSApp* app = context;

    // Check if the protocol matches the current filter
    if(!tpms_protocol_filter_match(app, decoder_base)) {
        subghz_receiver_reset(receiver);
        return;
    }

    FuriString* str_buff;
    str_buff = furi_string_alloc();

    if(tpms_history_add_to_history(app->txrx->history, decoder_base, app->txrx->preset) ==
       TPMSHistoryStateAddKeyNewDada) {
        furi_string_reset(str_buff);

        tpms_history_get_text_item_menu(
            app->txrx->history, str_buff, tpms_history_get_item(app->txrx->history) - 1);
        tpms_view_receiver_add_item_to_menu(
            app->tpms_receiver,
            furi_string_get_cstr(str_buff),
            tpms_history_get_type_protocol(
                app->txrx->history, tpms_history_get_item(app->txrx->history) - 1));

        tpms_scene_receiver_update_statusbar(app);
        notification_message(app->notifications, &sequence_blink_green_10);
        if(app->lock != TPMSLockOn) {
            notification_message(app->notifications, &subghz_sequence_rx);
        } else {
            notification_message(app->notifications, &subghz_sequence_rx_locked);
        }

        // Mark signal found for sweep mode and store result info
        if(app->scan_mode == TPMSScanModeSweep) {
            app->sweep_found_signal = true;
            // Store the successful frequency and modulation
            app->sweep_result_frequency = app->txrx->preset->frequency;
            strncpy(
                app->sweep_result_preset,
                furi_string_get_cstr(app->txrx->preset->name),
                sizeof(app->sweep_result_preset) - 1);
            app->sweep_result_preset[sizeof(app->sweep_result_preset) - 1] = '\0';
        }
    }
    subghz_receiver_reset(receiver);
    furi_string_free(str_buff);
    app->txrx->rx_key_state = TPMSRxKeyStateAddKey;
}

void tpms_scene_receiver_on_enter(void* context) {
    TPMSApp* app = context;

    FuriString* str_buff;
    str_buff = furi_string_alloc();

    if(app->txrx->rx_key_state == TPMSRxKeyStateIDLE) {
        // For sweep mode, use first sweep combo; otherwise use defaults
        if(app->scan_mode == TPMSScanModeSweep) {
            tpms_preset_init(
                app,
                sweep_presets[app->sweep_preset_index],
                sweep_frequencies[app->sweep_frequency_index],
                NULL,
                0);
        } else {
            tpms_preset_init(
                app, "AM650", subghz_setting_get_default_frequency(app->setting), NULL, 0);
        }
        tpms_history_reset(app->txrx->history);
        app->txrx->rx_key_state = TPMSRxKeyStateStart;
    }

    tpms_view_receiver_set_lock(app->tpms_receiver, app->lock);
    tpms_view_receiver_set_scan_mode(app->tpms_receiver, app->scan_mode);
    if(app->scan_mode == TPMSScanModeSweep) {
        tpms_view_receiver_set_sweep_cycle(app->tpms_receiver, app->sweep_cycle_count);
        tpms_view_receiver_set_sweep_countdown(app->tpms_receiver, app->sweep_seconds_per_combo);
    }

    //Load history to receiver
    tpms_view_receiver_exit(app->tpms_receiver);
    for(uint8_t i = 0; i < tpms_history_get_item(app->txrx->history); i++) {
        furi_string_reset(str_buff);
        tpms_history_get_text_item_menu(app->txrx->history, str_buff, i);
        tpms_view_receiver_add_item_to_menu(
            app->tpms_receiver,
            furi_string_get_cstr(str_buff),
            tpms_history_get_type_protocol(app->txrx->history, i));
        app->txrx->rx_key_state = TPMSRxKeyStateAddKey;
    }
    furi_string_free(str_buff);
    tpms_scene_receiver_update_statusbar(app);

    tpms_view_receiver_set_callback(app->tpms_receiver, tpms_scene_receiver_callback, app);
    subghz_receiver_set_rx_callback(
        app->txrx->receiver, tpms_scene_receiver_add_to_history_callback, app);

    if(app->txrx->txrx_state == TPMSTxRxStateRx) {
        tpms_rx_end(app);
    };
    if((app->txrx->txrx_state == TPMSTxRxStateIDLE) ||
       (app->txrx->txrx_state == TPMSTxRxStateSleep)) {
        tpms_begin(
            app,
            subghz_setting_get_preset_data_by_name(
                app->setting, furi_string_get_cstr(app->txrx->preset->name)));

        tpms_rx(app, app->txrx->preset->frequency);
    }

    tpms_view_receiver_set_idx_menu(app->tpms_receiver, app->txrx->idx_menu_chosen);
    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewReceiver);
}

bool tpms_scene_receiver_on_event(void* context, SceneManagerEvent event) {
    TPMSApp* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case TPMSCustomEventViewReceiverBack:
            // Stop CC1101 Rx
            if(app->txrx->txrx_state == TPMSTxRxStateRx) {
                tpms_rx_end(app);
                tpms_sleep(app);
            };
            app->txrx->hopper_state = TPMSHopperStateOFF;
            app->txrx->idx_menu_chosen = 0;
            subghz_receiver_set_rx_callback(app->txrx->receiver, NULL, app);

            app->txrx->rx_key_state = TPMSRxKeyStateIDLE;
            tpms_preset_init(
                app, "AM650", subghz_setting_get_default_frequency(app->setting), NULL, 0);
            if(scene_manager_has_previous_scene(app->scene_manager, TPMSSceneStart)) {
                consumed = scene_manager_search_and_switch_to_previous_scene(
                    app->scene_manager, TPMSSceneStart);
            } else {
                scene_manager_next_scene(app->scene_manager, TPMSSceneStart);
            }
            break;
        case TPMSCustomEventViewReceiverOK:
            app->txrx->idx_menu_chosen = tpms_view_receiver_get_idx_menu(app->tpms_receiver);
            scene_manager_next_scene(app->scene_manager, TPMSSceneReceiverInfo);
            consumed = true;
            break;
        case TPMSCustomEventViewReceiverConfig:
            app->txrx->idx_menu_chosen = tpms_view_receiver_get_idx_menu(app->tpms_receiver);
            scene_manager_next_scene(app->scene_manager, TPMSSceneReceiverConfig);
            consumed = true;
            break;
        case TPMSCustomEventViewReceiverOffDisplay:
            notification_message(app->notifications, &sequence_display_backlight_off);
            consumed = true;
            break;
        case TPMSCustomEventViewReceiverUnlock:
            app->lock = TPMSLockOff;
            consumed = true;
            break;
        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        if(app->txrx->hopper_state != TPMSHopperStateOFF) {
            tpms_hopper_update(app);
            tpms_scene_receiver_update_statusbar(app);
        }

        // Sweep mode logic
        if(app->scan_mode == TPMSScanModeSweep) {
            // Check if signal was found - navigate to result
            if(app->sweep_found_signal) {
                if(app->txrx->txrx_state == TPMSTxRxStateRx) {
                    tpms_rx_end(app);
                    tpms_sleep(app);
                }
                app->scan_mode = TPMSScanModeScanOnly;
                scene_manager_next_scene(app->scene_manager, TPMSSceneSweepResult);
                consumed = true;
            } else {
                app->sweep_tick_count++;

                // Update countdown display
                uint8_t remaining = app->sweep_seconds_per_combo - (app->sweep_tick_count / 10);
                tpms_view_receiver_set_sweep_countdown(app->tpms_receiver, remaining);

                // Trigger 125kHz at start of each combo (tick 0 and 1)
                if(app->sweep_tick_count == 1) {
                    // Trigger 125kHz activation via the view's relearn function
                    tpms_view_receiver_trigger_activation(app->tpms_receiver);
                }

                // Time to move to next combo? (seconds * 10 ticks per second)
                if(app->sweep_tick_count >= (app->sweep_seconds_per_combo * 10)) {
                    // Check if we've completed all cycles
                    if(app->sweep_cycle_count >= app->sweep_max_cycles) {
                        // Sweep complete - no signal found, go to result screen
                        if(app->txrx->txrx_state == TPMSTxRxStateRx) {
                            tpms_rx_end(app);
                            tpms_sleep(app);
                        }
                        app->scan_mode = TPMSScanModeScanOnly;
                        scene_manager_next_scene(app->scene_manager, TPMSSceneSweepResult);
                        consumed = true;
                    } else {
                        // Advance to next combo
                        tpms_scene_receiver_sweep_next_combo(app);
                        tpms_scene_receiver_sweep_apply(app);
                        tpms_scene_receiver_update_statusbar(app);
                        // Update cycle display
                        tpms_view_receiver_set_sweep_cycle(
                            app->tpms_receiver, app->sweep_cycle_count);
                    }
                }
            }
        }

        // Get current RSSI
        float rssi = furi_hal_subghz_get_rssi();
        tpms_view_receiver_set_rssi(app->tpms_receiver, rssi);

        if(app->txrx->txrx_state == TPMSTxRxStateRx) {
            notification_message(app->notifications, &sequence_blink_cyan_10);
        }
    }
    return consumed;
}

void tpms_scene_receiver_on_exit(void* context) {
    UNUSED(context);
}
