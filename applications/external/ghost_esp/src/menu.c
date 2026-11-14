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
    const char* prefix; // Prefix for capture files
    const char* ext; // File extension for captures
    const char* folder; // Folder for captures
} CaptureFileDef;

typedef struct {
    const char* header; // Confirmation dialog header
    const char* text; // Confirmation dialog text
} ConfirmDef;

typedef struct {
    const char* label; // Display label in menu
    const char* command; // UART command to send
    const CaptureFileDef* capture; // Capture file info (NULL if none)
    bool needs_input; // Whether command requires text input
    const char* input_text; // Text to show in input box (NULL if none)
    const ConfirmDef* confirm; // Confirmation dialog (NULL if none)
    const char* details_header; // Header for details view
    const char* details_text; // Detailed description/info text
} MenuCommand;

// Shared capture file definitions
static const CaptureFileDef capture_wps_def = {
    .prefix = "wps_capture",
    .ext = "pcap",
    .folder = GHOST_ESP_APP_FOLDER_PCAPS,
};
static const CaptureFileDef capture_handshake_def = {
    .prefix = "handshake_deauth",
    .ext = "pcap",
    .folder = GHOST_ESP_APP_FOLDER_PCAPS,
};
static const CaptureFileDef capture_zigbee_def = {
    .prefix = "zigbee_capture",
    .ext = "pcap",
    .folder = GHOST_ESP_APP_FOLDER_PCAPS,
};
static const CaptureFileDef capture_skimmer_def = {
    .prefix = "skimmer_scan",
    .ext = "pcap",
    .folder = GHOST_ESP_APP_FOLDER_PCAPS,
};
static const CaptureFileDef capture_ble_def = {
    .prefix = "ble_raw_capture",
    .ext = "pcap",
    .folder = GHOST_ESP_APP_FOLDER_PCAPS,
};
static const CaptureFileDef wardrive_wifi_def = {
    .prefix = "wardrive_wifi",
    .ext = "csv",
    .folder = GHOST_ESP_APP_FOLDER_WARDRIVE,
};
static const CaptureFileDef wardrive_ble_def = {
    .prefix = "wardrive_ble",
    .ext = "csv",
    .folder = GHOST_ESP_APP_FOLDER_WARDRIVE,
};

// Shared confirmation dialog definitions
static const ConfirmDef confirm_forget_wifi = {
    .header = "Forget Saved WiFi",
    .text = "Disconnect and forget the\nsaved WiFi network on the\nESP?",
};
static const ConfirmDef confirm_scanlocal = {
    .header = "Local Network Scan",
    .text = "Make sure you've connected\nto WiFi first via "
            "the\n'Connect to WiFi' option.\n",
};
static const ConfirmDef confirm_cast_video = {
    .header = "Cast Video",
    .text = "Make sure you've connected\nto WiFi first via "
            "the\n'Connect to WiFi' option.\n",
};
static const ConfirmDef confirm_printer_power = {
    .header = "Printer Power",
    .text = "You need to configure\n settings in the WebUI\n for this command.\n",
};
static const ConfirmDef confirm_reset_apcred = {
    .header = "Reset AP Credentials",
    .text = "Reset WebUI AP to\ndefault credentials?\n"
            "SSID: GhostNet\nPassword: GhostNet\n",
};
static const ConfirmDef confirm_sd_save = {
    .header = "Save SD Config",
    .text = "Save current SD pin\nconfig to SD card?\nRequires SD mounted.",
};
static const ConfirmDef confirm_reboot = {
    .header = "Reboot Device",
    .text = "Are you sure you want to reboot?",
};
static const ConfirmDef confirm_coredump_erase = {
    .header = "Erase Coredump",
    .text = "Permanently erase the stored\ncrash coredump?\nThis cannot be undone.",
};
static const ConfirmDef confirm_force_crash = {
    .header = "Force Crash",
    .text = "This intentionally crashes\nthe ESP to test coredump.\nContinue?",
};
static const ConfirmDef confirm_ir_dazzler = {
    .header = "IR Dazzler Warning",
    .text = "The Rabbit-Labs Poltergeist\n"
            "can draw too much current,\n"
            "use at your own risk.",
};

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
static bool menu_view_input_handler(InputEvent* event, void* context);
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
        .label = "Stop Scan",
        .command = "stopscan\n",
        .details_header = "Stop Scan",
        .details_text = "Stops AP or Station scan.",
    },
    {
        .label = "Stop Listen Probes",
        .command = "listenprobes stop\n",
        .details_header = "Stop Listening",
        .details_text = "Stops the probe listener.",
    },
    {
        .label = "Full Environment Sweep",
        .command = "sweep\n",
        .details_header = "Environment Sweep",
        .details_text = "Sweep WiFi APs, stations and\n"
                        "BLE; saves a CSV report to\n"
                        "the ESP SD card.\n",
    },
    {
        .label = "Combined AP+STA Scan",
        .command = "scanall",
        .needs_input = true,
        .input_text = "Seconds",
        .details_header = "Scan All",
        .details_text = "Combined AP and station scan\n"
                        "with summary report.\n"
                        "Optional duration (seconds).\n",
    },
    {
        .label = "Track Selected AP",
        .command = "trackap\n",
        .details_header = "Track AP Signal",
        .details_text = "Track selected AP signal\n"
                        "(RSSI) live.\n"
                        "Select an AP first.\n",
    },
    {
        .label = "Track Selected Station",
        .command = "tracksta\n",
        .details_header = "Track Station Signal",
        .details_text = "Track selected station RSSI\n"
                        "live.\n"
                        "Select a station first.\n",
    },
    {
        .label = "Channel Congestion",
        .command = "congestion\n",
        .details_header = "Channel Congestion",
        .details_text = "Show Wi-Fi channel\n"
                        "congestion chart.\n",
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
        .details_text = "Detects Flock Safety cameras\n"
                        "and related surveillance\n"
                        "devices on 2.4 GHz Wi-Fi via\n"
                        "OUI, probe and SSID matching.\n",
    },
    {
        .label = "Flock List",
        .command = "flocklist\n",
        .details_header = "Flock List",
        .details_text = "List detected Flock devices.\n"
                        "Run after Flock Detection.\n",
    },
    {
        .label = "Stop Flock Detection",
        .command = "flockstop\n",
        .details_header = "Stop Flock Detection",
        .details_text = "Stops Flock camera detection.\n",
    },
    {
        .label = "ARP Scan",
        .command = "scanarp\n",
        .details_header = "ARP Scan",
        .details_text = "ARP-scan the local network to\n"
                        "find active hosts.\n"
                        "Requires WiFi connection.\n",
    },
    {
        .label = "Scan Ports",
        .command = "scanports",
        .needs_input = true,
        .input_text = "local or IP [options]",
        .details_header = "Port Scanner",
        .details_text = "Scan ports on local net or\n"
                        "an IP: -C, -A, or a range.\n"
                        "Ex: 192.168.1.1 80-1000\n",
    },
    {
        .label = "Scan SSH",
        .command = "scanssh",
        .needs_input = true,
        .input_text = "IP",
        .details_header = "SSH Scan",
        .details_text = "Scan one IP for SSH services\n"
                        "and grab banners.\n"
                        "Ex: 192.168.1.10\n",
    },
    {
        .label = "NetBIOS Scan",
        .command = "netbiosscan subnet",
        .needs_input = true,
        .input_text = "Subnet (e.g. 192.168.1.)",
        .details_header = "NetBIOS Scan",
        .details_text = "Scan a subnet for NetBIOS\n"
                        "names/hosts.\n"
                        "Enter prefix, e.g. 192.168.1.\n",
    },
    {
        .label = "HTTP Banner Scan",
        .command = "httpbannerscan subnet",
        .needs_input = true,
        .input_text = "Subnet (e.g. 192.168.1.)",
        .details_header = "HTTP Banner Scan",
        .details_text = "Grab HTTP banners from hosts\n"
                        "on a subnet.\n"
                        "Enter prefix, e.g. 192.168.1.\n",
    },
    {
        .label = "SNMP Probe",
        .command = "snmpprobe subnet",
        .needs_input = true,
        .input_text = "Subnet (e.g. 192.168.1.)",
        .details_header = "SNMP Probe",
        .details_text = "Probe a subnet for SNMP\n"
                        "devices.\n"
                        "Enter prefix, e.g. 192.168.1.\n",
    },
    {
        .label = "WPA3 Compliance Check",
        .command = "wpa3check\n",
        .details_header = "WPA3 Compliance",
        .details_text = "Check selected AP for WPA3\n"
                        "and SAE support.\n"
                        "Select an AP first.\n",
    },
};

