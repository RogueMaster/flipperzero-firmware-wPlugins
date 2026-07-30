#include "wol_flipper.h"

#include <string.h>

typedef enum {
    EditIndexName,
    EditIndexMac,
    EditIndexIp,
    EditIndexPort,
    EditIndexSave,
    EditIndexDelete,
} EditIndex;

static char edit_label_name[WOL_NAME_LEN + 8];
static char edit_label_mac[32];
static char edit_label_ip[WOL_IP_LEN + 8];
static char edit_label_port[16];
static char edit_header[32];

static void wol_scene_target_edit_callback(void* context, uint32_t index) {
    WolApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static bool wol_target_mac_is_empty(const WolTarget* target) {
    for(size_t i = 0; i < 6; i++) {
        if(target->mac[i] != 0) return false;
    }
    return true;
}

static void wol_scene_target_edit_build(WolApp* app, const char* header) {
    char mac[18];
    wol_mac_to_str(app->edit.mac, mac, sizeof(mac));

    snprintf(
        edit_label_name,
        sizeof(edit_label_name),
        "Name: %s",
        app->edit.name[0] ? app->edit.name : "<unset>");
    snprintf(edit_label_mac, sizeof(edit_label_mac), "MAC: %s", mac);
    // labelled as a broadcast on purpose: putting the target's own address here
    // silently does nothing, since a sleeping host answers no ARP
    snprintf(edit_label_ip, sizeof(edit_label_ip), "Bcast: %s", app->edit.ip);
    snprintf(edit_label_port, sizeof(edit_label_port), "Port: %u", app->edit.port);

    wol_strcpy(edit_header, sizeof(edit_header), header);

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, edit_header);
    submenu_add_item(
        app->submenu, edit_label_name, EditIndexName, wol_scene_target_edit_callback, app);
    submenu_add_item(
        app->submenu, edit_label_mac, EditIndexMac, wol_scene_target_edit_callback, app);
    submenu_add_item(
        app->submenu, edit_label_ip, EditIndexIp, wol_scene_target_edit_callback, app);
    submenu_add_item(
        app->submenu, edit_label_port, EditIndexPort, wol_scene_target_edit_callback, app);
    submenu_add_item(app->submenu, "Save", EditIndexSave, wol_scene_target_edit_callback, app);
    if(!app->edit_is_new) {
        submenu_add_item(
            app->submenu, "Delete", EditIndexDelete, wol_scene_target_edit_callback, app);
    }

    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, WolSceneTargetEdit));
}

void wol_scene_target_edit_on_enter(void* context) {
    WolApp* app = context;
    wol_scene_target_edit_build(app, app->edit_is_new ? "New target" : "Edit target");
    view_dispatcher_switch_to_view(app->view_dispatcher, WolViewSubmenu);
}

bool wol_scene_target_edit_on_event(void* context, SceneManagerEvent event) {
    WolApp* app = context;

    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event > EditIndexDelete) return false;

    scene_manager_set_scene_state(app->scene_manager, WolSceneTargetEdit, event.event);

    switch(event.event) {
    case EditIndexName:
        app->text_field = WolTextFieldName;
        wol_strcpy(app->text_buf, sizeof(app->text_buf), app->edit.name);
        scene_manager_next_scene(app->scene_manager, WolSceneTextInput);
        return true;

    case EditIndexMac:
        scene_manager_next_scene(app->scene_manager, WolSceneByteInput);
        return true;

    case EditIndexIp:
        app->text_field = WolTextFieldIp;
        wol_strcpy(app->text_buf, sizeof(app->text_buf), app->edit.ip);
        scene_manager_next_scene(app->scene_manager, WolSceneTextInput);
        return true;

    case EditIndexPort:
        // the only ports anything listens on for WoL
        if(app->edit.port == 9) {
            app->edit.port = 7;
        } else if(app->edit.port == 7) {
            app->edit.port = 0;
        } else {
            app->edit.port = 9;
        }
        wol_scene_target_edit_build(app, app->edit_is_new ? "New target" : "Edit target");
        return true;

    case EditIndexSave:
        if(wol_target_mac_is_empty(&app->edit)) {
            wol_scene_target_edit_build(app, "Set a MAC first");
            return true;
        }
        if(!wol_ip_is_valid(app->edit.ip)) {
            wol_scene_target_edit_build(app, "Bad broadcast IP");
            return true;
        }
        if(app->edit.name[0] == '\0') {
            wol_mac_to_str(app->edit.mac, app->edit.name, WOL_NAME_LEN);
        }

        if(app->edit_is_new) {
            if(app->config.target_count >= WOL_MAX_TARGETS) {
                wol_scene_target_edit_build(app, "Target list is full");
                return true;
            }
            app->target_index = app->config.target_count++;
        }
        app->config.targets[app->target_index] = app->edit;
        wol_config_save(&app->config);
        scene_manager_previous_scene(app->scene_manager);
        return true;

    case EditIndexDelete:
        for(size_t i = app->target_index; i + 1 < app->config.target_count; i++) {
            app->config.targets[i] = app->config.targets[i + 1];
        }
        if(app->config.target_count) app->config.target_count--;
        memset(&app->config.targets[app->config.target_count], 0, sizeof(WolTarget));
        wol_config_save(&app->config);
        scene_manager_set_scene_state(app->scene_manager, WolSceneTargets, 0);
        scene_manager_previous_scene(app->scene_manager);
        return true;

    default:
        return false;
    }
}

void wol_scene_target_edit_on_exit(void* context) {
    WolApp* app = context;
    submenu_reset(app->submenu);
}
