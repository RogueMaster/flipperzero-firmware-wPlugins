/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * Confirming that the stored details are to be deleted.
 *
 * Deleting is not undoable and the values are tedious to type again, so it is
 * asked for twice; and because the point of the question is that the data
 * really leaves the card, the screen afterwards says so plainly.
 */
#include "../emrtd_i.h"

/** What the scene is showing; kept in the scene state across on_enter. */
typedef enum {
    EmrtdSceneForgetStateAsk,
    EmrtdSceneForgetStateDone,
} EmrtdSceneForgetState;

static void emrtd_scene_forget_confirm_button_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    furi_assert(context);
    Emrtd* app = context;

    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, result);
    }
}

static void emrtd_scene_forget_confirm_popup_callback(void* context) {
    furi_assert(context);
    Emrtd* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, EmrtdCustomEventViewExit);
}

void emrtd_scene_forget_confirm_on_enter(void* context) {
    furi_assert(context);
    Emrtd* app = context;
    Widget* widget = app->widget;

    scene_manager_set_scene_state(
        app->scene_manager, EmrtdSceneForgetConfirm, EmrtdSceneForgetStateAsk);

    widget_add_string_element(
        widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "Forget stored data?");
    widget_add_text_box_element(
        widget,
        0,
        16,
        128,
        32,
        AlignLeft,
        AlignTop,
        "The document number, both\ndates and the CAN will be\ndeleted from the SD card.\nThe options are kept.",
        false);
    widget_add_button_element(
        widget, GuiButtonTypeLeft, "Keep", emrtd_scene_forget_confirm_button_callback, app);
    widget_add_button_element(
        widget, GuiButtonTypeRight, "Forget", emrtd_scene_forget_confirm_button_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, EmrtdViewWidget);
}

bool emrtd_scene_forget_confirm_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    Emrtd* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeRight) {
            const bool deleted = emrtd_settings_delete(app);

            /* The options outlive the credentials, so what is left is written
             * back out; with the credentials gone the file no longer names
             * anybody. */
            emrtd_settings_save(app);

            scene_manager_set_scene_state(
                app->scene_manager, EmrtdSceneForgetConfirm, EmrtdSceneForgetStateDone);

            /*
             * The values are out of memory either way; what is in question is
             * only whether the file went with them, and a failure there has
             * to be said plainly rather than dressed up as success.
             */
            popup_set_header(
                app->popup,
                deleted ? "Forgotten" : "Not deleted",
                64,
                20,
                AlignCenter,
                AlignCenter);
            popup_set_text(
                app->popup,
                deleted ? "The details are off the card" : "The file is still on the card",
                64,
                36,
                AlignCenter,
                AlignCenter);
            popup_set_context(app->popup, app);
            popup_set_callback(app->popup, emrtd_scene_forget_confirm_popup_callback);
            popup_set_timeout(app->popup, 1500);
            popup_enable_timeout(app->popup);

            if(deleted) {
                emrtd_notify_success(app);
            } else {
                emrtd_notify_error(app);
            }

            view_dispatcher_switch_to_view(app->view_dispatcher, EmrtdViewPopup);
            consumed = true;
        } else if(event.event == GuiButtonTypeLeft || event.event == EmrtdCustomEventViewExit) {
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        /* Back on the confirmation screen means "no". Back while the result is
         * showing means the same thing the timeout does. */
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void emrtd_scene_forget_confirm_on_exit(void* context) {
    furi_assert(context);
    Emrtd* app = context;

    widget_reset(app->widget);
    popup_reset(app->popup);
}
