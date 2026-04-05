// scenes/protopirate_scene_receiver.c
#include "../protopirate_app_i.h"
#include "../helpers/protopirate_storage.h"
#include "views/protopirate_receiver.h"
#include <notification/notification_messages.h>
#include "proto_pirate_icons.h"

#define TAG                             "ProtoPirateSceneRx"
#define PROTOPIRATE_DISPLAY_HISTORY_MAX 20 // Reduced from 50 to save memory

// Forward declaration
void protopirate_scene_receiver_view_callback(ProtoPirateCustomEvent event, void* context);

static void protopirate_scene_receiver_update_statusbar(void* context) {
    furi_check(context);
    ProtoPirateApp* app = context;

    FuriString* frequency_str = furi_string_alloc();
    if(!frequency_str) {
        FURI_LOG_E(TAG, "frequency_str allocation failed");
        return;
    }
    FuriString* modulation_str = furi_string_alloc();
    if(!modulation_str) {
        FURI_LOG_E(TAG, "modulation_str allocation failed");
        furi_string_free(frequency_str);
        return;
    }
    FuriString* history_stat_str = furi_string_alloc();
    if(!history_stat_str) {
        FURI_LOG_E(TAG, "history_stat_str allocation failed");
        furi_string_free(frequency_str);
        furi_string_free(modulation_str);
        return;
    }

    protopirate_get_frequency_modulation(app, frequency_str, modulation_str);

    // Check if using external radio (only if radio is initialized)
    bool is_external = false;
    if(app->radio_initialized && app->txrx->radio_device) {
        is_external = radio_device_loader_is_external(app->txrx->radio_device);
    }

    furi_string_printf(
        history_stat_str,
        "%u/%u",
        protopirate_history_get_item(app->txrx->history),
        PROTOPIRATE_DISPLAY_HISTORY_MAX);
    // Pass actual external radio status
    protopirate_view_receiver_add_data_statusbar(
        app->protopirate_receiver,
        furi_string_get_cstr(frequency_str),
        furi_string_get_cstr(modulation_str),
        furi_string_get_cstr(history_stat_str),
        is_external);

    furi_string_free(frequency_str);
    furi_string_free(modulation_str);
    furi_string_free(history_stat_str);
}

