#include "../bt_audio.h"
#include <string.h>
#include <gui/elements.h>

// Submenu indices for control menu
typedef enum {
    SubmenuIndexBrowseMusic, // Browse and select MP3 files
    SubmenuIndexPlayAudio,
    SubmenuIndexPlayFavorites, // Play favorites playlist
    SubmenuIndexPlayPlaylist, // Play from playlist1.m3u
    SubmenuIndexPlayPlaylist2, // Play from playlist2.m3u
    SubmenuIndexStreamAudio, // Stream audio from WiFi (new)
    SubmenuIndexPlayTone,
    SubmenuIndexPlayJingle,
    SubmenuIndexPlayBeeps,
    SubmenuIndexStopPlayback,
    SubmenuIndexReconnect, // Force reconnection when connection drops
    SubmenuIndexDisconnect,
    SubmenuIndexNowPlaying, // Added at end to avoid breaking existing indices
} SubmenuIndex;

// WiFi streaming state - tracks if we're waiting for WiFi connection before starting stream
static bool wifi_stream_pending = false;

// Double-tap detection timing (ms)
#define DOUBLE_TAP_TIME_MS        400
// Hold detection timing (ms)
#define HOLD_TIME_MS              500
// Scroll speed (ms between scroll ticks)
#define SCROLL_TICK_MS            300
// Pixels to scroll per tick
#define SCROLL_PIXELS             6
// Screen width for scrolling calculation
#define SCREEN_WIDTH              128
// Approximate character width for FontSecondary
#define FONT_SECONDARY_CHAR_WIDTH 6
// Timer callback for scroll animation
static void bt_audio_scroll_timer_cb(void* context) {
    BtAudio* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, BtAudioEventScrollTick);
}

// Timer callback for play completion timeout (fallback if ESP32 doesn't respond)
static void bt_audio_play_timeout_cb(void* context) {
    BtAudio* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, BtAudioEventPlayComplete);
}

// Helper function to start playlist playback and switch to player view
// Used by both Play Playlist 1 and Play Playlist 2 handlers to avoid code duplication
static void start_playlist_playback(BtAudio* app) {
    app->is_playing = true;
    app->is_paused = false;
    app->playing_favorites = false;
    app->scroll_offset = 0;

    // Start scroll timer for filename scrolling
    if(!app->scroll_timer) {
        app->scroll_timer = furi_timer_alloc(bt_audio_scroll_timer_cb, FuriTimerTypePeriodic, app);
    }
    furi_timer_start(app->scroll_timer, furi_ms_to_ticks(SCROLL_TICK_MS));

    // Switch to player view
    view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewPlayer);
}

// Extract just the filename from full path (e.g., "/music/song.mp3" -> "song.mp3")
static void extract_filename(const char* path, char* filename, size_t max_len) {
    const char* last_slash = strrchr(path, '/');
    if(last_slash && *(last_slash + 1) != '\0') {
        strncpy(filename, last_slash + 1, max_len - 1);
    } else {
        strncpy(filename, path, max_len - 1);
    }
    filename[max_len - 1] = '\0';
}

// Draw callback for player view
static void bt_audio_player_draw_callback(Canvas* canvas, void* model) {
    PlayerViewModel* player_model = (PlayerViewModel*)model;
    if(!player_model || !player_model->app) {
        canvas_clear(canvas);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Loading...");
        return;
    }
    BtAudio* app = player_model->app;

    canvas_clear(canvas);

    // Draw title with favorite indicator
    canvas_set_font(canvas, FontPrimary);
    if(app->current_is_favorite) {
        canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignCenter, "MP3 Player *");
    } else {
        canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignCenter, "MP3 Player");
    }

    // Draw status (playing/paused) with favorites mode indicator
    canvas_set_font(canvas, FontSecondary);
    if(app->is_paused) {
        if(app->playing_favorites) {
            canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "|| PAUSED (Fav)");
        } else {
            canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "|| PAUSED");
        }
    } else {
        if(app->playing_favorites) {
            canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "> PLAYING (Fav)");
        } else {
            canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "> PLAYING");
        }
    }

    // Draw filename with scrolling if needed
    char display_name[BT_AUDIO_FILENAME_LEN];
    extract_filename(app->current_filename, display_name, sizeof(display_name));

    // Calculate text width
    size_t text_len = strlen(display_name);
    int text_width = text_len * FONT_SECONDARY_CHAR_WIDTH;

    canvas_set_font(canvas, FontSecondary);
    if(text_width > SCREEN_WIDTH - 8) {
        // Text is too long, need to scroll
        // Draw with offset
        int x_pos = 4 - app->scroll_offset;
        canvas_draw_str(canvas, x_pos, 36, display_name);

        // Reset scroll if we've scrolled past the end
        if(app->scroll_offset > text_width - SCREEN_WIDTH + 16) {
            // We'll reset in the scroll tick event
        }
    } else {
        // Text fits, center it
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, display_name);
    }

    // Draw control hints at bottom
    canvas_draw_str_aligned(
        canvas, 64, 50, AlignCenter, AlignCenter, "^v:Vol  <:Rst <<:Prev  >:Next");
    canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignCenter, "OK:Pause  Hold OK:Fav");
}

