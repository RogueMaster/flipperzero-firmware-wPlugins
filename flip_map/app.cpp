#include "app.hpp"

FlipMapApp::FlipMapApp()
{
    gui = static_cast<Gui *>(furi_record_open(RECORD_GUI));

    // Allocate ViewDispatcher
    if (!easy_flipper_set_view_dispatcher(&viewDispatcher, gui, this))
    {
        FURI_LOG_E(TAG, "Failed to allocate view dispatcher");
        return;
    }

    // Submenu
    if (!easy_flipper_set_submenu(&submenu, FlipMapViewSubmenu,
                                  VERSION_TAG, callbackExitApp, &viewDispatcher))
    {
        FURI_LOG_E(TAG, "Failed to allocate submenu");
        return;
    }

    submenu_add_item(submenu, "Run", FlipMapSubmenuRun, submenuChoicesCallback, this);
    submenu_add_item(submenu, "About", FlipMapSubmenuAbout, submenuChoicesCallback, this);
    submenu_add_item(submenu, "Settings", FlipMapSubmenuSettings, submenuChoicesCallback, this);

    flipperHttp = flipper_http_alloc();
    if (!flipperHttp)
    {
        FURI_LOG_E(TAG, "Failed to allocate FlipperHTTP");
        return;
    }

    createAppDataPath(APP_ID);
    createAppDataPath("flipper_http");

    // Switch to the submenu view
    view_dispatcher_switch_to_view(viewDispatcher, FlipMapViewSubmenu);
}

