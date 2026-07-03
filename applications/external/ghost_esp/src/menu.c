#include "menu.h"
#include "confirmation_view.h"
#include "ghost_esp_ep.h"
#include "settings_def.h"
#include "settings_storage.h"
#include "uart_utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>

struct View {
    ViewDrawCallback draw_callback;
    ViewInputCallback input_callback;
    ViewCustomCallback custom_callback;

    ViewModelType model_type;
    ViewNavigationCallback previous_callback;
    ViewCallback enter_callback;
    ViewCallback exit_callback;
    ViewOrientation orientation;

    ViewUpdateCallback update_callback;
    void* update_callback_context;

    void* model;
    void* context;
};

typedef struct {
    const char* label; // Display label in menu
    const char* command; // UART command to send
    const char* capture_prefix; // Prefix for capture files (NULL if none)
    const char* file_ext; // File extension for captures (NULL if none)
    const char* folder; // Folder for captures (NULL if none)
    bool needs_input; // Whether command requires text input
    const char* input_text; // Text to show in input box (NULL if none)
    bool needs_confirmation; // Whether command needs confirmation
    const char* confirm_header; // Confirmation dialog header
    const char* confirm_text; // Confirmation dialog text
    const char* details_header; // Header for details view
    const char* details_text; // Detailed description/info text
} MenuCommand;

typedef struct {
    const char* label;
    const char* command;
    const char* capture_prefix;
} SniffCommandDef;

typedef struct {
    const char* label;
    const char* command;
} BeaconSpamDef;

typedef struct {
    AppState* state;
    const MenuCommand* command;
} MenuCommandContext;

typedef struct {
    const char* label;
    const char* command;
    const char* details_header;
    const char* details_text;
    bool needs_input;
    const char* input_text;
} CyclingMenuDef;

// Forward declarations of static functions
static void show_menu(
    AppState* state,
    const MenuCommand* commands,
    size_t command_count,
    const char* header,
    Submenu* menu,
    uint8_t view_id);
static void show_command_details(AppState* state, const MenuCommand* command);
static bool menu_input_handler(InputEvent* event, void* context);
static void text_input_result_callback(void* context);
static void confirmation_ok_callback(void* context);
static void confirmation_cancel_callback(void* context);
static void app_info_ok_callback(void* context);
static void ir_sweep_stop_callback(void* context);
static void execute_menu_command(AppState* state, const MenuCommand* command);
static void error_callback(void* context);
static bool ir_query_and_parse_list(AppState* state);
static bool ir_query_and_parse_show(AppState* state, uint32_t remote_index);
static bool ir_query_and_parse_universals(AppState* state);
static bool ir_query_and_parse_universal_buttons(AppState* state, const char* filename);
static void ir_show_remotes_menu(AppState* state);
static void ir_show_buttons_menu(AppState* state);
static void ir_show_universals_menu(AppState* state);
static void ir_show_error(AppState* state, const char* text);
static void show_result_dialog(AppState* state, const char* header, const char* text);
static bool handle_ir_command_feedback_ex(
    AppState* state,
    const char* cmd,
    bool send_cmd,
    bool reset_buffers);
static bool handle_ir_command_feedback(AppState* state, const char* cmd);
static bool ir_index_buttons_from_file(AppState* state);
static void ir_send_button_from_file(AppState* state, uint32_t button_index);

// Sniff command definitions
static const SniffCommandDef sniff_commands[] = {
    {"< Sniff WPS >", "capture -wps\n", "wps_capture"},
    {"< Sniff Raw Packets >", "capture -raw\n", "raw_capture"},
    {"< Sniff Probes >", "capture -p\n", "probe_capture"},
    {"< Sniff Deauth >", "capture -deauth\n", "deauth_capture"},
    {"< Sniff Beacons >", "capture -beacon\n", "beacon_capture"},
    {"< Sniff EAPOL >", "capture -eapol\n", "eapol_capture"},
    {"< Sniff Pwn >", "capture -pwn\n", "pwn_capture"},
};

// Beacon spam command definitions
static const CyclingMenuDef beacon_spam_commands[] = {
    {"< Beacon Spam (List) >",
     "beaconspam -l\n",
     "Beacon Spam (List)",
     "Spam SSIDs from list.",
     false,
     NULL},
    {"< Beacon Spam (Random) >",
     "beaconspam -r\n",
     "Beacon Spam (Random)",
     "Spam random SSIDs.",
     false,
     NULL},
    {"< Beacon Spam (Rickroll) >",
     "beaconspam -rr\n",
     "Beacon Spam (Rickroll)",
     "Spam Rickroll SSIDs.",
     false,
     NULL},
    {"< Beacon Spam (Custom) >",
     "beaconspam",
     "Beacon Spam (Custom)",
     "Spam custom SSID.",
     true,
     "SSID Name"},
};

// BLE spam command definitions
static const CyclingMenuDef ble_spam_commands[] = {
    {"< BLE Spam (Apple) >",
     "blespam -apple\n",
     "BLE Spam (Apple)",
     "Spam Apple BLE devices.",
     false,
     NULL},
    {"< BLE Spam (Microsoft) >",
     "blespam -ms\n",
     "BLE Spam (Microsoft)",
     "Spam Microsoft BLE devices.",
     false,
     NULL},
    {"< BLE Spam (Samsung) >",
     "blespam -samsung\n",
     "BLE Spam (Samsung)",
     "Spam Samsung BLE devices.",
     false,
     NULL},
    {"< BLE Spam (Google) >",
     "blespam -google\n",
     "BLE Spam (Google)",
     "Spam Google BLE devices.",
     false,
     NULL},
    {"< BLE Spam (Random) >",
     "blespam -random\n",
     "BLE Spam (Random)",
     "Spam random BLE devices.",
     false,
     NULL},
};

static size_t current_rgb_index = 0;

static const CyclingMenuDef rgbmode_commands[] = {
    {"< LED: Rainbow >", "rgbmode rainbow\n", "LED: Rainbow", "Cycle rainbow colors.", false, NULL},
    {"< LED: Police >", "rgbmode police\n", "LED: Police", "Police light effect.", false, NULL},
    {"< LED: Strobe >", "rgbmode strobe\n", "LED: Strobe", "Strobe light effect.", false, NULL},
    {"< LED: Off >", "rgbmode off\n", "LED: Off", "Turn off LED.", false, NULL},
    {"< LED: Red >", "rgbmode red\n", "LED: Red", "Set LED to red.", false, NULL},
    {"< LED: Green >", "rgbmode green\n", "LED: Green", "Set LED to green.", false, NULL},
    {"< LED: Blue >", "rgbmode blue\n", "LED: Blue", "Set LED to blue.", false, NULL},
    {"< LED: Yellow >", "rgbmode yellow\n", "LED: Yellow", "Set LED to yellow.", false, NULL},
    {"< LED: Purple >", "rgbmode purple\n", "LED: Purple", "Set LED to purple.", false, NULL},
    {"< LED: Cyan >", "rgbmode cyan\n", "LED: Cyan", "Set LED to cyan.", false, NULL},
    {"< LED: Orange >", "rgbmode orange\n", "LED: Orange", "Set LED to orange.", false, NULL},
    {"< LED: White >", "rgbmode white\n", "LED: White", "Set LED to white.", false, NULL},
    {"< LED: Pink >", "rgbmode pink\n", "LED: Pink", "Set LED to pink.", false, NULL},
};

static const CyclingMenuDef wifi_scan_modes[] = {
    {"< Scan: (APs) >", "scanap\n", "WiFi AP Scanner", "Scans for WiFi APs...", false, NULL},
    {"< Scan: (APs Live) >",
     "scanap -live\n",
     "Live AP Scanner",
     "Continuously updates as APs are found\n- SSID names\n- Signal levels\n- "
     "Security type\n- Channel info\n",
     false,
     NULL},
    {"< Scan: (Stations) >", "scansta\n", "Station Scanner", "Scans for clients...", false, NULL},
    {"< Scan: (AP+STA) >", "scanall\n", "Scan All", "Combined AP/Station scan...", false, NULL},
};

static const CyclingMenuDef wifi_list_modes[] = {
    {"< List: (APs) >",
     "list -a\n",
     "List Access Points",
     "Shows list of APs found during last scan.",
     false,
     NULL},
    {"< List: (Stations) >",
     "list -s\n",
     "List Stations",
     "Shows list of clients found during last scan.",
     false,
     NULL},
};

static const CyclingMenuDef wifi_select_modes[] = {
    {"< Select: (AP) >",
     "select -a",
     "Select Access Point",
     "Select an AP by number from the scanned list.",
     true,
     "AP Number"},
    {"< Select: (Station) >",
     "select -s",
     "Select Station",
     "Target a station by number from the scan list for attacks.",
     true,
     "Station Number"},
};

static const CyclingMenuDef wifi_listen_modes[] = {
    {"< Listen Probes (Hop) >",
     "listenprobes\n",
     "Listen for Probes",
     "Listen for and log probe requests\nwhile hopping channels.",
     false,
     NULL},
    {"< Listen Probes (Chan) >",
     "listenprobes",
     "Listen on Channel",
     "Listen for probe requests on a\nspecific channel.",
     true,
     "Channel (1-165)"},
};

static size_t current_sniff_index = 0;
static size_t current_beacon_index = 0;
static size_t current_ble_spam_index = 0;
static size_t current_wifi_scan_index = 0;
static size_t current_wifi_list_index = 0;
static size_t current_wifi_select_index = 0;
static size_t current_wifi_listen_index = 0;

// WiFi menu command definitions
static const MenuCommand wifi_scanning_commands[] = {
    {
        .label = "< Scan: (APs) >", // Initial label
        .command = wifi_scan_modes[0].command,
        .details_header = wifi_scan_modes[0].details_header,
        .details_text = wifi_scan_modes[0].details_text,
    },
    {
        .label = "< List: (APs) >", // Initial label
        .command = wifi_list_modes[0].command,
        .details_header = wifi_list_modes[0].details_header,
        .details_text = wifi_list_modes[0].details_text,
    },
    {
        .label = "< Select: (AP) >", // Initial label
        .command = wifi_select_modes[0].command,
        .needs_input = wifi_select_modes[0].needs_input,
        .input_text = wifi_select_modes[0].input_text,
        .details_header = wifi_select_modes[0].details_header,
        .details_text = wifi_select_modes[0].details_text,
    },
    {
        .label = "< Listen Probes (Hop) >", // Initial label
        .command = wifi_listen_modes[0].command,
        .needs_input = wifi_listen_modes[0].needs_input,
        .input_text = wifi_listen_modes[0].input_text,
        .details_header = wifi_listen_modes[0].details_header,
        .details_text = wifi_listen_modes[0].details_text,
    },
    {
        .label = "Pineapple Detect",
        .command = "pineap\n",
        .details_header = "Pineapple Detection",
        .details_text = "Detects WiFi Pineapple devices\n",
    },
    {
        .label = "Stop Pineapple Detect",
        .command = "pineap -s\n",
        .details_header = "Stop Pineapple Detect",
        .details_text = "Stops Pineapple detection mode.",
    },
    {
        .label = "Flock Detection",
        .command = "flockscan\n",
        .details_header = "Flock Detection",
        .details_text = "Detect Flock Safety cameras,\n"
                        "extended battery units, and\n"
                        "Penguin surveillance devices\n"
                        "on 2.4 GHz Wi-Fi.\n"
                        "Uses OUI matching, wildcard\n"
                        "probe detection, and SSID\n"
                        "keyword matching.\n",
    },
    {
        .label = "Flock List",
        .command = "flocklist\n",
        .details_header = "Flock List",
        .details_text = "List detected Flock Safety\n"
                        "surveillance devices.\n"
                        "Run after Flock Detection.\n",
    },
    {
        .label = "Stop Flock Detection",
        .command = "flockstop\n",
        .details_header = "Stop Flock Detection",
        .details_text = "Stops Flock Safety camera\n"
                        "detection scanning.\n",
    },
    {
        .label = "Channel Congestion",
        .command = "congestion\n",
        .details_header = "Channel Congestion",
        .details_text = "Display Wi-Fi channel\n"
                        "congestion chart.\n",
    },
    {
        .label = "Scan Ports",
        .command = "scanports",
        .needs_input = true,
        .input_text = "local or IP [options]",
        .details_header = "Port Scanner",
        .details_text = "Scan ports on local net\n"
                        "or specific IP.\n"
                        "Options: -C, -A, range\n"
                        "Ex: local -C\n"
                        "Ex: 192.168.1.1 80-1000",
    },
    {
        .label = "ARP Scan",
        .command = "scanarp\n",
        .details_header = "ARP Scan",
        .details_text = "Initiates an ARP scan on the local network to discover hosts:\n"
                        "- Sends ARP requests across the subnet\n"
                        "- Shows responding IP/MAC pairs\n"
                        "Requires WiFi connection.\n",
    },
    {
        .label = "Scan SSH",
        .command = "scanssh",
        .needs_input = true,
        .input_text = "IP",
        .details_header = "SSH Scan",
        .details_text = "Initiate an SSH port/service scan against the target IP:\n"
                        "- Provide an IP address (e.g., 192.168.1.10)\n"
                        "- Scans common SSH ports and reports responses\n"
                        "- Requires network connectivity\n\n",
    },
    {
        .label = "Full Environment Sweep",
        .command = "sweep\n",
        .details_header = "Environment Sweep",
        .details_text = "Full sweep: WiFi APs, stations,\n"
                        "and BLE devices.\n"
                        "Saves CSV report to SD.\n"
                        "Uses default timing for scan.\n",
    },
    {
        .label = "Combined AP+STA Scan",
        .command = "scanall",
        .needs_input = true,
        .input_text = "Seconds",
        .details_header = "Scan All",
        .details_text = "Combined AP and station scan\n"
                        "with summary report.\n"
                        "Optionally specify duration.\n",
    },
    {
        .label = "Track Selected AP",
        .command = "trackap\n",
        .details_header = "Track AP Signal",
        .details_text = "Track selected AP signal\n"
                        "strength (RSSI) in real-time.\n"
                        "Select an AP first.\n",
    },
    {
        .label = "Track Selected Station",
        .command = "tracksta\n",
        .details_header = "Track Station Signal",
        .details_text = "Track selected station signal\n"
                        "strength (RSSI) in real-time.\n"
                        "Select a station first.\n",
    },
    {
        .label = "NetBIOS Scan",
        .command = "netbiosscan subnet",
        .needs_input = true,
        .input_text = "Subnet (e.g. 192.168.1.)",
        .details_header = "NetBIOS Scan",
        .details_text = "Scan a subnet for NetBIOS\n"
                        "names/hosts.\n"
                        "Enter subnet prefix, e.g.\n"
                        "192.168.1. or 192.168.1.0\n",
    },
    {
        .label = "HTTP Banner Scan",
        .command = "httpbannerscan subnet",
        .needs_input = true,
        .input_text = "Subnet (e.g. 192.168.1.)",
        .details_header = "HTTP Banner Scan",
        .details_text = "Grab HTTP banners from hosts\n"
                        "on a subnet.\n"
                        "Enter subnet prefix, e.g.\n"
                        "192.168.1. or 192.168.1.0\n",
    },
    {
        .label = "SNMP Probe",
        .command = "snmpprobe subnet",
        .needs_input = true,
        .input_text = "Subnet (e.g. 192.168.1.)",
        .details_header = "SNMP Probe",
        .details_text = "Probe a subnet for SNMP\n"
                        "enabled devices.\n"
                        "Enter subnet prefix, e.g.\n"
                        "192.168.1. or 192.168.1.0\n",
    },
    {
        .label = "WPA3 Compliance Check",
        .command = "wpa3check\n",
        .details_header = "WPA3 Compliance",
        .details_text = "Check selected AP for WPA3\n"
                        "compliance and SAE support.\n"
                        "Select an AP first.\n",
    },
    {
        .label = "Stop Listen Probes",
        .command = "listenprobes stop\n",
        .details_header = "Stop Listening",
        .details_text = "Stops the probe listener.",
    },
    {
        .label = "Stop Scan",
        .command = "stopscan\n",
        .details_header = "Stop Scan",
        .details_text = "Stops AP or Station scan.",
    },
};

static const MenuCommand wifi_capture_commands[] = {
    {
        .label = "< Sniff WPS >",
        .command = "capture -wps\n",
        .capture_prefix = "wps_capture",
        .file_ext = "pcap",
        .folder = GHOST_ESP_APP_FOLDER_PCAPS,
        .details_header = "Variable Sniff",
        .details_text = "Use Left/Right to change:\n"
                        "- WPS traffic\n"
                        "- Raw packets\n"
                        "- Probe requests\n"
                        "- Deauth frames\n"
                        "- Beacon frames\n"
                        "- EAPOL/Handshakes\n",
    },
};

static const MenuCommand wifi_attack_commands[] = {
    {
        .label = "< Beacon Spam (List) >",
        .command = "beaconspam -l\n",
        .needs_input = false,
        .input_text = "SSID Name",
        .details_header = "Variable Beacon Spam",
        .details_text = "Use Left/Right to change:\n"
                        "- List mode\n"
                        "- Random names\n"
                        "- Rickroll mode\n"
                        "- Custom SSID\n"
                        "Range: ~50-100m\n",
    },
    {
        .label = "Deauth",
        .command = "attack -d\n",
        .details_header = "Deauth Attack",
        .details_text = "Sends deauth frames to\n"
                        "disconnect clients from\n"
                        "selected network.\n"
                        "Range: ~50-100m\n",
    },
    {
        .label = "EAPOL Logoff",
        .command = "attack -e\n",
        .details_header = "EAPOL Logoff Attack",
        .details_text = "Sends EAPOL logoff frames to\n"
                        "disconnect clients.",
    },
    {
        .label = "SAE Handshake Flood",
        .command = "saeflood",
        .needs_input = true,
        .input_text = "SAE PSK",
        .details_header = "SAE Flood Attack",
        .details_text = "Floods WPA3 networks with\nSAE handshakes using the\n"
                        "provided PSK. Select a\nWPA3 AP first.",
    },
    {
        .label = "Stop SAE Flood",
        .command = "stopsaeflood\n",
        .details_header = "Stop SAE Flood",
        .details_text = "Stops active SAE flood and\n"
                        "password spray attacks.",
    },
    {
        .label = "SAE Flood Help",
        .command = "saefloodhelp\n",
        .details_header = "SAE Flood Help",
        .details_text = "Shows usage guidance for\n"
                        "SAE flood operations.",
    },
    {
        .label = "Karma Start",
        .command = "karma start\n",
        .details_header = "Karma Rogue AP",
        .details_text = "Replies to probe requests\n"
                        "with saved SSIDs.",
    },
    {
        .label = "Karma Start (Custom)",
        .command = "karma start",
        .needs_input = true,
        .input_text = "SSID [SSID...]",
        .details_header = "Karma Rogue AP (Custom)",
        .details_text = "Replies to probe requests\n"
                        "using SSIDs you provide\n"
                        "or saved entries.",
    },
    {
        .label = "Karma Stop",
        .command = "karma stop\n",
        .details_header = "Stop Karma Rogue AP",
        .details_text = "Stops the active Karma\n"
                        "rogue AP responder.",
    },
    {
        .label = "DHCP Starve Start",
        .command = "dhcpstarve",
        .needs_input = true,
        .input_text = "start [threads]",
        .details_header = "DHCP Starve Attack",
        .details_text = "Exhausts DHCP server's IP pool.\n"
                        "Input: start [threads]\n"
                        "e.g., 'start' or 'start 5'",
    },
    {
        .label = "DHCP Starve Stop",
        .command = "dhcpstarve stop\n",
        .details_header = "Stop DHCP Starve",
        .details_text = "Stops the DHCP starvation attack.",
    },
    {
        .label = "Stop Deauth/SAE/EAPOL",
        .command = "stopdeauth\n",
        .details_header = "Stop Attacks",
        .details_text = "Stops Deauth, SAE Flood,\n"
                        "and EAPOL Logoff attacks.",
    },
    {
        .label = "Add SSID to Beacon List",
        .command = "beaconadd",
        .needs_input = true,
        .input_text = "SSID",
        .details_header = "Add to Beacon List",
        .details_text = "Add an SSID to the list used\n"
                        "by Beacon List Spam.",
    },
    {
        .label = "Remove SSID from Beacon List",
        .command = "beaconremove",
        .needs_input = true,
        .input_text = "SSID",
        .details_header = "Remove from Beacon List",
        .details_text = "Remove an SSID from the\n"
                        "beacon spam list.",
    },
    {
        .label = "Clear Beacon List",
        .command = "beaconclear\n",
        .details_header = "Clear Beacon List",
        .details_text = "Clears all SSIDs from the\n"
                        "beacon spam list.",
    },
    {
        .label = "Show Beacon List",
        .command = "beaconshow\n",
        .details_header = "Show Beacon List",
        .details_text = "Displays all SSIDs in the\n"
                        "beacon spam list.",
    },
    {
        .label = "Start Beacon List Spam",
        .command = "beaconspamlist\n",
        .details_header = "Beacon List Spam",
        .details_text = "Starts beacon spam using the\n"
                        "custom list of SSIDs.",
    },
    {
        .label = "Stop Beacon Spam",
        .command = "stopspam\n",
        .details_header = "Stop Beacon Spam",
        .details_text = "Stops any active beacon spam.",
    },
};

