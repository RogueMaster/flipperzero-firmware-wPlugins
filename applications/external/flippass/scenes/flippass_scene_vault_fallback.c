#include "flippass_scene_vault_fallback.h"

#include "../flippass.h"
#include "flippass_scene.h"
#include "flippass_scene_db_entries.h"

#include <stdio.h>

static void flippass_scene_vault_fallback_callback(
    GuiButtonType button_type,
    InputType type,
    void* context) {
    App* app = context;

    if(type != InputTypeShort) {
        return;
    }

    if(button_type == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, DialogExResultLeft);
    } else if(button_type == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(app->view_dispatcher, DialogExResultCenter);
    }
}

void flippass_scene_vault_fallback_on_enter(void* context) {
    App* app = context;
    char compact_message[192];

    snprintf(
        compact_message,
        sizeof(compact_message),
        "\e*FlipPass needs an encrypted /ext session file to finish opening this database.\e*");

    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 5, AlignCenter, AlignTop, FontPrimary, "Need /ext Session");
    widget_add_text_box_element(
        app->widget,
        0,
        16,
        128,
        40,
        AlignLeft,
        AlignTop,
        compact_message,
        true);
    widget_add_button_element(
        app->widget,
        GuiButtonTypeLeft,
        "Cancel",
        flippass_scene_vault_fallback_callback,
        app);
    widget_add_button_element(
        app->widget,
        GuiButtonTypeCenter,
        "Continue",
        flippass_scene_vault_fallback_callback,
        app);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewWidget);
}

bool flippass_scene_vault_fallback_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        flippass_reset_database(app);
        flippass_clear_master_password(app);
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, FlipPassScene_PasswordEntry);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DialogExResultLeft) {
            flippass_reset_database(app);
            flippass_clear_master_password(app);
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, FlipPassScene_PasswordEntry);
            return true;
        }

        if(event.event == DialogExResultCenter) {
            app->pending_vault_fallback = false;
            app->requested_vault_backend = KDBXVaultBackendRam;
            app->allow_ext_vault_promotion = true;
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
    }

    return false;
}

void flippass_scene_vault_fallback_on_exit(void* context) {
    App* app = context;
    widget_reset(app->widget);
}