FlipMapApp::~FlipMapApp()
{
    // Stop and free timer first
    if (timer)
    {
        furi_timer_stop(timer);
        furi_timer_free(timer);
        timer = nullptr;
    }

    // Clean up viewport if it exists
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

    // Clean up settings
    if (settings)
    {
        settings.reset();
    }

    // Clean up about
    if (about)
    {
        about.reset();
    }

    // Free submenu
    if (submenu)
    {
        view_dispatcher_remove_view(viewDispatcher, FlipMapViewSubmenu);
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

    // Free FlipperHTTP
    if (flipperHttp)
    {
        flipper_http_free(flipperHttp);
    }
}

uint32_t FlipMapApp::callbackExitApp(void *context)
{
    UNUSED(context);
    return VIEW_NONE;
}

void FlipMapApp::callbackSubmenuChoices(uint32_t index)
{
    switch (index)
    {
    case FlipMapSubmenuRun:
        // if the board is not connected, we can't use WiFi
        if (!isBoardConnected())
        {
            easy_flipper_dialog("FlipperHTTP Error", "Ensure your WiFi Developer\nBoard or Pico W is connected\nand the latest FlipperHTTP\nfirmware is installed.");
            return;
        }
        // if we don't have WiFi credentials, we can't connect to WiFi in case
        // we are not connected to WiFi yet
        if (!hasWiFiCredentials())
        {
            easy_flipper_dialog("No WiFi Credentials", "Please set your WiFi SSID\nand Password in Settings.");
            return;
        }

        // if we don't have user credentials, we can't connect to the user account
        if (!hasUserCredentials())
        {
            easy_flipper_dialog("No User Credentials", "Please set your Username\nand Password in Settings.");
            return;
        }

        if (!run)
        {
            run = std::make_unique<FlipMapRun>(this);
        }

        // setup view port
        viewPort = view_port_alloc();
        view_port_draw_callback_set(viewPort, viewPortDraw, this);
        view_port_input_callback_set(viewPort, viewPortInput, this);
        gui_add_view_port(gui, viewPort, GuiLayerFullscreen);

        // make sure timer is free
        if (timer)
        {
            furi_timer_stop(timer);
            furi_timer_free(timer);
            timer = nullptr;
        }

        // Start the timer for game updates
        if (!timer)
        {
            timer = furi_timer_alloc(timerCallback, FuriTimerTypePeriodic, this);
        }
        if (timer)
        {
            furi_timer_start(timer, 100); // Update every 100ms
        }
        break;
    case FlipMapSubmenuAbout:
        if (!about)
        {
            about = std::make_unique<FlipMapAbout>(&viewDispatcher);
        }
        view_dispatcher_switch_to_view(viewDispatcher, FlipMapViewAbout);
        break;
    case FlipMapSubmenuSettings:
        // if the board is not connected, we can't use WiFi when getting location
        if (!isBoardConnected())
        {
            easy_flipper_dialog("FlipperHTTP Error", "Ensure your WiFi Developer\nBoard or Pico W is connected\nand the latest FlipperHTTP\nfirmware is installed.");
            return;
        }
        if (!settings)
        {
            settings = std::make_unique<FlipMapSettings>(&viewDispatcher, this);
        }
        view_dispatcher_switch_to_view(viewDispatcher, FlipMapViewSettings);
        break;
    default:
        break;
    }
}

void FlipMapApp::createAppDataPath(const char *appId)
{
    Storage *storage = static_cast<Storage *>(furi_record_open(RECORD_STORAGE));
    char directory_path[256];
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/%s", appId);
    storage_common_mkdir(storage, directory_path);
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/%s/data", appId);
    storage_common_mkdir(storage, directory_path);
    furi_record_close(RECORD_STORAGE);
}

bool FlipMapApp::hasWiFiCredentials()
{
    char ssid[64] = {0};
    char password[64] = {0};
    return loadChar("wifi_ssid", ssid, sizeof(ssid), "flipper_http") &&
           loadChar("wifi_pass", password, sizeof(password), "flipper_http") &&
           strlen(ssid) > 0 &&
           strlen(password) > 0;
}

bool FlipMapApp::hasUserCredentials()
{
    char username[64] = {0};
    char password[64] = {0};
    return loadChar("user_name", username, sizeof(username), "flipper_http") &&
           loadChar("user_pass", password, sizeof(password), "flipper_http") &&
           strlen(username) > 0 &&
           strlen(password) > 0;
}

FuriString *FlipMapApp::httpRequest(
    const char *url,
    HTTPMethod method,
    const char *headers,
    const char *payload)
{
    if (!flipperHttp)
    {
        FURI_LOG_E(TAG, "FlipMapApp::httpRequest: FlipperHTTP is NULL");
        return NULL;
    }
    snprintf(flipperHttp->file_path, sizeof(flipperHttp->file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/%s/data/temp.json", APP_ID);
    flipperHttp->save_received_data = true;
    flipperHttp->state = IDLE;
    if (!flipper_http_request(flipperHttp, method, url, headers, payload))
    {
        FURI_LOG_E(TAG, "FlipMapApp::httpRequest: Failed to send HTTP request");
        return NULL;
    }
    flipperHttp->state = RECEIVING;
    while (flipperHttp->state != IDLE)
    {
        furi_delay_ms(100);
    }
    return flipper_http_load_from_file(flipperHttp->file_path);
}

bool FlipMapApp::httpRequestAsync(
    const char *saveLocation,
    const char *url,
    HTTPMethod method,
    const char *headers,
    const char *payload)
{
    if (!flipperHttp)
    {
        FURI_LOG_E(TAG, "FlipMapApp::httpRequestAsync: FlipperHTTP is NULL");
        return false;
    }
    snprintf(flipperHttp->file_path, sizeof(flipperHttp->file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/%s/data/%s", APP_ID, saveLocation);
    flipperHttp->save_received_data = true;
    flipperHttp->state = IDLE;
    if (!flipper_http_request(flipperHttp, method, url, headers, payload))
    {
        FURI_LOG_E(TAG, "FlipMapApp::httpRequestAsync: Failed to send HTTP request");
        return false;
    }
    flipperHttp->state = RECEIVING;
    return true;
}

bool FlipMapApp::isBoardConnected()
{
    if (!flipperHttp)
    {
        FURI_LOG_E(TAG, "FlipperHTTP is not initialized");
        return false;
    }

    if (!flipper_http_send_command(flipperHttp, HTTP_CMD_PING))
    {
        FURI_LOG_E(TAG, "Failed to ping the device");
        return false;
    }

    furi_delay_ms(100);

    // Try to wait for pong response.
    uint32_t counter = 100;
    while (flipperHttp->state == INACTIVE && --counter > 0)
    {
        furi_delay_ms(100);
    }

    // last response should be PONG
    return flipperHttp->last_response && strcmp(flipperHttp->last_response, "[PONG]") == 0;
}

bool FlipMapApp::loadChar(const char *path_name, char *value, size_t value_size, const char *appId)
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

bool FlipMapApp::loadFileChunk(const char *filePath, char *buffer, size_t sizeOfChunk, uint8_t iteration)
{
    if (!filePath || !buffer || sizeOfChunk == 0)
    {
        FURI_LOG_E(TAG, "Invalid parameters for loadFileChunk");
        return false;
    }

    Storage *storage = static_cast<Storage *>(furi_record_open(RECORD_STORAGE));
    if (!storage)
    {
        FURI_LOG_E(TAG, "Failed to open storage record");
        return false;
    }

    File *file = storage_file_alloc(storage);
    if (!file)
    {
        FURI_LOG_E(TAG, "Failed to allocate file");
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    // Open the file for reading
    if (!storage_file_open(file, filePath, FSAM_READ, FSOM_OPEN_EXISTING))
    {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        FURI_LOG_E(TAG, "Failed to open file for reading: %s", filePath);
        return false;
    }

    // Change the current access position in a file.
    if (!storage_file_seek(file, iteration * sizeOfChunk, true))
    {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        FURI_LOG_E(TAG, "Failed to seek file: %s", filePath);
        return false;
    }

    // Check whether the current access position is at the end of the file.
    if (storage_file_eof(file))
    {
        FURI_LOG_E(TAG, "End of file reached: %s", filePath);
        return false;
    }

    // Read data into the buffer
    size_t read_count = storage_file_read(file, buffer, sizeOfChunk);
    if (storage_file_get_error(file) != FSE_OK)
    {
        FURI_LOG_E(TAG, "Error reading from file.");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    // ensure we don't go out of bounds
    if (read_count > 0 && read_count < sizeOfChunk)
    {
        buffer[read_count] = '\0'; // Null-terminate after the last read character
    }
    else if (read_count >= sizeOfChunk && sizeOfChunk > 0)
    {
        buffer[sizeOfChunk - 1] = '\0'; // Use last byte for null terminator
    }
    else
    {
        buffer[0] = '\0'; // Empty buffer
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return read_count > 0;
}

void FlipMapApp::runDispatcher()
{
    view_dispatcher_run(viewDispatcher);
}

bool FlipMapApp::saveChar(const char *path_name, const char *value, const char *appId)
{
    Storage *storage = static_cast<Storage *>(furi_record_open(RECORD_STORAGE));
    File *file = storage_file_alloc(storage);
    char file_path[256];
    snprintf(file_path, sizeof(file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/%s/data/%s.txt", appId, path_name);
    storage_file_open(file, file_path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    size_t data_size = strlen(value) + 1; // Include null terminator
    storage_file_write(file, value, data_size);
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return true;
}

bool FlipMapApp::sendWiFiCredentials(const char *ssid, const char *password)
{
    if (!flipperHttp)
    {
        FURI_LOG_E(TAG, "FlipperHTTP is not initialized");
        return false;
    }
    if (!ssid || !password)
    {
        FURI_LOG_E(TAG, "SSID or Password is NULL");
        return false;
    }
    return flipper_http_save_wifi(flipperHttp, ssid, password);
}

bool FlipMapApp::setHttpState(HTTPState state) noexcept
{
    if (flipperHttp)
    {
        flipperHttp->state = state;
        return true;
    }
    return false;
}

void FlipMapApp::settingsItemSelected(uint32_t index)
{
    if (settings)
    {
        settings->settingsItemSelected(index);
    }
}

void FlipMapApp::submenuChoicesCallback(void *context, uint32_t index)
{
    FlipMapApp *app = (FlipMapApp *)context;
    app->callbackSubmenuChoices(index);
}

void FlipMapApp::timerCallback(void *context)
{
    FlipMapApp *app = static_cast<FlipMapApp *>(context);
    furi_check(app);
    auto run = app->run.get();
    if (run)
    {
        if (run->isActive())
        {
            // Run is active, update the viewport
            if (app->viewPort)
            {
                view_port_update(app->viewPort);
            }
        }
        else
        {
            // Stop and cleanup timer first
            if (app->timer)
            {
                furi_timer_stop(app->timer);
            }

            // Clean up viewport if it exists
            if (app->gui && app->viewPort)
            {
                gui_remove_view_port(app->gui, app->viewPort);
                view_port_free(app->viewPort);
                app->viewPort = nullptr;
            }

            view_dispatcher_switch_to_view(app->viewDispatcher, FlipMapViewSubmenu);
            app->run.reset();
        }
    }
}

void FlipMapApp::viewPortDraw(Canvas *canvas, void *context)
{
    FlipMapApp *app = static_cast<FlipMapApp *>(context);
    furi_check(app);
    auto run = app->run.get();
    if (run && run->isActive())
    {
        run->updateDraw(canvas);
    }
}

void FlipMapApp::viewPortInput(InputEvent *event, void *context)
{
    FlipMapApp *app = static_cast<FlipMapApp *>(context);
    furi_check(app);
    auto run = app->run.get();
    if (run && run->isActive())
    {
        run->updateInput(event);
    }
}

extern "C"
{
    int32_t flip_map_main(void *p)
    {
        // Suppress unused parameter warning
        UNUSED(p);

        // Create the app
        FlipMapApp app;

        app.saveChar("app_version", VERSION_TAG);

        // Run the app
        app.runDispatcher();

        // return success
        return 0;
    }
}