// Input callback for player view
// Note on track navigation: PREV/NEXT commands are handled by ESP32 firmware.
// The ESP32 firmware is responsible for maintaining track history and playlist order.
// The Flipper app simply sends navigation commands and displays the current track.
static bool bt_audio_player_input_callback(InputEvent* event, void* context) {
    BtAudio* app = (BtAudio*)context;
    bool consumed = false;

    if(event->type == InputTypePress) {
        app->button_held = false;
    }

    // Handle long press OK to toggle favorite
    if(event->key == InputKeyOk && event->type == InputTypeLong) {
        // Long press OK: toggle favorite for current track
        if(app->current_filename[0] != '\0') {
            bool was_favorite = bt_audio_is_favorite(app, app->current_filename);
            bool toggle_success = bt_audio_toggle_favorite(app, app->current_filename);
            app->current_is_favorite = bt_audio_is_favorite(app, app->current_filename);

            // Provide appropriate feedback based on result
            if(toggle_success) {
                // Success - single vibration
                notification_message(app->notifications, &sequence_single_vibro);
                FURI_LOG_I(
                    TAG,
                    "Toggled favorite: %s (now %s)",
                    app->current_filename,
                    app->current_is_favorite ? "favorited" : "unfavorited");
            } else if(!was_favorite && app->favorites_count >= BT_AUDIO_MAX_FAVORITES) {
                // Failed to add because list is full - use error notification
                notification_message(app->notifications, &sequence_error);
                FURI_LOG_W(
                    TAG,
                    "Could not add favorite (list full, max %d): %s",
                    BT_AUDIO_MAX_FAVORITES,
                    app->current_filename);
            } else {
                // Some other failure (shouldn't normally happen) - still provide feedback
                notification_message(app->notifications, &sequence_single_vibro);
                FURI_LOG_W(TAG, "Favorite toggle returned false for: %s", app->current_filename);
            }
        }
        consumed = true;
        return consumed;
    }

    if(event->type == InputTypeRelease) {
        if(event->key == InputKeyUp) {
            // Up button: volume up
            bt_audio_uart_tx(app->uart, "VOL_UP\n");
            consumed = true;
        } else if(event->key == InputKeyDown) {
            // Down button: volume down
            bt_audio_uart_tx(app->uart, "VOL_DOWN\n");
            consumed = true;
        } else if(event->key == InputKeyRight) {
            // Right button: next track
            bt_audio_uart_tx(app->uart, "NEXT\n");
            consumed = true;
        } else if(event->key == InputKeyLeft) {
            // Left button: double-tap detection for prev track vs restart
            // First tap: restart current track
            // Second tap within DOUBLE_TAP_TIME_MS: go to previous track
            uint32_t current_time = furi_get_tick();
            uint32_t time_since_last = current_time - app->last_left_press;

            if(app->last_left_press > 0 &&
               time_since_last < furi_ms_to_ticks(DOUBLE_TAP_TIME_MS)) {
                // Double-tap detected: go to previous track
                bt_audio_uart_tx(app->uart, "PREV\n");
                app->last_left_press = 0; // Reset to prevent triple-tap
                FURI_LOG_D(TAG, "Double-tap left: PREV");
            } else {
                // Single tap: restart current track
                bt_audio_uart_tx(app->uart, "RESTART\n");
                app->last_left_press = current_time;
                FURI_LOG_D(TAG, "Single tap left: RESTART");
            }
            consumed = true;
        }
    }

    // Handle short press OK for pause/resume (only on short press, not after long)
    if(event->key == InputKeyOk && event->type == InputTypeShort) {
        // Short OK: toggle pause/resume
        if(app->is_paused) {
            bt_audio_uart_tx(app->uart, "RESUME\n");
        } else {
            bt_audio_uart_tx(app->uart, "PAUSE\n");
        }
        consumed = true;
    }

    // Handle Back button - single press to return to menu without stopping
    if(event->key == InputKeyBack) {
        if(event->type == InputTypeShort) {
            // Single press Back: return to menu, keep playing
            if(app->scroll_timer) furi_timer_stop(app->scroll_timer);
            // Don't stop playback, don't clear state - just navigate away
            // The scene manager will handle the navigation
            consumed = false; // Let scene manager handle the back navigation
        }
    }

    return consumed;
}

