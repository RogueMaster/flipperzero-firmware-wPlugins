#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/text_input.h>
#include <gui/modules/popup.h>
#include <gui/modules/widget.h>
#include <dialogs/dialogs.h>
#include <notification/notification_messages.h>

#define EVIL_BW16_TEXT_BOX_STORE_SIZE   (4096)
#define EVIL_BW16_TEXT_INPUT_STORE_SIZE (512)
#define EVIL_BW16_UART_BAUD_RATE        (115200)
#define EVIL_BW16_UART_RX_BUF_SIZE      (2048)
#define EVIL_BW16_MAX_NETWORKS          (50)
#define EVIL_BW16_MAX_TARGETS           (10)

// Scene definitions
typedef enum {
    EvilBw16SceneStart,
    EvilBw16SceneMainMenu,
    EvilBw16SceneScanner,
    EvilBw16SceneScannerResults,
    EvilBw16SceneAttacks,
    EvilBw16SceneAttackConfig,
    EvilBw16SceneSniffer,
    EvilBw16SceneSnifferResults,
    EvilBw16SceneConfig,
    EvilBw16SceneDeviceInfo,
    EvilBw16SceneUartTerminal,
    EvilBw16SceneNum,
} EvilBw16Scene;

// View definitions
typedef enum {
    EvilBw16ViewMainMenu,
    EvilBw16ViewScanner,
    EvilBw16ViewAttacks,
    EvilBw16ViewSniffer,
    EvilBw16ViewConfig,
    EvilBw16ViewTextBox,
    EvilBw16ViewTextInput,
    EvilBw16ViewPopup,
    EvilBw16ViewWidget,
} EvilBw16View;

// Command types
typedef enum {
    EvilBw16CommandScan,
    EvilBw16CommandStartDeauth,
    EvilBw16CommandStopDeauth,
    EvilBw16CommandDisassoc,
    EvilBw16CommandStartSniff,
    EvilBw16CommandStopSniff,
    EvilBw16CommandHopOn,
    EvilBw16CommandHopOff,
    EvilBw16CommandSetTarget,
    EvilBw16CommandSetChannel,
    EvilBw16CommandInfo,
    EvilBw16CommandRandomAttack,
    EvilBw16CommandTimedAttack,
    EvilBw16CommandResults,
    EvilBw16CommandHelp,
} EvilBw16Command;

// Response types
typedef enum {
    EvilBw16ResponseInfo,
    EvilBw16ResponseError,
    EvilBw16ResponseDebug,
    EvilBw16ResponseScanResult,
    EvilBw16ResponseAttackStatus,
    EvilBw16ResponseCommand,
    EvilBw16ResponseChannelHop,
    EvilBw16ResponseUnknown,
} EvilBw16ResponseType;

// Attack modes
typedef enum {
    EvilBw16AttackModeDeauth,
    EvilBw16AttackModeDisassoc,
    EvilBw16AttackModeRandom,
} EvilBw16AttackMode;

// Sniffer modes
typedef enum {
    EvilBw16SniffModeAll,
    EvilBw16SniffModeBeacon,
    EvilBw16SniffModeProbe,
    EvilBw16SniffModeDeauth,
    EvilBw16SniffModeEapol,
    EvilBw16SniffModePwnagotchi,
} EvilBw16SniffMode;

// WiFi band types
typedef enum {
    EvilBw16BandUnknown,
    EvilBw16Band24GHz,
    EvilBw16Band5GHz,
} EvilBw16Band;

// GPIO pin selection for UART
typedef enum {
    EvilBw16GpioPins13_14 = 0, // USART (pins 13/14)
    EvilBw16GpioPins15_16 = 1, // LPUART (pins 15/16)
} EvilBw16GpioPins;

