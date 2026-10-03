/* Abandon hope, all ye who enter here. */

#include <furi/core/log.h>
#include <subghz/types.h>
#include <lib/toolbox/path.h>
#include <float_tools.h>
#include "subghz_wardriving_i.h"

#include <expansion/expansion.h>

#define TAG "SubGhzWarDrivingApp"

bool subghz_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    SubGhz* subghz = context;
    return scene_manager_handle_custom_event(subghz->scene_manager, event);
}

bool subghz_back_event_callback(void* context) {
    furi_assert(context);
    SubGhz* subghz = context;
    return scene_manager_handle_back_event(subghz->scene_manager);
}

void subghz_tick_event_callback(void* context) {
    furi_assert(context);
    SubGhz* subghz = context;
    scene_manager_handle_tick_event(subghz->scene_manager);
}

SubGhz* subghz_alloc() {
    SubGhz* subghz = malloc(sizeof(SubGhz));

    expansion_disable(furi_record_open(RECORD_EXPANSION));
    furi_record_close(RECORD_EXPANSION);

    subghz->file_path = furi_string_alloc();
    subghz->file_path_tmp = furi_string_alloc();

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, SUBGHZ_WARDR_APP_FOLDER);
    storage_simply_mkdir(storage, SUBGHZ_WARDR_APP_FOLDER "/autosaved");
    furi_record_close(RECORD_STORAGE);

    // GUI
    subghz->gui = furi_record_open(RECORD_GUI);

    // View Dispatcher
    subghz->view_dispatcher = view_dispatcher_alloc();

    subghz->scene_manager = scene_manager_alloc(&subghz_scene_handlers, subghz);
    view_dispatcher_set_event_callback_context(subghz->view_dispatcher, subghz);
    view_dispatcher_set_custom_event_callback(
        subghz->view_dispatcher, subghz_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        subghz->view_dispatcher, subghz_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        subghz->view_dispatcher, subghz_tick_event_callback, 100);

    // Open Notification record
    subghz->notifications = furi_record_open(RECORD_NOTIFICATION);
#if SUBGHZ_MEASURE_LOADING
    uint32_t load_ticks = furi_get_tick();
#endif
    subghz->txrx = subghz_wardriving_txrx_alloc();

    // SubMenu
    subghz->submenu = submenu_alloc();
    view_dispatcher_add_view(
        subghz->view_dispatcher, SubGhzViewIdMenu, submenu_get_view(subghz->submenu));

    // Receiver
    subghz->subghz_receiver = subghz_view_receiver_alloc();
    view_dispatcher_add_view(
        subghz->view_dispatcher,
        SubGhzViewIdReceiver,
        subghz_view_receiver_get_view(subghz->subghz_receiver));
    // Popup
    subghz->popup = popup_alloc();
    view_dispatcher_add_view(
        subghz->view_dispatcher, SubGhzViewIdPopup, popup_get_view(subghz->popup));
    // Text Input
    subghz->text_input = text_input_alloc();
    view_dispatcher_add_view(
        subghz->view_dispatcher, SubGhzViewIdTextInput, text_input_get_view(subghz->text_input));

    // Byte Input
    subghz->byte_input = byte_input_alloc();
    view_dispatcher_add_view(
        subghz->view_dispatcher, SubGhzViewIdByteInput, byte_input_get_view(subghz->byte_input));

    // Custom Widget
    subghz->widget = widget_alloc();
    view_dispatcher_add_view(
        subghz->view_dispatcher, SubGhzViewIdWidget, widget_get_view(subghz->widget));
    //Dialog
    subghz->dialogs = furi_record_open(RECORD_DIALOGS);

    // Transmitter
    subghz->subghz_transmitter = subghz_view_transmitter_alloc();
    view_dispatcher_add_view(
        subghz->view_dispatcher,
        SubGhzViewIdTransmitter,
        subghz_view_transmitter_get_view(subghz->subghz_transmitter));
    // Variable Item List
    subghz->variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        subghz->view_dispatcher,
        SubGhzViewIdVariableItemList,
        variable_item_list_get_view(subghz->variable_item_list));

    //init threshold rssi
    subghz->threshold_rssi = subghz_threshold_rssi_alloc();

    //init TxRx & Protocol & History & KeyBoard
    subghz_unlock(subghz);

    SubGhzSetting* setting = subghz_wardriving_txrx_get_setting(subghz->txrx);

    // Load last used values for Read, Read RAW, etc. or default
    subghz->last_settings = subghz_wardriving_last_settings_alloc();
    size_t preset_count = subghz_setting_get_preset_count(setting);
    subghz_wardriving_last_settings_load(subghz->last_settings, preset_count);
    // Make sure we select a frequency available in loaded setting configuration
    uint32_t last_frequency = subghz->last_settings->frequency;
    size_t count = subghz_setting_get_frequency_count(setting);
    bool found_last = false;
    bool found_default = false;
    for(size_t i = 0; i < count; i++) {
        uint32_t frequency = subghz_setting_get_frequency(setting, i);
        if(frequency == last_frequency) {
            found_last = true;
            break;
        }
        if(frequency == SUBGHZ_LAST_SETTING_DEFAULT_FREQUENCY) found_default = true;
    }
    if(!found_last) {
        if(found_default) {
            last_frequency = SUBGHZ_LAST_SETTING_DEFAULT_FREQUENCY;
        } else if(count > 0) {
            last_frequency = subghz_setting_get_frequency(setting, 0);
        }
        subghz->last_settings->frequency = last_frequency;
    }

    subghz_wardriving_txrx_set_preset_internal(
        subghz->txrx,
        subghz->last_settings->frequency,
        subghz->last_settings->preset_index,
        subghz->tx_power);
    subghz->history = subghz_wardriving_history_alloc();

    subghz_rx_key_state_set(subghz, SubGhzRxKeyStateIDLE);

    subghz->remove_duplicates = subghz->last_settings->remove_duplicates;
    subghz->ignore_filter = subghz->last_settings->ignore_filter;
    subghz->filter = subghz->last_settings->filter;
    subghz->tx_power = subghz->last_settings->tx_power;

    subghz_wardriving_txrx_receiver_set_filter(subghz->txrx, subghz->filter);
    subghz_wardriving_txrx_set_need_save_callback(subghz->txrx, subghz_save_to_file, subghz);

    if(!float_is_equal(subghz->last_settings->rssi, 0)) {
        subghz_threshold_rssi_set(subghz->threshold_rssi, subghz->last_settings->rssi);
    } else {
        subghz->last_settings->rssi = (-93.0f);
    }
