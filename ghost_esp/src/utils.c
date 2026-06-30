#include "utils.h"
#include "app_state.h"
#include <storage/storage.h>
#include <furi_hal.h>
#include <toolbox/path.h> // For EXT_PATH

// Structure to hold confirmation dialog data
typedef struct {
    const char* header;
    const char* text;
    ConfirmationViewCallback ok_callback;
    ConfirmationViewCallback cancel_callback;
} ConfirmationDialogData;

static ConfirmationDialogData current_dialog;

void show_confirmation_dialog_ex(
    void* context,
    const char* header,
    const char* text,
    ConfirmationViewCallback ok_callback,
    ConfirmationViewCallback cancel_callback) {
    FURI_LOG_D("ConfDialog", "Starting dialog, context: %p", context);

    AppState* state = (AppState*)context;
    if(!state) {
        FURI_LOG_E("ConfDialog", "Null state");
        return;
    }

    // Don't try to show dialog if we're already in confirmation view
    if(state->current_view == VIEW_CONFIRMATION) {
        FURI_LOG_W("ConfDialog", "Already in confirmation view, ignoring");
        return;
    }

    FURI_LOG_D(
        "ConfDialog",
        "Previous view: %d, Current view: %d",
        state->previous_view,
        state->current_view);

    // Allocate new context
    SettingsConfirmContext* confirm_ctx = malloc(sizeof(SettingsConfirmContext));
    if(!confirm_ctx) {
        FURI_LOG_E("ConfDialog", "Failed to allocate context");
        return;
    }

    confirm_ctx->state = state;
    if(state->active_confirm_context) {
        free(state->active_confirm_context);
    }
    state->active_confirm_context = confirm_ctx;

    // Set up the confirmation dialog
    confirmation_view_set_header(state->confirmation_view, header);
    confirmation_view_set_text(state->confirmation_view, text);
    confirmation_view_set_ok_callback(state->confirmation_view, ok_callback, confirm_ctx);
    confirmation_view_set_cancel_callback(state->confirmation_view, cancel_callback, confirm_ctx);

    FURI_LOG_D("ConfDialog", "Set callbacks - OK: %p, Cancel: %p", ok_callback, cancel_callback);

    // Save current view before switching
    state->previous_view = state->current_view;
    FURI_LOG_D("ConfDialog", "Saved previous view: %d", state->previous_view);

    // Switch to confirmation view
    view_dispatcher_switch_to_view(state->view_dispatcher, VIEW_CONFIRMATION);
    state->current_view = VIEW_CONFIRMATION;
    FURI_LOG_D("ConfDialog", "Switched to confirmation view");
}

void show_confirmation_view_wrapper(void* context, ConfirmationView* view) {
    (void)view; // Mark parameter as unused to avoid compiler warning
    // Redirect to show_confirmation_dialog_ex with the stored dialog data
    show_confirmation_dialog_ex(
        context,
        current_dialog.header,
        current_dialog.text,
        current_dialog.ok_callback,
        current_dialog.cancel_callback);
}

// 6675636B796F7564656B69