// WiFi Network sub-category: Evil Portal
static const MenuCommand wifi_network_portal_commands[] = {
    {
        .label = "Evil Portal",
        .command = "startportal",
        .needs_input = true,
        .input_text = "<filepath> <SSID> <PSK (leave blank for open)>",
        .details_header = "Evil Portal",
        .details_text = "Captive portal for\n"
                        "credential harvest.\n"
                        "Configure in WebUI:\n"
                        "- Portal settings\n"
                        "- Landing page\n",
    },
    {
        .label = "List Portals",
        .command = "listportals\n",
        .details_header = "List Portals",
        .details_text = "Show all available HTML portals\non the SD card.",
    },
    {
        .label = "Set Evil Portal HTML",
        .command = "set_evil_portal_html",
        .needs_input = true,
        .input_text = "HTML File",
        .details_header = "Set Evil Portal HTML",
        .details_text = "Select and send an HTML\n"
                        "file to the ESP32 for\n"
                        "the evil portal.\n\n",
    },
    {
        .label = "Clear Evil Portal HTML",
        .command = "evilportal -c clear\n",
        .details_header = "Clear Evil Portal",
        .details_text = "Restores the default portal\n"
                        "landing page on the ESP.",
    },
    {
        .label = "Stop Evil Portal",
        .command = "stopportal\n",
        .details_header = "Stop Evil Portal",
        .details_text = "Stops the Evil Portal.",
    },
};

// WiFi Network sub-category: WiFi Connection
static const MenuCommand wifi_network_conn_commands[] = {
    {
        .label = "Connect To WiFi",
        .command = "connect",
        .needs_input = true,
        .input_text = "SSID",
        .details_header = "WiFi Connect",
        .details_text = "Connect ESP to WiFi:\n"
                        "Enter SSID followed by password.\n\n",
    },
    {
        .label = "Connect to Saved WiFi",
        .command = "connect\n",
        .details_header = "Connect (Saved)",
        .details_text = "Connect to the previously saved WiFi credentials on the ESP.\n"
                        "No input required.\n\n",
    },
    {
        .label = "Disconnect WiFi",
        .command = "disconnect\n",
        .details_header = "Disconnect",
        .details_text = "Disconnects from the current WiFi network on the ESP.\n"
                        "No input required.\n",
    },
    {
        .label = "WiFi Status",
        .command = "wifistatus\n",
        .details_header = "WiFi Status",
        .details_text = "Show current WiFi connection\n"
                        "status (SSID, IP, RSSI).\n",
    },
};

// WiFi Network sub-category: Network Scan
static const MenuCommand wifi_network_scan_commands[] = {
    {
        .label = "Scan Local Network",
        .command = "scanlocal\n",
        .needs_confirmation = true,
        .confirm_header = "Local Network Scan",
        .confirm_text = "Make sure you've connected\nto WiFi first via "
                        "the\n'Connect to WiFi' option.\n",
        .details_header = "Network Scanner",
        .details_text = "Scans local network for:\n"
                        "- Printers\n"
                        "- Smart devices\n"
                        "- Cast devices\n"
                        "- Requires WiFi connection\n\n",
    },
    {
        .label = "Scan Ports",
        .command = "scanports",
        .needs_input = true,
        .input_text = "local or IP [options]",
        .details_header = "Port Scanner",
        .details_text = "Scan ports on local net\n"
                        "or specific IP.\n"
                        "Options: -C, -A, range\n"
                        "Ex: local -C\n"
                        "Ex: 192.168.1.1 80-1000",
    },
    {
        .label = "ARP Scan",
        .command = "scanarp\n",
        .details_header = "ARP Scan",
        .details_text = "Initiates an ARP scan on the local network to discover hosts:\n"
                        "- Sends ARP requests across the subnet\n"
                        "- Shows responding IP/MAC pairs\n"
                        "Requires WiFi connection.\n",
    },
    {
        .label = "Scan SSH",
        .command = "scanssh",
        .needs_input = true,
        .input_text = "IP",
        .details_header = "SSH Scan",
        .details_text = "Initiate an SSH port/service scan against the target IP:\n"
                        "- Provide an IP address (e.g., 192.168.1.10)\n"
                        "- Scans common SSH ports and reports responses\n"
                        "- Requires network connectivity\n\n",
    },
    {
        .label = "NetBIOS Scan",
        .command = "netbiosscan subnet",
        .needs_input = true,
        .input_text = "Subnet (e.g. 192.168.1.)",
        .details_header = "NetBIOS Scan",
        .details_text = "Scan a subnet for NetBIOS\n"
                        "names/hosts.\n"
                        "Enter subnet prefix, e.g.\n"
                        "192.168.1. or 192.168.1.0\n",
    },
    {
        .label = "HTTP Banner Scan",
        .command = "httpbannerscan subnet",
        .needs_input = true,
        .input_text = "Subnet (e.g. 192.168.1.)",
        .details_header = "HTTP Banner Scan",
        .details_text = "Grab HTTP banners from hosts\n"
                        "on a subnet.\n"
                        "Enter subnet prefix, e.g.\n"
                        "192.168.1. or 192.168.1.0\n",
    },
    {
        .label = "SNMP Probe",
        .command = "snmpprobe subnet",
        .needs_input = true,
        .input_text = "Subnet (e.g. 192.168.1.)",
        .details_header = "SNMP Probe",
        .details_text = "Probe a subnet for SNMP\n"
                        "enabled devices.\n"
                        "Enter subnet prefix, e.g.\n"
                        "192.168.1. or 192.168.1.0\n",
    },
};

// WiFi Network sub-category: IoT Control
static const MenuCommand wifi_network_iot_commands[] = {
    {
        .label = "Cast Random Video",
        .command = "dialconnect\n",
        .needs_confirmation = true,
        .confirm_header = "Cast Video",
        .confirm_text = "Make sure you've connected\nto WiFi first via "
                        "the\n'Connect to WiFi' option.\n",
        .details_header = "Video Cast",
        .details_text = "Casts random videos\n"
                        "to nearby Cast/DIAL\n"
                        "enabled devices.\n"
                        "Range: ~50m\n\n",
    },
    {
        .label = "Printer Power",
        .command = "powerprinter\n",
        .needs_confirmation = true,
        .confirm_header = "Printer Power",
        .confirm_text = "You need to configure\n settings in the WebUI\n for "
                        "this command.\n",
        .details_header = "WiFi Printer",
        .details_text = "Control power state\n"
                        "of network printers.\n"
                        "Configure in WebUI:\n"
                        "- Printer IP/Port\n"
                        "- Protocol type\n\n",
    },
    {
        .label = "TP-Link Smart Plug",
        .command = "tplinktest",
        .needs_input = true,
        .input_text = "on | off | loop",
        .details_header = "TP-Link Control",
        .details_text = "Control TP-Link smart plugs\n"
                        "on the local network.",
    },
};

// WiFi Network sub-category: WebUI
static const MenuCommand wifi_network_webui_commands[] = {
    {
        .label = "Set WebUI Creds",
        .command = "apcred",
        .needs_input = true,
        .input_text = "MySSID MyPassword",
        .details_header = "Set AP Credentials",
        .details_text = "Set custom WebUI AP:\n"
                        "Format:\nMySSID MyPassword\n"
                        "Example: GhostNet,spooky123\n",
    },
    {
        .label = "Reset WebUI Creds",
        .command = "apcred -r\n",
        .needs_confirmation = true,
        .confirm_header = "Reset AP Credentials",
        .confirm_text = "Reset WebUI AP to\n"
                        "default credentials?\n"
                        "SSID: GhostNet\n"
                        "Password: GhostNet\n",
        .details_header = "Reset AP Credentials",
        .details_text = "Restores default WebUI AP:\n"
                        "SSID: GhostNet\n"
                        "Password: GhostNet\n"
                        "Requires ESP reboot\n",
    },
    {
        .label = "WebUI AP",
        .command = "webuiap",
        .needs_input = true,
        .input_text = "on | off | toggle | status",
        .details_header = "WebUI Access Point",
        .details_text = "Control the dedicated WebUI\n"
                        "access point.\n"
                        "Options: on, off, toggle,\n"
                        "status.\n",
    },
};

// WiFi Settings sub-category: LED & RGB
static const MenuCommand wifi_settings_led_commands[] = {
    {
        .label = "< LED: Rainbow >",
        .command = "rgbmode rainbow\n",
        .confirm_header = "LED Effects",
        .details_header = "LED Effects",
        .details_text = "Control LED effects:\n"
                        "- rainbow, police, strobe, off, or fixed colors\n"
                        "Cycle with Left/Right to select an effect\n",
    },
    {
        .label = "Set RGB Pins",
        .command = "setrgbpins",
        .needs_input = true,
        .input_text = "<red> <green> <blue>",
        .details_header = "Set RGB Pins",
        .details_text = "Change RGB LED pins.\n"
                        "Requires restart.\n"
                        "Use same value for all\n"
                        "pins for single-pin LED.",
    },
    {
        .label = "Set RGB Profile",
        .command = "setrgbmode",
        .needs_input = true,
        .input_text = "normal|rainbow|stealth",
        .details_header = "Set RGB Profile",
        .details_text = "Save the default LED mode\n"
                        "used after reboot.",
    },
    {
        .label = "Set NeoPixel Brightness",
        .command = "setneopixelbrightness",
        .needs_input = true,
        .input_text = "0-100",
        .details_header = "NeoPixel Brightness",
        .details_text = "Adjust NeoPixel brightness\n"
                        "from 0 to 100%.",
    },
    {
        .label = "Get NeoPixel Brightness",
        .command = "getneopixelbrightness\n",
        .details_header = "NeoPixel Brightness",
        .details_text = "Displays the current NeoPixel\n"
                        "brightness level.",
    },
    {
        .label = "Set RGB LED Count",
        .command = "setrgbcount",
        .needs_input = true,
        .input_text = "1-512",
        .details_header = "Set RGB LED Count",
        .details_text = "Set the number of RGB LEDs\n"
                        "connected (1-512).\n"
                        "Effects will span the correct\n"
                        "length. Reinitializes if pins\n"
                        "are already configured.\n",
    },
};

// WiFi Settings sub-category: SD Card
static const MenuCommand wifi_settings_sd_commands[] = {
    {
        .label = "Show SD Pin Config",
        .command = "sd_config",
        .details_header = "SD Pin Config",
        .details_text = "Show current SD GPIO\n"
                        "pin configuration for\n"
                        "MMC and SPI modes.",
    },
    {
        .label = "Set SD Pins (MMC)",
        .command = "sd_pins_mmc",
        .needs_input = true,
        .input_text = "<clk> <cmd> <d0..d3>",
        .details_header = "Set SD Pins (MMC)",
        .details_text = "Set GPIO pins for SDMMC.\n"
                        "Requires restart.\n"
                        "Only if firmware built\n"
                        "for SDMMC mode.",
    },
    {
        .label = "Set SD Pins (SPI)",
        .command = "sd_pins_spi",
        .needs_input = true,
        .input_text = "<cs> <clk> <miso> <mosi>",
        .details_header = "Set SD Pins (SPI)",
        .details_text = "Set GPIO pins for SPI.\n"
                        "Requires restart.\n"
                        "Only if firmware built\n"
                        "for SPI mode.",
    },
    {
        .label = "Save SD Pin Config",
        .command = "sd_save_config",
        .needs_confirmation = true,
        .confirm_header = "Save SD Config",
        .confirm_text = "Save current SD pin\n"
                        "config to SD card?\n"
                        "Requires SD mounted.",
        .details_header = "Save SD Pin Config",
        .details_text = "Save current SD pin\n"
                        "config (both modes) to\n"
                        "SD card (sd_config.conf).",
    },
};

// WiFi Settings sub-category: Settings Management
static const MenuCommand wifi_settings_mgmt_commands[] = {
    {
        .label = "Settings List",
        .command = "settings list\n",
        .details_header = "List Settings",
        .details_text = "Shows available configuration\n"
                        "keys and descriptions.",
    },
    {
        .label = "Settings Help",
        .command = "settings help\n",
        .details_header = "Settings Help",
        .details_text = "Displays CLI usage for\n"
                        "settings commands.",
    },
    {
        .label = "Settings Get",
        .command = "settings get",
        .needs_input = true,
        .input_text = "Key",
        .details_header = "Get Setting",
        .details_text = "Read the current value for\n"
                        "a configuration key.",
    },
    {
        .label = "Settings Set",
        .command = "settings set",
        .needs_input = true,
        .input_text = "Key Value",
        .details_header = "Set Setting",
        .details_text = "Update a configuration key\n"
                        "with a new value.",
    },
    {
        .label = "Settings Reset (Key)",
        .command = "settings reset",
        .needs_input = true,
        .input_text = "Key",
        .details_header = "Reset Setting",
        .details_text = "Reset a specific configuration\n"
                        "key to defaults.",
    },
    {
        .label = "Settings Reset (All)",
        .command = "settings reset\n",
        .details_header = "Reset Settings",
        .details_text = "Restore all configuration\n"
                        "keys to defaults.",
    },
};

// WiFi Settings sub-category: Device & Debug
static const MenuCommand wifi_settings_device_commands[] = {
    {
        .label = "Chip Info",
        .command = "chipinfo\n",
        .details_header = "Chip Info",
        .details_text = "Show chip and memory info.",
    },
    {
        .label = "Show Time",
        .command = "time\n",
        .details_header = "Device Time",
        .details_text = "Show the current device time.",
    },
    {
        .label = "Reboot Device",
        .command = "reboot\n",
        .needs_confirmation = true,
        .confirm_header = "Reboot Device",
        .confirm_text = "Are you sure you want to reboot?",
        .details_header = "Reboot",
        .details_text = "Restart the ESP device.",
    },
    {
        .label = "Enable/Disable AP",
        .command = "apenable",
        .needs_input = true,
        .input_text = "on | off",
        .details_header = "AP Enable/Disable",
        .details_text = "Enable or disable the Access Point\nacross reboots.",
    },
    {
        .label = "Load Config",
        .command = "loadconfig\n",
        .details_header = "Load Config",
        .details_text = "Reload configuration from the\n"
                        "SD card.",
    },
    {
        .label = "Identify Device",
        .command = "identify\n",
        .details_header = "Identify Device",
        .details_text = "Trigger a visual/audible\n"
                        "identify signal on the ESP.",
    },
    {
        .label = "Coredump Dump",
        .command = "coredump dump\n",
        .details_header = "Coredump Dump",
        .details_text = "Read out the stored crash\n"
                        "coredump from the partition.",
    },
    {
        .label = "Erase Coredump",
        .command = "coredump erase\n",
        .needs_confirmation = true,
        .confirm_header = "Erase Coredump",
        .confirm_text = "Permanently erase the stored\n"
                        "crash coredump?\n"
                        "This cannot be undone.",
        .details_header = "Erase Coredump",
        .details_text = "Erase the crash coredump\n"
                        "partition on the ESP.",
    },
    {
        .label = "Force Crash",
        .command = "crash\n",
        .needs_confirmation = true,
        .confirm_header = "Force Crash",
        .confirm_text = "This intentionally crashes\n"
                        "the ESP to test coredump.\n"
                        "Continue?",
        .details_header = "Force Crash",
        .details_text = "Intentionally crash the ESP\n"
                        "to generate a coredump for\n"
                        "debugging.",
    },
};

// WiFi Settings sub-category: Misc
static const MenuCommand wifi_settings_misc_commands[] = {
    {
        .label = "Set Timezone",
        .command = "timezone",
        .needs_input = true,
        .input_text = "TZ String",
        .details_header = "Set Timezone",
        .details_text = "Set timezone for the clock.\n"
                        "e.g. 'EST5EDT,M3.2.0,M11.1.0'",
    },
    {
        .label = "Set Web Auth",
        .command = "webauth",
        .needs_input = true,
        .input_text = "on | off",
        .details_header = "Set Web Auth",
        .details_text = "Enable or disable Web\n"
                        "UI authentication.",
    },
    {
        .label = "Set WiFi Country",
        .command = "setcountry",
        .needs_input = true,
        .input_text = "Country Code (e.g. US)",
        .details_header = "Set WiFi Country",
        .details_text = "Set the WiFi country code.\n"
                        "May require ESP32-C5.",
    },
    {
        .label = "Show Help",
        .command = "help\n",
        .details_header = "Help",
        .details_text = "Show complete command list.",
    },
};

static const MenuCommand wifi_stop_command = {
    .label = "Stop All WiFi",
    .command = "stop\n",
    .details_header = "Stop WiFi Operations",
    .details_text = "Stops all active WiFi\n"
                    "operations including:\n"
                    "- Scanning\n"
                    "- Beacon Spam\n"
                    "- Deauth Attacks\n"
                    "- Packet Captures\n"
                    "- Evil Portal\n",
};

static const MenuCommand status_idle_commands[] = {
    {
        .label = "Life (Game of Life)",
        .command = "statusidle set life\n",
        .details_header = "Life Animation",
        .details_text = "Set idle status display to\n"
                        "Game of Life animation.",
    },
    {
        .label = "Ghost (Sprite)",
        .command = "statusidle set ghost\n",
        .details_header = "Ghost Animation",
        .details_text = "Set idle status display to\n"
                        "ghost sprite animation.",
    },
    {
        .label = "Starfield",
        .command = "statusidle set starfield\n",
        .details_header = "Starfield Animation",
        .details_text = "Set idle status display to\n"
                        "starfield effect.",
    },
    {
        .label = "HUD",
        .command = "statusidle set hud\n",
        .details_header = "HUD Animation",
        .details_text = "Set idle status display to\n"
                        "HUD-style overlay.",
    },
    {
        .label = "Matrix",
        .command = "statusidle set matrix\n",
        .details_header = "Matrix Animation",
        .details_text = "Set idle status display to\n"
                        "Matrix-style rain effect.",
    },
    {
        .label = "Multiple Ghosts",
        .command = "statusidle set ghosts\n",
        .details_header = "Ghosts Animation",
        .details_text = "Set idle status display to\n"
                        "floating ghosts effect.",
    },
    {
        .label = "Spiral",
        .command = "statusidle set spiral\n",
        .details_header = "Spiral Animation",
        .details_text = "Set idle status display to\n"
                        "spiral pattern effect.",
    },
    {
        .label = "Falling Leaves",
        .command = "statusidle set leaves\n",
        .details_header = "Falling Leaves Animation",
        .details_text = "Set idle status display to\n"
                        "falling leaves effect.",
    },
    {
        .label = "Bouncing Text",
        .command = "statusidle set bouncing\n",
        .details_header = "Bouncing Text Animation",
        .details_text = "Set idle status display to\n"
                        "bouncing text effect.",
    },
};

