#include "flippass_scene_other_fields.h"

#include "../flippass.h"
#include "../flippass_db.h"
#include "../plugins/flippass_other_fields_plugin.h"
#include "flippass_db_browser_view.h"
#include "flippass_scene.h"
#include "flippass_scene_editor.h"
#include "flippass_scene_other_field_actions.h"
#include "flippass_scene_status.h"

#include <stdio.h>

enum {
    FlipPassSceneOtherFieldsEventShowSelected = 1,
    FlipPassSceneOtherFieldsEventExecuteUsbAction,
    FlipPassSceneOtherFieldsEventExecuteBluetoothAction,
    FlipPassSceneOtherFieldsEventSelectUsbLayout,
    FlipPassSceneOtherFieldsEventSelectBluetoothLayout,
    FlipPassSceneOtherFieldsEventEditSelected,
    FlipPassSceneOtherFieldsEventRunPendingAction = 0x200U,
};

static void
    flippass_other_fields_trim_for_layout_selection(App* app, FlipPassOutputTransport transport) {
    const FlipPassOutputTransport opposite = (transport == FlipPassOutputTransportBluetooth) ?
                                                 FlipPassOutputTransportUsb :
                                                 FlipPassOutputTransportBluetooth;

    flippass_output_release_all(app);
    flippass_output_cleanup_transport(app, opposite);
}

static void flippass_other_fields_sync_selection_from_view(App* app) {
    furi_assert(app);
    app->other_field_selected_index = flippass_db_browser_view_get_selected_item(app->db_browser);
}

static const FlipPassOtherFieldsPluginV1*
    flippass_other_fields_plugin_load(App* app, FuriString* error) {
    const FlipperAppPluginDescriptor* descriptor = flippass_module_ensure(
        app,
        FlipPassModuleSlotOtherFields,
        NULL,
        FLIPPASS_OTHER_FIELDS_PLUGIN_APP_ID,
        FLIPPASS_OTHER_FIELDS_PLUGIN_API_VERSION,
        error);
    if(descriptor == NULL || descriptor->entry_point == NULL) {
        return NULL;
    }

    const FlipPassOtherFieldsPluginV1* plugin = descriptor->entry_point;
    if(plugin->api_version != FLIPPASS_OTHER_FIELDS_PLUGIN_API_VERSION ||
       plugin->render_type_list == NULL || plugin->render_editor_list == NULL ||
       plugin->select == NULL || plugin->release == NULL) {
        furi_string_set_str(error, "FlipPass other-fields plugin has an incompatible API.");
        return NULL;
    }

    return plugin;
}

static void flippass_other_fields_add_view_item(
    void* context,
    FlipPassOtherFieldsPluginViewItemType type,
    const char* label) {
    App* app = context;
    FlipPassDbBrowserItemType view_type = FlipPassDbBrowserItemTypeInfo;

    switch(type) {
    case FlipPassOtherFieldsPluginViewItemField:
        view_type = FlipPassDbBrowserItemTypeField;
        break;
    case FlipPassOtherFieldsPluginViewItemAdd:
        view_type = FlipPassDbBrowserItemTypeAdd;
        break;
    case FlipPassOtherFieldsPluginViewItemInfo:
    default:
        view_type = FlipPassDbBrowserItemTypeInfo;
        break;
    }

    flippass_db_browser_view_add_item(app->db_browser, view_type, label != NULL ? label : "");
}

static bool flippass_other_fields_select_current_item(App* app) {
    FuriString* error = furi_string_alloc();
    const FlipPassOtherFieldsPluginV1* plugin = flippass_other_fields_plugin_load(app, error);
    FlipPassOtherFieldsSelectionV1 selection = {0};
    bool ok = false;

    if(plugin != NULL) {
        flippass_other_fields_sync_selection_from_view(app);
        ok = plugin->select(app->other_field_selected_index, &selection);
    }

    if(ok) {
        app->pending_other_field_mask = selection.field_mask;
        app->pending_other_custom_field = selection.custom_field;
        app->pending_other_otp_kind = selection.otp_kind;
        snprintf(
            app->pending_other_field_name,
            sizeof(app->pending_other_field_name),
            "%s",
            selection.label);
    }

    furi_string_free(error);
    return ok;
}

static bool flippass_other_fields_render(App* app, const KDBXEntry* entry) {
    FuriString* error = furi_string_alloc();
    const FlipPassOtherFieldsPluginV1* plugin = flippass_other_fields_plugin_load(app, error);
    uint32_t selected_index = app->other_field_selected_index;
    const FlipPassOtherFieldsHostApiV1 host_api = {
        .api_version = FLIPPASS_OTHER_FIELDS_HOST_API_VERSION,
        .context = app,
        .add_item = flippass_other_fields_add_view_item,
    };
    bool ok = false;

    if(plugin == NULL) {
        flippass_scene_status_show(
            app, "Open Failed", furi_string_get_cstr(error), FlipPassScene_DbEntries);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
        furi_string_free(error);
        return false;
    }

    flippass_db_browser_view_reset(app->db_browser);
    flippass_db_browser_view_set_mode(app->db_browser, FlipPassDbBrowserModeDirectActions);
    flippass_db_browser_view_set_has_parent(app->db_browser, false);
    flippass_db_browser_view_set_header(app->db_browser, "Other Fields");

    ok = plugin->render_type_list(entry, selected_index, &host_api, &selected_index, error);
    if(!ok) {
        flippass_scene_status_show(
            app, "Open Failed", furi_string_get_cstr(error), FlipPassScene_DbEntries);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
        furi_string_free(error);
        return false;
    }

    app->other_field_selected_index = selected_index;
    flippass_db_browser_view_set_selected_item(app->db_browser, app->other_field_selected_index);
    furi_string_free(error);
    return true;
}

