#include <easy_flipper/easy_flipper.h>

void easy_flipper_dialog(
    const char *header,
    const char *text)
{
    DialogsApp *dialogs = furi_record_open(RECORD_DIALOGS);
    DialogMessage *message = dialog_message_alloc();
    dialog_message_set_header(
        message, header, 64, 0, AlignCenter, AlignTop);
    dialog_message_set_text(
        message,
        text,
        0,
        63,
        AlignLeft,
        AlignBottom);
    dialog_message_show(dialogs, message);
    dialog_message_free(message);
    furi_record_close(RECORD_DIALOGS);
}

/**
 * @brief Navigation callback for exiting the application
 * @param context The context - unused
 * @return next view id (VIEW_NONE to exit the app)
 */
uint32_t easy_flipper_callback_exit_app(void *context)
{
    // Exit the application
    if (!context)
    {
        FURI_LOG_E(EASY_TAG, "Context is NULL");
        return VIEW_NONE;
    }
    UNUSED(context);
    return VIEW_NONE; // Return VIEW_NONE to exit the app
}

/**
 * @brief Initialize a buffer
 * @param buffer The buffer to initialize
 * @param buffer_size The size of the buffer
 * @return true if successful, false otherwise
 */
bool easy_flipper_set_buffer(char **buffer, uint32_t buffer_size)
{
    if (!buffer)
    {
        FURI_LOG_E(EASY_TAG, "Invalid arguments provided to set_buffer");
        return false;
    }
    *buffer = (char *)malloc(buffer_size);
    if (!*buffer)
    {
        FURI_LOG_E(EASY_TAG, "Failed to allocate buffer");
        return false;
    }
    *buffer[0] = '\0';
    return true;
}

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
    void *context)
{
    if (!view || !view_dispatcher)
    {
        FURI_LOG_E(EASY_TAG, "Invalid arguments provided to set_view");
        return false;
    }
    *view = view_alloc();
    if (!*view)
    {
        FURI_LOG_E(EASY_TAG, "Failed to allocate View");
        return false;
    }
    if (draw_callback)
    {
        view_set_draw_callback(*view, draw_callback);
    }
    if (input_callback)
    {
        view_set_input_callback(*view, input_callback);
    }
    if (context)
    {
        view_set_context(*view, context);
    }
    if (previous_callback)
    {
        view_set_previous_callback(*view, previous_callback);
    }
    view_dispatcher_add_view(*view_dispatcher, view_id, *view);
    return true;
}

/**
 * @brief Initialize a ViewDispatcher object
 * @param view_dispatcher The ViewDispatcher object to initialize
 * @param gui The GUI object
 * @param context The context to pass to the event callback
 * @return true if successful, false otherwise
 */