// BLE menu command definitions
// BLE Scanning sub-category: Flipper & Skimmer Detection
static const MenuCommand ble_scan_detect_commands[] = {
    {
        .label = "Skimmer Detection",
        .command = "capture -skimmer\n",
        .capture_prefix = "skimmer_scan",
        .file_ext = "pcap",
        .folder = GHOST_ESP_APP_FOLDER_PCAPS,
        .details_header = "Skimmer Scanner",
        .details_text = "Detects potential\n"
                        "card skimmers by\n"
                        "analyzing BLE\n"
                        "signatures and\n"
                        "known patterns.\n",
    },
    {
        .label = "Find the Flippers",
        .command = "blescan -f\n",
        .details_header = "Flipper Scanner",
        .details_text = "Scans for Flippers:\n"
                        "- Device name\n"
                        "- BT address\n"
                        "- Signal level\n"
                        "Range: ~50m\n",
    },
    {
        .label = "List Flippers",
        .command = "listflippers\n",
        .details_header = "List Flippers",
        .details_text = "List discovered Flipper Devices\n"
                        "in range.",
    },
    {
        .label = "Select Flipper to Track",
        .command = "selectflipper",
        .needs_input = true,
        .input_text = "Flipper Number",
        .details_header = "Select Flipper to Track",
        .details_text = "Select a Flipper by number to track RSSI strength.",
    },
};

// BLE Scanning sub-category: AirTag
static const MenuCommand ble_scan_airtag_commands[] = {
    {
        .label = "AirTag Scanner",
        .command = "blescan -a\n",
        .details_header = "AirTag Scanner",
        .details_text = "Detects nearby Apple\n"
                        "AirTags and shows:\n"
                        "- Device ID\n"
                        "- Signal strength\n"
                        "- Last seen time\n",
    },
    {
        .label = "List AirTags",
        .command = "listairtags\n",
        .details_header = "List AirTags",
        .details_text = "List discovered AirTags.",
    },
    {
        .label = "Select AirTag",
        .command = "select -airtag",
        .needs_input = true,
        .input_text = "AirTag Number",
        .details_header = "Select AirTag",
        .details_text = "Target an AirTag by number\nfrom the scan list.",
    },
};

// BLE Scanning sub-category: GATT
static const MenuCommand ble_scan_gatt_commands[] = {
    {
        .label = "Scan GATT Devices",
        .command = "blescan -g\n",
        .details_header = "GATT Device Scanner",
        .details_text = "Scan for connectable BLE\n"
                        "devices for GATT enumeration.\n"
                        "Shows device addresses and\n"
                        "connection capability.\n",
    },
    {
        .label = "List GATT Devices",
        .command = "listgatt\n",
        .details_header = "List GATT Devices",
        .details_text = "List discovered GATT devices\n"
                        "with tracker type detection.\n",
    },
    {
        .label = "Select GATT Device",
        .command = "selectgatt",
        .needs_input = true,
        .input_text = "Device Index",
        .details_header = "Select GATT Device",
        .details_text = "Select a GATT device by index\n"
                        "for enumeration or tracking.\n",
    },
    {
        .label = "Enumerate GATT Services",
        .command = "enumgatt\n",
        .details_header = "Enumerate GATT",
        .details_text = "Connect to selected device\n"
                        "and enumerate its GATT\n"
                        "services, characteristics,\n"
                        "and descriptors.\n",
    },
    {
        .label = "Track GATT Device",
        .command = "trackgatt\n",
        .details_header = "Track GATT Device",
        .details_text = "Track selected GATT device\n"
                        "using real-time RSSI signal\n"
                        "strength monitoring.\n",
    },
};

// BLE Scanning sub-category: Advertisers & Traffic
static const MenuCommand ble_scan_adv_commands[] = {
    {
        .label = "View All BLE Traffic",
        .command = "blescan -r\n",
        .details_header = "BLE Raw Traffic",
        .details_text = "View all Bluetooth Low Energy\ntraffic in range.",
    },
    {
        .label = "Scan Advertisers",
        .command = "blescan -adv\n",
        .details_header = "Scan Advertisers",
        .details_text = "Start a parsed BLE\n"
                        "advertiser scan.\n"
                        "Run 'List Advertisers'\n"
                        "afterwards for results.\n",
    },
    {
        .label = "List Advertisers",
        .command = "listadv\n",
        .details_header = "List Advertisers",
        .details_text = "List discovered BLE\n"
                        "advertisers in range.\n",
    },
    {
        .label = "BLE Bridge",
        .command = "blebridge",
        .needs_input = true,
        .input_text = "start | stop | status | pair <peer>",
        .details_header = "BLE GhostLink Bridge",
        .details_text = "Start/stop/status the BLE\n"
                        "GhostLink bridge, or pair\n"
                        "with a peer by name.\n",
    },
    {
        .label = "Stop BLE Scanning",
        .command = "blescan -s\n",
        .details_header = "Stop BLE Scan",
        .details_text = "Stops any active BLE scanning.",
    },
};

static const MenuCommand ble_capture_commands[] = {
    {
        .label = "BLE Raw Capture",
        .command = "capture -ble\n",
        .capture_prefix = "ble_raw_capture",
        .file_ext = "pcap",
        .folder = GHOST_ESP_APP_FOLDER_PCAPS,
        .details_header = "BLE Raw Capture",
        .details_text = "Captures raw BLE\n"
                        "traffic and data.\n"
                        "Range: ~10-30m\n",
    },
};

static const MenuCommand ble_attack_commands[] = {
    {
        .label = "< BLE Spam (Apple) >",
        .command = "blespam -apple\n",
        .details_header = "Variable BLE Spam",
        .details_text = "Use Left/Right to change:\n"
                        "- Apple device spam\n"
                        "- Microsoft Swift Pair\n"
                        "- Samsung Galaxy Watch\n"
                        "- Google Fast Pair\n"
                        "- Random spam (all types)\n"
                        "Range: ~50m\n",
    },
    {
        .label = "Spoof Selected AirTag",
        .command = "spoofairtag\n",
        .details_header = "Spoof AirTag",
        .details_text = "Spoof the selected AirTag.",
    },
    {
        .label = "Stop BLE Spam",
        .command = "blespam -s\n",
        .details_header = "Stop BLE Spam",
        .details_text = "Stops BLE advertisement\n"
                        "spam attacks.",
    },
    {
        .label = "Stop AirTag Spoof",
        .command = "stopspoof\n",
        .details_header = "Stop Spoofing",
        .details_text = "Stops AirTag spoofing.",
    },
};

static const MenuCommand ble_stop_command = {
    .label = "Stop All BLE",
    .command = "stop\n",
    .details_header = "Stop BLE Operations",
    .details_text = "Stops all active BLE\n"
                    "operations including:\n"
                    "- BLE Scanning\n"
                    "- Skimmer Detection\n"
                    "- Packet Captures\n"
                    "- Device Detection\n",
};

// GPS sub-category: GPS Config
static const MenuCommand gps_config_commands[] = {
    {
        .label = "GPS Info",
        .command = "gpsinfo\n",
        .details_header = "GPS Information",
        .details_text = "Shows GPS details:\n"
                        "- Position (Lat/Long)\n"
                        "- Altitude & Speed\n"
                        "- Direction & Quality\n"
                        "- Satellite Status\n",
    },
    {
        .label = "Set GPS Pin",
        .command = "gpspin",
        .needs_input = true,
        .input_text = "Pin Number",
        .details_header = "Set GPS RX Pin",
        .details_text = "Set the GPS RX pin for\n"
                        "external GPS modules.\n"
                        "Setting persists to NVS.\n"
                        "Restart GPS commands to apply.\n",
    },
    {
        .label = "View GPS Pin",
        .command = "gpspin\n",
        .details_header = "View GPS RX Pin",
        .details_text = "Shows current GPS RX pin\n"
                        "configuration for external\n"
                        "GPS modules.\n",
    },
    {
        .label = "Set GPS Baud",
        .command = "gpsbaud",
        .needs_input = true,
        .input_text = "Baud Rate (e.g. 9600)",
        .details_header = "Set GPS Baud Rate",
        .details_text = "Set the GPS module baud\n"
                        "rate. Persists to NVS.\n"
                        "Restart GPS to apply.\n",
    },
};

// GPS sub-category: Wardriving
static const MenuCommand gps_wardrive_commands[] = {
    {
        .label = "WiGLE Auto Upload",
        .command = "wigle auto",
        .needs_input = true,
        .input_text = "on | off",
        .details_header = "WiGLE Auto Upload",
        .details_text = "Enable or disable automatic\n"
                        "upload of wardrive data to\n"
                        "WiGLE.\n",
    },
    {
        .label = "WiGLE Donate",
        .command = "wigle donate",
        .needs_input = true,
        .input_text = "on | off",
        .details_header = "WiGLE Donate Mode",
        .details_text = "Toggle WiGLE donate mode for\n"
                        "uploaded observations.\n",
    },
    {
        .label = "Start Wardriving",
        .command = "startwd\n",
        .capture_prefix = "wardrive_wifi",
        .file_ext = "csv",
        .folder = GHOST_ESP_APP_FOLDER_WARDRIVE,
        .details_header = "Wardrive Mode",
        .details_text = "Maps WiFi networks:\n"
                        "- Network info\n"
                        "- GPS location\n"
                        "- Signal levels\n"
                        "Saves as CSV\n",
    },
    {
        .label = "BLE Wardriving",
        .command = "blewardriving\n",
        .capture_prefix = "wardrive_ble",
        .file_ext = "csv",
        .folder = GHOST_ESP_APP_FOLDER_WARDRIVE,
        .details_header = "BLE Wardriving",
        .details_text = "Maps BLE devices:\n"
                        "- Device info\n"
                        "- GPS location\n"
                        "- Signal levels\n"
                        "Saves as CSV\n",
    },
    {
        .label = "Stop BLE Wardriving",
        .command = "blewardriving -s\n",
        .details_header = "Stop BLE Wardrive",
        .details_text = "Stops BLE wardriving capture\n"
                        "and logging.",
    },
    {
        .label = "Stop All GPS",
        .command = "stop\n",
        .details_header = "Stop GPS Operations",
        .details_text = "Stops all active GPS\n"
                        "operations including:\n"
                        "- GPS Info Updates\n"
                        "- WiFi Wardriving\n"
                        "- BLE Wardriving\n",
    },
};

// Aerial Detector menu command definitions - all in one menu
static const MenuCommand aerial_commands[] = {
    {
        .label = "Start Scan (30s)",
        .command = "aerialscan 30\n",
        .details_header = "Scan for Drones",
        .details_text = "Scans for aerial devices:\n"
                        "- OpenDroneID (WiFi/BLE)\n"
                        "- DJI drones\n"
                        "- Drone networks\n"
                        "Phase 1: WiFi (all channels)\n"
                        "Phase 2: BLE\n"
                        "Duration: 30 seconds\n",
    },
    {
        .label = "Quick Scan (15s)",
        .command = "aerialscan 15\n",
        .details_header = "Quick Scan",
        .details_text = "Fast 15 second scan for\n"
                        "nearby aerial devices.\n",
    },
    {
        .label = "Extended Scan (60s)",
        .command = "aerialscan 60\n",
        .details_header = "Extended Scan",
        .details_text = "Extended 60 second scan\n"
                        "for maximum coverage.\n",
    },
    {
        .label = "List Detected Drones",
        .command = "aeriallist\n",
        .details_header = "Detected Devices",
        .details_text = "Lists all detected aerial\n"
                        "devices with:\n"
                        "- Device ID & Type\n"
                        "- GPS coordinates\n"
                        "- Altitude & Speed\n"
                        "- Operator location\n"
                        "- RSSI signal\n",
    },
    {
        .label = "Track Drone by Index",
        .command = "aerialtrack",
        .needs_input = true,
        .input_text = "Device Index",
        .details_header = "Track Drone",
        .details_text = "Track specific drone by\n"
                        "index from aeriallist.\n"
                        "Shows real-time updates\n"
                        "for selected device.\n",
    },
    {
        .label = "Track Drone by MAC",
        .command = "aerialtrack",
        .needs_input = true,
        .input_text = "MAC Address",
        .details_header = "Track by MAC",
        .details_text = "Track specific drone by\n"
                        "MAC address.\n"
                        "Format: aa:bb:cc:dd:ee:ff\n",
    },
    {
        .label = "Spoof Test Drone",
        .command = "aerialspoof\n",
        .details_header = "Test Spoof",
        .details_text = "Broadcasts test RemoteID:\n"
                        "ID: GHOST-TEST\n"
                        "Location: San Francisco\n"
                        "Altitude: 100m\n"
                        "Status: Airborne\n\n"
                        "Note: WiFi suspended\n"
                        "during BLE broadcast\n",
    },
    {
        .label = "Custom Spoof",
        .command = "aerialspoof",
        .needs_input = true,
        .input_text = "ID Lat Lon Alt",
        .details_header = "Custom Spoof",
        .details_text = "Broadcast custom RemoteID.\n"
                        "Format:\n"
                        "DRONE-ID lat lon alt\n\n"
                        "Example:\n"
                        "GHOST-1 40.7128 -74.0060 100\n",
    },
    {
        .label = "Stop Spoofing",
        .command = "aerialspoofstop\n",
        .details_header = "Stop Spoofing",
        .details_text = "Stops RemoteID broadcast\n"
                        "and restores WiFi.\n",
    },
    {
        .label = "Stop All",
        .command = "aerialstop\n",
        .details_header = "Stop All Operations",
        .details_text = "Stops all active aerial\n"
                        "operations including:\n"
                        "- Scanning\n"
                        "- Tracking\n"
                        "- Spoofing\n",
    },
};

// IR menu command definitions
static const MenuCommand ir_commands[] = {
    {
        .label = "Browse IR Remotes",
        .command = "ir list\n",
        .details_header = "Browse IR Remotes",
        .details_text = "Browse IR remotes on ESP\n",
    },
    {
        .label = "Browse Universals",
        .command = "ir universals list\n",
        .details_header = "Browse Universals",
        .details_text = "Browse built-in universal IR\n",
    },
    {
        .label = "Send from Flipper",
        .command = "send_ir_file",
        .details_header = "Send from Flipper",
        .details_text = "Browse Flipper IR files and\nsend signals to ESP.\n",
    },
    {
        .label = "IR Learn (Auto File)",
        .command = "ir learn\n",
        .details_header = "Learn IR (Auto)",
        .details_text = "Capture IR signal (10s wait).\n"
                        "Auto-create a new IR file.\n",
    },
    {
        .label = "IR Learn (Path)",
        .command = "ir learn",
        .needs_input = true,
        .input_text = "Path (optional)",
        .details_header = "Learn IR (Path)",
        .details_text = "Capture IR signal (10s wait).\n"
                        "Leave blank to auto-create,\n"
                        "or specify path to append.\n",
    },
    {
        .label = "IR Receive",
        .command = "ir rx",
        .needs_input = true,
        .input_text = "Timeout (default 60)",
        .details_header = "Receive IR",
        .details_text = "Wait for single IR signal.\n"
                        "Prints decoded or RAW data.\n",
    },
    {
        .label = "IR List Files (Raw)",
        .command = "ir list\n",
        .details_header = "List IR Files",
        .details_text = "List all .ir/.json files in\n"
                        "remote directories.\n",
    },
    {
        .label = "IR Show File (Raw)",
        .command = "ir show",
        .needs_input = true,
        .input_text = "Path or Index",
        .details_header = "Show IR File",
        .details_text = "Display signals from an IR file.\n",
    },
    {
        .label = "IR Send (Raw)",
        .command = "ir send",
        .needs_input = true,
        .input_text = "Index [Button]",
        .details_header = "Send IR (Raw)",
        .details_text = "Transmit using raw indices.\n",
    },
    {
        .label = "IR Dazzler Start",
        .command = "ir dazzler\n",
        .details_header = "IR Dazzler Start",
        .details_text = "Start continuous IR dazzler flood.\n",
    },
    {
        .label = "IR Dazzler Stop",
        .command = "ir dazzler stop\n",
        .details_header = "IR Dazzler Stop",
        .details_text = "Stop continuous IR dazzler flood.\n",
    },
    {
        .label = "Stop IR",
        .command = "stop\n",
        .details_header = "Stop IR",
        .details_text = "Stop all active IR operations.\n",
    },
};

#define IR_UART_PARSE_BUF_SIZE 1024

static char* next_line(char* buf, size_t* offset) {
    if(!buf || !offset) return NULL;
    char* p = buf + *offset;
    while(*p == '\r' || *p == '\n')
        p++;
    if(*p == '\0') return NULL;
    char* start = p;
    while(*p && *p != '\r' && *p != '\n')
        p++;
    if(*p) {
        *p++ = '\0';
    }
    *offset = (size_t)(p - buf);
    return start;
}

static bool ir_query_and_parse_list(AppState* state) {
    if(!state || !state->uart_context) return false;

    uart_reset_text_buffers(state->uart_context);
    send_uart_command("ir list\n", state);

    char buffer[IR_UART_PARSE_BUF_SIZE];

    size_t len = 0;
    uint32_t start = furi_get_tick();
    const uint32_t timeout_ms = 2000;
    while(furi_get_tick() - start < timeout_ms) {
        furi_delay_ms(100);
        if(uart_copy_text_buffer_tail(state->uart_context, buffer, IR_UART_PARSE_BUF_SIZE, &len) &&
           len > 0) {
            if(strstr(buffer, "IR files in ") || strstr(buffer, "(none)") ||
               strstr(buffer, "(none).") || strchr(buffer, '[')) {
                break;
            }
        }
    }

    if(len == 0) {
        return false;
    }

    state->ir_remote_count = 0;

    size_t pos = 0;
    char* line = NULL;
    while((line = next_line(buffer, &pos))) {
        while(*line == ' ' || *line == '\t')
            line++;
        if(strncmp(line, "IR files in ", 12) == 0) {
            continue;
        }
        if(strncmp(line, "(none).", 7) == 0 || strncmp(line, "(none)", 6) == 0) {
            continue;
        }
        if(line[0] == '[') {
            unsigned int idx = 0;
            char name[64] = {0};
            if(sscanf(line, "[%u] %63s", &idx, name) == 2) {
                if(state->ir_remote_count < COUNT_OF(state->ir_remotes)) {
                    IrRemoteEntry* e = &state->ir_remotes[state->ir_remote_count++];
                    e->index = idx;
                    strncpy(e->name, name, sizeof(e->name) - 1);
                    e->name[sizeof(e->name) - 1] = '\0';
                }
            }
        }
    }

    return state->ir_remote_count > 0;
}

static bool ir_query_and_parse_show(AppState* state, uint32_t remote_index) {
    if(!state || !state->uart_context) return false;

    uart_reset_text_buffers(state->uart_context);

    char cmd[32];
    snprintf(cmd, sizeof(cmd), "ir show %lu\n", (unsigned long)remote_index);
    send_uart_command(cmd, state);

    char buffer[IR_UART_PARSE_BUF_SIZE];

    size_t len = 0;
    uint32_t start = furi_get_tick();
    const uint32_t timeout_ms = 3000;
    while(furi_get_tick() - start < timeout_ms) {
        furi_delay_ms(100);
        if(uart_copy_text_buffer_tail(state->uart_context, buffer, IR_UART_PARSE_BUF_SIZE, &len) &&
           len > 0) {
            if(strstr(buffer, "Signals in ") || strstr(buffer, "Unique buttons in ") ||
               strchr(buffer, '[')) {
                break;
            }
        }
    }

    if(len == 0) {
        return false;
    }

    state->ir_signal_count = 0;

    size_t pos = 0;
    char* line = NULL;
    while((line = next_line(buffer, &pos))) {
        while(*line == ' ' || *line == '\t')
            line++;
        if(strncmp(line, "Signals in ", 11) == 0) {
            continue;
        }
        if(strncmp(line, "IR: ", 4) == 0) {
            continue;
        }
        if(line[0] == '[') {
            unsigned int idx = 0;
            char name[32] = {0};
            char proto[16] = {0};
            int n = sscanf(line, "[%u] %31s (%15[^)])", &idx, name, proto);
            if(n >= 2) {
                if(state->ir_signal_count < COUNT_OF(state->ir_signals)) {
                    IrSignalEntry* e = &state->ir_signals[state->ir_signal_count++];
                    e->index = idx;
                    strncpy(e->name, name, sizeof(e->name) - 1);
                    e->name[sizeof(e->name) - 1] = '\0';
                    if(n == 3 && proto[0]) {
                        strncpy(e->proto, proto, sizeof(e->proto) - 1);
                        e->proto[sizeof(e->proto) - 1] = '\0';
                    } else {
                        e->proto[0] = '\0';
                    }
                }
            }
        }
    }

    return state->ir_signal_count > 0;
}

