#ifndef EASY_FLIPPER_H
#define EASY_FLIPPER_H

#include <malloc.h>
#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/modules/submenu.h>
#include <gui/view_dispatcher.h>
#include <gui/elements.h>
#include <gui/modules/menu.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/text_input.h>
#include <gui/modules/text_box.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/dialog_ex.h>
#include <notification/notification.h>
#include <dialogs/dialogs.h>
#include <gui/modules/popup.h>
#include <gui/modules/loading.h>
#include <text_input/uart_text_input.h>
#include <stdio.h>
#include <string.h>
#include <jsmn/jsmn_furi.h>
#include <jsmn/jsmn.h>
#include <math.h>

#ifdef __cplusplus
#include <memory>
extern "C"
{
#endif

#define EASY_TAG "EasyFlipper"

    void easy_flipper_dialog(
        const char *header,
        const char *text);

    /**
     * @brief Navigation callback for exiting the application
     * @param context The context - unused
     * @return next view id (VIEW_NONE to exit the app)
     */
    uint32_t easy_flipper_callback_exit_app(void *context);
    /**
     * @brief Initialize a buffer
     * @param buffer The buffer to initialize
     * @param buffer_size The size of the buffer
     * @return true if successful, false otherwise
     */
    bool easy_flipper_set_buffer(char **buffer, uint32_t buffer_size);
    /**
     * @brief Initialize a View object
     * @param view The View object to initialize
     * @param view_id The ID/Index of the view
     * @param draw_callback The draw callback function (set to NULL if not needed)
     * @param input_callback The input callback function (set to NULL if not needed)
     * @param previous_callback The previous callback function (can be set to NULL)
     * @param view_dispatcher The ViewDispatcher object
     * @return true if successful, false otherwise
     */
    bool easy_flipper_set_view(
        View **view,
        int32_t view_id,
        void draw_callback(Canvas *, void *),
        bool input_callback(InputEvent *, void *),
        uint32_t (*previous_callback)(void *),
        ViewDispatcher **view_dispatcher,
        void *context);

    /**
     * @brief Initialize a ViewDispatcher object
     * @param view_dispatcher The ViewDispatcher object to initialize
     * @param gui The GUI object
     * @param context The context to pass to the event callback
     * @return true if successful, false otherwise
     */
    bool easy_flipper_set_view_dispatcher(ViewDispatcher **view_dispatcher, Gui *gui, void *context);

    /**
     * @brief Initialize a Submenu object
     * @note This does not set the items in the submenu
     * @param submenu The Submenu object to initialize
     * @param view_id The ID/Index of the view
     * @param title The title/header of the submenu
     * @param previous_callback The previous callback function (can be set to NULL)
     * @param view_dispatcher The ViewDispatcher object
     * @return true if successful, false otherwise
     */
    bool easy_flipper_set_submenu(
        Submenu **submenu,
        int32_t view_id,
        const char *title,
        uint32_t(previous_callback)(void *),
        ViewDispatcher **view_dispatcher);

    /**
     * @brief Initialize a Menu object
     * @note This does not set the items in the menu
     * @param menu The Menu object to initialize
     * @param view_id The ID/Index of the view
     * @param item_callback The item callback function
     * @param previous_callback The previous callback function (can be set to NULL)
     * @param view_dispatcher The ViewDispatcher object
     * @return true if successful, false otherwise
     */
    bool easy_flipper_set_menu(
        Menu **menu,
        int32_t view_id,
        uint32_t(previous_callback)(void *),
        ViewDispatcher **view_dispatcher);

    /**
     * @brief Initialize a Widget object
     * @param widget The Widget object to initialize
     * @param view_id The ID/Index of the view
     * @param text The text to display in the widget
     * @param previous_callback The previous callback function (can be set to NULL)
     * @param view_dispatcher The ViewDispatcher object
     * @return true if successful, false otherwise
     */
    bool easy_flipper_set_widget(
        Widget **widget,
        int32_t view_id,
        const char *text,
        uint32_t(previous_callback)(void *),
        ViewDispatcher **view_dispatcher);

    /**
     * @brief Initialize a VariableItemList object
     * @note This does not set the items in the VariableItemList
     * @param variable_item_list The VariableItemList object to initialize
     * @param view_id The ID/Index of the view
     * @param enter_callback The enter callback function (can be set to NULL)
     * @param previous_callback The previous callback function (can be set to NULL)
     * @param view_dispatcher The ViewDispatcher object
     * @param context The context to pass to the enter callback (usually the app)
     * @return true if successful, false otherwise
     */
    bool easy_flipper_set_variable_item_list(
        VariableItemList **variable_item_list,
        int32_t view_id,
        void (*enter_callback)(void *, uint32_t),
        uint32_t(previous_callback)(void *),
        ViewDispatcher **view_dispatcher,
        void *context);

    /**
     * @brief Initialize a TextInput object
     * @param text_input The TextInput object to initialize
     * @param view_id The ID/Index of the view
     * @param previous_callback The previous callback function (can be set to NULL)
     * @param view_dispatcher The ViewDispatcher object
     * @return true if successful, false otherwise
     */
    bool easy_flipper_set_text_input(
        TextInput **text_input,
        int32_t view_id,
        const char *header_text,
        char *text_input_temp_buffer,
        uint32_t text_input_buffer_size,
        void (*result_callback)(void *),
        uint32_t(previous_callback)(void *),
        ViewDispatcher **view_dispatcher,
        void *context);

    /**
     * @brief Initialize a UART_TextInput object
     * @param uart_text_input The UART_TextInput object to initialize
     * @param view_id The ID/Index of the view
     * @param previous_callback The previous callback function (can be set to NULL)
     * @param view_dispatcher The ViewDispatcher object
     * @return true if successful, false otherwise
     */
    bool easy_flipper_set_uart_text_input(
        UART_TextInput **uart_text_input,
        int32_t view_id,
        const char *header_text,
        char *uart_text_input_temp_buffer,
        uint32_t uart_text_input_buffer_size,
        void (*result_callback)(void *),
        uint32_t(previous_callback)(void *),
        ViewDispatcher **view_dispatcher,
        void *context);

    /**
     * @brief Initialize a DialogEx object
     * @param dialog_ex The DialogEx object to initialize
     * @param view_id The ID/Index of the view
     * @param header The header of the dialog
     * @param header_x The x coordinate of the header
     * @param header_y The y coordinate of the header
     * @param text The text of the dialog
     * @param text_x The x coordinate of the dialog
     * @param text_y The y coordinate of the dialog
     * @param left_button_text The text of the left button
     * @param right_button_text The text of the right button
     * @param center_button_text The text of the center button
     * @param result_callback The result callback function
     * @param previous_callback The previous callback function (can be set to NULL)
     * @param view_dispatcher The ViewDispatcher object
     * @param context The context to pass to the result callback
     * @return true if successful, false otherwise
     */
    bool easy_flipper_set_dialog_ex(
        DialogEx **dialog_ex,
        int32_t view_id,
        const char *header,
        uint16_t header_x,
        uint16_t header_y,
        const char *text,
        uint16_t text_x,
        uint16_t text_y,
        const char *left_button_text,
        const char *right_button_text,
        const char *center_button_text,
        void (*result_callback)(DialogExResult, void *),
        uint32_t(previous_callback)(void *),
        ViewDispatcher **view_dispatcher,
        void *context);

    /**
     * @brief Initialize a Popup object
     * @param popup The Popup object to initialize
     * @param view_id The ID/Index of the view
     * @param header The header of the dialog
     * @param header_x The x coordinate of the header
     * @param header_y The y coordinate of the header
     * @param text The text of the dialog
     * @param text_x The x coordinate of the dialog
     * @param text_y The y coordinate of the dialog
     * @param result_callback The result callback function
     * @param previous_callback The previous callback function (can be set to NULL)
     * @param view_dispatcher The ViewDispatcher object
     * @param context The context to pass to the result callback
     * @return true if successful, false otherwise
     */
    bool easy_flipper_set_popup(
        Popup **popup,
        int32_t view_id,
        const char *header,
        uint16_t header_x,
        uint16_t header_y,
        const char *text,
        uint16_t text_x,
        uint16_t text_y,
        void (*result_callback)(void *),
        uint32_t(previous_callback)(void *),
        ViewDispatcher **view_dispatcher,
        void *context);

    /**
     * @brief Initialize a Loading object
     * @param loading The Loading object to initialize
     * @param view_id The ID/Index of the view
     * @param previous_callback The previous callback function (can be set to NULL)
     * @param view_dispatcher The ViewDispatcher object
     * @return true if successful, false otherwise
     */
    bool easy_flipper_set_loading(
        Loading **loading,
        int32_t view_id,
        uint32_t(previous_callback)(void *),
        ViewDispatcher **view_dispatcher);

    /**
     * @brief Set a char butter to a FuriString
     * @param furi_string The FuriString object
     * @param buffer The buffer to copy the string to
     * @return true if successful, false otherwise
     */
    bool easy_flipper_set_char_to_furi_string(FuriString **furi_string, const char *buffer);

#ifdef __cplusplus
}
#endif

#endif