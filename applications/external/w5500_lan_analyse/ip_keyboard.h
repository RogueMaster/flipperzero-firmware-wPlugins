#pragma once

#include <gui/view.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * IP Keyboard — custom view for convenient IP address entry on Flipper Zero.
 *
 * Displays octets as  [192].[168].[  1].[  1]  with the active octet
 * highlighted in an inverted rounded box.  Up/Down change the value,
 * Left/Right move between octets, OK confirms.
 *
 * Optional CIDR mode adds a "/prefix" field (0-32).
 */

typedef struct IpKeyboard IpKeyboard;

/** Callback invoked when the user presses OK. */
typedef void (*IpKeyboardCallback)(void* context);

/** Allocate a new IP keyboard instance. */
IpKeyboard* ip_keyboard_alloc(void);

/** Free an IP keyboard instance. */
void ip_keyboard_free(IpKeyboard* kb);

/** Get the underlying View* (for view_dispatcher_add_view). */
View* ip_keyboard_get_view(IpKeyboard* kb);

/**
 * Configure the IP keyboard before switching to it.
 *
 * @param kb            IP keyboard instance
 * @param header        Header text shown at the top of the screen
 * @param initial_str   Initial value as a string ("x.x.x.x" or "x.x.x.x/p")
 * @param cidr_mode     If true, show an editable /prefix field
 * @param callback      Called when the user presses OK
 * @param context       Passed to the callback
 * @param result_buffer Where the formatted result string will be written
 * @param result_size   Size of result_buffer
 * @param back_callback ViewNavigationCallback for the Back button
 */
void ip_keyboard_setup(
    IpKeyboard* kb,
    const char* header,
    const char* initial_str,
    bool cidr_mode,
    IpKeyboardCallback callback,
    void* context,
    char* result_buffer,
    uint8_t result_size,
    ViewNavigationCallback back_callback);
