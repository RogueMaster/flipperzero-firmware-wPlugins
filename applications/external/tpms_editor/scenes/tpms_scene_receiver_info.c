#include "../tpms_app_i.h"
#include "../views/tpms_receiver.h"

void tpms_scene_receiver_info_callback(TPMSCustomEvent event, void* context) {
    furi_assert(context);
    TPMSApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

void tpms_scene_receiver_info_on_enter(void* context) {
    TPMSApp* app = context;

    /* RX is paused before entering this scene. This keeps Back/OK immediate
     * even while a strong continuous signal is present. */
    if(app->txrx->txrx_state == TPMSTxRxStateRx) tpms_rx_end(app);
    tpms_view_receiver_info_set_callback(
        app->tpms_receiver_info, tpms_scene_receiver_info_callback, app);
    furi_mutex_acquire(app->history_mutex, FuriWaitForever);
    int16_t frame_rssi_dbm = -127;
    const bool frame_rssi_valid = tpms_history_get_frame_rssi(
        app->txrx->history, app->txrx->idx_menu_chosen, &frame_rssi_dbm);
    const uint32_t frame_frequency_hz =
        tpms_history_get_frequency(app->txrx->history, app->txrx->idx_menu_chosen);
    const char* frame_preset_name =
        tpms_history_get_preset(app->txrx->history, app->txrx->idx_menu_chosen);
    uint8_t wake_profile = TpmsWakeProfileNone;
    const bool wake_profile_valid = tpms_history_get_wake_profile(
        app->txrx->history, app->txrx->idx_menu_chosen, &wake_profile);
    tpms_view_receiver_info_update(
        app->tpms_receiver_info,
        tpms_history_get_raw_data(app->txrx->history, app->txrx->idx_menu_chosen),
        frame_frequency_hz,
        frame_preset_name,
        frame_rssi_dbm,
        frame_rssi_valid,
        wake_profile,
        wake_profile_valid);
    furi_mutex_release(app->history_mutex);
    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewReceiverInfo);
}

bool tpms_scene_receiver_info_on_event(void* context, SceneManagerEvent event) {
    TPMSApp* app = context;
    bool consumed = false;
    if((event.type == SceneManagerEventTypeCustom) &&
       (event.event == TPMSCustomEventViewReceiverInfoEdit)) {
        scene_manager_next_scene(app->scene_manager, TPMSSceneEditor);
        consumed = true;
    }
    return consumed;
}

void tpms_scene_receiver_info_on_exit(void* context) {
    UNUSED(context);
}