// UART callback to handle ESP32 responses during control/play
static void bt_audio_control_rx_callback(const char* data, size_t len, void* context) {
    BtAudio* app = context;
    UNUSED(len);

    FURI_LOG_I(TAG, "Control RX: %s", data);

    // Handle play completion responses from ESP32
    if(strncmp(data, "PLAY_COMPLETE", 13) == 0 || strncmp(data, "STOPPED", 7) == 0) {
        // Only process if we're still playing (avoid race with timer callback)
        if(app->is_playing || app->is_paused) {
            if(app->play_timer) furi_timer_stop(app->play_timer);
            if(app->scroll_timer) furi_timer_stop(app->scroll_timer);
            app->is_playing = false;
            app->is_paused = false;
            app->playing_favorites = false;
            app->current_filename[0] = '\0';
            app->current_is_favorite = false;
            view_dispatcher_send_custom_event(app->view_dispatcher, BtAudioEventPlayComplete);
        }
    } else if(strncmp(data, "PLAYING_FILE:", 13) == 0) {
        // ESP32 is playing a specific file - update display
        strncpy(app->current_filename, data + 13, BT_AUDIO_FILENAME_LEN - 1);
        app->current_filename[BT_AUDIO_FILENAME_LEN - 1] = '\0';
        app->scroll_offset = 0; // Reset scroll position
        app->is_playing = true;
        app->is_paused = false;
        // Update favorite status for the new track
        app->current_is_favorite = bt_audio_is_favorite(app, app->current_filename);
        FURI_LOG_I(
            TAG,
            "Playing file: %s (favorite: %s)",
            app->current_filename,
            app->current_is_favorite ? "yes" : "no");
        view_dispatcher_send_custom_event(app->view_dispatcher, BtAudioEventFileChanged);
    } else if(strncmp(data, "FILE_NOT_FOUND:", 15) == 0) {
        // File was not found (e.g., in favorites) - ESP32 will skip it
        FURI_LOG_W(TAG, "File not found: %s", data + 15);
    } else if(strncmp(data, "PAUSED", 6) == 0) {
        app->is_paused = true;
        view_dispatcher_send_custom_event(app->view_dispatcher, BtAudioEventPaused);
    } else if(strncmp(data, "RESUMED", 7) == 0 || strncmp(data, "PLAYBACK_RESUMED", 16) == 0) {
        // Handle both RESUMED (user action) and PLAYBACK_RESUMED (auto-reconnect)
        app->is_paused = false;
        app->is_playing = true;
        view_dispatcher_send_custom_event(app->view_dispatcher, BtAudioEventResumed);
    } else if(strncmp(data, "PLAYING", 7) == 0) {
        // ESP32 acknowledged play command
        FURI_LOG_I(TAG, "ESP32 started playing");
        app->is_playing = true;
        app->is_paused = false;
    } else if(strncmp(data, "BT_DISCONNECTED_RECONNECTING", 28) == 0) {
        // Bluetooth connection dropped but ESP32 will try to reconnect
        // Keep playback state - ESP32 is actively trying to reconnect
        app->is_connected = false;
        FURI_LOG_W(TAG, "BT connection dropped, ESP32 will auto-reconnect...");
        // Notify user with vibration
        notification_message(app->notifications, &sequence_single_vibro);
        // Don't send BtAudioEventDisconnected - keep player view active
    } else if(strncmp(data, "BT_DISCONNECTED", 15) == 0) {
        // Bluetooth connection dropped - keep playback state for potential auto-reconnect
        // Don't clear is_playing/is_paused yet - ESP32 may auto-reconnect
        app->is_connected = false;
        FURI_LOG_W(TAG, "BT connection dropped, waiting for auto-reconnect...");
        // Notify user with vibration
        notification_message(app->notifications, &sequence_single_vibro);
        // Don't send BtAudioEventDisconnected - give time for auto-reconnect
    } else if(strncmp(data, "BT_RECONNECTED", 14) == 0) {
        // Bluetooth auto-reconnected successfully
        app->is_connected = true;
        app->reconnect_in_progress = false;
        FURI_LOG_I(TAG, "BT auto-reconnected!");
        // Provide positive feedback
        notification_message(app->notifications, &sequence_success);
        view_dispatcher_send_custom_event(app->view_dispatcher, BtAudioEventResumed);
    } else if(strncmp(data, "RECONNECT_ATTEMPT", 17) == 0) {
        // ESP32 is attempting to reconnect
        FURI_LOG_I(TAG, "ESP32 attempting reconnection...");
    } else if(strncmp(data, "RECONNECT_TIMEOUT", 17) == 0) {
        // ESP32 gave up on reconnection
        FURI_LOG_W(TAG, "ESP32 reconnection timeout, giving up");
        app->is_connected = false;
        app->is_playing = false;
        app->is_paused = false;
        app->reconnect_in_progress = false;
        view_dispatcher_send_custom_event(app->view_dispatcher, BtAudioEventDisconnected);
    } else if(strncmp(data, "RECONNECTING", 12) == 0) {
        // ESP32 is attempting to reconnect (old message format)
        FURI_LOG_I(TAG, "ESP32 attempting reconnection...");
    } else if(strncmp(data, "CONNECTED", 9) == 0) {
        // Connection successful (from manual Reconnect command)
        if(app->connect_timer) furi_timer_stop(app->connect_timer);
        app->is_connected = true;
        app->reconnect_in_progress = false;
        FURI_LOG_I(TAG, "Reconnection successful!");
        notification_message(app->notifications, &sequence_success);
        view_dispatcher_send_custom_event(app->view_dispatcher, BtAudioEventReconnectSuccess);
    } else if(strncmp(data, "FAILED", 6) == 0) {
        // Connection failed (from manual Reconnect command)
        if(app->connect_timer) furi_timer_stop(app->connect_timer);
        app->is_connected = false;
        app->reconnect_in_progress = false;
        FURI_LOG_W(TAG, "Reconnection failed");
        view_dispatcher_send_custom_event(app->view_dispatcher, BtAudioEventReconnectFailed);
    } else if(
        strncmp(data, "SD_ERROR", 8) == 0 || strncmp(data, "NO_FILES", 8) == 0 ||
        strncmp(data, "NO_FAVORITES", 12) == 0 || strncmp(data, "NO_PLAYLIST", 11) == 0 ||
        strncmp(data, "NO_PLAYLIST2", 12) == 0) {
        // SD card or file errors
        FURI_LOG_W(TAG, "ESP32 error: %s", data);
        app->is_playing = false;
        app->is_paused = false;
        app->playing_favorites = false;
        view_dispatcher_send_custom_event(app->view_dispatcher, BtAudioEventPlayComplete);
    } else if(strncmp(data, "DISCONNECTED", 12) == 0) {
        // Full disconnection (user requested or final)
        // Ignore if we're in the middle of a reconnection sequence
        if(app->reconnect_in_progress) {
            FURI_LOG_I(TAG, "Ignoring DISCONNECTED during reconnection sequence");
        } else {
            app->is_connected = false;
            app->is_playing = false;
            app->is_paused = false;
            app->playing_favorites = false;
            view_dispatcher_send_custom_event(app->view_dispatcher, BtAudioEventDisconnected);
        }
    } else if(strncmp(data, "ERROR:", 6) == 0) {
        FURI_LOG_W(TAG, "ESP32 error: %s", data + 6);
        // Check for "Not connected" error - update connection state
        // But ignore during reconnection sequence
        if(strstr(data + 6, "Not connected") != NULL && !app->reconnect_in_progress) {
            app->is_connected = false;
            app->is_playing = false;
            app->is_paused = false;
            view_dispatcher_send_custom_event(app->view_dispatcher, BtAudioEventDisconnected);
        }
        // Clear pending WiFi stream on any error
        wifi_stream_pending = false;
    } else if(strncmp(data, "WIFI_CONNECTED:", 15) == 0) {
        // WiFi connected successfully - now send stream commands if pending
        FURI_LOG_I(TAG, "WiFi connected: %s", data + 15);
        if(wifi_stream_pending) {
            // WiFi is now connected - send stream URL and start command
            char stream_cmd[BT_AUDIO_STREAM_URL_LEN + 16];
            snprintf(
                stream_cmd, sizeof(stream_cmd), "STREAM_URL:%s\n", app->wifi_config.stream_url);
            bt_audio_uart_tx(app->uart, stream_cmd);

            bt_audio_uart_tx(app->uart, "STREAM_START\n");
            wifi_stream_pending = false;
        }
    } else if(strncmp(data, "WIFI_FAILED", 11) == 0) {
        // WiFi connection failed
        FURI_LOG_W(TAG, "WiFi connection failed");
        wifi_stream_pending = false;
        app->is_playing = false;
        app->is_paused = false;
        // Update display to show error
        snprintf(app->current_filename, BT_AUDIO_FILENAME_LEN, "WiFi connection failed");
        notification_message(app->notifications, &sequence_error);
        view_dispatcher_send_custom_event(app->view_dispatcher, BtAudioEventPlayComplete);
    } else if(strncmp(data, "STREAM_ERROR:", 13) == 0) {
        // Stream error (HTTP error code or other stream issue)
        FURI_LOG_W(TAG, "Stream error: %s", data + 13);
        wifi_stream_pending = false;
        app->is_playing = false;
        app->is_paused = false;
        // Update display to show error - limit error message length to prevent buffer overflow
        snprintf(app->current_filename, BT_AUDIO_FILENAME_LEN, "Stream error: %.100s", data + 13);
        notification_message(app->notifications, &sequence_error);
        view_dispatcher_send_custom_event(app->view_dispatcher, BtAudioEventPlayComplete);
    } else if(strncmp(data, "STREAM_CONNECTED", 16) == 0) {
        // Stream HTTP connection successful, now buffering
        FURI_LOG_I(TAG, "Stream connected, starting buffer");
        snprintf(app->current_filename, BT_AUDIO_FILENAME_LEN, "Connected, buffering...");
        view_dispatcher_send_custom_event(app->view_dispatcher, BtAudioEventFileChanged);
    } else if(strncmp(data, "STREAM_BUFFERING", 16) == 0) {
        // Stream is pre-buffering data before playback
        FURI_LOG_I(TAG, "Stream buffering");
        snprintf(app->current_filename, BT_AUDIO_FILENAME_LEN, "Buffering stream...");
        view_dispatcher_send_custom_event(app->view_dispatcher, BtAudioEventFileChanged);
    } else if(strncmp(data, "STREAM_BUFFER_UNDERRUN", 22) == 0) {
        // Stream buffer ran dry - network may be slow
        FURI_LOG_W(TAG, "Stream buffer underrun - waiting for data");
        // Don't change display - just log it
    } else if(strncmp(data, "STREAM_BUFFER_RECOVERED", 23) == 0) {
        // Stream buffer recovered from underrun
        FURI_LOG_I(TAG, "Stream buffer recovered");
    } else if(strncmp(data, "STREAM_STARTED", 14) == 0) {
        // Stream started successfully (buffering complete)
        FURI_LOG_I(TAG, "Stream started");
        wifi_stream_pending = false;
        // Confirm playing state and update display with stream URL
        app->is_playing = true;
        app->is_paused = false;
        // Truncate URL for display to fit in filename buffer
        // "Streaming: " is 11 chars, leaving 116 chars for URL (128 - 11 - 1 for null)
        snprintf(
            app->current_filename,
            BT_AUDIO_FILENAME_LEN,
            "Streaming: %.116s",
            app->wifi_config.stream_url);
        view_dispatcher_send_custom_event(app->view_dispatcher, BtAudioEventFileChanged);
    }
}

