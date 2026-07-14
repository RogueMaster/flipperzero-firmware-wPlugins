#include "../bt_audio.h"
#include <string.h>

// State offset to indicate we're viewing About (not in submenu)
#define ABOUT_VIEW_STATE_OFFSET 100

typedef enum {
    SubmenuIndexScan,
    SubmenuIndexSettings,
    SubmenuIndexVersion,
    SubmenuIndexDiagnostics,
    SubmenuIndexAbout,
    // Background mode items use higher indices to avoid collision
    SubmenuIndexNowPlayingBackground = 100,
} SubmenuIndex;

static void bt_audio_firmware_check_callback(const char* data, size_t len, void* context) {
    BtAudio* app = context;
    UNUSED(len);

    // Check for firmware response - expecting "BT_AUDIO_READY" or similar
    if(strncmp(data, "BT_AUDIO_READY", 14) == 0 || strncmp(data, "ESP32", 5) == 0 ||
       strncmp(data, "READY", 5) == 0) {
        bt_audio_set_firmware_response(app, data);
        app->esp32_present = true;
        app->firmware_check_done = true;

        // Send saved BT device name to ESP32 so it can persist it
        // This ensures ESP32's NVS has the latest name from Flipper's config
        // The name will take effect on next ESP32 restart if different
        if(app->config.bt_device_name[0] != '\0') {
            char cmd[12 + BT_AUDIO_BT_NAME_LEN + 2]; // "SET_BT_NAME:" + name + "\n" + null
            snprintf(cmd, sizeof(cmd), "SET_BT_NAME:%s\n", app->config.bt_device_name);
            bt_audio_uart_tx(app->uart, cmd);
            FURI_LOG_I(TAG, "Sent BT name to ESP32: %s", app->config.bt_device_name);
        }

        // Send shuffle mode setting to ESP32
        {
            char cmd[16];
            snprintf(cmd, sizeof(cmd), "SHUFFLE:%s\n", app->config.shuffle_mode ? "ON" : "OFF");
            bt_audio_uart_tx(app->uart, cmd);
            FURI_LOG_I(
                TAG, "Sent shuffle mode to ESP32: %s", app->config.shuffle_mode ? "ON" : "OFF");
        }

        view_dispatcher_send_custom_event(app->view_dispatcher, BtAudioEventFirmwareOK);
    }
    // Handle STATUS response (for background mode re-entry)
    else if(strncmp(data, "STATUS_CONNECTED:YES", 20) == 0) {
        app->is_connected = true;
        FURI_LOG_I(TAG, "Background mode: ESP32 still connected");
    } else if(strncmp(data, "STATUS_PLAYING:YES", 18) == 0) {
        app->is_playing = true;
        FURI_LOG_I(TAG, "Background mode: ESP32 still playing");
    } else if(strncmp(data, "STATUS_PAUSED:YES", 17) == 0) {
        app->is_paused = true;
        FURI_LOG_I(TAG, "Background mode: ESP32 paused");
    } else if(strncmp(data, "STATUS_FILE:", 12) == 0) {
        snprintf(app->current_filename, BT_AUDIO_FILENAME_LEN, "%s", data + 12);
        FURI_LOG_I(TAG, "Background mode: Current file: %s", app->current_filename);
    } else if(strncmp(data, "STATUS_COMPLETE", 15) == 0) {
        // STATUS response complete - trigger menu rebuild if audio is playing
        FURI_LOG_I(TAG, "STATUS complete, playing=%d, paused=%d", app->is_playing, app->is_paused);
        view_dispatcher_send_custom_event(app->view_dispatcher, BtAudioEventStatusComplete);
    }
}