// Stream/index .ir file without holding entire file in RAM
static bool ir_index_buttons_from_file(AppState* state) {
    if(!state || !state->ir_file_path[0]) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool ok = false;

    do {
        if(!storage_file_open(file, state->ir_file_path, FSAM_READ, FSOM_OPEN_EXISTING)) break;

        const size_t buf_size = 512;
        uint8_t buf[buf_size];
        size_t global_offset = 0;
        bool in_block = false;
        size_t block_start = 0;

        state->ir_signal_count = 0;

        while(true) {
            uint16_t read = storage_file_read(file, buf, buf_size);
            if(read == 0) break;

            size_t pos = 0;
            while(pos < read && state->ir_signal_count < COUNT_OF(state->ir_signals)) {
                // Consume whitespace
                while(pos < read && (buf[pos] == '\r' || buf[pos] == '\n' || buf[pos] == ' ' ||
                                     buf[pos] == '\t')) {
                    if(buf[pos] == '\n' || buf[pos] == '\r') {
                        if(in_block) {
                            // Potential end of block handled when we see next header
                        }
                    }
                    pos++;
                    global_offset++;
                }
                if(pos >= read) break;

                // Skip comments
                if(buf[pos] == '#') {
                    while(pos < read && buf[pos] != '\n' && buf[pos] != '\r') {
                        pos++;
                        global_offset++;
                    }
                    continue;
                }

                // Detect "name:" start
                const char name_hdr[] = "name:";
                if(read - pos >= sizeof(name_hdr) - 1 &&
                   memcmp(buf + pos, name_hdr, sizeof(name_hdr) - 1) == 0) {
                    // If we were already in a block, close it at current global_offset
                    if(in_block && state->ir_signal_count > 0) {
                        state->ir_signal_block_lengths[state->ir_signal_count - 1] =
                            (global_offset)-state
                                ->ir_signal_block_offsets[state->ir_signal_count - 1];
                    }

                    in_block = true;
                    block_start = global_offset;

                    // Parse name on this line to populate label
                    size_t line_end = pos;
                    while(line_end < read && buf[line_end] != '\n' && buf[line_end] != '\r')
                        line_end++;

                    size_t val_start = pos + (sizeof(name_hdr) - 1);
                    while(val_start < line_end &&
                          (buf[val_start] == ' ' || buf[val_start] == '\t')) {
                        val_start++;
                    }
                    size_t val_end = line_end;
                    while(val_end > val_start &&
                          (buf[val_end - 1] == ' ' || buf[val_end - 1] == '\t')) {
                        val_end--;
                    }

                    if(state->ir_signal_count < COUNT_OF(state->ir_signals)) {
                        IrSignalEntry* e = &state->ir_signals[state->ir_signal_count];
                        size_t name_len = (val_end > val_start) ? (val_end - val_start) : 0;
                        if(name_len >= sizeof(e->name)) name_len = sizeof(e->name) - 1;
                        if(name_len > 0) {
                            memcpy(e->name, buf + val_start, name_len);
                            e->name[name_len] = '\0';
                        } else {
                            e->name[0] = '\0';
                        }
                        e->index = state->ir_signal_count; // use slot index
                        e->proto[0] = '\0';

                        state->ir_signal_block_offsets[state->ir_signal_count] = block_start;
                        state->ir_signal_block_lengths[state->ir_signal_count] = 0; // temp
                        state->ir_signal_count++;
                    }

                    global_offset += (line_end - pos);
                    pos = line_end;
                    continue;
                }

                // Detect end of block by seeing next header in subsequent iterations
                // Consume rest of line
                while(pos < read && buf[pos] != '\n' && buf[pos] != '\r') {
                    pos++;
                    global_offset++;
                }
            }
        }

        // Close last block length if open
        if(in_block && state->ir_signal_count > 0) {
            uint64_t file_size = storage_file_size(file);
            state->ir_signal_block_lengths[state->ir_signal_count - 1] =
                (size_t)file_size - state->ir_signal_block_offsets[state->ir_signal_count - 1];
        }

        ok = state->ir_signal_count > 0;
    } while(false);

    if(file) {
        storage_file_close(file);
        storage_file_free(file);
    }
    if(storage) {
        furi_record_close(RECORD_STORAGE);
    }

    return ok;
}

static bool ir_query_and_parse_universals(AppState* state) {
    if(!state || !state->uart_context) return false;

    uart_reset_text_buffers(state->uart_context);
    send_uart_command("ir universals list\n", state);

    char buffer[IR_UART_PARSE_BUF_SIZE];

    size_t len = 0;
    uint32_t start = furi_get_tick();
    const uint32_t timeout_ms = 3000;
    while(furi_get_tick() - start < timeout_ms) {
        furi_delay_ms(100);
        if(uart_copy_text_buffer_tail(state->uart_context, buffer, IR_UART_PARSE_BUF_SIZE, &len) &&
           len > 0) {
            if(strstr(buffer, "Universal Files in ") || strstr(buffer, "Built-in") ||
               strstr(buffer, "(none)") || strchr(buffer, '.')) {
                break;
            }
        }
    }

    if(len == 0) {
        return false;
    }

    state->ir_universal_count = 0;
    bool in_files_section = false;

    size_t pos = 0;
    char* line = NULL;
    while((line = next_line(buffer, &pos))) {
        while(*line == ' ' || *line == '\t')
            line++;

        if(strncmp(line, "IR: ", 4) == 0) {
            line += 4;
            while(*line == ' ' || *line == '\t')
                line++;
        }

        if(strncmp(line, "Universal Files in ", 19) == 0) {
            in_files_section = true;
            continue;
        }

        if(in_files_section) {
            if(line[0] == '\0') {
                continue;
            }

            if(strncmp(line, "Built-in Universal Signals", 26) == 0 ||
               strncmp(line, "Use 'ir universals list", 23) == 0) {
                in_files_section = false;
                continue;
            }

            if(strncmp(line, "(none)", 6) == 0) {
                continue;
            }

            if(state->ir_universal_count < COUNT_OF(state->ir_universals)) {
                IrUniversalEntry* e = &state->ir_universals[state->ir_universal_count];
                e->index = state->ir_universal_count;
                strncpy(e->name, line, sizeof(e->name) - 1);
                e->name[sizeof(e->name) - 1] = '\0';
                e->proto[0] = '\0';
                state->ir_universal_count++;
            }
        }
    }

    return state->ir_universal_count > 0;
}

static bool ir_query_and_parse_universal_buttons(AppState* state, const char* filename) {
    if(!state || !state->uart_context || !filename || !filename[0]) return false;

    uart_reset_text_buffers(state->uart_context);

    char path[128];
    snprintf(path, sizeof(path), "/mnt/ghostesp/infrared/universals/%s", filename);

    char cmd[192];
    snprintf(cmd, sizeof(cmd), "ir show %s\n", path);
    send_uart_command(cmd, state);

    char buffer[IR_UART_PARSE_BUF_SIZE];

    size_t len = 0;

    uint32_t start = furi_get_tick();
    const uint32_t timeout_ms = 5000;
    while(furi_get_tick() - start < timeout_ms) {
        furi_delay_ms(100);
        if(uart_copy_text_buffer(state->uart_context, buffer, IR_UART_PARSE_BUF_SIZE, &len) &&
           len > 0) {
            if(strstr(buffer, "Unique buttons in ") || strstr(buffer, "Signals in ")) {
                break;
            }
        }
    }

    if(len == 0) {
        return false;
    }

    state->ir_signal_count = 0;

    size_t pos = 0;
    char* line = NULL;
    while((line = next_line(buffer, &pos))) {
        while(*line == ' ' || *line == '\t')
            line++;

        if(strncmp(line, "Signals in ", 11) == 0) {
            continue;
        }
        if(strncmp(line, "IR: ", 4) == 0) {
            continue;
        }

        if(line[0] == '[') {
            unsigned int idx = 0;
            char name[32] = {0};
            char proto[16] = {0};
            int n = sscanf(line, "[%u] %31s (%15[^)])", &idx, name, proto);
            if(n < 2) {
                proto[0] = '\0';
                n = sscanf(line, "[%u] %31s", &idx, name);
            }
            if(n >= 2) {
                bool exists = false;
                for(size_t i = 0; i < state->ir_signal_count; i++) {
                    if(strcmp(state->ir_signals[i].name, name) == 0) {
                        exists = true;
                        break;
                    }
                }
                if(!exists && state->ir_signal_count < COUNT_OF(state->ir_signals)) {
                    IrSignalEntry* e = &state->ir_signals[state->ir_signal_count++];
                    e->index = idx;
                    strncpy(e->name, name, sizeof(e->name) - 1);
                    e->name[sizeof(e->name) - 1] = '\0';
                    if(n == 3) {
                        strncpy(e->proto, proto, sizeof(e->proto) - 1);
                        e->proto[sizeof(e->proto) - 1] = '\0';
                    } else {
                        e->proto[0] = '\0';
                    }
                }
            }
        }
    }

    bool result = state->ir_signal_count > 0;
    return result;
}

static void ir_show_remotes_menu(AppState* state) {
    if(!state || !state->ir_remotes_menu) return;

    submenu_reset(state->ir_remotes_menu);
    submenu_set_header(state->ir_remotes_menu, "IR Remotes");

    uint32_t selected = 0;
    for(size_t i = 0; i < state->ir_remote_count; i++) {
        submenu_add_item(
            state->ir_remotes_menu, state->ir_remotes[i].name, i, submenu_callback, state);
        if(state->ir_remotes[i].index == state->ir_current_remote_index) {
            selected = i;
        }
    }

    if(state->ir_remote_count > 0) {
        submenu_set_selected_item(state->ir_remotes_menu, selected);
    }

    view_dispatcher_switch_to_view(state->view_dispatcher, VIEW_IR_REMOTES);
    state->current_view = VIEW_IR_REMOTES;
}

static void ir_show_buttons_menu(AppState* state) {
    if(!state || !state->ir_buttons_menu) return;

    submenu_reset(state->ir_buttons_menu);
    if(state->ir_universal_buttons_mode) {
        submenu_set_header(state->ir_buttons_menu, "Universal Buttons");
    } else {
        submenu_set_header(state->ir_buttons_menu, "IR Buttons");
    }

    for(size_t i = 0; i < state->ir_signal_count; i++) {
        const char* label = state->ir_signals[i].name;
        submenu_add_item(state->ir_buttons_menu, label, i, submenu_callback, state);
    }

    if(state->ir_signal_count > 0) {
        submenu_set_selected_item(state->ir_buttons_menu, 0);
    }

    view_dispatcher_switch_to_view(state->view_dispatcher, VIEW_IR_BUTTONS);
    state->current_view = VIEW_IR_BUTTONS;
}

static void ir_show_universals_menu(AppState* state) {
    if(!state || !state->ir_universals_menu) return;

    submenu_reset(state->ir_universals_menu);
    submenu_set_header(state->ir_universals_menu, "IR Universals");

    for(size_t i = 0; i < state->ir_universal_count; i++) {
        const char* label = state->ir_universals[i].name;
        submenu_add_item(state->ir_universals_menu, label, i, submenu_callback, state);
    }

    if(state->ir_universal_count > 0) {
        submenu_set_selected_item(state->ir_universals_menu, 0);
    }

    view_dispatcher_switch_to_view(state->view_dispatcher, VIEW_IR_UNIVERSALS);
    state->current_view = VIEW_IR_UNIVERSALS;
}

static void ir_show_error(AppState* state, const char* text) {
    if(!state || !state->confirmation_view) return;

    state->previous_view = state->current_view;
    confirmation_view_set_header(state->confirmation_view, "IR Error");
    confirmation_view_set_text(state->confirmation_view, text ? text : "IR error");
    confirmation_view_set_ok_callback(state->confirmation_view, app_info_ok_callback, state);
    confirmation_view_set_cancel_callback(state->confirmation_view, app_info_ok_callback, state);
    view_dispatcher_switch_to_view(state->view_dispatcher, VIEW_CONFIRMATION);
    state->current_view = VIEW_CONFIRMATION;
}

static bool cycle_menu_item(
    CyclingMenuDef* cycling_array,
    size_t cycling_count,
    size_t* current_index,
    MenuCommand* menu_commands,
    size_t menu_index,
    Submenu* menu,
    InputEvent* event) {
    if(event->key == InputKeyRight) {
        *current_index = (*current_index + 1) % cycling_count;
    } else {
        *current_index = (*current_index == 0) ? (cycling_count - 1) : (*current_index - 1);
    }
    submenu_change_item_label(menu, menu_index, cycling_array[*current_index].label);

    // Update menu command fields
    MenuCommand* cmd = &menu_commands[menu_index];
    cmd->command = cycling_array[*current_index].command;
    cmd->needs_input = cycling_array[*current_index].needs_input;
    cmd->input_text = cycling_array[*current_index].input_text;
    cmd->details_header = cycling_array[*current_index].details_header;
    cmd->details_text = cycling_array[*current_index].details_text;

    return true;
}

void send_uart_command(const char* command, void* state) {
    AppState* app_state = (AppState*)state;
    uart_send(app_state->uart_context, (uint8_t*)command, strlen(command));
}

void send_uart_command_with_text(const char* command, char* text, AppState* state) {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s %s\n", command, text);
    uart_send(state->uart_context, (uint8_t*)buffer, strlen(buffer));
}

void send_uart_command_with_bytes(
    const char* command,
    const uint8_t* bytes,
    size_t length,
    AppState* state) {
    send_uart_command(command, state);
    uart_send(state->uart_context, bytes, length);
}

static void confirmation_ok_callback(void* context) {
    MenuCommandContext* cmd_ctx = context;
    if(cmd_ctx && cmd_ctx->state && cmd_ctx->command) {
        bool file_opened = false;

        // Handle capture commands
        if(cmd_ctx->command->capture_prefix || cmd_ctx->command->file_ext ||
           cmd_ctx->command->folder) {
            FURI_LOG_I("Capture", "Attempting to open PCAP file before sending capture command.");
            file_opened = uart_receive_data(
                cmd_ctx->state->uart_context,
                cmd_ctx->state->view_dispatcher,
                cmd_ctx->state,
                cmd_ctx->command->capture_prefix ? cmd_ctx->command->capture_prefix : "",
                cmd_ctx->command->file_ext ? cmd_ctx->command->file_ext : "",
                cmd_ctx->command->folder ? cmd_ctx->command->folder : "");

            if(!file_opened) {
                FURI_LOG_E("Capture", "Failed to open PCAP file. Aborting capture command.");
                confirmation_cancel_callback(cmd_ctx);
                return;
            }

            // Send capture command
            send_uart_command(cmd_ctx->command->command, cmd_ctx->state);
            FURI_LOG_I("Capture", "Capture command sent to firmware.");
        } else {
            // For non-capture confirmation commands, send command and switch to text
            // view
            send_uart_command(cmd_ctx->command->command, cmd_ctx->state);
            uart_receive_data(
                cmd_ctx->state->uart_context,
                cmd_ctx->state->view_dispatcher,
                cmd_ctx->state,
                "",
                "",
                ""); // No capture files needed
        }
    }
    if(cmd_ctx->state) cmd_ctx->state->active_confirm_context = NULL;
    free(cmd_ctx);
}

void navigate_to_view(AppState* state, uint8_t view_id) {
    if(!state || !state->view_dispatcher) return;
    switch(view_id) {
    case VIEW_MAIN:
        show_main_menu(state);
        break;
    case VIEW_WIFI:
        show_wifi_menu(state);
        break;
    case VIEW_WIFI_SCANNING:
        show_wifi_scanning_menu(state);
        break;
    case VIEW_WIFI_CAPTURE:
        show_wifi_capture_menu(state);
        break;
    case VIEW_WIFI_ATTACK:
        show_wifi_attack_menu(state);
        break;
    case VIEW_WIFI_NETWORK:
        show_wifi_network_menu(state);
        break;
    case VIEW_WIFI_SETTINGS:
        show_wifi_settings_menu(state);
        break;
    // WiFi Settings sub-categories
    case VIEW_WIFI_SETTINGS_LED:
        show_wifi_settings_led_menu(state);
        break;
    case VIEW_WIFI_SETTINGS_SD:
        show_wifi_settings_sd_menu(state);
        break;
    case VIEW_WIFI_SETTINGS_MGMT:
        show_wifi_settings_mgmt_menu(state);
        break;
    case VIEW_WIFI_SETTINGS_DEVICE:
        show_wifi_settings_device_menu(state);
        break;
    case VIEW_WIFI_SETTINGS_MISC:
        show_wifi_settings_misc_menu(state);
        break;
    // WiFi Network sub-categories
    case VIEW_WIFI_NETWORK_PORTAL:
        show_wifi_network_portal_menu(state);
        break;
    case VIEW_WIFI_NETWORK_CONN:
        show_wifi_network_conn_menu(state);
        break;
    case VIEW_WIFI_NETWORK_SCAN:
        show_wifi_network_scan_menu(state);
        break;
    case VIEW_WIFI_NETWORK_IOT:
        show_wifi_network_iot_menu(state);
        break;
    case VIEW_WIFI_NETWORK_WEBUI:
        show_wifi_network_webui_menu(state);
        break;
    // BLE Scanning sub-categories
    case VIEW_BLE_SCAN_DETECT:
        show_ble_scan_detect_menu(state);
        break;
    case VIEW_BLE_SCAN_AIRTAG:
        show_ble_scan_airtag_menu(state);
        break;
    case VIEW_BLE_SCAN_GATT:
        show_ble_scan_gatt_menu(state);
        break;
    case VIEW_BLE_SCAN_ADV:
        show_ble_scan_adv_menu(state);
        break;
    // GPS sub-categories
    case VIEW_GPS_CONFIG:
        show_gps_config_menu(state);
        break;
    case VIEW_GPS_WARDRIVE:
        show_gps_wardrive_menu(state);
        break;
    case VIEW_AERIAL:
        show_aerial_menu(state);
        break;
    case VIEW_BLE:
        show_ble_menu(state);
        break;
    case VIEW_BLE_SCANNING:
        show_ble_scanning_menu(state);
        break;
    case VIEW_BLE_CAPTURE:
        show_ble_capture_menu(state);
        break;
    case VIEW_BLE_ATTACK:
        show_ble_attack_menu(state);
        break;
    case VIEW_GPS:
        show_gps_menu(state);
        break;
    case VIEW_IR:
        show_ir_menu(state);
        break;
    case VIEW_IR_REMOTES:
        ir_show_remotes_menu(state);
        break;
    case VIEW_IR_BUTTONS:
        ir_show_buttons_menu(state);
        break;
    case VIEW_IR_UNIVERSALS:
        ir_show_universals_menu(state);
        break;
    case VIEW_STATUS_IDLE:
        show_status_idle_menu(state);
        break;
    case VIEW_SETTINGS_CONFIG:
    case VIEW_SETTINGS_ACTIONS:
        view_dispatcher_switch_to_view(state->view_dispatcher, view_id);
        state->current_view = view_id;
        break;
    default:
        show_main_menu(state);
        break;
    }
}

static void confirmation_cancel_callback(void* context) {
    MenuCommandContext* cmd_ctx = context;
    if(cmd_ctx && cmd_ctx->state) {
        cmd_ctx->state->active_confirm_context = NULL;
        navigate_to_view(cmd_ctx->state, cmd_ctx->state->previous_view);
    }
    free(cmd_ctx);
}

static void app_info_ok_callback(void* context) {
    AppState* state = context;
    if(!state) return;

    view_dispatcher_switch_to_view(state->view_dispatcher, state->previous_view);
    state->current_view = state->previous_view;
}

static void show_command_details(AppState* state, const MenuCommand* command) {
    if(!command->details_header || !command->details_text) return;

    // Save current view before switching
    state->previous_view = state->current_view;

    // Setup confirmation view to show details
    confirmation_view_set_header(state->confirmation_view, command->details_header);
    confirmation_view_set_text(state->confirmation_view, command->details_text);

    // Set up callbacks for OK/Cancel to return to previous view
    confirmation_view_set_ok_callback(
        state->confirmation_view,
        app_info_ok_callback, // Reuse app info callback since it does the same
        // thing
        state);
    confirmation_view_set_cancel_callback(state->confirmation_view, app_info_ok_callback, state);

    // Switch to confirmation view
    view_dispatcher_switch_to_view(state->view_dispatcher, VIEW_CONFIRMATION);
    state->current_view = VIEW_CONFIRMATION;
}

