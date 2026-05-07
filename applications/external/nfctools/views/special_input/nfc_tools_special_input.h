#pragma once

#include <gui/view.h>
#include <stdbool.h>
#include <stddef.h>

// Text keyboard with ABC / SYM toggle.
// ABC page: letters + digits (identical to EmailInput without the _ ).
// SYM page: ! " # $ % & ' ( ) * + - / = < > ; : ^ { } [ ] \ | ~ @
//           plus right-side punctuation: , . ?  (ABC: @ . - )

typedef struct SpecialInput SpecialInput;
typedef void (*SpecialInputCallback)(void* context);

SpecialInput* special_input_alloc(void);
void special_input_free(SpecialInput* si);
void special_input_reset(SpecialInput* si);
View* special_input_get_view(SpecialInput* si);

void special_input_set_header_text(SpecialInput* si, const char* text);

void special_input_set_result_callback(
    SpecialInput* si,
    SpecialInputCallback callback,
    void* callback_context,
    char* text_buffer,
    size_t text_buffer_size,
    bool clear_default_text);

void special_input_set_minimum_length(SpecialInput* si, size_t minimum_length);
