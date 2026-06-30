#include "app.hpp"

uint32_t FreeRoamApp::callback_exit_app(void *context)
{
    UNUSED(context);
    return VIEW_NONE;
}

void FreeRoamApp::settings_item_selected_callback(void *context, uint32_t index)
{
    FreeRoamApp *app = (FreeRoamApp *)context;
    app->settingsItemSelected(index);
}

void FreeRoamApp::submenu_choices_callback(void *context, uint32_t index)
{
    FreeRoamApp *app = (FreeRoamApp *)context;
    app->callbackSubmenuChoices(index);
}

void FreeRoamApp::settingsItemSelected(uint32_t index)
{
    if (settings)
    {
        settings->settingsItemSelected(index);
    }
}

void FreeRoamApp::createAppDataPath(const char *appId)
{
    Storage *storage = static_cast<Storage *>(furi_record_open(RECORD_STORAGE));
    char directory_path[256];
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/%s", appId);
    storage_common_mkdir(storage, directory_path);
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/%s/data", appId);
    storage_common_mkdir(storage, directory_path);
    furi_record_close(RECORD_STORAGE);
}

void FreeRoamApp::viewPortDraw(Canvas *canvas, void *context)
{
    FreeRoamApp *app = static_cast<FreeRoamApp *>(context);
    furi_check(app);
    auto game = app->game.get();
    if (game)
    {
        if (game->isActive())
        {
            game->updateDraw(canvas);
        }
    }
}
void FreeRoamApp::viewPortInput(InputEvent *event, void *context)
{
    if (event->type != InputTypeShort && event->type != InputTypeLong && event->type != InputTypeRepeat)
    {
        return;
    }
    FreeRoamApp *app = static_cast<FreeRoamApp *>(context);
    furi_check(app);
    auto game = app->game.get();
    if (game && game->isActive())
    {
        game->updateInput(event);
    }
}

void FreeRoamApp::timerCallback(void *context)
{
    FreeRoamApp *app = static_cast<FreeRoamApp *>(context);
    furi_check(app);
    auto game = app->game.get();
    if (game)
    {
        if (game->isActive())
        {
            // Game is active, update the viewport
            if (app->viewPort)
            {
                view_port_update(app->viewPort);
            }
        }
        else
        {
            // Stop the timer first
            if (app->timer)
            {
                furi_timer_stop(app->timer);
            }

            // Remove viewport
            if (app->gui && app->viewPort)
            {
                gui_remove_view_port(app->gui, app->viewPort);
                view_port_free(app->viewPort);
                app->viewPort = nullptr;
            }

            view_dispatcher_switch_to_view(app->viewDispatcher, FreeRoamViewSubmenu);
            app->game.reset();
        }
    }
}

void FreeRoamApp::callbackSubmenuChoices(uint32_t index)
{
    switch (index)
    {
    case FreeRoamSubmenuRun:
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

        game = std::make_unique<FreeRoamGame>();
        if (!game->init(this))
        {
            FURI_LOG_E(TAG, "Failed to initialize game");
            game.reset();
            return;
        }

        viewPort = view_port_alloc();
        view_port_draw_callback_set(viewPort, viewPortDraw, this);
        view_port_input_callback_set(viewPort, viewPortInput, this);
        gui_add_view_port(gui, viewPort, GuiLayerFullscreen);

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
    case FreeRoamSubmenuAbout:
        about = std::make_unique<FreeRoamAbout>();
        if (!about->init(&viewDispatcher, this))
        {
            FURI_LOG_E(TAG, "Failed to initialize about");
            about.reset();
            return;
        }
        view_dispatcher_switch_to_view(viewDispatcher, FreeRoamViewAbout);
        break;
    case FreeRoamSubmenuSettings:
        settings = std::make_unique<FreeRoamSettings>();
        if (!settings->init(&viewDispatcher, this))
        {
            FURI_LOG_E(TAG, "Failed to initialize settings");
            settings.reset();
            return;
        }
        view_dispatcher_switch_to_view(viewDispatcher, FreeRoamViewSettings);
        break;
    default:
        break;
    }
}

bool FreeRoamApp::hasWiFiCredentials()
{
    char ssid[64] = {0};
    char password[64] = {0};
    return load_char("wifi_ssid", ssid, sizeof(ssid), "flipper_http") &&
           load_char("wifi_pass", password, sizeof(password), "flipper_http") &&
           strlen(ssid) > 0 &&
           strlen(password) > 0;
}

bool FreeRoamApp::hasUserCredentials()
{
    char username[64] = {0};
    char password[64] = {0};
    return load_char("user_name", username, sizeof(username), "flipper_http") &&
           load_char("user_pass", password, sizeof(password), "flipper_http") &&
           strlen(username) > 0 &&
           strlen(password) > 0;
}

bool FreeRoamApp::isBoardConnected()
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

bool FreeRoamApp::save_char(const char *path_name, const char *value, const char *appId)
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

bool FreeRoamApp::load_char(const char *path_name, char *value, size_t value_size, const char *appId)
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

