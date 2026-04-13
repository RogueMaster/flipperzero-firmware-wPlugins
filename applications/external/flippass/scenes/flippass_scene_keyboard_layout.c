#include "flippass_scene_keyboard_layout.h"

#include "../flippass.h"
#include "flippass_scene.h"
#include "flippass_scene_send_confirm.h"
#include "flippass_scene_status.h"

#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>

#define FLIPPASS_LAYOUT_MAX_ITEMS    40U
#define FLIPPASS_LAYOUT_LABEL_SIZE   32U
#define FLIPPASS_LAYOUT_PATH_SIZE    96U
#define FLIPPASS_LAYOUT_EVENT_SELECT 0x400U

typedef struct {
    char label[FLIPPASS_LAYOUT_LABEL_SIZE];
    char path[FLIPPASS_LAYOUT_PATH_SIZE];
    bool use_alt_numpad;
} FlipPassKeyboardLayoutItem;

static FlipPassKeyboardLayoutItem* flippass_layout_items = NULL;
static uint32_t flippass_layout_item_count = 0U;

static void flippass_keyboard_layout_free_items(void) {
    if(flippass_layout_items != NULL) {
        free(flippass_layout_items);
        flippass_layout_items = NULL;
    }
    flippass_layout_item_count = 0U;
}

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

static void
    flippass_keyboard_layout_add_item(const char* label, const char* path, bool use_alt_numpad) {
    if(flippass_layout_items == NULL || flippass_layout_item_count >= FLIPPASS_LAYOUT_MAX_ITEMS) {
        return;
    }

    FlipPassKeyboardLayoutItem* item = &flippass_layout_items[flippass_layout_item_count++];
    snprintf(item->label, sizeof(item->label), "%s", label != NULL ? label : "");
    snprintf(item->path, sizeof(item->path), "%s", path != NULL ? path : "");
    item->use_alt_numpad = use_alt_numpad;
}

static void flippass_keyboard_layout_load_items(void) {
    if(flippass_layout_items == NULL) {
        flippass_layout_items =
            malloc(sizeof(FlipPassKeyboardLayoutItem) * FLIPPASS_LAYOUT_MAX_ITEMS);
        if(flippass_layout_items == NULL) {
            flippass_layout_item_count = 0U;
            return;
        }
    }

    flippass_layout_item_count = 0U;
    flippass_keyboard_layout_add_item("Alt+NumPad", "", true);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* dir = storage_file_alloc(storage);

    if(storage_dir_open(dir, FLIPPASS_BADUSB_LAYOUT_DIR)) {
        FileInfo file_info;
        char file_name[FLIPPASS_LAYOUT_LABEL_SIZE];

        while(flippass_layout_item_count < FLIPPASS_LAYOUT_MAX_ITEMS &&
              storage_dir_read(dir, &file_info, file_name, sizeof(file_name))) {
            char* extension = NULL;
            char full_path[FLIPPASS_LAYOUT_PATH_SIZE];

            if(file_info.flags & FSF_DIRECTORY) {
                continue;
            }

            extension = strrchr(file_name, '.');
            if(extension == NULL || strcmp(extension, FLIPPASS_BADUSB_LAYOUT_EXT) != 0 ||
               file_info.size != 256U) {
                continue;
            }

            *extension = '\0';
            snprintf(
                full_path,
                sizeof(full_path),
                "%s/%s%s",
                FLIPPASS_BADUSB_LAYOUT_DIR,
                file_name,
                FLIPPASS_BADUSB_LAYOUT_EXT);
            flippass_keyboard_layout_add_item(file_name, full_path, false);
        }

        storage_dir_close(dir);
    }

    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);
}

static uint32_t flippass_keyboard_layout_selected_index(const App* app) {
    if(app == NULL || app->keyboard_layout_path == NULL ||
       furi_string_empty(app->keyboard_layout_path)) {
        return 0U;
    }

    const char* current_path = furi_string_get_cstr(app->keyboard_layout_path);
    for(uint32_t index = 1U; index < flippass_layout_item_count; index++) {
        if(strcmp(current_path, flippass_layout_items[index].path) == 0) {
            return index;
        }
    }

    return 0U;
}

static bool flippass_keyboard_layout_apply_selection(App* app, uint32_t index) {
    if(app == NULL || index >= flippass_layout_item_count || app->keyboard_layout_path == NULL) {
        return false;
    }

    if(flippass_layout_items[index].use_alt_numpad) {
        furi_string_reset(app->keyboard_layout_path);
    } else {
        furi_string_set_str(app->keyboard_layout_path, flippass_layout_items[index].path);
    }

    flippass_save_settings(app);
    return true;
}

static void flippass_keyboard_layout_execute_pending(App* app) {
    FuriString* error = furi_string_alloc();

    flippass_entry_action_prepare_pending(app);
    flippass_progress_begin(
        app, flippass_keyboard_layout_progress_title(app->pending_entry_action), "Connecting", 5U);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewLoading);

    if(flippass_entry_action_execute_pending(app, error)) {
        flippass_progress_update(app, "Done", "Field sent.", 100U);
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

    flippass_keyboard_layout_load_items();
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Keyboard Layout");

    for(uint32_t index = 0U; index < flippass_layout_item_count; index++) {
        submenu_add_item(
            app->submenu,
            flippass_layout_items[index].label,
            index,
            flippass_keyboard_layout_select_callback,
            app);
    }

    submenu_set_selected_item(app->submenu, flippass_keyboard_layout_selected_index(app));
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewSubmenu);
}

bool flippass_scene_keyboard_layout_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom && event.event >= FLIPPASS_LAYOUT_EVENT_SELECT &&
       event.event < (FLIPPASS_LAYOUT_EVENT_SELECT + flippass_layout_item_count)) {
        const uint32_t selected_index = event.event - FLIPPASS_LAYOUT_EVENT_SELECT;
        if(flippass_keyboard_layout_apply_selection(app, selected_index)) {
            flippass_keyboard_layout_execute_pending(app);
        }
        return true;
    }

    return false;
}

void flippass_scene_keyboard_layout_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
    flippass_keyboard_layout_free_items();
}
