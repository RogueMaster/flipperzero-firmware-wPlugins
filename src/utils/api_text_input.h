#pragma once

/*
 * Extended text input keyboard for Flipper API Caller.
 * Based on the custom keyboard of "UART Terminal" by Malik cool4uma
 * (https://github.com/cool4uma/UART_Terminal), MIT License.
 * Copyright (c) 2023 Malik cool4uma
 */

#include <gui/view.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Text input anonymous structure */
typedef struct ApiTextInput ApiTextInput;
typedef void (*ApiTextInputCallback)(void* context);
typedef bool (*ApiTextInputValidatorCallback)(const char* text, FuriString* error, void* context);

/** Allocate and initialize text input
 *
 * This text input is used to enter string
 *
 * @return     ApiTextInput instance
 */
ApiTextInput* api_text_input_alloc();

/** Deinitialize and free text input
 *
 * @param      api_text_input  ApiTextInput instance
 */
void api_text_input_free(ApiTextInput* api_text_input);

/** Clean text input view Note: this function does not free memory
 *
 * @param      api_text_input  ApiTextInput instance
 */
void api_text_input_reset(ApiTextInput* api_text_input);

/** Get text input view
 *
 * @param      api_text_input  ApiTextInput instance
 *
 * @return     View instance that can be used for embedding
 */
View* api_text_input_get_view(ApiTextInput* api_text_input);

/** Set text input result callback
 *
 * @param      api_text_input      ApiTextInput instance
 * @param      callback            callback fn
 * @param      callback_context    callback context
 * @param      text_buffer         pointer to YOUR text buffer, that we going
 *                                 to modify
 * @param      text_buffer_size    YOUR text buffer size in bytes. Max string
 *                                 length will be text_buffer_size-1.
 * @param      clear_default_text  clear text from text_buffer on first OK
 *                                 event
 */
void api_text_input_set_result_callback(
    ApiTextInput* api_text_input,
    ApiTextInputCallback callback,
    void* callback_context,
    char* text_buffer,
    size_t text_buffer_size,
    bool clear_default_text);

void api_text_input_set_validator(
    ApiTextInput* api_text_input,
    ApiTextInputValidatorCallback callback,
    void* callback_context);

ApiTextInputValidatorCallback api_text_input_get_validator_callback(ApiTextInput* api_text_input);

void* api_text_input_get_validator_callback_context(ApiTextInput* api_text_input);

/** Set text input header text
 *
 * @param      api_text_input  ApiTextInput instance
 * @param      text        text to be shown
 */
void api_text_input_set_header_text(ApiTextInput* api_text_input, const char* text);

#ifdef __cplusplus
}
#endif
