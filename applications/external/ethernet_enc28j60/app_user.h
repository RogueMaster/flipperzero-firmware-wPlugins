#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>

#include <dialogs/dialogs.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/text_input.h>
#include <gui/modules/number_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/modules/file_browser.h>
#include <gui/modules/file_browser_worker.h>
#include <gui/modules/loading.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <storage/storage.h>

#include "draw_functions/ip_assigner.h"
#include "scenes_config/app_scene_functions.h"
#include "ethernet_app_icons.h"

#include "libraries/chip/enc28j60.h"
#include "modules/arp_module.h"
#include "modules/dhcp_protocol.h"
#include "modules/tcp_module.h"
#include "modules/udp_module.h"
#include "modules/capture_module.h"
#include "modules/analysis_module.h"
#include "modules/ping_module.h"
#include "modules/os_detector_module.h"

#define MAX_OS_SCAN_PORTS 16

#include "libraries/functions/functions.h"

#define DEV_MODE 0

// Version of the app
#define APP_NAME "ETHERNET APP"
#ifndef FAP_APP_VERSION
#define FAP_APP_VERSION "v1.1.1.2"
#endif
#define APP_VERSION FAP_APP_VERSION

// Path for the files
#define PATHAPP    "apps_data/ethernet" // Path
#define PATHAPPEXT EXT_PATH(PATHAPP) // Add path to the Flipper

#define PATHPCAPS PATHAPPEXT "/files" // Path to save pcaps

// F0.4e — ethernet_app_flags_t / ALL_FLAGS / MASK_FLAGS / IS_NOT_LINK_UP
// were the worker thread's signaling protocol (flag_dhcp_dora set by
// GetIPScene, processed in ethernet_thread). With the worker thread
// gone, DORA runs in GetIPScene's alt thread and these flags are
// dead. Removed.

// For GET IP scene Events
typedef enum {
    wait_ip_event = 1,
    ip_no_gotten_event,
    ip_gotten_event,
} get_ip_events;

// For Passive Discovery scene
typedef enum {
    PassiveDiscoveryStateConfig,
    PassiveDiscoveryStateListening,
    PassiveDiscoveryStateFinished,
} passive_discovery_state_t;

// For Passive Discovery scene protocols
typedef enum {
    PassiveProtocolALL,
    PassiveProtocolLLDP,
    PassiveProtocolEAPOL,
    PassiveProtocolCDP,
    PassiveProtocolClearAll,

    PassiveProtocolCount
} passive_protocol_t;

// For Passive Discovery scene protocol names
typedef struct {
    passive_discovery_state_t state;
    passive_protocol_t protocol;
} passive_discovery_context_t;

// Cross-scene scan parameters (formerly file-static globals scattered
// across scenes; centralized in F0.1 to remove name collisions and
// enable persistence in F0.2).
typedef struct {
    uint8_t target_ip[4]; // generic target IPv4 (port scan / OS detect / ARP spoof)
    uint16_t target_port; // single port (port scan start)
    uint16_t range_port; // count of ports to scan from target_port
    uint8_t protocols_index; // 0=TCP, 1=UDP (PORTS_SCANNER_TCP/UDP)
    uint8_t ip_ping[4]; // ping target IPv4
    uint8_t ip_start[4]; // ARP scan start IPv4
    uint8_t range_ip; // ARP scan count
} scan_params_t;

// Forward declaration so the App struct can hold rx_handle_t* fields.
// The full definition lives in libraries/chip/rx_dispatch.h, which is
// included below the App typedef (it depends on App* in its API).
typedef struct rx_handle rx_handle_t;