static void error_callback(void* context) {
    AppState* state = (AppState*)context;
    if(!state) return;
    view_dispatcher_switch_to_view(state->view_dispatcher, state->previous_view);
    state->current_view = state->previous_view;
}

static void show_result_dialog(AppState* state, const char* header, const char* text) {
    if(!state || !state->confirmation_view) return;

    state->previous_view = state->current_view;
    confirmation_view_set_header(state->confirmation_view, header ? header : "Result");
    confirmation_view_set_text(state->confirmation_view, text ? text : "");
    confirmation_view_set_ok_callback(state->confirmation_view, app_info_ok_callback, state);
    confirmation_view_set_cancel_callback(state->confirmation_view, app_info_ok_callback, state);
    view_dispatcher_switch_to_view(state->view_dispatcher, VIEW_CONFIRMATION);
    state->current_view = VIEW_CONFIRMATION;
}

static void ir_sweep_stop_callback(void* context) {
    AppState* state = (AppState*)context;
    if(!state) return;
    send_uart_command("stop\n", state);
    app_info_ok_callback(state);
}

static bool handle_ir_command_feedback_ex(
    AppState* state,
    const char* cmd,
    bool send_cmd,
    bool reset_buffers) {
    if(!state || !state->uart_context || !cmd) return false;

    bool is_send = strncmp(cmd, "ir send", 7) == 0;
    bool is_uni_send = strncmp(cmd, "ir universals send ", 20) == 0;
    bool is_uni_sendall = strncmp(cmd, "ir universals sendall", 21) == 0;
    bool is_inline = strncmp(cmd, "ir inline", 9) == 0;
    bool is_dazzler = strncmp(cmd, "ir dazzler", 10) == 0;

    if(!is_send && !is_uni_send && !is_uni_sendall && !is_inline && !is_dazzler) return false;

    if(reset_buffers) {
        uart_reset_text_buffers(state->uart_context);
    }
    if(send_cmd) {
        send_uart_command(cmd, state);
    }

    if(is_uni_sendall) {
        state->previous_view = state->current_view;
        confirmation_view_set_header(state->confirmation_view, "Universal send");
        confirmation_view_set_text(state->confirmation_view, "Universal sending...\nOK = Stop");
        confirmation_view_set_ok_callback(state->confirmation_view, ir_sweep_stop_callback, state);
        confirmation_view_set_cancel_callback(
            state->confirmation_view, app_info_ok_callback, state);
        view_dispatcher_switch_to_view(state->view_dispatcher, VIEW_CONFIRMATION);
        state->current_view = VIEW_CONFIRMATION;
    } else if(is_dazzler) {
        show_result_dialog(state, "IR Dazzler", "Working...");
    } else {
        show_result_dialog(state, "IR", "Transmitting...");
    }

    char buffer[512];
    char raw_buffer[512];
    size_t len = 0;
    char* message = state->confirmation_message;
    char summary[128];
    message[0] = '\0';
    summary[0] = '\0';
    raw_buffer[0] = '\0';
    bool have_output = false;
    bool saw_ok = false;

    uint32_t start = furi_get_tick();
    const uint32_t timeout_ms = is_uni_sendall ? 60000 : 5000;

    while(furi_get_tick() - start < timeout_ms) {
        furi_delay_ms(100);

        if(!uart_copy_text_buffer_tail(state->uart_context, buffer, sizeof(buffer), &len) ||
           len == 0) {
            continue;
        }

        have_output = true;
        memcpy(raw_buffer, buffer, len < sizeof(raw_buffer) ? len : sizeof(raw_buffer) - 1);
        raw_buffer[len < sizeof(raw_buffer) ? len : sizeof(raw_buffer) - 1] = '\0';

        size_t pos = 0;
        char* line = NULL;
        while((line = next_line(buffer, &pos))) {
            while(*line == ' ' || *line == '\t')
                line++;

            if(is_dazzler) {
                char* tag = strstr(line, "IR_DAZZLER:");
                if(tag) {
                    const char* code = tag + 11; // skip "IR_DAZZLER:"
                    while(*code == ' ' || *code == '\t')
                        code++;

                    if(strncmp(code, "STARTED", 7) == 0) {
                        strncpy(
                            message,
                            "Dazzler started successfully",
                            sizeof(state->confirmation_message) - 1);
                    } else if(strncmp(code, "FAILED", 6) == 0) {
                        strncpy(
                            message, "Dazzler failed", sizeof(state->confirmation_message) - 1);
                    } else if(strncmp(code, "ALREADY_RUNNING", 15) == 0) {
                        strncpy(
                            message,
                            "Dazzler is already running",
                            sizeof(state->confirmation_message) - 1);
                    } else if(strncmp(code, "STOPPING", 8) == 0) {
                        strncpy(
                            message, "Stopped dazzler.", sizeof(state->confirmation_message) - 1);
                    } else if(strncmp(code, "NOT_RUNNING", 11) == 0) {
                        strncpy(
                            message,
                            "Dazzler is not running",
                            sizeof(state->confirmation_message) - 1);
                    } else {
                        snprintf(
                            message, sizeof(state->confirmation_message), "Dazzler: %.64s", code);
                    }
                    message[sizeof(state->confirmation_message) - 1] = '\0';
                    start = timeout_ms + start;
                    break;
                }
            }

            if(is_send || is_uni_send || is_inline) {
                if(strncmp(line, "IR: signal ", 11) == 0) {
                    const char* p = line + 11;
                    char name[16] = {0};
                    char proto[16] = {0};
                    char addr[16] = {0};
                    char cmd[16] = {0};
                    char len_str[8] = {0};
                    char freq_str[24] = {0};
                    char duty_str[16] = {0};

                    if(strncmp(p, "raw ", 4) == 0 || strncmp(p, "raw len=", 8) == 0 ||
                       strstr(p, " raw ") || strstr(p, " raw len=")) {
                        if(sscanf(
                               line,
                               "IR: signal raw len=%15s freq=%31s duty=%15s",
                               len_str,
                               freq_str,
                               duty_str) == 3) {
                            snprintf(
                                summary,
                                sizeof(summary),
                                "Raw len=%s\nFreq: %s\nDuty: %s",
                                len_str,
                                freq_str,
                                duty_str);
                        }
                    } else {
                        if(strchr(p, '[')) {
                            if(sscanf(
                                   line,
                                   "IR: signal [%15[^]]] protocol=%15s addr=%15s cmd=%15s",
                                   name,
                                   proto,
                                   addr,
                                   cmd) >= 4) {
                                if(cmd[0] == '\0') {
                                    cmd[0] = '-';
                                    cmd[1] = '\0';
                                }
                                snprintf(
                                    summary,
                                    sizeof(summary),
                                    "%s (%s)\nAddr: %s\nCmd: %s",
                                    name,
                                    proto,
                                    addr,
                                    cmd);
                            }
                        } else {
                            if(sscanf(
                                   line,
                                   "IR: signal protocol=%15s addr=%15s cmd=%15s",
                                   proto,
                                   addr,
                                   cmd) >= 3) {
                                snprintf(
                                    summary,
                                    sizeof(summary),
                                    "Proto: %s\nAddr: %s\nCmd: %s",
                                    proto,
                                    addr,
                                    cmd);
                            }
                        }
                    }

                    if(saw_ok && summary[0] && !message[0]) {
                        // Truncate summary if needed to prevent buffer overflow when combining
                        size_t max_len = sizeof(state->confirmation_message) -
                                         10; // Reserve space for "Send OK\n"
                        if(strlen(summary) > max_len) {
                            summary[max_len] = '\0';
                        }
                        snprintf(
                            message,
                            sizeof(state->confirmation_message),
                            "Send OK%s%s",
                            "\n",
                            summary);
                        start = timeout_ms + start;
                        break;
                    }
                    continue;
                }
            }

            if(is_inline && strstr(line, "IR inline parse failed")) {
                strncpy(message, "Inline parse failed", sizeof(state->confirmation_message) - 1);
                message[sizeof(state->confirmation_message) - 1] = '\0';
                start = timeout_ms + start;
                break;
            }

            if(is_send || is_uni_send || is_inline) {
                if(strstr(line, "send OK") || strstr(line, "status: OK") ||
                   strstr(line, "status OK") || strstr(line, "ir signal transmission complete")) {
                    saw_ok = true;
                    if(summary[0]) {
                        // Truncate summary if needed
                        size_t max_len = sizeof(state->confirmation_message) - 10;
                        if(strlen(summary) > max_len) {
                            summary[max_len] = '\0';
                        }
                        snprintf(
                            message,
                            sizeof(state->confirmation_message),
                            "Send OK%s%s",
                            "\n",
                            summary);
                        start = timeout_ms + start;
                        break;
                    }
                }
                if(strstr(line, "send FAIL") || strstr(line, "status: FAIL") ||
                   strstr(line, "status FAIL") || strstr(line, "status: ERROR")) {
                    strncpy(message, "Send failed", sizeof(state->confirmation_message) - 1);
                    message[sizeof(state->confirmation_message) - 1] = '\0';
                    start = timeout_ms + start;
                    break;
                }
                if(strstr(line, "failed to read list")) {
                    strncpy(
                        message, "Failed to read list", sizeof(state->confirmation_message) - 1);
                    message[sizeof(state->confirmation_message) - 1] = '\0';
                    start = timeout_ms + start;
                    break;
                }
                if(strstr(line, "no signals in")) {
                    strncpy(
                        message, "No signals in list", sizeof(state->confirmation_message) - 1);
                    message[sizeof(state->confirmation_message) - 1] = '\0';
                    start = timeout_ms + start;
                    break;
                }
                if(strstr(line, "remote index out of range")) {
                    strncpy(
                        message,
                        "Remote index out of range",
                        sizeof(state->confirmation_message) - 1);
                    message[sizeof(state->confirmation_message) - 1] = '\0';
                    start = timeout_ms + start;
                    break;
                }
                if(strstr(line, "index out of range")) {
                    strncpy(
                        message,
                        "Button index out of range",
                        sizeof(state->confirmation_message) - 1);
                    message[sizeof(state->confirmation_message) - 1] = '\0';
                    start = timeout_ms + start;
                    break;
                }
                if(strstr(line, "invalid universal index")) {
                    strncpy(
                        message,
                        "Invalid universal index",
                        sizeof(state->confirmation_message) - 1);
                    message[sizeof(state->confirmation_message) - 1] = '\0';
                    start = timeout_ms + start;
                    break;
                }
            } else if(is_uni_sendall) {
                if(strstr(line, "universal sendall already running")) {
                    strncpy(
                        message,
                        "Universal send already running; use 'stop' to cancel.",
                        sizeof(state->confirmation_message) - 1);
                    message[sizeof(state->confirmation_message) - 1] = '\0';
                    start = timeout_ms + start;
                    break;
                }
                if(strstr(line, "universal sendall started")) {
                }
                if(strstr(line, "no builtin signals named")) {
                    strncpy(
                        message,
                        "No builtin signals with that name.",
                        sizeof(state->confirmation_message) - 1);
                    message[sizeof(state->confirmation_message) - 1] = '\0';
                    start = timeout_ms + start;
                    break;
                }
                if(strstr(line, "no signals named")) {
                    strncpy(
                        message,
                        "No file signals with that name.",
                        sizeof(state->confirmation_message) - 1);
                    message[sizeof(state->confirmation_message) - 1] = '\0';
                    start = timeout_ms + start;
                    break;
                }
                if(strstr(line, "universal sendall finished")) {
                    strncpy(
                        message,
                        "Universal send finished.",
                        sizeof(state->confirmation_message) - 1);
                    message[sizeof(state->confirmation_message) - 1] = '\0';
                    confirmation_view_set_header(state->confirmation_view, "Universal send");
                    confirmation_view_set_text(state->confirmation_view, message);
                    confirmation_view_set_ok_callback(
                        state->confirmation_view, app_info_ok_callback, state);
                    confirmation_view_set_cancel_callback(
                        state->confirmation_view, app_info_ok_callback, state);
                    start = timeout_ms + start;
                    break;
                }
                if(strstr(line, "universal sendall stopped")) {
                    strncpy(
                        message,
                        "Universal send stopped.",
                        sizeof(state->confirmation_message) - 1);
                    message[sizeof(state->confirmation_message) - 1] = '\0';
                    confirmation_view_set_header(state->confirmation_view, "Universal send");
                    confirmation_view_set_text(state->confirmation_view, message);
                    confirmation_view_set_ok_callback(
                        state->confirmation_view, app_info_ok_callback, state);
                    confirmation_view_set_cancel_callback(
                        state->confirmation_view, app_info_ok_callback, state);
                    start = timeout_ms + start;
                    break;
                }
            }
        }

        if(message[0]) break;
    }

    if(!message[0] && saw_ok) {
        strncpy(message, "Send OK", sizeof(state->confirmation_message) - 1);
        message[sizeof(state->confirmation_message) - 1] = '\0';
    }

    if(message[0]) {
        if(strncmp(message, "Send OK", 7) == 0) {
            const char* body = message + 7;
            if(*body == '\n') body++;
            confirmation_view_set_header(state->confirmation_view, "Sent Successfully");
            confirmation_view_set_text(state->confirmation_view, body);
        } else {
            confirmation_view_set_text(state->confirmation_view, message);
        }
    } else if(have_output) {
        char display[256];
        snprintf(display, sizeof(display), "No match.\nRaw:\n%.180s", raw_buffer);
        confirmation_view_set_text(state->confirmation_view, display);
    } else {
        confirmation_view_set_text(state->confirmation_view, "No response from ESP.");
    }

    return true;
}

static bool handle_ir_command_feedback(AppState* state, const char* cmd) {
    return handle_ir_command_feedback_ex(state, cmd, true, true);
}

// Text input callback implementation
static void text_input_result_callback(void* context) {
    AppState* input_state = (AppState*)context;
    if(input_state->connect_input_stage == 1) {
        size_t len = strlen(input_state->input_buffer);
        if(len >= sizeof(input_state->connect_ssid)) len = sizeof(input_state->connect_ssid) - 1;
        memcpy(input_state->connect_ssid, input_state->input_buffer, len);
        input_state->connect_ssid[len] = '\0';
        input_state->connect_input_stage = 2;
        if(input_state->input_buffer) memset(input_state->input_buffer, 0, INPUT_BUFFER_SIZE);
        text_input_reset(input_state->text_input);
        text_input_set_header_text(input_state->text_input, "PASSWORD");
        text_input_set_result_callback(
            input_state->text_input,
            text_input_result_callback,
            input_state,
            input_state->input_buffer,
            INPUT_BUFFER_SIZE,
            true);
#ifdef HAS_MOMENTUM_SUPPORT
        text_input_show_illegal_symbols(input_state->text_input, true);
#endif
        view_dispatcher_switch_to_view(input_state->view_dispatcher, VIEW_TEXT_INPUT);
        return;
    }
    if(input_state->connect_input_stage == 2) {
        char buffer[256];
        snprintf(
            buffer,
            sizeof(buffer),
            "connect \"%s\" \"%s\"\n",
            input_state->connect_ssid,
            input_state->input_buffer);
        uart_send(input_state->uart_context, (uint8_t*)buffer, strlen(buffer));
        input_state->connect_input_stage = 0;
        input_state->connect_ssid[0] = '\0';
    } else {
        if(input_state->uart_command && strcmp(input_state->uart_command, "ir send") == 0) {
            char cmd[256];
            snprintf(cmd, sizeof(cmd), "ir send %s\n", input_state->input_buffer);
            handle_ir_command_feedback_ex(input_state, cmd, true, true);
        } else {
            send_uart_command_with_text(
                input_state->uart_command, input_state->input_buffer, input_state);
            uart_receive_data(
                input_state->uart_context, input_state->view_dispatcher, input_state, "", "", "");
        }
    }
    if(input_state->input_buffer) memset(input_state->input_buffer, 0, INPUT_BUFFER_SIZE);
}

static void send_ir_file(AppState* state) {
    uint8_t* ir_data = NULL;
    size_t ir_size = 0;

    if(!ghost_esp_ep_read_ir_file(state, &ir_data, &ir_size)) {
        return;
    }

    // Clear any cached buffer; we stream now
    if(state->ir_file_buffer) {
        free(state->ir_file_buffer);
        state->ir_file_buffer = NULL;
        state->ir_file_buffer_size = 0;
    }

    state->ir_universal_buttons_mode = false;
    state->ir_file_buttons_mode = true;

    if(!ir_index_buttons_from_file(state)) {
        state->ir_file_buttons_mode = false;
        ir_show_error(state, "No IR buttons found.");
        return;
    }

    ir_show_buttons_menu(state);
}

static void ir_send_button_from_file(AppState* state, uint32_t button_index) {
    if(!state || !state->uart_context) return;
    if(button_index >= state->ir_signal_count) return;
    if(!state->ir_file_path[0]) return;

    size_t start = state->ir_signal_block_offsets[button_index];
    size_t payload_len = state->ir_signal_block_lengths[button_index];
    if(payload_len == 0) return;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, state->ir_file_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return;
    }

    storage_file_seek(file, start, true);

    const size_t chunk = 512;
    uint8_t buf[chunk];
    const char* ir_begin_marker = "[IR/BEGIN]";
    const char* ir_close_marker = "[IR/CLOSE]";

    uart_reset_text_buffers(state->uart_context);
    uart_send(state->uart_context, (const uint8_t*)ir_begin_marker, 10);
    uart_send(state->uart_context, (const uint8_t*)"\n", 1);
    size_t remaining = payload_len;
    while(remaining > 0) {
        size_t to_read = (remaining > chunk) ? chunk : remaining;
        uint16_t read = storage_file_read(file, buf, (uint16_t)to_read);
        if(read == 0) break;
        uart_send(state->uart_context, buf, read);
        remaining -= read;
    }
    uart_send(state->uart_context, (const uint8_t*)ir_close_marker, 10);
    uart_send(state->uart_context, (const uint8_t*)"\n", 1);

    handle_ir_command_feedback_ex(state, "ir inline", false, false);

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void send_evil_portal_html(AppState* state) {
    uint8_t* the_html = NULL;
    size_t html_size = 0;

    if(ghost_esp_ep_read_html_file(state, &the_html, &html_size)) {
        if(the_html != NULL) {
            // Send the command first
            const char* command_str = "evilportal -c sethtmlstr\n";
            uart_send(state->uart_context, (const uint8_t*)command_str, strlen(command_str));

            // Begin HTML block
            const char* html_begin_marker = "[HTML/BEGIN]";
            uart_send(state->uart_context, (const uint8_t*)html_begin_marker, 12);

            // Send HTML content
            uart_send(state->uart_context, the_html, html_size);

            // End HTML block
            const char* html_close_marker = "[HTML/CLOSE]";
            uart_send(state->uart_context, (const uint8_t*)html_close_marker, 12);
            uart_send(state->uart_context, (const uint8_t*)"\n", 1);

            free(the_html);
        }
    } else {
        // Only free if read failed but buffer was allocated (unlikely but safe)
        if(the_html) free(the_html);
    }
}