static bool flippass_other_fields_render_editor(App* app) {
    FuriString* error = furi_string_alloc();
    const FlipPassOtherFieldsPluginV1* plugin = flippass_other_fields_plugin_load(app, error);
    uint32_t selected_index = app->other_field_selected_index;
    const FlipPassOtherFieldsHostApiV1 host_api = {
        .api_version = FLIPPASS_OTHER_FIELDS_HOST_API_VERSION,
        .context = app,
        .add_item = flippass_other_fields_add_view_item,
    };
    bool ok = false;

    if(plugin == NULL) {
        flippass_scene_status_show(
            app, "Open Failed", furi_string_get_cstr(error), FlipPassScene_DbEntries);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
        furi_string_free(error);
        return false;
    }

    flippass_db_browser_view_reset(app->db_browser);
    flippass_db_browser_view_set_mode(app->db_browser, FlipPassDbBrowserModeBrowse);
    flippass_db_browser_view_set_has_parent(app->db_browser, false);
    flippass_db_browser_view_set_header(app->db_browser, "Fields");

    ok = plugin->render_editor_list(
        app->editor_entry != NULL ? app->editor_entry->custom_fields : NULL,
        app->editor_entry == NULL ? app->editor_custom_fields : NULL,
        selected_index,
        &host_api,
        &selected_index,
        error);
    if(!ok) {
        flippass_scene_status_show(
            app, "Open Failed", furi_string_get_cstr(error), FlipPassScene_DbEntries);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
        furi_string_free(error);
        return false;
    }

    app->other_field_selected_index = selected_index;
    flippass_db_browser_view_set_selected_item(app->db_browser, app->other_field_selected_index);
    furi_string_free(error);
    return true;
}

static bool flippass_other_fields_open_editor_item(App* app) {
    FuriString* load_error = furi_string_alloc();
    const FlipPassOtherFieldsPluginV1* plugin = flippass_other_fields_plugin_load(app, load_error);
    FlipPassOtherFieldsSelectionV1 selection = {0};

    flippass_other_fields_sync_selection_from_view(app);
    if(plugin == NULL || !plugin->select(app->other_field_selected_index, &selection)) {
        furi_string_free(load_error);
        return false;
    }
    furi_string_free(load_error);

    if(selection.new_field) {
        flippass_editor_prepare_new_custom_field(app);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Editor);
        return true;
    }

    if(selection.otp_kind != FlipPassOtpKindNone) {
        return false;
    }

    if(selection.custom_field == NULL && selection.draft_field == NULL) {
        return false;
    }

    if(selection.custom_field != NULL) {
        FuriString* error = furi_string_alloc();
        if(!flippass_db_ensure_custom_field(
               app, app->editor_entry, selection.custom_field, error)) {
            flippass_scene_status_show(
                app, "Field Load Failed", furi_string_get_cstr(error), FlipPassScene_OtherFields);
            scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
            furi_string_free(error);
            return true;
        }
        furi_string_free(error);
    }

    flippass_editor_prepare_edit_custom_field(app, selection.custom_field, selection.draft_field);
    scene_manager_next_scene(app->scene_manager, FlipPassScene_Editor);
    return true;
}

static void flippass_other_fields_view_callback(FlipPassDbBrowserEvent event, void* context) {
    App* app = context;
    uint32_t custom_event = 0U;

    switch(event) {
    case FlipPassDbBrowserEventEnter:
        custom_event = FlipPassSceneOtherFieldsEventEditSelected;
        break;
    case FlipPassDbBrowserEventShow:
        custom_event = FlipPassSceneOtherFieldsEventShowSelected;
        break;
    case FlipPassDbBrowserEventTypeUsb:
        custom_event = FlipPassSceneOtherFieldsEventExecuteUsbAction;
        break;
    case FlipPassDbBrowserEventTypeBluetooth:
        custom_event = FlipPassSceneOtherFieldsEventExecuteBluetoothAction;
        break;
    case FlipPassDbBrowserEventTypeUsbLong:
        custom_event = FlipPassSceneOtherFieldsEventSelectUsbLayout;
        break;
    case FlipPassDbBrowserEventTypeBluetoothLong:
        custom_event = FlipPassSceneOtherFieldsEventSelectBluetoothLayout;
        break;
    default:
        break;
    }

    if(custom_event != 0U) {
        view_dispatcher_send_custom_event(app->view_dispatcher, custom_event);
    }
}