static void protopirate_scene_receiver_callback(
    SubGhzReceiver* receiver,
    SubGhzProtocolDecoderBase* decoder_base,
    void* context) {
    UNUSED(receiver);
    furi_check(decoder_base);
    furi_check(context);
    ProtoPirateApp* app = context;

    FURI_LOG_I(TAG, "=== SIGNAL DECODED ===");

    FuriString* str_buff = furi_string_alloc();
    if(!str_buff) {
        FURI_LOG_E(TAG, "str_buff allocation failed");
        return;
    }

    subghz_protocol_decoder_base_get_string(decoder_base, str_buff);
    FURI_LOG_I(TAG, "%s", furi_string_get_cstr(str_buff));

    // Add to history
    if(protopirate_history_add_to_history(app->txrx->history, decoder_base, app->txrx->preset)) {
        notification_message(app->notifications, &sequence_semi_success);

        FURI_LOG_I(
            TAG,
            "Added to history, total items: %u",
            protopirate_history_get_item(app->txrx->history));

        FuriString* item_name = furi_string_alloc();
        if(!item_name) {
            FURI_LOG_E(TAG, "item_name allocation failed");
            return;
        }

        protopirate_history_get_text_item_menu(
            app->txrx->history, item_name, protopirate_history_get_item(app->txrx->history) - 1);

        protopirate_view_receiver_add_item_to_menu(
            app->protopirate_receiver, furi_string_get_cstr(item_name), 0);

        furi_string_free(item_name);

        // Auto-scroll to the last detected signal
        uint16_t last_index = protopirate_history_get_item(app->txrx->history) - 1;
        protopirate_view_receiver_set_idx_menu(app->protopirate_receiver, last_index);

        // Auto-save if enabled
        if(app->auto_save) {
            FlipperFormat* ff = protopirate_history_get_raw_data(
                app->txrx->history, protopirate_history_get_item(app->txrx->history) - 1);

            if(ff) {
                FuriString* protocol = furi_string_alloc();
                if(!protocol) {
                    FURI_LOG_E(TAG, "protocol allocation failed");
                    furi_string_free(str_buff);
                    return;
                }

                flipper_format_rewind(ff);
                if(!flipper_format_read_string(ff, "Protocol", protocol)) {
                    furi_string_set_str(protocol, "Unknown");
                }

                // Clean protocol name for filename
                furi_string_replace_all(protocol, "/", "_");
                furi_string_replace_all(protocol, " ", "_");

                FuriString* saved_path = furi_string_alloc();
                if(!saved_path) {
                    FURI_LOG_E(TAG, "saved_path allocation failed");
                    furi_string_free(protocol);
                    furi_string_free(str_buff);
                    return;
                }

                if(protopirate_storage_save_capture(
                       ff, furi_string_get_cstr(protocol), saved_path)) {
                    FURI_LOG_I(TAG, "Auto-saved: %s", furi_string_get_cstr(saved_path));
                    notification_message(app->notifications, &sequence_double_vibro);
                } else {
                    FURI_LOG_E(TAG, "Auto-save failed");
                }

                furi_string_free(protocol);
                furi_string_free(saved_path);
            }
        }

        view_dispatcher_send_custom_event(
            app->view_dispatcher, ProtoPirateCustomEventSceneReceiverUpdate);
    } else {
        FURI_LOG_W(TAG, "Failed to add to history (duplicate or full)");
    }

    furi_string_free(str_buff);

    // Pause hopper when we receive something
    if(app->txrx->hopper_state == ProtoPirateHopperStateRunning) {
        app->txrx->hopper_state = ProtoPirateHopperStatePause;
        app->txrx->hopper_timeout = 10;
    }
}

void protopirate_scene_receiver_on_enter(void* context) {
    furi_check(context);
    ProtoPirateApp* app = context;

    FURI_LOG_I(TAG, "=== ENTERING RECEIVER SCENE ===");

// Now safe to access radio device
#ifndef REMOVE_LOGS
    bool is_external =
        app->txrx->radio_device ? radio_device_loader_is_external(app->txrx->radio_device) : false;
    const char* device_name =
        app->txrx->radio_device ? subghz_devices_get_name(app->txrx->radio_device) : NULL;
    FURI_LOG_I(TAG, "Radio device: %s", device_name ? device_name : "NULL");
    FURI_LOG_I(TAG, "Is External: %s", is_external ? "YES" : "NO");
    FURI_LOG_I(TAG, "Frequency: %lu Hz", app->txrx->preset->frequency);
    FURI_LOG_I(TAG, "Modulation: %s", furi_string_get_cstr(app->txrx->preset->name));
    FURI_LOG_I(TAG, "Auto-save: %s", app->auto_save ? "ON" : "OFF");
#endif

    // Allocate history
    if(!app->txrx->history) {
        app->txrx->history = protopirate_history_alloc();
        if(!app->txrx->history) {
            FURI_LOG_E(TAG, "Failed to allocate history!");
            return;
        }
    }

    // Allocate worker
    if(!app->txrx->worker) {
        app->txrx->worker = subghz_worker_alloc();
        if(!app->txrx->worker) {
            FURI_LOG_E(TAG, "Failed to allocate worker!");
            return;
        }
        // Set up worker callbacks
        subghz_worker_set_overrun_callback(
            app->txrx->worker, (SubGhzWorkerOverrunCallback)subghz_receiver_reset);
        subghz_worker_set_pair_callback(
            app->txrx->worker, (SubGhzWorkerPairCallback)subghz_receiver_decode);
        subghz_worker_set_context(app->txrx->worker, app->txrx->receiver);
    }

    // Set up the receiver callback
    subghz_receiver_set_rx_callback(app->txrx->receiver, protopirate_scene_receiver_callback, app);

    // Set up view callback
    protopirate_view_receiver_set_callback(
        app->protopirate_receiver, protopirate_scene_receiver_view_callback, app);

    // Update status bar
    protopirate_scene_receiver_update_statusbar(app);

    // Start hopper if enabled
    if(app->txrx->hopper_state != ProtoPirateHopperStateOFF) {
        app->txrx->hopper_state = ProtoPirateHopperStateRunning;
    }

    // Get preset data
    const char* preset_name = furi_string_get_cstr(app->txrx->preset->name);
    uint8_t* preset_data = subghz_setting_get_preset_data_by_name(app->setting, preset_name);

    if(preset_data == NULL) {
        FURI_LOG_E(TAG, "Failed to get preset data for %s, using AM650", preset_name);
        preset_data = subghz_setting_get_preset_data_by_name(app->setting, "AM650");
    }

    // Begin receiving
    protopirate_begin(app, preset_data);

    uint32_t frequency = app->txrx->preset->frequency;
    if(app->txrx->hopper_state == ProtoPirateHopperStateRunning) {
        frequency = subghz_setting_get_hopper_frequency(app->setting, 0);
        app->txrx->hopper_idx_frequency = 0;
    }

    FURI_LOG_I(TAG, "Starting RX on %lu Hz", frequency);
    protopirate_rx(app, frequency);
    FURI_LOG_I(TAG, "RX started, state: %d", app->txrx->txrx_state);

    // Update lock state in view
    protopirate_view_receiver_set_lock(app->protopirate_receiver, app->lock);

    // Update auto-save state in view
    protopirate_view_receiver_set_autosave(app->protopirate_receiver, app->auto_save);

    //Not in Sub Decode Mode
    protopirate_view_receiver_set_sub_decode_mode(app->protopirate_receiver, false);

    // Switch to receiver view
    view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewReceiver);
}

