#include <callback/loader.h>
#include <callback/utils.h>
#include <alloc/alloc.h>

bool loader_view_alloc(void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app, "FlipWorldApp is NULL");
    if (app->view_loader)
    {
        FURI_LOG_E(TAG, "View loader already allocated");
        return false;
    }
    if (app->widget_result)
    {
        FURI_LOG_E(TAG, "Widget result already allocated");
        return false;
    }

    view_dispatcher_set_custom_event_callback(app->view_dispatcher, loader_custom_event_callback);

    if (!easy_flipper_set_view(&app->view_loader, FlipWorldViewLoader, loader_draw_callback, NULL, callback_to_submenu, &app->view_dispatcher, app))
    {
        return false;
    }

    loader_init(app->view_loader);

    return easy_flipper_set_widget(&app->widget_result, FlipWorldViewWidgetResult, "", callback_to_submenu, &app->view_dispatcher);
}

void loader_view_free(void *context)
{
    FlipWorldApp *app = (FlipWorldApp *)context;
    furi_check(app, "FlipWorldApp is NULL");
    // Free Widget(s)
    if (app->widget_result)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipWorldViewWidgetResult);
        widget_free(app->widget_result);
        app->widget_result = NULL;
    }

    // Free View(s)
    if (app->view_loader)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipWorldViewLoader);
        loader_free_model(app->view_loader);
        view_free(app->view_loader);
        app->view_loader = NULL;
    }
}

static void loader_error_draw(Canvas *canvas, DataLoaderModel *model)
{
    if (canvas == NULL)
    {
        FURI_LOG_E(TAG, "error_draw - canvas is NULL");
        DEV_CRASH();
        return;
    }
    if (model->fhttp->last_response != NULL)
    {
        if (strstr(model->fhttp->last_response, "[ERROR] Not connected to Wifi. Failed to reconnect.") != NULL)
        {
            canvas_clear(canvas);
            canvas_draw_str(canvas, 0, 10, "[ERROR] Not connected to Wifi.");
            canvas_draw_str(canvas, 0, 50, "Update your WiFi settings.");
            canvas_draw_str(canvas, 0, 60, "Press BACK to return.");
        }
        else if (strstr(model->fhttp->last_response, "[ERROR] Failed to connect to Wifi.") != NULL)
        {
            canvas_clear(canvas);
            canvas_draw_str(canvas, 0, 10, "[ERROR] Not connected to Wifi.");
            canvas_draw_str(canvas, 0, 50, "Update your WiFi settings.");
            canvas_draw_str(canvas, 0, 60, "Press BACK to return.");
        }
        else if (strstr(model->fhttp->last_response, "[ERROR] GET request failed or returned empty data.") != NULL)
        {
            canvas_clear(canvas);
            canvas_draw_str(canvas, 0, 10, "[ERROR] WiFi error.");
            canvas_draw_str(canvas, 0, 50, "Update your WiFi settings.");
            canvas_draw_str(canvas, 0, 60, "Press BACK to return.");
        }
        else if (strstr(model->fhttp->last_response, "[PONG]") != NULL)
        {
            canvas_clear(canvas);
            canvas_draw_str(canvas, 0, 10, "[STATUS]Connecting to AP...");
        }
        else
        {
            canvas_clear(canvas);
            FURI_LOG_E(TAG, "Received an error: %s", model->fhttp->last_response);
            canvas_draw_str(canvas, 0, 10, "[ERROR] Unusual error...");
            canvas_draw_str(canvas, 0, 60, "Press BACK and retry.");
        }
    }
    else
    {
        canvas_clear(canvas);
        canvas_draw_str(canvas, 0, 10, "[ERROR] Unknown error.");
        canvas_draw_str(canvas, 0, 50, "Update your WiFi settings.");
        canvas_draw_str(canvas, 0, 60, "Press BACK to return.");
    }
}

