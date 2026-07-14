#include "ui.h"
#include "core/check.h"
#include "sd_mounter.h"

#include "notification/notification.h"
#include "gui/modules/popup.h"
#include "gui/view_holder.h"
#include <dialogs/dialogs.h>

Gui* gui;
ViewHolder* view_holder;
Popup* popup;
NotificationApp* notifications;

void show(const char* message) {
    popup_set_header(popup, "SD Card Mounter", 64, 14, AlignCenter, AlignBottom);
    popup_set_text(popup, message, 64, 60, AlignCenter, AlignBottom);
    view_holder_set_view(view_holder, popup_get_view(popup));
    furi_delay_ms(5); // Make sure it shows on the screen
}
void update_existing_popup(const char* message) {
    popup_set_text(popup, message, 64, 60, AlignCenter, AlignBottom);
    furi_thread_yield();
    furi_delay_ms(5); // Make sure it shows on the screen
}

void show_error_and_wait(const char* text, const Icon* icon, int icon_width) {
    notify(&led_red);
    furi_delay_ms(500);

    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
    DialogMessage* message = dialog_message_alloc();
    dialog_message_set_header(message, "SD Card\nMounter", 5, 2, AlignLeft, AlignTop);
    dialog_message_set_text(message, text, 2, 32, AlignLeft, AlignTop);

    dialog_message_set_icon(message, icon, 128 - icon_width, 0);
    dialog_message_set_buttons(message, NULL, NULL, "Ok");
    dialog_message_show(dialogs, message);
    dialog_message_free(message);
    furi_record_close(RECORD_DIALOGS);

    notify(NULL);
}

static const NotificationSequence reset = {
    &message_red_0,
    &message_green_0,
    &message_blue_0,
    &message_blink_stop,
    &message_vibro_off,
    &message_sound_off,
    NULL,
};
void notify(const NotificationSequence* sequence) {
    // Always reset existing notifications before sending a new one
    notification_message(notifications, &reset);
    // If the sequence is NULL, just reset
    if(sequence != NULL) {
        notification_message(notifications, sequence);
    }
}

#include <notification/notification_messages.h>

// When the back button is pressed, set a flag on the main thread
void back_button_callback(void* ctx) {
    FuriThreadId thread_id = (FuriThreadId)ctx;
    furi_thread_flags_set(thread_id, FlagBackButtonPressed);
}

// Helper method to check for the above flag
bool back_button_was_pressed(bool clear) {
    uint32_t flags = furi_thread_flags_wait(FlagBackButtonPressed, FuriFlagWaitAny, 1);
    bool pressed = flags == FlagBackButtonPressed;
    if(pressed) {
        // Only vibrate if we're also clearing the flag, as otherwise we'll vibrate multiple times per press
        if(clear)
            notify(&vibrate);
        else
            furi_thread_flags_set(furi_thread_get_current_id(), FlagBackButtonPressed);
    }
    return pressed;
}

void ui_init() {
    notifications = furi_record_open(RECORD_NOTIFICATION);
    gui = furi_record_open(RECORD_GUI);

    popup = popup_alloc();

    view_holder = view_holder_alloc();
    view_holder_attach_to_gui(view_holder, gui);
    view_holder_set_back_callback(view_holder, back_button_callback, furi_thread_get_current_id());

    FURI_LOG_I("DBG", "Current thread ID: %p", furi_thread_get_current_id());
}

void ui_cleanup() {
    if(view_holder != NULL) {
        view_holder_set_view(view_holder, NULL);
        view_holder_free(view_holder);
        view_holder = NULL;
    }

    if(popup != NULL) {
        popup_free(popup);
        popup = NULL;
    }

    if(gui != NULL) {
        furi_record_close(RECORD_GUI);
        gui = NULL;
    }

    if(notifications != NULL) {
        notify(&reset);
        furi_record_close(RECORD_NOTIFICATION);
        notifications = NULL;
    }
}