bool protopirate_scene_receiver_on_event(void* context, SceneManagerEvent event) {
    furi_check(context);
    ProtoPirateApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case ProtoPirateCustomEventSceneReceiverUpdate:
            protopirate_scene_receiver_update_statusbar(app);
            consumed = true;
            break;

        case ProtoPirateCustomEventViewReceiverOK: {
            uint16_t idx = protopirate_view_receiver_get_idx_menu(app->protopirate_receiver);
            FURI_LOG_I(TAG, "Selected item %d", idx);
            if(idx < protopirate_history_get_item(app->txrx->history)) {
                app->txrx->idx_menu_chosen = idx;
                scene_manager_set_scene_state(app->scene_manager, ProtoPirateSceneReceiver, 1);
                scene_manager_next_scene(app->scene_manager, ProtoPirateSceneReceiverInfo);
            }
        }
            consumed = true;
            break;

        case ProtoPirateCustomEventViewReceiverConfig:
            scene_manager_set_scene_state(app->scene_manager, ProtoPirateSceneReceiver, 1);
            scene_manager_next_scene(app->scene_manager, ProtoPirateSceneReceiverConfig);
            consumed = true;
            break;

        case ProtoPirateCustomEventViewReceiverBack:
            bool confirmed_exit = false;

            if(protopirate_history_get_item(app->txrx->history)) {
                scene_manager_set_scene_state(app->scene_manager, ProtoPirateSceneReceiver, 1);
                //app->option_flags += FLAG_RECEIVER_DIRTY_DIALOG;
                app->dialogs = furi_record_open(RECORD_DIALOGS);
                DialogMessage* message = dialog_message_alloc();
                dialog_message_set_buttons(message, "Back", NULL, "Continue");
                dialog_message_set_icon(message, &I_WarningDolphin_45x42, 0, 12);
                dialog_message_set_header(
                    message, "Exit Confirmation", 64, 0, AlignCenter, AlignTop);
                dialog_message_set_text(
                    message, "You will lose\nany unsaved\nsignals.", 50, 14, AlignLeft, AlignTop);
                DialogMessageButton dialog_result = dialog_message_show(app->dialogs, message);
                dialog_message_free(message);
                furi_record_close(RECORD_DIALOGS);
                app->dialogs = NULL;

                //Exit if the user said yes.
                if(dialog_result == DialogMessageButtonRight) {
                    confirmed_exit = true;
                    scene_manager_set_scene_state(app->scene_manager, ProtoPirateSceneReceiver, 0);
                }
            } else {
                //Dont need to confirm.
                confirmed_exit = true;
            }

            //If the user confirmed, or we arent dirty we can exit now.
            if(confirmed_exit) {
                if(app->txrx->txrx_state == ProtoPirateTxRxStateRx) {
                    protopirate_rx_end(app);
                }
                protopirate_sleep(app);
                scene_manager_search_and_switch_to_previous_scene(
                    app->scene_manager, ProtoPirateSceneStart);
            }
            consumed = true;
            break;

        case ProtoPirateCustomEventViewReceiverUnlock:
            app->lock = ProtoPirateLockOff;
            protopirate_view_receiver_set_lock(app->protopirate_receiver, app->lock);
            consumed = true;
            break;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        // Update hopper
        if(app->txrx->hopper_state != ProtoPirateHopperStateOFF) {
            protopirate_hopper_update(app);
            protopirate_scene_receiver_update_statusbar(app);
        }

        // Update RSSI from the correct radio device (only if initialized)
        if(app->radio_initialized && app->txrx->txrx_state == ProtoPirateTxRxStateRx &&
           app->txrx->radio_device) {
            float rssi = subghz_devices_get_rssi(app->txrx->radio_device);
            protopirate_view_receiver_set_rssi(app->protopirate_receiver, rssi);

            // Debug: Log RSSI periodically (every ~5 seconds)
            static uint8_t rssi_log_counter = 0;
            if(++rssi_log_counter >= 50) {
#ifndef REMOVE_LOGS
                bool is_external = app->txrx->radio_device ?
                                       radio_device_loader_is_external(app->txrx->radio_device) :
                                       false;
                FURI_LOG_D(TAG, "RSSI: %.1f dBm (%s)", (double)rssi, is_external ? "EXT" : "INT");
#endif
                rssi_log_counter = 0;
            }

            // Blink the light like the SubGHZ app
            notification_message(app->notifications, &sequence_blink_cyan_10);
        }

        consumed = true;
    }

    return consumed;
}

