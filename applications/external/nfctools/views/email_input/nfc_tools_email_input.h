#pragma once

#include <gui/view.h>
#include <stdbool.h>
#include <stddef.h>

// Specialised email text keyboard: identical to TextInput but with
// @  .  -  in place of 7  8  9 on the 3rd row.

typedef struct EmailInput EmailInput;
typedef void (*EmailInputCallback)(void* context);

EmailInput* email_input_alloc(void);
void email_input_free(EmailInput* email_input);
void email_input_reset(EmailInput* email_input);
View* email_input_get_view(EmailInput* email_input);

void email_input_set_header_text(EmailInput* email_input, const char* text);

void email_input_set_result_callback(
    EmailInput* email_input,
    EmailInputCallback callback,
    void* callback_context,
    char* text_buffer,
    size_t text_buffer_size,
    bool clear_default_text);

void email_input_set_minimum_length(EmailInput* email_input, size_t minimum_length);
