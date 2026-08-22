/*
 * bv_text_input — a vendored copy of the Flipper 1.4.3 text_input keyboard,
 * extended with a symbol keyboard page (toggled by an on-screen switch key) so
 * passwords with punctuation/special characters can be typed on-device.
 *
 * Renamed from the SDK's built-in text_input to avoid a symbol collision, with
 * the validator/warning machinery stripped (unused here). The 4 keyboard icons
 * are bundled as FAP assets (see images/), so <assets_icons.h> resolves to them.
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

// text_buffer is YOUR buffer; max string length is text_buffer_size - 1.
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