void protopirate_scene_receiver_on_exit(void* context) {
    furi_check(context);
    ProtoPirateApp* app = context;

    FURI_LOG_I(TAG, "=== EXITING RECEIVER SCENE ===");

    // Only try to stop RX if radio is initialized
    if(app->radio_initialized && app->txrx->txrx_state == ProtoPirateTxRxStateRx) {
        protopirate_rx_end(app);
    }
    if(app->txrx->worker) {
        FURI_LOG_D(TAG, "Freeing worker %p", app->txrx->worker);
        subghz_worker_free(app->txrx->worker);
        app->txrx->worker = NULL;
    } else {
        FURI_LOG_D(TAG, "Worker was NULL, skipping free");
    }

    if(scene_manager_get_scene_state(app->scene_manager, ProtoPirateSceneReceiver) == 1) {
        scene_manager_set_scene_state(app->scene_manager, ProtoPirateSceneReceiver, 0);
        return;
    }

    // Reset both view menu AND history when actually leaving (only if radio initialized)
    protopirate_view_receiver_reset_menu(app->protopirate_receiver);
    if(app->radio_initialized && app->txrx->history) {
        protopirate_history_reset(app->txrx->history);
    }

    if(app->txrx->history) {
        FURI_LOG_D(TAG, "Freeing history %p", app->txrx->history);
        protopirate_history_free(app->txrx->history);
        app->txrx->history = NULL;
    } else {
        FURI_LOG_D(TAG, "History was NULL, skipping free");
    }
}

void protopirate_scene_receiver_view_callback(ProtoPirateCustomEvent event, void* context) {
    furi_check(context);
    ProtoPirateApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}
