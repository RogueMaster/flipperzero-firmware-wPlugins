#include "../blackhat_app_i.h"

void blackhat_console_output_handle_rx_data_cb(
    uint8_t* buf, size_t len, void* context
)
{
    furi_assert(context);
    BlackhatApp* app = context;

    // If text box store gets too big, then truncate it
    app->text_box_store_strlen += len;
    if (app->text_box_store_strlen >= BLACKHAT_TEXT_BOX_STORE_SIZE - 1) {
        furi_string_right(app->text_box_store, app->text_box_store_strlen / 2);
        app->text_box_store_strlen =
            furi_string_size(app->text_box_store) + len;
    }

    // We gotta parse the output
    if (app->is_script_scan) {
        memcpy(&app->script_text[app->script_text_ptr], buf, len);
        app->script_text_ptr += len;
    }

    // Null-terminate buf and append to text box store
    buf[len] = '\0';
    furi_string_cat_printf(app->text_box_store, "%s", buf);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
}

void blackhat_scene_console_output_on_enter(void* context)
{
    BlackhatApp* app = context;

    TextBox* text_box = app->text_box;
    text_box_reset(app->text_box);
    text_box_set_font(text_box, TextBoxFontText);

    text_box_set_focus(text_box, TextBoxFocusEnd);

    furi_string_reset(app->text_box_store);
    app->text_box_store_strlen = 0;

    app->is_script_scan = false;
    if (!strcmp(app->selected_tx_string, SCAN_CMD)) {
        app->is_script_scan = true;
        app->script_text_ptr = 0;
        app->scanned = true;
    }
    else if(!strncmp(app->selected_tx_string, "bh set", strlen("bh set"))) {
        app->is_script_scan = true;
        app->script_text_ptr = 0;
        app->scanned = true;
        app->selected_tx_string[3] = 'g'; // bh get
    }
    if (!strcmp(app->selected_tx_string, CHG_RUN_CMD_SCREEN)) {
        if (app->scanned) {
            app->script_text_ptr++;
            app->script_text[app->script_text_ptr] = 0x00;
            scene_manager_next_scene(app->scene_manager, BlackhatSceneScripts);
        }
        return;
    }

    snprintf(
        app->text_store,
        sizeof(app->text_store),
        "%s %s\n",
        app->selected_tx_string,
        app->selected_option_item_text
    );

    FURI_LOG_I("tag/app name", "%s", app->text_store);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));

    if (app->text_input_req) {
        app->selected_tx_string[3] = 's'; // bh set
        scene_manager_next_scene(app->scene_manager, BlackhatSceneRename);
        return;
    }
    else {
        scene_manager_set_scene_state(
            app->scene_manager, BlackhatSceneConsoleOutput, 0
        );

        view_dispatcher_switch_to_view(
            app->view_dispatcher, BlackhatAppViewConsoleOutput
        );
    }

    // Register callback to receive data
    blackhat_uart_set_handle_rx_data_cb(
        app->uart, blackhat_console_output_handle_rx_data_cb
    );

    blackhat_uart_tx(app->uart, app->text_store, strlen(app->text_store));

}

bool blackhat_scene_console_output_on_event(
    void* context, SceneManagerEvent event
)
{
    UNUSED(context);

    bool consumed = false;

    if (event.type == SceneManagerEventTypeTick) {
        consumed = true;
    }

    return consumed;
}

void blackhat_scene_console_output_on_exit(void* context)
{
    BlackhatApp* app = context;

    // Unregister rx callback
    blackhat_uart_set_handle_rx_data_cb(app->uart, NULL);
}
