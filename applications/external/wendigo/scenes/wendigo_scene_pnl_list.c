#include "../wendigo_app_i.h"
#include "../wendigo_scan.h"

wendigo_device* current_device = NULL;

/** When displaying all probed networks this function handles the display of
 * a device list when an SSID is selected.
 */
static void wendigo_scene_pnl_list_var_list_enter_callback(void* context, uint32_t index) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scen_pnl_list_var_list_enter_callback()");
    /* If we're displaying PNL for a specific device this function has no effect */
    if(current_device != NULL && current_device->scanType == SCAN_WIFI_STA) {
        FURI_LOG_T(
            WENDIGO_TAG,
            "End wendigo_scen_pnl_list_var_list_enter_callback() - current_device is valid.");
        return;
    }
    WendigoApp* app = (WendigoApp*)context;
    if(index >= networks_count) {
        /* Index out of bounds - Panic and quit */
        char* msg = malloc(sizeof(char) * 115);
        if(msg == NULL) {
            wendigo_log(
                MSG_ERROR,
                "wendigo_scene_pnl_list_var_list_enter_callback() called with index out of bounds.");
            wendigo_display_popup(
                app,
                "Index out of bounds",
                "wendigo_scene_pnl_list_var_list_enter_callback() called with index out of bounds.");
        } else {
            snprintf(
                msg,
                115,
                "wendigo_scene_pnl_list_var_list_enter_callback(): Called with index %ld which is out of bounds of networks[%d].",
                index,
                networks_count);
            wendigo_log(MSG_ERROR, msg);
            wendigo_display_popup(app, "Index out of bounds", msg);
            free(msg);
        }
        FURI_LOG_T(
            WENDIGO_TAG,
            "End wendigo_scen_pnl_list_var_list_enter_callback() - index out of bounds.");
        return;
    }
    /* Display a device list for networks[index].devices */
    DeviceListInstance deviceList;
    bzero(&deviceList, sizeof(DeviceListInstance));
    strncpy(deviceList.devices_msg, networks[index].ssid, MAX_SSID_LEN);
    deviceList.devices_mask = DEVICE_CUSTOM;
    deviceList.free_devices = false;
    deviceList.view = WendigoAppViewPNLDeviceList;
    deviceList.devices = networks[index].devices;
    deviceList.devices_count = networks[index].device_count;
    wendigo_scene_device_list_set_current_devices(&deviceList);
    /* Save selected menu item index so it can be restored later */
    scene_manager_set_scene_state(app->scene_manager, WendigoScenePNLList, index);
    scene_manager_next_scene(app->scene_manager, WendigoSceneDeviceList);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scen_pnl_list_var_list_enter_callback()");
}

void wendigo_scene_pnl_list_redraw_sta(WendigoApp* app) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_pnl_list_redraw_sta()");
    VariableItemList* var_item_list = app->var_item_list;

    if(current_device == NULL || current_device->scanType != SCAN_WIFI_STA) {
        FURI_LOG_T(
            WENDIGO_TAG, "End wendigo_scene_pnl_list_redraw_sta() - Invalid current_device.");
        return;
    }
    VariableItem* item;
    if(current_device->radio.sta.saved_networks_count == 0 ||
       current_device->radio.sta.saved_networks == NULL) {
        item = variable_item_list_add(var_item_list, "No networks found", 1, NULL, app);
        variable_item_set_current_value_index(item, 0);
        variable_item_set_current_value_text(item, "");
        FURI_LOG_T(
            WENDIGO_TAG, "End wendigo_scene_pnl_list_redraw_sta() - No networks to display.");
        return;
    }
    /* Display the MAC as the list header. Shame we can't make this text smaller. */
    char macStr[MAC_STRLEN + 1];
    bytes_to_string(current_device->mac, MAC_BYTES, macStr);
    variable_item_list_set_header(var_item_list, macStr);
    for(uint8_t i = 0; i < current_device->radio.sta.saved_networks_count; ++i) {
        if(current_device->radio.sta.saved_networks[i] != NULL) {
            item = variable_item_list_add(
                var_item_list, current_device->radio.sta.saved_networks[i], 1, NULL, app);
            variable_item_set_current_value_index(item, 0);
            variable_item_set_current_value_text(item, "");
        }
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_pnl_list_redraw_sta()");
}

