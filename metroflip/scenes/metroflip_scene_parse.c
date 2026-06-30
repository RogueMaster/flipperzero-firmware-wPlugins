#include "../metroflip_i.h"
#include <furi.h>
#include "../metroflip_plugins.h"
#include "../api/metroflip/metroflip_api.h"
#define TAG "Metroflip:Scene:Parse"
#include <stdio.h>

void metroflip_scene_parse_on_enter(void* context) {
    Metroflip* app = context;
    
    FURI_LOG_I(TAG, "Parse scene entered - card_type: %s, data_loaded: %s", 
               app->card_type ? app->card_type : "NULL", 
               app->data_loaded ? "true" : "false");

    if(!app->card_type ||
   (app->card_type[0] == '\0') ||
   (strcmp(app->card_type, "unknown") == 0) ||
   (strcmp(app->card_type, "Unknown Card") == 0) ||
   (app->is_desfire && is_desfire_locked(app->card_type))) {
        FURI_LOG_I(TAG, "Bad card condition met - sending wrong card event");
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventWrongCard);
    } else {
        if((strcmp(app->card_type, "atr") == 0) && app->hist_bytes_count > 0) {
            FURI_LOG_I(TAG, "Tag is either T-Mobilitat or T-Money");
            if(app->hist_bytes[0] == 0x2A && app->hist_bytes[1] == 0x26){
                FURI_LOG_I(TAG, "Card is T-Mobilitat");
                app->card_type = "tmobilitat";
                
            } else if (app->hist_bytes[0] == 0x04 && app->hist_bytes[1] == 0x09) {
                FURI_LOG_I(TAG, "Card is T-Money");

                app->card_type = "tmoney"; 
                //for now we blank out the line above as it's not merged yet
                view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventWrongCard);
            } else {
                view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventWrongCard);
            }
        }
        FURI_LOG_I(TAG, "Card is valid, loading plugin for: %s", app->card_type);
        metroflip_plugin_manager_alloc(app);
        char path[128]; // Adjust size as needed
        snprintf(
            path, sizeof(path), "/ext/apps_assets/metroflip/plugins/%s_plugin.fal", app->card_type);

        FURI_LOG_I(TAG, "Plugin path: %s", path);

        // Try loading the plugin
        if(plugin_manager_load_single(app->plugin_manager, path) != PluginManagerErrorNone) {
            FURI_LOG_E(TAG, "Failed to load parse plugin");
            return;
        }

        // Get and run the plugin's on_enter function
        const MetroflipPlugin* plugin = plugin_manager_get_ep(app->plugin_manager, 0);
        plugin->plugin_on_enter(app);
    }
}

bool metroflip_scene_parse_on_event(void* context, SceneManagerEvent event) {
    Metroflip* app = context;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MetroflipCustomEventWrongCard) {
            FURI_LOG_I(TAG, "Wrong card event received - switching to unknown scene");
            scene_manager_next_scene(app->scene_manager, MetroflipSceneUnknown);
            return true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        FURI_LOG_I(TAG, "Back event received - returning to start");
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, MetroflipSceneStart);
        return true;
    }

    // Get and run the plugin's on_event function
    const MetroflipPlugin* plugin = plugin_manager_get_ep(app->plugin_manager, 0);
    return plugin->plugin_on_event(app, event);
}

void metroflip_scene_parse_on_exit(void* context) {
    Metroflip* app = context;
    if(!(!app->card_type ||
   (app->card_type[0] == '\0') ||
   (strcmp(app->card_type, "unknown") == 0) ||
   (strcmp(app->card_type, "Unknown Card") == 0) ||
   (app->is_desfire && is_desfire_locked(app->card_type)))) {
        // Get and run the plugin's on_exit function
        const MetroflipPlugin* plugin = plugin_manager_get_ep(app->plugin_manager, 0);
        plugin->plugin_on_exit(app);

        plugin_manager_free(app->plugin_manager);
        composite_api_resolver_free(app->resolver);
        app->is_desfire = false;
    }
    
    app->data_loaded = false;
}
