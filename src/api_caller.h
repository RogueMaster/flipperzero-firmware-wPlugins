#pragma once

#include <gui/gui.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>

#include "api/flipper_http.h"
#include "utils/locale.h"
#include "utils/long_text_view.h"

typedef enum {
    ApiCallerSceneMain,
    ApiCallerSceneWifi,
    ApiCallerSceneWifiScan,
    ApiCallerSceneWifiSaved,
    ApiCallerSceneWifiConnect,
    ApiCallerSceneCallAdd,
    ApiCallerSceneCallList,
    ApiCallerSceneCallDetail,
    ApiCallerSceneCount,
} ApiCallerScene;

typedef enum {
    ApiCallerViewMainMenu,
    ApiCallerViewWifiMenu,
    ApiCallerViewCallMenu,
    ApiCallerViewWifiScan,
    ApiCallerViewTextInput,
    ApiCallerViewTextBox,
    ApiCallerViewCallResult,
} ApiCallerView;

#define WIFI_HISTORY_MAX 8

#define CALL_HISTORY_MAX     16
#define CALL_URL_MAX         128
#define CALL_QUERY_MAX       64
#define CALL_HEADERS_MAX     128
#define CALL_BODY_MAX        256
#define CALL_EDIT_INDEX_NONE 0xFF
#define CALL_RESPONSE_MAX    32768
#define CALL_ERROR_MAX       160

typedef enum {
    CallProtocolHttp,
    CallProtocolHttps,
    CallProtocolCount,
} CallProtocol;

typedef enum {
    CallMethodGet,
    CallMethodPost,
    CallMethodPut,
    CallMethodDelete,
    CallMethodPatch,
    CallMethodHead,
    CallMethodCount,
} CallMethod;

typedef struct {
    char url[CALL_URL_MAX];
    char query[CALL_QUERY_MAX];
    char headers[CALL_HEADERS_MAX];
    char body[CALL_BODY_MAX];
    CallProtocol protocol;
    CallMethod method;
} CallEntry;

/** Human-readable names, defined in call_history.c. */
extern const char* const call_method_names[CallMethodCount];
extern const char* const call_protocol_names[CallProtocolCount];

typedef struct {
    char ssid[64];
    char password[64];
} WifiHistoryEntry;

typedef struct AppContext AppContext;

struct AppContext {
    Gui* gui;
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;

    // Locale
    LocLang locale_lang;

    // Views
    VariableItemList* var_item_list; // Main menu
    VariableItemList* var_item_list_wifi; // WiFi menu
    VariableItemList* var_item_list_call; // Call add/edit form
    Submenu* submenu; // WiFi scan results
    TextInput* text_input; // WiFi password input
    LongTextView* long_text_view; // Scrollable view for long responses
    TextBox* text_box; // Shared placeholder/status for stub scenes

    // FlipperHTTP link (owned by wifi_manager)
    FlipperHTTP* fhttp;

    // WiFi state
    FuriString* wifi_ssid_list; // Newline-separated scan results
    char ssid[64]; // Selected SSID (scan -> connect)
    char password[64]; // Password buffer for TextInput

    // Asynchronous WiFi scan state
    bool wifi_scan_in_progress;
    uint32_t wifi_scan_start_tick;
    uint32_t wifi_scan_done_tick;

    // Saved WiFi networks history (persisted in apps_data)
    WifiHistoryEntry wifi_history[WIFI_HISTORY_MAX];
    uint8_t wifi_history_count;

    // Saved API calls (persisted in apps_data)
    CallEntry call_history[CALL_HISTORY_MAX];
    uint8_t call_history_count;

    // Call add/edit form state
    CallEntry call_form; // Working copy shown by the form
    uint8_t call_edit_index; // CALL_EDIT_INDEX_NONE when adding a new call

    // In-flight API request state (written by the FlipperHTTP RX thread)
    char call_response[CALL_RESPONSE_MAX];
    volatile size_t call_response_len;
    bool call_response_truncated; // The capture hit the buffer limit
    char call_error[CALL_ERROR_MAX];
    int call_status_code;
    bool call_received; // Success marker received from the board
    bool call_body_done; // End marker received from the board
    bool call_in_progress;
    uint32_t call_start_tick;
    uint32_t call_progress_last_second; // Progress screen update throttle
};