#if SUBGHZ_MEASURE_LOADING
    load_ticks = furi_get_tick() - load_ticks;
    FURI_LOG_I(TAG, "Loaded: %ld ms.", load_ticks);
#endif
    //Init Error_str
    subghz->error_str = furi_string_alloc();
    subghz->gps = subghz_gps_apply(
        NULL, subghz->last_settings->gps_protocol, subghz->last_settings->gps_baudrate,
            subghz->last_settings->gps_pins);

    return subghz;
}

void subghz_free(SubGhz* subghz) {
    furi_assert(subghz);

    subghz_wardriving_txrx_speaker_off(subghz->txrx);
    subghz_wardriving_txrx_stop(subghz->txrx);
    subghz_wardriving_txrx_sleep(subghz->txrx);

    // Receiver
    view_dispatcher_remove_view(subghz->view_dispatcher, SubGhzViewIdReceiver);
    subghz_view_receiver_free(subghz->subghz_receiver);

    // TextInput
    view_dispatcher_remove_view(subghz->view_dispatcher, SubGhzViewIdTextInput);
    text_input_free(subghz->text_input);

    // ByteInput
    view_dispatcher_remove_view(subghz->view_dispatcher, SubGhzViewIdByteInput);
    byte_input_free(subghz->byte_input);

    // Custom Widget
    view_dispatcher_remove_view(subghz->view_dispatcher, SubGhzViewIdWidget);
    widget_free(subghz->widget);
    //Dialog
    furi_record_close(RECORD_DIALOGS);

    // Transmitter
    view_dispatcher_remove_view(subghz->view_dispatcher, SubGhzViewIdTransmitter);
    subghz_view_transmitter_free(subghz->subghz_transmitter);
    // Variable Item List
    view_dispatcher_remove_view(subghz->view_dispatcher, SubGhzViewIdVariableItemList);
    variable_item_list_free(subghz->variable_item_list);

    // Submenu
    view_dispatcher_remove_view(subghz->view_dispatcher, SubGhzViewIdMenu);
    submenu_free(subghz->submenu);
    // Popup
    view_dispatcher_remove_view(subghz->view_dispatcher, SubGhzViewIdPopup);
    popup_free(subghz->popup);

    // Scene manager
    scene_manager_free(subghz->scene_manager);

    // View Dispatcher
    view_dispatcher_free(subghz->view_dispatcher);

    // GUI
    furi_record_close(RECORD_GUI);
    subghz->gui = NULL;

    // threshold rssi
    subghz_threshold_rssi_free(subghz->threshold_rssi);

    subghz_wardriving_history_free(subghz->history);

    //TxRx
    subghz_wardriving_txrx_free(subghz->txrx);

    //Error string
    furi_string_free(subghz->error_str);

    // Notifications
    furi_record_close(RECORD_NOTIFICATION);
    subghz->notifications = NULL;

    // Path strings
    furi_string_free(subghz->file_path);
    furi_string_free(subghz->file_path_tmp);

    // GPS
    subghz_gps_stop(subghz->gps);

    subghz_wardriving_last_settings_free(subghz->last_settings);

    expansion_enable(furi_record_open(RECORD_EXPANSION));
    furi_record_close(RECORD_EXPANSION);

    // The rest
    free(subghz);
}

int32_t subghz_app() {
    SubGhz* subghz = subghz_alloc();

    view_dispatcher_attach_to_gui(
        subghz->view_dispatcher, subghz->gui, ViewDispatcherTypeFullscreen);
    furi_string_set(subghz->file_path, SUBGHZ_WARDR_APP_FOLDER);
    if(subghz_wardriving_txrx_is_database_loaded(subghz->txrx)) {
        scene_manager_next_scene(subghz->scene_manager, SubGhzSceneStart);
    } else {
        scene_manager_set_scene_state(
            subghz->scene_manager, SubGhzSceneShowError, SubGhzCustomEventManagerSet);
        furi_string_set(
            subghz->error_str,
            "No SD card or\ndatabase found.\nSome app function\nmay be reduced.");
        scene_manager_next_scene(subghz->scene_manager, SubGhzSceneShowError);
    }

    furi_hal_power_suppress_charge_enter();

    view_dispatcher_run(subghz->view_dispatcher);

    if(subghz->timer) {
        furi_timer_stop(subghz->timer);
        furi_timer_free(subghz->timer);
    }

    furi_hal_power_suppress_charge_exit();

    subghz_free(subghz);

    return 0;
}
