#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_box.h>
#include <gui/modules/text_input.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/number_input.h>
#include <gui/view.h>
#include <notification/notification_messages.h>
#include "protocols/ping_graph.h"
#include "protocols/history.h"
#include "protocols/pxe_server.h"
#include "bridge/eth_bridge.h"
#include "bridge/pcap_dump.h"
#include "ip_keyboard.h"

/* Forward declarations */
typedef struct LanTesterApp LanTesterApp;

/* View IDs for ViewDispatcher */
typedef enum {
    LanTesterViewMainMenu,
    LanTesterViewContPing,
    LanTesterViewIpKeyboard,
    LanTesterViewHistory,
    LanTesterViewAbout,
    LanTesterViewCatPortInfo,
    LanTesterViewCatScan,
    LanTesterViewCatDiag,
    LanTesterViewCatTraffic,
    LanTesterViewCatUtilities,
    LanTesterViewPortScanMode,
    LanTesterViewSettings,
    LanTesterViewEthBridge,
    LanTesterViewPxeSettings,
    LanTesterViewPxeHelp,
    LanTesterViewPacketCapture,
    LanTesterViewHostList,
    LanTesterViewHostActions,
    LanTesterViewAutoTest,
    LanTesterViewCatSecurity,
    LanTesterViewToolResult, /* shared TextBox for all tools */
    LanTesterViewToolInput, /* shared TextInput for all tools */
    LanTesterViewToolByteInput, /* shared ByteInput for MAC entry */
    LanTesterViewNumberInput, /* shared NumberInput */
} LanTesterView;

/* Main menu item indices */
typedef enum {
    LanTesterMenuItemAutoTest,
    LanTesterMenuItemLinkInfo,
    LanTesterMenuItemLldpCdp,
    LanTesterMenuItemArpScan,
    LanTesterMenuItemDhcpAnalyze,
    LanTesterMenuItemPing,
    LanTesterMenuItemStats,
    LanTesterMenuItemDnsLookup,
    LanTesterMenuItemWol,
    LanTesterMenuItemContPing,
    LanTesterMenuItemPortScan,
    LanTesterMenuItemPortScanFull,
    LanTesterMenuItemMacChanger,
    LanTesterMenuItemTraceroute,
    LanTesterMenuItemPortScanCustom,
    LanTesterMenuItemPingSweep,
    LanTesterMenuItemDiscovery,
    LanTesterMenuItemStpVlan,
    LanTesterMenuItemHistory,
    LanTesterMenuItemAbout,
    LanTesterMenuItemEthBridge,
    LanTesterMenuItemPxeServer,
    LanTesterMenuItemFileManager,
    LanTesterMenuItemPacketCapture,
    LanTesterMenuItemSnmpGet,
    LanTesterMenuItemNtpDiag,
    LanTesterMenuItemNtpSync,
    LanTesterMenuItemNetbiosQuery,
    LanTesterMenuItemDnsPoisonCheck,
    LanTesterMenuItemArpWatch,
    LanTesterMenuItemRogueDhcp,
    LanTesterMenuItemRogueRa,
    LanTesterMenuItemDhcpFingerprint,
    LanTesterMenuItemEapolProbe,
    LanTesterMenuItemVlanHopTop10,
    LanTesterMenuItemVlanHopCustom,
    LanTesterMenuItemTftpClient,
    LanTesterMenuItemIpmiClient,
    LanTesterMenuItemPxeDownload,
} LanTesterMenuItem;

/* Packet statistics counters */
typedef struct {
    uint32_t total_frames;
    uint32_t broadcast_frames;
    uint32_t multicast_frames;
    uint32_t unicast_frames;
    uint32_t ipv4_frames;
    uint32_t arp_frames;
    uint32_t ipv6_frames;
    uint32_t lldp_frames;
    uint32_t cdp_frames;
    uint32_t unknown_frames;
} PacketStats;

