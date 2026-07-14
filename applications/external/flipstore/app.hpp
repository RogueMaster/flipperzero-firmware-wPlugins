#pragma once
#include "font/font.h"
#include "easy_flipper/easy_flipper.h"
#include "flipper_http/flipper_http.h"
#include "run/run.hpp"
#include "settings/settings.hpp"
#include "about/about.hpp"

#define TAG         "FlipDownloader"
#define VERSION     "1.3.5"
#define VERSION_TAG TAG " " VERSION
#define APP_ID      "flip_downloader"

typedef enum {
    FlipDownloaderSubmenuRun = 0,
    FlipDownloaderSubmenuAbout = 1,
    FlipDownloaderSubmenuSettings = 2,
} FlipDownloaderSubmenuIndex;

typedef enum {
    FlipDownloaderViewMain = 0,
    FlipDownloaderViewSubmenu = 1,
    FlipDownloaderViewAbout = 2,
    FlipDownloaderViewSettings = 3,
    FlipDownloaderViewTextInput = 4,
} FlipDownloaderView;

class FlipDownloaderApp {
private:
    std::unique_ptr<FlipDownloaderAbout> about; // About class instance
    FlipperHTTP* flipperHttp = nullptr; // FlipperHTTP instance for network requests
    std::unique_ptr<FlipDownloaderRun> run; // Run class instance
    std::unique_ptr<FlipDownloaderSettings> settings; // Settings class instance
    Submenu* submenu = nullptr; // Submenu instance for navigation
    FuriTimer* timer = nullptr; // Timer for run updates

    static uint32_t callbackExitApp(void* context);
    void callbackSubmenuChoices(uint32_t index);
    static uint32_t callbackToSubmenu(void* context);
    void createAppDataPath(const char* appId = APP_ID);
    void settingsItemSelected(uint32_t index);
    static void settings_item_selected_callback(void* context, uint32_t index);
    static void submenu_choices_callback(void* context, uint32_t index);
    static void timerCallback(void* context);

public:
    FlipDownloaderApp();
    ~FlipDownloaderApp();
    //
    Gui* gui = nullptr;
    ViewDispatcher* viewDispatcher = nullptr;
    ViewPort* viewPort = nullptr;
    //
    void freeRunView(); // free the Run View
    size_t getBytesReceived() const noexcept {
        return flipperHttp ? flipperHttp->bytes_received : 0;
    }
    size_t getContentLength() const noexcept {
        return flipperHttp ? flipperHttp->content_length : 0;
    }
    HTTPState getHttpState() const noexcept {
        return flipperHttp ? flipperHttp->state : INACTIVE;
    }
    bool hasWiFiCredentials(); // check if WiFi credentials are set
    bool initRunView(); // initialize the Run View
    bool isBoardConnected(); // check if the board is connected
    bool load_char(
        const char* path_name,
        char* value,
        size_t value_size,
        const char* appId = APP_ID); // load a string from storage
    void runDispatcher();
    bool save_char(
        const char* path_name,
        const char* value,
        const char* appId = APP_ID); // save a string to storage
    bool sendWiFiCredentials(
        const char* ssid,
        const char* password); // send WiFi credentials to the board
    bool setHttpState(HTTPState state = IDLE) noexcept {
        if(flipperHttp) {
            flipperHttp->state = state;
            return true;
        }
        return false;
    }
    static void viewPortDraw(Canvas* canvas, void* context);
    static void viewPortInput(InputEvent* event, void* context);
    void updateApp(); // check for app update

    bool httpDownloadFile(
        const char* saveLocation, // full path where the file will be saved
        const char* url // URL to download the file from
    );

    FuriString* httpRequest(
        const char* url,
        HTTPMethod method = GET,
        const char* headers = "{\"Content-Type\": \"application/json\"}",
        const char* payload = nullptr);

    // for this one, check the HttpState to see if the request is finished
    // and then check the location
    // I think I'll add the loading view/animation to the one above
    // so this one is just like we used in the old apps
    // saveLocation is just a filename (like "response.json" or "data.json")
    bool httpRequestAsync(
        const char* saveLocation,
        const char* url,
        HTTPMethod method = GET,
        const char* headers = "{\"Content-Type\": \"application/json\"}",
        const char* payload = nullptr);
};
