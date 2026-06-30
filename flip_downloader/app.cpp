#include "app.hpp"
#include <update/update.h>

void FlipDownloaderApp::callbackSubmenuChoices(uint32_t index)
{
    switch (index)
    {
    case FlipDownloaderSubmenuRun:
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
        run = std::make_unique<FlipDownloaderRun>();
        if (!run->init(this))
        {
            FURI_LOG_E(TAG, "Failed to initialize run");
            run.reset();
            return;
        }
        freeRunView(); // Free any previous run view resources
        if (!initRunView())
        {
            FURI_LOG_E(TAG, "Failed to initialize run view");
            run.reset();
            return;
        }
        break;
    case FlipDownloaderSubmenuAbout:
        about = std::make_unique<FlipDownloaderAbout>();
        if (!about->init(&viewDispatcher, this))
        {
            FURI_LOG_E(TAG, "Failed to initialize about");
            about.reset();
            return;
        }
        view_dispatcher_switch_to_view(viewDispatcher, FlipDownloaderViewAbout);
        break;
    case FlipDownloaderSubmenuSettings:
        settings = std::make_unique<FlipDownloaderSettings>();
        if (!settings->init(&viewDispatcher, this))
        {
            FURI_LOG_E(TAG, "Failed to initialize settings");
            settings.reset();
            return;
        }
        view_dispatcher_switch_to_view(viewDispatcher, FlipDownloaderViewSettings);
        break;
    default:
        break;
    }
}

uint32_t FlipDownloaderApp::callbackExitApp(void *context)
{
    UNUSED(context);
    return VIEW_NONE;
}

void FlipDownloaderApp::createAppDataPath(const char *appId)
{
    Storage *storage = static_cast<Storage *>(furi_record_open(RECORD_STORAGE));
    char directory_path[256];
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/%s", appId);
    storage_common_mkdir(storage, directory_path);
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/%s/data", appId);
    storage_common_mkdir(storage, directory_path);
    furi_record_close(RECORD_STORAGE);
}

void FlipDownloaderApp::freeRunView()
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
}

bool FlipDownloaderApp::httpDownloadFile(
    const char *saveLocation, // full path where the file will be saved
    const char *url           // URL to download the file from
)
{
    if (!flipperHttp)
    {
        FURI_LOG_E(TAG, "FlipDownloaderApp::httpDownloadFile: FlipperHTTP is NULL");
        return false;
    }
    snprintf(flipperHttp->file_path, sizeof(flipperHttp->file_path), saveLocation);
    flipperHttp->save_received_data = false;
    flipperHttp->is_bytes_request = true;
    flipperHttp->state = IDLE;
    return flipper_http_request(flipperHttp, BYTES, url, "{\"Content-Type\": \"application/octet-stream\"}", NULL);
}

