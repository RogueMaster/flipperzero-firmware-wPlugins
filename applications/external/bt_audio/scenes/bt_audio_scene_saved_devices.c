#include "../bt_audio.h"
#include <string.h>

// Submenu indices for saved device selection
// Using base offset to avoid collision with other scene indices
#define SAVED_DEVICE_INDEX_BASE 200

static void bt_audio_scene_saved_devices_submenu_callback(void* context, uint32_t index) {
    BtAudio* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void bt_audio_scene_saved_devices_on_enter(void* context) {
    BtAudio* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);

    if(app->device_history_count == 0) {
        // No saved devices - show message
        submenu_add_item(submenu, "No saved devices", SAVED_DEVICE_INDEX_BASE, NULL, app);
        submenu_add_item(
            submenu, "Scan for devices first", SAVED_DEVICE_INDEX_BASE + 1, NULL, app);
    } else {
        // Add each saved device to the submenu
        // The first device (index 0) is the most recently connected, shown as "Last Device: <name>"
        for(uint8_t i = 0; i < app->device_history_count; i++) {
            // Create display string with name and MAC
            char display_str[BT_AUDIO_DEVICE_NAME_LEN + BT_AUDIO_MAC_LEN + 16];

            // Show name first, then MAC in parentheses if space allows
            if(strlen(app->device_history[i].name) > 0 &&
               strcmp(app->device_history[i].name, "Unknown Device") != 0) {
                // Has a name
                if(i == 0) {
                    // First device is most recent - mark as "Last Device"
                    snprintf(
                        display_str, sizeof(display_str), "Last: %s", app->device_history[i].name);
                } else {
                    // Other devices - just show name
                    snprintf(display_str, sizeof(display_str), "%s", app->device_history[i].name);
                }
            } else {
                // No name - show just MAC (with "Last: " prefix for first device)
                if(i == 0) {
                    snprintf(
                        display_str, sizeof(display_str), "Last: %s", app->device_history[i].mac);
                } else {
                    snprintf(display_str, sizeof(display_str), "%s", app->device_history[i].mac);
                }
            }

            submenu_add_item(
                submenu,
                display_str,
                SAVED_DEVICE_INDEX_BASE + i,
                bt_audio_scene_saved_devices_submenu_callback,
                app);
        }
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewSubmenu);
}

bool bt_audio_scene_saved_devices_on_event(void* context, SceneManagerEvent event) {
    BtAudio* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        uint32_t index = event.event;

        // Check if this is a device selection event
        if(index >= SAVED_DEVICE_INDEX_BASE &&
           index < (uint32_t)(SAVED_DEVICE_INDEX_BASE + app->device_history_count)) {
            uint8_t device_index = index - SAVED_DEVICE_INDEX_BASE;

            // Copy MAC address to device list slot 0 for connection
            // Format: "MAC,Name" - connect scene will extract MAC
            // MAC is 17 chars (AA:BB:CC:DD:EE:FF), truncate name to fit buffer:
            // 64 - 17 (MAC) - 1 (comma) - 1 (null) = 45 chars max for name
            snprintf(
                app->device_list[0],
                BT_AUDIO_DEVICE_NAME_LEN,
                "%.17s,%.45s",
                app->device_history[device_index].mac,
                app->device_history[device_index].name);
            app->device_count = 1;
            app->selected_device = 0;

            FURI_LOG_I(TAG, "Selected saved device: %s", app->device_history[device_index].mac);

            // Go to connect scene
            scene_manager_next_scene(app->scene_manager, BtAudioSceneConnect);
            consumed = true;
        }
    }

    return consumed;
}

void bt_audio_scene_saved_devices_on_exit(void* context) {
    BtAudio* app = context;
    submenu_reset(app->submenu);
}
