#include "../application.h"
#include "../tonuino_nfc.h"
#include "scenes.h"

typedef struct {
    FuriThread* thread;
    TonuinoNfcThreadContext* nfc_context;
} ReadCardWaitingState;

typedef enum {
    ReadCardWaitingEventCancel,
} ReadCardWaitingEvent;

static void tonuino_scene_read_card_waiting_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    TonuinoApp* app = context;
    if(type == InputTypeShort && result == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, ReadCardWaitingEventCancel);
    }
}

void tonuino_scene_read_card_waiting_on_enter(void* context) {
    TonuinoApp* app = context;

    // Allocate scene state
    ReadCardWaitingState* state = malloc(sizeof(ReadCardWaitingState));
    state->nfc_context = malloc(sizeof(TonuinoNfcThreadContext));
    state->nfc_context->app = app;
    state->nfc_context->cancel_flag = false;
    state->nfc_context->success = false;

    // CRITICAL: Reset widget state before use
    widget_reset(app->widget);

    widget_add_string_element(
        app->widget, 64, 22, AlignCenter, AlignCenter, FontPrimary, "Waiting for card...");
    widget_add_string_element(
        app->widget, 64, 34, AlignCenter, AlignCenter, FontSecondary, "Place card on back");

    // Add cancel button
    widget_add_button_element(
        app->widget,
        GuiButtonTypeLeft,
        "Cancel",
        tonuino_scene_read_card_waiting_widget_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TonuinoViewWidget);

    // Create and start thread
    state->thread = furi_thread_alloc();
    furi_thread_set_name(state->thread, "TonuinoReadWorker");
    furi_thread_set_stack_size(state->thread, 2048);
    furi_thread_set_context(state->thread, state->nfc_context);
    furi_thread_set_callback(state->thread, tonuino_read_card_worker);
    furi_thread_start(state->thread);

    scene_manager_set_scene_state(
        app->scene_manager, TonuinoSceneReadCardWaiting, (uint32_t)state);
}

bool tonuino_scene_read_card_waiting_on_event(void* context, SceneManagerEvent event) {
    TonuinoApp* app = context;
    ReadCardWaitingState* state = (ReadCardWaitingState*)scene_manager_get_scene_state(
        app->scene_manager, TonuinoSceneReadCardWaiting);

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == ReadCardWaitingEventCancel) {
            // Set cancel flag and wait for thread to finish
            state->nfc_context->cancel_flag = true;
            notification_message(app->notifications, &sequence_error);
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        // Check if thread is still running
        if(furi_thread_get_state(state->thread) == FuriThreadStateStopped) {
            if(state->nfc_context->success) {
                notification_message(app->notifications, &sequence_success);
                scene_manager_next_scene(app->scene_manager, TonuinoSceneReadCardResult);
            } else if(!state->nfc_context->cancel_flag) {
                // Only show error if not cancelled
                notification_message(app->notifications, &sequence_error);
                widget_reset(app->widget);
                widget_add_string_element(
                    app->widget, 64, 22, AlignCenter, AlignCenter, FontPrimary, "Read Failed!");
                widget_add_string_element(
                    app->widget,
                    64,
                    34,
                    AlignCenter,
                    AlignCenter,
                    FontSecondary,
                    "No TonUINO data");
                widget_add_string_element(
                    app->widget, 64, 46, AlignCenter, AlignCenter, FontSecondary, "found on card");
                widget_add_string_element(
                    app->widget,
                    64,
                    58,
                    AlignCenter,
                    AlignCenter,
                    FontSecondary,
                    "Press Back to return");
            }
            return true;
        }
    }
    return false;
}

void tonuino_scene_read_card_waiting_on_exit(void* context) {
    TonuinoApp* app = context;
    ReadCardWaitingState* state = (ReadCardWaitingState*)scene_manager_get_scene_state(
        app->scene_manager, TonuinoSceneReadCardWaiting);

    if(state) {
        // Ensure thread is stopped
        state->nfc_context->cancel_flag = true;
        if(furi_thread_get_state(state->thread) != FuriThreadStateStopped) {
            furi_thread_join(state->thread);
        }

        furi_thread_free(state->thread);
        free(state->nfc_context);
        free(state);
    }

    widget_reset(app->widget);
}
