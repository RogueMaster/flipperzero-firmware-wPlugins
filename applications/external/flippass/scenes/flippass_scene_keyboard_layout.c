#include "flippass_scene_keyboard_layout.h"

#include "../flippass.h"
#include "../plugins/flippass_keyboard_layout_plugin.h"
#include "flippass_scene.h"
#include "flippass_scene_send_confirm.h"
#include "flippass_scene_status.h"

#include <string.h>

#define FLIPPASS_LAYOUT_EVENT_SELECT 0x400U

static FlipPassOutputTransport flippass_keyboard_layout_transport(FlipPassEntryAction action) {
    switch(action) {
    case FlipPassEntryActionTypeUsernameBluetooth:
    case FlipPassEntryActionTypePasswordBluetooth:
    case FlipPassEntryActionTypeAutoTypeBluetooth:
    case FlipPassEntryActionTypeLoginBluetooth:
    case FlipPassEntryActionTypeOtherBluetooth:
        return FlipPassOutputTransportBluetooth;
    case FlipPassEntryActionTypeUsernameUsb:
    case FlipPassEntryActionTypePasswordUsb:
    case FlipPassEntryActionTypeAutoTypeUsb:
    case FlipPassEntryActionTypeLoginUsb:
    case FlipPassEntryActionTypeOtherUsb:
    default:
        return FlipPassOutputTransportUsb;
    }
}

static const char* flippass_keyboard_layout_progress_title(FlipPassEntryAction action) {
    switch(action) {
    case FlipPassEntryActionTypeUsernameUsb:
    case FlipPassEntryActionTypeUsernameBluetooth:
        return "Typing Username";
    case FlipPassEntryActionTypePasswordUsb:
    case FlipPassEntryActionTypePasswordBluetooth:
        return "Typing Password";
    case FlipPassEntryActionTypeLoginUsb:
    case FlipPassEntryActionTypeLoginBluetooth:
        return "Typing Login";
    case FlipPassEntryActionTypeOtherUsb:
    case FlipPassEntryActionTypeOtherBluetooth:
        return "Typing Field";
    case FlipPassEntryActionTypeAutoTypeUsb:
    case FlipPassEntryActionTypeAutoTypeBluetooth:
    default:
        return "Typing AutoType";
    }
}

static const char* flippass_keyboard_layout_failure_title(App* app) {
    if(flippass_keyboard_layout_transport(app->pending_entry_action) ==
       FlipPassOutputTransportBluetooth) {
        return flippass_output_bluetooth_is_advertising(app) ? "Bluetooth Waiting" :
                                                               "Bluetooth Typing Failed";
    }

    return "USB Typing Failed";
}

static void flippass_keyboard_layout_select_callback(void* context, uint32_t index) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, FLIPPASS_LAYOUT_EVENT_SELECT + index);
}

static const char* flippass_keyboard_layout_host_get_current_path(void* host_context) {
    App* app = host_context;
    if(app == NULL || app->keyboard_layout_path == NULL ||
       furi_string_empty(app->keyboard_layout_path)) {
        return "";
    }

    return furi_string_get_cstr(app->keyboard_layout_path);
}

static bool flippass_keyboard_layout_host_set_current_path(
    void* host_context,
    const char* path,
    bool use_alt_numpad) {
    App* app = host_context;
    if(app == NULL || app->keyboard_layout_path == NULL) {
        return false;
    }

    if(use_alt_numpad || path == NULL || path[0] == '\0') {
        furi_string_reset(app->keyboard_layout_path);
    } else {
        furi_string_set_str(app->keyboard_layout_path, path);
    }

    flippass_save_settings(app);
    return true;
}

static void flippass_keyboard_layout_host_log(
    void* host_context,
    const char* module_name,
    const char* message) {
    App* app = host_context;
    if(app == NULL || module_name == NULL || message == NULL) {
        return;
    }

    FLIPPASS_LOG_EVENT(app, "%s %s", module_name, message);
}

static FlipPassKeyboardLayoutHostApiV1 flippass_keyboard_layout_host_api(App* app) {
    const FlipPassKeyboardLayoutHostApiV1 host_api = {
        .api_version = FLIPPASS_KEYBOARD_LAYOUT_PLUGIN_API_VERSION,
        .host_context = app,
        .get_current_layout_path = flippass_keyboard_layout_host_get_current_path,
        .set_current_layout_path = flippass_keyboard_layout_host_set_current_path,
        .log = flippass_keyboard_layout_host_log,
    };
    return host_api;
}

static const FlipPassKeyboardLayoutPluginV1*
    flippass_keyboard_layout_plugin_loaded(const App* app) {
    if(app == NULL) {
        return NULL;
    }

    const FlipPassModuleInstance* instance =
        &app->module_loader.slot[FlipPassModuleSlotKeyboardLayout];
    return (instance->descriptor != NULL) ? instance->descriptor->entry_point : NULL;
}