// WiFi Network structure
typedef struct {
    uint8_t index; // Internal array index for menu selection
    uint8_t device_index; // Original index from BW16 device (for commands)
    char ssid[64];
    char bssid[18];
    int channel;
    int rssi;
    EvilBw16Band band;
    bool selected;
} EvilBw16Network;

// Configuration structure
typedef struct {
    uint32_t cycle_delay;
    uint32_t scan_time;
    uint32_t num_frames;
    uint8_t start_channel;
    bool scan_cycles;
    bool led_enabled;
    bool debug_mode;
    EvilBw16GpioPins gpio_pins; // GPIO pin selection
} EvilBw16Config;

// Attack state
typedef struct {
    bool active;
    EvilBw16AttackMode mode;
    uint32_t duration;
    uint32_t start_time;
    uint8_t target_indices[EVIL_BW16_MAX_TARGETS];
    uint8_t num_targets;
} EvilBw16AttackState;

// Sniffer state
typedef struct {
    bool is_running;
    EvilBw16SniffMode mode;
    uint8_t channel;
    bool hopping;
    uint32_t packet_count;
} EvilBw16SnifferState;

// UART worker
typedef struct EvilBw16UartWorker EvilBw16UartWorker;

// Main app structure
typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;
    DialogsApp* dialogs;

    // Views
    Submenu* submenu;
    TextBox* text_box;
    TextInput* text_input;
    Popup* popup;
    Widget* widget;

    // Text storage
    char* text_box_store;
    char* text_input_store;
    FuriString* text_box_string;

    // UART communication
    EvilBw16UartWorker* uart_worker;
    FuriStreamBuffer* uart_rx_stream;

    // Data storage
    EvilBw16Network networks[EVIL_BW16_MAX_NETWORKS];
    uint8_t network_count;
    bool scan_in_progress;

    EvilBw16Config config;
    EvilBw16AttackState attack_state;
    EvilBw16SnifferState sniffer_state;

    // UI state
    uint8_t selected_menu_index;
    uint8_t selected_network_index;
    bool show_loading;

    // Status
    bool device_connected;
    char device_info[256];
    FuriString* log_string;
    FuriString* uart_log_string;
} EvilBw16App;

// Scene manager events
typedef enum {
    EvilBw16EventStartScan,
    EvilBw16EventScanComplete,
    EvilBw16EventStartAttack,
    EvilBw16EventStopAttack,
    EvilBw16EventStartSniffer,
    EvilBw16EventStopSniffer,
    EvilBw16EventSnifferStarted,
    EvilBw16EventSnifferStopped,
    EvilBw16EventPacketReceived,
    EvilBw16EventConfigUpdate,
    EvilBw16EventUartTerminalRefresh,
    EvilBw16EventBack,
    EvilBw16EventExit,
} EvilBw16Event;

// Function prototypes

// Main app functions
EvilBw16App* evil_bw16_app_alloc(void);
void evil_bw16_app_free(EvilBw16App* app);
int32_t evil_bw16_app(void* p);

// Scene functions
void evil_bw16_scene_on_enter_start(void* context);
void evil_bw16_scene_on_enter_main_menu(void* context);
void evil_bw16_scene_on_enter_scanner(void* context);
void evil_bw16_scene_on_enter_scanner_results(void* context);
void evil_bw16_scene_on_enter_attacks(void* context);
void evil_bw16_scene_on_enter_attack_config(void* context);
void evil_bw16_scene_on_enter_sniffer(void* context);
void evil_bw16_scene_on_enter_sniffer_results(void* context);
void evil_bw16_scene_on_enter_config(void* context);
void evil_bw16_scene_on_enter_device_info(void* context);
void evil_bw16_scene_on_enter_uart_terminal(void* context);

