#pragma once

#include "bunnyconnect.h"
#include "lib/bunnyconnect_views.h"
#include "lib/bunnyconnect_keyboard.h"
#include "lib/bunnyconnect_helpers.h"
#include "lib/bunnyconnect_draw.h"
#include "lib/bunnyconnect_power.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_serial.h>
#include <furi_hal_usb_hid.h>
#include <furi_hal_usb_cdc.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/text_input.h>
#include <gui/modules/popup.h>
#include <gui/modules/widget.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TAG                  "BunnyConnect"
#define TERMINAL_BUFFER_SIZE 2048
#define INPUT_BUFFER_SIZE    256
#define RX_BUFFER_SIZE       512
#define TX_BUFFER_SIZE       512

typedef enum {
    BunnyConnectViewMainMenu,
    BunnyConnectViewTerminal,
    BunnyConnectViewCustomKeyboard,
    BunnyConnectViewConfig,
    BunnyConnectViewInfo,
    BunnyConnectViewPopup,
} BunnyConnectViewId;

typedef enum {
    BunnyConnectStateDisconnected,
    BunnyConnectStateConnected,
    BunnyConnectStatePopup,
} BunnyConnectState;

typedef enum {
    BunnyConnectCustomEventKeyboardDone,
    BunnyConnectCustomEventRefreshScreen,
    BunnyConnectCustomEventConnect,
    BunnyConnectCustomEventDisconnect,
    BunnyConnectCustomEventConnectionFailed,
    BunnyConnectCustomEventTogglePower,
    BunnyConnectCustomEventConfigSave,
} BunnyConnectCustomEvent;

typedef struct {
    char device_name[32];
    uint32_t baud_rate;
    bool auto_connect;
    bool flow_control;
    uint8_t data_bits;
    uint8_t stop_bits;
    uint8_t parity;
    bool usb_power_enabled; // Enable USB power output
    bool auto_enumerate; // Auto-enumerate as CDC device
} BunnyConnectConfig;

struct BunnyConnectApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;

    // Views
    Submenu* main_menu;
    Submenu* config_menu;
    TextBox* text_box;
    BunnyConnectKeyboard* custom_keyboard;
    Popup* popup;
    Widget* info_widget;

    NotificationApp* notifications;

    // Serial communication - using generic handle since control API may not be available
    FuriHalSerialHandle* serial_handle;
    BunnyConnectState state;
    BunnyConnectConfig config;

    // View tracking
    BunnyConnectViewId current_view;

    // Terminal
    char* terminal_buffer;
    size_t terminal_buffer_position;

    // Threading and synchronization
    FuriThread* worker_thread;
    FuriMutex* mutex;
    FuriMessageQueue* event_queue;

    // App state
    bool is_running;
    bool usb_connected;

    // USB CDC communication
    bool usb_cdc_connected;
    uint8_t usb_cdc_port; // CDC port number (0 for single CDC)

    FuriString* text_string;
    char input_buffer[INPUT_BUFFER_SIZE];
    char rx_buffer[RX_BUFFER_SIZE];
    char tx_buffer[TX_BUFFER_SIZE];
};

// Function declarations
void bunnyconnect_show_error_popup(BunnyConnectApp* app, const char* message);
bool bunnyconnect_custom_event_callback(void* context, uint32_t event);
bool bunnyconnect_navigation_callback(void* context);
int32_t bunnyconnect_worker_thread(void* context);

#ifdef __cplusplus
}
#endif
