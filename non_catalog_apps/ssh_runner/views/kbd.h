/* On-screen keyboard for hosts, commands and passwords.
 *
 * The stock TextInput (both pinned firmwares) types only letters, digits and '_', so it
 * cannot enter user@host:port or a shell command. This is the firmware's text_input.c
 * (GPL-3.0) with the community extended key table (views/kbd_layout.c), plus three small
 * additions: password masking, a validator with an error bubble, and an example shown in
 * the empty field ("e.g. user@host", normal ink, gone at the first key). Long press = shift
 * (upper case, '_' -> space, ';' -> ':' ...); the Shift key shifts the next key, twice locks
 * upper case (as phone keyboards; letters show in upper case meanwhile); a masked character
 * shows for a second after it is typed. Long Back = backspace; Up from the top row moves the
 * text cursor. Sources: THIRD_PARTY.md.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#pragma once

#include <gui/view.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct SshKbd SshKbd;
typedef void (*SshKbdCallback)(void* context);
typedef bool (*SshKbdValidator)(const char* text, char* error, size_t error_size, void* context);

SshKbd* ssh_kbd_alloc(void);
void ssh_kbd_free(SshKbd* kbd);
View* ssh_kbd_get_view(SshKbd* kbd);
void ssh_kbd_reset(SshKbd* kbd);
void ssh_kbd_set_header(SshKbd* kbd, const char* header);
/* An example in the empty field, drawn as "e.g. <hint>" (not part of the text). */
void ssh_kbd_set_hint(SshKbd* kbd, const char* hint);
void ssh_kbd_set_result_callback(
    SshKbd* kbd,
    SshKbdCallback callback,
    void* context,
    char* buffer,
    size_t buffer_size,
    bool clear_default);
/* Start on Save instead of 'q': for a prefilled value that is usually sent as it is. Call
 * after ssh_kbd_set_result_callback. */
void ssh_kbd_select_save(SshKbd* kbd);
void ssh_kbd_set_validator(SshKbd* kbd, SshKbdValidator validator, void* context);
void ssh_kbd_set_mask(SshKbd* kbd, bool mask); /* show '*' (passwords) */
void ssh_kbd_set_min_length(SshKbd* kbd, size_t min_length);
/* The field's limit, as maxlength on the web: keys that would add a character do nothing at
 * the limit, and "used/limit" shows in the header once it is 80% full. 0: the buffer's size.
 * Call after ssh_kbd_set_result_callback. */
void ssh_kbd_set_max_length(SshKbd* kbd, size_t max_length);
/* The limit in force (tests): the smaller of max_length and the buffer's room. */
size_t ssh_kbd_limit(SshKbd* kbd);
