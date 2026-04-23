// scenes/protopirate_scene_receiver.c
#include "../protopirate_app_i.h"
#include "../helpers/protopirate_storage.h"
#include "views/protopirate_receiver.h"
#include <notification/notification_messages.h>
#include <stdio.h>
#include "proto_pirate_icons.h"

#define TAG "ProtoPirateSceneRx"

// Forward declaration
void protopirate_scene_receiver_view_callback(ProtoPirateCustomEvent event, void* context);

static void protopirate_scene_receiver_update_statusbar(void* context) {
    furi_check(context);
    ProtoPirateApp* app = context;

    char frequency_str[16] = {0};
    char modulation_str[8] = {0};
    char history_stat_str[16] = {0};

    protopirate_get_frequency_modulation_str(
        app, frequency_str, sizeof(frequency_str), modulation_str, sizeof(modulation_str));

    // Check if using external radio (only if radio is initialized)
    bool is_external = false;
    if(app->radio_initialized && app->txrx->radio_device) {
        is_external = radio_device_loader_is_external(app->txrx->radio_device);
    }

    if(app->txrx->rx_low_memory_hold) {
        snprintf(history_stat_str, sizeof(history_stat_str), "RAM!");
    } else {
        protopirate_history_format_status_text(
            app->txrx->history, history_stat_str, sizeof(history_stat_str));
    }
    // Pass actual external radio status
    protopirate_view_receiver_add_data_statusbar(
        app->protopirate_receiver, frequency_str, modulation_str, history_stat_str, is_external);
}

static bool protopirate_scene_receiver_low_memory_hold(ProtoPirateApp* app) {
    furi_check(app);

    if(!app->txrx->history) {
        return false;
    }

    if(protopirate_history_is_low_memory(app->txrx->history)) {
        app->txrx->rx_low_memory_hold = true;
    }

    if(!app->txrx->rx_low_memory_hold) {
        return false;
    }

    if(app->txrx->hopper_state != ProtoPirateHopperStateOFF) {
        app->txrx->hopper_state = ProtoPirateHopperStatePause;
        app->txrx->hopper_timeout = 0;
    }

    protopirate_view_receiver_set_rssi(app->protopirate_receiver, -127.0f);

    if(app->txrx->receiver || app->txrx->worker) {
        FURI_LOG_W(TAG, "Low memory active, suspending RX while browsing history");
        protopirate_rx_stack_suspend_for_tx(app);
    } else {
        FURI_LOG_W(TAG, "Low memory active, keeping receiver in history-only mode");
    }

    return true;
}

static void protopirate_scene_receiver_callback(
    SubGhzReceiver* receiver,
    SubGhzProtocolDecoderBase* decoder_base,
    void* context) {
    UNUSED(receiver);
    furi_check(decoder_base);
    furi_check(context);
    ProtoPirateApp* app = context;

    size_t free_heap_before = memmgr_get_free_heap();
    size_t max_free_block_before = memmgr_heap_get_max_free_block();
    FURI_LOG_I(TAG, "=== SIGNAL DECODED (%s) ===", decoder_base->protocol->name);

    // Add to history
    uint16_t count_before = protopirate_history_get_item(app->txrx->history);
    if(protopirate_history_add_to_history(app->txrx->history, decoder_base, app->txrx->preset)) {
        notification_message(app->notifications, &sequence_semi_success);

        FURI_LOG_I(
            TAG,
            "Added to history, total items: %u",
            protopirate_history_get_item(app->txrx->history));

        uint16_t count_after = protopirate_history_get_item(app->txrx->history);

        if(count_before >= PROTOPIRATE_HISTORY_MAX && count_after == PROTOPIRATE_HISTORY_MAX) {
            protopirate_view_receiver_pop_first_menu_item(app->protopirate_receiver);
            protopirate_view_receiver_append_menu_row_from_history(
                app->protopirate_receiver, app->txrx->history, count_after - 1);
        } else if(count_after > count_before) {
            protopirate_view_receiver_append_menu_row_from_history(
                app->protopirate_receiver, app->txrx->history, count_after - 1);
        }
        protopirate_history_note_signal_allocated(
            app->txrx->history, free_heap_before, max_free_block_before);

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
        if(protopirate_history_is_low_memory(app->txrx->history)) {
            FURI_LOG_W(TAG, "History capture paused due to low memory");
            view_dispatcher_send_custom_event(
                app->view_dispatcher, ProtoPirateCustomEventSceneReceiverUpdate);
        } else {
            FURI_LOG_W(TAG, "Failed to add to history (duplicate)");
        }
    }

    // Pause hopper when we receive something
    if(app->txrx->hopper_state == ProtoPirateHopperStateRunning) {
        app->txrx->hopper_state = ProtoPirateHopperStatePause;
        app->txrx->hopper_timeout = 10;
    }
}

