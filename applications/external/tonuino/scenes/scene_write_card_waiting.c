#include "../application.h"
#include "../tonuino_nfc.h"
#include "scenes.h"

typedef struct {
    FuriThread* thread;
    TonuinoNfcThreadContext* nfc_context;
} WriteCardWaitingState;

typedef enum {
    WriteCardWaitingEventCancel,
} WriteCardWaitingEvent;

static void tonuino_scene_write_card_waiting_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    TonuinoApp* app = context;
    if(type == InputTypeShort && result == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, WriteCardWaitingEventCancel);
    }
}

void tonuino_scene_write_card_waiting_on_enter(void* context) {
    TonuinoApp* app = context;

    // Allocate scene state
    WriteCardWaitingState* state = malloc(sizeof(WriteCardWaitingState));
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
        tonuino_scene_write_card_waiting_widget_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TonuinoViewWidget);

    // Create and start thread
    state->thread = furi_thread_alloc();
    furi_thread_set_name(state->thread, "TonuinoWriteWorker");
    furi_thread_set_stack_size(state->thread, 2048);
    furi_thread_set_context(state->thread, state->nfc_context);
    furi_thread_set_callback(state->thread, tonuino_write_card_worker);
    furi_thread_start(state->thread);

    scene_manager_set_scene_state(
        app->scene_manager, TonuinoSceneWriteCardWaiting, (uint32_t)state);
}

bool tonuino_scene_write_card_waiting_on_event(void* context, SceneManagerEvent event) {
    TonuinoApp* app = context;
    WriteCardWaitingState* state = (WriteCardWaitingState*)scene_manager_get_scene_state(
        app->scene_manager, TonuinoSceneWriteCardWaiting);

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == WriteCardWaitingEventCancel) {
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
                scene_manager_next_scene(app->scene_manager, TonuinoSceneWriteCardResult);
            } else if(!state->nfc_context->cancel_flag) {
                // Only show error if not cancelled
                notification_message(app->notifications, &sequence_error);
                widget_reset(app->widget);
                widget_add_string_element(
                    app->widget, 64, 28, AlignCenter, AlignCenter, FontPrimary, "Write Failed!");
                widget_add_string_element(
                    app->widget,
                    64,
                    40,
                    AlignCenter,
                    AlignCenter,
                    FontSecondary,
                    "Card not detected");
                widget_add_string_element(
                    app->widget,
                    64,
                    52,
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

void tonuino_scene_write_card_waiting_on_exit(void* context) {
    TonuinoApp* app = context;
    WriteCardWaitingState* state = (WriteCardWaitingState*)scene_manager_get_scene_state(
        app->scene_manager, TonuinoSceneWriteCardWaiting);

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
