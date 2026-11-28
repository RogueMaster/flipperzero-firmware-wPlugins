#include "tpms_app_i.h"
#include "tpms_save.h"

#include <furi.h>
#include <furi_hal.h>
#include <stdlib.h>
#include "protocols/proto6/custom_presets.h"



bool tpms_ensure_text_input(TPMSApp* app) {
    if(!app || !app->view_dispatcher) return false;
    if(app->text_input) return true;
    app->text_input = text_input_alloc();
    if(!app->text_input) return false;
    view_dispatcher_add_view(
        app->view_dispatcher, TPMSViewTextInput, text_input_get_view(app->text_input));
    return true;
}

void tpms_release_text_input(TPMSApp* app) {
    if(!app || !app->text_input) return;
    view_dispatcher_remove_view(app->view_dispatcher, TPMSViewTextInput);
    text_input_free(app->text_input);
    app->text_input = NULL;
}

bool tpms_ensure_number_input(TPMSApp* app) {
    if(!app || !app->view_dispatcher) return false;
    if(app->number_input) return true;
    app->number_input = number_input_alloc();
    if(!app->number_input) return false;
    view_dispatcher_add_view(
        app->view_dispatcher, TPMSViewNumberInput, number_input_get_view(app->number_input));
    return true;
}

void tpms_release_number_input(TPMSApp* app) {
    if(!app || !app->number_input) return;
    view_dispatcher_remove_view(app->view_dispatcher, TPMSViewNumberInput);
    number_input_free(app->number_input);
    app->number_input = NULL;
}

static bool tpms_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    TPMSApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool tpms_app_back_event_callback(void* context) {
    furi_assert(context);
    TPMSApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void tpms_app_tick_event_callback(void* context) {
    furi_assert(context);
    TPMSApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

TPMSApp* tpms_app_alloc() {
    TPMSApp* app = calloc(1, sizeof(TPMSApp));
    furi_check(app);

    // GUI
    app->gui = furi_record_open(RECORD_GUI);

    // View Dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&tpms_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, tpms_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, tpms_app_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, tpms_app_tick_event_callback, 100);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Open Notification record
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    /* The classic GUI reads and updates history from scene callbacks. */
    app->history_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    furi_check(app->history_mutex);

    // Variable Item List
    app->variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        TPMSViewVariableItemList,
        variable_item_list_get_view(app->variable_item_list));

    // SubMenu
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TPMSViewSubmenu, submenu_get_view(app->submenu));

    // Widget
    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, TPMSViewWidget, widget_get_view(app->widget));

    // Receiver
    app->tpms_receiver = tpms_view_receiver_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TPMSViewReceiver, tpms_view_receiver_get_view(app->tpms_receiver));

    // Receiver Info
    app->tpms_receiver_info = tpms_view_receiver_info_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        TPMSViewReceiverInfo,
        tpms_view_receiver_info_get_view(app->tpms_receiver_info));

    //init setting
    app->setting = subghz_setting_alloc();

    //ToDo FIX  file name setting
    subghz_setting_load(app->setting, EXT_PATH("subghz/assets/setting_user"));

    //init Worker & Protocol & History
    app->lock = TPMSLockOff;
    app->txrx = calloc(1, sizeof(TPMSTxRx));
    furi_check(app->txrx);
    app->txrx->preset = calloc(1, sizeof(SubGhzRadioPreset));
    furi_check(app->txrx->preset);
    app->txrx->preset->name = furi_string_alloc();
    tpms_preset_init(
        app,
        "TPMS FSK",
        subghz_setting_get_default_frequency(app->setting),
        (uint8_t*)protoview_subghz_tpms1_fsk_async_regs,
        protoview_subghz_tpms1_fsk_async_regs_size);

    app->txrx->txrx_state = TPMSTxRxStateIDLE;
    app->txrx->rx_key_state = TPMSRxKeyStateIDLE;
    app->txrx->idx_menu_chosen = 0U;
    app->txrx->hopper_state = TPMSHopperStateOFF;
    app->relearn = TPMSRelearnOff;
    app->relearn_protocol_index = 0U; /* legacy field kept for source compatibility */
    app->relearn_band_index = 0U;
    app->relearn_el_band_index = 0U;
    app->relearn_ford_band_index = 0U;
    app->relearn_rx_band_index = TPMSRelearnRXBand433;
    app->relearn_modulation_index = TPMSRelearnModFSK;
    app->relearn_type = TPMSRelearnTypeCommon;
    app->relearn_autostart = false;
    app->relearn_auto_rx = false;
    app->relearn_auto_profile_index = 0U;
    app->relearn_auto_next_tick = 0U;
    /* TPMS 4.0: manual-safe defaults are 433.92 MHz + FSK.
       AUTO is optional and rotates a complete frozen UHF profile between
       EL-50449 LF cycles, starting from 433.92 FSK. */
    app->relearn_ford_auto_lf_step = 0U;
    app->relearn_ford_auto_wake_step = 0U;
    app->relearn_ford_auto_pair_uhf_profile = 3U; /* 433.92 FSK */
    app->relearn_ford_auto_pair_uhf_valid = false;
    app->relearn_ford_profile = TPMSFordLFProfileAuto;
    app->relearn_cw_frequency = TPMSCWFrequency125;
    app->relearn_repeat_mode = TPMSRelearnRepeatManual;
    app->relearn_rx_wait = TPMSRelearnRXWait5;
    app->relearn_cycle_was_active = false;
    app->relearn_repeat_waiting = false;
    app->relearn_auto_satisfied = false;
    app->relearn_last_wake_profile = TpmsWakeProfileNone;
    app->relearn_last_wake_valid = false;
    app->relearn_last_wake_tick = 0U;
    app->relearn_repeat_next_tick = 0U;
    app->relearn_led_green_phase = false;
    app->relearn_led_rx_on = false;
    app->relearn_led_next_tick = 0U;
    app->receiver_rssi_recent_peak = -127.0f;
    app->receiver_rssi_recent_start_tick = 0U;
    app->receiver_rssi_recent_valid = false;
    app->receiver_modulation_auto = false;
    app->receiver_modulation_auto_index = 0U;
    app->receiver_modulation_auto_next_tick = 0U;
    app->autosave_enabled = tpms_autosave_load_enabled();
    tpms_autosave_reset_session(app);
    app->simulation_prefill_valid = false;
    memset(&app->simulation_prefill_values, 0, sizeof(app->simulation_prefill_values));
    app->simulation_lab_source = TPMSLabSimulationNone;
    app->simulation_lab_session_slot = 0U;
    app->simulation_lab_path[0] = '\0';
    app->lab_unlocked = false;
    app->lab_about_taps = 0U;
    app->lab_about_last_tick = 0U;
    app->lab_target_pressure_kpa = 0.0f;
    app->lab_band_index = 0U;
    app->lab_modulation_index = 0U;
    app->lab_auto_profile_index = 0U;
    app->lab_sensor_count = 0U;
    app->lab_sensor_list_source = TPMSLabListSession;
    app->lab_phase_started_tick = 0U;
    app->lab_round = 0U;
    app->lab_last_tx_ok = 0U;
    app->lab_format = NULL;
    app->lab_protocol = NULL;
    tpms_lab_clear_session_files();
    app->txrx->history = tpms_history_alloc();
    app->txrx->proto6 = proto6_bridge_alloc(app->notifications);
    furi_check(app->txrx->proto6);

    furi_hal_power_suppress_charge_enter();
    furi_check(tpms_radio_init(app));

    scene_manager_next_scene(app->scene_manager, TPMSSceneStart);

    return app;
}

