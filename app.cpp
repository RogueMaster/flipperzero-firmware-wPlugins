#include "app.hpp"
#include "lcd.hpp"
#include "http.hpp"

uint32_t GhoulsApp::callback_exit_app(void *context)
{
    UNUSED(context);
    return VIEW_NONE;
}

void GhoulsApp::settings_item_selected_callback(void *context, uint32_t index)
{
    GhoulsApp *app = (GhoulsApp *)context;
    app->settingsItemSelected(index);
}

void GhoulsApp::submenu_choices_callback(void *context, uint32_t index)
{
    GhoulsApp *app = (GhoulsApp *)context;
    app->callbackSubmenuChoices(index);
}

void GhoulsApp::settingsItemSelected(uint32_t index)
{
    if (settings)
    {
        settings->settingsItemSelected(index);
    }
}

void GhoulsApp::createAppDataPath(const char *appId)
{
    Storage *storage = static_cast<Storage *>(furi_record_open(RECORD_STORAGE));
    char directory_path[256];
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/%s", appId);
    storage_common_mkdir(storage, directory_path);
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/%s/data", appId);
    storage_common_mkdir(storage, directory_path);
    furi_record_close(RECORD_STORAGE);
}

void GhoulsApp::viewPortDraw(Canvas *canvas, void *context)
{
    GhoulsApp *app = static_cast<GhoulsApp *>(context);
    furi_check(app);
    auto game = app->game.get();
    if (game)
    {
        if (game->getDraw() == nullptr)
        {
            lcd_init_canvas(canvas);
            game->initDraw();
        }
        if (game->isActive())
        {
            game->updateDraw();
        }
    }
}
void GhoulsApp::viewPortInput(InputEvent *event, void *context)
{
    if (event->type != InputTypeShort && event->type != InputTypeLong && event->type != InputTypeRepeat)
    {
        return;
    }
    GhoulsApp *app = static_cast<GhoulsApp *>(context);
    furi_check(app);
    auto game = app->game.get();
    if (game && game->isActive())
    {
        game->updateInput(event->key, event->type == InputTypeLong || event->type == InputTypeRepeat);
    }
}

void GhoulsApp::timerCallback(void *context)
{
    GhoulsApp *app = static_cast<GhoulsApp *>(context);
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

            view_dispatcher_switch_to_view(app->viewDispatcher, GhoulsViewSubmenu);
            app->game.reset();
        }
    }
}

void GhoulsApp::callbackSubmenuChoices(uint32_t index)
{
    switch (index)
    {
    case GhoulsSubmenuRun:
    {
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

        if (!http_init(flipperHttp))
        {
            easy_flipper_dialog("HTTP Initialization Error", "Failed to initialize HTTP.");
            return;
        }

        char user_name[64] = {0};
        char user_pass[64] = {0};
        load_char("user_name", user_name, sizeof(user_name), "flipper_http");
        load_char("user_pass", user_pass, sizeof(user_pass), "flipper_http");

        game = std::make_unique<GhoulsGame>(user_name, user_pass, isSoundEnabled());
        if (!game)
        {
            easy_flipper_dialog("Game Initialization Error", "Failed to initialize the game.");
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
            furi_timer_start(timer, 16.67); // Update every 16.67ms (~60 FPS)
        }
        break;
    }
    case GhoulsSubmenuAbout:
        about = std::make_unique<GhoulsAbout>();
        if (!about->init(&viewDispatcher, this))
        {
            FURI_LOG_E(TAG, "Failed to initialize about");
            about.reset();
            return;
        }
        view_dispatcher_switch_to_view(viewDispatcher, GhoulsViewAbout);
        break;
    case GhoulsSubmenuSettings:
        settings = std::make_unique<GhoulsSettings>();
        if (!settings->init(&viewDispatcher, this))
        {
            FURI_LOG_E(TAG, "Failed to initialize settings");
            settings.reset();
            return;
        }
        view_dispatcher_switch_to_view(viewDispatcher, GhoulsViewSettings);
        break;
    default:
        break;
    }
}

bool GhoulsApp::hasWiFiCredentials()
{
    char ssid[64] = {0};
    char password[64] = {0};
    return load_char("wifi_ssid", ssid, sizeof(ssid), "flipper_http") &&
           load_char("wifi_pass", password, sizeof(password), "flipper_http") &&
           strlen(ssid) > 0 &&
           strlen(password) > 0;
}

bool GhoulsApp::hasUserCredentials()
{
    char username[64] = {0};
    char password[64] = {0};
    return load_char("user_name", username, sizeof(username), "flipper_http") &&
           load_char("user_pass", password, sizeof(password), "flipper_http") &&
           strlen(username) > 0 &&
           strlen(password) > 0;
}

bool GhoulsApp::isBoardConnected()
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

bool GhoulsApp::save_char(const char *path_name, const char *value, const char *appId)
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

bool GhoulsApp::load_char(const char *path_name, char *value, size_t value_size, const char *appId)
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

bool GhoulsApp::isVibrationEnabled()
{
    char value[16] = {0};
    if (load_char("vibration", value, sizeof(value)))
    {
        return strcmp(value, "On") == 0;
    }
    return true;
}

