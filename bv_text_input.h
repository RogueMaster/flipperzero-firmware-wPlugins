/*
 * bv_text_input — vendored Flipper text_input keyboard, extended with a symbol
 * page for typing punctuation. Renamed to avoid a symbol collision; validator
 * machinery stripped.
 */
#pragma once

#include <gui/view.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BvTextInput BvTextInput;
typedef void (*BvTextInputCallback)(void* context);

BvTextInput* bv_text_input_alloc(void);
void bv_text_input_free(BvTextInput* text_input);
void bv_text_input_reset(BvTextInput* text_input);
View* bv_text_input_get_view(BvTextInput* text_input);

// text_buffer is caller-owned; max string length is text_buffer_size - 1.
void bv_text_input_set_result_callback(
    BvTextInput* text_input,
    BvTextInputCallback callback,
    void* callback_context,
    char* text_buffer,
    size_t text_buffer_size,
    bool clear_default_text);

void bv_text_input_set_minimum_length(BvTextInput* text_input, size_t minimum_length);
void bv_text_input_set_header_text(BvTextInput* text_input, const char* text);

#ifdef __cplusplus
}
#endif
