#pragma once

#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_input.h>
#include <furi.h>
#include <string>
#include <memory>
#include "font/font.h"
#include "easy_flipper/easy_flipper.h"
#include "flipper_http/flipper_http.h"
#include "settings/settings.hpp"
#include "about/about.hpp"
#include "ghouls/src/game.hpp"

#define TAG         "Ghouls"
#define VERSION     FAP_VERSION
#define VERSION_TAG TAG " " VERSION
#define APP_ID      "ghouls"

typedef enum {
    GhoulsSubmenuRun = 0,
    GhoulsSubmenuAbout = 1,
    GhoulsSubmenuSettings = 2,
} GhoulsSubmenuIndex;

typedef enum {
    GhoulsViewMain = 0,
    GhoulsViewSubmenu = 1,
    GhoulsViewAbout = 2,
    GhoulsViewSettings = 3,
    GhoulsViewTextInput = 4,
} GhoulsView;

class GhoulsApp {
private:
    Submenu* submenu = nullptr;
    FlipperHTTP* flipperHttp = nullptr;

    // Settings class instance
    std::unique_ptr<GhoulsSettings> settings;

    // About class instance
    std::unique_ptr<GhoulsAbout> about;

    // Timer for game updates
    FuriTimer* timer = nullptr;

    // Static callback functions
    static uint32_t callback_exit_app(void* context);
    static void submenu_choices_callback(void* context, uint32_t index);
    static void settings_item_selected_callback(void* context, uint32_t index);
    static void timerCallback(void* context);

    // Member functions
    void callbackSubmenuChoices(uint32_t index);
    void settingsItemSelected(uint32_t index);

    void createAppDataPath(const char* appId = "ghouls");

public:
    bool load_char(
        const char* path_name,
        char* value,
        size_t value_size,
        const char* appId = "ghouls"); // load a string from storage
    bool save_char(
        const char* path_name,
        const char* value,
        const char* appId = "ghouls"); // save a string to storage
    ViewDispatcher* viewDispatcher = nullptr;
    Gui* gui = nullptr;
    ViewPort* viewPort = nullptr;
    std::unique_ptr<GhoulsGame> game;
    //
    bool isVibrationEnabled(); // check if vibration is enabled
    bool isBoardConnected(); // check if the board is connected
    bool isSoundEnabled(); // check if sound is enabled
    void setVibrationEnabled(bool enabled); // set vibration enabled/disabled
    void setSoundEnabled(bool enabled); // set sound enabled/disabled

    HTTPState getHttpState() const noexcept {
        return flipperHttp ? flipperHttp->state : INACTIVE;
    }
    bool hasWiFiCredentials();
    bool hasUserCredentials();

    bool httpRequestAsync(
        const char* saveLocation,
        const char* url,
        HTTPMethod method = GET,
        const char* headers = "{\"Content-Type\": \"application/json\"}",
        const char* payload = nullptr);

    bool sendWiFiCredentials(
        const char* ssid,
        const char* password); // send WiFi credentials to the board

    bool websocketStart(const char* url, uint16_t port = 80); // start a WebSocket connection
    bool websocketStop(); // stop the active WebSocket connection
    bool websocketSend(const char* message); // send a message over the open WebSocket
    void clearLastResponse(); // clear the most recent data received over UART
    const char* getLastResponse() const noexcept; // get the most recent data received over UART

    GhoulsApp();
    ~GhoulsApp();

    void run();

    static void viewPortDraw(Canvas* canvas, void* context);
    static void viewPortInput(InputEvent* event, void* context);
};