static void bt_audio_scene_control_submenu_callback(void* context, uint32_t index) {
    BtAudio* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void bt_audio_scene_control_on_enter(void* context) {
    BtAudio* app = context;
    Submenu* submenu = app->submenu;

    // Mark as connected when entering control scene
    app->is_connected = true;

    // Note: Don't reset is_playing/is_paused here - preserve state when returning from player view
    if(!app->is_playing && !app->is_paused) {
        app->current_filename[0] = '\0';
        app->current_is_favorite = false;
    }
    app->scroll_offset = 0;
    app->last_left_press = 0;

    // Set up UART callback for responses
    bt_audio_uart_set_rx_callback(app->uart, bt_audio_control_rx_callback);

    // Configure player view
    view_set_draw_callback(app->player_view, bt_audio_player_draw_callback);
    view_set_input_callback(app->player_view, bt_audio_player_input_callback);
    view_set_context(app->player_view, app);

    // Apply backlight setting - use enforce functions for persistent effect
    if(app->config.keep_backlight_on) {
        notification_message(app->notifications, &sequence_display_backlight_enforce_on);
    }

    // Check if we should go directly to player view (e.g., when coming from file browser after selecting a file)
    // Scene state 1 means "go directly to player view"
    uint32_t scene_state = scene_manager_get_scene_state(app->scene_manager, BtAudioSceneControl);
    if(scene_state == 1 && (app->is_playing || app->is_paused)) {
        // Clear the state so next time we show menu normally
        scene_manager_set_scene_state(app->scene_manager, BtAudioSceneControl, 0);

        // Start scroll timer for filename scrolling
        if(!app->scroll_timer) {
            app->scroll_timer =
                furi_timer_alloc(bt_audio_scroll_timer_cb, FuriTimerTypePeriodic, app);
        }
        furi_timer_start(app->scroll_timer, furi_ms_to_ticks(SCROLL_TICK_MS));

        // Update favorite status for current track
        app->current_is_favorite = bt_audio_is_favorite(app, app->current_filename);

        FURI_LOG_I(TAG, "Going directly to player view (from file browser)");
        view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewPlayer);
        return;
    }

    // Clear scene state if it was set but conditions weren't met
    if(scene_state == 1) {
        scene_manager_set_scene_state(app->scene_manager, BtAudioSceneControl, 0);
    }

    // Track whether "Now Playing" is shown at the top
    bool now_playing_shown = (app->is_playing || app->is_paused);

    FURI_LOG_I(
        TAG,
        "Control scene: building menu, now_playing_shown=%d (is_playing=%d, is_paused=%d)",
        now_playing_shown,
        app->is_playing,
        app->is_paused);

    // Add "Now Playing" item if audio is currently playing
    if(now_playing_shown) {
        submenu_add_item(
            submenu,
            "Now Playing",
            SubmenuIndexNowPlaying,
            bt_audio_scene_control_submenu_callback,
            app);
        FURI_LOG_I(TAG, "Added 'Now Playing' menu item");
    }

    // Add "Browse Music" option to select specific files
    submenu_add_item(
        submenu,
        "Browse Music",
        SubmenuIndexBrowseMusic,
        bt_audio_scene_control_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "Play Music Folder",
        SubmenuIndexPlayAudio,
        bt_audio_scene_control_submenu_callback,
        app);

    // Add "Play Playlist 1" option for playlist1.m3u
    submenu_add_item(
        submenu,
        "Play Playlist 1",
        SubmenuIndexPlayPlaylist,
        bt_audio_scene_control_submenu_callback,
        app);

    // Add "Play Playlist 2" option for playlist2.m3u
    submenu_add_item(
        submenu,
        "Play Playlist 2",
        SubmenuIndexPlayPlaylist2,
        bt_audio_scene_control_submenu_callback,
        app);

    // Add "Play Favorites" if there are favorites
    if(app->favorites_count > 0) {
        char fav_label[32];
        snprintf(fav_label, sizeof(fav_label), "Play Favorites (%d)", app->favorites_count);
        submenu_add_item(
            submenu,
            fav_label,
            SubmenuIndexPlayFavorites,
            bt_audio_scene_control_submenu_callback,
            app);
    }

    // WiFi Streaming DISABLED - ESP32 BT/WiFi coexistence limitation
    // The ESP32 (original) cannot reliably run WiFi and Bluetooth A2DP simultaneously
    // because they share the same 2.4GHz radio. WiFi initialization fails when BT is active.
    // This feature is kept in code for future ESP32-S3 support which has better coexistence.
    // See WIFI_STREAMING_IMPLEMENTATION.md for technical details.
    //
    // Uncomment below to re-enable (experimental, may not work on ESP32):
    // if(app->wifi_config.wifi_enabled &&
    //    app->wifi_config.ssid[0] != '\0' &&
    //    app->wifi_config.stream_url[0] != '\0') {
    //     submenu_add_item(
    //         submenu,
    //         "Stream Audio (WiFi) [WIP]",
    //         SubmenuIndexStreamAudio,
    //         bt_audio_scene_control_submenu_callback,
    //         app);
    // }
    (void)SubmenuIndexStreamAudio; // Suppress unused warning

    submenu_add_item(
        submenu,
        "Play Test Tone",
        SubmenuIndexPlayTone,
        bt_audio_scene_control_submenu_callback,
        app);
    submenu_add_item(
        submenu,
        "Play Jingle",
        SubmenuIndexPlayJingle,
        bt_audio_scene_control_submenu_callback,
        app);
    submenu_add_item(
        submenu,
        "Stop Playback",
        SubmenuIndexStopPlayback,
        bt_audio_scene_control_submenu_callback,
        app);
    // Add "Reconnect" option - useful when BT connection drops and auto-reconnect fails
    submenu_add_item(
        submenu, "Reconnect", SubmenuIndexReconnect, bt_audio_scene_control_submenu_callback, app);
    submenu_add_item(
        submenu,
        "Disconnect",
        SubmenuIndexDisconnect,
        bt_audio_scene_control_submenu_callback,
        app);

    // If "Now Playing" is shown, select it by index value
    if(now_playing_shown) {
        submenu_set_selected_item(submenu, SubmenuIndexNowPlaying);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewSubmenu);
}