FuriString *FlipDownloaderApp::httpRequest(
    const char *url,
    HTTPMethod method,
    const char *headers,
    const char *payload)
{
    if (!flipperHttp)
    {
        FURI_LOG_E(TAG, "FlipDownloaderApp::httpRequest: FlipperHTTP is NULL");
        return NULL;
    }
    snprintf(flipperHttp->file_path, sizeof(flipperHttp->file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/%s/data/temp.json", APP_ID);
    flipperHttp->save_received_data = true;
    flipperHttp->state = IDLE;
    if (!flipper_http_request(flipperHttp, method, url, headers, payload))
    {
        FURI_LOG_E(TAG, "FlipDownloaderApp::httpRequest: Failed to send HTTP request");
        return NULL;
    }
    flipperHttp->state = RECEIVING;
    while (flipperHttp->state != IDLE)
    {
        furi_delay_ms(100);
    }
    return flipper_http_load_from_file(flipperHttp->file_path);
}

bool FlipDownloaderApp::httpRequestAsync(
    const char *saveLocation,
    const char *url,
    HTTPMethod method,
    const char *headers,
    const char *payload)
{
    if (!flipperHttp)
    {
        FURI_LOG_E(TAG, "FlipDownloaderApp::httpRequestAsync: FlipperHTTP is NULL");
        return false;
    }
    snprintf(flipperHttp->file_path, sizeof(flipperHttp->file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/%s/data/%s", APP_ID, saveLocation);
    flipperHttp->save_received_data = true;
    flipperHttp->state = IDLE;
    if (!flipper_http_request(flipperHttp, method, url, headers, payload))
    {
        FURI_LOG_E(TAG, "FlipDownloaderApp::httpRequestAsync: Failed to send HTTP request");
        return false;
    }
    flipperHttp->state = RECEIVING;
    return true;
}

bool FlipDownloaderApp::hasWiFiCredentials()
{
    char ssid[64] = {0};
    char password[64] = {0};
    return load_char("wifi_ssid", ssid, sizeof(ssid), "flipper_http") &&
           load_char("wifi_pass", password, sizeof(password), "flipper_http") &&
           strlen(ssid) > 0 &&
           strlen(password) > 0;
}

bool FlipDownloaderApp::initRunView()
{
    if (!gui)
    {
        FURI_LOG_E(TAG, "GUI is not initialized");
        return false;
    }
    if (viewPort)
    {
        FURI_LOG_E(TAG, "ViewPort is already initialized");
        return false;
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
    return true;
}

bool FlipDownloaderApp::isBoardConnected()
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

bool FlipDownloaderApp::load_char(const char *path_name, char *value, size_t value_size, const char *appId)
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

bool FlipDownloaderApp::save_char(const char *path_name, const char *value, const char *appId)
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

bool FlipDownloaderApp::sendWiFiCredentials(const char *ssid, const char *password)
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

void FlipDownloaderApp::settings_item_selected_callback(void *context, uint32_t index)
{
    FlipDownloaderApp *app = (FlipDownloaderApp *)context;
    app->settingsItemSelected(index);
}

void FlipDownloaderApp::submenu_choices_callback(void *context, uint32_t index)
{
    FlipDownloaderApp *app = (FlipDownloaderApp *)context;
    app->callbackSubmenuChoices(index);
}

void FlipDownloaderApp::timerCallback(void *context)
{
    FlipDownloaderApp *app = static_cast<FlipDownloaderApp *>(context);
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
        }
    }
}

void FlipDownloaderApp::settingsItemSelected(uint32_t index)
{
    if (settings)
    {
        settings->settingsItemSelected(index);
    }
}

void FlipDownloaderApp::viewPortDraw(Canvas *canvas, void *context)
{
    FlipDownloaderApp *app = static_cast<FlipDownloaderApp *>(context);
    furi_check(app);
    auto run = app->run.get();
    if (run)
    {
        if (run->isActive())
        {
            run->updateDraw(canvas);
        }
    }
}
void FlipDownloaderApp::viewPortInput(InputEvent *event, void *context)
{
    FlipDownloaderApp *app = static_cast<FlipDownloaderApp *>(context);
    furi_check(app);
    auto run = app->run.get();
    if (run && run->isActive())
    {
        run->updateInput(event);
    }
}

FlipDownloaderApp::FlipDownloaderApp()
{
    gui = static_cast<Gui *>(furi_record_open(RECORD_GUI));

    // Allocate ViewDispatcher
    if (!easy_flipper_set_view_dispatcher(&viewDispatcher, gui, this))
    {
        FURI_LOG_E(TAG, "Failed to allocate view dispatcher");
        return;
    }

    // Submenu
    if (!easy_flipper_set_submenu(&submenu, FlipDownloaderViewSubmenu, VERSION_TAG, callbackExitApp, &viewDispatcher))
    {
        FURI_LOG_E(TAG, "Failed to allocate submenu");
        return;
    }

    submenu_add_item(submenu, "Browse", FlipDownloaderSubmenuRun, submenu_choices_callback, this);
    submenu_add_item(submenu, "About", FlipDownloaderSubmenuAbout, submenu_choices_callback, this);
    submenu_add_item(submenu, "Settings", FlipDownloaderSubmenuSettings, submenu_choices_callback, this);

    flipperHttp = flipper_http_alloc();
    if (!flipperHttp)
    {
        FURI_LOG_E(TAG, "Failed to allocate FlipperHTTP");
        return;
    }

    createAppDataPath();

    // Switch to the submenu view
    view_dispatcher_switch_to_view(viewDispatcher, FlipDownloaderViewSubmenu);
}

FlipDownloaderApp::~FlipDownloaderApp()
{
    freeRunView();

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
        view_dispatcher_remove_view(viewDispatcher, FlipDownloaderViewSubmenu);
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

void FlipDownloaderApp::runDispatcher()
{
    view_dispatcher_run(viewDispatcher);
}

void FlipDownloaderApp::updateApp()
{
    if (flipperHttp && isBoardConnected() && hasWiFiCredentials())
    {
        if (update_is_ready(flipperHttp, true))
        {
            easy_flipper_dialog("Update Status", "Complete.\nRestart your Flipper Zero.");
        }
    }
}

extern "C"
{
    int32_t main_flip_downloader(void *p)
    {
        // Suppress unused parameter warning
        UNUSED(p);

        // Create the app
        FlipDownloaderApp app;

        app.save_char("app_version", VERSION);

        // check if update is available from lab.flipper.net
        app.updateApp();

        // Run the app
        app.runDispatcher();

        // return success
        return 0;
    }
}