bool GhoulsApp::isSoundEnabled()
{
    char value[16] = {0};
    if (load_char("sound", value, sizeof(value)))
    {
        return strcmp(value, "On") == 0;
    }
    return true;
}

void GhoulsApp::setVibrationEnabled(bool enabled)
{
    save_char("vibration", enabled ? "On" : "Off");
}

void GhoulsApp::setSoundEnabled(bool enabled)
{
    save_char("sound", enabled ? "On" : "Off");
}

bool GhoulsApp::httpRequestAsync(
    const char *saveLocation,
    const char *url,
    HTTPMethod method,
    const char *headers,
    const char *payload)
{
    if (!flipperHttp)
    {
        FURI_LOG_E(TAG, "GhoulsApp::httpRequestAsync: FlipperHTTP is NULL");
        return false;
    }
    snprintf(flipperHttp->file_path, sizeof(flipperHttp->file_path), "%s/%s", STORAGE_EXT_PATH_PREFIX "/apps_data/ghouls/data", saveLocation);
    flipperHttp->save_received_data = true;
    flipperHttp->state = IDLE;
    if (!flipper_http_request(flipperHttp, method, url, headers, payload))
    {
        FURI_LOG_E(TAG, "GhoulsApp::httpRequestAsync: Failed to send HTTP request");
        return false;
    }
    flipperHttp->state = RECEIVING;
    return true;
}

bool GhoulsApp::sendWiFiCredentials(const char *ssid, const char *password)
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

bool GhoulsApp::websocketStart(const char *url, uint16_t port)
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

bool GhoulsApp::websocketStop()
{
    if (!flipperHttp)
    {
        FURI_LOG_E(TAG, "GhoulsApp::websocketStop: FlipperHTTP is NULL");
        return false;
    }
    return flipper_http_websocket_stop(flipperHttp);
}

bool GhoulsApp::websocketSend(const char *message)
{
    if (!flipperHttp || !message)
    {
        FURI_LOG_E(TAG, "GhoulsApp::websocketSend: invalid arguments");
        return false;
    }
    return flipper_http_send_data(flipperHttp, message);
}

void GhoulsApp::clearLastResponse()
{
    if (flipperHttp)
    {
        flipperHttp->last_response[0] = '\0';
    }
}

const char *GhoulsApp::getLastResponse() const noexcept
{
    return flipperHttp ? flipperHttp->last_response : nullptr;
}

GhoulsApp::GhoulsApp()
{
    gui = static_cast<Gui *>(furi_record_open(RECORD_GUI));

    // Allocate ViewDispatcher
    if (!easy_flipper_set_view_dispatcher(&viewDispatcher, gui, this))
    {
        FURI_LOG_E(TAG, "Failed to allocate view dispatcher");
        return;
    }

    // Submenu
    if (!easy_flipper_set_submenu(&submenu, GhoulsViewSubmenu,
                                  "Ghouls", callback_exit_app, &viewDispatcher))
    {
        FURI_LOG_E(TAG, "Failed to allocate submenu");
        return;
    }

    submenu_add_item(submenu, "Run", GhoulsSubmenuRun, submenu_choices_callback, this);
    submenu_add_item(submenu, "About", GhoulsSubmenuAbout, submenu_choices_callback, this);
    submenu_add_item(submenu, "Settings", GhoulsSubmenuSettings, submenu_choices_callback, this);

    flipperHttp = flipper_http_alloc();
    if (!flipperHttp)
    {
        FURI_LOG_E(TAG, "Failed to allocate FlipperHTTP");
        return;
    }

    createAppDataPath("ghouls");
    createAppDataPath("flipper_http");

    // Switch to the submenu view
    view_dispatcher_switch_to_view(viewDispatcher, GhoulsViewSubmenu);
}

GhoulsApp::~GhoulsApp()
{
    lcd_deinit();

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
        game = nullptr;
    }

    // Clean up settings
    if (settings)
    {
        settings.reset();
        settings = nullptr;
    }

    // Clean up about
    if (about)
    {
        about.reset();
        about = nullptr;
    }

    // Free submenu
    if (submenu)
    {
        view_dispatcher_remove_view(viewDispatcher, GhoulsViewSubmenu);
        submenu_free(submenu);
        submenu = nullptr;
    }

    // Free view dispatcher
    if (viewDispatcher)
    {
        view_dispatcher_free(viewDispatcher);
        viewDispatcher = nullptr;
    }

    // Close GUI
    if (gui)
    {
        furi_record_close(RECORD_GUI);
        gui = nullptr;
    }

    // Free FlipperHTTP
    if (flipperHttp)
    {
        flipper_http_free(flipperHttp);
        flipperHttp = nullptr;
    }
}

void GhoulsApp::run()
{
    view_dispatcher_run(viewDispatcher);
}

extern "C"
{
    int32_t ghouls_main(void *p)
    {
        // Suppress unused parameter warning
        UNUSED(p);

        // Create the app
        GhoulsApp app;

        // Run the app
        app.run();

        // return success
        return 0;
    }
}
