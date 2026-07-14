#ifndef APP_H
#define APP_H

#include "./structs.h" // Structs
#include "./uart/uart.h" // Uart Communications

//SCENE MANAGER and VIEW
#include <gui/scene_manager.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>

// SCENES
#include <gui/modules/dialog_ex.h> // Setup dialog
#include <gui/modules/menu.h> // Main menu
#include <gui/modules/submenu.h> // Subemenus, Connect, Connect_Favs, Connect_Details
#include <gui/modules/text_box.h> // Text box for displaying Uart responses
#include <gui/modules/variable_item_list.h> // Variable item list for GET view
#include "./modules/uart_text_input/uart_text_input.h" // Custom text input for passwords and urls
#include <gui/modules/widget.h> // As TEXT BOX
// CSV STORAGE
#include <storage/storage.h>

// TAG for logging
#define TAG "FLIPPER_POSTMAN"

typedef struct {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Menu* menu; // Main menu
    Submenu* submenu;

    // START UP SCENE
    FuriTimer* timer;
    DialogEx* dialog; // Setup dialog
    SetupDialogState dialog_state; // State for setup dialog

    // UART COMMUNICATIONS
    Uart* uart; // Uart communications
    uint8_t uart_ch; // Uart channel (USART1)
    UartStatus status; // Uart status

    // TEXT INPUT
    UART_TextInput* text_input; // Custom text input for passwords and urls
    char text_input_store[UART_TERMINAL_TEXT_INPUT_STORE_SIZE + 1]; // Store for text input
    bool is_custom_tx_string; // Flag for custom text input
    const char* selected_tx_string; // Selected text input string
    TextInputState text_input_state; // Current text input state

    // Files and Storage
    File* file; // CSV file ops
    Storage* storage; // Storage for CSV files

    // WIFI lists
    AvailableWifiList wifi_list; // This holds the board wifi scan results
    WifiCredential* csv_networks; // List of wifi networks from csv
    size_t csv_networks_count; // Number of wifi networks from csv

    // GET VIEW
    VariableItemList* variable_item_list; // Variable item list for get view
    GetState* get_state; // Get state for get view
    UrlList* url_list; // Dynamic list of get urls
    size_t url_list_count; // List of get urls count

    // POST VIEW
    PostState* post_state; // Post state for get view
    PostUrlList* post_url_list;
    size_t post_url_list_count; // List of post urls count

    // Save to file global flag for uart responses
    bool save_to_file; // Flag for saving uart responses to file
    char filename[KEY_NAME_SIZE]; // Filename for saving uart responses
    DownloadMode download_mode; // Modes for download, get, post, custom

    // DISPLAY VIEW
    Widget* text_box; // Text box for displaying uart responses
    FuriString* text_box_store; // Store for displaying uart responses
    bool full_response; // Flag for attaching full uart response otherwise last line
    DisplayMode display_mode; // Display mode for display view

    // Download view
    View* view; // View for download progress

    //Build Http View
    BuildHttpState* build_http_state; // Build http (custom config) call
    BuildHttpList* build_http_list; // Build http csv store
    size_t build_http_list_size; // Number of items in the build_http_list
} App;

#endif // APP_H
