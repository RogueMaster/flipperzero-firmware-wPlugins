#include "app.hpp"

VideoGameModuleConsoleApp::VideoGameModuleConsoleApp()
{
    gui = static_cast<Gui *>(furi_record_open(RECORD_GUI));

    // Allocate ViewDispatcher
    viewDispatcher = view_dispatcher_alloc();
    if (!viewDispatcher)
    {
        FURI_LOG_E(TAG, "Failed to allocate ViewDispatcher");
        return;
    }
    view_dispatcher_attach_to_gui(viewDispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(viewDispatcher, this);

    // Submenu
    submenu = submenu_alloc();
    if (!submenu)
    {
        FURI_LOG_E(TAG, "Failed to allocate Submenu");
        return;
    }
    submenu_set_header(submenu, VERSION_TAG);
    view_set_previous_callback(submenu_get_view(submenu), callbackExitApp);
    view_dispatcher_add_view(viewDispatcher, VideoGameModuleConsoleViewSubmenu, submenu_get_view(submenu));

    submenu_add_item(submenu, "Run", VideoGameModuleConsoleSubmenuRun, submenuChoicesCallback, this);
    submenu_add_item(submenu, "About", VideoGameModuleConsoleSubmenuAbout, submenuChoicesCallback, this);

    flipperHttp = flipper_http_alloc();
    if (!flipperHttp)
    {
        FURI_LOG_E(TAG, "Failed to allocate FlipperHTTP");
        return;
    }

    // Allocate the draw-command queue (32 slots of char*) and register the per-line callback
    // so every UART line is enqueued rather than overwriting a single last_response buffer.
    responseQueue = furi_message_queue_alloc(32, sizeof(char *));
    if (responseQueue)
    {
        flipperHttp->user_rx_line_cb = httpLineCallback;
        flipperHttp->user_callback_context = this;
    }

    createAppDataPath();

    // Register custom event callback so the run thread can signal completion
    view_dispatcher_set_custom_event_callback(viewDispatcher, customEventCallback);

    // Switch to the submenu view
    view_dispatcher_switch_to_view(viewDispatcher, VideoGameModuleConsoleViewSubmenu);
}

VideoGameModuleConsoleApp::~VideoGameModuleConsoleApp()
{
    // If the run thread is still alive wait for it
    if (runThread)
    {
        furi_thread_join(runThread);
        furi_thread_free(runThread);
        runThread = nullptr;
    }

    // Drain and free the input queue
    if (inputQueue)
    {
        InputEvent tmp;
        while (furi_message_queue_get(inputQueue, &tmp, 0) == FuriStatusOk)
        {
        }
        furi_message_queue_free(inputQueue);
        inputQueue = nullptr;
    }

    // Release direct canvas access if it was acquired during run
    if (gui && canvas)
    {
        gui_direct_draw_release(gui);
        canvas = nullptr;
    }

    // Remove and free the run ViewPort if still present
    if (gui && viewPort)
    {
        gui_remove_view_port(gui, viewPort);
        view_port_free(viewPort);
        viewPort = nullptr;
    }

    // Clean up run
    if (run)
    {
        run.reset();
    }

    // Clean up about
    if (about)
    {
        about.reset();
    }

    // Free submenu
    if (submenu)
    {
        view_dispatcher_remove_view(viewDispatcher, VideoGameModuleConsoleViewSubmenu);
        submenu_free(submenu);
    }

    // Free view dispatcher
    if (viewDispatcher)
    {
        view_dispatcher_free(viewDispatcher);
    }

    // Close GUI
    if (gui)
    {
        furi_record_close(RECORD_GUI);
    }

    // Free FlipperHTTP (stops UART worker thread before we touch the queue)
    if (flipperHttp)
    {
        flipper_http_free(flipperHttp);
    }

    // Drain and free the response queue after the worker thread has stopped
    if (responseQueue)
    {
        char *line = nullptr;
        while (furi_message_queue_get(responseQueue, &line, 0) == FuriStatusOk && line)
        {
            free(line);
        }
        furi_message_queue_free(responseQueue);
        responseQueue = nullptr;
    }
}

uint32_t VideoGameModuleConsoleApp::callbackExitApp(void *context)
{
    UNUSED(context);
    return VIEW_NONE;
}

void VideoGameModuleConsoleApp::callbackSubmenuChoices(uint32_t index)
{
    switch (index)
    {
    case VideoGameModuleConsoleSubmenuRun:
        if (!run)
        {
            run = std::make_unique<VideoGameModuleConsoleRun>(this);
        }

        // Input queue: run thread drains this; ViewPort enqueues into it
        inputQueue = furi_message_queue_alloc(8, sizeof(InputEvent));

        // ViewPort in GuiLayerFullscreen intercepts all input so the
        // view dispatcher never sees key events while the game is running
        viewPort = view_port_alloc();
        view_port_input_callback_set(viewPort, viewPortInputEnqueue, this);
        gui_add_view_port(gui, viewPort, GuiLayerFullscreen);

        runThread = furi_thread_alloc_ex("GameBoyRun", 4096, runThreadCallback, this);
        furi_thread_start(runThread);
        break;
    case VideoGameModuleConsoleSubmenuAbout:
        if (!about)
        {
            about = std::make_unique<VideoGameModuleConsoleAbout>(viewDispatcher);
        }
        view_dispatcher_switch_to_view(viewDispatcher, VideoGameModuleConsoleViewAbout);
        break;
    default:
        break;
    }
}

void VideoGameModuleConsoleApp::clearHttpBuffer()
{
    if (flipperHttp)
    {
        memset(flipperHttp->file_buffer, 0, sizeof(flipperHttp->file_buffer));
        flipperHttp->file_buffer_len = 0;
    }
}

void VideoGameModuleConsoleApp::clearHttpResponse()
{
    if (flipperHttp && flipperHttp->last_response)
    {
        memset(flipperHttp->last_response, 0, strlen(flipperHttp->last_response) + 1);
    }
}

void VideoGameModuleConsoleApp::httpLineCallback(const char *line, void *context)
{
    VideoGameModuleConsoleApp *app = static_cast<VideoGameModuleConsoleApp *>(context);
    if (!app || !app->responseQueue || !line || line[0] == '\0')
        return;

    char *copy = strdup(line);
    if (!copy)
        return;

    if (furi_message_queue_put(app->responseQueue, &copy, 0) != FuriStatusOk)
    {
        // Queue full — drop the oldest item and retry once
        char *dropped = nullptr;
        furi_message_queue_get(app->responseQueue, &dropped, 0);
        if (dropped)
            free(dropped);
        if (furi_message_queue_put(app->responseQueue, &copy, 0) != FuriStatusOk)
            free(copy);
    }
}

char *VideoGameModuleConsoleApp::popHttpResponse()
{
    if (!responseQueue)
        return nullptr;
    char *line = nullptr;
    if (furi_message_queue_get(responseQueue, &line, 0) == FuriStatusOk)
        return line;
    return nullptr;
}

void VideoGameModuleConsoleApp::createAppDataPath(const char *appId)
{
    Storage *storage = static_cast<Storage *>(furi_record_open(RECORD_STORAGE));
    char directory_path[256];
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/%s", appId);
    storage_common_mkdir(storage, directory_path);
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/%s/data", appId);
    storage_common_mkdir(storage, directory_path);
    furi_record_close(RECORD_STORAGE);
}

bool VideoGameModuleConsoleApp::getHttpBuffer(uint8_t *buffer, size_t buffer_size)
{
    if (!flipperHttp || flipperHttp->file_buffer_len == 0)
    {
        FURI_LOG_E(TAG, "No HTTP buffer available");
        return false;
    }
    if (flipperHttp->file_buffer_len > buffer_size)
    {
        FURI_LOG_E(TAG, "HTTP buffer is too large for the provided buffer");
        return false;
    }
    memcpy(buffer, flipperHttp->file_buffer, flipperHttp->file_buffer_len);
    return true;
}

bool VideoGameModuleConsoleApp::loadChar(const char *path_name, char *value, size_t value_size, const char *appId)
{
    Storage *storage = static_cast<Storage *>(furi_record_open(RECORD_STORAGE));
    File *file = storage_file_alloc(storage);
    char file_path[256];
    snprintf(file_path, sizeof(file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/%s/data/%s.txt", appId, path_name);
    if (!storage_file_open(file, file_path, FSAM_READ, FSOM_OPEN_EXISTING))
    {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }
    size_t read_count = storage_file_read(file, value, value_size);
    // ensure we don't go out of bounds
    if (read_count > 0 && read_count < value_size)
    {
        value[read_count - 1] = '\0';
    }
    else if (read_count >= value_size && value_size > 0)
    {
        value[value_size - 1] = '\0';
    }
    else
    {
        value[0] = '\0';
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return strlen(value) > 0;
}

void VideoGameModuleConsoleApp::runDispatcher()
{
    view_dispatcher_run(viewDispatcher);
}

bool VideoGameModuleConsoleApp::saveChar(const char *path_name, const char *value, const char *appId, bool overwrite)
{
    Storage *storage = static_cast<Storage *>(furi_record_open(RECORD_STORAGE));
    File *file = storage_file_alloc(storage);
    char file_path[256];
    snprintf(file_path, sizeof(file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/%s/data/%s.txt", appId, path_name);
    if (!storage_file_open(file, file_path, FSAM_WRITE, overwrite ? FSOM_CREATE_ALWAYS : FSOM_OPEN_APPEND))
    {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }
    size_t data_size = strlen(value);
    if (storage_file_write(file, value, data_size) != data_size)
    {
        FURI_LOG_E(TAG, "Failed to write complete data to file: %s", file_path);
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return true;
}

bool VideoGameModuleConsoleApp::sendData(const char *data)
{
    if (!flipperHttp)
    {
        FURI_LOG_E(TAG, "FlipperHTTP is not initialized");
        return false;
    }
    if (!data || strlen(data) == 0)
    {
        FURI_LOG_E(TAG, "Data is NULL or empty");
        return false;
    }
    return flipper_http_send_data(flipperHttp, data);
}

void VideoGameModuleConsoleApp::submenuChoicesCallback(void *context, uint32_t index)
{
    VideoGameModuleConsoleApp *app = (VideoGameModuleConsoleApp *)context;
    app->callbackSubmenuChoices(index);
}

void VideoGameModuleConsoleApp::viewPortInputEnqueue(InputEvent *event, void *context)
{
    VideoGameModuleConsoleApp *app = static_cast<VideoGameModuleConsoleApp *>(context);
    if (!app || !app->inputQueue)
        return;
    // Only forward actionable event types; drop press/release noise
    if (event->type != InputTypeShort && event->type != InputTypeLong && event->type != InputTypeRepeat)
        return;
    // Non-blocking put: if the queue is full, the oldest item is already pending
    furi_message_queue_put(app->inputQueue, event, 0);
}

int32_t VideoGameModuleConsoleApp::runThreadCallback(void *context)
{
    VideoGameModuleConsoleApp *app = static_cast<VideoGameModuleConsoleApp *>(context);
    furi_check(app);

    // Acquire the direct canvas
    app->canvas = gui_direct_draw_acquire(app->gui);

    auto *run = app->run.get();
    while (run && run->isActive())
    {
        // Drain every queued input event before drawing
        InputEvent event;
        while (app->inputQueue &&
               furi_message_queue_get(app->inputQueue, &event, 0) == FuriStatusOk)
        {
            run->updateInput(&event);
        }

        // Draw the current frame and push it to the screen
        if (app->canvas)
        {
            run->updateDraw(app->canvas);
            canvas_commit(app->canvas);
        }
    }

    // Release canvas before signalling so the GUI can resume immediately
    gui_direct_draw_release(app->gui);
    app->canvas = nullptr;

    // Ask the view dispatcher thread (main thread) to do the rest of the cleanup
    view_dispatcher_send_custom_event(app->viewDispatcher, APP_EVENT_RUN_FINISHED);
    return 0;
}

bool VideoGameModuleConsoleApp::customEventCallback(void *context, uint32_t event)
{
    if (event == APP_EVENT_RUN_FINISHED)
    {
        VideoGameModuleConsoleApp *app = static_cast<VideoGameModuleConsoleApp *>(context);

        // Join the run thread (it has already returned at this point)
        if (app->runThread)
        {
            furi_thread_join(app->runThread);
            furi_thread_free(app->runThread);
            app->runThread = nullptr;
        }

        // Drain and free the input queue
        if (app->inputQueue)
        {
            InputEvent tmp;
            while (furi_message_queue_get(app->inputQueue, &tmp, 0) == FuriStatusOk)
            {
            }
            furi_message_queue_free(app->inputQueue);
            app->inputQueue = nullptr;
        }

        // Remove the fullscreen ViewPort that was blocking input to the view dispatcher
        if (app->gui && app->viewPort)
        {
            gui_remove_view_port(app->gui, app->viewPort);
            view_port_free(app->viewPort);
            app->viewPort = nullptr;
        }

        // Destroy the run instance and return to the submenu
        app->run.reset();
        view_dispatcher_switch_to_view(app->viewDispatcher, VideoGameModuleConsoleViewSubmenu);
        return true;
    }
    return false;
}

extern "C"
{
    int32_t video_game_module_console_main(void *p)
    {
        // Suppress unused parameter warning
        UNUSED(p);

        // Create the app
        VideoGameModuleConsoleApp app;

        // Run the app
        app.runDispatcher();

        // return success
        return 0;
    }
}