void wendigo_scene_pnl_list_redraw_all_devices(WendigoApp* app) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_pnl_list_redraw_all_devices()");
    /* Initialise or update networks[] prior to display */
    map_ssids_to_devices(app);
    VariableItem* item;
    if(networks_count == 0 || networks == NULL) {
        item = variable_item_list_add(app->var_item_list, "No networks found", 1, NULL, app);
        variable_item_set_current_value_index(item, 0);
        variable_item_set_current_value_text(item, "");
        FURI_LOG_T(
            WENDIGO_TAG,
            "End wendigo_scene_pnl_list_redraw_all_devices() - No networks to display.");
        return;
    }
    /* Set the list header */
    variable_item_list_set_header(app->var_item_list, "Probed Networks");

    /* Display networks[] */
    char countStr[9];
    for(uint16_t idx = 0; idx < networks_count; ++idx) {
        /* Add networks[idx].ssid with option networks[idx].device_count */
        snprintf(
            countStr,
            sizeof(countStr),
            "%d STA%s",
            networks[idx].device_count,
            (networks[idx].device_count == 1) ? "" : "s");
        item = variable_item_list_add(app->var_item_list, networks[idx].ssid, 1, NULL, app);
        variable_item_set_current_value_index(item, 0);
        variable_item_set_current_value_text(item, countStr);
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_pnl_list_redraw_all_devices()");
}

void wendigo_scene_pnl_list_redraw(WendigoApp* app) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_pnl_list_redraw()");
    if(current_device != NULL && current_device->scanType == SCAN_WIFI_STA) {
        FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_pnl_list_redraw() - Displaying STA.");
        return wendigo_scene_pnl_list_redraw_sta(app);
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_pnl_list_redraw() - Displaying all devices.");
    return wendigo_scene_pnl_list_redraw_all_devices(app);
}

void wendigo_scene_pnl_list_set_device(wendigo_device* dev, WendigoApp* app) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_pnl_list_set_device()");
    current_device = dev;
    /* Redraw the list if this scene is active */
    // if (app->current_view == WendigoAppViewPNLList) {
    //     FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_pnl_list_set_device() - Redrawing.");
    //     return wendigo_scene_pnl_list_redraw(app);
    // }
    UNUSED(app);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_pnl_list_set_device()");
}

wendigo_device* wendigo_scene_pnl_list_get_device() {
    FURI_LOG_T(WENDIGO_TAG, "Start+End wendigo_scene_pnl_list_get_device()");
    return current_device;
}

/** Scene initialisation. */
void wendigo_scene_pnl_list_on_enter(void* context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_pnl_list_on_enter()");
    WendigoApp* app = context;
    app->current_view = WendigoAppViewPNLList;
    variable_item_list_reset(app->var_item_list);
    variable_item_list_set_header(app->var_item_list, NULL);
    /* Set enter callback */
    variable_item_list_set_enter_callback(
        app->var_item_list, wendigo_scene_pnl_list_var_list_enter_callback, app);

    wendigo_scene_pnl_list_redraw(app);

    /* Restore selected menu item if it's there */
    uint8_t selected_item = scene_manager_get_scene_state(app->scene_manager, WendigoScenePNLList);
    if(selected_item >= networks_count) {
        selected_item = 0;
    }
    variable_item_list_set_selected_item(app->var_item_list, selected_item);
    view_dispatcher_switch_to_view(app->view_dispatcher, WendigoAppViewVarItemList);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_pnl_list_on_enter()");
}

/** We have no need to respond to events */
bool wendigo_scene_pnl_list_on_event(void* context, SceneManagerEvent event) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_pnl_list_on_event()");
    bool consumed = false;
    if(event.type == SceneManagerEventTypeTick) {
        consumed = true;
    }
    UNUSED(context);
    return consumed;
}

void wendigo_scene_pnl_list_on_exit(void* context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_pnl_list_on_exit()");
    WendigoApp* app = context;
    variable_item_list_reset(app->var_item_list);
    /* If we hit the back button in this scene, set current_device to NULL */
    wendigo_scene_pnl_list_set_device(NULL, app);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_pnl_list_on_exit()");
}

void wendigo_scene_pnl_list_free() {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_pnl_list_free()");
    if(networks_count > 0 && networks != NULL) {
        for(uint16_t i = 0; i < networks_count; ++i) {
            if(networks[i].device_count > 0 && networks[i].devices != NULL) {
                /* Free networks[i].devices - a wendigo_device** - but don't
                 * free the individual wendigo_device elements. networks[i].devices
                 * should contain pointers to the master device cache devices[].
                 */
                free(networks[i].devices);
                networks[i].devices = NULL;
                networks[i].device_count = 0;
            }
        }
        free(networks);
    }
    networks_count = 0;
    networks = NULL;
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_pnl_list_free()");
}