/* Application state */
struct LanTesterApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    Submenu* submenu_cat_portinfo;
    Submenu* submenu_cat_scan;
    Submenu* submenu_cat_diag;
    Submenu* submenu_cat_traffic;
    Submenu* submenu_cat_utilities;
    Submenu* submenu_port_scan_mode;
    View* view_cont_ping;
    IpKeyboard* ip_keyboard;
    Submenu* submenu_history;
    HistoryState* history_state;
    uint16_t history_selected; /* index of currently viewed file */
    TextBox* text_box_about;
    VariableItemList* settings_list;
    NotificationApp* notifications;

    /* User settings (persisted to SD) */
    bool setting_autosave; /* auto-save results to history */
    bool setting_sound; /* LED/vibro/sound notifications */
    bool dns_custom_enabled; /* use custom DNS instead of DHCP */
    uint8_t dns_custom_server[4]; /* custom DNS IP (default 8.8.8.8) */
    char dns_custom_ip_input[16]; /* text input buffer for DNS IP */

    /* Ping settings */
    uint8_t ping_count; /* packets for normal ping (1-100, default 4) */
    uint16_t ping_timeout_ms; /* reply timeout (500-10000, step 500, default 3000) */
    uint16_t ping_interval_ms; /* continuous ping interval (200-5000, step 200, default 1000) */

    /* Worker thread for non-blocking operations */
    FuriThread* worker_thread;
    volatile bool worker_running;
    uint32_t worker_op; /* LanTesterMenuItem value */

    /* W5500 state */
    bool w5500_initialized;
    bool link_up;
    uint8_t link_speed; /* 0 = 10M, 1 = 100M */
    uint8_t link_duplex; /* 0 = half, 1 = full */
    uint8_t mac_addr[6];

    /* Frame receive buffer (heap-allocated, shared by worker thread) */
    uint8_t* frame_buf;

    /* DHCP timer (1 second periodic for DHCP_time_handler) */
    FuriTimer* dhcp_timer;

    /* Cached DHCP results (for auto-populating scan ranges) */
    uint8_t dhcp_ip[4];
    uint8_t dhcp_mask[4];
    uint8_t dhcp_gw[4];
    uint8_t dhcp_dns[4];
    bool dhcp_valid;

    /* Custom ping target IP (parsed from user input) */
    uint8_t ping_ip_custom[4];
    char ping_ip_input[16]; /* text input buffer "xxx.xxx.xxx.xxx" */

    /* Packet statistics */
    PacketStats stats;

    /* DNS lookup state */
    char dns_hostname_input[64]; /* text input buffer for hostname */
    uint8_t dns_server_ip[4]; /* DNS server from DHCP */

    /* Wake-on-LAN state */
    uint8_t wol_mac_input[6]; /* byte input buffer for MAC */

    /* Continuous ping state */
    char cont_ping_ip_input[16]; /* text input buffer */
    uint8_t cont_ping_target[4]; /* parsed target IP */
    PingGraphState* ping_graph; /* heap-allocated ping graph state */

    /* Port scanner state */
    char port_scan_ip_input[16]; /* text input buffer */
    uint8_t port_scan_target[4]; /* parsed target IP */
    bool port_scan_top100; /* false=Top20, true=Top100 */
    bool port_scan_custom; /* true = custom range mode */
    uint16_t port_scan_custom_start; /* default 1 */
    uint16_t port_scan_custom_end; /* default 1024 */
    char port_scan_start_input[6]; /* text buffer "xxxxx" */
    char port_scan_end_input[6];

    /* MAC changer state */
    uint8_t mac_changer_input[6]; /* byte input buffer for custom MAC */

    /* Traceroute state */
    char traceroute_ip_input[16]; /* text input buffer (kept for compat) */
    uint8_t traceroute_target[4]; /* parsed target IP */
    char traceroute_host_input[64]; /* text input for IP or hostname */
    bool traceroute_is_hostname; /* true if input is hostname, not IP */

    /* Ping sweep state */
    char ping_sweep_ip_input[20]; /* "192.168.1.0/24" */

    /* ETH Bridge state */
    View* view_bridge;
    EthBridgeState* bridge_state;

    /* PXE Server state */
    TextBox* text_box_pxe_help;
    VariableItemList* pxe_settings_list;
    VariableItem* pxe_item_dhcp;
    VariableItem* pxe_item_sip;
    VariableItem* pxe_item_cip;
    VariableItem* pxe_item_sub;
    VariableItem* pxe_item_boot;

    /* PXE settings (user-configurable) */
    char pxe_server_ip_input[16]; /* "192.168.77.1" */
    char pxe_client_ip_input[16]; /* "192.168.77.10" */
    char pxe_subnet_input[16]; /* "255.255.255.0" */
    bool pxe_dhcp_enabled; /* true = run DHCP server */
    uint8_t pxe_server_ip[4]; /* parsed */
    uint8_t pxe_client_ip[4]; /* parsed */
    uint8_t pxe_subnet[4]; /* parsed */

    /* PXE boot file selection */
    PxeServerState pxe_scan; /* cached boot file scan results */
    uint8_t pxe_boot_file_idx; /* currently selected boot file */
    bool pxe_dhcp_probed; /* external DHCP already probed? */

    /* Discovered hosts from scans */
    Submenu* submenu_host_list;
    Submenu* submenu_host_actions;
    uint16_t discovered_host_count; /* lines in last_scan.txt */
    uint16_t selected_host_idx;
    uint16_t host_list_page; /* current page in Discovered Hosts (0-based) */

    /* Packet Capture state */
    View* view_packet_capture;
    PcapDumpState pcap_state;
    uint32_t pcap_start_tick;

    /* Auto Test UI */
    TextBox* text_box_autotest;
    FuriString* autotest_text;

    /* Auto Test runtime */
    volatile bool autotest_running;

    /* Auto Test settings */
    char autotest_dns_host[64];
    uint8_t autotest_lldp_wait_s;
    bool autotest_arp_enabled;

    /* Shared views for all tools (memory-efficient: 1 TextBox for all) */
    TextBox* text_box_tool;
    FuriString* tool_text;
    TextInput* text_input_tool;
    ByteInput* byte_input_tool;
    NumberInput* number_input_tool;
    LanTesterView tool_back_view; /* navigation target when pressing Back */

    /* Tool input buffers (small, always allocated) */
    uint8_t snmp_target[4];
    char snmp_ip_input[16];
    uint8_t ntp_target[4];
    char ntp_ip_input[16];
    uint32_t ntp_unix_time; /* last NTP result for clock sync (0 = none) */
    uint32_t ntp_query_tick; /* furi_get_tick() when NTP result was received */
    int8_t ntp_tz_hours; /* timezone offset hours (-12..+14) */
    int8_t ntp_tz_minutes; /* timezone offset minutes (0/15/30/45) */
    uint8_t netbios_target[4];
    char netbios_ip_input[16];
    char dns_poison_host_input[64];
    uint8_t tftp_target[4];
    char tftp_ip_input[16];
    char tftp_filename_input[64];
    uint8_t ipmi_target[4];
    char ipmi_ip_input[16];
    /* VLAN Hop state */
    bool vlan_hop_custom;
    char vlan_hop_input[32]; /* comma-separated VLAN IDs */

    /* Security category submenu */
    Submenu* submenu_cat_security;
};