bool FreeRoamApp::isVibrationEnabled()
{
    char value[16] = {0};
    if (load_char("vibration", value, sizeof(value)))
    {
        return strcmp(value, "On") == 0;
    }
    return true;
}

bool FreeRoamApp::isSoundEnabled()
{
    char value[16] = {0};
    if (load_char("sound", value, sizeof(value)))
    {
        return strcmp(value, "On") == 0;
    }
    return true;
}

void FreeRoamApp::setVibrationEnabled(bool enabled)
{
    save_char("vibration", enabled ? "On" : "Off");
}

void FreeRoamApp::setSoundEnabled(bool enabled)
{
    save_char("sound", enabled ? "On" : "Off");
}

bool FreeRoamApp::httpRequestAsync(
    const char *saveLocation,
    const char *url,
    HTTPMethod method,
    const char *headers,
    const char *payload)
{
    if (!flipperHttp)
    {
        FURI_LOG_E(TAG, "FreeRoamApp::httpRequestAsync: FlipperHTTP is NULL");
        return false;
    }
    snprintf(flipperHttp->file_path, sizeof(flipperHttp->file_path), "%s/%s", STORAGE_EXT_PATH_PREFIX "/apps_data/free_roam/data", saveLocation);
    flipperHttp->save_received_data = true;
    flipperHttp->state = IDLE;
    if (!flipper_http_request(flipperHttp, method, url, headers, payload))
    {
        FURI_LOG_E(TAG, "FreeRoamApp::httpRequestAsync: Failed to send HTTP request");
        return false;
    }
    flipperHttp->state = RECEIVING;
    return true;
}

bool FreeRoamApp::sendWiFiCredentials(const char *ssid, const char *password)
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

bool FreeRoamApp::setHttpState(HTTPState state) noexcept
{
    if (flipperHttp)
    {
        flipperHttp->state = state;
        return true;
    }
    return false;
}

bool FreeRoamApp::websocketStart(const char *url, uint16_t port)
{
    if (!flipperHttp)
    {
        FURI_LOG_E(TAG, "FlipperHTTP is not initialized");
        return false;
    }
    if (!url || strlen(url) == 0)
    {
        FURI_LOG_E(TAG, "WebSocket URL is NULL or empty");
        return false;
    }
    return flipper_http_websocket_start(flipperHttp, url, port, "{\"Content-Type\":\"application/json\"}");
}

bool FreeRoamApp::websocketStop()
{
    if (!flipperHttp)
    {
        FURI_LOG_E(TAG, "FreeRoamApp::websocketStop: FlipperHTTP is NULL");
        return false;
    }
    return flipper_http_websocket_stop(flipperHttp);
}

bool FreeRoamApp::websocketSend(const char *message)
{
    if (!flipperHttp || !message)
    {
        FURI_LOG_E(TAG, "FreeRoamApp::websocketSend: invalid arguments");
        return false;
    }
    return flipper_http_send_data(flipperHttp, message);
}

void FreeRoamApp::clearLastResponse()
{
    if (flipperHttp)
    {
        flipperHttp->last_response[0] = '\0';
    }
}

const char *FreeRoamApp::getLastResponse() const noexcept
{
    return flipperHttp ? flipperHttp->last_response : nullptr;
}

FreeRoamApp::FreeRoamApp()
{
    gui = static_cast<Gui *>(furi_record_open(RECORD_GUI));

    // Allocate ViewDispatcher
    if (!easy_flipper_set_view_dispatcher(&viewDispatcher, gui, this))
    {
        FURI_LOG_E(TAG, "Failed to allocate view dispatcher");
        return;
    }

    // Submenu
    if (!easy_flipper_set_submenu(&submenu, FreeRoamViewSubmenu,
                                  "Free Roam", callback_exit_app, &viewDispatcher))
    {
        FURI_LOG_E(TAG, "Failed to allocate submenu");
        return;
    }

    submenu_add_item(submenu, "Run", FreeRoamSubmenuRun, submenu_choices_callback, this);
    submenu_add_item(submenu, "About", FreeRoamSubmenuAbout, submenu_choices_callback, this);
    submenu_add_item(submenu, "Settings", FreeRoamSubmenuSettings, submenu_choices_callback, this);

    flipperHttp = flipper_http_alloc();
    if (!flipperHttp)
    {
        FURI_LOG_E(TAG, "Failed to allocate FlipperHTTP");
        return;
    }

    createAppDataPath("free_roam");
    createAppDataPath("flipper_http");

    // Switch to the submenu view
    view_dispatcher_switch_to_view(viewDispatcher, FreeRoamViewSubmenu);
}

FreeRoamApp::~FreeRoamApp()
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

    // Clean up game
    if (game)
    {
        game.reset();
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
        view_dispatcher_remove_view(viewDispatcher, FreeRoamViewSubmenu);
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

void FreeRoamApp::run()
{
    view_dispatcher_run(viewDispatcher);
}

extern "C"
{
    int32_t free_roam_main(void *p)
    {
        // Suppress unused parameter warning
        UNUSED(p);

        // Create the app
        FreeRoamApp app;

        // Run the app
        app.run();

        // return success
        return 0;
    }
}