// WiFi Recon Tools menu command definitions
static const MenuCommand wifi_recon_commands[] = {
    {
        .label = "Enum Scan (Local Network)",
        .command = "enumscan\n",
        .details_header = "Enum Scan",
        .details_text = "Enumerate local hosts\n"
                        "(ARP + name discovery).\n"
                        "Requires WiFi or Ethernet.\n",
    },
    {
        .label = "Enum Scan (Target)...",
        .command = "enumscan",
        .needs_input = true,
        .input_text = "IP or 'subnet a.b.c.'",
        .details_header = "Enum Scan Target",
        .details_text = "Enumerate one host or subnet.\n"
                        "Ex: 192.168.1.10\n"
                        "Ex: subnet 192.168.1.\n",
    },
    {
        .label = "Name Sniff (All Hosts)",
        .command = "mdnssniff all\n",
        .details_header = "Name Sniff",
        .details_text = "Passively sniff mDNS, LLMNR,\n"
                        "SSDP and NetBIOS names from\n"
                        "all hosts.\n",
    },
    {
        .label = "Name Sniff (Target)...",
        .command = "mdnssniff",
        .needs_input = true,
        .input_text = "Target IP",
        .details_header = "Name Sniff Target",
        .details_text = "Sniff local names advertised\n"
                        "by one host.\n",
    },
    {
        .label = "Stop Name Sniff",
        .command = "mdnssniff stop\n",
        .details_header = "Stop Name Sniff",
        .details_text = "Stop the passive name\nsniffer.\n",
    },
    {
        .label = "Hop Plan: Auto",
        .command = "hop auto\n",
        .details_header = "Hop Plan: Auto",
        .details_text = "Default channel plan\n"
                        "(country / AP list).\n",
    },
    {
        .label = "Hop Plan: Show Current",
        .command = "hop\n",
        .details_header = "Channel Hop Plan",
        .details_text = "Show the channel hopping plan\n"
                        "used by scans and monitors.\n",
    },
    {
        .label = "Hop Plan: Basic (1/6/11)",
        .command = "hop basic\n",
        .details_header = "Hop Plan: Basic",
        .details_text = "Fixed 1, 6, 11 channel plan.\n",
    },
    {
        .label = "Hop Plan: All Channels",
        .command = "hop all\n",
        .details_header = "Hop Plan: All",
        .details_text = "Hop every legal channel for\nthis device.\n",
    },
    {
        .label = "Hop Plan: Custom...",
        .command = "hop custom",
        .needs_input = true,
        .input_text = "1,6,11",
        .details_header = "Hop Plan: Custom",
        .details_text = "Comma-separated channel list\n"
                        "allowed by your country.\n"
                        "Ex: 1,6,11\n",
    },
};

static const MenuCommand wifi_capture_commands[] = {
    {
        .label = "< Sniff WPS >",
        .command = "capture -wps\n",
        .capture = &capture_wps_def,
        .details_header = "Variable Sniff",
        .details_text = "Use Left/Right:\n"
                        "WPS, Raw, Probe, Deauth,\n"
                        "Beacon, EAPOL, Pwn.\n",
    },
    {
        .label = "Sniff 802.15.4 (C5/C6)",
        .command = "capture -802154\n",
        .capture = &capture_zigbee_def,
        .details_header = "802.15.4 Sniff",
        .details_text = "Capture Zigbee/Thread traffic\n"
                        "(C5/C6 only). Hops 11-26.\n",
    },
    {
        .label = "List Saved PCAPs",
        .command = "capture -list\n",
        .details_header = "List PCAPs",
        .details_text = "List PCAPs on the ESP SD\n"
                        "card. [+] = has PMKID or\n"
                        "handshake material.\n",
    },
    {
        .label = "Export PCAP to hc22000...",
        .command = "capture -export",
        .needs_input = true,
        .input_text = "PCAP filename",
        .details_header = "Export hc22000",
        .details_text = "Convert an ESP SD PCAP to\n"
                        "hashcat hc22000 (PMKID +\n"
                        "M2/M3). Run List Saved\n"
                        "PCAPs first for the name.\n",
    },
};

static const MenuCommand wifi_attack_commands[] = {
    {
        .label = "< Beacon Spam (List) >",
        .command = "beaconspam -l\n",
        .needs_input = false,
        .input_text = "SSID Name",
        .details_header = "Variable Beacon Spam",
        .details_text = "Use Left/Right:\n"
                        "List, Random, Rickroll,\n"
                        "Custom SSID.\n"
                        "Range: ~50-100m\n",
    },
    {
        .label = "Deauth",
        .command = "attack -d\n",
        .details_header = "Deauth Attack",
        .details_text = "Send deauth frames to\n"
                        "disconnect clients.\n"
                        "Range: ~50-100m\n",
    },
    {
        .label = "Handshake + Deauth",
        .command = "attack -hsd\n",
        .capture = &capture_handshake_def,
        .details_header = "Handshake + Deauth",
        .details_text = "Deauth bursts + handshake\n"
                        "capture. Saves to ESP SD,\n"
                        "or Flipper SD via UART\n"
                        "when ESP SD is unavailable.\n"
                        "Select an AP/STA first.\n"
                        "Range: ~50-100m\n",
    },
    {
        .label = "Stop All Attacks",
        .command = "stop\n",
        .details_header = "Stop Attacks",
        .details_text = "Stops all running operations\n"
                        "on the ESP.\n",
    },
    {
        .label = "Stop Beacon Spam",
        .command = "stopspam\n",
        .details_header = "Stop Beacon Spam",
        .details_text = "Stops any active beacon spam.",
    },
    {
        .label = "EAPOL Logoff",
        .command = "attack -e\n",
        .details_header = "EAPOL Logoff Attack",
        .details_text = "Sends EAPOL logoff frames to\n"
                        "disconnect clients.",
    },
    {
        .label = "Karma Start",
        .command = "karma start\n",
        .details_header = "Karma Rogue AP",
        .details_text = "Replies to probe requests\n"
                        "with saved SSIDs.",
    },
    {
        .label = "Karma Stop",
        .command = "karma stop\n",
        .details_header = "Stop Karma Rogue AP",
        .details_text = "Stops the active Karma\n"
                        "rogue AP responder.",
    },
    {
        .label = "Karma Start (Custom)",
        .command = "karma start",
        .needs_input = true,
        .input_text = "SSID [SSID...]",
        .details_header = "Karma Rogue AP (Custom)",
        .details_text = "Reply to probes using your\n"
                        "SSIDs or saved entries.\n",
    },
    {
        .label = "Start Beacon List Spam",
        .command = "beaconspamlist\n",
        .details_header = "Beacon List Spam",
        .details_text = "Beacon-spam the custom\n"
                        "SSID list.\n",
    },
    {
        .label = "Show Beacon List",
        .command = "beaconshow\n",
        .details_header = "Show Beacon List",
        .details_text = "Show the beacon spam list.\n",
    },
    {
        .label = "Add SSID to Beacon List",
        .command = "beaconadd",
        .needs_input = true,
        .input_text = "SSID",
        .details_header = "Add to Beacon List",
        .details_text = "Add an SSID to the beacon\n"
                        "spam list.\n",
    },
    {
        .label = "Remove SSID from Beacon List",
        .command = "beaconremove",
        .needs_input = true,
        .input_text = "SSID",
        .details_header = "Remove from Beacon List",
        .details_text = "Remove an SSID from the\n"
                        "beacon list.\n",
    },
    {
        .label = "Clear Beacon List",
        .command = "beaconclear\n",
        .details_header = "Clear Beacon List",
        .details_text = "Clear the beacon spam list.\n",
    },
    {
        .label = "Channel Switch Attack",
        .command = "attack -c\n",
        .details_header = "Channel Switch Attack",
        .details_text = "Move clients to another\n"
                        "channel with CSA frames.\n"
                        "Select AP(s) first.\n",
    },
    {
        .label = "Probe Request Flood",
        .command = "attack -p\n",
        .details_header = "Probe Flood",
        .details_text = "Flood probe requests to\n"
                        "create phantom clients.\n"
                        "Select AP(s) first.\n",
    },
    {
        .label = "Bad Msg Attack",
        .command = "attack -b\n",
        .details_header = "Bad Msg Attack",
        .details_text = "EAPOL key install + zero MIC\n"
                        "frames.\n"
                        "Select AP(s) first.\n",
    },
    {
        .label = "Auth Flood",
        .command = "attack -a\n",
        .details_header = "Auth Flood",
        .details_text = "Flood auth frames to exhaust\n"
                        "the AP.\n"
                        "Select AP(s) first.\n",
    },
    {
        .label = "GTK Abuse Test",
        .command = "attack -g",
        .needs_input = true,
        .input_text = "SSID Password",
        .details_header = "GTK Abuse Test",
        .details_text = "Test client isolation bypass\n"
                        "via GTK injection.\n"
                        "Enter SSID + password,\n"
                        "space-separated.\n",
    },
    {
        .label = "SAE Handshake Flood",
        .command = "saeflood",
        .needs_input = true,
        .input_text = "SAE PSK",
        .details_header = "SAE Flood Attack",
        .details_text = "Flood a WPA3 AP with SAE\n"
                        "handshakes using the PSK.\n"
                        "Select a WPA3 AP first.\n",
    },
    {
        .label = "Stop SAE Flood",
        .command = "stopsaeflood\n",
        .details_header = "Stop SAE Flood",
        .details_text = "Stops SAE flood and password\n"
                        "spray attacks.\n",
    },
    {
        .label = "SAE Flood Help",
        .command = "saefloodhelp\n",
        .details_header = "SAE Flood Help",
        .details_text = "Shows usage guidance for\n"
                        "SAE flood operations.",
    },
    {
        .label = "DHCP Starve Start",
        .command = "dhcpstarve",
        .needs_input = true,
        .input_text = "start [threads]",
        .details_header = "DHCP Starve Attack",
        .details_text = "Exhaust the DHCP server's IP\n"
                        "pool.\n"
                        "Input: 'start' or 'start 5'.\n",
    },
    {
        .label = "DHCP Starve Stop",
        .command = "dhcpstarve stop\n",
        .details_header = "Stop DHCP Starve",
        .details_text = "Stops the DHCP starvation attack.",
    },
};

