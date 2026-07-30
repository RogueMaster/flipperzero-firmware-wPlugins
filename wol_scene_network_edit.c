#include "wol_flipper.h"

#include <string.h>

typedef enum {
    NetworkIndexSsid,
    NetworkIndexPassword,
    NetworkIndexSave,
    NetworkIndexDelete,
} NetworkIndex;

static char network_label_ssid[WOL_SSID_LEN + 8];
static char network_label_pass[24];
static char network_header[32];

static void wol_scene_network_edit_callback(void* context, uint32_t index) {
    WolApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void wol_scene_network_edit_build(WolApp* app, const char* header) {
    snprintf(
        network_label_ssid,
        sizeof(network_label_ssid),
        "SSID: %s",
        app->edit_network.ssid[0] ? app->edit_network.ssid : "<unset>");

    size_t pass_len = strlen(app->edit_network.pass);
    if(pass_len == 0) {
        wol_strcpy(network_label_pass, sizeof(network_label_pass), "Key: none, open");
    } else {
        if(pass_len > 8) pass_len = 8;
        char stars[9];
        memset(stars, '*', pass_len);
        stars[pass_len] = '\0';
        snprintf(network_label_pass, sizeof(network_label_pass), "Key: %s", stars);
    }

    wol_strcpy(network_header, sizeof(network_header), header);

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, network_header);
    submenu_add_item(
        app->submenu, network_label_ssid, NetworkIndexSsid, wol_scene_network_edit_callback, app);
    submenu_add_item(
        app->submenu,
        network_label_pass,
        NetworkIndexPassword,
        wol_scene_network_edit_callback,
        app);
    submenu_add_item(app->submenu, "Save", NetworkIndexSave, wol_scene_network_edit_callback, app);
    if(!app->network_is_new) {
        submenu_add_item(
            app->submenu, "Delete", NetworkIndexDelete, wol_scene_network_edit_callback, app);
    }

    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, WolSceneNetworkEdit));
}

void wol_scene_network_edit_on_enter(void* context) {
    WolApp* app = context;
    wol_scene_network_edit_build(app, app->network_is_new ? "New network" : "Edit network");
    view_dispatcher_switch_to_view(app->view_dispatcher, WolViewSubmenu);
}

bool wol_scene_network_edit_on_event(void* context, SceneManagerEvent event) {
    WolApp* app = context;

    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event > NetworkIndexDelete) return false;

    scene_manager_set_scene_state(app->scene_manager, WolSceneNetworkEdit, event.event);

    switch(event.event) {
    case NetworkIndexSsid:
        app->text_field = WolTextFieldSsid;
        wol_strcpy(app->text_buf, sizeof(app->text_buf), app->edit_network.ssid);
        scene_manager_next_scene(app->scene_manager, WolSceneTextInput);
        return true;

    case NetworkIndexPassword:
        app->text_field = WolTextFieldPassword;
        wol_strcpy(app->text_buf, sizeof(app->text_buf), app->edit_network.pass);
        scene_manager_next_scene(app->scene_manager, WolSceneTextInput);
        return true;

    case NetworkIndexSave:
        if(app->edit_network.ssid[0] == '\0') {
            wol_scene_network_edit_build(app, "Set an SSID first");
            return true;
        }

        if(app->network_is_new) {
            if(app->config.network_count >= WOL_MAX_NETWORKS) {
                wol_scene_network_edit_build(app, "Network list is full");
                return true;
            }
            /* Adding an SSID that is already saved edits that entry instead of
             * producing a duplicate the picker could never tell apart. */
            uint8_t existing = wol_config_find_network(&app->config, app->edit_network.ssid);
            app->network_index = (existing < WOL_MAX_NETWORKS) ? existing :
                                                                 app->config.network_count++;
        }
        app->config.networks[app->network_index] = app->edit_network;
        wol_config_save(&app->config);
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, WolSceneWifi);
        return true;

    case NetworkIndexDelete:
        for(size_t i = app->network_index; i + 1 < app->config.network_count; i++) {
            app->config.networks[i] = app->config.networks[i + 1];
        }
        if(app->config.network_count) app->config.network_count--;
        memset(&app->config.networks[app->config.network_count], 0, sizeof(WolNetwork));
        wol_config_save(&app->config);
        scene_manager_set_scene_state(app->scene_manager, WolSceneWifi, 0);
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, WolSceneWifi);
        return true;

    default:
        return false;
    }
}

void wol_scene_network_edit_on_exit(void* context) {
    WolApp* app = context;
    submenu_reset(app->submenu);
}