bool easy_flipper_set_view_dispatcher(ViewDispatcher **view_dispatcher, Gui *gui, void *context)
{
    if (!view_dispatcher)
    {
        FURI_LOG_E(EASY_TAG, "Invalid arguments provided to set_view_dispatcher");
        return false;
    }
    *view_dispatcher = view_dispatcher_alloc();
    if (!*view_dispatcher)
    {
        FURI_LOG_E(EASY_TAG, "Failed to allocate ViewDispatcher");
        return false;
    }
    view_dispatcher_attach_to_gui(*view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    if (context)
    {
        view_dispatcher_set_event_callback_context(*view_dispatcher, context);
    }
    return true;
}

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
    ViewDispatcher **view_dispatcher)
{
    if (!submenu)
    {
        FURI_LOG_E(EASY_TAG, "Invalid arguments provided to set_submenu");
        return false;
    }
    *submenu = submenu_alloc();
    if (!*submenu)
    {
        FURI_LOG_E(EASY_TAG, "Failed to allocate Submenu");
        return false;
    }
    if (title)
    {
        submenu_set_header(*submenu, title);
    }
    if (previous_callback)
    {
        view_set_previous_callback(submenu_get_view(*submenu), previous_callback);
    }
    view_dispatcher_add_view(*view_dispatcher, view_id, submenu_get_view(*submenu));
    return true;
}
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
    ViewDispatcher **view_dispatcher)
{
    if (!menu)
    {
        FURI_LOG_E(EASY_TAG, "Invalid arguments provided to set_menu");
        return false;
    }
    *menu = menu_alloc();
    if (!*menu)
    {
        FURI_LOG_E(EASY_TAG, "Failed to allocate Menu");
        return false;
    }
    if (previous_callback)
    {
        view_set_previous_callback(menu_get_view(*menu), previous_callback);
    }
    view_dispatcher_add_view(*view_dispatcher, view_id, menu_get_view(*menu));
    return true;
}

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
    ViewDispatcher **view_dispatcher)
{
    if (!widget)
    {
        FURI_LOG_E(EASY_TAG, "Invalid arguments provided to set_widget");
        return false;
    }
    *widget = widget_alloc();
    if (!*widget)
    {
        FURI_LOG_E(EASY_TAG, "Failed to allocate Widget");
        return false;
    }
    if (text)
    {
        widget_add_text_scroll_element(*widget, 0, 0, 128, 64, text);
    }
    if (previous_callback)
    {
        view_set_previous_callback(widget_get_view(*widget), previous_callback);
    }
    view_dispatcher_add_view(*view_dispatcher, view_id, widget_get_view(*widget));
    return true;
}

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
    void *context)
{
    if (!variable_item_list)
    {
        FURI_LOG_E(EASY_TAG, "Invalid arguments provided to set_variable_item_list");
        return false;
    }
    *variable_item_list = variable_item_list_alloc();
    if (!*variable_item_list)
    {
        FURI_LOG_E(EASY_TAG, "Failed to allocate VariableItemList");
        return false;
    }
    if (enter_callback)
    {
        variable_item_list_set_enter_callback(*variable_item_list, enter_callback, context);
    }
    if (previous_callback)
    {
        view_set_previous_callback(variable_item_list_get_view(*variable_item_list), previous_callback);
    }
    view_dispatcher_add_view(*view_dispatcher, view_id, variable_item_list_get_view(*variable_item_list));
    return true;
}

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
    void *context)
{
    if (!text_input)
    {
        FURI_LOG_E(EASY_TAG, "Invalid arguments provided to set_text_input");
        return false;
    }
    *text_input = text_input_alloc();
    if (!*text_input)
    {
        FURI_LOG_E(EASY_TAG, "Failed to allocate TextInput");
        return false;
    }
    if (previous_callback)
    {
        view_set_previous_callback(text_input_get_view(*text_input), previous_callback);
    }
    if (header_text)
    {
        text_input_set_header_text(*text_input, header_text);
    }
    if (text_input_temp_buffer && text_input_buffer_size && result_callback)
    {
        text_input_set_result_callback(*text_input, result_callback, context, text_input_temp_buffer, text_input_buffer_size, false);
    }
    view_dispatcher_add_view(*view_dispatcher, view_id, text_input_get_view(*text_input));
    return true;
}

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
    void *context)
{
    if (!uart_text_input)
    {
        FURI_LOG_E(EASY_TAG, "Invalid arguments provided to set_uart_text_input");
        return false;
    }
    *uart_text_input = uart_text_input_alloc();
    if (!*uart_text_input)
    {
        FURI_LOG_E(EASY_TAG, "Failed to allocate UART_TextInput");
        return false;
    }
    if (previous_callback)
    {
        view_set_previous_callback(uart_text_input_get_view(*uart_text_input), previous_callback);
    }
    if (header_text)
    {
        uart_text_input_set_header_text(*uart_text_input, header_text);
    }
    if (uart_text_input_temp_buffer && uart_text_input_buffer_size && result_callback)
    {
        uart_text_input_set_result_callback(*uart_text_input, result_callback, context, uart_text_input_temp_buffer, uart_text_input_buffer_size, false);
    }
    view_dispatcher_add_view(*view_dispatcher, view_id, uart_text_input_get_view(*uart_text_input));
    return true;
}

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
    void *context)
{
    if (!dialog_ex)
    {
        FURI_LOG_E(EASY_TAG, "Invalid arguments provided to set_dialog_ex");
        return false;
    }
    *dialog_ex = dialog_ex_alloc();
    if (!*dialog_ex)
    {
        FURI_LOG_E(EASY_TAG, "Failed to allocate DialogEx");
        return false;
    }
    if (header)
    {
        dialog_ex_set_header(*dialog_ex, header, header_x, header_y, AlignLeft, AlignTop);
    }
    if (text)
    {
        dialog_ex_set_text(*dialog_ex, text, text_x, text_y, AlignLeft, AlignTop);
    }
    if (left_button_text)
    {
        dialog_ex_set_left_button_text(*dialog_ex, left_button_text);
    }
    if (right_button_text)
    {
        dialog_ex_set_right_button_text(*dialog_ex, right_button_text);
    }
    if (center_button_text)
    {
        dialog_ex_set_center_button_text(*dialog_ex, center_button_text);
    }
    if (result_callback)
    {
        dialog_ex_set_result_callback(*dialog_ex, result_callback);
    }
    if (previous_callback)
    {
        view_set_previous_callback(dialog_ex_get_view(*dialog_ex), previous_callback);
    }
    if (context)
    {
        dialog_ex_set_context(*dialog_ex, context);
    }
    view_dispatcher_add_view(*view_dispatcher, view_id, dialog_ex_get_view(*dialog_ex));
    return true;
}

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
    void *context)
{
    if (!popup)
    {
        FURI_LOG_E(EASY_TAG, "Invalid arguments provided to set_popup");
        return false;
    }
    *popup = popup_alloc();
    if (!*popup)
    {
        FURI_LOG_E(EASY_TAG, "Failed to allocate Popup");
        return false;
    }
    if (header)
    {
        popup_set_header(*popup, header, header_x, header_y, AlignLeft, AlignTop);
    }
    if (text)
    {
        popup_set_text(*popup, text, text_x, text_y, AlignLeft, AlignTop);
    }
    if (result_callback)
    {
        popup_set_callback(*popup, result_callback);
    }
    if (previous_callback)
    {
        view_set_previous_callback(popup_get_view(*popup), previous_callback);
    }
    if (context)
    {
        popup_set_context(*popup, context);
    }
    view_dispatcher_add_view(*view_dispatcher, view_id, popup_get_view(*popup));
    return true;
}

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
    ViewDispatcher **view_dispatcher)
{
    if (!loading)
    {
        FURI_LOG_E(EASY_TAG, "Invalid arguments provided to set_loading");
        return false;
    }
    *loading = loading_alloc();
    if (!*loading)
    {
        FURI_LOG_E(EASY_TAG, "Failed to allocate Loading");
        return false;
    }
    if (previous_callback)
    {
        view_set_previous_callback(loading_get_view(*loading), previous_callback);
    }
    view_dispatcher_add_view(*view_dispatcher, view_id, loading_get_view(*loading));
    return true;
}

/**
 * @brief Set a char buffer to a FuriString
 * @param furi_string The FuriString object
 * @param buffer The buffer to copy the string to
 * @return true if successful, false otherwise
 */
bool easy_flipper_set_char_to_furi_string(FuriString **furi_string, const char *buffer)
{
    if (!furi_string)
    {
        FURI_LOG_E(EASY_TAG, "Invalid arguments provided to set_buffer_to_furi_string");
        return false;
    }
    *furi_string = furi_string_alloc();
    if (!furi_string)
    {
        FURI_LOG_E(EASY_TAG, "Failed to allocate FuriString");
        return false;
    }
    furi_string_set_str(*furi_string, buffer);
    return true;
}