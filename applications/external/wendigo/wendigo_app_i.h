#pragma once

#include "wendigo_app.h"
#include "scenes/wendigo_scene.h"
#include "wendigo_custom_event.h"
#include "wendigo_uart.h"

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/text_box.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_input.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/popup.h>
#include <furi_hal_rtc.h>
#include <sys/time.h>

#include "wendigo_hex_input.h"

#define IS_FLIPPER_APP          (1)
/* TODO: Find a way to extract fap_version from application.fam */
#define FLIPPER_WENDIGO_VERSION "0.5.1"

#include "wendigo_common_defs.h"
#include "wendigo_pnl.h"

/* How frequently should Flipper poll ESP32 when scanning to restart
   scanning in the event the device restarts (seconds)? */
#define ESP32_POLL_INTERVAL      (3)
#define START_MENU_ITEMS         (7)
#define SETUP_MENU_ITEMS         (4)
#define SETUP_CHANNEL_MENU_ITEMS (14)

#define SETUP_RADIO_WIFI_IDX (2)
#define SETUP_RADIO_BT_IDX   (1)
#define SETUP_RADIO_BLE_IDX  (0)
#define RADIO_ON             (0)
#define RADIO_OFF            (1)
#define RADIO_MAC            (2)

#define CH_MASK_ALL (16384)

#define MAX_OPTIONS (7)

#define WENDIGO_TEXT_BOX_STORE_SIZE   (4096)
#define WENDIGO_TEXT_INPUT_STORE_SIZE (512)

// Command action type
typedef enum {
    NO_ACTION = 0,
    OPEN_SETUP,
    OPEN_SCAN,
    LIST_DEVICES,
    LIST_SELECTED_DEVICES,
    PNL_LIST,
    UART_TERMINAL,
    OPEN_MAC,
    OPEN_HELP
} ActionType;

// Command availability in different modes
typedef enum {
    OFF = 0,
    TEXT_MODE = 1,
    HEX_MODE = 2,
    BOTH_MODES = 3
} ModeMask;

/* Interface types - One for each radio that be enabled/disabled */
typedef enum {
    IF_WIFI = 0,
    IF_BT_CLASSIC = 1,
    IF_BLE = 2,
    IF_COUNT = 3
} InterfaceType;

/* Interface struct to encapsulate MAC and radio state */
typedef struct {
    uint8_t mac_bytes[MAC_BYTES];
    bool active;
    bool mutable;
    bool scanning;
    bool initialised;
} WendigoRadio;

typedef struct {
    const char* item_string;
    const char* options_menu[MAX_OPTIONS];
    int num_options_menu;
    ActionType action;
    ModeMask mode_mask;
} WendigoItem;

typedef enum msgType {
    MSG_ERROR = 0,
    MSG_WARN,
    MSG_INFO,
    MSG_DEBUG,
    MSG_TRACE,
    MSG_TYPE_COUNT
} MsgType;

typedef enum {
    WendigoAppViewVarItemList,
    WendigoAppViewDeviceList,
    WendigoAppViewDeviceDetail,
    WendigoAppViewStatus, /* This doesn't have a view but is used as a flag in app->current_view */
    WendigoAppViewPNLList, /* As above */
    WendigoAppViewPNLDeviceList, /* This too */
    WendigoAppViewAPSTAs, /* And this */
    WendigoAppViewSTAAP, /* Also */
    WendigoAppViewConsoleOutput, // TODO: Consider whether there's a better way to flag these views
    WendigoAppViewTextInput,
    WendigoAppViewHexInput,
    WendigoAppViewHelp,
    WendigoAppViewSetup, /* This doesn't have an associated view but is used as a flag in app->current_view */
    WendigoAppViewSetupMAC,
    WendigoAppViewSetupChannel,
    WendigoAppViewLoading,
    WendigoAppViewPopup,
} WendigoAppView;

/* The Device List scene can be nested any number of times (well, until the
 * stack pointer overflows) to allow navigation, for example, from the
 * device list to an AP to one of its stations. DeviceListInstance captures
 * the components of that scene that differ between instances.
 */
typedef struct DeviceListInstance {
    wendigo_device** devices;
    uint16_t devices_count;
    uint8_t devices_mask;
    WendigoAppView view;
    char devices_msg[MAX_SSID_LEN + 18]; // Space for "Clients of MAX_SSID_LEN"
    bool free_devices; // Do we need to free devices[] when we're done with it?
} DeviceListInstance;

struct WendigoApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    WendigoAppView current_view;
    bool is_scanning;
    bool leaving_scene; /* Set to true when the back button is pressed to allow device list to be
                         * used for a variety of purposes - device list, selected device list, and
                         * navigating from device list to AP list to station list, etc.
                         */
    Widget* widget;
    VariableItemList* var_item_list;
    VariableItemList* devices_var_item_list;
    VariableItemList* detail_var_item_list;
    Wendigo_Uart* uart;
    ByteInput* setup_mac;
    Popup* popup; // Avoid continual allocation and freeing of Popup by initialising at launch
    WendigoRadio interfaces[IF_COUNT];
    InterfaceType active_interface;
    uint32_t last_packet;

    uint8_t setup_selected_menu_index;
    uint16_t device_list_selected_menu_index;
    uint8_t setup_selected_option_index[SETUP_MENU_ITEMS];
    uint8_t selected_menu_index;
    uint8_t selected_option_index[START_MENU_ITEMS];
    uint16_t channel_mask;
    uint16_t CH_MASK[SETUP_CHANNEL_MENU_ITEMS + 1];

    // TODO: Review these attributes - Remove what I can as remnants of the past
    char text_input_store[WENDIGO_TEXT_INPUT_STORE_SIZE + 1];
    FuriString* text_box_store;
    size_t text_box_store_strlen;
    TextBox* text_box;
    TextInput* text_input;
    Wendigo_TextInput* hex_input;
    /* Mutexes to manage access to buffer[], networks[] and devices[] */
    FuriMutex* bufferMutex;
    FuriMutex* devicesMutex;
    FuriMutex* pnlMutex;
    /* Timer to poll the ESP32 if packets have stopped being received */
    FuriTimer* scan_timer;

    const char* selected_tx_string;
    bool is_command;
    bool is_custom_tx_string;
    bool hex_mode;
    uint8_t uart_ch;
    uint8_t new_uart_ch;
    int BAUDRATE;
    int NEW_BAUDRATE;
    int TERMINAL_MODE; //1=AT mode, 0=other mode
};

/* Public methods from wendigo_app.c */
void wendigo_popup_callback(void* context);
void wendigo_display_popup(WendigoApp* app, char* header, char* body);
void wendigo_uart_set_binary_cb(Wendigo_Uart* uart);
void wendigo_uart_set_console_cb(Wendigo_Uart* uart);
void bytes_to_string(uint8_t* bytes, uint16_t bytesCount, char* strBytes);
void wendigo_mac_query(WendigoApp* app);
void wendigo_set_mac_rcvd_callback(void (*update_callback)(void*));
void wendigo_mac_set(
    WendigoApp* app,
    InterfaceType type,
    uint8_t mac_bytes[MAC_BYTES],
    void (*update_callback)(void*));
void wendigo_mac_rcvd_callback(WendigoApp* app);
char* furi_status_to_string(FuriStatus status, char* result, uint8_t resultLen);
