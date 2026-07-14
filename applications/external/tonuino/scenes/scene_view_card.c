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

typedef enum {
    SceneViewCardEventWrite,
} SceneViewCardEvent;

static void
    tonuino_scene_view_card_widget_callback(GuiButtonType result, InputType type, void* context) {
    TonuinoApp* app = context;

    if(type == InputTypeShort) {
        if(result == GuiButtonTypeRight) {
            scene_manager_handle_custom_event(app->scene_manager, SceneViewCardEventWrite);
        }
    }
}

void tonuino_scene_view_card_on_enter(void* context) {
    TonuinoApp* app = context;

    // CRITICAL: Reset widget state before use
    widget_reset(app->widget);

    char buffer[128];

    snprintf(buffer, sizeof(buffer), "Folder: %d", app->card_data.folder);
    widget_add_string_element(app->widget, 0, 0, AlignLeft, AlignTop, FontPrimary, buffer);

    const char* mode_str;
    if(app->card_data.mode >= 1 && app->card_data.mode <= ModeRepeatLast) {
        mode_str = mode_names[app->card_data.mode];
    } else {
        mode_str = "Unknown";
    }
    snprintf(buffer, sizeof(buffer), "Mode: %s", mode_str);
    widget_add_string_element(app->widget, 0, 12, AlignLeft, AlignTop, FontSecondary, buffer);

    snprintf(buffer, sizeof(buffer), "Special 1: %d", app->card_data.special1);
    widget_add_string_element(app->widget, 0, 24, AlignLeft, AlignTop, FontSecondary, buffer);

    snprintf(buffer, sizeof(buffer), "Special 2: %d", app->card_data.special2);
    widget_add_string_element(app->widget, 0, 36, AlignLeft, AlignTop, FontSecondary, buffer);

    widget_add_button_element(
        app->widget, GuiButtonTypeRight, "Write", tonuino_scene_view_card_widget_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TonuinoViewWidget);
}

bool tonuino_scene_view_card_on_event(void* context, SceneManagerEvent event) {
    TonuinoApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SceneViewCardEventWrite) {
            scene_manager_next_scene(app->scene_manager, TonuinoSceneWriteCardWaiting);
            return true;
        }
    }

    return false;
}

void tonuino_scene_view_card_on_exit(void* context) {
    TonuinoApp* app = context;
    widget_reset(app->widget);
}
