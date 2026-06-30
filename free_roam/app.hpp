#pragma once

#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_input.h>
#include <furi.h>
#include <memory>
#include <string>
#include "font/font.h"
#include "easy_flipper/easy_flipper.h"
#include "flipper_http/flipper_http.h"
#include "settings/settings.hpp"
#include "about/about.hpp"
#include "game/game.hpp"

#define TAG "Free Roam"
#define VERSION "0.6"
#define VERSION_TAG TAG " " VERSION
#define APP_ID "free_roam"

typedef enum
{
    FreeRoamSubmenuRun = 0,
    FreeRoamSubmenuAbout = 1,
    FreeRoamSubmenuSettings = 2,
} FreeRoamSubmenuIndex;

typedef enum
{
    FreeRoamViewMain = 0,
    FreeRoamViewSubmenu = 1,
    FreeRoamViewAbout = 2,
    FreeRoamViewSettings = 3,
    FreeRoamViewTextInput = 4,
} FreeRoamView;

class FreeRoamApp
{
private:
    Submenu *submenu = nullptr;
    FlipperHTTP *flipperHttp = nullptr;

    // Settings class instance
    std::unique_ptr<FreeRoamSettings> settings;

    // About class instance
    std::unique_ptr<FreeRoamAbout> about;

    // Timer for game updates
    FuriTimer *timer = nullptr;

    // Static callback functions
    static uint32_t callback_exit_app(void *context);
    static void submenu_choices_callback(void *context, uint32_t index);
    static void settings_item_selected_callback(void *context, uint32_t index);
    static void timerCallback(void *context);

    // Member functions
    void callbackSubmenuChoices(uint32_t index);
    void settingsItemSelected(uint32_t index);

    void createAppDataPath(const char *appId = "free_roam");

public:
    bool load_char(const char *path_name, char *value, size_t value_size, const char *appId = "free_roam"); // load a string from storage
    bool save_char(const char *path_name, const char *value, const char *appId = "free_roam");              // save a string to storage
    ViewDispatcher *viewDispatcher = nullptr;
    Gui *gui = nullptr;
    ViewPort *viewPort = nullptr;
    std::unique_ptr<FreeRoamGame> game;
    //
    bool isVibrationEnabled();              // check if vibration is enabled
    bool isBoardConnected();                // check if the board is connected
    bool isSoundEnabled();                  // check if sound is enabled
    void setVibrationEnabled(bool enabled); // set vibration enabled/disabled
    void setSoundEnabled(bool enabled);     // set sound enabled/disabled

    HTTPState getHttpState() const noexcept { return flipperHttp ? flipperHttp->state : INACTIVE; }
    bool hasWiFiCredentials();
    bool hasUserCredentials();
    bool setHttpState(HTTPState state = IDLE) noexcept;

    bool httpRequestAsync(
        const char *saveLocation,
        const char *url,
        HTTPMethod method = GET,
        const char *headers = "{\"Content-Type\": \"application/json\"}",
        const char *payload = nullptr);

    bool sendWiFiCredentials(const char *ssid, const char *password); // send WiFi credentials to the board

    bool websocketStart(const char *url, uint16_t port = 80); // start a WebSocket connection
    bool websocketStop();                                     // stop the active WebSocket connection
    bool websocketSend(const char *message);                  // send a message over the open WebSocket
    void clearLastResponse();                                 // clear the most recent data received over UART
    const char *getLastResponse() const noexcept;             // get the most recent data received over UART

    FreeRoamApp();
    ~FreeRoamApp();

    void run();

    static void viewPortDraw(Canvas *canvas, void *context);
    static void viewPortInput(InputEvent *event, void *context);
};