static void loader_process_callback(void *context)
{
    if (context == NULL)
    {
        FURI_LOG_E(TAG, "loader_process_callback - context is NULL");
        DEV_CRASH();
        return;
    }

    FlipWorldApp *app = (FlipWorldApp *)context;
    View *view = app->view_loader;

    DataState current_data_state;
    DataLoaderModel *loader_model = NULL;
    with_view_model(
        view,
        DataLoaderModel * model,
        {
            current_data_state = model->data_state;
            loader_model = model;
        },
        false);
    if (!loader_model || !loader_model->fhttp)
    {
        FURI_LOG_E(TAG, "Model or fhttp is NULL");
        DEV_CRASH();
        return;
    }

    if (current_data_state == DataStateInitial)
    {
        with_view_model(
            view,
            DataLoaderModel * model,
            {
                model->data_state = DataStateRequested;
                DataLoaderFetch fetch = model->fetcher;
                if (fetch == NULL)
                {
                    FURI_LOG_E(TAG, "Model doesn't have Fetch function assigned.");
                    model->data_state = DataStateError;
                    return;
                }

                // Clear any previous responses
                strncpy(model->fhttp->last_response, "", 1);
                bool request_status = fetch(model);
                if (!request_status)
                {
                    model->data_state = DataStateError;
                }
            },
            true);
    }
    else if (current_data_state == DataStateRequested || current_data_state == DataStateError)
    {
        if (loader_model->fhttp->state == IDLE && loader_model->fhttp->last_response != NULL)
        {
            if (strstr(loader_model->fhttp->last_response, "[PONG]") != NULL)
            {
                FURI_LOG_DEV(TAG, "PONG received.");
            }
            else if (strncmp(loader_model->fhttp->last_response, "[SUCCESS]", 9))
            {
                FURI_LOG_DEV(TAG, "SUCCESS received. %s", loader_model->fhttp->last_response ? loader_model->fhttp->last_response : "NULL");
            }
            else if (strncmp(loader_model->fhttp->last_response, "[ERROR]", 9))
            {
                FURI_LOG_DEV(TAG, "ERROR received. %s", loader_model->fhttp->last_response ? loader_model->fhttp->last_response : "NULL");
            }
            else if (strlen(loader_model->fhttp->last_response))
            {
                // Still waiting on response
            }
            else
            {
                with_view_model(view, DataLoaderModel * model, { model->data_state = DataStateReceived; }, true);
            }
        }
        else if (loader_model->fhttp->state == SENDING || loader_model->fhttp->state == RECEIVING)
        {
            // continue waiting
        }
        else if (loader_model->fhttp->state == INACTIVE)
        {
            // inactive. try again
        }
        else if (loader_model->fhttp->state == ISSUE)
        {
            with_view_model(view, DataLoaderModel * model, { model->data_state = DataStateError; }, true);
        }
        else
        {
            FURI_LOG_DEV(TAG, "Unexpected state: %d lastresp: %s", loader_model->fhttp->state, loader_model->fhttp->last_response ? loader_model->fhttp->last_response : "NULL");
            DEV_CRASH();
        }
    }
    else if (current_data_state == DataStateReceived)
    {
        with_view_model(
            view,
            DataLoaderModel * model,
            {
                char *data_text;
                if (model->parser == NULL)
                {
                    data_text = NULL;
                    FURI_LOG_DEV(TAG, "Parser is NULL");
                    DEV_CRASH();
                }
                else
                {
                    data_text = model->parser(model);
                }
                FURI_LOG_DEV(TAG, "Parsed data: %s\r\ntext: %s", model->fhttp->last_response ? model->fhttp->last_response : "NULL", data_text ? data_text : "NULL");
                model->data_text = data_text;
                if (data_text == NULL)
                {
                    model->data_state = DataStateParseError;
                }
                else
                {
                    model->data_state = DataStateParsed;
                }
            },
            true);
    }
    else if (current_data_state == DataStateParsed)
    {
        with_view_model(
            view,
            DataLoaderModel * model,
            {
                if (++model->request_index < model->request_count)
                {
                    model->data_state = DataStateInitial;
                }
                else
                {
                    loader_widget_set_text(model->data_text != NULL ? model->data_text : "", &app->widget_result);
                    if (model->data_text != NULL)
                    {
                        free(model->data_text);
                        model->data_text = NULL;
                    }
                    view_set_previous_callback(widget_get_view(app->widget_result), model->back_callback);
                    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewWidgetResult);
                }
            },
            true);
    }
}

bool loader_custom_event_callback(void *context, uint32_t index)
{
    if (context == NULL)
    {
        FURI_LOG_E(TAG, "custom_event_callback - context is NULL");
        DEV_CRASH();
        return false;
    }

    switch (index)
    {
    case FlipWorldCustomEventProcess:
        loader_process_callback(context);
        return true;
    default:
        FURI_LOG_DEV(TAG, "custom_event_callback. Unknown index: %ld", index);
        return false;
    }
}

