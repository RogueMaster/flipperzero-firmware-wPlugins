#include "../protopirate_app_i.h"
#include "protopirate_bruteforce_host.h"
#include "radio_device_loader.h"
#include "helpers/protopirate_storage.h"

#include <notification/notification_messages.h>

#define TAG "PPHostAPI"

static inline bool host_ensure_receiver_view(void* app) {
    return protopirate_ensure_receiver_view((ProtoPirateApp*)app);
}

static inline bool host_ensure_widget(void* app) {
    return protopirate_ensure_widget((ProtoPirateApp*)app);
}

static inline bool host_ensure_view_about(void* app) {
    return protopirate_ensure_view_about((ProtoPirateApp*)app);
}

static inline bool host_radio_init(void* app) {
    return protopirate_radio_init((ProtoPirateApp*)app);
}

static inline void host_rx_stack_resume_after_tx(void* app) {
    protopirate_rx_stack_resume_after_tx((ProtoPirateApp*)app);
}

static inline bool host_refresh_protocol_registry(void* app, bool ensure_receiver_ready) {
    return protopirate_refresh_protocol_registry((ProtoPirateApp*)app, ensure_receiver_ready);
}

static inline bool host_apply_protocol_registry_for_context(
    void* app,
    const char* preset_name,
    uint32_t frequency,
    const uint8_t* preset_data,
    size_t preset_data_size,
    const char* protocol_name) {
    return protopirate_apply_protocol_registry_for_context(
        (ProtoPirateApp*)app, preset_name, frequency, preset_data, preset_data_size, protocol_name);
}

static inline void host_begin(void* app, uint8_t* preset_data) {
    protopirate_begin((ProtoPirateApp*)app, preset_data);
}

static inline uint32_t host_rx(void* app, uint32_t frequency) {
    return protopirate_rx((ProtoPirateApp*)app, frequency);
}

static inline void host_rx_end(void* app) {
    protopirate_rx_end((ProtoPirateApp*)app);
}

static inline void host_get_frequency_modulation_str(
    void* app,
    char* frequency,
    size_t frequency_size,
    char* modulation,
    size_t modulation_size) {
    protopirate_get_frequency_modulation_str(
        (ProtoPirateApp*)app, frequency, frequency_size, modulation, modulation_size);
}

static inline void host_bruteforce_context_release(ProtoPirateApp* app) {
    protopirate_bruteforce_context_release((ProtoPirateApp*)app);
}

static inline Widget* host_get_widget(ProtoPirateApp* app) {
    return app ? app->widget : NULL;
}

static inline FlipperFormat* host_get_history_flipper_format(ProtoPirateApp* app) {
    if(!app || !app->txrx || !app->txrx->history) return NULL;
    return protopirate_history_get_raw_data(app->txrx->history, app->txrx->idx_menu_chosen);
}

static inline uint16_t host_get_history_index(ProtoPirateApp* app) {
    return app && app->txrx ? app->txrx->idx_menu_chosen : 0;
}

static inline void host_set_history_index(ProtoPirateApp* app, uint16_t idx) {
    if(app && app->txrx) app->txrx->idx_menu_chosen = idx;
}

static inline ProtoPirateHistory* host_get_history(ProtoPirateApp* app) {
    return app && app->txrx ? app->txrx->history : NULL;
}

static inline void host_history_set_item_str(ProtoPirateApp* app, uint16_t idx, const char* str) {
    ProtoPirateHistory* history = host_get_history(app);
    if(history) protopirate_history_set_item_str(history, idx, str);
}

static inline void
    host_patch_flipper_format_on_success(FlipperFormat* ff, const BruteForceState* s) {
    if(!ff || !s) return;
    flipper_format_rewind(ff);
    flipper_format_insert_or_update_uint32(ff, FF_SERIAL, &s->decrypted_serial, 1);
    uint32_t btn = s->decrypted_button;
    flipper_format_insert_or_update_uint32(ff, FF_BTN, &btn, 1);
    flipper_format_insert_or_update_uint32(ff, FF_CNT, &s->decrypted_counter, 1);
    uint32_t type = s->decrypted_type;
    flipper_format_insert_or_update_uint32(ff, FF_TYPE, &type, 1);
    uint32_t crc_val = s->decrypted_crc;
    flipper_format_insert_or_update_uint32(ff, "CRC", &crc_val, 1);
    flipper_format_insert_or_update_uint32(ff, "Seed", &s->decrypted_seed, 1);
}

