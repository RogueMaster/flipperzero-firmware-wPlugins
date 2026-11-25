#pragma once

#include <gui/view.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PCSGTextInput PCSGTextInput;
typedef void (*PCSGTextInputCallback)(void* context);
typedef bool (*PCSGTextInputValidatorCallback)(const char* text, FuriString* error, void* context);

PCSGTextInput* pcsg_text_input_alloc(void);
void pcsg_text_input_free(PCSGTextInput* text_input);
void pcsg_text_input_reset(PCSGTextInput* text_input);
View* pcsg_text_input_get_view(PCSGTextInput* text_input);

void pcsg_text_input_set_result_callback(
    PCSGTextInput* text_input,
    PCSGTextInputCallback callback,
    void* callback_context,
    char* text_buffer,
    size_t text_buffer_size,
    bool clear_default_text);

void pcsg_text_input_set_validator(
    PCSGTextInput* text_input,
    PCSGTextInputValidatorCallback callback,
    void* callback_context);

void pcsg_text_input_set_minimum_length(PCSGTextInput* text_input, size_t minimum_length);
void pcsg_text_input_set_header_text(PCSGTextInput* text_input, const char* text);

#ifdef __cplusplus
}
#endif