// WiFi Network sub-category: Evil Portal
static const MenuCommand wifi_network_portal_commands[] = {
    {
        .label = "Evil Portal",
        .command = "startportal",
        .needs_input = true,
        .input_text = "<path|default> <SSID> <PSK (leave blank for open)>",
        .details_header = "Evil Portal",
        .details_text = "Captive portal for credential\n"
                        "harvest. Configure in WebUI.\n\n"
                        "Use 'default' to serve the HTML\n"
                        "set from the Flipper, or a file\n"
                        "on the ESP SD card.\n",
    },
    {
        .label = "List Portals",
        .command = "listportals\n",
        .details_header = "List Portals",
        .details_text = "List portal HTML files on SD.\n",
    },
    {
        .label = "Set Evil Portal HTML",
        .command = "set_evil_portal_html",
        .needs_input = true,
        .input_text = "HTML File",
        .details_header = "Set Evil Portal HTML",
        .details_text = "Send an HTML file to the ESP\n"
                        "for the evil portal.\n",
    },
    {
        .label = "Start Flipper Portal",
        .command = "startportal default",
        .needs_input = true,
        .input_text = "SSID [PSK]",
        .details_header = "Evil Portal",
        .details_text = "Starts the portal using the\n"
                        "HTML sent with Set Evil\n"
                        "Portal HTML. Enter the SSID\n"
                        "then an optional password.\n",
    },
    {
        .label = "Clear Evil Portal HTML",
        .command = "evilportal -c clear\n",
        .details_header = "Clear Evil Portal",
        .details_text = "Restore the default portal\n"
                        "landing page.\n",
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
        .details_text = "Connect ESP to WiFi.\n"
                        "Enter SSID then password.\n",
    },
    {
        .label = "Connect to Saved WiFi",
        .command = "connect\n",
        .details_header = "Connect (Saved)",
        .details_text = "Connect using saved ESP WiFi\n"
                        "credentials.\n",
    },
    {
        .label = "WiFi Status",
        .command = "wifistatus\n",
        .details_header = "WiFi Status",
        .details_text = "Show WiFi status (SSID, IP,\n"
                        "RSSI).\n",
    },
    {
        .label = "Disconnect WiFi",
        .command = "disconnect\n",
        .details_header = "Disconnect",
        .details_text = "Disconnect ESP from WiFi.\n",
    },
    {
        .label = "Auto-Reconnect",
        .command = "autoreconnect",
        .needs_input = true,
        .input_text = "on | off",
        .details_header = "Auto-Reconnect",
        .details_text = "Auto-reconnect after WiFi\n"
                        "drops.\n",
    },
    {
        .label = "Forget Saved WiFi",
        .command = "forgetwifi\n",
        .confirm = &confirm_forget_wifi,
        .details_header = "Forget WiFi",
        .details_text = "Disconnect and delete the\nsaved WiFi credentials.\n",
    },
};

// WiFi Network sub-category: Network Scan
static const MenuCommand wifi_network_scan_commands[] = {
    {
        .label = "Scan Local Network",
        .command = "scanlocal\n",
        .confirm = &confirm_scanlocal,
        .details_header = "Network Scanner",
        .details_text = "Scan local network for\n"
                        "printers, smart/cast devices.\n"
                        "Requires WiFi connection.\n",
    },
    {
        .label = "Scan Ports",
        .command = "scanports",
        .needs_input = true,
        .input_text = "local or IP [options]",
        .details_header = "Port Scanner",
        .details_text = "Scan ports on local net or\n"
                        "an IP: -C, -A, or a range.\n"
                        "Ex: 192.168.1.1 80-1000\n",
    },
    {
        .label = "ARP Scan",
        .command = "scanarp\n",
        .details_header = "ARP Scan",
        .details_text = "ARP-scan the local network to\n"
                        "find active hosts.\n"
                        "Requires WiFi connection.\n",
    },
    {
        .label = "Scan SSH",
        .command = "scanssh",
        .needs_input = true,
        .input_text = "IP",
        .details_header = "SSH Scan",
        .details_text = "Scan one IP for SSH services\n"
                        "and grab banners.\n"
                        "Ex: 192.168.1.10\n",
    },
    {
        .label = "NetBIOS Scan",
        .command = "netbiosscan subnet",
        .needs_input = true,
        .input_text = "Subnet (e.g. 192.168.1.)",
        .details_header = "NetBIOS Scan",
        .details_text = "Scan a subnet for NetBIOS\n"
                        "names/hosts.\n"
                        "Enter prefix, e.g. 192.168.1.\n",
    },
    {
        .label = "HTTP Banner Scan",
        .command = "httpbannerscan subnet",
        .needs_input = true,
        .input_text = "Subnet (e.g. 192.168.1.)",
        .details_header = "HTTP Banner Scan",
        .details_text = "Grab HTTP banners from hosts\n"
                        "on a subnet.\n"
                        "Enter prefix, e.g. 192.168.1.\n",
    },
    {
        .label = "SNMP Probe",
        .command = "snmpprobe subnet",
        .needs_input = true,
        .input_text = "Subnet (e.g. 192.168.1.)",
        .details_header = "SNMP Probe",
        .details_text = "Probe a subnet for SNMP\n"
                        "devices.\n"
                        "Enter prefix, e.g. 192.168.1.\n",
    },
};