static void protopirate_scene_receiver_start_rx_stack(ProtoPirateApp* app) {
    furi_check(app);
    if(!app->radio_initialized) {
        return;
    }

    protopirate_rx_stack_resume_after_tx(app);
    if(!app->txrx->receiver) {
        FURI_LOG_E(TAG, "SubGhz receiver unavailable — cannot start RX");
        notification_message(app->notifications, &sequence_error);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    if(!app->txrx->worker) {
        app->txrx->worker = subghz_worker_alloc();
        if(!app->txrx->worker) {
            FURI_LOG_E(TAG, "Failed to allocate worker!");
            return;
        }
        subghz_worker_set_overrun_callback(
            app->txrx->worker, (SubGhzWorkerOverrunCallback)subghz_receiver_reset);
        subghz_worker_set_pair_callback(
            app->txrx->worker, (SubGhzWorkerPairCallback)subghz_receiver_decode);
        subghz_worker_set_context(app->txrx->worker, app->txrx->receiver);
    }

    subghz_receiver_set_rx_callback(app->txrx->receiver, protopirate_scene_receiver_callback, app);

    if(app->txrx->hopper_state != ProtoPirateHopperStateOFF) {
        app->txrx->hopper_state = ProtoPirateHopperStateRunning;
    }

    const char* preset_name = furi_string_get_cstr(app->txrx->preset->name);
    uint8_t* preset_data = subghz_setting_get_preset_data_by_name(app->setting, preset_name);

    if(preset_data == NULL) {
        FURI_LOG_E(TAG, "Failed to get preset data for %s, using AM650", preset_name);
        preset_data = subghz_setting_get_preset_data_by_name(app->setting, "AM650");
    }

    protopirate_begin(app, preset_data);

    uint32_t frequency = app->txrx->preset->frequency;
    if(app->txrx->hopper_state == ProtoPirateHopperStateRunning) {
        frequency = subghz_setting_get_hopper_frequency(app->setting, 0);
        app->txrx->hopper_idx_frequency = 0;
    }

    FURI_LOG_I(TAG, "Starting RX on %lu Hz", frequency);
    protopirate_rx(app, frequency);
    app->txrx->rx_low_memory_hold = false;
    FURI_LOG_I(TAG, "RX started, state: %d", app->txrx->txrx_state);
}

void protopirate_scene_receiver_on_enter(void* context) {
    furi_check(context);
    ProtoPirateApp* app = context;

    if(app->txrx->history) {
        protopirate_history_release_scratch(app->txrx->history);
    }

    if(!app->txrx->history) {
        app->txrx->history = protopirate_history_alloc();
        if(!app->txrx->history) {
            FURI_LOG_E(TAG, "Failed to allocate history!");
            return;
        }
    }

    protopirate_view_receiver_set_callback(
        app->protopirate_receiver, protopirate_scene_receiver_view_callback, app);

    protopirate_scene_receiver_update_statusbar(app);

    protopirate_view_receiver_set_lock(app->protopirate_receiver, app->lock);
    protopirate_view_receiver_set_autosave(app->protopirate_receiver, app->auto_save);
    protopirate_view_receiver_set_sub_decode_mode(app->protopirate_receiver, false);

    const bool low_memory_hold = protopirate_scene_receiver_low_memory_hold(app);

    if(app->radio_initialized && !app->txrx->receiver) {
        view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewReceiver);
        if(!low_memory_hold) {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, ProtoPirateCustomEventReceiverDeferredRxStart);
        }
        return;
    }

#ifndef REMOVE_LOGS
    bool is_external =
        app->txrx->radio_device ? radio_device_loader_is_external(app->txrx->radio_device) : false;
    const char* device_name =
        app->txrx->radio_device ? subghz_devices_get_name(app->txrx->radio_device) : NULL;
    FURI_LOG_I(TAG, "=== ENTERING RECEIVER SCENE ===");
    FURI_LOG_I(TAG, "Radio device: %s", device_name ? device_name : "NULL");
    FURI_LOG_I(TAG, "Is External: %s", is_external ? "YES" : "NO");
    FURI_LOG_I(TAG, "Frequency: %lu Hz", app->txrx->preset->frequency);
    FURI_LOG_I(TAG, "Modulation: %s", furi_string_get_cstr(app->txrx->preset->name));
    FURI_LOG_I(TAG, "Auto-save: %s", app->auto_save ? "ON" : "OFF");