static void bt_audio_scene_start_submenu_callback(void* context, uint32_t index) {
    BtAudio* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void bt_audio_scene_start_on_enter(void* context) {
    BtAudio* app = context;
    Submenu* submenu = app->submenu;

    // Set up UART callback for firmware detection and status checks
    bt_audio_uart_set_rx_callback(app->uart, bt_audio_firmware_check_callback);

    // Apply backlight setting - use enforce functions for persistent effect
    if(app->config.keep_backlight_on) {
        notification_message(app->notifications, &sequence_display_backlight_enforce_on);
    }

    // Only perform firmware check once
    if(!app->firmware_check_done) {
        // Send version check command
        bt_audio_uart_tx(app->uart, "VERSION\n");

        // Wait briefly for response - we'll check esp32_present when user tries to scan
        app->firmware_check_done = true;
    }

    // Send STATUS command asynchronously - don't block the UI thread
    // The STATUS response will arrive via UART callback and trigger a menu rebuild
    // if audio is currently playing (see BtAudioEventStatusComplete handler in on_event)
    // This asynchronous approach keeps the UI responsive during app entry
    bt_audio_uart_tx(app->uart, "STATUS\n");

    // Check if we already know audio is playing (from previous app entry or background mode)
    // This allows immediate display of "Now Playing Menu" if state is already known
    // The STATUS response will confirm/update this state asynchronously
    bool now_playing_shown = (app->is_playing || app->is_paused);

    // Add "Now Playing Menu" item if audio is playing (from background mode)
    if(now_playing_shown) {
        submenu_add_item(
            submenu,
            "Now Playing Menu",
            SubmenuIndexNowPlayingBackground,
            bt_audio_scene_start_submenu_callback,
            app);
        FURI_LOG_I(
            TAG,
            "Menu built with Now Playing Menu (is_playing=%d, is_paused=%d)",
            app->is_playing,
            app->is_paused);
    } else {
        FURI_LOG_I(
            TAG,
            "Menu built without Now Playing Menu (is_playing=%d, is_paused=%d)",
            app->is_playing,
            app->is_paused);
    }

    submenu_add_item(
        submenu, "Scan for devices", SubmenuIndexScan, bt_audio_scene_start_submenu_callback, app);
    submenu_add_item(
        submenu, "Settings", SubmenuIndexSettings, bt_audio_scene_start_submenu_callback, app);
    submenu_add_item(
        submenu, "Board version", SubmenuIndexVersion, bt_audio_scene_start_submenu_callback, app);
    submenu_add_item(
        submenu,
        "Diagnostics",
        SubmenuIndexDiagnostics,
        bt_audio_scene_start_submenu_callback,
        app);
    submenu_add_item(
        submenu, "About", SubmenuIndexAbout, bt_audio_scene_start_submenu_callback, app);

    // If "Now Playing Menu" is shown, select it; otherwise use saved state
    if(now_playing_shown) {
        submenu_set_selected_item(
            submenu, SubmenuIndexNowPlayingBackground); // Select by index value
    } else {
        submenu_set_selected_item(
            submenu, scene_manager_get_scene_state(app->scene_manager, BtAudioSceneStart));
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewSubmenu);
}

bool bt_audio_scene_start_on_event(void* context, SceneManagerEvent event) {
    BtAudio* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == BtAudioEventStatusComplete) {
            // STATUS response complete - rebuild menu if audio is playing and menu doesn't have it
            if(app->is_playing || app->is_paused) {
                // Rebuild the menu to include "Now Playing Menu"
                submenu_reset(app->submenu);

                // Add "Now Playing Menu" at the top and select it
                submenu_add_item(
                    app->submenu,
                    "Now Playing Menu",
                    SubmenuIndexNowPlayingBackground,
                    bt_audio_scene_start_submenu_callback,
                    app);

                submenu_add_item(
                    app->submenu,
                    "Scan for devices",
                    SubmenuIndexScan,
                    bt_audio_scene_start_submenu_callback,
                    app);
                submenu_add_item(
                    app->submenu,
                    "Settings",
                    SubmenuIndexSettings,
                    bt_audio_scene_start_submenu_callback,
                    app);
                submenu_add_item(
                    app->submenu,
                    "Board version",
                    SubmenuIndexVersion,
                    bt_audio_scene_start_submenu_callback,
                    app);
                submenu_add_item(
                    app->submenu,
                    "Diagnostics",
                    SubmenuIndexDiagnostics,
                    bt_audio_scene_start_submenu_callback,
                    app);
                submenu_add_item(
                    app->submenu,
                    "About",
                    SubmenuIndexAbout,
                    bt_audio_scene_start_submenu_callback,
                    app);

                // Select the "Now Playing Menu" item by index value
                submenu_set_selected_item(app->submenu, SubmenuIndexNowPlayingBackground);

                FURI_LOG_I(TAG, "Menu rebuilt with Now Playing Menu");
            }
            consumed = true;
        } else if(event.event == SubmenuIndexNowPlayingBackground) {
            // "Now Playing Menu" from background mode - go directly to control scene
            scene_manager_set_scene_state(
                app->scene_manager, BtAudioSceneStart, SubmenuIndexNowPlayingBackground);
            scene_manager_next_scene(app->scene_manager, BtAudioSceneControl);
            consumed = true;
        } else if(event.event == SubmenuIndexScan) {
            scene_manager_set_scene_state(app->scene_manager, BtAudioSceneStart, event.event);
            scene_manager_next_scene(app->scene_manager, BtAudioSceneScan);
            consumed = true;
        } else if(event.event == SubmenuIndexSettings) {
            scene_manager_set_scene_state(app->scene_manager, BtAudioSceneStart, event.event);
            scene_manager_next_scene(app->scene_manager, BtAudioSceneSettings);
            consumed = true;
        } else if(event.event == SubmenuIndexVersion) {
            scene_manager_set_scene_state(app->scene_manager, BtAudioSceneStart, event.event);
            scene_manager_next_scene(app->scene_manager, BtAudioSceneVersion);
            consumed = true;
        } else if(event.event == SubmenuIndexDiagnostics) {
            scene_manager_set_scene_state(app->scene_manager, BtAudioSceneStart, event.event);
            scene_manager_next_scene(app->scene_manager, BtAudioSceneDiagnostics);
            consumed = true;
        } else if(event.event == SubmenuIndexAbout) {
            // Use state + offset to indicate we're viewing About (not in submenu)
            scene_manager_set_scene_state(
                app->scene_manager,
                BtAudioSceneStart,
                SubmenuIndexAbout + ABOUT_VIEW_STATE_OFFSET);
            furi_string_printf(
                app->text_box_store,
                "BT Audio v%s\n"
                "Created by FatherDivine\n\n"
                "An ESP32 Bluetooth Audio\n"
                "Controller over UART\n"
                "(and other methods WIP)\n\n"
                "Hardware:\n"
                "- Mayhem Board (100%% Supported)\n"
                "- ESP32-CAM (tested via Mayhem)\n"
                "- Generic ESP32 (Untested)\n"
                "- WiFi Dev Board\n"
                "  (untested, only has BLE)\n\n"
                "UART: 115200 baud\n"
                "TX=Pin 13, RX=Pin 14\n\n"
                "Protocol Commands:\n"
                "CONNECT <MAC> - Connect\n"
                "DEBUG - Diagnostics\n"
                "DISCONNECT - Disconnect\n"
                "EQ_BASS/MID/TREBLE:<dB>\n"
                "NEXT/PREV - Track nav\n"
                "PAUSE/RESUME - Control\n"
                "PLAY - Play test tone\n"
                "PLAY_FAVORITES - Play favs\n"
                "PLAY_JINGLE - Play jingle\n"
                "PLAY_MP3 - Play from SD\n"
                "PLAY_PLAYLIST - playlist1.m3u\n"
                "PLAY_PLAYLIST2 - playlist2.m3u\n"
                "SCAN - Scan for devices\n"
                "SET_BT_NAME:<name> - BT name\n"
                "SET_INIT_VOL:<0-127> - Vol\n"
                "SHUFFLE:<ON/OFF> - Shuffle\n"
                "STATUS - Query play state\n"
                "STOP - Stop playback\n"
                "TX_POWER:<LVL> - TX power\n"
                "VERSION - Check firmware\n"
                "VOL_UP/VOL_DOWN - Volume\n",
                FAP_VERSION);
            text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
            view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewTextBox);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // Check if we're viewing About (indicated by state being SubmenuIndexAbout + offset)
        uint32_t state = scene_manager_get_scene_state(app->scene_manager, BtAudioSceneStart);
        if(state == SubmenuIndexAbout + ABOUT_VIEW_STATE_OFFSET) {
            // Return to main menu from About screen
            scene_manager_set_scene_state(
                app->scene_manager, BtAudioSceneStart, SubmenuIndexAbout);
            view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewSubmenu);
            consumed = true;
        } else {
            // Exit app from main menu
            scene_manager_stop(app->scene_manager);
            view_dispatcher_stop(app->view_dispatcher);
            consumed = true;
        }
    }

    return consumed;
}

void bt_audio_scene_start_on_exit(void* context) {
    BtAudio* app = context;
    submenu_reset(app->submenu);
}