static void execute_menu_command(AppState* state, const MenuCommand* command) {
    if(strcmp(command->command, "set_evil_portal_html") == 0) {
        send_evil_portal_html(state);
        return;
    }
    if(strcmp(command->command, "send_ir_file") == 0) {
        // Ensure capture streams are cleaned up before opening file browser
        if(state->uart_context) {
            uart_cleanup_capture_streams(state->uart_context);
        }
        send_ir_file(state);
        return;
    }
    if(!uart_is_esp_connected(state->uart_context)) {
        state->previous_view = state->current_view;
        confirmation_view_set_header(state->confirmation_view, "Connection Error");
        confirmation_view_set_text(
            state->confirmation_view,
            "No response from ESP!\nIs a command running?\nRestart the "
            "app.\nRestart ESP.\nCheck UART Pins.\nReflash if issues persist.\nYou "
            "can disable this check in the settings menu.\n\n");
        confirmation_view_set_ok_callback(state->confirmation_view, error_callback, state);
        confirmation_view_set_cancel_callback(state->confirmation_view, error_callback, state);

        view_dispatcher_switch_to_view(state->view_dispatcher, VIEW_CONFIRMATION);
        state->current_view = VIEW_CONFIRMATION;
        return;
    }

    if(!command->needs_input && !command->needs_confirmation && !command->capture_prefix &&
       !command->file_ext && !command->folder) {
        if(handle_ir_command_feedback(state, command->command)) {
            return;
        }
    }

    if(command->needs_input && strcmp(command->command, "connect") == 0) {
        state->connect_input_stage = 1;
        state->uart_command = command->command;
        state->previous_view = state->current_view;
        text_input_reset(state->text_input);
        if(state->input_buffer) memset(state->input_buffer, 0, INPUT_BUFFER_SIZE);
        text_input_set_header_text(state->text_input, "SSID");
        text_input_set_result_callback(
            state->text_input,
            text_input_result_callback,
            state,
            state->input_buffer,
            INPUT_BUFFER_SIZE,
            true);
#ifdef HAS_MOMENTUM_SUPPORT
        text_input_show_illegal_symbols(state->text_input, true);
#endif
        view_dispatcher_switch_to_view(state->view_dispatcher, VIEW_TEXT_INPUT);
        state->current_view = VIEW_TEXT_INPUT;
        return;
    }

    // For commands needing input
    if(command->needs_input) {
        state->uart_command = command->command;
        state->previous_view = state->current_view;
        text_input_reset(state->text_input);
        if(state->input_buffer) memset(state->input_buffer, 0, INPUT_BUFFER_SIZE);
        text_input_set_header_text(state->text_input, command->input_text);
        text_input_set_result_callback(
            state->text_input,
            text_input_result_callback,
            state,
            state->input_buffer,
            INPUT_BUFFER_SIZE,
            true);
#ifdef HAS_MOMENTUM_SUPPORT
        text_input_show_illegal_symbols(state->text_input, true);
#endif
        view_dispatcher_switch_to_view(state->view_dispatcher, VIEW_TEXT_INPUT);
        state->current_view = VIEW_TEXT_INPUT;
        return;
    }

    // For commands needing confirmation
    if(command->needs_confirmation) {
        MenuCommandContext* cmd_ctx = malloc(sizeof(MenuCommandContext));
        cmd_ctx->state = state;
        cmd_ctx->command = command;
        state->active_confirm_context = cmd_ctx;
        confirmation_view_set_header(state->confirmation_view, command->confirm_header);
        confirmation_view_set_text(state->confirmation_view, command->confirm_text);
        confirmation_view_set_ok_callback(
            state->confirmation_view, confirmation_ok_callback, cmd_ctx);
        confirmation_view_set_cancel_callback(
            state->confirmation_view, confirmation_cancel_callback, cmd_ctx);

        view_dispatcher_switch_to_view(state->view_dispatcher, VIEW_CONFIRMATION);
        state->current_view = VIEW_CONFIRMATION;
        return;
    }

    // Handle variable sniff command
    if(state->current_view == VIEW_WIFI_CAPTURE && state->current_index == 0) {
        const SniffCommandDef* current_sniff = &sniff_commands[current_sniff_index];
        // Handle capture commands
        if(current_sniff->capture_prefix) {
            // Save current view for proper back navigation
            state->previous_view = state->current_view;
            bool file_opened = uart_receive_data(
                state->uart_context,
                state->view_dispatcher,
                state,
                current_sniff->capture_prefix,
                "pcap",
                GHOST_ESP_APP_FOLDER_PCAPS);

            if(!file_opened) {
                FURI_LOG_E("Capture", "Failed to open capture file");
                return;
            }

            furi_delay_ms(10);
            send_uart_command(current_sniff->command, state);
            state->current_view = VIEW_TEXT_BOX;
            return;
        }

        // Save view and show terminal log
        state->previous_view = state->current_view;
        uart_receive_data(state->uart_context, state->view_dispatcher, state, "", "", "");
        state->current_view = VIEW_TEXT_BOX;

        furi_delay_ms(5);
        send_uart_command(current_sniff->command, state);
        return;
    }

    // Handle variable beacon spam command
    if(state->current_view == VIEW_WIFI_ATTACK && state->current_index == 0) {
        const CyclingMenuDef* current_beacon = &beacon_spam_commands[current_beacon_index];

        // If it's custom mode (last index), handle text input
        if(current_beacon_index == COUNT_OF(beacon_spam_commands) - 1) {
            state->uart_command = current_beacon->command;
            // Save current view for proper back navigation
            state->previous_view = state->current_view;
            text_input_reset(state->text_input);
            text_input_set_header_text(state->text_input, "SSID Name");
            text_input_set_result_callback(
                state->text_input,
                text_input_result_callback,
                state,
                state->input_buffer,
                INPUT_BUFFER_SIZE,
                true);
#ifdef HAS_MOMENTUM_SUPPORT
            text_input_show_illegal_symbols(state->text_input, true);
#endif
            view_dispatcher_switch_to_view(state->view_dispatcher, VIEW_TEXT_INPUT);
            state->current_view = VIEW_TEXT_INPUT;
            return;
        }

        // Save view and show terminal log
        state->previous_view = state->current_view;
        uart_receive_data(state->uart_context, state->view_dispatcher, state, "", "", "");
        state->current_view = VIEW_TEXT_BOX;
        furi_delay_ms(5);
        send_uart_command(current_beacon->command, state);
        return;
    }

    // Handle variable rgbmode command (LED sub-category, index 0)
    if(state->current_view == VIEW_WIFI_SETTINGS_LED && state->current_index == 0) {
        const CyclingMenuDef* current_rgb = &rgbmode_commands[current_rgb_index];
        // Save view and show terminal log
        state->previous_view = state->current_view;
        uart_receive_data(state->uart_context, state->view_dispatcher, state, "", "", "");
        state->current_view = VIEW_TEXT_BOX;
        furi_delay_ms(5);
        send_uart_command(current_rgb->command, state);
        return;
    }

    // Handle variable WiFi scan command (scan modes like APs / APs Live / Stations / All)
    if(state->current_view == VIEW_WIFI_SCANNING && state->current_index == 0) {
        const CyclingMenuDef* current_scan = &wifi_scan_modes[current_wifi_scan_index];
        // Save view and show terminal log
        state->previous_view = state->current_view;
        uart_receive_data(state->uart_context, state->view_dispatcher, state, "", "", "");
        state->current_view = VIEW_TEXT_BOX;
        furi_delay_ms(5);
        send_uart_command(current_scan->command, state);
        return;
    }

    // Handle variable BLE spam command
    if(state->current_view == VIEW_BLE_ATTACK && state->current_index == 0) {
        const CyclingMenuDef* current_ble_spam = &ble_spam_commands[current_ble_spam_index];
        // Save view and show terminal log
        state->previous_view = state->current_view;
        uart_receive_data(state->uart_context, state->view_dispatcher, state, "", "", "");
        state->current_view = VIEW_TEXT_BOX;
        furi_delay_ms(5);
        send_uart_command(current_ble_spam->command, state);
        return;
    }

    // Handle capture commands
    if(command->capture_prefix || command->file_ext || command->folder) {
        // Save view and show terminal log
        state->previous_view = state->current_view;
        bool file_opened = uart_receive_data(
            state->uart_context,
            state->view_dispatcher,
            state,
            command->capture_prefix ? command->capture_prefix : "",
            command->file_ext ? command->file_ext : "",
            command->folder ? command->folder : "");

        if(!file_opened) {
            FURI_LOG_E("Capture", "Failed to open capture file");
            return;
        }

        furi_delay_ms(10);
        send_uart_command(command->command, state);
        state->current_view = VIEW_TEXT_BOX;
        return;
    }

    // Save view and show terminal log
    state->previous_view = state->current_view;
    uart_receive_data(state->uart_context, state->view_dispatcher, state, "", "", "");

    furi_delay_ms(5);
    send_uart_command(command->command, state);
}

// Menu display function implementation
static void show_menu(
    AppState* state,
    const MenuCommand* commands,
    size_t command_count,
    const char* header,
    Submenu* menu,
    uint8_t view_id) {
    submenu_reset(menu);
    submenu_set_header(menu, header);

    for(size_t i = 0; i < command_count; i++) {
        submenu_add_item(menu, commands[i].label, i, submenu_callback, state);
    }

    // Set up view with input handler
    View* menu_view = submenu_get_view(menu);
    view_set_context(menu_view, state);
    view_set_input_callback(menu_view, menu_input_handler);

    // Restore last selection based on menu type
    uint32_t last_index = 0;
    switch(view_id) {
    case VIEW_WIFI: // WiFi categories
        last_index = state->last_wifi_category_index;
        break;
    case VIEW_WIFI_SCANNING: // WiFi Scanning
        last_index = state->last_wifi_scanning_index;
        break;
    case VIEW_WIFI_CAPTURE: // WiFi Capture
        last_index = state->last_wifi_capture_index;
        break;
    case VIEW_WIFI_ATTACK: // WiFi Attack
        last_index = state->last_wifi_attack_index;
        break;
    case VIEW_WIFI_NETWORK: // WiFi Network
        last_index = state->last_wifi_network_index;
        break;
    case VIEW_WIFI_SETTINGS: // WiFi Settings
        last_index = state->last_wifi_settings_index;
        break;
    case VIEW_BLE: // BLE categories
        last_index = state->last_ble_category_index;
        break;
    case VIEW_BLE_SCANNING: // BLE Scanning
        last_index = state->last_ble_scanning_index;
        break;
    case VIEW_BLE_CAPTURE: // BLE Capture
        last_index = state->last_ble_capture_index;
        break;
    case VIEW_BLE_ATTACK: // BLE Attack
        last_index = state->last_ble_attack_index;
        break;
    case VIEW_GPS: // GPS
        last_index = state->last_gps_index;
        break;
    case VIEW_AERIAL: // Aerial
        last_index = state->last_aerial_category_index;
        break;
    // WiFi Settings sub-categories
    case VIEW_WIFI_SETTINGS_LED:
        last_index = state->last_wifi_settings_led_index;
        break;
    case VIEW_WIFI_SETTINGS_SD:
        last_index = state->last_wifi_settings_sd_index;
        break;
    case VIEW_WIFI_SETTINGS_MGMT:
        last_index = state->last_wifi_settings_mgmt_index;
        break;
    case VIEW_WIFI_SETTINGS_DEVICE:
        last_index = state->last_wifi_settings_device_index;
        break;
    case VIEW_WIFI_SETTINGS_MISC:
        last_index = state->last_wifi_settings_misc_index;
        break;
    // WiFi Network sub-categories
    case VIEW_WIFI_NETWORK_PORTAL:
        last_index = state->last_wifi_network_portal_index;
        break;
    case VIEW_WIFI_NETWORK_CONN:
        last_index = state->last_wifi_network_conn_index;
        break;
    case VIEW_WIFI_NETWORK_SCAN:
        last_index = state->last_wifi_network_scan_index;
        break;
    case VIEW_WIFI_NETWORK_IOT:
        last_index = state->last_wifi_network_iot_index;
        break;
    case VIEW_WIFI_NETWORK_WEBUI:
        last_index = state->last_wifi_network_webui_index;
        break;
    // BLE Scanning sub-categories
    case VIEW_BLE_SCAN_DETECT:
        last_index = state->last_ble_scan_detect_index;
        break;
    case VIEW_BLE_SCAN_AIRTAG:
        last_index = state->last_ble_scan_airtag_index;
        break;
    case VIEW_BLE_SCAN_GATT:
        last_index = state->last_ble_scan_gatt_index;
        break;
    case VIEW_BLE_SCAN_ADV:
        last_index = state->last_ble_scan_adv_index;
        break;
    // GPS sub-categories
    case VIEW_GPS_CONFIG:
        last_index = state->last_gps_config_index;
        break;
    case VIEW_GPS_WARDRIVE:
        last_index = state->last_gps_wardrive_index;
        break;
    }
    if(last_index < command_count) {
        submenu_set_selected_item(menu, last_index);
    }

    state->previous_view = state->current_view;
    view_dispatcher_switch_to_view(state->view_dispatcher, view_id);
    state->current_view = view_id;
}

// Menu display functions
void show_wifi_scanning_menu(AppState* state) {
    show_menu(
        state,
        wifi_scanning_commands,
        COUNT_OF(wifi_scanning_commands),
        "Scanning & Probing",
        state->wifi_scanning_menu,
        10);

    // Ensure the first item label reflects the currently selected scan mode
    // (so the menu shows "Scan: (APs Live)" etc. after cycling)
    submenu_change_item_label(
        state->wifi_scanning_menu, 0, wifi_scan_modes[current_wifi_scan_index].label);

    // Also persist labels for list/select/listen cycling entries
    submenu_change_item_label(
        state->wifi_scanning_menu, 1, wifi_list_modes[current_wifi_list_index].label);
    submenu_change_item_label(
        state->wifi_scanning_menu, 2, wifi_select_modes[current_wifi_select_index].label);
    submenu_change_item_label(
        state->wifi_scanning_menu, 3, wifi_listen_modes[current_wifi_listen_index].label);
}

void show_wifi_capture_menu(AppState* state) {
    show_menu(
        state,
        wifi_capture_commands,
        COUNT_OF(wifi_capture_commands),
        "Packet Capture",
        state->wifi_capture_menu,
        11);
}

void show_wifi_attack_menu(AppState* state) {
    show_menu(
        state,
        wifi_attack_commands,
        COUNT_OF(wifi_attack_commands),
        "Attacks",
        state->wifi_attack_menu,
        12);

    // Ensure beacon spam cycling label persists
    submenu_change_item_label(
        state->wifi_attack_menu, 0, beacon_spam_commands[current_beacon_index].label);
}

void show_wifi_network_menu(AppState* state) {
    submenu_reset(state->wifi_network_menu);
    submenu_set_header(state->wifi_network_menu, "Portal & Network");
    submenu_add_item(state->wifi_network_menu, "Evil Portal >", 0, submenu_callback, state);
    submenu_add_item(state->wifi_network_menu, "WiFi Connection >", 1, submenu_callback, state);
    submenu_add_item(state->wifi_network_menu, "Network Scan >", 2, submenu_callback, state);
    submenu_add_item(state->wifi_network_menu, "IoT Control >", 3, submenu_callback, state);
    submenu_add_item(state->wifi_network_menu, "WebUI >", 4, submenu_callback, state);
    submenu_set_selected_item(state->wifi_network_menu, state->last_wifi_network_index);
    view_dispatcher_switch_to_view(state->view_dispatcher, VIEW_WIFI_NETWORK);
    state->current_view = VIEW_WIFI_NETWORK;
}

void show_wifi_settings_menu(AppState* state) {
    submenu_reset(state->wifi_settings_menu);
    submenu_set_header(state->wifi_settings_menu, "Settings & Hardware");
    submenu_add_item(state->wifi_settings_menu, "LED & RGB >", 0, submenu_callback, state);
    submenu_add_item(state->wifi_settings_menu, "SD Card >", 1, submenu_callback, state);
    submenu_add_item(state->wifi_settings_menu, "Settings Mgmt >", 2, submenu_callback, state);
    submenu_add_item(state->wifi_settings_menu, "Device & Debug >", 3, submenu_callback, state);
    submenu_add_item(state->wifi_settings_menu, "Misc >", 4, submenu_callback, state);
    submenu_set_selected_item(state->wifi_settings_menu, state->last_wifi_settings_index);
    view_dispatcher_switch_to_view(state->view_dispatcher, VIEW_WIFI_SETTINGS);
    state->current_view = VIEW_WIFI_SETTINGS;
}

// WiFi Settings sub-category show functions
void show_wifi_settings_led_menu(AppState* state) {
    show_menu(
        state,
        wifi_settings_led_commands,
        COUNT_OF(wifi_settings_led_commands),
        "LED & RGB",
        state->wifi_settings_led_menu,
        VIEW_WIFI_SETTINGS_LED);
    submenu_change_item_label(
        state->wifi_settings_led_menu, 0, rgbmode_commands[current_rgb_index].label);
}

void show_wifi_settings_sd_menu(AppState* state) {
    show_menu(
        state,
        wifi_settings_sd_commands,
        COUNT_OF(wifi_settings_sd_commands),
        "SD Card",
        state->wifi_settings_sd_menu,
        VIEW_WIFI_SETTINGS_SD);
}

void show_wifi_settings_mgmt_menu(AppState* state) {
    show_menu(
        state,
        wifi_settings_mgmt_commands,
        COUNT_OF(wifi_settings_mgmt_commands),
        "Settings Mgmt",
        state->wifi_settings_mgmt_menu,
        VIEW_WIFI_SETTINGS_MGMT);
}

void show_wifi_settings_device_menu(AppState* state) {
    show_menu(
        state,
        wifi_settings_device_commands,
        COUNT_OF(wifi_settings_device_commands),
        "Device & Debug",
        state->wifi_settings_device_menu,
        VIEW_WIFI_SETTINGS_DEVICE);
}

void show_wifi_settings_misc_menu(AppState* state) {
    show_menu(
        state,
        wifi_settings_misc_commands,
        COUNT_OF(wifi_settings_misc_commands),
        "Misc",
        state->wifi_settings_misc_menu,
        VIEW_WIFI_SETTINGS_MISC);
}

// WiFi Network sub-category show functions
void show_wifi_network_portal_menu(AppState* state) {
    show_menu(
        state,
        wifi_network_portal_commands,
        COUNT_OF(wifi_network_portal_commands),
        "Evil Portal",
        state->wifi_network_portal_menu,
        VIEW_WIFI_NETWORK_PORTAL);
}

void show_wifi_network_conn_menu(AppState* state) {
    show_menu(
        state,
        wifi_network_conn_commands,
        COUNT_OF(wifi_network_conn_commands),
        "WiFi Connection",
        state->wifi_network_conn_menu,
        VIEW_WIFI_NETWORK_CONN);
}

void show_wifi_network_scan_menu(AppState* state) {
    show_menu(
        state,
        wifi_network_scan_commands,
        COUNT_OF(wifi_network_scan_commands),
        "Network Scan",
        state->wifi_network_scan_menu,
        VIEW_WIFI_NETWORK_SCAN);
}

void show_wifi_network_iot_menu(AppState* state) {
    show_menu(
        state,
        wifi_network_iot_commands,
        COUNT_OF(wifi_network_iot_commands),
        "IoT Control",
        state->wifi_network_iot_menu,
        VIEW_WIFI_NETWORK_IOT);
}

void show_wifi_network_webui_menu(AppState* state) {
    show_menu(
        state,
        wifi_network_webui_commands,
        COUNT_OF(wifi_network_webui_commands),
        "WebUI",
        state->wifi_network_webui_menu,
        VIEW_WIFI_NETWORK_WEBUI);
}

void show_status_idle_menu(AppState* state) {
    show_menu(
        state,
        status_idle_commands,
        COUNT_OF(status_idle_commands),
        "Select an animation",
        state->status_idle_menu,
        40);
}

void show_ble_scanning_menu(AppState* state) {
    submenu_reset(state->ble_scanning_menu);
    submenu_set_header(state->ble_scanning_menu, "Scanning & Detection");
    submenu_add_item(state->ble_scanning_menu, "Flipper & Skimmer >", 0, submenu_callback, state);
    submenu_add_item(state->ble_scanning_menu, "AirTag >", 1, submenu_callback, state);
    submenu_add_item(state->ble_scanning_menu, "GATT >", 2, submenu_callback, state);
    submenu_add_item(
        state->ble_scanning_menu, "Advertisers & Traffic >", 3, submenu_callback, state);
    submenu_set_selected_item(state->ble_scanning_menu, state->last_ble_scanning_index);
    view_dispatcher_switch_to_view(state->view_dispatcher, VIEW_BLE_SCANNING);
    state->current_view = VIEW_BLE_SCANNING;
}

// BLE Scanning sub-category show functions
void show_ble_scan_detect_menu(AppState* state) {
    show_menu(
        state,
        ble_scan_detect_commands,
        COUNT_OF(ble_scan_detect_commands),
        "Flipper & Skimmer",
        state->ble_scan_detect_menu,
        VIEW_BLE_SCAN_DETECT);
}

void show_ble_scan_airtag_menu(AppState* state) {
    show_menu(
        state,
        ble_scan_airtag_commands,
        COUNT_OF(ble_scan_airtag_commands),
        "AirTag",
        state->ble_scan_airtag_menu,
        VIEW_BLE_SCAN_AIRTAG);
}