void loader_draw_callback(Canvas *canvas, void *model)
{
    if (!canvas || !model)
    {
        FURI_LOG_E(TAG, "loader_draw_callback - canvas or model is NULL");
        return;
    }

    DataLoaderModel *data_loader_model = (DataLoaderModel *)model;
    HTTPState http_state = data_loader_model->fhttp->state;
    DataState data_state = data_loader_model->data_state;
    char *title = data_loader_model->title;

    canvas_set_font(canvas, FontSecondary);

    if (http_state == INACTIVE)
    {
        canvas_draw_str(canvas, 0, 7, "Wifi Dev Board disconnected.");
        canvas_draw_str(canvas, 0, 17, "Please connect to the board.");
        canvas_draw_str(canvas, 0, 32, "If your board is connected,");
        canvas_draw_str(canvas, 0, 42, "make sure you have flashed");
        canvas_draw_str(canvas, 0, 52, "your WiFi Devboard with the");
        canvas_draw_str(canvas, 0, 62, "latest FlipperHTTP flash.");
        return;
    }

    if (data_state == DataStateError || data_state == DataStateParseError)
    {
        loader_error_draw(canvas, data_loader_model);
        return;
    }

    canvas_draw_str(canvas, 0, 7, title);
    canvas_draw_str(canvas, 0, 17, "Loading...");

    if (data_state == DataStateInitial)
    {
        return;
    }

    if (http_state == SENDING)
    {
        canvas_draw_str(canvas, 0, 27, "Fetching...");
        return;
    }

    if (http_state == RECEIVING || data_state == DataStateRequested)
    {
        canvas_draw_str(canvas, 0, 27, "Receiving...");
        return;
    }

    if (http_state == IDLE && data_state == DataStateReceived)
    {
        canvas_draw_str(canvas, 0, 27, "Processing...");
        return;
    }

    if (http_state == IDLE && data_state == DataStateParsed)
    {
        canvas_draw_str(canvas, 0, 27, "Processed...");
        return;
    }
}

static void loader_timer_callback(void *context)
{
    if (context == NULL)
    {
        FURI_LOG_E(TAG, "loader_timer_callback - context is NULL");
        DEV_CRASH();
        return;
    }
    FlipWorldApp *app = (FlipWorldApp *)context;
    view_dispatcher_send_custom_event(app->view_dispatcher, FlipWorldCustomEventProcess);
}

static void loader_on_enter(void *context)
{
    if (context == NULL)
    {
        FURI_LOG_E(TAG, "loader_on_enter - context is NULL");
        DEV_CRASH();
        return;
    }
    FlipWorldApp *app = (FlipWorldApp *)context;
    View *view = app->view_loader;
    with_view_model(
        view,
        DataLoaderModel * model,
        {
            view_set_previous_callback(view, model->back_callback);
            if (model->timer == NULL)
            {
                model->timer = furi_timer_alloc(loader_timer_callback, FuriTimerTypePeriodic, app);
            }
            furi_timer_start(model->timer, 250);
        },
        true);
}

static void loader_on_exit(void *context)
{
    if (context == NULL)
    {
        FURI_LOG_E(TAG, "loader_on_exit - context is NULL");
        DEV_CRASH();
        return;
    }
    FlipWorldApp *app = (FlipWorldApp *)context;
    View *view = app->view_loader;
    with_view_model(
        view,
        DataLoaderModel * model,
        {
            if (model->timer)
            {
                furi_timer_stop(model->timer);
            }
        },
        false);
}

void loader_init(View *view)
{
    if (view == NULL)
    {
        FURI_LOG_E(TAG, "loader_init - view is NULL");
        DEV_CRASH();
        return;
    }
    view_allocate_model(view, ViewModelTypeLocking, sizeof(DataLoaderModel));
    view_set_enter_callback(view, loader_on_enter);
    view_set_exit_callback(view, loader_on_exit);
}

void loader_free_model(View *view)
{
    if (view == NULL)
    {
        FURI_LOG_E(TAG, "loader_free_model - view is NULL");
        DEV_CRASH();
        return;
    }
    with_view_model(
        view,
        DataLoaderModel * model,
        {
            if (model->timer)
            {
                furi_timer_free(model->timer);
                model->timer = NULL;
            }
            if (model->parser_context)
            {
                // do not free the context here, it is the app context
                // free(model->parser_context);
                // model->parser_context = NULL;
            }
            if (model->fhttp)
            {
                flipper_http_free(model->fhttp);
                model->fhttp = NULL;
            }
        },
        false);
}