#endif

    if(app->radio_initialized && !low_memory_hold) {
        protopirate_scene_receiver_start_rx_stack(app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewReceiver);
}

static void protopirate_scene_receiver_handle_back(ProtoPirateApp* app) {
    if(app->txrx->history && protopirate_history_get_item(app->txrx->history) > 0 &&
       !app->auto_save) {
        scene_manager_set_scene_state(app->scene_manager, ProtoPirateSceneReceiver, 1);
        scene_manager_next_scene(app->scene_manager, ProtoPirateSceneNeedSaving);
    } else {
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, ProtoPirateSceneStart);
    }
}

bool protopirate_scene_receiver_on_event(void* context, SceneManagerEvent event) {
    furi_check(context);
    ProtoPirateApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case ProtoPirateCustomEventReceiverDeferredRxStart:
#ifndef REMOVE_LOGS
            FURI_LOG_I(TAG, "Deferred RX start (post-emulate path)");
#endif
            if(!protopirate_scene_receiver_low_memory_hold(app)) {
                protopirate_scene_receiver_start_rx_stack(app);
            }
            consumed = true;
            break;

        case ProtoPirateCustomEventSceneReceiverUpdate:
            if(!protopirate_scene_receiver_low_memory_hold(app) && app->radio_initialized &&
               !app->txrx->receiver) {
                protopirate_scene_receiver_start_rx_stack(app);
            }
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

        case ProtoPirateCustomEventViewReceiverDeleteItem: {
            uint16_t idx = protopirate_view_receiver_get_idx_menu(app->protopirate_receiver);
            if(idx < protopirate_history_get_item(app->txrx->history)) {
                protopirate_history_delete_item(app->txrx->history, idx);
                protopirate_view_receiver_delete_item(app->protopirate_receiver, idx);
                if(app->txrx->rx_low_memory_hold &&
                   !protopirate_history_is_low_memory(app->txrx->history)) {
                    app->txrx->rx_low_memory_hold = false;
                }
                if(!protopirate_scene_receiver_low_memory_hold(app)) {
                    if(app->radio_initialized && !app->txrx->receiver) {
                        protopirate_scene_receiver_start_rx_stack(app);
                    }
                }
                protopirate_scene_receiver_update_statusbar(app);
                app->txrx->idx_menu_chosen =
                    protopirate_view_receiver_get_idx_menu(app->protopirate_receiver);
            }
            consumed = true;
            break;
        }

        case ProtoPirateCustomEventViewReceiverConfig:
            scene_manager_set_scene_state(app->scene_manager, ProtoPirateSceneReceiver, 1);
            scene_manager_next_scene(app->scene_manager, ProtoPirateSceneReceiverConfig);
            consumed = true;
            break;

        case ProtoPirateCustomEventViewReceiverBack:
            protopirate_scene_receiver_handle_back(app);
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
            static uint8_t hopper_statusbar_tick = 0;
            if(++hopper_statusbar_tick >= 8) {
                hopper_statusbar_tick = 0;
                protopirate_scene_receiver_update_statusbar(app);
            }
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

    const bool leaving_for_subscene =
        (scene_manager_get_scene_state(app->scene_manager, ProtoPirateSceneReceiver) == 1);

    // Only try to stop RX if radio is initialized
    if(app->radio_initialized && app->txrx->txrx_state == ProtoPirateTxRxStateRx) {
        protopirate_rx_end(app);
    }

    if(leaving_for_subscene) {
        scene_manager_set_scene_state(app->scene_manager, ProtoPirateSceneReceiver, 0);
        return;
    }

    if(app->txrx->worker) {
        FURI_LOG_D(TAG, "Freeing worker %p", app->txrx->worker);
        subghz_worker_free(app->txrx->worker);
        app->txrx->worker = NULL;
    } else {
        FURI_LOG_D(TAG, "Worker was NULL, skipping free");
    }

    // Full teardown: put radio to sleep, free worker and history
    protopirate_sleep(app);

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

    app->txrx->rx_low_memory_hold = false;
}

void protopirate_scene_receiver_view_callback(ProtoPirateCustomEvent event, void* context) {
    furi_check(context);
    ProtoPirateApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}