void show_ble_scan_gatt_menu(AppState* state) {
    show_menu(
        state,
        ble_scan_gatt_commands,
        COUNT_OF(ble_scan_gatt_commands),
        "GATT",
        state->ble_scan_gatt_menu,
        VIEW_BLE_SCAN_GATT);
}

void show_ble_scan_adv_menu(AppState* state) {
    show_menu(
        state,
        ble_scan_adv_commands,
        COUNT_OF(ble_scan_adv_commands),
        "Advertisers & Traffic",
        state->ble_scan_adv_menu,
        VIEW_BLE_SCAN_ADV);
}

void show_ble_capture_menu(AppState* state) {
    show_menu(
        state,
        ble_capture_commands,
        COUNT_OF(ble_capture_commands),
        "Packet Capture",
        state->ble_capture_menu,
        21);
}

void show_ble_attack_menu(AppState* state) {
    show_menu(
        state,
        ble_attack_commands,
        COUNT_OF(ble_attack_commands),
        "Attacks & Spoofing",
        state->ble_attack_menu,
        22);

    // Ensure BLE spam cycling label persists
    submenu_change_item_label(
        state->ble_attack_menu, 0, ble_spam_commands[current_ble_spam_index].label);
}

void show_wifi_menu(AppState* state) {
    submenu_reset(state->wifi_menu);
    submenu_set_header(state->wifi_menu, "WiFi Commands");
    submenu_add_item(state->wifi_menu, "Scanning & Probing > ", 0, submenu_callback, state);
    submenu_add_item(state->wifi_menu, "Packet Capture > ", 1, submenu_callback, state);
    submenu_add_item(state->wifi_menu, "Attacks > ", 2, submenu_callback, state);
    submenu_add_item(state->wifi_menu, "Evil Portal & Network >", 3, submenu_callback, state);
    submenu_add_item(state->wifi_menu, "Aerial Detector >", 4, submenu_callback, state);
    submenu_add_item(state->wifi_menu, wifi_stop_command.label, 5, submenu_callback, state);
    // Restore last selected WiFi category
    submenu_set_selected_item(state->wifi_menu, state->last_wifi_category_index);

    view_dispatcher_switch_to_view(state->view_dispatcher, VIEW_WIFI);
    state->current_view = VIEW_WIFI;
}

void show_aerial_menu(AppState* state) {
    show_menu(
        state,
        aerial_commands,
        COUNT_OF(aerial_commands),
        "Aerial Detector:",
        state->aerial_menu,
        15);
}

void show_ble_menu(AppState* state) {
    submenu_reset(state->ble_menu);
    submenu_set_header(state->ble_menu, "BLE Commands");
    submenu_add_item(state->ble_menu, "Scanning & Detection >", 0, submenu_callback, state);
    submenu_add_item(state->ble_menu, "Packet Capture >", 1, submenu_callback, state);
    submenu_add_item(state->ble_menu, "Attacks & Spoofing >", 2, submenu_callback, state);
    submenu_add_item(state->ble_menu, ble_stop_command.label, 3, submenu_callback, state);
    // Restore last selected BLE category
    submenu_set_selected_item(state->ble_menu, state->last_ble_category_index);

    view_dispatcher_switch_to_view(state->view_dispatcher, VIEW_BLE);
    state->current_view = VIEW_BLE;
}

void show_gps_menu(AppState* state) {
    state->came_from_settings = false;
    submenu_reset(state->gps_menu);
    submenu_set_header(state->gps_menu, "GPS Commands");
    submenu_add_item(state->gps_menu, "GPS Config >", 0, submenu_callback, state);
    submenu_add_item(state->gps_menu, "Wardriving >", 1, submenu_callback, state);
    submenu_set_selected_item(state->gps_menu, state->last_gps_index);
    view_dispatcher_switch_to_view(state->view_dispatcher, VIEW_GPS);
    state->current_view = VIEW_GPS;
}

// GPS sub-category show functions
void show_gps_config_menu(AppState* state) {
    show_menu(
        state,
        gps_config_commands,
        COUNT_OF(gps_config_commands),
        "GPS Config",
        state->gps_config_menu,
        VIEW_GPS_CONFIG);
}

void show_gps_wardrive_menu(AppState* state) {
    show_menu(
        state,
        gps_wardrive_commands,
        COUNT_OF(gps_wardrive_commands),
        "Wardriving",
        state->gps_wardrive_menu,
        VIEW_GPS_WARDRIVE);
}

void show_ir_menu(AppState* state) {
    show_menu(state, ir_commands, COUNT_OF(ir_commands), "IR Commands:", state->ir_menu, 30);
}

// Menu command handlers
void handle_wifi_menu(AppState* state, uint32_t index) {
    const MenuCommand* command = NULL;
    switch(state->current_view) {
    case VIEW_WIFI_SCANNING: // Scanning
        if(index < COUNT_OF(wifi_scanning_commands)) {
            command = &wifi_scanning_commands[index];
            state->last_wifi_scanning_index = index;
        }
        break;
    case VIEW_WIFI_CAPTURE: // Capture
        if(index < COUNT_OF(wifi_capture_commands)) {
            command = &wifi_capture_commands[index];
            state->last_wifi_capture_index = index;
        }
        break;
    case VIEW_WIFI_ATTACK: // Attack
        if(index < COUNT_OF(wifi_attack_commands)) {
            command = &wifi_attack_commands[index];
            state->last_wifi_attack_index = index;
        }
        break;
    // WiFi Settings sub-categories
    case VIEW_WIFI_SETTINGS_LED:
        if(index < COUNT_OF(wifi_settings_led_commands)) {
            command = &wifi_settings_led_commands[index];
            state->last_wifi_settings_led_index = index;
        }
        break;
    case VIEW_WIFI_SETTINGS_SD:
        if(index < COUNT_OF(wifi_settings_sd_commands)) {
            command = &wifi_settings_sd_commands[index];
            state->last_wifi_settings_sd_index = index;
        }
        break;
    case VIEW_WIFI_SETTINGS_MGMT:
        if(index < COUNT_OF(wifi_settings_mgmt_commands)) {
            command = &wifi_settings_mgmt_commands[index];
            state->last_wifi_settings_mgmt_index = index;
        }
        break;
    case VIEW_WIFI_SETTINGS_DEVICE:
        if(index < COUNT_OF(wifi_settings_device_commands)) {
            command = &wifi_settings_device_commands[index];
            state->last_wifi_settings_device_index = index;
        }
        break;
    case VIEW_WIFI_SETTINGS_MISC:
        if(index < COUNT_OF(wifi_settings_misc_commands)) {
            command = &wifi_settings_misc_commands[index];
            state->last_wifi_settings_misc_index = index;
        }
        break;
    // WiFi Network sub-categories
    case VIEW_WIFI_NETWORK_PORTAL:
        if(index < COUNT_OF(wifi_network_portal_commands)) {
            command = &wifi_network_portal_commands[index];
            state->last_wifi_network_portal_index = index;
        }
        break;
    case VIEW_WIFI_NETWORK_CONN:
        if(index < COUNT_OF(wifi_network_conn_commands)) {
            command = &wifi_network_conn_commands[index];
            state->last_wifi_network_conn_index = index;
        }
        break;
    case VIEW_WIFI_NETWORK_SCAN:
        if(index < COUNT_OF(wifi_network_scan_commands)) {
            command = &wifi_network_scan_commands[index];
            state->last_wifi_network_scan_index = index;
        }
        break;
    case VIEW_WIFI_NETWORK_IOT:
        if(index < COUNT_OF(wifi_network_iot_commands)) {
            command = &wifi_network_iot_commands[index];
            state->last_wifi_network_iot_index = index;
        }
        break;
    case VIEW_WIFI_NETWORK_WEBUI:
        if(index < COUNT_OF(wifi_network_webui_commands)) {
            command = &wifi_network_webui_commands[index];
            state->last_wifi_network_webui_index = index;
        }
        break;
    }

    if(command) {
        execute_menu_command(state, command);
    }
}

void handle_ble_menu(AppState* state, uint32_t index) {
    const MenuCommand* command = NULL;
    switch(state->current_view) {
    case VIEW_BLE_CAPTURE: // Capture
        if(index < COUNT_OF(ble_capture_commands)) {
            command = &ble_capture_commands[index];
            state->last_ble_capture_index = index;
        }
        break;
    case VIEW_BLE_ATTACK: // Attack
        if(index < COUNT_OF(ble_attack_commands)) {
            command = &ble_attack_commands[index];
            state->last_ble_attack_index = index;
        }
        break;
    // BLE Scanning sub-categories
    case VIEW_BLE_SCAN_DETECT:
        if(index < COUNT_OF(ble_scan_detect_commands)) {
            command = &ble_scan_detect_commands[index];
            state->last_ble_scan_detect_index = index;
        }
        break;
    case VIEW_BLE_SCAN_AIRTAG:
        if(index < COUNT_OF(ble_scan_airtag_commands)) {
            command = &ble_scan_airtag_commands[index];
            state->last_ble_scan_airtag_index = index;
        }
        break;
    case VIEW_BLE_SCAN_GATT:
        if(index < COUNT_OF(ble_scan_gatt_commands)) {
            command = &ble_scan_gatt_commands[index];
            state->last_ble_scan_gatt_index = index;
        }
        break;
    case VIEW_BLE_SCAN_ADV:
        if(index < COUNT_OF(ble_scan_adv_commands)) {
            command = &ble_scan_adv_commands[index];
            state->last_ble_scan_adv_index = index;
        }
        break;
    }

    if(command) {
        execute_menu_command(state, command);
    }
}

void handle_aerial_menu(AppState* state, uint32_t index) {
    if(index < COUNT_OF(aerial_commands)) {
        state->last_aerial_category_index = index;
        execute_menu_command(state, &aerial_commands[index]);
    }
}

void handle_gps_menu(AppState* state, uint32_t index) {
    const MenuCommand* command = NULL;
    switch(state->current_view) {
    case VIEW_GPS_CONFIG:
        if(index < COUNT_OF(gps_config_commands)) {
            command = &gps_config_commands[index];
            state->last_gps_config_index = index;
        }
        break;
    case VIEW_GPS_WARDRIVE:
        if(index < COUNT_OF(gps_wardrive_commands)) {
            command = &gps_wardrive_commands[index];
            state->last_gps_wardrive_index = index;
        }
        break;
    }
    if(command) {
        execute_menu_command(state, command);
    }
}

void handle_ir_menu(AppState* state, uint32_t index) {
    if(index >= COUNT_OF(ir_commands)) return;

    state->last_ir_index = index;

    switch(index) {
    case 0:
        if(ir_query_and_parse_list(state)) {
            ir_show_remotes_menu(state);
        } else {
            ir_show_error(state, "No IR remotes found.");
        }
        break;
    case 1:
        if(ir_query_and_parse_universals(state)) {
            ir_show_universals_menu(state);
        } else {
            ir_show_error(state, "No universal signals found.");
        }
        break;
    default:
        execute_menu_command(state, &ir_commands[index]);
        break;
    }
}

void submenu_callback(void* context, uint32_t index) {
    AppState* state = (AppState*)context;
    state->current_index = index; // Track current selection

    switch(state->current_view) {
    case VIEW_MAIN:
        switch(index) {
        case 0:
            show_wifi_menu(state);
            state->last_wifi_category_index = 0;
            break;
        case 1:
            show_ble_menu(state);
            state->last_ble_category_index = 0;
            break;
        case 2:
            show_gps_menu(state);
            break;
        case 3:
            show_ir_menu(state);
            break;
        case 4:
            view_dispatcher_switch_to_view(state->view_dispatcher, VIEW_SETTINGS_ACTIONS);
            state->current_view = VIEW_SETTINGS_ACTIONS;
            break;
        }
        break;
    case VIEW_WIFI:
        state->last_wifi_category_index = index;
        switch(index) {
        case 0:
            show_wifi_scanning_menu(state);
            break;
        case 1:
            show_wifi_capture_menu(state);
            break;
        case 2:
            show_wifi_attack_menu(state);
            break;
        case 3:
            show_wifi_network_menu(state);
            break;
        case 4:
            show_aerial_menu(state);
            state->last_aerial_category_index = 0;
            break;
        case 5:
            execute_menu_command(state, &wifi_stop_command);
            break;
        }
        break;
    case VIEW_BLE:
        state->last_ble_category_index = index;
        switch(index) {
        case 0:
            show_ble_scanning_menu(state);
            break;
        case 1:
            show_ble_capture_menu(state);
            break;
        case 2:
            show_ble_attack_menu(state);
            break;
        case 3:
            execute_menu_command(state, &ble_stop_command);
            break;
        }
        break;
    case VIEW_GPS:
        state->last_gps_index = index;
        switch(index) {
        case 0:
            show_gps_config_menu(state);
            break;
        case 1:
            show_gps_wardrive_menu(state);
            break;
        }
        break;
    case VIEW_WIFI_NETWORK:
        state->last_wifi_network_index = index;
        switch(index) {
        case 0:
            show_wifi_network_portal_menu(state);
            break;
        case 1:
            show_wifi_network_conn_menu(state);
            break;
        case 2:
            show_wifi_network_scan_menu(state);
            break;
        case 3:
            show_wifi_network_iot_menu(state);
            break;
        case 4:
            show_wifi_network_webui_menu(state);
            break;
        }
        break;
    case VIEW_WIFI_SETTINGS:
        state->last_wifi_settings_index = index;
        switch(index) {
        case 0:
            show_wifi_settings_led_menu(state);
            break;
        case 1:
            show_wifi_settings_sd_menu(state);
            break;
        case 2:
            show_wifi_settings_mgmt_menu(state);
            break;
        case 3:
            show_wifi_settings_device_menu(state);
            break;
        case 4:
            show_wifi_settings_misc_menu(state);
            break;
        }
        break;
    case VIEW_BLE_SCANNING:
        state->last_ble_scanning_index = index;
        switch(index) {
        case 0:
            show_ble_scan_detect_menu(state);
            break;
        case 1:
            show_ble_scan_airtag_menu(state);
            break;
        case 2:
            show_ble_scan_gatt_menu(state);
            break;
        case 3:
            show_ble_scan_adv_menu(state);
            break;
        }
        break;
    case VIEW_BLE_CAPTURE:
    case VIEW_BLE_ATTACK:
        handle_ble_menu(state, index);
        break;
    case VIEW_WIFI_SETTINGS_LED:
    case VIEW_WIFI_SETTINGS_SD:
    case VIEW_WIFI_SETTINGS_MGMT:
    case VIEW_WIFI_SETTINGS_DEVICE:
    case VIEW_WIFI_SETTINGS_MISC:
    case VIEW_WIFI_NETWORK_PORTAL:
    case VIEW_WIFI_NETWORK_CONN:
    case VIEW_WIFI_NETWORK_SCAN:
    case VIEW_WIFI_NETWORK_IOT:
    case VIEW_WIFI_NETWORK_WEBUI:
        handle_wifi_menu(state, index);
        break;
    case VIEW_BLE_SCAN_DETECT:
    case VIEW_BLE_SCAN_AIRTAG:
    case VIEW_BLE_SCAN_GATT:
    case VIEW_BLE_SCAN_ADV:
        handle_ble_menu(state, index);
        break;
    case VIEW_GPS_CONFIG:
    case VIEW_GPS_WARDRIVE:
        handle_gps_menu(state, index);
        break;
    case VIEW_AERIAL:
        handle_aerial_menu(state, index);
        break;
    case VIEW_IR_REMOTES:
        if(index < state->ir_remote_count) {
            state->ir_universal_buttons_mode = false;
            state->ir_file_buttons_mode = false;
            state->ir_current_remote_index = state->ir_remotes[index].index;
            if(ir_query_and_parse_show(state, state->ir_current_remote_index)) {
                ir_show_buttons_menu(state);
            } else {
                ir_show_error(state, "No IR buttons found.");
            }
        }
        break;
    case VIEW_IR_BUTTONS:
        if(index < state->ir_signal_count) {
            if(state->ir_file_buttons_mode) {
                ir_send_button_from_file(state, index);
            } else {
                IrSignalEntry* sig = &state->ir_signals[index];
                char cmd[128];
                if(state->ir_universal_buttons_mode) {
                    snprintf(
                        cmd,
                        sizeof(cmd),
                        "ir universals sendall %s %s\n",
                        state->ir_current_universal_file,
                        sig->name);
                } else {
                    snprintf(
                        cmd,
                        sizeof(cmd),
                        "ir send %lu %lu\n",
                        (unsigned long)state->ir_current_remote_index,
                        (unsigned long)sig->index);
                }
                MenuCommand dyn = {0};
                dyn.command = cmd;
                execute_menu_command(state, &dyn);
            }
        }
        break;
    case VIEW_IR_UNIVERSALS:
        if(index < state->ir_universal_count) {
            IrUniversalEntry* uni = &state->ir_universals[index];
            strncpy(
                state->ir_current_universal_file,
                uni->name,
                sizeof(state->ir_current_universal_file) - 1);
            state->ir_current_universal_file[sizeof(state->ir_current_universal_file) - 1] = '\0';
            state->ir_universal_buttons_mode = true;
            state->ir_file_buttons_mode = false;
            if(ir_query_and_parse_universal_buttons(state, state->ir_current_universal_file)) {
                ir_show_buttons_menu(state);
            } else {
                ir_show_error(state, "No universal buttons found.");
            }
        }
        break;
    case VIEW_IR:
        handle_ir_menu(state, index);
        break;
    }
}

static void show_menu_help(void* context, uint32_t index) {
    UNUSED(index);
    AppState* state = context;

    // Save current view
    state->previous_view = state->current_view;

    // Define help text with essential actions only
    const char* help_text = "=== Controls ===\n"
                            "Hold [Ok]\n"
                            "    Show command details\n"
                            "Back button returns to\n"
                            "previous menu\n"
                            "\n"
                            "=== File Locations ===\n"
                            "/apps_data/ghost_esp/\n"
                            "\n"
                            "\n"
                            "=== Tips ===\n"
                            "- One capture at a time\n"
                            "- Hold OK on any command\n"
                            "  to see range & details\n"
                            "\n"
                            "=== Settings ===\n"
                            "Configure options in\n"
                            "SET menu including:\n"
                            "- Auto-stop behavior\n"
                            "- LED settings\n"
                            "\n"
                            "Join the Discord\n"
                            "for support and\n"
                            "to stay updated!\n";

    // Set header and help text in the confirmation view
    confirmation_view_set_header(state->confirmation_view, "Quick Help");
    confirmation_view_set_text(state->confirmation_view, help_text);

    // Set callbacks for user actions
    confirmation_view_set_ok_callback(state->confirmation_view, app_info_ok_callback, state);
    confirmation_view_set_cancel_callback(state->confirmation_view, app_info_ok_callback, state);

    // Switch to confirmation view to display help
    view_dispatcher_switch_to_view(state->view_dispatcher, VIEW_CONFIRMATION);
    state->current_view = VIEW_CONFIRMATION;
}