void flippass_scene_other_fields_on_enter(void* context) {
    App* app = context;
    const KDBXEntry* entry = app->active_entry;
    const FlipPassOtherFieldsMode mode =
        scene_manager_get_scene_state(app->scene_manager, FlipPassScene_OtherFields);

    flippass_db_browser_view_set_callback(
        app->db_browser, flippass_other_fields_view_callback, app);

    if(mode == FlipPassOtherFieldsModeEdit || mode == FlipPassOtherFieldsModeEditNoAuto) {
        if(flippass_editor_custom_field_count(app) == 0U && mode == FlipPassOtherFieldsModeEdit) {
            scene_manager_set_scene_state(
                app->scene_manager, FlipPassScene_OtherFields, FlipPassOtherFieldsModeEditNoAuto);
            flippass_editor_prepare_new_custom_field(app);
            scene_manager_next_scene(app->scene_manager, FlipPassScene_Editor);
            return;
        }

        if(flippass_other_fields_render_editor(app)) {
            view_dispatcher_switch_to_view(app->view_dispatcher, AppViewDbBrowser);
        }
        return;
    }

    if(entry == NULL) {
        flippass_scene_status_show(
            app,
            "No Entry Selected",
            "Return to the browser and pick an entry first.",
            FlipPassScene_DbEntries);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
        return;
    }

    if(flippass_other_fields_render(app, entry)) {
        view_dispatcher_switch_to_view(app->view_dispatcher, AppViewDbBrowser);
    }
}

bool flippass_scene_other_fields_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    const FlipPassOtherFieldsMode mode =
        scene_manager_get_scene_state(app->scene_manager, FlipPassScene_OtherFields);

    if(event.type == SceneManagerEventTypeCustom) {
        if(mode == FlipPassOtherFieldsModeEdit || mode == FlipPassOtherFieldsModeEditNoAuto) {
            if(event.event == FlipPassSceneOtherFieldsEventEditSelected) {
                return flippass_other_fields_open_editor_item(app);
            }
            return true;
        }

        if(!flippass_other_fields_select_current_item(app)) {
            return true;
        }

        if(event.event == FlipPassSceneOtherFieldsEventShowSelected) {
            app->other_field_action_selected_index = 1U;
            flippass_other_field_show_selected_value(app, FlipPassScene_OtherFields);
            return true;
        }

        if(event.event == FlipPassSceneOtherFieldsEventExecuteUsbAction) {
            flippass_other_field_begin_type_action(
                app, false, FlipPassSceneOtherFieldsEventRunPendingAction);
            return true;
        }

        if(event.event == FlipPassSceneOtherFieldsEventExecuteBluetoothAction) {
            flippass_other_field_begin_type_action(
                app, true, FlipPassSceneOtherFieldsEventRunPendingAction);
            return true;
        }

        if(event.event == FlipPassSceneOtherFieldsEventSelectUsbLayout) {
            app->other_field_action_selected_index = 2U;
            app->pending_entry_action = FlipPassEntryActionTypeOtherUsb;
            app->keyboard_layout_return_scene = FlipPassScene_OtherFields;
            flippass_other_fields_trim_for_layout_selection(app, FlipPassOutputTransportUsb);
            scene_manager_next_scene(app->scene_manager, FlipPassScene_KeyboardLayout);
            return true;
        }

        if(event.event == FlipPassSceneOtherFieldsEventSelectBluetoothLayout) {
            app->other_field_action_selected_index = 0U;
            app->pending_entry_action = FlipPassEntryActionTypeOtherBluetooth;
            app->keyboard_layout_return_scene = FlipPassScene_OtherFields;
            flippass_other_fields_trim_for_layout_selection(app, FlipPassOutputTransportBluetooth);
            scene_manager_next_scene(app->scene_manager, FlipPassScene_KeyboardLayout);
            return true;
        }

        if(event.event == FlipPassSceneOtherFieldsEventRunPendingAction) {
            flippass_other_field_run_pending_type_action(app, FlipPassScene_OtherFields);
            return true;
        }
    }

    if(event.type == SceneManagerEventTypeBack) {
        if(mode == FlipPassOtherFieldsModeEdit || mode == FlipPassOtherFieldsModeEditNoAuto) {
            app->editor_selected_index = FlipPassEditorEntryRowOtherFields;
        }
        scene_manager_set_scene_state(
            app->scene_manager, FlipPassScene_OtherFields, FlipPassOtherFieldsModeType);
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }

    return false;
}

void flippass_scene_other_fields_on_exit(void* context) {
    App* app = context;
    const FlipperAppPluginDescriptor* descriptor =
        app->module_loader.slot[FlipPassModuleSlotOtherFields].descriptor;

    flippass_other_fields_sync_selection_from_view(app);
    flippass_db_browser_view_set_action_menu_open(app->db_browser, false);
    flippass_output_release_all(app);
    flippass_output_cleanup(app);
    if(descriptor != NULL && descriptor->entry_point != NULL) {
        const FlipPassOtherFieldsPluginV1* plugin = descriptor->entry_point;
        plugin->release();
    }
    flippass_module_unload(app, FlipPassModuleSlotOtherFields);
}
