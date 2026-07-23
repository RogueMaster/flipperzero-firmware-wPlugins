#pragma once

#include <gui/view.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Full on-screen keyboard with a symbol page (ported from the WiFi Marauder
 * companion app). Unlike the stock firmware text_input, this one has a second
 * keyboard page with punctuation — including the "." needed to type IP
 * addresses and hostnames (see traceroute / nslookup).
 *
 * Switch between the letter and symbol pages with the on-screen keyboard key.
 */

/** Text input anonymous structure */
typedef struct LanTesterTextInput LanTesterTextInput;
typedef void (*LanTesterTextInputCallback)(void* context);
typedef bool (
    *LanTesterTextInputValidatorCallback)(const char* text, FuriString* error, void* context);

/** Allocate and initialize text input. Used to enter a string.
 *
 * @return     LanTesterTextInput instance
 */
LanTesterTextInput* lan_tester_text_input_alloc();

/** Deinitialize and free text input.
 *
 * @param      text_input  LanTesterTextInput instance
 */
void lan_tester_text_input_free(LanTesterTextInput* text_input);

/** Clean text input view. Note: this function does not free memory.
 *
 * @param      text_input  Text input instance
 */
void lan_tester_text_input_reset(LanTesterTextInput* text_input);

/** Get text input view.
 *
 * @param      text_input  LanTesterTextInput instance
 *
 * @return     View instance that can be used for embedding
 */
View* lan_tester_text_input_get_view(LanTesterTextInput* text_input);

/** Set text input result callback.
 *
 * @param      text_input          LanTesterTextInput instance
 * @param      callback            callback fn
 * @param      callback_context    callback context
 * @param      text_buffer         pointer to your text buffer, modified in place
 * @param      text_buffer_size    your text buffer size in bytes. Max string
 *                                 length will be text_buffer_size-1.
 * @param      clear_default_text  clear text from text_buffer on first OK event
 */
void lan_tester_text_input_set_result_callback(
    LanTesterTextInput* text_input,
    LanTesterTextInputCallback callback,
    void* callback_context,
    char* text_buffer,
    size_t text_buffer_size,
    bool clear_default_text);

void lan_tester_text_input_set_validator(
    LanTesterTextInput* text_input,
    LanTesterTextInputValidatorCallback callback,
    void* callback_context);

void lan_tester_text_input_set_minimum_length(
    LanTesterTextInput* text_input,
    size_t minimum_length);

LanTesterTextInputValidatorCallback
    lan_tester_text_input_get_validator_callback(LanTesterTextInput* text_input);

void* lan_tester_text_input_get_validator_callback_context(LanTesterTextInput* text_input);

/** Set text input header text.
 *
 * @param      text_input  LanTesterTextInput instance
 * @param      text        text to be shown
 */
void lan_tester_text_input_set_header_text(LanTesterTextInput* text_input, const char* text);

#ifdef __cplusplus
}
#endif