// WiFi Network sub-category: IoT Control
static const MenuCommand wifi_network_iot_commands[] = {
    {
        .label = "Cast Random Video",
        .command = "dialconnect\n",
        .confirm = &confirm_cast_video,
        .details_header = "Video Cast",
        .details_text = "Cast random videos to Cast/\n"
                        "DIAL devices.\n"
                        "Range: ~50m\n",
    },
    {
        .label = "Printer Power",
        .command = "powerprinter\n",
        .confirm = &confirm_printer_power,
        .details_header = "WiFi Printer",
        .details_text = "Control network printer power.\n"
                        "Configure IP/port in WebUI.\n",
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
    {
        .label = "Govee Scan",
        .command = "govee scan\n",
        .details_header = "Govee Scan",
        .details_text = "Find Govee lights on the LAN.\n"
                        "Requires WiFi connection.\n",
    },
    {
        .label = "Govee Control...",
        .command = "govee",
        .needs_input = true,
        .input_text = "<IP> <command>",
        .details_header = "Govee Control",
        .details_text = "Control a Govee light.\n"
                        "Ex: IP on | off | status |\n"
                        "brightness N | color RRGGBB\n",
    },
    {
        .label = "Wake-on-LAN...",
        .command = "wol",
        .needs_input = true,
        .input_text = "MAC or IP",
        .details_header = "Wake-on-LAN",
        .details_text = "Send a Wake-on-LAN packet.\n"
                        "Ex MAC: AA:BB:CC:DD:EE:FF\n"
                        "Needs same WiFi network.\n",
    },
};

// WiFi Network sub-category: WebUI
static const MenuCommand wifi_network_webui_commands[] = {
    {
        .label = "WebUI AP",
        .command = "webuiap",
        .needs_input = true,
        .input_text = "on | off | toggle | status",
        .details_header = "WebUI Access Point",
        .details_text = "Control the WebUI access\n"
                        "point: on/off/toggle/status.\n",
    },
    {
        .label = "Set WebUI Creds",
        .command = "apcred",
        .needs_input = true,
        .input_text = "MySSID MyPassword",
        .details_header = "Set AP Credentials",
        .details_text = "Set custom WebUI AP:\n"
                        "MySSID MyPassword\n"
                        "Ex: GhostNet,spooky123\n",
    },
    {
        .label = "Reset WebUI Creds",
        .command = "apcred -r\n",
        .confirm = &confirm_reset_apcred,
        .details_header = "Reset AP Credentials",
        .details_text = "Restore default WebUI AP\n"
                        "(GhostNet). Requires reboot.\n",
    },
};

// WiFi Network sub-category: DNS Sinkhole
static const MenuCommand wifi_network_sinkhole_commands[] = {
    {
        .label = "Start Sinkhole",
        .command = "sinkhole start\n",
        .details_header = "Start DNS Sinkhole",
        .details_text = "Start the DNS sinkhole.\n"
                        "Connect to WiFi/Ethernet first.\n",
    },
    {
        .label = "Stop Sinkhole",
        .command = "sinkhole stop\n",
        .details_header = "Stop Sinkhole",
        .details_text = "Stop the sinkhole and\nrestore the AP.\n",
    },
    {
        .label = "Sinkhole Status",
        .command = "sinkhole status\n",
        .details_header = "Sinkhole Status",
        .details_text = "Show sinkhole running state.\n",
    },
    {
        .label = "Sinkhole Stats",
        .command = "sinkhole stats\n",
        .details_header = "Sinkhole Stats",
        .details_text = "Show detailed sinkhole\nstatistics.\n",
    },
    {
        .label = "Add Domain...",
        .command = "sinkhole add",
        .needs_input = true,
        .input_text = "example.com",
        .details_header = "Add Blocked Domain",
        .details_text = "Add a domain to the\nblocklist.\n",
    },
    {
        .label = "Remove Domain...",
        .command = "sinkhole remove",
        .needs_input = true,
        .input_text = "example.com",
        .details_header = "Remove Domain",
        .details_text = "Remove a domain from the\nblocklist.\n",
    },
    {
        .label = "Show Blocklist Sources",
        .command = "sinkhole download\n",
        .details_header = "Blocklist Sources",
        .details_text = "List the available blocklists\nand their numbers.\n",
    },
    {
        .label = "Download Blocklist...",
        .command = "sinkhole download",
        .needs_input = true,
        .input_text = "List number",
        .details_header = "Download Blocklist",
        .details_text = "Download a blocklist to SD.\n"
                        "See sources list for numbers.\n"
                        "Then use Load Blocklist.\n"
                        "Needs PSRAM + SD + WiFi.\n",
    },
    {
        .label = "Load Blocklist...",
        .command = "sinkhole load",
        .needs_input = true,
        .input_text = "Blocklist filename",
        .details_header = "Load Blocklist",
        .details_text = "Activate a downloaded list\n"
                        "and start the sinkhole.\n"
                        "1: peter_lowe.txt\n"
                        "2: oisd.txt\n"
                        "3: stevenblack.txt\n"
                        "Wait for download to finish.\n"
                        "Requires WiFi/Ethernet.\n",
    },
    {
        .label = "Reload Sinkhole",
        .command = "sinkhole reload\n",
        .details_header = "Reload Sinkhole",
        .details_text = "Reload the active blocklist\n"
                        "after adding/removing domains.\n"
                        "Use Load Blocklist to switch\n"
                        "to a downloaded list.\n",
    },
    {
        .label = "Blocklist Log: On",
        .command = "sinkhole log on\n",
        .details_header = "Log Queries",
        .details_text = "Log sinkhole DNS queries to\nthe SD card.\n",
    },
    {
        .label = "Blocklist Log: Off",
        .command = "sinkhole log off\n",
        .details_header = "Stop Logging",
        .details_text = "Stop logging sinkhole DNS\nqueries.\n",
    },
};

// WiFi Settings sub-category: LED & RGB
static const MenuCommand wifi_settings_led_commands[] = {
    {
        .label = "< LED: Rainbow >",
        .command = "rgbmode rainbow\n",
        .details_header = "LED Effects",
        .details_text = "Left/Right: pick an LED\n"
                        "effect (rainbow, police,\n"
                        "strobe, off, color).\n",
    },
    {
        .label = "Set NeoPixel Brightness",
        .command = "setneopixelbrightness",
        .needs_input = true,
        .input_text = "0-100",
        .details_header = "NeoPixel Brightness",
        .details_text = "Set NeoPixel brightness\n"
                        "0-100%.\n",
    },
    {
        .label = "Get NeoPixel Brightness",
        .command = "getneopixelbrightness\n",
        .details_header = "NeoPixel Brightness",
        .details_text = "Show NeoPixel brightness.\n",
    },
    {
        .label = "Set RGB Profile",
        .command = "setrgbmode",
        .needs_input = true,
        .input_text = "normal|rainbow|stealth",
        .details_header = "Set RGB Profile",
        .details_text = "Save default LED mode for\n"
                        "after reboot.\n",
    },
    {
        .label = "Set RGB Pins",
        .command = "setrgbpins",
        .needs_input = true,
        .input_text = "<red> <green> <blue>",
        .details_header = "Set RGB Pins",
        .details_text = "Set RGB pins. Same value for\n"
                        "all if using one pin.\n"
                        "Requires restart.\n",
    },
    {
        .label = "Set RGB LED Count",
        .command = "setrgbcount",
        .needs_input = true,
        .input_text = "1-512",
        .details_header = "Set RGB LED Count",
        .details_text = "Number of RGB LEDs (1-512).\n"
                        "Effects span the right length.\n",
    },
};

// WiFi Settings sub-category: SD Card
static const MenuCommand wifi_settings_sd_commands[] = {
    {
        .label = "Show SD Pin Config",
        .command = "sd_config",
        .details_header = "SD Pin Config",
        .details_text = "Show SD GPIO pins (MMC + SPI).\n",
    },
    {
        .label = "Set SD Pins (MMC)",
        .command = "sd_pins_mmc",
        .needs_input = true,
        .input_text = "<clk> <cmd> <d0..d3>",
        .details_header = "Set SD Pins (MMC)",
        .details_text = "Set SDMMC pins.\n"
                        "Requires restart + SDMMC\n"
                        "firmware.\n",
    },
    {
        .label = "Set SD Pins (SPI)",
        .command = "sd_pins_spi",
        .needs_input = true,
        .input_text = "<cs> <clk> <miso> <mosi>",
        .details_header = "Set SD Pins (SPI)",
        .details_text = "Set SPI pins.\n"
                        "Requires restart + SPI\n"
                        "firmware.\n",
    },
    {
        .label = "Save SD Pin Config",
        .command = "sd_save_config",
        .confirm = &confirm_sd_save,
        .details_header = "Save SD Pin Config",
        .details_text = "Save SD pin config to SD\n"
                        "(sd_config.conf).\n",
    },
};

// WiFi Settings sub-category: Settings Management
static const MenuCommand wifi_settings_mgmt_commands[] = {
    {
        .label = "Settings List",
        .command = "settings list\n",
        .details_header = "List Settings",
        .details_text = "List config keys + help.\n",
    },
    {
        .label = "Settings Get",
        .command = "settings get",
        .needs_input = true,
        .input_text = "Key",
        .details_header = "Get Setting",
        .details_text = "Read a config key's value.\n",
    },
    {
        .label = "Settings Set",
        .command = "settings set",
        .needs_input = true,
        .input_text = "Key Value",
        .details_header = "Set Setting",
        .details_text = "Set a config key's value.\n",
    },
    {
        .label = "Settings Help",
        .command = "settings help\n",
        .details_header = "Settings Help",
        .details_text = "CLI usage for settings.\n",
    },
    {
        .label = "Settings Reset (Key)",
        .command = "settings reset",
        .needs_input = true,
        .input_text = "Key",
        .details_header = "Reset Setting",
        .details_text = "Reset one config key.\n",
    },
    {
        .label = "Settings Reset (All)",
        .command = "settings reset\n",
        .details_header = "Reset Settings",
        .details_text = "Reset all config keys.\n",
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
        .label = "Reboot Device",
        .command = "reboot\n",
        .confirm = &confirm_reboot,
        .details_header = "Reboot",
        .details_text = "Restart the ESP device.",
    },
    {
        .label = "Identify Device",
        .command = "identify\n",
        .details_header = "Identify Device",
        .details_text = "Blink/beep to identify the\n"
                        "ESP.\n",
    },
    {
        .label = "List Scripts",
        .command = "script list\n",
        .details_header = "GhostScripts",
        .details_text = "List GhostScripts on SD.\n",
    },
    {
        .label = "Run Script...",
        .command = "script run",
        .needs_input = true,
        .input_text = "Script index",
        .details_header = "Run GhostScript",
        .details_text = "Run a GhostScript by index\n"
                        "(see List Scripts).\n",
    },
    {
        .label = "Script Status",
        .command = "script status\n",
        .details_header = "Script Status",
        .details_text = "Show running GhostScript\nstatus.\n",
    },
    {
        .label = "Stop Script",
        .command = "script stop\n",
        .details_header = "Stop GhostScript",
        .details_text = "Stop the running\nGhostScript.\n",
    },
    {
        .label = "Show Time",
        .command = "time\n",
        .details_header = "Device Time",
        .details_text = "Show the current device time.",
    },
    {
        .label = "Enable/Disable AP",
        .command = "apenable",
        .needs_input = true,
        .input_text = "on | off",
        .details_header = "AP Enable/Disable",
        .details_text = "Toggle the ESP AP across\n"
                        "reboots.\n",
    },
    {
        .label = "Load Config",
        .command = "loadconfig\n",
        .details_header = "Load Config",
        .details_text = "Reload configuration from the\n"
                        "SD card.",
    },
    {
        .label = "Coredump Dump",
        .command = "coredump dump\n",
        .details_header = "Coredump Dump",
        .details_text = "Read the stored crash\n"
                        "coredump.\n",
    },
    {
        .label = "Erase Coredump",
        .command = "coredump erase\n",
        .confirm = &confirm_coredump_erase,
        .details_header = "Erase Coredump",
        .details_text = "Erase the crash coredump.\n",
    },
    {
        .label = "Force Crash",
        .command = "crash\n",
        .confirm = &confirm_force_crash,
        .details_header = "Force Crash",
        .details_text = "Crash the ESP to make a\n"
                        "test coredump.\n",
    },
};

// WiFi Settings sub-category: Misc
static const MenuCommand wifi_settings_misc_commands[] = {
    {
        .label = "Privacy Mode",
        .command = "privacy",
        .needs_input = true,
        .input_text = "on | off | toggle | status",
        .details_header = "Privacy Mode",
        .details_text = "Hide SSIDs/MACs on screen.\n",
    },
    {
        .label = "Clock Style",
        .command = "clockstyle",
        .needs_input = true,
        .input_text = "digital | analog | segment",
        .details_header = "Clock Style",
        .details_text = "Switch clock face (digital,\n"
                        "analog, segment).\n",
    },
    {
        .label = "Status Bar Clock",
        .command = "statusbarclock",
        .needs_input = true,
        .input_text = "on | off | toggle | status",
        .details_header = "Status Bar Clock",
        .details_text = "Show/hide status bar clock.\n",
    },
    {
        .label = "Set Timezone",
        .command = "timezone",
        .needs_input = true,
        .input_text = "TZ String",
        .details_header = "Set Timezone",
        .details_text = "Set the clock timezone.\n"
                        "Ex: EST5EDT,M3.2.0,M11.1.0\n",
    },
    {
        .label = "Set WiFi Country",
        .command = "setcountry",
        .needs_input = true,
        .input_text = "Country Code (e.g. US)",
        .details_header = "Set WiFi Country",
        .details_text = "Set WiFi country code.\n"
                        "May need ESP32-C5.\n",
    },
    {
        .label = "Set Web Auth",
        .command = "webauth",
        .needs_input = true,
        .input_text = "on | off",
        .details_header = "Set Web Auth",
        .details_text = "Toggle WebUI authentication.\n",
    },
    {
        .label = "Set Log Level",
        .command = "loglevel",
        .needs_input = true,
        .input_text = "none|error|warn|info|debug|verbose",
        .details_header = "Log Level",
        .details_text = "Change ESP log verbosity.\n",
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
    .details_text = "Stops all WiFi activity\n"
                    "(scan, spam, attacks,\n"
                    "captures, portal).\n",
};

static const MenuCommand status_idle_commands[] = {
    {
        .label = "Life (Game of Life)",
        .command = "statusidle set life\n",
        .details_header = "Life Animation",
        .details_text = "Idle screen: Game of Life.\n",
    },
    {
        .label = "Ghost (Sprite)",
        .command = "statusidle set ghost\n",
        .details_header = "Ghost Animation",
        .details_text = "Idle screen: ghost sprite.\n",
    },
    {
        .label = "Starfield",
        .command = "statusidle set starfield\n",
        .details_header = "Starfield Animation",
        .details_text = "Idle screen: starfield.\n",
    },
    {
        .label = "HUD",
        .command = "statusidle set hud\n",
        .details_header = "HUD Animation",
        .details_text = "Idle screen: HUD overlay.\n",
    },
    {
        .label = "Matrix",
        .command = "statusidle set matrix\n",
        .details_header = "Matrix Animation",
        .details_text = "Idle screen: matrix rain.\n",
    },
    {
        .label = "Multiple Ghosts",
        .command = "statusidle set ghosts\n",
        .details_header = "Ghosts Animation",
        .details_text = "Idle screen: ghost swarm.\n",
    },
    {
        .label = "Spiral",
        .command = "statusidle set spiral\n",
        .details_header = "Spiral Animation",
        .details_text = "Idle screen: spiral.\n",
    },
    {
        .label = "Falling Leaves",
        .command = "statusidle set leaves\n",
        .details_header = "Falling Leaves Animation",
        .details_text = "Idle screen: falling leaves.\n",
    },
    {
        .label = "Bouncing Text",
        .command = "statusidle set bouncing\n",
        .details_header = "Bouncing Text Animation",
        .details_text = "Idle screen: bouncing text.\n",
    },
};

// BLE menu command definitions
// BLE Scanning sub-category: Flipper & Skimmer Detection
static const MenuCommand ble_scan_detect_commands[] = {
    {
        .label = "Find the Flippers",
        .command = "blescan -f\n",
        .details_header = "Flipper Scanner",
        .details_text = "Scan for nearby Flippers:\n"
                        "name, BT address, RSSI.\n"
                        "Range: ~50m\n",
    },
    {
        .label = "List Flippers",
        .command = "listflippers\n",
        .details_header = "List Flippers",
        .details_text = "List Flippers found in range.\n",
    },
    {
        .label = "Select Flipper to Track",
        .command = "selectflipper",
        .needs_input = true,
        .input_text = "Flipper Number",
        .details_header = "Select Flipper to Track",
        .details_text = "Pick a Flipper to track RSSI.\n",
    },
    {
        .label = "Skimmer Detection",
        .command = "capture -skimmer\n",
        .capture = &capture_skimmer_def,
        .details_header = "Skimmer Scanner",
        .details_text = "Detect card skimmers by BLE\n"
                        "signatures.\n",
    },
    {
        .label = "BLE Spam Detector",
        .command = "blescan -ds\n",
        .details_header = "BLE Spam Detector",
        .details_text = "Detect BLE ad spam nearby.\n"
                        "Stop via Stop BLE Scanning.\n",
    },
};

// BLE Scanning sub-category: AirTag
static const MenuCommand ble_scan_airtag_commands[] = {
    {
        .label = "AirTag Scanner",
        .command = "blescan -a\n",
        .details_header = "AirTag Scanner",
        .details_text = "Detect nearby Apple AirTags:\n"
                        "ID, signal, last seen.\n",
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
        .details_text = "Scan connectable BLE devices\n"
                        "for GATT enumeration.\n",
    },
    {
        .label = "List GATT Devices",
        .command = "listgatt\n",
        .details_header = "List GATT Devices",
        .details_text = "List GATT devices (with\n"
                        "tracker detection).\n",
    },
    {
        .label = "Select GATT Device",
        .command = "selectgatt",
        .needs_input = true,
        .input_text = "Device Index",
        .details_header = "Select GATT Device",
        .details_text = "Pick a GATT device to\n"
                        "enumerate or track.\n",
    },
    {
        .label = "Enumerate GATT Services",
        .command = "enumgatt\n",
        .details_header = "Enumerate GATT",
        .details_text = "Enumerate the selected\n"
                        "device's GATT services.\n",
    },
    {
        .label = "Track GATT Device",
        .command = "trackgatt\n",
        .details_header = "Track GATT Device",
        .details_text = "Track selected GATT device\n"
                        "RSSI live.\n",
    },
};

// BLE Scanning sub-category: Advertisers & Traffic
static const MenuCommand ble_scan_adv_commands[] = {
    {
        .label = "Scan Advertisers",
        .command = "blescan -adv\n",
        .details_header = "Scan Advertisers",
        .details_text = "Scan parsed BLE advertisers.\n"
                        "Then run List Advertisers.\n",
    },
    {
        .label = "List Advertisers",
        .command = "listadv\n",
        .details_header = "List Advertisers",
        .details_text = "List found BLE advertisers.\n",
    },
    {
        .label = "View All BLE Traffic",
        .command = "blescan -r\n",
        .details_header = "BLE Raw Traffic",
        .details_text = "Show all BLE traffic in\n"
                        "range.\n",
    },
    {
        .label = "Stop BLE Scanning",
        .command = "blescan -s\n",
        .details_header = "Stop BLE Scan",
        .details_text = "Stops any active BLE scanning.",
    },
    {
        .label = "Scan by OUI Prefix...",
        .command = "blescan -oui",
        .needs_input = true,
        .input_text = "00:1A:2B",
        .details_header = "OUI Scan",
        .details_text = "Scan advertisers by MAC OUI.\n"
                        "Format: 6 hex digits,\n"
                        "Ex: 00:1A:2B\n",
    },
    {
        .label = "Scan by Vendor...",
        .command = "blescan -vendor",
        .needs_input = true,
        .input_text = "Exact vendor name",
        .details_header = "Vendor Scan",
        .details_text = "Scan by OUI vendor name\n"
                        "(exact match), e.g.\n"
                        "Apple, Inc.\n",
    },
    {
        .label = "BLE Bridge",
        .command = "blebridge",
        .needs_input = true,
        .input_text = "start | stop | status | pair <peer>",
        .details_header = "BLE GhostLink Bridge",
        .details_text = "BLE GhostLink bridge:\n"
                        "start/stop/status/pair.\n",
    },
};

// BLE Scanning sub-category: Trackers & Beacons
static const MenuCommand ble_scan_tracker_commands[] = {
    {
        .label = "Start Tracker Detect",
        .command = "bledetect\n",
        .details_header = "Tracker Detection",
        .details_text = "Scan for trackers, skimmers\n"
                        "and beacons by signature.\n",
    },
    {
        .label = "List Detected Devices",
        .command = "bledetect -l\n",
        .details_header = "List Detected",
        .details_text = "List devices: index, type,\n"
                        "name/MAC, RSSI.\n",
    },
    {
        .label = "Follow Device (RSSI)...",
        .command = "bledetect -t",
        .needs_input = true,
        .input_text = "Device index",
        .details_header = "Follow Device",
        .details_text = "Follow one device's live RSSI\n"
                        "(index from List Detected).\n",
    },
    {
        .label = "Stop Tracker Detect",
        .command = "bledetect -s\n",
        .details_header = "Stop Detection",
        .details_text = "Stop detection (results are\n"
                        "kept).\n",
    },
    {
        .label = "Stop Following",
        .command = "bledetect -u\n",
        .details_header = "Stop Following",
        .details_text = "Stop tracking the selected\ndevice.\n",
    },
    {
        .label = "Spoof Detected AirTag...",
        .command = "bledetect -sp",
        .needs_input = true,
        .input_text = "AirTag index",
        .details_header = "Spoof AirTag",
        .details_text = "Spoof a detected AirTag\n"
                        "(index from List Detected).\n"
                        "Stop with Stop Spoofing.\n",
    },
    {
        .label = "Stop Spoofing",
        .command = "stopspoof\n",
        .details_header = "Stop Spoofing",
        .details_text = "Stop AirTag spoofing.\n",
    },
    {
        .label = "Clear Results",
        .command = "bledetect -c\n",
        .details_header = "Clear Results",
        .details_text = "Clear stored results (stop\n"
                        "the scan first).\n",
    },
};

static const MenuCommand ble_capture_commands[] = {
    {
        .label = "BLE Raw Capture",
        .command = "capture -ble\n",
        .capture = &capture_ble_def,
        .details_header = "BLE Raw Capture",
        .details_text = "Raw BLE capture to PCAP.\n"
                        "Range: ~10-30m\n",
    },
};

static const MenuCommand ble_attack_commands[] = {
    {
        .label = "< BLE Spam (Apple) >",
        .command = "blespam -apple\n",
        .details_header = "Variable BLE Spam",
        .details_text = "Use Left/Right:\n"
                        "Apple, Microsoft, Samsung,\n"
                        "Google, Random.\n"
                        "Range: ~50m\n",
    },
    {
        .label = "Stop BLE Spam",
        .command = "blespam -s\n",
        .details_header = "Stop BLE Spam",
        .details_text = "Stops BLE advertisement\n"
                        "spam attacks.",
    },
    {
        .label = "Spoof Selected AirTag",
        .command = "spoofairtag\n",
        .details_header = "Spoof AirTag",
        .details_text = "Spoof the selected AirTag.",
    },
    {
        .label = "Stop AirTag Spoof",
        .command = "stopspoof\n",
        .details_header = "Stop Spoofing",
        .details_text = "Stops AirTag spoofing.",
    },
    {
        .label = "Set Spam Name...",
        .command = "blespam -name",
        .needs_input = true,
        .input_text = "Name (no spaces)",
        .details_header = "Set Spam Name",
        .details_text = "Set BLE spam name (no\n"
                        "spaces); renames a running\n"
                        "spam.\n",
    },
};

static const MenuCommand ble_stop_command = {
    .label = "Stop All BLE",
    .command = "stop\n",
    .details_header = "Stop BLE Operations",
    .details_text = "Stops all BLE activity\n"
                    "(scan, skimmer, captures,\n"
                    "spoofs).\n",
};

// GPS sub-category: GPS Config
static const MenuCommand gps_config_commands[] = {
    {
        .label = "GPS Info",
        .command = "gpsinfo\n",
        .details_header = "GPS Information",
        .details_text = "Show GPS details: position,\n"
                        "altitude, speed, sats.\n",
    },
    {
        .label = "View GPS Pin",
        .command = "gpspin\n",
        .details_header = "View GPS RX Pin",
        .details_text = "Show current GPS RX pin.\n",
    },
    {
        .label = "Set GPS Pin",
        .command = "gpspin",
        .needs_input = true,
        .input_text = "Pin Number",
        .details_header = "Set GPS RX Pin",
        .details_text = "Set GPS RX pin for external\n"
                        "modules. Persists to NVS;\n"
                        "restart GPS to apply.\n",
    },
    {
        .label = "Set GPS Baud",
        .command = "gpsbaud",
        .needs_input = true,
        .input_text = "Baud Rate (e.g. 9600)",
        .details_header = "Set GPS Baud Rate",
        .details_text = "Set GPS baud rate (or auto).\n"
                        "Persists to NVS.\n",
    },
};

// GPS sub-category: Wardriving
static const MenuCommand gps_wardrive_commands[] = {
    {
        .label = "Start Wardriving",
        .command = "startwd\n",
        .capture = &wardrive_wifi_def,
        .details_header = "Wardrive Mode",
        .details_text = "Map WiFi + GPS to CSV.\n",
    },
    {
        .label = "BLE Wardriving",
        .command = "blewardriving\n",
        .capture = &wardrive_ble_def,
        .details_header = "BLE Wardriving",
        .details_text = "Map BLE + GPS to CSV.\n",
    },
    {
        .label = "Stop All GPS",
        .command = "stop\n",
        .details_header = "Stop GPS Operations",
        .details_text = "Stops GPS: info, WiFi and\n"
                        "BLE wardriving.\n",
    },
    {
        .label = "Stop BLE Wardriving",
        .command = "blewardriving -s\n",
        .details_header = "Stop BLE Wardrive",
        .details_text = "Stops BLE wardriving capture\n"
                        "and logging.",
    },
    {
        .label = "WiGLE List CSV Files",
        .command = "wigle files\n",
        .details_header = "WiGLE CSV Files",
        .details_text = "List GPS CSVs on SD for\n"
                        "manual upload.\n",
    },
    {
        .label = "WiGLE Upload All",
        .command = "wigle upload all\n",
        .details_header = "WiGLE Upload All",
        .details_text = "Upload all pending CSVs\n"
                        "(needs WiFi + API key).\n",
    },
    {
        .label = "WiGLE Upload File...",
        .command = "wigle upload",
        .needs_input = true,
        .input_text = "CSV filename",
        .details_header = "WiGLE Upload File",
        .details_text = "Upload one CSV from SD\n"
                        "(name from List CSV Files).\n",
    },
    {
        .label = "WiGLE Account Stats",
        .command = "wigle stats\n",
        .details_header = "WiGLE Stats",
        .details_text = "Show WiGLE account stats\n"
                        "(needs WiFi + API key).\n",
    },
    {
        .label = "WiGLE Show Settings",
        .command = "wigle show\n",
        .details_header = "WiGLE Settings",
        .details_text = "Show WiGLE key + settings.\n",
    },
    {
        .label = "WiGLE Set API Token...",
        .command = "wigle API",
        .needs_input = true,
        .input_text = "Token or Name:Token",
        .details_header = "WiGLE API Token",
        .details_text = "Set WiGLE API credentials\n"
                        "from wigle.net/account\n"
                        "(encoded or Name:Token).\n",
    },
    {
        .label = "WiGLE Auto Upload",
        .command = "wigle auto",
        .needs_input = true,
        .input_text = "on | off",
        .details_header = "WiGLE Auto Upload",
        .details_text = "Auto-upload wardrive data\n"
                        "to WiGLE.\n",
    },
    {
        .label = "WiGLE Donate",
        .command = "wigle donate",
        .needs_input = true,
        .input_text = "on | off",
        .details_header = "WiGLE Donate Mode",
        .details_text = "Toggle WiGLE donate mode.\n",
    },
};

// Aerial Detector menu command definitions - all in one menu
static const MenuCommand aerial_commands[] = {
    {
        .label = "Start Scan (30s)",
        .command = "aerialscan 30\n",
        .details_header = "Scan for Drones",
        .details_text = "Scan for drones: RemoteID,\n"
                        "DJI, drone networks.\n"
                        "30s WiFi+BLE.\n",
    },
    {
        .label = "List Detected Drones",
        .command = "aeriallist\n",
        .details_header = "Detected Devices",
        .details_text = "List drones: ID, type, GPS,\n"
                        "altitude, operator, RSSI.\n",
    },
    {
        .label = "Track Drone by Index",
        .command = "aerialtrack",
        .needs_input = true,
        .input_text = "Device Index",
        .details_header = "Track Drone",
        .details_text = "Track a drone by index\n"
                        "(from the drone list).\n",
    },
    {
        .label = "Stop All",
        .command = "aerialstop\n",
        .details_header = "Stop All Operations",
        .details_text = "Stops aerial scan, tracking\n"
                        "and spoofing.\n",
    },
    {
        .label = "Quick Scan (15s)",
        .command = "aerialscan 15\n",
        .details_header = "Quick Scan",
        .details_text = "Fast 15 s drone scan.\n",
    },
    {
        .label = "Extended Scan (60s)",
        .command = "aerialscan 60\n",
        .details_header = "Extended Scan",
        .details_text = "Extended 60 s drone scan.\n",
    },
    {
        .label = "Track Drone by MAC",
        .command = "aerialtrack",
        .needs_input = true,
        .input_text = "MAC Address",
        .details_header = "Track by MAC",
        .details_text = "Track a drone by MAC.\n"
                        "Ex: aa:bb:cc:dd:ee:ff\n",
    },
    {
        .label = "Spoof Test Drone",
        .command = "aerialspoof\n",
        .details_header = "Test Spoof",
        .details_text = "Broadcast a test RemoteID\n"
                        "(GHOST-TEST, San Francisco).\n"
                        "WiFi pauses during BLE.\n",
    },
    {
        .label = "Custom Spoof",
        .command = "aerialspoof",
        .needs_input = true,
        .input_text = "ID Lat Lon Alt",
        .details_header = "Custom Spoof",
        .details_text = "Broadcast custom RemoteID:\n"
                        "ID lat lon alt\n"
                        "Ex: GHOST-1 40.7 -74.0 100\n",
    },
    {
        .label = "Stop Spoofing",
        .command = "aerialspoofstop\n",
        .details_header = "Stop Spoofing",
        .details_text = "Stops RemoteID broadcast\n"
                        "and restores WiFi.\n",
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
        .details_text = "Send Flipper IR files to ESP.\n",
    },
    {
        .label = "IR Learn (Auto File)",
        .command = "ir learn\n",
        .details_header = "Learn IR (Auto)",
        .details_text = "Learn IR (10s). New file\n"
                        "auto-created.\n",
    },
    {
        .label = "IR Learn (Path)",
        .command = "ir learn",
        .needs_input = true,
        .input_text = "Path (optional)",
        .details_header = "Learn IR (Path)",
        .details_text = "Learn IR (10s). Blank = new\n"
                        "file, or give a path to\n"
                        "append.\n",
    },
    {
        .label = "IR Receive",
        .command = "ir rx",
        .needs_input = true,
        .input_text = "Timeout (default 60)",
        .details_header = "Receive IR",
        .details_text = "Wait for one IR signal;\n"
                        "prints decoded/raw.\n",
    },
    {
        .label = "IR List Files (Raw)",
        .command = "ir list\n",
        .details_header = "List IR Files",
        .details_text = "List .ir/.json remote files.\n",
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
        .confirm = &confirm_ir_dazzler,
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
#define IR_INITIAL_CAPACITY    4

static bool ir_grow_catalog(
    void** catalog,
    size_t* capacity,
    size_t element_size,
    size_t required,
    size_t maximum) {
    if(!catalog || !capacity || required > maximum) return false;
    if(required <= *capacity) return true;

    size_t new_capacity = *capacity ? *capacity * 2 : IR_INITIAL_CAPACITY;
    if(new_capacity < required) new_capacity = required;
    if(new_capacity > maximum) new_capacity = maximum;

    void* resized = realloc(*catalog, new_capacity * element_size);
    if(!resized) return false;

    if(new_capacity > *capacity) {
        memset(
            (uint8_t*)resized + (*capacity * element_size),
            0,
            (new_capacity - *capacity) * element_size);
    }
    *catalog = resized;
    *capacity = new_capacity;
    return true;
}

static bool ir_ensure_remotes(AppState* state, size_t required) {
    return state && ir_grow_catalog(
                        (void**)&state->ir_remotes,
                        &state->ir_remote_capacity,
                        sizeof(IrRemoteEntry),
                        required,
                        IR_REMOTE_CAPACITY);
}

static bool ir_ensure_signals(AppState* state, size_t required) {
    return state && ir_grow_catalog(
                        (void**)&state->ir_signals,
                        &state->ir_signal_capacity,
                        sizeof(IrSignalEntry),
                        required,
                        IR_SIGNAL_CAPACITY);
}

static bool ir_ensure_universals(AppState* state, size_t required) {
    return state && ir_grow_catalog(
                        (void**)&state->ir_universals,
                        &state->ir_universal_capacity,
                        sizeof(IrUniversalEntry),
                        required,
                        IR_UNIVERSAL_CAPACITY);
}

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

// Count reply lines that begin with an "[index]" entry marker.
static size_t ir_count_entry_lines(const char* buf) {
    size_t count = 0;
    const char* p = buf;
    while(p && *p) {
        const char* nl = strchr(p, '\n');
        const char* end = nl ? nl : p + strlen(p);
        const char* s = p;
        while(s < end && (*s == ' ' || *s == '\t'))
            s++;
        if(s < end && *s == '[') count++;
        if(!nl) break;
        p = nl + 1;
    }
    return count;
}

// Read the announced entry count from a "Signals in ... (N):" or
// "Unique buttons in ... (N):" header. Returns -1 if not present.
static int ir_header_entry_count(const char* buf) {
    const char* header = strstr(buf, "Signals in ");
    if(!header) header = strstr(buf, "Unique buttons in ");
    if(!header) return -1;
    const char* paren = strchr(header, '(');
    if(!paren) return -1;
    return atoi(paren + 1);
}

// Wait for an ESP IR reply to finish arriving before parsing it. Replies that
// announce an entry count ("... (N):") are complete once N "[index]" lines have
// arrived; other replies are considered complete after a short quiet period.
// This prevents parsing a partially-received list (which showed only some IR
// buttons until the view was reopened).
static bool ir_wait_for_reply(
    AppState* state,
    char* buffer,
    size_t buffer_size,
    size_t* out_len,
    bool expect_entry_list,
    uint32_t timeout_ms) {
    if(!state || !state->uart_context || !buffer || buffer_size == 0) return false;

    size_t len = 0;
    uint32_t start = furi_get_tick();
    uint32_t last_change = start;
    size_t last_len = 0;
    bool seen_header = false;

    while(furi_get_tick() - start < timeout_ms) {
        furi_delay_ms(50);
        if(!uart_copy_text_buffer_tail(state->uart_context, buffer, buffer_size, &len) ||
           len == 0) {
            continue;
        }

        const bool header =
            strstr(buffer, "Signals in ") || strstr(buffer, "Unique buttons in ") ||
            strstr(buffer, "IR files in ") || strstr(buffer, "Universal Files in ");
        if(header) seen_header = true;

        if(expect_entry_list) {
            const int expected = ir_header_entry_count(buffer);
            if(expected > 0 && ir_count_entry_lines(buffer) >= (size_t)expected) break;
        }

        if(len != last_len) {
            last_len = len;
            last_change = furi_get_tick();
        } else if(seen_header && furi_get_tick() - last_change >= 300) {
            break;
        }
    }

    if(out_len) *out_len = len;
    return len > 0;
}

static bool ir_query_and_parse_list(AppState* state) {
    if(!state || !state->uart_context) return false;

    uart_reset_text_buffers(state->uart_context);
    send_uart_command("ir list\n", state);

    char buffer[IR_UART_PARSE_BUF_SIZE];

    if(!ir_wait_for_reply(state, buffer, sizeof(buffer), NULL, false, 2000)) {
        return false;
    }

    state->ir_remote_count = 0;
    if(!ir_ensure_remotes(state, 1)) return false;

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
                if(state->ir_remote_count < IR_REMOTE_CAPACITY &&
                   ir_ensure_remotes(state, state->ir_remote_count + 1)) {
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

    if(!ir_wait_for_reply(state, buffer, sizeof(buffer), NULL, true, 3000)) {
        return false;
    }

    state->ir_signal_count = 0;
    if(!ir_ensure_signals(state, 1)) return false;

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
                if(state->ir_signal_count < IR_SIGNAL_CAPACITY &&
                   ir_ensure_signals(state, state->ir_signal_count + 1)) {
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
        if(!ir_ensure_signals(state, 1)) break;

        while(true) {
            uint16_t read = storage_file_read(file, buf, buf_size);
            if(read == 0) break;

            size_t pos = 0;
            while(pos < read && state->ir_signal_count < IR_SIGNAL_CAPACITY) {
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

                    if(state->ir_signal_count < IR_SIGNAL_CAPACITY &&
                       ir_ensure_signals(state, state->ir_signal_count + 1)) {
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

    if(!ir_wait_for_reply(state, buffer, sizeof(buffer), NULL, false, 3000)) {
        return false;
    }

    state->ir_universal_count = 0;
    if(!ir_ensure_universals(state, 1)) return false;
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

            if(state->ir_universal_count < IR_UNIVERSAL_CAPACITY &&
               ir_ensure_universals(state, state->ir_universal_count + 1)) {
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

    if(!ir_wait_for_reply(state, buffer, sizeof(buffer), NULL, true, 5000)) {
        return false;
    }

    state->ir_signal_count = 0;
    if(!ir_ensure_signals(state, 1)) return false;

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
                if(!exists && state->ir_signal_count < IR_SIGNAL_CAPACITY &&
                   ir_ensure_signals(state, state->ir_signal_count + 1)) {
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
        if(cmd_ctx->command->capture) {
            FURI_LOG_I("Capture", "Attempting to open PCAP file before sending capture command.");
            file_opened = uart_receive_data(
                cmd_ctx->state->uart_context,
                cmd_ctx->state->view_dispatcher,
                cmd_ctx->state,
                cmd_ctx->command->capture->prefix,
                cmd_ctx->command->capture->ext,
                cmd_ctx->command->capture->folder);

            if(!file_opened) {
                FURI_LOG_E("Capture", "Failed to open PCAP file. Aborting capture command.");
                confirmation_cancel_callback(cmd_ctx);
                return;
            }

            // Send capture command
            send_uart_command(cmd_ctx->command->command, cmd_ctx->state);
            FURI_LOG_I("Capture", "Capture command sent to firmware.");
        } else {
            // Restore the origin so result dialogs and logs return to the menu.
            AppState* state = cmd_ctx->state;
            state->current_view = state->previous_view;
            if(!handle_ir_command_feedback(state, cmd_ctx->command->command)) {
                uart_receive_data(state->uart_context, state->view_dispatcher, state, "", "", "");
                send_uart_command(cmd_ctx->command->command, state);
            }
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
    // Recon / sinkhole / tracker sub-categories
    case VIEW_WIFI_RECON:
        show_wifi_recon_menu(state);
        break;
    case VIEW_WIFI_NETWORK_SINKHOLE:
        show_wifi_network_sinkhole_menu(state);
        break;
    case VIEW_BLE_SCAN_TRACKERS:
        show_ble_scan_tracker_menu(state);
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
        char buffer[320];
        snprintf(
            buffer,
            sizeof(buffer),
            "connect \"%s\" \"%s\"\n",
            input_state->connect_ssid,
            input_state->input_buffer);
        uart_send(input_state->uart_context, (uint8_t*)buffer, strlen(buffer));
        input_state->connect_input_stage = 0;
        input_state->connect_ssid[0] = '\0';
        input_state->uart_command = NULL;
        if(input_state->input_buffer) memset(input_state->input_buffer, 0, INPUT_BUFFER_SIZE);

        // Leave the text input so a second submit can't be mistaken for a
        // password and sent as "connect <password>".
        uart_receive_data(
            input_state->uart_context, input_state->view_dispatcher, input_state, "", "", "");
        input_state->previous_view = VIEW_WIFI_NETWORK_CONN;
        input_state->connect_log_active = true;
        return;
    } else if(input_state->uart_command) {
        if(strcmp(input_state->uart_command, "ir send") == 0) {
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

            // Begin HTML block. The trailing newline puts the marker on its
            // own line so the ESP starts capturing before the first HTML byte.
            const char* html_begin_marker = "[HTML/BEGIN]";
            uart_send(state->uart_context, (const uint8_t*)html_begin_marker, 12);
            uart_send(state->uart_context, (const uint8_t*)"\n", 1);

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

    if(!command->needs_input && !command->confirm && !command->capture) {
        if(handle_ir_command_feedback(state, command->command)) {
            return;
        }
    }

    if(command->needs_input && strcmp(command->command, "connect") == 0) {
        state->connect_input_stage = 1;
        state->connect_log_active = false;
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
    if(command->confirm) {
        MenuCommandContext* cmd_ctx = malloc(sizeof(MenuCommandContext));
        if(!cmd_ctx) return;
        cmd_ctx->state = state;
        cmd_ctx->command = command;
        state->active_confirm_context = cmd_ctx;
        state->previous_view = state->current_view;
        confirmation_view_set_header(state->confirmation_view, command->confirm->header);
        confirmation_view_set_text(state->confirmation_view, command->confirm->text);
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
                show_result_dialog(
                    state,
                    "Capture unavailable",
                    "Check free memory\nand SD card space.\nCapture not started.");
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
    if(command->capture) {
        // Save view and show terminal log
        state->previous_view = state->current_view;
        bool file_opened = uart_receive_data(
            state->uart_context,
            state->view_dispatcher,
            state,
            command->capture->prefix,
            command->capture->ext,
            command->capture->folder);

        if(!file_opened) {
            FURI_LOG_E("Capture", "Failed to open capture file");
            show_result_dialog(
                state,
                "Capture unavailable",
                "Check free memory\nand SD card space.\nCapture not started.");
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
    if(!state->submenu_original_input) {
        state->submenu_original_input = menu_view->input_callback;
        state->submenu_original_context = menu_view->context;
    }
    view_set_context(menu_view, state);
    view_set_input_callback(menu_view, menu_view_input_handler);

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
    case VIEW_WIFI_RECON:
        last_index = state->last_wifi_recon_index;
        break;
    case VIEW_WIFI_NETWORK_SINKHOLE:
        last_index = state->last_wifi_network_sinkhole_index;
        break;
    case VIEW_BLE_SCAN_TRACKERS:
        last_index = state->last_ble_scan_tracker_index;
        break;
    }
    if(last_index < command_count) {
        submenu_set_selected_item(menu, last_index);
    }

    // Transient views (log output, text input, confirmation) already have the
    // correct previous_view set by whoever triggered them; don't clobber it
    // with the transient view id when returning from one of them, or back
    // navigation ends up bouncing to the default (main menu) case instead.
    if(state->current_view != VIEW_TEXT_BOX && state->current_view != VIEW_TEXT_INPUT &&
       state->current_view != VIEW_CONFIRMATION) {
        state->previous_view = state->current_view;
    }
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

void show_wifi_recon_menu(AppState* state) {
    show_menu(
        state,
        wifi_recon_commands,
        COUNT_OF(wifi_recon_commands),
        "Recon Tools",
        state->wifi_recon_menu,
        VIEW_WIFI_RECON);
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
    submenu_add_item(state->wifi_network_menu, "WiFi Connection >", 0, submenu_callback, state);
    submenu_add_item(state->wifi_network_menu, "Evil Portal >", 1, submenu_callback, state);
    submenu_add_item(state->wifi_network_menu, "Network Scan >", 2, submenu_callback, state);
    submenu_add_item(state->wifi_network_menu, "IoT Control >", 3, submenu_callback, state);
    submenu_add_item(state->wifi_network_menu, "WebUI >", 4, submenu_callback, state);
    submenu_add_item(state->wifi_network_menu, "DNS Sinkhole >", 5, submenu_callback, state);
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

void show_wifi_network_sinkhole_menu(AppState* state) {
    show_menu(
        state,
        wifi_network_sinkhole_commands,
        COUNT_OF(wifi_network_sinkhole_commands),
        "DNS Sinkhole",
        state->wifi_network_sinkhole_menu,
        VIEW_WIFI_NETWORK_SINKHOLE);
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
    submenu_add_item(state->ble_scanning_menu, "Trackers & Beacons >", 4, submenu_callback, state);
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

void show_ble_scan_tracker_menu(AppState* state) {
    show_menu(
        state,
        ble_scan_tracker_commands,
        COUNT_OF(ble_scan_tracker_commands),
        "Trackers & Beacons",
        state->ble_scan_tracker_menu,
        VIEW_BLE_SCAN_TRACKERS);
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
    submenu_add_item(state->wifi_menu, "Recon Tools >", 1, submenu_callback, state);
    submenu_add_item(state->wifi_menu, "Packet Capture > ", 2, submenu_callback, state);
    submenu_add_item(state->wifi_menu, "Attacks > ", 3, submenu_callback, state);
    submenu_add_item(state->wifi_menu, "Evil Portal & Network >", 4, submenu_callback, state);
    submenu_add_item(state->wifi_menu, "Aerial Detector >", 5, submenu_callback, state);
    submenu_add_item(state->wifi_menu, wifi_stop_command.label, 6, submenu_callback, state);
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
    submenu_add_item(state->gps_menu, "Wardriving >", 0, submenu_callback, state);
    submenu_add_item(state->gps_menu, "GPS Config >", 1, submenu_callback, state);
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
    case VIEW_WIFI_RECON:
        if(index < COUNT_OF(wifi_recon_commands)) {
            command = &wifi_recon_commands[index];
            state->last_wifi_recon_index = index;
        }
        break;
    case VIEW_WIFI_NETWORK_SINKHOLE:
        if(index < COUNT_OF(wifi_network_sinkhole_commands)) {
            command = &wifi_network_sinkhole_commands[index];
            state->last_wifi_network_sinkhole_index = index;
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
    case VIEW_BLE_SCAN_TRACKERS:
        if(index < COUNT_OF(ble_scan_tracker_commands)) {
            command = &ble_scan_tracker_commands[index];
            state->last_ble_scan_tracker_index = index;
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
            show_wifi_recon_menu(state);
            break;
        case 2:
            show_wifi_capture_menu(state);
            break;
        case 3:
            show_wifi_attack_menu(state);
            break;
        case 4:
            show_wifi_network_menu(state);
            break;
        case 5:
            show_aerial_menu(state);
            state->last_aerial_category_index = 0;
            break;
        case 6:
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
            show_gps_wardrive_menu(state);
            break;
        case 1:
            show_gps_config_menu(state);
            break;
        }
        break;
    case VIEW_WIFI_NETWORK:
        state->last_wifi_network_index = index;
        switch(index) {
        case 0:
            show_wifi_network_conn_menu(state);
            break;
        case 1:
            show_wifi_network_portal_menu(state);
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
        case 5:
            show_wifi_network_sinkhole_menu(state);
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
        case 4:
            show_ble_scan_tracker_menu(state);
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
    case VIEW_WIFI_RECON:
    case VIEW_WIFI_NETWORK_SINKHOLE:
        handle_wifi_menu(state, index);
        break;
    case VIEW_BLE_SCAN_DETECT:
    case VIEW_BLE_SCAN_AIRTAG:
    case VIEW_BLE_SCAN_GATT:
    case VIEW_BLE_SCAN_ADV:
    case VIEW_BLE_SCAN_TRACKERS:
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
        // A connect log is informational: leaving it should not stop the
        // connection that was just started.
        if(state->settings.stop_on_back_index && !state->connect_log_active) {
            send_uart_command(wifi_stop_command.command, state);
        }
        // The firmware buffers captured rows and flushes them as framed chunks;
        // stopping makes it emit one final flush. Give that frame a moment to
        // arrive before the capture streams are torn down, otherwise the tail
        // of the capture is lost or leaks into the text log.
        if(state->settings.stop_on_back_index && !state->connect_log_active &&
           state->uart_context && state->uart_context->capture_type != UartFrameText) {
            uart_wait_capture_idle(state->uart_context);
        }
        state->connect_log_active = false;
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
    } else if(cv == VIEW_WIFI_RECON) {
        navigate_to_view(state, VIEW_WIFI);
    } else if(cv == VIEW_WIFI_NETWORK_SINKHOLE) {
        navigate_to_view(state, VIEW_WIFI_NETWORK);
    } else if(cv == VIEW_BLE_SCAN_TRACKERS) {
        navigate_to_view(state, VIEW_BLE_SCANNING);
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
        if(state->connect_log_active) {
            // Leaving the connect log must not cancel the connection.
            return back_event_callback(state);
        }
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
                    state->connect_log_active ?
                        "Scroll paused.\nPress Right arrow to resume.\nPress OK to return." :
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

static bool menu_view_input_handler(InputEvent* event, void* context) {
    AppState* state = context;
    if(!state || !event) return false;

    if(menu_input_handler(event, context)) return true;

    // The shared view keeps this handler when rebuilt as a category or IR list.
    // Let the native submenu handle navigation and item callbacks in those views.
    return state->submenu_original_input &&
           state->submenu_original_input(event, state->submenu_original_context);
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
    case VIEW_WIFI_RECON:
        current_menu = state->wifi_recon_menu;
        commands = wifi_recon_commands;
        commands_count = COUNT_OF(wifi_recon_commands);
        break;
    case VIEW_WIFI_NETWORK_SINKHOLE:
        current_menu = state->wifi_network_sinkhole_menu;
        commands = wifi_network_sinkhole_commands;
        commands_count = COUNT_OF(wifi_network_sinkhole_commands);
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
    case VIEW_BLE_SCAN_TRACKERS:
        current_menu = state->ble_scan_tracker_menu;
        commands = ble_scan_tracker_commands;
        commands_count = COUNT_OF(ble_scan_tracker_commands);
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
                    } else if(state->current_view == VIEW_WIFI_RECON) {
                        state->last_wifi_recon_index = current_index;
                    } else if(state->current_view == VIEW_WIFI_NETWORK_SINKHOLE) {
                        state->last_wifi_network_sinkhole_index = current_index;
                    } else if(state->current_view == VIEW_BLE_SCAN_TRACKERS) {
                        state->last_ble_scan_tracker_index = current_index;
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
            } else if(state->current_view == VIEW_WIFI_RECON) {
                navigate_to_view(state, VIEW_WIFI);
            } else if(state->current_view == VIEW_WIFI_NETWORK_SINKHOLE) {
                navigate_to_view(state, VIEW_WIFI_NETWORK);
            } else if(state->current_view == VIEW_BLE_SCAN_TRACKERS) {
                navigate_to_view(state, VIEW_BLE_SCANNING);
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