static const FlipPassKeyboardLayoutPluginV1*
    flippass_keyboard_layout_plugin_ensure(App* app, FuriString* error) {
    const FlipperAppPluginDescriptor* descriptor = flippass_module_ensure(
        app,
        FlipPassModuleSlotKeyboardLayout,
        NULL,
        FLIPPASS_KEYBOARD_LAYOUT_PLUGIN_APP_ID,
        FLIPPASS_KEYBOARD_LAYOUT_PLUGIN_API_VERSION,
        error);
    if(descriptor == NULL || descriptor->entry_point == NULL) {
        return NULL;
    }

    const FlipPassKeyboardLayoutPluginV1* plugin = descriptor->entry_point;
    if(plugin->api_version != FLIPPASS_KEYBOARD_LAYOUT_PLUGIN_API_VERSION) {
        if(error != NULL) {
            furi_string_set_str(error, "Keyboard layout plugin API mismatch.");
        }
        flippass_module_unload(app, FlipPassModuleSlotKeyboardLayout);
        return NULL;
    }

    return plugin;
}

static void flippass_keyboard_layout_execute_pending(App* app) {
    FuriString* error = furi_string_alloc();
    bool typed = false;
    bool canceled = false;

    flippass_typing_begin(app);
    flippass_entry_action_prepare_pending(app);
    flippass_progress_begin(
        app, flippass_keyboard_layout_progress_title(app->pending_entry_action), "Connecting", 5U);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewLoading);

    typed = flippass_entry_action_execute_pending(app, error);
    canceled = !typed && flippass_typing_should_cancel(app);
    flippass_typing_end(app);

    if(typed) {
        flippass_progress_update(app, "Done", "Field sent.", 100U);
        flippass_progress_reset(app);
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, FlipPassScene_DbEntries);
    } else if(canceled) {
        flippass_progress_reset(app);
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, FlipPassScene_DbEntries);
    } else {
        flippass_progress_reset(app);
        flippass_scene_status_show(
            app,
            flippass_keyboard_layout_failure_title(app),
            furi_string_get_cstr(error),
            app->keyboard_layout_return_scene);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
    }

    furi_string_free(error);
}

void flippass_scene_keyboard_layout_on_enter(void* context) {
    App* app = context;
    FuriString* error = furi_string_alloc();
    const FlipPassKeyboardLayoutPluginV1* plugin =
        flippass_keyboard_layout_plugin_ensure(app, error);
    const FlipPassKeyboardLayoutHostApiV1 host_api = flippass_keyboard_layout_host_api(app);

    if(plugin == NULL || plugin->load_items == NULL || !plugin->load_items(&host_api)) {
        flippass_scene_status_show(
            app,
            "Layout Plugin Failed",
            furi_string_empty(error) ? "The keyboard layout plugin could not load." :
                                       furi_string_get_cstr(error),
            app->keyboard_layout_return_scene);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
        furi_string_free(error);
        return;
    }

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Keyboard Layout");

    for(uint32_t index = 0U; index < plugin->item_count(); index++) {
        const char* label = plugin->item_label(index);
        if(label == NULL) {
            continue;
        }
        submenu_add_item(
            app->submenu, label, index, flippass_keyboard_layout_select_callback, app);
    }

    submenu_set_selected_item(app->submenu, plugin->selected_index(&host_api));
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewSubmenu);
    furi_string_free(error);
}

bool flippass_scene_keyboard_layout_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    const FlipPassKeyboardLayoutPluginV1* plugin = flippass_keyboard_layout_plugin_loaded(app);
    const FlipPassKeyboardLayoutHostApiV1 host_api = flippass_keyboard_layout_host_api(app);

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom && plugin != NULL && plugin->item_count != NULL &&
       plugin->apply_selection != NULL && event.event >= FLIPPASS_LAYOUT_EVENT_SELECT &&
       event.event < (FLIPPASS_LAYOUT_EVENT_SELECT + plugin->item_count())) {
        const uint32_t selected_index = event.event - FLIPPASS_LAYOUT_EVENT_SELECT;
        if(plugin->apply_selection(&host_api, selected_index)) {
            flippass_keyboard_layout_execute_pending(app);
        } else {
            flippass_scene_status_show(
                app,
                "Layout Save Failed",
                "The selected keyboard layout could not be stored.",
                app->keyboard_layout_return_scene);
            scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
        }
        return true;
    }

    return false;
}

void flippass_scene_keyboard_layout_on_exit(void* context) {
    App* app = context;
    const FlipPassKeyboardLayoutPluginV1* plugin = flippass_keyboard_layout_plugin_loaded(app);

    submenu_reset(app->submenu);
    if(plugin != NULL && plugin->reset != NULL) {
        plugin->reset();
    }
    flippass_module_unload(app, FlipPassModuleSlotKeyboardLayout);
}
