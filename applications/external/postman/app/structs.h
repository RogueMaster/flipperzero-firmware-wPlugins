
#ifndef STRUCTS_H
#define STRUCTS_H

#include <furi.h>

#define RX_BUF_SIZE         (2096)
#define LAST_RESPONSE_SIZE  (2096)
#define MAX_WIFI_NETWORKS   20
#define MAX_SSID_LENGTH     128
#define MAX_PASSWORD_LENGTH 128
#define CONNECT_CMD_BUFFER_SIZE                       \
    (16 + MAX_SSID_LENGTH + 1 + MAX_PASSWORD_LENGTH + \
     1) // "WIFI_CONNECT: " + ssid + " " + password + null terminator
#define MAX_WIFI_CREDENTIALS 10 // Define the maximum number of WiFi credentials

#define KEY_NAME_SIZE 40
#define TEXT_STORE_SIZE \
    1024 // Approximate size of bearer token is around 800, hopefully this will be enough
#define UART_TERMINAL_TEXT_INPUT_STORE_SIZE (512)

/** all scenes */

typedef enum {
    SetUpDialog,
    MainMenu,
    Connect,
    Connect_Details,
    Text_Input,
    Connect_Favs,
    Get,
    Display,
    Get_Url_List,
    Download,
    Post,
    Post_Url_List,
    Build_Http,
    Build_Http_Url_List,
    Build_Http_Headers,
    // Listen,
    // About,
    Count
} AppScene;

/** all views */
typedef enum {
    AppView_SetupDialog,
    AppView_Menu,
    AppView_Connect,
    AppView_Connect_Details,
    AppView_Connect_Text_Input,
    AppView_Connect_Favs,
    AppView_Get,
    AppView_Get_Url_List,
    AppView_Download,
    AppView_Post,
    AppView_Post_Url_List,
    AppView_BuildHttpCall,
    AppView_BuildHttp_Url_List,
    AppView_BuildHttp_Headers,
    // AppView_Listen,
    // AppView_About,
    AppView_Display,
} AppView;

/** all custom events */
typedef enum {
    AppEvent_SetupDialog,
    AppEvent_MainMenu,
    AppEvent_Connect,
    AppEvent_Connect_Details,
    AppEvent_Input_Text,
    AppEvent_Connect_Favs,
    AppEvent_Get,
    AppEvent_Post,
    AppEvent_Build_Http,
    AppEvent_Build_Http_Url,
    AppEvent_Build_Http_Payload,
    AppEvent_Build_Http_Url_List,
    AppEvent_Build_Http_Header_Key,
    AppEvent_Build_Http_Header_Value,
    AppEvent_Download,
    AppEvent_Listen,
    AppEvent_About,
    AppEvent_Display,
    AppEvent_Get_Url_List,
    AppEvent_Set_Filename,
    AppEvent_StartDownload,
    AppEvent_Payload,
    AppEvent_Post_Url_List
} AppEvent;

/* main menu scene */

/** indices for menu items */
typedef enum {
    MenuSelection_Connect,
    MenuSelection_Get,
    MenuSelection_Post,
    MenuSelection_Build_http,
    MenuSelection_Download,
    MenuSelection_Listen,
    MenuSelection_About,
    MenuSelection_Exit,
    MenuSelection_Connect_Favs,
} AppMenuSelection;

// Wifi / UART
// Event Flags for UART Worker Thread

typedef enum {
    BOARD_CONNECTED_WIFI_OFF,
    BOARD_CONNECTED_WIFI_ON,
    BOARD_ERROR
} UartStatus;

typedef enum {
    WorkerEvtStop = (1 << 0),
    WorkerEvtRxDone = (1 << 1),
    WorkerEvtRxIdle = (1 << 2),
    WorkerJobDone = (1 << 3),
} WorkerEvtFlags;

typedef struct {
    const char* name;
    const char* description;
    void (*execute)(const char* argument);
} Command;

typedef struct {
    char ssid[MAX_SSID_LENGTH];
} WifiNetwork;

typedef struct {
    char ssid[128];
    char password[128];
    bool is_default;
} WifiCredential;