void loader_switch_to_view(FlipWorldApp *app, char *title, DataLoaderFetch fetcher, DataLoaderParser parser, size_t request_count, ViewNavigationCallback back, uint32_t view_id)
{
    if (app == NULL)
    {
        FURI_LOG_E(TAG, "loader_switch_to_view - app is NULL");
        DEV_CRASH();
        return;
    }

    View *view = app->view_loader;
    if (view == NULL)
    {
        FURI_LOG_E(TAG, "loader_switch_to_view - view is NULL");
        DEV_CRASH();
        return;
    }

    with_view_model(
        view,
        DataLoaderModel * model,
        {
            model->title = title;
            model->fetcher = fetcher;
            model->parser = parser;
            model->request_index = 0;
            model->request_count = request_count;
            model->back_callback = back;
            model->data_state = DataStateInitial;
            model->data_text = NULL;
            //
            model->parser_context = app;
            if (!model->fhttp)
            {
                model->fhttp = flipper_http_alloc();
            }
        },
        true);

    view_dispatcher_switch_to_view(app->view_dispatcher, view_id);
}

void loader_widget_set_text(char *message, Widget **widget)
{
    if (widget == NULL)
    {
        FURI_LOG_E(TAG, "set_widget_text - widget is NULL");
        DEV_CRASH();
        return;
    }
    if (message == NULL)
    {
        FURI_LOG_E(TAG, "set_widget_text - message is NULL");
        DEV_CRASH();
        return;
    }
    widget_reset(*widget);

    uint32_t message_length = strlen(message); // Length of the message
    uint32_t i = 0;                            // Index tracker
    uint32_t formatted_index = 0;              // Tracker for where we are in the formatted message
    char *formatted_message;                   // Buffer to hold the final formatted message

    // Allocate buffer with double the message length plus one for safety
    if (!easy_flipper_set_buffer(&formatted_message, message_length * 2 + 1))
    {
        return;
    }

    while (i < message_length)
    {
        uint32_t max_line_length = 31;                  // Maximum characters per line
        uint32_t remaining_length = message_length - i; // Remaining characters
        uint32_t line_length = (remaining_length < max_line_length) ? remaining_length : max_line_length;

        // Check for newline character within the current segment
        uint32_t newline_pos = i;
        bool found_newline = false;
        for (; newline_pos < i + line_length && newline_pos < message_length; newline_pos++)
        {
            if (message[newline_pos] == '\n')
            {
                found_newline = true;
                break;
            }
        }

        if (found_newline)
        {
            // If newline found, set line_length up to the newline
            line_length = newline_pos - i;
        }

        // Temporary buffer to hold the current line
        char line[32];
        strncpy(line, message + i, line_length);
        line[line_length] = '\0';

        // If newline was found, skip it for the next iteration
        if (found_newline)
        {
            i += line_length + 1; // +1 to skip the '\n' character
        }
        else
        {
            // Check if the line ends in the middle of a word and adjust accordingly
            if (line_length == max_line_length && message[i + line_length] != '\0' && message[i + line_length] != ' ')
            {
                // Find the last space within the current line to avoid breaking a word
                char *last_space = strrchr(line, ' ');
                if (last_space != NULL)
                {
                    // Adjust the line_length to avoid cutting the word
                    line_length = last_space - line;
                    line[line_length] = '\0'; // Null-terminate at the space
                }
            }

            // Move the index forward by the determined line_length
            i += line_length;

            // Skip any spaces at the beginning of the next line
            while (i < message_length && message[i] == ' ')
            {
                i++;
            }
        }

        // Manually copy the fixed line into the formatted_message buffer
        for (uint32_t j = 0; j < line_length; j++)
        {
            formatted_message[formatted_index++] = line[j];
        }

        // Add a newline character for line spacing
        formatted_message[formatted_index++] = '\n';
    }

    // Null-terminate the formatted_message
    formatted_message[formatted_index] = '\0';

    // Add the formatted message to the widget
    widget_add_text_scroll_element(*widget, 0, 0, 128, 64, formatted_message);
}