#pragma once

#include <gui/view.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Text input anonymous structure */
typedef struct MarauderTextInput MarauderTextInput;
typedef void (*MarauderTextInputCallback)(void* context);
typedef bool (
    *MarauderTextInputValidatorCallback)(const char* text, FuriString* error, void* context);

/** Allocate and initialize text input
 *
 * This text input is used to enter string
 *
 * @return     MarauderTextInput instance
 */
MarauderTextInput* marauder_text_input_alloc();

/** Deinitialize and free text input
 *
 * @param      text_input  MarauderTextInput instance
 */
void marauder_text_input_free(MarauderTextInput* text_input);

/** Clean text input view Note: this function does not free memory
 *
 * @param      text_input  Text input instance
 */
void marauder_text_input_reset(MarauderTextInput* text_input);

/** Get text input view
 *
 * @param      text_input  MarauderTextInput instance
 *
 * @return     View instance that can be used for embedding
 */
View* marauder_text_input_get_view(MarauderTextInput* text_input);

/** Set text input result callback
 *
 * @param      text_input          MarauderTextInput instance
 * @param      callback            callback fn
 * @param      callback_context    callback context
 * @param      text_buffer         pointer to YOUR text buffer, that we going
 *                                 to modify
 * @param      text_buffer_size    YOUR text buffer size in bytes. Max string
 *                                 length will be text_buffer_size-1.
 * @param      clear_default_text  clear text from text_buffer on first OK
 *                                 event
 */
void marauder_text_input_set_result_callback(
    MarauderTextInput* text_input,
    MarauderTextInputCallback callback,
    void* callback_context,
    char* text_buffer,
    size_t text_buffer_size,
    bool clear_default_text);

void marauder_text_input_set_validator(
    MarauderTextInput* text_input,
    MarauderTextInputValidatorCallback callback,
    void* callback_context);

void marauder_text_input_set_minimum_length(MarauderTextInput* text_input, size_t minimum_length);

MarauderTextInputValidatorCallback
    marauder_text_input_get_validator_callback(MarauderTextInput* text_input);

void* marauder_text_input_get_validator_callback_context(MarauderTextInput* text_input);

/** Set text input header text
 *
 * @param      text_input  MarauderTextInput instance
 * @param      text        text to be shown
 */
void marauder_text_input_set_header_text(MarauderTextInput* text_input, const char* text);

#ifdef __cplusplus
}
#endif