// Struct for the App
typedef struct {
    arp_list ip_list[255];
    uint8_t ip_counter; // Variable for countrt of ip_list
    uint8_t ip_gateway[4]; // Array to save the gateway ip
    uint8_t mac_gateway[6]; // Array to save the mac_gateway

    uint8_t ip_helper[4];
    uint8_t mac_helper[6]; // F0.5d-wave2 — was [4]; ArpScannerScene
        // memcpy'd 6 bytes here, silently overwriting
        // is_static_ip / enc28j60_connected on every
        // "select host from scan list" action.

    bool is_static_ip; // To know if the device has the static IP
    bool enc28j60_connected; // To know if the enc28j60 is connected
    bool is_dora;
    volatile bool dora_cancel; // F0.5f — flipped by GetIPScene on_exit so the
    // alt thread's DORA loop can break out before
    // its 10 s timeout fires.
    bool open_pcap_after_sniff;
    volatile bool sniffer_stop;
    volatile bool sniffer_finished;
    bool sniffer_link_error;
    volatile bool passive_discovery_stop;
    uint16_t passive_neighbor_count;
    uint8_t passive_selected_neighbor;
    uint8_t passive_details_page;
    volatile bool arpspoofing_stop;

    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Widget* widget;
    Submenu* submenu;
    VariableItemList* varList;
    TextBox* text_box;
    ByteInput* input_byte_value;
    FileBrowser* file_browser;
    ip_assigner_t* ip_assigner;
    Loading* loading;
    NumberInput* number_input;

    enc28j60_t* ethernet; // Instance for the enc28j60

    Storage* storage; // Set the storage
    DialogsApp* dialogs;

    File* file; // File to save logs

    FuriString* text; // String for general use
    FuriString* path; // String to get path from file browser

    // F0.4e — `thread` (the long-running worker that handled DORA flags)
    // is gone; rx_dispatch owns the chip and per-scene alt threads do
    // any heavy lifting.
    FuriThread* thread_alternative; // Per-scene alt thread (one at a time)

    port_result_t ports[MAX_OS_SCAN_PORTS];
    uint8_t ports_count;
    bool os_guess;
    uint16_t src_port;
    uint16_t selected_menu_index;

    scan_params_t scan_params; // F0.1 — centralized cross-scene targets

    passive_discovery_context_t passive_discovery; // F1.1 — Passive Discovery scene state & protocol

    rx_handle_t* auto_arp_handle;
    rx_handle_t* auto_icmp_handle;
} App;

// F0.2 — settings persistence. Must be included AFTER the App typedef
// because settings.h declares functions taking `App*`, and App is an
// anonymous-struct typedef (cannot be forward-declared).
#include "libraries/settings/settings.h"
#include "libraries/scanner/scanner_session.h"
#include "libraries/chip/rx_dispatch.h"

// Views in the App
typedef enum {
    SubmenuView,
    WidgetView,
    VarListView,
    TextBoxView,
    DialogInfoView,
    InputByteView,
    NumberInputView,
    FileBrowserView,
    IpAssignerView,
    LoadingView
} scenesViews;

typedef enum {
    ARP_STATE_START_SCAN = 0,
    ARP_STATE_SET_IP,
    ARP_STATE_SHOW_LIST,
    ARP_STATE_SPOOF,
    ARP_STATE_SELECT_IP,
    ARP_STATE_SET_RANGE,
} ARP_SCENE_STATES;

// This functions works only to draw repetitive views in widgets
void draw_in_development(App* app); // draws when something is on development
void draw_device_no_connected(App* app); // draws when the device is not connected
void draw_network_not_connected(App* app); // draws if the device is not connected to a network
void draw_waiting_for_ip(App* app); // Draw when you're waiting for an IP
void draw_your_ip_is(App* app); // Draw the IP when you got it
void draw_ip_not_got_it(App* app); // Draw when get the ip failed
void draw_dora_failed(App* app); // Draw when the DORA process failed
void draw_dora_needed(App* app); // Draw when the DORA process needed
void draw_port_open(App* app); // Draw when the port is open
void draw_port_not_open(App* app); // Draw when the port is not open
void draw_ask_for_ip(App* app); // Draw to ask a new IP
void draw_text(App* app, const char* text); // Draw text

// F0.4e — ethernet_thread declaration removed; the function is gone.