typedef struct {
    WifiNetwork networks[MAX_WIFI_NETWORKS];
    char selected_ssid[MAX_SSID_LENGTH];
    char connected_ssid[MAX_SSID_LENGTH];
    char password_ssid[MAX_PASSWORD_LENGTH];
} AvailableWifiList;

typedef enum {
    Details_Disconnect,
    Details_Connect,
    Details_SetPassword,
    Details_SaveToCsv,
    Details_MarkAsDefault,
    Details_Forget,
    Details_Exit
} Connect_DetailsActionMenu;

/// App Views States
// Intro setup state

typedef enum {
    SetupDialogStateWelcome,
    SetupDialogStateChecking,
    SetupDialogStateResult
} SetupDialogState;

// Custom text input state

typedef enum {
    TextInputState_GetUrl,
    TextInputState_WifiPassword,
    TextInputState_PostUrl,
    TextInputState_Filename,
    TextInputState_Payload,
    TextInputState_BuildHttpUrl,
    TextInputState_BuildHttpPayload,
    TextInputState_BuildHttpHeaderKey,
    TextInputState_BuildHttpHeaderValue,
    TextInputState_Message,
} TextInputState;

// DISPLAY VIEW

typedef enum {
    DISPLAY_GET,
    DISPLAY_GET_STREAM,
    DISPLAY_POST,
    DISPLAY_POST_STREAM,
    DISPLAY_BUILD_HTTP,
    DISPLAY_DOWNLOAD,
    DISPLAY_LISTEN,
    DISPLAY_NONE,
    COUNT
} DisplayMode;

typedef enum {
    DOWNLOAD_NONE,
    DOWNLOAD_GET,
    DOWNLOAD_POST,
    DOWNLOAD_CUSTOM
} DownloadMode;

// GET VIEW

typedef struct {
    bool mode; // Mode: Display or Save
    char url[TEXT_STORE_SIZE]; // URL
    bool method; // Method: GET or GET_STREAM
} GetState;

typedef struct {
    char url[TEXT_STORE_SIZE];
} UrlList;

// POST VIEW
typedef struct {
    bool mode; // Mode: Display or Save
    char url[TEXT_STORE_SIZE]; // URL
    bool method; // Method: GET or GET_STREAM
    FuriString* payload; // Payload {body}
} PostState;

typedef enum {
    PostItemMode,
    PostItemMethod,
    PostItemSetUrl,
    PostItemSetPayload,
    PostItemAction,
    PostItemSaveToCsv,
    PostItemDeleteFromCsv,
    PostItemLoadFromCsv,
} PostItem;

typedef struct {
    char url[TEXT_STORE_SIZE];
    FuriString* payload;
} PostUrlList;

// Build Http call view

typedef struct {
    char key[TEXT_STORE_SIZE];
    char value[TEXT_STORE_SIZE];
} HttpBuildHeader;

typedef enum {
    HEAD,
    GET,
    POST,
    PATCH,
    PUT,
    DELETE
} HttpBuildMethod;

// Build Http call State
#define MAX_HEADERS 5

// Build Http State
typedef struct {
    bool mode; // Mode: Display or Save
    char url[TEXT_STORE_SIZE]; // URL
    HttpBuildMethod http_method; // HEAD, GET, POST, PATCH, PUT, DELETE
    HttpBuildHeader* headers; // Headers
    size_t headers_count; // Number of headers
    FuriString* payload; // Payload {body}
    bool show_response_headers;
} BuildHttpState;

typedef enum {
    BuildHttpItemMode,
    BuildHttpItemMethod,
    BuildHttpItemShowHeaders,
    BuildHttpItemSetUrl,
    BuildHttpItemSetHeaders,
    BuildHttpItemSetPayload,
    BuildHttpItemAction,
    BuildHttpItemSaveToCsv,
    BuildHttpItemDeleteFromCsv,
    BuildHttpItemLoadFromCsv
} BuildHttpItem;

typedef struct {
    char url[TEXT_STORE_SIZE];
    bool mode;
    HttpBuildMethod http_method;
    HttpBuildHeader* headers;
    size_t headers_count;
    FuriString* payload;
    bool show_response_headers;

} BuildHttpList;

#endif // STRUCTS_H
