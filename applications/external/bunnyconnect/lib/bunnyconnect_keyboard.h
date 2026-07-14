#pragma once

#include <furi.h>
#include <gui/view.h>
#include <gui/gui.h>
#include <input/input.h>
#include <furi_hal_usb_hid.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BunnyConnectKeyboard BunnyConnectKeyboard;

typedef void (*BunnyConnectKeyboardCallback)(void* context);
typedef bool (
    *BunnyConnectKeyboardValidatorCallback)(const char* text, FuriString* error, void* context);

/** Allocate and initialize custom keyboard
 * 
 * This keyboard is used to enter text with an improved layout
 *
 * @return     BunnyConnectKeyboard instance
 */
BunnyConnectKeyboard* bunnyconnect_keyboard_alloc(void);

/** Deinitialize and free keyboard
 *
 * @param      keyboard  BunnyConnectKeyboard instance
 */
void bunnyconnect_keyboard_free(BunnyConnectKeyboard* keyboard);

/** Reset keyboard to default state
 *
 * @param      keyboard  BunnyConnectKeyboard instance
 */
void bunnyconnect_keyboard_reset(BunnyConnectKeyboard* keyboard);

/** Get keyboard view
 *
 * @param      keyboard  BunnyConnectKeyboard instance
 *
 * @return     View instance that can be used for embedding
 */
View* bunnyconnect_keyboard_get_view(BunnyConnectKeyboard* keyboard);

/** Set keyboard result callback
 *
 * @param      keyboard            BunnyConnectKeyboard instance
 * @param      callback            Callback function
 * @param      callback_context    Callback context
 * @param      text_buffer         Pointer to text buffer that will be modified
 * @param      text_buffer_size    Text buffer size in bytes (max string length is text_buffer_size-1)
 * @param      clear_default_text  Clear text from text_buffer on first OK event
 */
void bunnyconnect_keyboard_set_result_callback(
    BunnyConnectKeyboard* keyboard,
    BunnyConnectKeyboardCallback callback,
    void* callback_context,
    char* text_buffer,
    size_t text_buffer_size,
    bool clear_default_text);

/** Set validator callback
 * 
 * @param      keyboard            BunnyConnectKeyboard instance
 * @param      callback            Validator callback
 * @param      callback_context    Callback context
 */
void bunnyconnect_keyboard_set_validator_callback(
    BunnyConnectKeyboard* keyboard,
    BunnyConnectKeyboardValidatorCallback callback,
    void* context);

/** Set minimum input length
 * 
 * @param      keyboard         BunnyConnectKeyboard instance
 * @param      minimum_length   Minimum required input length
 */
void bunnyconnect_keyboard_set_minimum_length(
    BunnyConnectKeyboard* keyboard,
    size_t minimum_length);

/** Get validator callback
 * 
 * @param      keyboard  BunnyConnectKeyboard instance
 * @return     Validator callback
 */
BunnyConnectKeyboardValidatorCallback
    bunnyconnect_keyboard_get_validator_callback(BunnyConnectKeyboard* keyboard);

/** Get validator callback context
 * 
 * @param      keyboard  BunnyConnectKeyboard instance
 * @return     Validator callback context
 */
void* bunnyconnect_keyboard_get_validator_callback_context(BunnyConnectKeyboard* keyboard);

/** Set header text
 *
 * @param      keyboard  BunnyConnectKeyboard instance
 * @param      text      Text to be shown
 */
void bunnyconnect_keyboard_set_header_text(BunnyConnectKeyboard* keyboard, const char* text);

/** Send key event
 *
 * @param      keyboard  BunnyConnectKeyboard instance
 * @param      key       Key to be sent
 */
void bunnyconnect_keyboard_send_key(BunnyConnectKeyboard* keyboard, uint16_t key);

/** Send string event
 *
 * @param      keyboard  BunnyConnectKeyboard instance
 * @param      string    String to be sent
 */
void bunnyconnect_keyboard_send_string(BunnyConnectKeyboard* keyboard, const char* string);

#ifdef __cplusplus
}
#endif