static inline void host_send_custom_event(ProtoPirateApp* app, uint32_t event) {
    if(app) view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

static inline void host_notification_error(ProtoPirateApp* app) {
    if(app) notification_message(app->notifications, &sequence_error);
}

static inline void host_notification_success(ProtoPirateApp* app) {
    if(app) notification_message(app->notifications, &sequence_success);
}

static inline void host_saved_info_rebuild_widget(ProtoPirateApp* app) {
    protopirate_scene_saved_info_on_enter(app);
}

static inline void host_subdecode_signal_info_refresh(ProtoPirateApp* app) {
    host_send_custom_event(app, ProtoPirateCustomEventSubDecodeUpdate);
}

static inline void host_scene_previous(ProtoPirateApp* app) {
    if(app) scene_manager_previous_scene(app->scene_manager);
}

static const char* host_get_loaded_file_path(ProtoPirateApp* app) {
    return app->loaded_file_path;
}

const ProtoPirateSharedPluginHostApi protopirate_shared_plugin_host_api = {
    .fap_version = FAP_VERSION,
    .ensure_receiver_view = host_ensure_receiver_view,
    .ensure_widget = host_ensure_widget,
    .get_widget = host_get_widget,
    .ensure_view_about = host_ensure_view_about,
    .ensure_text_input = protopirate_ensure_text_input,
    .free_text_input = protopirate_free_text_input,
    .radio_init = host_radio_init,
    .rx_stack_resume_after_tx = host_rx_stack_resume_after_tx,
    .preset_init = protopirate_preset_init,
    .refresh_protocol_registry = host_refresh_protocol_registry,
    .apply_protocol_registry_for_context = host_apply_protocol_registry_for_context,
    .begin = host_begin,
    .rx = host_rx,
    .rx_end = host_rx_end,
    .get_frequency_modulation_str = host_get_frequency_modulation_str,
    .history_release_scratch = protopirate_history_release_scratch,
    .radio_device_is_external = radio_device_loader_is_external,
    .receiver_add_data_statusbar = protopirate_view_receiver_add_data_statusbar,
    .receiver_get_idx_menu = protopirate_view_receiver_get_idx_menu,
    .receiver_set_idx_menu = protopirate_view_receiver_set_idx_menu,
    .receiver_set_callback = protopirate_view_receiver_set_callback,
    .receiver_set_sub_decode_mode = protopirate_view_receiver_set_sub_decode_mode,
    .receiver_set_sub_decode_progress = protopirate_view_receiver_set_sub_decode_progress,
    .receiver_reset_menu = protopirate_view_receiver_reset_menu,
    .receiver_sync_menu_from_history = protopirate_view_receiver_sync_menu_from_history,
    .settings_load = protopirate_settings_load,
    .settings_save = protopirate_settings_save,
    .plugin_load = shared_plugin_load,
    .plugin_unload = shared_plugin_unload,
    .bruteforce_plugin_ensure_loaded = protopirate_bruteforce_plugin_ensure_loaded,
    .bruteforce_plugin_unload_if_idle = protopirate_bruteforce_plugin_unload_if_idle,
    .bruteforce_context_release = host_bruteforce_context_release,
    .protocol_catalog_can_tx = protopirate_protocol_catalog_can_tx,
    .storage_delete_file = protopirate_storage_delete_file,
    .protocol_catalog_offers_bruteforce = protopirate_protocol_catalog_offers_bruteforce,
    .rx_stack_suspend_for_tx = protopirate_rx_stack_suspend_for_tx,
    .idle = protopirate_idle,
    .storage_delete_temp = protopirate_storage_delete_temp,
    .get_history_flipper_format = host_get_history_flipper_format,
    .get_history_index = host_get_history_index,
    .set_history_index = host_set_history_index,
    .get_history = host_get_history,
    .history_set_item_str = host_history_set_item_str,
    .patch_flipper_format_on_success = host_patch_flipper_format_on_success,
    .send_custom_event = host_send_custom_event,
    .notification_error = host_notification_error,
    .notification_success = host_notification_success,
    .receiver_info_rebuild_widget = protopirate_receiver_info_rebuild_normal_widget,
    .saved_info_rebuild_widget = host_saved_info_rebuild_widget,
    .subdecode_signal_info_refresh = host_subdecode_signal_info_refresh,
    .scene_previous = host_scene_previous,
    .get_loaded_file_path = host_get_loaded_file_path,
};
