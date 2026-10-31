/**
 * @file text_input.h
 * GUI: TextInput keyboard view module API
 */

#pragma once

#include <gui/view.h>
#include "validators.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Text input anonymous structure */
typedef struct TextInput TextInput;
typedef void (*TextInputCallback)(void* context);
typedef bool (*TextInputValidatorCallback)(const char* text, FuriString* error, void* context);

/** Allocate and initialize text input 
 * 
 * This text input is used to enter string
 *
 * @return     TextInput instance
 */
#ifndef FLIPPASS_TEXT_INPUT_PLUGIN_IMPLEMENTATION
/* This facade reuses the firmware TextInput function names. Plugins resolve imports
 * against the firmware API before the FlipPass host table, so with the plain names a
 * plugin would bind to the firmware's text input and hand it FlipPass's TextInput.
 * Every FlipPass translation unit therefore calls the facade by these unique names. */
#define text_input_alloc                  flippass_text_input_alloc
#define text_input_free                   flippass_text_input_free
#define text_input_reset                  flippass_text_input_reset
#define text_input_get_view               flippass_text_input_get_view
#define text_input_set_result_callback    flippass_text_input_set_result_callback
#define text_input_set_minimum_length     flippass_text_input_set_minimum_length
#define text_input_set_validator          flippass_text_input_set_validator
#define text_input_show_illegal_symbols   flippass_text_input_show_illegal_symbols
#define text_input_set_is_password        flippass_text_input_set_is_password
#define text_input_set_for_open           flippass_text_input_set_for_open
#define text_input_get_validator_callback flippass_text_input_get_validator_callback
#define text_input_get_validator_callback_context \
    flippass_text_input_get_validator_callback_context
#define text_input_set_header_text flippass_text_input_set_header_text

TextInput* text_input_alloc(void);

/** Deinitialize and free text input
 *
 * @param      text_input  TextInput instance
 */
void text_input_free(TextInput* text_input);

/** Clean text input view Note: this function does not free memory
 *
 * @param      text_input  Text input instance
 */
void text_input_reset(TextInput* text_input);

/** Get text input view
 *
 * @param      text_input  TextInput instance
 *
 * @return     View instance that can be used for embedding
 */
View* text_input_get_view(TextInput* text_input);

/** Set text input result callback
 *
 * @param      text_input          TextInput instance
 * @param      callback            callback fn
 * @param      callback_context    callback context
 * @param      text_buffer         pointer to YOUR text buffer, that we going
 *                                 to modify
 * @param      text_buffer_size    YOUR text buffer size in bytes. Max string
 *                                 length will be text_buffer_size-1.
 * @param      clear_default_text  clear text from text_buffer on first OK
 *                                 event
 */
void text_input_set_result_callback(
    TextInput* text_input,
    TextInputCallback callback,
    void* callback_context,
    char* text_buffer,
    size_t text_buffer_size,
    bool clear_default_text);

/**
 * @brief Sets the minimum length of a TextInput
 * @param [in] text_input TextInput
 * @param [in] minimum_length Minimum input length
 */
void text_input_set_minimum_length(TextInput* text_input, size_t minimum_length);

void text_input_set_validator(
    TextInput* text_input,
    TextInputValidatorCallback callback,
    void* callback_context);

/**
 * @brief Show the 9 illegal (windows) symbols <>:"/\|?* in the symbols keyboard instead of the numbers
 * @param [in] text_input TextInput
 * @param [in] show Whether to show the illegal symbols or not
 */
void text_input_show_illegal_symbols(TextInput* text_input, bool show);

void text_input_set_is_password(TextInput* text_input, bool is_password);

void text_input_set_for_open(TextInput* text_input, bool for_open);

TextInputValidatorCallback text_input_get_validator_callback(TextInput* text_input);

void* text_input_get_validator_callback_context(TextInput* text_input);

/** Set text input header text
 *
 * @param      text_input  TextInput instance
 * @param      text        text to be shown
 */
void text_input_set_header_text(TextInput* text_input, const char* text);
#endif

#ifdef __cplusplus
}
#endif
