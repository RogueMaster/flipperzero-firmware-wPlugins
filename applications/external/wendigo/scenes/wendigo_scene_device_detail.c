#include "../wendigo_app_i.h"
#include "../wendigo_scan.h"

wendigo_device* device = NULL;

void wendigo_scene_device_detail_set_device(wendigo_device* d) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_device_detail_set_device()");
    device = d;
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_device_detail_set_device()");
}

static void wendigo_scene_device_detail_var_list_enter_callback(void* context, uint32_t index) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_device_detail_var_list_enter_callback()");
    furi_assert(context);
    WendigoApp* app = context;

    //    furi_assert(index < ((display_selected_only) ? bt_selected_devices_count : bt_devices_count));

    UNUSED(app);
    UNUSED(index);

    // TODO: something

    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_device_detail_var_list_enter_callback()");
}

static void wendigo_scene_device_detail_var_list_change_callback(VariableItem* item) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_device_detail_var_list_change_callback()");
    furi_assert(item);

    WendigoApp* app = variable_item_get_context(item);
    furi_assert(app);

    uint8_t item_index = variable_item_get_current_value_index(item);
    UNUSED(item_index);

    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_device_detail_var_list_change_callback()");
}

void wendigo_scene_device_detail_on_enter(void* context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_device_detail_on_enter()");
    WendigoApp* app = context;
    VariableItemList* var_item_list = app->detail_var_item_list;
    app->current_view = WendigoAppViewDeviceDetail;

    variable_item_list_set_enter_callback(
        var_item_list, wendigo_scene_device_detail_var_list_enter_callback, app);

    variable_item_list_reset(var_item_list);
    VariableItem* item;
    // TODO: Populate with device details
    item = variable_item_list_add(
        var_item_list, "foo", 1, wendigo_scene_device_detail_var_list_change_callback, app);
    UNUSED(item);

    variable_item_list_set_selected_item(
        var_item_list,
        scene_manager_get_scene_state(app->scene_manager, WendigoSceneDeviceDetail));

    view_dispatcher_switch_to_view(app->view_dispatcher, WendigoAppViewDeviceDetail);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_device_detail_on_enter()");
}

bool wendigo_scene_device_detail_on_event(void* context, SceneManagerEvent event) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_device_detail_on_event()");
    WendigoApp* app = context;
    bool consumed = false;
    UNUSED(app);

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case Wendigo_EventListDevices:
            // scene_manager_set_scene_state(
            //     app->scene_manager, WendigoSceneDeviceDetail, app->device_detail_selected_menu_index);
            //scene_manager_next_scene(app->scene_manager, WendigoSceneDeviceDetail);
            break;
        default:
            // TODO: Panic
            break;
        }
        consumed = true;
    } else if(event.type == SceneManagerEventTypeTick) {
        // app->device_detail_selected_menu_index =
        //     variable_item_list_get_selected_item_index(app->detail_var_item_list);
        consumed = true;
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_device_detail_on_event()");
    return consumed;
}

void wendigo_scene_device_detail_on_exit(void* context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_device_detail_on_exit()");
    WendigoApp* app = context;
    variable_item_list_reset(app->detail_var_item_list);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_device_detail_on_exit()");
}
