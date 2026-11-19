#include "subghz_scene_plugin.h"

#include "../subghz_i.h"
#include "subghz_custom_event.h"
#include "subghz_feature_plugin.h"

#define SUBGHZ_SETTINGS_PLUGIN_DIR EXT_PATH("apps_data/subghz/plugins/")

static void subghz_scene_plugin_enter_stub(void* context, uint32_t index) {
    UNUSED(context);
    UNUSED(index);
}

static void subghz_scene_plugin_on_enter(SubGhz* subghz, const char* path) {
    furi_assert(!subghz->scene_plugin_manager);

    subghz->scene_plugin_manager =
        subghz_feature_plugin_load(subghz, SUBGHZ_SCENE_PLUGIN_APP_ID, path);
    if(subghz->scene_plugin_manager) {
        subghz->scene_plugin = plugin_manager_get_ep(subghz->scene_plugin_manager, 0);
        subghz->scene_plugin->on_enter(subghz);
    } else {
        furi_string_set(
            subghz->error_str, "Settings plugin\nis missing or\noutdated. Update\nresources.");
        view_dispatcher_send_custom_event(
            subghz->view_dispatcher, SubGhzCustomEventSceneSettingsMissing);
    }
}

static bool subghz_scene_plugin_on_event(void* context, SceneManagerEvent event) {
    SubGhz* subghz = context;
    if(!subghz->scene_plugin) {
        if(event.type == SceneManagerEventTypeCustom &&
           event.event == SubGhzCustomEventSceneSettingsMissing) {
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneShowError);
            return true;
        }
        return false;
    }

    bool consumed = subghz->scene_plugin->on_event(subghz, event);
    if(consumed && event.type == SceneManagerEventTypeCustom &&
       event.event == SubGhzCustomEventByteInputDone) {
        // The transition unmaps the plugin, so it must run after the plugin handler returns.
        scene_manager_previous_scene(subghz->scene_manager);
    }
    return consumed;
}

void subghz_scene_plugin_unload(SubGhz* subghz) {
    if(subghz->scene_plugin) {
        subghz->scene_plugin->on_exit(subghz);
        subghz->scene_plugin = NULL;
        // Reset the list's enter callback, which outlives variable_item_list_reset.
        variable_item_list_set_enter_callback(
            subghz->variable_item_list, subghz_scene_plugin_enter_stub, NULL);
    }
    subghz_feature_plugin_unload(subghz, subghz->scene_plugin_manager);
    subghz->scene_plugin_manager = NULL;
}

static void subghz_scene_plugin_on_exit(void* context) {
    subghz_scene_plugin_unload(context);
}

// Scene subghz_scene_<name> forwarded to subghz_<name>.fal. Only on_enter differs per scene, so
// the scene table takes the shared event and exit handlers directly.
#define SUBGHZ_SCENE_PLUGIN(name)                                                                 \
    void subghz_scene_##name##_on_enter(void* context) {                                          \
        subghz_scene_plugin_on_enter(context, SUBGHZ_SETTINGS_PLUGIN_DIR "subghz_" #name ".fal"); \
    }                                                                                             \
    bool subghz_scene_##name##_on_event(void* context, SceneManagerEvent event)                   \
        __attribute__((alias("subghz_scene_plugin_on_event")));                                   \
    void subghz_scene_##name##_on_exit(void* context)                                             \
        __attribute__((alias("subghz_scene_plugin_on_exit")));

SUBGHZ_SCENE_PLUGIN(signal_settings)
SUBGHZ_SCENE_PLUGIN(radio_settings)