bool back_event_callback(void* context) {
    AppState* state = (AppState*)context;
    if(!state) return false;

    uint8_t cv = state->current_view;

    if(cv == VIEW_CONFIRMATION) return false;

    if(cv == VIEW_TEXT_BOX) {
        if(state->settings.stop_on_back_index) {
            send_uart_command(wifi_stop_command.command, state);
        }
        if(state->uart_context) {
            uart_reset_text_buffers(state->uart_context);
            uart_cleanup_capture_streams(state->uart_context);
        }
        if(state->textBoxBuffer) state->buffer_length = 0;
        navigate_to_view(state, state->previous_view);
    } else if(cv == VIEW_TEXT_INPUT) {
        if(state->settings.stop_on_back_index) {
            send_uart_command(wifi_stop_command.command, state);
        }
        state->uart_command = NULL;
        state->connect_input_stage = 0;
        state->connect_ssid[0] = '\0';
        if(state->text_input) text_input_reset(state->text_input);
        if(state->input_buffer) memset(state->input_buffer, 0, INPUT_BUFFER_SIZE);
        navigate_to_view(state, state->previous_view);
    } else if(cv == VIEW_SETTINGS_ACTIONS) {
        navigate_to_view(state, VIEW_MAIN);
    } else if(cv == VIEW_SETTINGS_CONFIG) {
        navigate_to_view(state, VIEW_SETTINGS_ACTIONS);
    } else if(cv >= VIEW_WIFI && cv <= VIEW_GPS) {
        navigate_to_view(state, VIEW_MAIN);
    } else if(cv >= VIEW_IR_REMOTES && cv <= VIEW_IR_UNIVERSALS) {
        if(state->ir_file_buffer) {
            free(state->ir_file_buffer);
            state->ir_file_buffer = NULL;
            state->ir_file_buffer_size = 0;
        }
        state->ir_file_buttons_mode = false;
        state->ir_universal_buttons_mode = false;
        navigate_to_view(state, VIEW_IR);
    } else if(cv == VIEW_IR) {
        navigate_to_view(state, VIEW_MAIN);
    } else if(cv >= VIEW_WIFI_SCANNING && cv <= VIEW_AERIAL) {
        if(state->came_from_settings && cv >= VIEW_WIFI_SCANNING && cv <= VIEW_WIFI_SETTINGS) {
            navigate_to_view(state, VIEW_SETTINGS_ACTIONS);
        } else {
            navigate_to_view(state, VIEW_WIFI);
        }
    } else if(cv >= VIEW_WIFI_SETTINGS_LED && cv <= VIEW_WIFI_SETTINGS_MISC) {
        if(state->came_from_settings) {
            navigate_to_view(state, VIEW_SETTINGS_ACTIONS);
        } else {
            navigate_to_view(state, VIEW_WIFI_SETTINGS);
        }
    } else if(cv >= VIEW_WIFI_NETWORK_PORTAL && cv <= VIEW_WIFI_NETWORK_WEBUI) {
        navigate_to_view(state, VIEW_WIFI_NETWORK);
    } else if(cv == VIEW_STATUS_IDLE) {
        navigate_to_view(state, VIEW_SETTINGS_ACTIONS);
    } else if(cv >= VIEW_BLE_SCANNING && cv <= VIEW_BLE_ATTACK) {
        navigate_to_view(state, VIEW_BLE);
    } else if(cv >= VIEW_BLE_SCAN_DETECT && cv <= VIEW_BLE_SCAN_ADV) {
        navigate_to_view(state, VIEW_BLE_SCANNING);
    } else if(cv >= VIEW_GPS_CONFIG && cv <= VIEW_GPS_WARDRIVE) {
        navigate_to_view(state, VIEW_GPS);
    } else if(cv == VIEW_MAIN) {
        view_dispatcher_stop(state->view_dispatcher);
    }

    return true;
}

void show_main_menu(AppState* state) {
    main_menu_reset(state->main_menu);
    main_menu_set_header(state->main_menu, "");
    main_menu_add_item(state->main_menu, "WiFi", 0, submenu_callback, state);
    main_menu_add_item(state->main_menu, "BLE", 1, submenu_callback, state);
    main_menu_add_item(state->main_menu, "GPS", 2, submenu_callback, state);
    main_menu_add_item(state->main_menu, "  IR", 3, submenu_callback, state);
    main_menu_add_item(state->main_menu, " SET", 4, submenu_callback, state);

    // Set up help callback
    main_menu_set_help_callback(state->main_menu, show_menu_help, state);

    state->came_from_settings = false;
    view_dispatcher_switch_to_view(state->view_dispatcher, VIEW_MAIN);
    state->current_view = VIEW_MAIN;
}

bool text_view_input_handler(InputEvent* event, void* context) {
    AppState* state = (AppState*)context;
    if(!state || !event) return false;

    bool consumed = false;

    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        send_uart_command(wifi_stop_command.command, state);
        consumed = true;
    } else if(event->type == InputTypeShort && event->key == InputKeyRight) {
        state->text_box_user_scrolled = false;
        update_text_box_view(state);
        consumed = true;
    } else {
        if((event->type == InputTypeShort || event->type == InputTypeRepeat) &&
           (event->key == InputKeyUp || event->key == InputKeyDown)) {
            state->text_box_user_scrolled = true;

            if(!state->text_box_pause_hint_shown) {
                state->text_box_pause_hint_shown = true;
                show_result_dialog(
                    state,
                    "Tip",
                    "Scroll paused.\nPress Right arrow to resume.\nPress OK to send stop.");
            }
        }

        if(state->text_box_original_input && state->text_box_original_context) {
            consumed = state->text_box_original_input(event, state->text_box_original_context);
        }
    }

    return consumed;
}

void text_view_attach_input_handler(AppState* state) {
    if(!state || !state->text_box) return;

    View* text_view = text_box_get_view(state->text_box);
    if(!text_view) return;

    state->text_box_original_input = text_view->input_callback;
    state->text_box_original_context = text_view->context;

    view_set_context(text_view, state);
    view_set_input_callback(text_view, text_view_input_handler);

    state->text_box_user_scrolled = false;
}

static bool menu_input_handler(InputEvent* event, void* context) {
    AppState* state = (AppState*)context;
    bool consumed = false;

    if(!state || !event) return false;

    const MenuCommand* commands = NULL;
    size_t commands_count = 0;
    Submenu* current_menu = NULL;

    // Determine current menu context
    switch(state->current_view) {
    case VIEW_WIFI:
        current_menu = state->wifi_menu;
        return false;
    case VIEW_BLE:
        current_menu = state->ble_menu;
        return false;
    case VIEW_BLE_SCANNING:
        current_menu = state->ble_scanning_menu;
        return false;
    case VIEW_BLE_CAPTURE:
        current_menu = state->ble_capture_menu;
        commands = ble_capture_commands;
        commands_count = COUNT_OF(ble_capture_commands);
        break;
    case VIEW_BLE_ATTACK:
        current_menu = state->ble_attack_menu;
        commands = ble_attack_commands;
        commands_count = COUNT_OF(ble_attack_commands);
        break;
    case VIEW_GPS:
        current_menu = state->gps_menu;
        return false;
    case VIEW_IR:
        current_menu = state->ir_menu;
        commands = ir_commands;
        commands_count = COUNT_OF(ir_commands);
        break;
    case VIEW_WIFI_SCANNING:
        current_menu = state->wifi_scanning_menu;
        commands = wifi_scanning_commands;
        commands_count = COUNT_OF(wifi_scanning_commands);
        break;
    case VIEW_WIFI_CAPTURE:
        current_menu = state->wifi_capture_menu;
        commands = wifi_capture_commands;
        commands_count = COUNT_OF(wifi_capture_commands);
        break;
    case VIEW_WIFI_ATTACK:
        current_menu = state->wifi_attack_menu;
        commands = wifi_attack_commands;
        commands_count = COUNT_OF(wifi_attack_commands);
        break;
    case VIEW_WIFI_NETWORK:
        current_menu = state->wifi_network_menu;
        return false;
    case VIEW_WIFI_SETTINGS:
        current_menu = state->wifi_settings_menu;
        return false;
    case VIEW_AERIAL:
        current_menu = state->aerial_menu;
        commands = aerial_commands;
        commands_count = COUNT_OF(aerial_commands);
        break;
    case VIEW_STATUS_IDLE:
        current_menu = state->status_idle_menu;
        commands = status_idle_commands;
        commands_count = COUNT_OF(status_idle_commands);
        break;
    // WiFi Settings sub-categories
    case VIEW_WIFI_SETTINGS_LED:
        current_menu = state->wifi_settings_led_menu;
        commands = wifi_settings_led_commands;
        commands_count = COUNT_OF(wifi_settings_led_commands);
        break;
    case VIEW_WIFI_SETTINGS_SD:
        current_menu = state->wifi_settings_sd_menu;
        commands = wifi_settings_sd_commands;
        commands_count = COUNT_OF(wifi_settings_sd_commands);
        break;
    case VIEW_WIFI_SETTINGS_MGMT:
        current_menu = state->wifi_settings_mgmt_menu;
        commands = wifi_settings_mgmt_commands;
        commands_count = COUNT_OF(wifi_settings_mgmt_commands);
        break;
    case VIEW_WIFI_SETTINGS_DEVICE:
        current_menu = state->wifi_settings_device_menu;
        commands = wifi_settings_device_commands;
        commands_count = COUNT_OF(wifi_settings_device_commands);
        break;
    case VIEW_WIFI_SETTINGS_MISC:
        current_menu = state->wifi_settings_misc_menu;
        commands = wifi_settings_misc_commands;
        commands_count = COUNT_OF(wifi_settings_misc_commands);
        break;
    // WiFi Network sub-categories
    case VIEW_WIFI_NETWORK_PORTAL:
        current_menu = state->wifi_network_portal_menu;
        commands = wifi_network_portal_commands;
        commands_count = COUNT_OF(wifi_network_portal_commands);
        break;
    case VIEW_WIFI_NETWORK_CONN:
        current_menu = state->wifi_network_conn_menu;
        commands = wifi_network_conn_commands;
        commands_count = COUNT_OF(wifi_network_conn_commands);
        break;
    case VIEW_WIFI_NETWORK_SCAN:
        current_menu = state->wifi_network_scan_menu;
        commands = wifi_network_scan_commands;
        commands_count = COUNT_OF(wifi_network_scan_commands);
        break;
    case VIEW_WIFI_NETWORK_IOT:
        current_menu = state->wifi_network_iot_menu;
        commands = wifi_network_iot_commands;
        commands_count = COUNT_OF(wifi_network_iot_commands);
        break;
    case VIEW_WIFI_NETWORK_WEBUI:
        current_menu = state->wifi_network_webui_menu;
        commands = wifi_network_webui_commands;
        commands_count = COUNT_OF(wifi_network_webui_commands);
        break;
    // BLE Scanning sub-categories
    case VIEW_BLE_SCAN_DETECT:
        current_menu = state->ble_scan_detect_menu;
        commands = ble_scan_detect_commands;
        commands_count = COUNT_OF(ble_scan_detect_commands);
        break;
    case VIEW_BLE_SCAN_AIRTAG:
        current_menu = state->ble_scan_airtag_menu;
        commands = ble_scan_airtag_commands;
        commands_count = COUNT_OF(ble_scan_airtag_commands);
        break;
    case VIEW_BLE_SCAN_GATT:
        current_menu = state->ble_scan_gatt_menu;
        commands = ble_scan_gatt_commands;
        commands_count = COUNT_OF(ble_scan_gatt_commands);
        break;
    case VIEW_BLE_SCAN_ADV:
        current_menu = state->ble_scan_adv_menu;
        commands = ble_scan_adv_commands;
        commands_count = COUNT_OF(ble_scan_adv_commands);
        break;
    // GPS sub-categories
    case VIEW_GPS_CONFIG:
        current_menu = state->gps_config_menu;
        commands = gps_config_commands;
        commands_count = COUNT_OF(gps_config_commands);
        break;
    case VIEW_GPS_WARDRIVE:
        current_menu = state->gps_wardrive_menu;
        commands = gps_wardrive_commands;
        commands_count = COUNT_OF(gps_wardrive_commands);
        break;
    default:
        return false;
    }

    if(!current_menu || !commands) return false;

    uint32_t current_index = submenu_get_selected_item(current_menu);

    switch(event->type) {
    case InputTypeShort:
        switch(event->key) {
        case InputKeyUp:
            if(current_index > 0) {
                submenu_set_selected_item(current_menu, current_index - 1);
            } else {
                // Wrap to bottom
                submenu_set_selected_item(current_menu, commands_count - 1);
            }
            consumed = true;
            break;

        case InputKeyDown:
            if(current_index < commands_count - 1) {
                submenu_set_selected_item(current_menu, current_index + 1);
            } else {
                // Wrap to top
                submenu_set_selected_item(current_menu, 0);
            }
            consumed = true;
            break;

        case InputKeyOk:
            if(current_index < commands_count) {
                if(state->current_view == VIEW_IR) {
                    submenu_callback(state, current_index);
                } else {
                    state->current_index = current_index;
                    // Save last selection for proper restore on exit
                    if(state->current_view >= VIEW_WIFI_SCANNING &&
                       state->current_view <= VIEW_WIFI_SETTINGS) {
                        switch(state->current_view) {
                        case VIEW_WIFI_SCANNING:
                            state->last_wifi_scanning_index = current_index;
                            break;
                        case VIEW_WIFI_CAPTURE:
                            state->last_wifi_capture_index = current_index;
                            break;
                        case VIEW_WIFI_ATTACK:
                            state->last_wifi_attack_index = current_index;
                            break;
                        case VIEW_WIFI_NETWORK:
                            state->last_wifi_network_index = current_index;
                            break;
                        case VIEW_WIFI_SETTINGS:
                            state->last_wifi_settings_index = current_index;
                            break;
                        }
                    } else if(
                        state->current_view >= VIEW_BLE_SCANNING &&
                        state->current_view <= VIEW_BLE_ATTACK) {
                        switch(state->current_view) {
                        case VIEW_BLE_SCANNING:
                            state->last_ble_scanning_index = current_index;
                            break;
                        case VIEW_BLE_CAPTURE:
                            state->last_ble_capture_index = current_index;
                            break;
                        case VIEW_BLE_ATTACK:
                            state->last_ble_attack_index = current_index;
                            break;
                        }
                    } else if(state->current_view == VIEW_GPS) {
                        state->last_gps_index = current_index;
                    } else if(state->current_view == VIEW_AERIAL) {
                        state->last_aerial_category_index = current_index;
                    }
                    execute_menu_command(state, &commands[current_index]);
                }
                consumed = true;
            }
            break;

        case InputKeyBack:
            if(state->current_view == VIEW_STATUS_IDLE) {
                navigate_to_view(state, VIEW_SETTINGS_ACTIONS);
            } else if(state->current_view >= VIEW_WIFI_SCANNING && state->current_view <= VIEW_AERIAL) {
                if(state->came_from_settings && state->current_view <= VIEW_WIFI_SETTINGS) {
                    navigate_to_view(state, VIEW_SETTINGS_ACTIONS);
                } else {
                    navigate_to_view(state, VIEW_WIFI);
                }
            } else if(
                state->current_view >= VIEW_WIFI_SETTINGS_LED &&
                state->current_view <= VIEW_WIFI_SETTINGS_MISC) {
                if(state->came_from_settings) {
                    navigate_to_view(state, VIEW_SETTINGS_ACTIONS);
                } else {
                    navigate_to_view(state, VIEW_WIFI_SETTINGS);
                }
            } else if(
                state->current_view >= VIEW_WIFI_NETWORK_PORTAL &&
                state->current_view <= VIEW_WIFI_NETWORK_WEBUI) {
                navigate_to_view(state, VIEW_WIFI_NETWORK);
            } else if(
                state->current_view >= VIEW_BLE_SCANNING &&
                state->current_view <= VIEW_BLE_ATTACK) {
                navigate_to_view(state, VIEW_BLE);
            } else if(
                state->current_view >= VIEW_BLE_SCAN_DETECT &&
                state->current_view <= VIEW_BLE_SCAN_ADV) {
                navigate_to_view(state, VIEW_BLE_SCANNING);
            } else if(
                state->current_view >= VIEW_GPS_CONFIG &&
                state->current_view <= VIEW_GPS_WARDRIVE) {
                navigate_to_view(state, VIEW_GPS);
            } else if(
                (state->current_view >= VIEW_WIFI && state->current_view <= VIEW_GPS) ||
                state->current_view == VIEW_IR) {
                navigate_to_view(state, VIEW_MAIN);
            }
            consumed = true;
            break;

        case InputKeyRight:
        case InputKeyLeft:
            // Handle sniff command cycling
            if(state->current_view == VIEW_WIFI_CAPTURE && current_index == 0) {
                // sniff_commands is not CyclingMenuDef, so keep legacy logic for now
                if(event->key == InputKeyRight) {
                    current_sniff_index = (current_sniff_index + 1) % COUNT_OF(sniff_commands);
                } else {
                    current_sniff_index = (current_sniff_index == 0) ?
                                              (size_t)(COUNT_OF(sniff_commands) - 1) :
                                              (current_sniff_index - 1);
                }
                submenu_change_item_label(
                    current_menu, current_index, sniff_commands[current_sniff_index].label);
                consumed = true;
            }
            // Handle beacon spam command cycling
            else if(state->current_view == VIEW_WIFI_ATTACK && current_index == 0) {
                consumed = cycle_menu_item(
                    (CyclingMenuDef*)beacon_spam_commands,
                    COUNT_OF(beacon_spam_commands),
                    &current_beacon_index,
                    (MenuCommand*)wifi_attack_commands,
                    0,
                    current_menu,
                    event);
            }
            // Handle rgbmode command cycling (LED sub-category, index 0)
            else if(state->current_view == VIEW_WIFI_SETTINGS_LED && current_index == 0) {
                consumed = cycle_menu_item(
                    (CyclingMenuDef*)rgbmode_commands,
                    COUNT_OF(rgbmode_commands),
                    &current_rgb_index,
                    (MenuCommand*)wifi_settings_led_commands,
                    0,
                    current_menu,
                    event);
            }
            // Handle BLE spam command cycling
            else if(state->current_view == VIEW_BLE_ATTACK && current_index == 0) {
                consumed = cycle_menu_item(
                    (CyclingMenuDef*)ble_spam_commands,
                    COUNT_OF(ble_spam_commands),
                    &current_ble_spam_index,
                    (MenuCommand*)ble_attack_commands,
                    0,
                    current_menu,
                    event);
            }
            // Handle WiFi scan mode cycling
            else if(state->current_view == VIEW_WIFI_SCANNING && current_index == 0) {
                consumed = cycle_menu_item(
                    (CyclingMenuDef*)wifi_scan_modes,
                    COUNT_OF(wifi_scan_modes),
                    &current_wifi_scan_index,
                    (MenuCommand*)wifi_scanning_commands,
                    0,
                    current_menu,
                    event);
            }
            // List mode cycling
            else if(state->current_view == VIEW_WIFI_SCANNING && current_index == 1) {
                consumed = cycle_menu_item(
                    (CyclingMenuDef*)wifi_list_modes,
                    COUNT_OF(wifi_list_modes),
                    &current_wifi_list_index,
                    (MenuCommand*)wifi_scanning_commands,
                    1,
                    current_menu,
                    event);
            }
            // Select mode cycling
            else if(state->current_view == VIEW_WIFI_SCANNING && current_index == 2) {
                consumed = cycle_menu_item(
                    (CyclingMenuDef*)wifi_select_modes,
                    COUNT_OF(wifi_select_modes),
                    &current_wifi_select_index,
                    (MenuCommand*)wifi_scanning_commands,
                    2,
                    current_menu,
                    event);
            }
            // Handle listen mode cycling
            else if(state->current_view == VIEW_WIFI_SCANNING && current_index == 3) {
                consumed = cycle_menu_item(
                    (CyclingMenuDef*)wifi_listen_modes,
                    COUNT_OF(wifi_listen_modes),
                    &current_wifi_listen_index,
                    (MenuCommand*)wifi_scanning_commands,
                    3,
                    current_menu,
                    event);
            }
            break;
        case InputKeyMAX:
            break;
        }
        break;

    case InputTypeLong:
        switch(event->key) {
        case InputKeyUp:
        case InputKeyDown:
        case InputKeyRight:
        case InputKeyLeft:
        case InputKeyBack:
        case InputKeyMAX:
            break;

        case InputKeyOk:
            if(current_index < commands_count) {
                const MenuCommand* command = &commands[current_index];
                if(command->details_header && command->details_text) {
                    show_command_details(state, command);
                    consumed = true;
                }
            }
            break;
        }
        break;

    case InputTypeRepeat:
        switch(event->key) {
        case InputKeyUp:
            if(current_index > 0) {
                submenu_set_selected_item(current_menu, current_index - 1);
            } else {
                // Wrap to bottom
                submenu_set_selected_item(current_menu, commands_count - 1);
            }
            consumed = true;
            break;

        case InputKeyDown:
            if(current_index < commands_count - 1) {
                submenu_set_selected_item(current_menu, current_index + 1);
            } else {
                // Wrap to top
                submenu_set_selected_item(current_menu, 0);
            }
            consumed = true;
            break;

        case InputKeyRight:
        case InputKeyLeft:
        case InputKeyOk:
        case InputKeyBack:
        case InputKeyMAX:
            break;
        }
        break;

    case InputTypePress:
    case InputTypeRelease:
    case InputTypeMAX:
        break;
    }

    return consumed;
}

// 6675636B796F7564656B69
