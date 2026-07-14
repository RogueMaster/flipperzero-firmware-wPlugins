#include "../application.h"
#include "scenes.h"
#include <stdio.h>

static const char* mode_names[] = {
    "",
    "Hoerspiel (Random)",
    "Album (Complete)",
    "Party (Shuffle)",
    "Einzel (Single)",
    "Hoerbuch (Progress)",
    "Admin",
    "Hoerspiel Von-Bis",
    "Album Von-Bis",
    "Party Von-Bis",
    "Hoerbuch Einzel",
    "Repeat Last",
};

typedef struct {
    bool selected_folder; // true = folder, false = mode
    char status_message[32];
    uint32_t status_timeout;
} RapidWriteState;

typedef enum {
    SceneRapidWriteEventWrite,
} SceneRapidWriteEvent;

static void tonuino_scene_rapid_write_update_display(TonuinoApp* app) {
    RapidWriteState* state = (RapidWriteState*)scene_manager_get_scene_state(
        app->scene_manager, TonuinoSceneRapidWrite);
    if(!state) return;

    widget_reset(app->widget);

    // Check if status message should be cleared
    if(state->status_message[0] != '\0' && furi_get_tick() >= state->status_timeout) {
        state->status_message[0] = '\0';
    }

    char buffer[128];
    const char* mode_str;
    if(app->card_data.mode >= 1 && app->card_data.mode <= ModeRepeatLast) {
        mode_str = mode_names[app->card_data.mode];
    } else {
        mode_str = "Unknown";
    }

    // Display folder number at top
    if(state->selected_folder) {
        snprintf(buffer, sizeof(buffer), "> Folder: %d <", app->card_data.folder);
    } else {
        snprintf(buffer, sizeof(buffer), "Folder: %d", app->card_data.folder);
    }
    widget_add_string_element(app->widget, 64, 8, AlignCenter, AlignTop, FontPrimary, buffer);

    // Display mode below
    if(!state->selected_folder) {
        snprintf(buffer, sizeof(buffer), "> Mode: %s <", mode_str);
    } else {
        snprintf(buffer, sizeof(buffer), "Mode: %s", mode_str);
    }
    widget_add_string_element(app->widget, 64, 24, AlignCenter, AlignTop, FontSecondary, buffer);

    // Display status message or "OK = Write" prompt
    if(state->status_message[0] != '\0') {
        widget_add_string_element(
            app->widget, 64, 50, AlignCenter, AlignTop, FontSecondary, state->status_message);
    } else {
        widget_add_string_element(
            app->widget, 64, 50, AlignCenter, AlignTop, FontSecondary, "OK = Write");
    }
}

static bool tonuino_scene_rapid_write_input_callback(InputEvent* event, void* context) {
    TonuinoApp* app = context;
    RapidWriteState* state = (RapidWriteState*)scene_manager_get_scene_state(
        app->scene_manager, TonuinoSceneRapidWrite);
    if(!state) return false;

    if(event->type == InputTypeShort) {
        // Clear status message on any input (except OK which triggers write)
        if(state->status_message[0] != '\0' && event->key != InputKeyOk) {
            state->status_message[0] = '\0';
            tonuino_scene_rapid_write_update_display(app);
        }

        if(event->key == InputKeyLeft) {
            if(state->selected_folder) {
                // Decrement folder
                if(app->card_data.folder > 1) {
                    app->card_data.folder--;
                } else {
                    app->card_data.folder = 99; // Wrap around
                }
            } else {
                // Decrement mode
                if(app->card_data.mode > 1) {
                    app->card_data.mode--;
                } else {
                    app->card_data.mode = ModeRepeatLast; // Wrap around
                }
            }
            tonuino_scene_rapid_write_update_display(app);
            return true;
        } else if(event->key == InputKeyRight) {
            if(state->selected_folder) {
                // Increment folder
                if(app->card_data.folder < 99) {
                    app->card_data.folder++;
                } else {
                    app->card_data.folder = 1; // Wrap around
                }
            } else {
                // Increment mode
                if(app->card_data.mode < ModeRepeatLast) {
                    app->card_data.mode++;
                } else {
                    app->card_data.mode = 1; // Wrap around
                }
            }
            tonuino_scene_rapid_write_update_display(app);
            return true;
        } else if(event->key == InputKeyUp || event->key == InputKeyDown) {
            // Toggle between folder and mode selection
            state->selected_folder = !state->selected_folder;
            tonuino_scene_rapid_write_update_display(app);
            return true;
        } else if(event->key == InputKeyOk) {
            // OK button triggers write
            scene_manager_handle_custom_event(app->scene_manager, SceneRapidWriteEventWrite);
            return true;
        }
    }
    return false;
}

void tonuino_scene_rapid_write_on_enter(void* context) {
    TonuinoApp* app = context;

    // Allocate scene state
    RapidWriteState* state = malloc(sizeof(RapidWriteState));
    state->selected_folder = true;
    state->status_message[0] = '\0';
    state->status_timeout = 0;
    scene_manager_set_scene_state(app->scene_manager, TonuinoSceneRapidWrite, (uint32_t)state);

    // CRITICAL: Reset widget state before use
    widget_reset(app->widget);

    // Set custom input callback
    view_set_input_callback(
        widget_get_view(app->widget), tonuino_scene_rapid_write_input_callback);
    view_set_context(widget_get_view(app->widget), app);

    // Initial display update
    tonuino_scene_rapid_write_update_display(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TonuinoViewWidget);
}

bool tonuino_scene_rapid_write_on_event(void* context, SceneManagerEvent event) {
    TonuinoApp* app = context;
    RapidWriteState* state = (RapidWriteState*)scene_manager_get_scene_state(
        app->scene_manager, TonuinoSceneRapidWrite);
    if(!state) return false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SceneRapidWriteEventWrite) {
            // Trigger write card scene
            scene_manager_next_scene(app->scene_manager, TonuinoSceneWriteCardWaiting);
            return true;
        }
    }

    return false;
}

void tonuino_scene_rapid_write_on_exit(void* context) {
    TonuinoApp* app = context;

    // Clear custom input callback
    view_set_input_callback(widget_get_view(app->widget), NULL);

    // Free scene state
    RapidWriteState* state = (RapidWriteState*)scene_manager_get_scene_state(
        app->scene_manager, TonuinoSceneRapidWrite);
    if(state) {
        free(state);
        scene_manager_set_scene_state(app->scene_manager, TonuinoSceneRapidWrite, 0);
    }

    widget_reset(app->widget);
}
