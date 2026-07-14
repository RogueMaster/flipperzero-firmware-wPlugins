#pragma once
#include <notification/notification_messages.h>
#include <gui/icon.h>

// Show a message and continue immediately
void show(const char* message);
// Update the existing popup with a new message
void update_existing_popup(const char* message);

// Show an error message and wait for the user to confirm
void show_error_and_wait(const char* text, const Icon* icon, int icon_width);

// Trigger a system "notification" (i.e. vibration, sound, and/or LED) sequence.
// Pass NULL to reset the notification state.
void notify(const NotificationSequence* sequence);

// Helper method to check for the "back button pressed" flag
bool back_button_was_pressed(bool clear);

void ui_init();
void ui_cleanup();

// Notification sequences
static const NotificationSequence vibrate = {
    &message_vibro_on,
    &message_delay_50,
    &message_vibro_off,
    NULL,
};
static const NotificationSequence led_red = {
    &message_red_255,
    &message_do_not_reset,
    NULL,
};
static const NotificationSequence led_green = {
    &message_green_255,
    &message_do_not_reset,
    NULL,
};
static const NotificationSequence led_blink_cyan = {
    &message_blink_start_10,
    &message_blink_set_color_cyan,
    &message_do_not_reset,
    NULL,
};
static const NotificationSequence led_blink_magenta = {
    &message_blink_start_10,
    &message_blink_set_color_magenta,
    &message_do_not_reset,
    NULL,
};
static const NotificationSequence led_blink_red = {
    &message_blink_start_10,
    &message_blink_set_color_red,
    &message_do_not_reset,
    NULL,
};
static const NotificationSequence led_blink_yellow_slow = {
    &message_blink_start_100,
    &message_blink_set_color_yellow,
    &message_do_not_reset,
    NULL,
};
