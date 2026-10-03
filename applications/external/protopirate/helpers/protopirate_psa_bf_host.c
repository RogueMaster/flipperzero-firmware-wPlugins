#include "../protopirate_app_i.h"
#include "protopirate_psa_bf_host.h"
#include "../protopirate_history.h"
#include "../protocols/protocols_common.h"
#include "../scenes/plugins/protopirate_psa_bf_plugin.h"

#include <notification/notification_messages.h>

#define TAG "PPPsaBfHost"

static bool host_ensure_widget(void* app) {
    return protopirate_ensure_widget((ProtoPirateApp*)app);
}

static Widget* host_get_widget(void* app) {
    ProtoPirateApp* a = (ProtoPirateApp*)app;
    return a ? a->widget : NULL;
}

static FlipperFormat* host_get_history_flipper_format(void* app) {
    ProtoPirateApp* a = (ProtoPirateApp*)app;
    if(!a || !a->txrx || !a->txrx->history) return NULL;
    return protopirate_history_get_raw_data(a->txrx->history, a->txrx->idx_menu_chosen);
}

static uint16_t host_get_history_index(void* app) {
    ProtoPirateApp* a = (ProtoPirateApp*)app;
    return a && a->txrx ? a->txrx->idx_menu_chosen : 0;
}

static void host_set_history_index(void* app, uint16_t idx) {
    ProtoPirateApp* a = (ProtoPirateApp*)app;
    if(a && a->txrx) a->txrx->idx_menu_chosen = idx;
}

static ProtoPirateHistory* host_get_history(void* app) {
    ProtoPirateApp* a = (ProtoPirateApp*)app;
    return a && a->txrx ? a->txrx->history : NULL;
}

static void host_history_set_item_str(void* app, uint16_t idx, const char* str) {
    ProtoPirateHistory* history = host_get_history(app);
    if(history) protopirate_history_set_item_str(history, idx, str);
}

static void host_patch_flipper_format_on_success(FlipperFormat* ff, const PsaBfState* s) {
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

static void host_send_custom_event(void* app, uint32_t event) {
    ProtoPirateApp* a = (ProtoPirateApp*)app;
    if(a) view_dispatcher_send_custom_event(a->view_dispatcher, event);
}

static void host_notification_error(void* app) {
    ProtoPirateApp* a = (ProtoPirateApp*)app;
    if(a) notification_message(a->notifications, &sequence_error);
}

static void host_notification_success(void* app) {
    ProtoPirateApp* a = (ProtoPirateApp*)app;
    if(a) notification_message(a->notifications, &sequence_success);
}

static void host_receiver_info_rebuild_widget(void* app) {
    protopirate_receiver_info_rebuild_normal_widget(app);
}

static void host_saved_info_rebuild_widget(void* app) {
    protopirate_scene_saved_info_on_enter(app);
}

static void host_subdecode_signal_info_refresh(void* app) {
    host_send_custom_event(app, ProtoPirateCustomEventSubDecodeUpdate);
}

static void host_scene_previous(void* app) {
    ProtoPirateApp* a = (ProtoPirateApp*)app;
    if(a) scene_manager_previous_scene(a->scene_manager);
}

static const char* host_get_loaded_file_path(void* app) {
    return ((ProtoPirateApp*)app)->loaded_file_path;
}

static const ProtoPiratePsaBfHostApi protopirate_psa_bf_host_api = {
    .ensure_widget = host_ensure_widget,
    .get_widget = host_get_widget,
    .get_history_flipper_format = host_get_history_flipper_format,
    .get_history_index = host_get_history_index,
    .set_history_index = host_set_history_index,
    .get_history = host_get_history,
    .history_set_item_str = host_history_set_item_str,
    .patch_flipper_format_on_success = host_patch_flipper_format_on_success,
    .send_custom_event = host_send_custom_event,
    .notification_error = host_notification_error,
    .notification_success = host_notification_success,
    .receiver_info_rebuild_widget = host_receiver_info_rebuild_widget,
    .saved_info_rebuild_widget = host_saved_info_rebuild_widget,
    .subdecode_signal_info_refresh = host_subdecode_signal_info_refresh,
    .scene_previous = host_scene_previous,
    .get_loaded_file_path = host_get_loaded_file_path,
};

bool protopirate_psa_bf_plugin_ensure_loaded(ProtoPirateApp* app) {
    if(shared_plugin_load(
           (void**)&app->psa_bf_plugin_flipper_application,
           (const void**)&app->psa_bf_plugin,
           ProtoPirateSharedPluginsPSABruteforce,
           NULL)) {
        app->psa_bf_plugin->set_host_api(&protopirate_psa_bf_host_api);
        return true;
    } else {
        return false;
    }
}
void protopirate_psa_bf_plugin_unload_if_idle(ProtoPirateApp* app) {
    if(!app) return;
    if(app->psa_bf_plugin && app->psa_bf_plugin->is_running &&
       app->psa_bf_plugin->is_running(app)) {
        return;
    }

    shared_plugin_unload(
        (void**)&app->psa_bf_plugin_flipper_application, (const void**)&app->psa_bf_plugin);
}

void protopirate_psa_bf_context_release(ProtoPirateApp* app) {
    if(!app) return;
    if(app->psa_bf_plugin && app->psa_bf_plugin->context_release) {
        app->psa_bf_plugin->context_release(app);
    }
    shared_plugin_unload(
        (void**)&app->psa_bf_plugin_flipper_application, (const void**)&app->psa_bf_plugin);
}