bool evil_bw16_scene_on_event_start(void* context, SceneManagerEvent event);
bool evil_bw16_scene_on_event_main_menu(void* context, SceneManagerEvent event);
bool evil_bw16_scene_on_event_scanner(void* context, SceneManagerEvent event);
bool evil_bw16_scene_on_event_scanner_results(void* context, SceneManagerEvent event);
bool evil_bw16_scene_on_event_attacks(void* context, SceneManagerEvent event);
bool evil_bw16_scene_on_event_attack_config(void* context, SceneManagerEvent event);
bool evil_bw16_scene_on_event_sniffer(void* context, SceneManagerEvent event);
bool evil_bw16_scene_on_event_sniffer_results(void* context, SceneManagerEvent event);
bool evil_bw16_scene_on_event_config(void* context, SceneManagerEvent event);
bool evil_bw16_scene_on_event_device_info(void* context, SceneManagerEvent event);
bool evil_bw16_scene_on_event_uart_terminal(void* context, SceneManagerEvent event);

void evil_bw16_scene_on_exit_start(void* context);
void evil_bw16_scene_on_exit_main_menu(void* context);
void evil_bw16_scene_on_exit_scanner(void* context);
void evil_bw16_scene_on_exit_scanner_results(void* context);
void evil_bw16_scene_on_exit_attacks(void* context);
void evil_bw16_scene_on_exit_attack_config(void* context);
void evil_bw16_scene_on_exit_sniffer(void* context);
void evil_bw16_scene_on_exit_sniffer_results(void* context);
void evil_bw16_scene_on_exit_config(void* context);
void evil_bw16_scene_on_exit_device_info(void* context);
void evil_bw16_scene_on_exit_uart_terminal(void* context);

// UART Worker Functions
EvilBw16UartWorker* evil_bw16_uart_init(EvilBw16App* app);
void evil_bw16_uart_free(EvilBw16UartWorker* worker);
void evil_bw16_uart_restart(EvilBw16App* app);
void evil_bw16_uart_tx(EvilBw16UartWorker* worker, const uint8_t* data, size_t len);
void evil_bw16_uart_tx_string(EvilBw16UartWorker* worker, const char* str);
void evil_bw16_uart_send_command(EvilBw16UartWorker* worker, const char* command);
bool evil_bw16_uart_read_line(
    EvilBw16UartWorker* worker,
    char* buffer,
    size_t buffer_size,
    uint32_t timeout_ms);
size_t evil_bw16_uart_rx_available(EvilBw16UartWorker* worker);
bool evil_bw16_uart_wait_for_response(
    EvilBw16UartWorker* worker,
    const char* expected_prefix,
    char* response_buffer,
    size_t buffer_size,
    uint32_t timeout_ms);

// Command Functions
void evil_bw16_send_command(EvilBw16App* app, const char* command);
void evil_bw16_send_scan_command(EvilBw16App* app);
void evil_bw16_send_attack_start(EvilBw16App* app);
void evil_bw16_send_attack_stop(EvilBw16App* app);
void evil_bw16_send_sniff_command(EvilBw16App* app, const char* mode);
void evil_bw16_send_set_target(EvilBw16App* app, const char* targets);
void evil_bw16_send_channel_hop(EvilBw16App* app, bool enable);
void evil_bw16_send_set_channel(EvilBw16App* app, int channel);
void evil_bw16_send_info_command(EvilBw16App* app);
void evil_bw16_send_config_to_device(EvilBw16App* app);

// Response Parsing
EvilBw16ResponseType evil_bw16_parse_response_type(const char* response);
bool evil_bw16_parse_scan_result(const char* response, EvilBw16Network* network);

// Utility functions
void evil_bw16_show_loading(EvilBw16App* app, const char* text);
void evil_bw16_hide_loading(EvilBw16App* app);
void evil_bw16_show_popup(EvilBw16App* app, const char* header, const char* text);
void evil_bw16_append_log(EvilBw16App* app, const char* text);
void evil_bw16_clear_log(EvilBw16App* app);
void evil_bw16_notification_message(EvilBw16App* app, const NotificationSequence* sequence);

// Debug functions
void debug_write_to_sd(const char* data);
