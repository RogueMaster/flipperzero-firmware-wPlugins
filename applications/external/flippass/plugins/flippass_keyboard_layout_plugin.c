#include "flippass_keyboard_layout_plugin.h"

#include <storage/storage.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FLIPPASS_BADUSB_LAYOUT_DIR EXT_PATH("badusb/assets/layouts")
#define FLIPPASS_BADUSB_LAYOUT_EXT ".kl"
#define FLIPPASS_LAYOUT_MAX_ITEMS  40U
#define FLIPPASS_LAYOUT_LABEL_SIZE 32U
#define FLIPPASS_LAYOUT_PATH_SIZE  96U

typedef struct {
    char label[FLIPPASS_LAYOUT_LABEL_SIZE];
    char path[FLIPPASS_LAYOUT_PATH_SIZE];
    bool use_alt_numpad;
} FlipPassKeyboardLayoutPluginItem;

static FlipPassKeyboardLayoutPluginItem flippass_keyboard_layout_items[FLIPPASS_LAYOUT_MAX_ITEMS];
static uint32_t flippass_keyboard_layout_item_count = 0U;

static void flippass_keyboard_layout_log(
    const FlipPassKeyboardLayoutHostApiV1* host_api,
    const char* text) {
    if(host_api != NULL && host_api->log != NULL && text != NULL) {
        host_api->log(host_api->host_context, "flippass_keyboard_layout", text);
    }
}

static void
    flippass_keyboard_layout_add_item(const char* label, const char* path, bool use_alt_numpad) {
    if(flippass_keyboard_layout_item_count >= FLIPPASS_LAYOUT_MAX_ITEMS) {
        return;
    }

    FlipPassKeyboardLayoutPluginItem* item =
        &flippass_keyboard_layout_items[flippass_keyboard_layout_item_count++];
    snprintf(item->label, sizeof(item->label), "%s", (label != NULL) ? label : "");
    snprintf(item->path, sizeof(item->path), "%s", (path != NULL) ? path : "");
    item->use_alt_numpad = use_alt_numpad;
}

static bool
    flippass_keyboard_layout_load_items_impl(const FlipPassKeyboardLayoutHostApiV1* host_api) {
    flippass_keyboard_layout_item_count = 0U;
    flippass_keyboard_layout_add_item("Alt+NumPad", "", true);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* dir = storage_file_alloc(storage);

    if(storage_dir_open(dir, FLIPPASS_BADUSB_LAYOUT_DIR)) {
        FileInfo file_info;
        char file_name[FLIPPASS_LAYOUT_LABEL_SIZE];

        while(flippass_keyboard_layout_item_count < FLIPPASS_LAYOUT_MAX_ITEMS &&
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
    } else {
        flippass_keyboard_layout_log(host_api, "layout directory not found");
    }

    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);
    return flippass_keyboard_layout_item_count > 0U;
}

static uint32_t flippass_keyboard_layout_item_count_impl(void) {
    return flippass_keyboard_layout_item_count;
}

static const char* flippass_keyboard_layout_item_label_impl(uint32_t index) {
    if(index >= flippass_keyboard_layout_item_count) {
        return NULL;
    }

    return flippass_keyboard_layout_items[index].label;
}

static uint32_t
    flippass_keyboard_layout_selected_index_impl(const FlipPassKeyboardLayoutHostApiV1* host_api) {
    if(host_api == NULL || host_api->get_current_layout_path == NULL) {
        return 0U;
    }

    const char* current_path = host_api->get_current_layout_path(host_api->host_context);
    if(current_path == NULL || current_path[0] == '\0') {
        return 0U;
    }

    for(uint32_t index = 1U; index < flippass_keyboard_layout_item_count; index++) {
        if(strcmp(current_path, flippass_keyboard_layout_items[index].path) == 0) {
            return index;
        }
    }

    return 0U;
}

static bool flippass_keyboard_layout_apply_selection_impl(
    const FlipPassKeyboardLayoutHostApiV1* host_api,
    uint32_t index) {
    if(host_api == NULL || host_api->set_current_layout_path == NULL ||
       index >= flippass_keyboard_layout_item_count) {
        return false;
    }

    const FlipPassKeyboardLayoutPluginItem* item = &flippass_keyboard_layout_items[index];
    return host_api->set_current_layout_path(
        host_api->host_context, item->path, item->use_alt_numpad);
}

static void flippass_keyboard_layout_reset_impl(void) {
    flippass_keyboard_layout_item_count = 0U;
    memset(flippass_keyboard_layout_items, 0, sizeof(flippass_keyboard_layout_items));
}

static const FlipPassKeyboardLayoutPluginV1 flippass_keyboard_layout_plugin = {
    .api_version = FLIPPASS_KEYBOARD_LAYOUT_PLUGIN_API_VERSION,
    .load_items = flippass_keyboard_layout_load_items_impl,
    .item_count = flippass_keyboard_layout_item_count_impl,
    .item_label = flippass_keyboard_layout_item_label_impl,
    .selected_index = flippass_keyboard_layout_selected_index_impl,
    .apply_selection = flippass_keyboard_layout_apply_selection_impl,
    .reset = flippass_keyboard_layout_reset_impl,
};

static const FlipperAppPluginDescriptor flippass_keyboard_layout_descriptor = {
    .appid = FLIPPASS_KEYBOARD_LAYOUT_PLUGIN_APP_ID,
    .ep_api_version = FLIPPASS_KEYBOARD_LAYOUT_PLUGIN_API_VERSION,
    .entry_point = &flippass_keyboard_layout_plugin,
};

const FlipperAppPluginDescriptor* flippass_keyboard_layout_plugin_ep(void) {
    return &flippass_keyboard_layout_descriptor;
}
