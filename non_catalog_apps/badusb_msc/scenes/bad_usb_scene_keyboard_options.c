#include "../bad_usb_app_i.h"
#include <string.h>

// The main Options menu is intentionally separate from the original BadUSB
// script Config scene. It has only the keyboard layout choice.
static void bad_usb_keyboard_options_callback(void* context, uint32_t index) {
    BadUsbApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void bad_usb_scene_keyboard_options_on_enter(void* context) {
    BadUsbApp* app = context;
    VariableItemList* list = app->var_item_list;
    variable_item_list_reset(list);

    const char* path = furi_string_get_cstr(app->keyboard_layout);
    const char* filename = strrchr(path, '/');
    filename = filename ? filename + 1 : path;
    char layout[20];
    if(!filename || !*filename) {
        strlcpy(layout, "US", sizeof(layout));
    } else {
        strlcpy(layout, filename, sizeof(layout));
        char* ext = strrchr(layout, '.');
        if(ext && strcmp(ext, ".kl") == 0) *ext = '\0';
    }

    // Real right-hand value, not text pasted into a long single line.
    VariableItem* item = variable_item_list_add(list, "Keyboard Layout", 0, NULL, NULL);
    variable_item_set_current_value_text(item, layout);
    variable_item_list_set_enter_callback(list, bad_usb_keyboard_options_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbAppViewConfig);
}

bool bad_usb_scene_keyboard_options_on_event(void* context, SceneManagerEvent event) {
    BadUsbApp* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == 0) {
        scene_manager_next_scene(app->scene_manager, BadUsbSceneConfigLayout);
        return true;
    }
    return false;
}

void bad_usb_scene_keyboard_options_on_exit(void* context) {
    BadUsbApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