void tpms_app_free(TPMSApp* app) {
    furi_assert(app);
    tpms_lab_release(app, true);

    if(app->txrx && app->txrx->txrx_state == TPMSTxRxStateRx) {
        tpms_rx_end(app);
    }

    tpms_radio_deinit(app);

    // Submenu
    view_dispatcher_remove_view(app->view_dispatcher, TPMSViewSubmenu);
    submenu_free(app->submenu);

    // Variable Item List
    view_dispatcher_remove_view(app->view_dispatcher, TPMSViewVariableItemList);
    variable_item_list_free(app->variable_item_list);

    tpms_release_text_input(app);
    tpms_release_number_input(app);

    //  Widget
    view_dispatcher_remove_view(app->view_dispatcher, TPMSViewWidget);
    widget_free(app->widget);

    // Receiver
    view_dispatcher_remove_view(app->view_dispatcher, TPMSViewReceiver);
    tpms_view_receiver_free(app->tpms_receiver);

    // Receiver Info
    view_dispatcher_remove_view(app->view_dispatcher, TPMSViewReceiverInfo);
    tpms_view_receiver_info_free(app->tpms_receiver_info);

    //setting
    subghz_setting_free(app->setting);

    //Worker & Protocol & History
    tpms_history_free(app->txrx->history);
    proto6_bridge_free(app->txrx->proto6);
    app->txrx->proto6 = NULL;
    furi_string_free(app->txrx->preset->name);
    free(app->txrx->preset);
    free(app->txrx);

    // View dispatcher
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    // Notifications
    furi_mutex_free(app->history_mutex);
    app->history_mutex = NULL;
    furi_record_close(RECORD_NOTIFICATION);
    app->notifications = NULL;

    // Close records
    furi_record_close(RECORD_GUI);

    furi_hal_power_suppress_charge_exit();

    free(app);
}

int32_t tpms_app(void* p) {
    UNUSED(p);
    TPMSApp* tpms_app = tpms_app_alloc();

    view_dispatcher_run(tpms_app->view_dispatcher);

    tpms_app_free(tpms_app);

    return 0;
}