bool bt_audio_scene_control_on_event(void* context, SceneManagerEvent event) {
    BtAudio* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubmenuIndexNowPlaying) {
            // Return to player view
            // Restart scroll timer if we have a long filename
            char display_name[BT_AUDIO_FILENAME_LEN];
            extract_filename(app->current_filename, display_name, sizeof(display_name));
            size_t text_len = strlen(display_name);
            int text_width = text_len * FONT_SECONDARY_CHAR_WIDTH;

            if(text_width > SCREEN_WIDTH - 8) {
                if(!app->scroll_timer) {
                    app->scroll_timer =
                        furi_timer_alloc(bt_audio_scroll_timer_cb, FuriTimerTypePeriodic, app);
                }
                furi_timer_start(app->scroll_timer, furi_ms_to_ticks(SCROLL_TICK_MS));
            }

            view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewPlayer);
            consumed = true;
        } else if(event.event == SubmenuIndexBrowseMusic) {
            // Open file browser scene to select specific MP3 files
            scene_manager_next_scene(app->scene_manager, BtAudioSceneFileBrowser);
            consumed = true;
        } else if(event.event == SubmenuIndexPlayAudio) {
            // Check SD source setting to determine playback method
            if(app->config.sd_source == BtAudioSdSourceFlipper) {
                // Flipper SD mode - stream from Flipper's SD card
                // Note: This initial implementation plays the first MP3 file found in /ext/music
                // A file browser for selection may be added in a future update
                FURI_LOG_I(TAG, "Starting Flipper SD playback mode");

                // Show status message
                snprintf(
                    app->current_filename, BT_AUDIO_FILENAME_LEN, "Searching for MP3 files...");

                // Try to find first MP3 in the music folder on Flipper SD
                Storage* storage = furi_record_open(RECORD_STORAGE);
                File* dir = storage_file_alloc(storage);

                bool found_file = false;
                char first_mp3[128] = {0};

                if(storage_dir_open(dir, "/ext/music")) {
                    FileInfo fileinfo;
                    char name[64];

                    while(storage_dir_read(dir, &fileinfo, name, sizeof(name))) {
                        // Skip directories - only process files
                        if(file_info_is_dir(&fileinfo)) {
                            continue;
                        }
                        // Check for MP3 extension (case insensitive)
                        size_t len = strlen(name);
                        if(len > 4 && (strcmp(name + len - 4, ".mp3") == 0 ||
                                       strcmp(name + len - 4, ".MP3") == 0)) {
                            snprintf(first_mp3, sizeof(first_mp3), "/ext/music/%s", name);
                            found_file = true;
                            FURI_LOG_I(TAG, "Found MP3 file: %s", first_mp3);
                            break;
                        }
                    }
                    storage_dir_close(dir);
                }

                storage_file_free(dir);
                furi_record_close(RECORD_STORAGE);

                if(found_file) {
                    // Start streaming the file
                    app->is_playing = true;
                    app->is_paused = false;
                    app->playing_favorites = false;
                    app->scroll_offset = 0;

                    // Extract filename for display (snprintf ensures null-termination)
                    const char* basename = strrchr(first_mp3, '/');
                    if(basename) {
                        snprintf(app->current_filename, BT_AUDIO_FILENAME_LEN, "%s", basename + 1);
                    } else {
                        snprintf(app->current_filename, BT_AUDIO_FILENAME_LEN, "%s", first_mp3);
                    }

                    // Start scroll timer for filename scrolling
                    if(!app->scroll_timer) {
                        app->scroll_timer =
                            furi_timer_alloc(bt_audio_scroll_timer_cb, FuriTimerTypePeriodic, app);
                    }
                    furi_timer_start(app->scroll_timer, furi_ms_to_ticks(SCROLL_TICK_MS));

                    // Switch to player view first (shows "loading" state)
                    view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewPlayer);

                    // Start the streaming - this blocks while streaming
                    // Note: Future improvement should use a background thread
                    if(!bt_audio_stream_flipper_sd_file(app, first_mp3)) {
                        FURI_LOG_E(TAG, "Failed to stream file from Flipper SD");
                        snprintf(app->current_filename, BT_AUDIO_FILENAME_LEN, "Stream failed");
                        app->is_playing = false;
                    }
                } else {
                    // No MP3 files found
                    FURI_LOG_W(TAG, "No MP3 files found in /ext/music");
                    widget_reset(app->widget);
                    widget_add_string_element(
                        app->widget, 64, 20, AlignCenter, AlignCenter, FontPrimary, "No MP3 Files");
                    widget_add_string_element(
                        app->widget,
                        64,
                        36,
                        AlignCenter,
                        AlignCenter,
                        FontSecondary,
                        "Add files to");
                    widget_add_string_element(
                        app->widget,
                        64,
                        48,
                        AlignCenter,
                        AlignCenter,
                        FontSecondary,
                        "/ext/music/");
                    view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewWidget);
                }
            } else {
                // ESP32 SD mode - send PLAY_MP3 command to ESP32
                bt_audio_uart_tx(app->uart, "PLAY_MP3\n");
                app->is_playing = true;
                app->is_paused = false;
                app->playing_favorites = false;
                app->scroll_offset = 0;

                // Start scroll timer for filename scrolling
                if(!app->scroll_timer) {
                    app->scroll_timer =
                        furi_timer_alloc(bt_audio_scroll_timer_cb, FuriTimerTypePeriodic, app);
                }
                furi_timer_start(app->scroll_timer, furi_ms_to_ticks(SCROLL_TICK_MS));

                // Switch to player view
                view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewPlayer);
            }

            consumed = true;
        } else if(event.event == SubmenuIndexPlayFavorites) {
            // Send favorites list to ESP32 and start playing
            // Format: PLAY_FAVORITES:<count>\n followed by filenames
            if(app->favorites_count > 0) {
                char cmd[64];
                snprintf(cmd, sizeof(cmd), "PLAY_FAVORITES:%d\n", app->favorites_count);
                bt_audio_uart_tx(app->uart, cmd);

                // Send each favorite filename with delay proportional to command length
                // At 115200 baud, ~11.5 bytes/ms, so need to wait for transmission plus processing
                // Formula: ((cmd_length + 9) / 10) + 15ms base for ESP32 processing
                for(uint8_t i = 0; i < app->favorites_count; i++) {
                    char fav_cmd[BT_AUDIO_FILENAME_LEN + 8];
                    int cmd_len =
                        snprintf(fav_cmd, sizeof(fav_cmd), "FAV:%s\n", app->favorites[i]);
                    bt_audio_uart_tx(app->uart, fav_cmd);
                    // Dynamic delay: transmission time (~cmd_len/10 ms) + processing time (15ms)
                    // Use (cmd_len + 9) / 10 to round up integer division
                    if((cmd_len > 0) && (cmd_len < (int)sizeof(fav_cmd))) {
                        uint32_t delay_ms = (uint32_t)((cmd_len + 9) / 10) + 15;
                        furi_delay_ms(delay_ms);
                    } else {
                        // Safe fallback delay on snprintf error or truncation
                        furi_delay_ms(30);
                    }
                }

                // Signal end of favorites list
                bt_audio_uart_tx(app->uart, "FAV_END\n");

                app->is_playing = true;
                app->is_paused = false;
                app->playing_favorites = true;
                app->scroll_offset = 0;

                // Start scroll timer for filename scrolling
                if(!app->scroll_timer) {
                    app->scroll_timer =
                        furi_timer_alloc(bt_audio_scroll_timer_cb, FuriTimerTypePeriodic, app);
                }
                furi_timer_start(app->scroll_timer, furi_ms_to_ticks(SCROLL_TICK_MS));

                // Switch to player view
                view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewPlayer);
            }
            consumed = true;
        } else if(event.event == SubmenuIndexStreamAudio) {
            // Stream audio from WiFi
            // First, stop any existing playback to ensure clean transition
            bt_audio_uart_tx(app->uart, "STOP\n");
            furi_delay_ms(100); // Delay to ensure STOP is processed

            // Set pending flag - the RX callback will send STREAM_URL and STREAM_START
            // after WIFI_CONNECTED is received
            wifi_stream_pending = true;

            // Connect to WiFi network with credentials from config
            // Buffer size: 13 (WIFI_CONNECT:) + 64 (SSID) + 1 (:) + 64 (password) + 1 (\n) + 1 (\0) = 144
            char wifi_cmd[BT_AUDIO_WIFI_SSID_LEN + BT_AUDIO_WIFI_PASS_LEN + 16];
            snprintf(
                wifi_cmd,
                sizeof(wifi_cmd),
                "WIFI_CONNECT:%s:%s\n",
                app->wifi_config.ssid,
                app->wifi_config.password);
            bt_audio_uart_tx(app->uart, wifi_cmd);

            FURI_LOG_I(TAG, "Starting WiFi stream: %s", app->wifi_config.stream_url);

            // Mark as playing (pending) and switch to player view
            app->is_playing = true;
            app->is_paused = false;
            app->playing_favorites = false;
            app->scroll_offset = 0;
            // Truncate URL for display to fit in filename buffer
            snprintf(app->current_filename, BT_AUDIO_FILENAME_LEN, "Connecting to WiFi...");

            // Start scroll timer for URL scrolling
            if(!app->scroll_timer) {
                app->scroll_timer =
                    furi_timer_alloc(bt_audio_scroll_timer_cb, FuriTimerTypePeriodic, app);
            }
            furi_timer_start(app->scroll_timer, furi_ms_to_ticks(SCROLL_TICK_MS));

            // Switch to player view
            view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewPlayer);
            consumed = true;
        } else if(event.event == SubmenuIndexPlayPlaylist) {
            // Send PLAY_PLAYLIST command to ESP32 to play from playlist1.m3u
            bt_audio_uart_tx(app->uart, "PLAY_PLAYLIST\n");
            start_playlist_playback(app);
            consumed = true;
        } else if(event.event == SubmenuIndexPlayPlaylist2) {
            // Send PLAY_PLAYLIST2 command to ESP32 to play from playlist2.m3u
            bt_audio_uart_tx(app->uart, "PLAY_PLAYLIST2\n");
            start_playlist_playback(app);
            consumed = true;
        } else if(event.event == SubmenuIndexPlayTone) {
            // Send PLAY command to ESP32
            bt_audio_uart_tx(app->uart, "PLAY\n");
            app->is_playing = true;

            // Show playing notification
            widget_reset(app->widget);
            widget_add_string_element(
                app->widget, 64, 20, AlignCenter, AlignCenter, FontPrimary, "Playing");
            widget_add_string_element(
                app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Test Tone");
            widget_add_string_element(
                app->widget, 64, 50, AlignCenter, AlignCenter, FontSecondary, "Please wait...");
            view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewWidget);

            // Start play timeout timer (3 seconds - slightly longer than the 2s tone)
            if(!app->play_timer) {
                app->play_timer =
                    furi_timer_alloc(bt_audio_play_timeout_cb, FuriTimerTypeOnce, app);
            }
            furi_timer_start(app->play_timer, furi_ms_to_ticks(3000));

            consumed = true;
        } else if(event.event == SubmenuIndexPlayJingle) {
            // Send PLAY_JINGLE command to ESP32 for Mary Had a Little Lamb
            bt_audio_uart_tx(app->uart, "PLAY_JINGLE\n");
            app->is_playing = true;

            // Show playing notification
            widget_reset(app->widget);
            widget_add_string_element(
                app->widget, 64, 20, AlignCenter, AlignCenter, FontPrimary, "Playing");
            widget_add_string_element(
                app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Mary's Lamb");
            widget_add_string_element(
                app->widget, 64, 50, AlignCenter, AlignCenter, FontSecondary, "Please wait...");
            view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewWidget);

            // Start play timeout timer (4 seconds for jingle)
            if(!app->play_timer) {
                app->play_timer =
                    furi_timer_alloc(bt_audio_play_timeout_cb, FuriTimerTypeOnce, app);
            }
            furi_timer_start(app->play_timer, furi_ms_to_ticks(4000));

            consumed = true;
        } else if(event.event == SubmenuIndexStopPlayback) {
            // Send STOP command to ESP32
            bt_audio_uart_tx(app->uart, "STOP\n");
            app->is_playing = false;
            app->is_paused = false;
            if(app->play_timer) furi_timer_stop(app->play_timer);
            if(app->scroll_timer) furi_timer_stop(app->scroll_timer);
            consumed = true;
        } else if(event.event == SubmenuIndexReconnect) {
            // Force reconnection to last BT device
            // This mimics the "Last devices" workflow which is known to work:
            // 1. First disconnect cleanly
            // 2. Wait for ESP32 to process
            // 3. Then connect using the proper CONNECT <MAC> command

            bool power_ok = true;
            bool power_was_off = false;

            // Set reconnect_in_progress flag BEFORE any operations that might trigger disconnect
            app->reconnect_in_progress = true;

            // First check if 5V power is available (may have been lost if USB-C was removed)
            if(app->config.enable_5v_gpio && !furi_hal_power_is_otg_enabled()) {
                power_was_off = true;
                // Try to re-enable 5V power
                furi_hal_power_enable_otg();
                furi_delay_ms(100); // Wait for power to stabilize
                if(!furi_hal_power_is_otg_enabled()) {
                    // 5V power failed - usually happens when USB-C is connected
                    // USB-C prevents GPIO 5V output (OTG mode conflict)
                    app->reconnect_in_progress = false; // Clear flag on failure
                    widget_reset(app->widget);
                    widget_add_string_element(
                        app->widget,
                        64,
                        8,
                        AlignCenter,
                        AlignCenter,
                        FontPrimary,
                        "5V GPIO Blocked");
                    widget_add_string_element(
                        app->widget,
                        64,
                        22,
                        AlignCenter,
                        AlignCenter,
                        FontSecondary,
                        "USB-C blocks GPIO power");
                    widget_add_string_element(
                        app->widget,
                        64,
                        36,
                        AlignCenter,
                        AlignCenter,
                        FontSecondary,
                        "Unplug USB to use GPIO");
                    widget_add_string_element(
                        app->widget,
                        64,
                        50,
                        AlignCenter,
                        AlignCenter,
                        FontSecondary,
                        "or power ESP32 externally");
                    view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewWidget);
                    power_ok = false;
                } else {
                    FURI_LOG_I(TAG, "Re-enabled 5V on GPIO for reconnection");
                }
            }

            if(power_ok) {
                // Check if we have a last connected MAC address
                if(app->config.last_connected_mac[0] == '\0') {
                    // No last device - show error
                    app->reconnect_in_progress = false; // Clear flag on failure
                    widget_reset(app->widget);
                    widget_add_string_element(
                        app->widget, 64, 24, AlignCenter, AlignCenter, FontPrimary, "No Device");
                    widget_add_string_element(
                        app->widget, 64, 40, AlignCenter, AlignCenter, FontSecondary, "Scan first");
                    view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewWidget);
                } else {
                    // If power was off, ESP32 needs time to boot (typically ~1-2 seconds)
                    if(power_was_off) {
                        FURI_LOG_I(TAG, "Waiting for ESP32 to boot after power restore...");
                        furi_delay_ms(1500); // Wait for ESP32 to boot
                    }

                    // Send disconnect first to clean up any stale connection state
                    // Only send once and wait for it to complete
                    bt_audio_uart_tx(app->uart, "DISCONNECT\n");
                    furi_delay_ms(500); // Give ESP32 time to fully process disconnect

                    // Clear reconnect flag before going to connect scene
                    app->reconnect_in_progress = false;

                    // Reset connection state
                    app->is_connected = false;
                    app->is_playing = false;
                    app->is_paused = false;
                    if(app->scroll_timer) furi_timer_stop(app->scroll_timer);

                    // Use the same approach as "Last devices" - set up device list and go to connect scene
                    // This is the workflow that works reliably

                    // Find device name in history if available
                    const char* device_name = "Saved Device";
                    for(uint8_t i = 0; i < app->device_history_count; i++) {
                        if(strcmp(app->device_history[i].mac, app->config.last_connected_mac) ==
                           0) {
                            device_name = app->device_history[i].name;
                            break;
                        }
                    }

                    // Copy MAC address to device list slot 0 for connection
                    // Format: "MAC,Name" - connect scene will extract MAC
                    // MAC is 17 chars (AA:BB:CC:DD:EE:FF), truncate name to fit buffer:
                    // 64 - 17 (MAC) - 1 (comma) - 1 (null) = 45 chars max for name
                    snprintf(
                        app->device_list[0],
                        BT_AUDIO_DEVICE_NAME_LEN,
                        "%.17s,%.45s",
                        app->config.last_connected_mac,
                        device_name);
                    app->device_count = 1;
                    app->selected_device = 0;

                    FURI_LOG_I(TAG, "Reconnecting to: %s", app->config.last_connected_mac);

                    // Go to connect scene - same workflow as "Last devices"
                    scene_manager_next_scene(app->scene_manager, BtAudioSceneConnect);
                }
            }

            consumed = true;
        } else if(event.event == SubmenuIndexDisconnect) {
            // Send DISCONNECT command
            bt_audio_uart_tx(app->uart, "DISCONNECT\n");
            app->is_connected = false;
            app->is_playing = false;
            app->is_paused = false;
            if(app->scroll_timer) furi_timer_stop(app->scroll_timer);

            // Restore backlight to auto mode when disconnecting
            if(app->config.keep_backlight_on) {
                notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
                FURI_LOG_I(TAG, "Backlight restored to auto (disconnect)");
            }

            // Go back to start
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, BtAudioSceneStart);
            consumed = true;
        } else if(event.event == BtAudioEventPlayComplete) {
            // Play completed - return to control menu
            if(app->play_timer) furi_timer_stop(app->play_timer);
            if(app->scroll_timer) furi_timer_stop(app->scroll_timer);
            app->is_playing = false;
            app->is_paused = false;
            app->current_filename[0] = '\0';

            // Restore backlight to auto mode when playback stops
            if(app->config.keep_backlight_on) {
                notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
                FURI_LOG_I(TAG, "Backlight restored to auto (playback complete)");
            }

            // Ensure UART callback is set for this scene (other scenes may set different callbacks)
            bt_audio_uart_set_rx_callback(app->uart, bt_audio_control_rx_callback);

            // Switch back to submenu
            view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewSubmenu);
            consumed = true;
        } else if(event.event == BtAudioEventDisconnected) {
            // Device disconnected unexpectedly
            app->is_connected = false;
            app->is_playing = false;
            app->is_paused = false;
            if(app->play_timer) furi_timer_stop(app->play_timer);
            if(app->scroll_timer) furi_timer_stop(app->scroll_timer);

            // Restore backlight to auto mode when disconnected
            if(app->config.keep_backlight_on) {
                notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
                FURI_LOG_I(TAG, "Backlight restored to auto (disconnected)");
            }

            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, BtAudioSceneStart);
            consumed = true;
        } else if(event.event == BtAudioEventFileChanged) {
            // File changed - reset scroll and force immediate redraw
            app->scroll_offset = 0;
            view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewPlayer);
            consumed = true;
        } else if(event.event == BtAudioEventPaused) {
            // Playback paused - restore backlight to auto since not actively playing
            if(app->config.keep_backlight_on) {
                notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
                FURI_LOG_I(TAG, "Backlight restored to auto (paused)");
            }
            // Force immediate redraw for visual feedback
            view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewPlayer);
            consumed = true;
        } else if(event.event == BtAudioEventResumed) {
            // Playback resumed - re-enable backlight always on if configured
            if(app->config.keep_backlight_on) {
                notification_message(app->notifications, &sequence_display_backlight_enforce_on);
                FURI_LOG_I(TAG, "Backlight enforced on (resumed)");
            }
            // Force immediate redraw for visual feedback
            view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewPlayer);
            consumed = true;
        } else if(event.event == BtAudioEventScrollTick) {
            // Update scroll offset for long filenames
            char display_name[BT_AUDIO_FILENAME_LEN];
            extract_filename(app->current_filename, display_name, sizeof(display_name));
            size_t text_len = strlen(display_name);
            int text_width = text_len * FONT_SECONDARY_CHAR_WIDTH;

            if(text_width > SCREEN_WIDTH - 8) {
                app->scroll_offset += SCROLL_PIXELS;
                if(app->scroll_offset > text_width - SCREEN_WIDTH + 32) {
                    app->scroll_offset = 0; // Reset to beginning
                }
            }
            // The view dispatcher will automatically redraw the active view
            consumed = true;
        } else if(event.event == BtAudioEventReconnectSuccess) {
            // Reconnection successful - return to control menu
            if(app->connect_timer) furi_timer_stop(app->connect_timer);
            app->is_connected = true;
            app->reconnect_in_progress = false; // Clear flag on success

            // Re-enable backlight always on if that setting is enabled
            if(app->config.keep_backlight_on) {
                notification_message(app->notifications, &sequence_display_backlight_enforce_on);
            }

            // Rebuild and show control menu
            submenu_reset(app->submenu);
            bt_audio_scene_control_on_enter(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewSubmenu);
            consumed = true;
        } else if(event.event == BtAudioEventReconnectFailed) {
            // Reconnection failed - show error then return to menu
            if(app->connect_timer) furi_timer_stop(app->connect_timer);
            app->is_connected = false;
            app->reconnect_in_progress = false; // Clear flag on failure

            widget_reset(app->widget);
            widget_add_string_element(
                app->widget, 64, 12, AlignCenter, AlignCenter, FontPrimary, "Reconnection");
            widget_add_string_element(
                app->widget, 64, 24, AlignCenter, AlignCenter, FontPrimary, "Failed");
            widget_add_string_element(
                app->widget, 64, 40, AlignCenter, AlignCenter, FontSecondary, "Put device in");
            widget_add_string_element(
                app->widget, 64, 52, AlignCenter, AlignCenter, FontSecondary, "pairing mode");
            // Widget will show until user presses back
            consumed = true;
        } else if(event.event == BtAudioEventConnectTimeout) {
            // Reconnection timed out
            if(app->connect_timer) furi_timer_stop(app->connect_timer);
            app->is_connected = false;
            app->reconnect_in_progress = false; // Clear flag on timeout

            widget_reset(app->widget);
            widget_add_string_element(
                app->widget, 64, 12, AlignCenter, AlignCenter, FontPrimary, "Reconnection");
            widget_add_string_element(
                app->widget, 64, 24, AlignCenter, AlignCenter, FontPrimary, "Timed Out");
            widget_add_string_element(
                app->widget, 64, 40, AlignCenter, AlignCenter, FontSecondary, "Device may need");
            widget_add_string_element(
                app->widget, 64, 52, AlignCenter, AlignCenter, FontSecondary, "pairing mode");
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // Handle back button press
        // Allow navigation back to start menu (disconnect/settings) even while playing
        // Audio will continue playing in background until explicitly stopped or disconnected
        if(app->scroll_timer) furi_timer_stop(app->scroll_timer);
        // Return to start scene - this allows access to disconnect and other options
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, BtAudioSceneStart);
        consumed = true;
    }

    return consumed;
}

void bt_audio_scene_control_on_exit(void* context) {
    BtAudio* app = context;
    submenu_reset(app->submenu);
    widget_reset(app->widget);
    if(app->play_timer) {
        furi_timer_stop(app->play_timer);
    }
    if(app->scroll_timer) {
        furi_timer_stop(app->scroll_timer);
    }
}
