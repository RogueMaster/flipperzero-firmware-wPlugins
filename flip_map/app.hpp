#pragma once
#include "font/font.h"
#include "easy_flipper/easy_flipper.h"
#include "flipper_http/flipper_http.h"
#include "run/run.hpp"
#include "settings/settings.hpp"
#include "about/about.hpp"

#define TAG "FlipMap"
#define VERSION "0.2.2"
#define VERSION_TAG TAG " " VERSION
#define APP_ID "flip_map"

typedef enum
{
    FlipMapSubmenuRun = 0,
    FlipMapSubmenuAbout = 1,
    FlipMapSubmenuSettings = 2,
} FlipMapSubmenuIndex;

typedef enum
{
    FlipMapViewMain = 0,
    FlipMapViewSubmenu = 1,
    FlipMapViewAbout = 2,
    FlipMapViewSettings = 3,
    FlipMapViewTextInput = 4,
} FlipMapView;

class FlipMapApp
{
private:
    std::unique_ptr<FlipMapAbout> about;       // About class instance
    FlipperHTTP *flipperHttp = nullptr;        // FlipperHTTP instance for HTTP requests
    std::unique_ptr<FlipMapRun> run;           // Run class instance
    std::unique_ptr<FlipMapSettings> settings; // Settings class instance
    Submenu *submenu = nullptr;                // Submenu for the app
    FuriTimer *timer = nullptr;                // Timer for run updates
    //
    static uint32_t callbackExitApp(void *context);                    // Callback to exit the app
    void callbackSubmenuChoices(uint32_t index);                       // Callback for submenu choices
    void createAppDataPath(const char *appId = APP_ID);                // Create the app data path in storage
    void settingsItemSelected(uint32_t index);                         // Handle settings item selection
    static void submenuChoicesCallback(void *context, uint32_t index); // Callback for submenu choices
    static void timerCallback(void *context);                          // Timer callback for run updates

public:
    FlipMapApp();
    ~FlipMapApp();
    //
    Gui *gui = nullptr;                       // GUI instance for the app
    ViewDispatcher *viewDispatcher = nullptr; // ViewDispatcher for managing views
    ViewPort *viewPort = nullptr;             // ViewPort for drawing and input handling (run instance)
    //
    size_t getBytesReceived() const noexcept { return flipperHttp ? flipperHttp->bytes_received : 0; } // get the number of bytes received
    size_t getContentLength() const noexcept { return flipperHttp ? flipperHttp->content_length : 0; } // get the content length of the last response
    HTTPState getHttpState() const noexcept { return flipperHttp ? flipperHttp->state : INACTIVE; }    // get the HTTP state
    bool hasWiFiCredentials();                                                                         // check if WiFi credentials are set
    bool hasUserCredentials();                                                                         // check if user credentials are set
    FuriString *httpRequest(                                                                           // synchronous HTTP request
        const char *url,                                                                               // URL to send the request to
        HTTPMethod method = GET,                                                                       // HTTP method to use (GET, POST, etc.)
        const char *headers = "{\"Content-Type\": \"application/json\"}",                              // Headers to include in the request
        const char *payload = nullptr);                                                                // Payload to send with the request (for POST, PUT, etc.)
    bool httpRequestAsync(                                                                             // asynchronous HTTP request (check the HttpState to see if the request is finished)
        const char *saveLocation,                                                                      // location to save the response (filename)
        const char *url,                                                                               // URL to send the request to
        HTTPMethod method = GET,                                                                       // HTTP method to use (GET, POST, etc.)
        const char *headers = "{\"Content-Type\": \"application/json\"}",                              // Headers to include in the request
        const char *payload = nullptr);                                                                // Payload to send with the request (for POST, PUT, etc.)
    bool isBoardConnected();                                                                           // check if the board is connected
    bool loadChar(const char *path_name, char *value, size_t value_size, const char *appId = APP_ID);  // load a string from storage
    bool loadFileChunk(const char *filePath, char *buffer, size_t sizeOfChunk, uint8_t iteration);     // Load a file chunk from storage
    void runDispatcher();                                                                              // run the app's view dispatcher to handle views and events
    bool saveChar(const char *path_name, const char *value, const char *appId = APP_ID);               // save a string to storage
    bool setHttpState(HTTPState state = IDLE) noexcept;                                                // set the HTTP state
    bool sendWiFiCredentials(const char *ssid, const char *password);                                  // send WiFi credentials to the board
    static void viewPortDraw(Canvas *canvas, void *context);                                           // draw callback for the ViewPort (used in run instance)
    static void viewPortInput(InputEvent *event, void *context);                                       // input callback for the ViewPort (used in run instance)
};