#include "../protopirate_app_i.h"
#include "protopirate_psa_bf_host.h"
#include "../protopirate_history.h"
#include "../protocols/protocols_common.h"
#include "../scenes/plugins/protopirate_psa_bf_plugin.h"

#include <loader/firmware_api/firmware_api.h>
#include <lib/flipper_application/plugins/plugin_manager.h>
#include <lib/flipper_application/plugins/composite_resolver.h>
#include <notification/notification_messages.h>

#define TAG                "ProtoPiratePsaBfHost"
#define PSA_BF_PLUGIN_PATH APP_ASSETS_PATH("plugins/protopirate_psa_bf_plugin.fal")

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

#ifdef ENABLE_SUB_DECODE_SCENE
static void host_subdecode_signal_info_refresh(void* app) {
    protopirate_subdecode_psa_bf_complete_refresh(app);
}
#else
static void host_subdecode_signal_info_refresh(void* app) {
    UNUSED(app);
}
#endif

static void host_scene_previous(void* app) {
    ProtoPirateApp* a = (ProtoPirateApp*)app;
    if(a) scene_manager_previous_scene(a->scene_manager);
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
    .subdecode_signal_info_refresh = host_subdecode_signal_info_refresh,
    .scene_previous = host_scene_previous,
};

static void psa_bf_plugin_unload(ProtoPirateApp* app) {
    furi_check(app);
    app->psa_bf_plugin = NULL;

    if(app->psa_bf_plugin_manager) {
        plugin_manager_free(app->psa_bf_plugin_manager);
        app->psa_bf_plugin_manager = NULL;
    }

    if(app->psa_bf_plugin_resolver) {
        composite_api_resolver_free(app->psa_bf_plugin_resolver);
        app->psa_bf_plugin_resolver = NULL;
    }
}

bool protopirate_psa_bf_plugin_ensure_loaded(ProtoPirateApp* app) {
    furi_check(app);

    if(app->psa_bf_plugin) return true;

    if(app->psa_bf_plugin_manager || app->psa_bf_plugin_resolver) {
        psa_bf_plugin_unload(app);
    }

    CompositeApiResolver* resolver = composite_api_resolver_alloc();
    if(!resolver) {
        FURI_LOG_E(TAG, "Failed to allocate PSA BF plugin resolver");
        return false;
    }
    composite_api_resolver_add(resolver, firmware_api_interface);

    PluginManager* manager = plugin_manager_alloc(
        PROTOPIRATE_PSA_BF_PLUGIN_APP_ID,
        PROTOPIRATE_PSA_BF_PLUGIN_API_VERSION,
        composite_api_resolver_get(resolver));
    if(!manager) {
        FURI_LOG_E(TAG, "Failed to allocate PSA BF plugin manager");
        composite_api_resolver_free(resolver);
        return false;
    }

    PluginManagerError error = plugin_manager_load_single(manager, PSA_BF_PLUGIN_PATH);
    if(error != PluginManagerErrorNone) {
        FURI_LOG_E(TAG, "Failed to load PSA BF plugin %s: %d", PSA_BF_PLUGIN_PATH, (int)error);
        plugin_manager_free(manager);
        composite_api_resolver_free(resolver);
        return false;
    }

    const ProtoPiratePsaBfPlugin* plugin = plugin_manager_get_ep(manager, 0U);
    if(!plugin || !plugin->set_host_api || !plugin->needs_bruteforce || !plugin->on_scene_event) {
        FURI_LOG_E(TAG, "PSA BF plugin entry point is invalid");
        plugin_manager_free(manager);
        composite_api_resolver_free(resolver);
        return false;
    }

    app->psa_bf_plugin_resolver = resolver;
    app->psa_bf_plugin_manager = manager;
    app->psa_bf_plugin = plugin;
    plugin->set_host_api(&protopirate_psa_bf_host_api);
    return true;
}

void protopirate_psa_bf_plugin_unload_if_idle(ProtoPirateApp* app) {
    if(!app) return;
    if(app->psa_bf_plugin && app->psa_bf_plugin->is_running &&
       app->psa_bf_plugin->is_running(app)) {
        return;
    }
    psa_bf_plugin_unload(app);
}

void protopirate_psa_bf_context_release(ProtoPirateApp* app) {
    if(!app) return;
    if(app->psa_bf_plugin && app->psa_bf_plugin->context_release) {
        app->psa_bf_plugin->context_release(app);
    }
    psa_bf_plugin_unload(app);
}
