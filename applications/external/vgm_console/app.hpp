#pragma once
#include "flipper_http/flipper_http.h"
#include "run/run.hpp"
#include "about/about.hpp"
#include <gui/modules/submenu.h>
#include <gui/view_dispatcher.h>
#include <gui/view_port.h>
#include <gui/canvas.h>
#include <gui/gui.h>

#define TAG "VGM Console"
#define VERSION "1.0"
#define VERSION_TAG TAG " " VERSION
#define APP_ID "video_game_module_console"
#define APP_EVENT_RUN_FINISHED (100u)

typedef enum
{
    VideoGameModuleConsoleSubmenuRun = 0,
    VideoGameModuleConsoleSubmenuAbout = 1,
    VideoGameModuleConsoleSubmenuSettings = 2,
} VideoGameModuleConsoleSubmenuIndex;

typedef enum
{
    VideoGameModuleConsoleViewMain = 0,
    VideoGameModuleConsoleViewSubmenu = 1,
    VideoGameModuleConsoleViewAbout = 2,
    VideoGameModuleConsoleViewSettings = 3,
    VideoGameModuleConsoleViewTextInput = 4,
} VideoGameModuleConsoleView;

class VideoGameModuleConsoleApp
{
private:
    std::unique_ptr<VideoGameModuleConsoleAbout> about; // About class instance
    FlipperHTTP *flipperHttp = nullptr;                 // FlipperHTTP instance for HTTP requests
    FuriMessageQueue *inputQueue = nullptr;             // Queue of InputEvent for the run thread
    FuriMessageQueue *responseQueue = nullptr;          // Queue of char* draw-command lines (one per UART line)
    FuriThread *runThread = nullptr;                    // Dedicated thread for the run loop
    std::unique_ptr<VideoGameModuleConsoleRun> run;     // Run class instance
    Submenu *submenu = nullptr;                         // Submenu for the app
    //
    static uint32_t callbackExitApp(void *context);                     // Callback to exit the app
    void callbackSubmenuChoices(uint32_t index);                        // Callback for submenu choices
    static bool customEventCallback(void *context, uint32_t event);     // View dispatcher custom event callback
    void createAppDataPath(const char *appId = APP_ID);                 // Create the app data path in storage
    static void httpLineCallback(const char *line, void *context);      // Per-line UART callback; enqueues draw commands
    static int32_t runThreadCallback(void *context);                    // Run thread: draws frames and processes input
    static void submenuChoicesCallback(void *context, uint32_t index);  // Callback for submenu choices
    static void viewPortInputEnqueue(InputEvent *event, void *context); // Enqueues input events for the run thread

public:
    VideoGameModuleConsoleApp();
    ~VideoGameModuleConsoleApp();
    //
    Canvas *canvas = nullptr;                                                                                   // Direct canvas handle (held only while run is active)
    Gui *gui = nullptr;                                                                                         // GUI instance for the app
    ViewDispatcher *viewDispatcher = nullptr;                                                                   // ViewDispatcher for managing views
    ViewPort *viewPort = nullptr;                                                                               // ViewPort in GuiLayerFullscreen to capture input during run (blocks view dispatcher)
                                                                                                                //
    void clearHttpBuffer();                                                                                     // clear the HTTP buffer (for bytes requests)
    void clearHttpResponse();                                                                                   // clear the last HTTP response
    bool getHttpBuffer(uint8_t *buffer, size_t buffer_size);                                                    // get the buffer of the last response (for bytes requests)
    const char *getHttpResponse() const noexcept { return flipperHttp ? flipperHttp->last_response : nullptr; } // get the last HTTP response (single-shot; may be stale)
    char *popHttpResponse();                                                                                    // pop one queued UART line (caller must free); returns nullptr if queue empty
    bool loadChar(const char *path_name, char *value, size_t value_size, const char *appId = APP_ID);           // load a string from storage
    void runDispatcher();                                                                                       // run the app's view dispatcher to handle views and events
    bool saveChar(const char *path_name, const char *value, const char *appId = APP_ID, bool overwrite = true); // save a string to storage
    bool sendData(const char *data);                                                                            // send data to the board
};