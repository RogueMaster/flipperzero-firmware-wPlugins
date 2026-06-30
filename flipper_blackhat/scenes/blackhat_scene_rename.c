#include "../blackhat_app_i.h"

void blackhat_console_output_handle_rx_data_cb(
    uint8_t* buf, size_t len, void* context
);

void blackhat_text_input_callback(void* context)
{
    furi_assert(context);
    BlackhatApp* app = context;
    view_dispatcher_send_custom_event(
        app->view_dispatcher, BlackhatEventTextInput
    );
}

void blackhat_scene_rename_on_enter(void* context)
{
    BlackhatApp* app = context;
    TextInput* text_input = app->text_input;

    size_t i = 0;
    size_t str_start = 0;

    // Register callback to receive data
    blackhat_uart_set_handle_rx_data_cb(
        app->uart, blackhat_console_output_handle_rx_data_cb
    );

    blackhat_uart_tx(app->uart, app->text_store, strlen(app->text_store));

    // Wait for all of the uart
    memset(app->text_input_ch,0x00, ENTER_NAME_LENGTH);
    furi_delay_ms(500);
    app->script_text_ptr++;
    app->script_text[app->script_text_ptr] = 0x00;

    while (app->script_text[i++]) {
        if (app->script_text[i] == '\n') {
            if(!str_start)
                str_start = i+1;
            else {
                app->script_text[i] = '\0';
                memcpy(
                    app->text_input_ch,
                    &app->script_text[str_start],
                    i - str_start - 1
                );
                break;
            }
        }
    }

    text_input_set_result_callback(
        text_input,
        blackhat_text_input_callback,
        context,
        app->text_input_ch,
        ENTER_NAME_LENGTH,
        false
    );

    view_dispatcher_switch_to_view(
        app->view_dispatcher, BlackhatAppViewTextInput
    );
}

bool blackhat_scene_rename_on_event(void* context, SceneManagerEvent event)
{
    BlackhatApp* app = context;
    bool consumed = false;
    if (event.type == SceneManagerEventTypeCustom) {
        snprintf(
            app->text_store,
            sizeof(app->text_store),
            "%s '%s'\n",
            app->selected_tx_string,
            app->text_input_ch
        );

        blackhat_uart_tx(app->uart, app->text_store, strlen(app->text_store));

        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, BlackhatSceneStart
        );

        consumed = true;
    }
    return consumed;
}

void blackhat_scene_rename_on_exit(void* context)
{
    BlackhatApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
