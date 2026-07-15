/*
 * Utilities category plugin (WOL, MAC Changer, Statistics, PXE Server/Download,
 * AutoTest, File Manager, TFTP, IPMI, NTP, NetBIOS). Protocols stay in the host
 * (ioLibrary); this plugin holds the tool orchestration.
 */

#include "../lan_tester_app.h"
#include "../lan_tester_plugin.h"
#include "../hal/w5500_hal.h"
#include "../utils/packet_utils.h"
#include "../utils/oui_lookup.h"
#include "../protocols/wol.h"
#include "../protocols/mac_changer.h"
#include "../protocols/pxe_server.h"
#include "../protocols/file_manager.h"
#include "../protocols/http_download.h"
#include "../protocols/tftp_client.h"
#include "../protocols/ipmi_client.h"
#include "../protocols/ntp_diag.h"
#include "../protocols/netbios_query.h"
#include "../protocols/lldp.h"
#include "../protocols/cdp.h"
#include "../protocols/arp_scan.h"
#include "../protocols/icmp.h"
#include "../protocols/dns_lookup.h"
#include "../api/lan_tester_ioshim.h"
#include <furi.h>
#include <furi_hal.h>
#include <flipper_application/flipper_application.h>

#define TAG "ETH"

static void lan_tester_do_wol(LanTesterApp* app);
static void lan_tester_do_mac_changer(LanTesterApp* app);
static void lan_tester_do_stats(LanTesterApp* app);
static void lan_tester_do_tftp_client(LanTesterApp* app);
static void lan_tester_do_ipmi_client(LanTesterApp* app);
static void lan_tester_do_ntp_diag(LanTesterApp* app);
static void lan_tester_do_netbios_query(LanTesterApp* app);

static void util_run(LanTesterApp* app, uint32_t op) {
    switch(op) {
    case LanTesterMenuItemWol:
        lan_tester_do_wol(app);
        break;
    case LanTesterMenuItemMacChanger:
        lan_tester_do_mac_changer(app);
        break;
    case LanTesterMenuItemStats:
        lan_tester_do_stats(app);
        break;
    case LanTesterMenuItemTftpClient:
        lan_tester_do_tftp_client(app);
        break;
    case LanTesterMenuItemIpmiClient:
        lan_tester_do_ipmi_client(app);
        break;
    case LanTesterMenuItemNtpDiag:
        lan_tester_do_ntp_diag(app);
        break;
    case LanTesterMenuItemNetbiosQuery:
        lan_tester_do_netbios_query(app);
        break;
    default:
        break;
    }
}

static const LanTesterCategoryPlugin util_plugin = {
    .name = "util",
    .run = util_run,
};

static const FlipperAppPluginDescriptor util_plugin_descriptor = {
    .appid = LAN_TESTER_PLUGIN_APP_ID,
    .ep_api_version = LAN_TESTER_PLUGIN_API_VERSION,
    .entry_point = &util_plugin,
};

const FlipperAppPluginDescriptor* lan_tester_util_plugin_ep(void) {
    return &util_plugin_descriptor;
}

/* ==================== moved from lan_tester_app.c ==================== */

static void lan_tester_do_wol(LanTesterApp* app) {
    furi_string_reset(app->tool_text);

    furi_string_printf(app->tool_text, "%s\n", lan_tester_net_acquire_msg(app));
    lan_tester_update_view(app->text_box_tool, app->tool_text);

    if(!lan_tester_check_dhcp(app)) return;

    char mac_str[18];
    pkt_format_mac(app->wol_mac_input, mac_str);

    furi_string_printf(app->tool_text, "[WoL] %s\n", mac_str);
    lan_tester_update_view(app->text_box_tool, app->tool_text);

    bool ok = wol_send(W5500_WOL_SOCKET, app->wol_mac_input);

    if(ok) {
        furi_string_printf(
            app->tool_text,
            "[Wake-on-LAN]\n"
            "Target: %s\n\n"
            "Magic packet sent!\n"
            "Press Back to return.\n",
            mac_str);
    } else {
        furi_string_printf(
            app->tool_text,
            "[Wake-on-LAN]\n"
            "Target: %s\n\n"
            "Failed to send!\n",
            mac_str);
    }
    if(app->setting_sound) {
        notification_message(app->notifications, ok ? &sequence_success : &sequence_error);
    }
}

static void lan_tester_do_mac_changer(LanTesterApp* app) {
    furi_string_reset(app->tool_text);

    /* Read current MAC */
    uint8_t current_mac[6];
    if(app->w5500_initialized) {
        w5500_hal_get_mac(current_mac);
    } else {
        memcpy(current_mac, app->mac_addr, 6);
    }

    uint8_t default_mac[6] = MAC_CHANGER_DEFAULT_MAC;
    bool is_default = (memcmp(current_mac, default_mac, 6) == 0);

    char mac_str[18];
    pkt_format_mac(current_mac, mac_str);

    furi_string_printf(
        app->tool_text,
        "Current MAC:\n"
        "%s %s\n\n"
        "OK = Randomize MAC\n"
        "Back = Cancel\n",
        mac_str,
        is_default ? "(default)" : "(custom)");
}

static void lan_tester_do_stats(LanTesterApp* app) {
    if(!lan_tester_check_w5500(app)) return;

    if(!w5500_hal_get_link_status()) {
        furi_string_set(app->tool_text, "No Link!\nConnect cable first.\n");
        return;
    }

    /* If no frames counted yet, do a quick capture */
    if(app->stats.total_frames == 0) {
        furi_string_set(app->tool_text, "Capturing frames...\n(10s remaining)\n");
        lan_tester_update_view(app->text_box_tool, app->tool_text);

        if(!w5500_hal_open_macraw()) {
            furi_string_set(app->tool_text, "Failed to open\nMACRAW!\n");
            return;
        }

        uint32_t start_tick = furi_get_tick();
        uint32_t last_sec = 0;
        while(furi_get_tick() - start_tick < 10000 && app->worker_running) {
            uint16_t recv_len = w5500_hal_macraw_recv(app->frame_buf, FRAME_BUF_SIZE);
            if(recv_len >= ETH_HEADER_SIZE) {
                lan_tester_count_frame(app, app->frame_buf, recv_len);
            }
            /* Update countdown every second */
            uint32_t sec = (furi_get_tick() - start_tick) / 1000;
            if(sec != last_sec) {
                last_sec = sec;
                furi_string_printf(
                    app->tool_text,
                    "Capturing frames...\n(%lus remaining)\nFrames: %lu\n",
                    (unsigned long)(10 - sec),
                    (unsigned long)app->stats.total_frames);
                lan_tester_update_view(app->text_box_tool, app->tool_text);
            }
            furi_delay_ms(10);
        }

        w5500_hal_close_macraw();
    }

    /* Format statistics with compact layout */
    PacketStats* s = &app->stats;
    uint32_t t = s->total_frames ? s->total_frames : 1; /* avoid div by 0 */
    furi_string_printf(
        app->tool_text,
        "[Stats] %lu frames\n"
        "Uni:%lu Bcast:%lu Mcast:%lu\n"
        "\nIPv4:%lu(%lu%%) ARP:%lu\n"
        "IPv6:%lu LLDP:%lu CDP:%lu\n"
        "Other:%lu\n",
        (unsigned long)s->total_frames,
        (unsigned long)s->unicast_frames,
        (unsigned long)s->broadcast_frames,
        (unsigned long)s->multicast_frames,
        (unsigned long)s->ipv4_frames,
        (unsigned long)(s->ipv4_frames * 100 / t),
        (unsigned long)s->arp_frames,
        (unsigned long)s->ipv6_frames,
        (unsigned long)s->lldp_frames,
        (unsigned long)s->cdp_frames,
        (unsigned long)s->unknown_frames);

    /* Save stats to SD card (no sound — passive capture) */
    if(app->setting_autosave) {
        lan_tester_save_results("stats.txt", furi_string_get_cstr(app->tool_text));
    }
}

static void lan_tester_do_tftp_client(LanTesterApp* app) {
    if(!lan_tester_check_w5500(app)) return;

    char ip_str[16];
    snprintf(
        ip_str,
        sizeof(ip_str),
        "%d.%d.%d.%d",
        app->tftp_target[0],
        app->tftp_target[1],
        app->tftp_target[2],
        app->tftp_target[3]);

    furi_string_cat(app->tool_text, "[TFTP] ");
    furi_string_cat_printf(app->tool_text, "Server: %s\n", ip_str);
    furi_string_cat_printf(app->tool_text, "File: %s\n", app->tftp_filename_input);
    furi_string_cat(app->tool_text, "Downloading...\n");
    lan_tester_update_view(app->text_box_tool, app->tool_text);

    /* Static to avoid 128B stack usage; worker is single-threaded */
    static char save_path[128];
    snprintf(save_path, sizeof(save_path), APP_DATA_PATH("tftp/%s"), app->tftp_filename_input);

    TftpClientResult result;
    tftp_client_get(
        app->tftp_target, app->tftp_filename_input, save_path, &result, &app->worker_running);

    if(result.success) {
        furi_string_cat_printf(
            app->tool_text,
            "\nSuccess!\n%lu bytes, %d blocks\n",
            (unsigned long)result.bytes_received,
            result.blocks_received);
        if(result.saved_to_sd) {
            furi_string_cat_printf(app->tool_text, "-> %s\n", result.save_path);
        }
    } else {
        furi_string_cat_printf(app->tool_text, "\nFailed: %s\n", result.error_msg);
        if(result.bytes_received > 0) {
            furi_string_cat_printf(
                app->tool_text, "Partial: %lu bytes\n", (unsigned long)result.bytes_received);
        }
    }

    lan_tester_save_and_notify(app, "tftp.txt", app->tool_text);
}

static void lan_tester_do_ipmi_client(LanTesterApp* app) {
    if(!lan_tester_check_w5500(app)) return;

    char ip_str[16];
    snprintf(
        ip_str,
        sizeof(ip_str),
        "%d.%d.%d.%d",
        app->ipmi_target[0],
        app->ipmi_target[1],
        app->ipmi_target[2],
        app->ipmi_target[3]);

    furi_string_cat_printf(app->tool_text, "[IPMI] %s\n", ip_str);
    lan_tester_update_view(app->text_box_tool, app->tool_text);

    IpmiResult result;
    ipmi_query(app->ipmi_target, &result);

    if(!result.valid) {
        furi_string_cat_printf(app->tool_text, "%s\n", result.error_msg);
        furi_string_cat(app->tool_text, "Check BMC IP and\nnetwork connectivity.\n");
        return;
    }

    if(result.chassis_ok) {
        furi_string_cat(app->tool_text, "== Chassis Status ==\n");
        furi_string_cat_printf(
            app->tool_text,
            "Power: %s\n",
            (result.power_state & IPMI_CHASSIS_POWER_ON) ? "ON" : "OFF");
        if(result.power_state & IPMI_CHASSIS_OVERLOAD)
            furi_string_cat(app->tool_text, "Overload detected!\n");
        if(result.power_state & IPMI_CHASSIS_FAULT)
            furi_string_cat(app->tool_text, "Power fault!\n");

        const char* policy = "Unknown";
        uint8_t pol = (result.power_state & IPMI_CHASSIS_POWER_POLICY) >> 5;
        if(pol == 0)
            policy = "Stay off";
        else if(pol == 1)
            policy = "Restore prev";
        else if(pol == 2)
            policy = "Always on";
        furi_string_cat_printf(app->tool_text, "Policy: %s\n", policy);
    }

    if(result.device_ok) {
        furi_string_cat(app->tool_text, "== Device Info ==\n");
        furi_string_cat_printf(app->tool_text, "Device ID: 0x%02X\n", result.device_id);
        furi_string_cat_printf(app->tool_text, "Revision: %d\n", result.device_revision);
        furi_string_cat_printf(
            app->tool_text, "Firmware: %d.%02d\n", result.firmware_major, result.firmware_minor);
        furi_string_cat_printf(
            app->tool_text,
            "IPMI ver: %d.%d\n",
            result.ipmi_version >> 4,
            result.ipmi_version & 0x0F);
    }

    lan_tester_save_and_notify(app, "ipmi.txt", app->tool_text);
}

static void lan_tester_do_ntp_diag(LanTesterApp* app) {
    if(!lan_tester_check_w5500(app)) return;

    char ip_str[16];
    snprintf(
        ip_str,
        sizeof(ip_str),
        "%d.%d.%d.%d",
        app->ntp_target[0],
        app->ntp_target[1],
        app->ntp_target[2],
        app->ntp_target[3]);
    furi_string_cat_printf(app->tool_text, "[NTP] %s\n", ip_str);
    lan_tester_update_view(app->text_box_tool, app->tool_text);

    NtpDiagResult result;
    if(!ntp_diag_query(app->ntp_target, &result)) {
        furi_string_cat(app->tool_text, "No NTP response.\nCheck server IP.\n");
        return;
    }

    furi_string_cat_printf(
        app->tool_text, "Stratum: %d (%s)\n", result.stratum, result.stratum_name);

    const char* leap_str = "none";
    if(result.leap == 1)
        leap_str = "+1 sec";
    else if(result.leap == 2)
        leap_str = "-1 sec";
    else if(result.leap == 3)
        leap_str = "unsync";
    furi_string_cat_printf(app->tool_text, "Leap: %s\n", leap_str);

    furi_string_cat_printf(app->tool_text, "Version: NTPv%d\n", result.version);

    if(result.stratum <= 1) {
        furi_string_cat_printf(app->tool_text, "Ref ID: %s\n", result.ref_id_str);
    } else {
        furi_string_cat_printf(app->tool_text, "Ref Clock: %s\n", result.ref_id_str);
    }

    uint32_t root_delay_us =
        (result.root_delay >> 16) * 1000000 + ((result.root_delay & 0xFFFF) * 1000000 / 65536);
    uint32_t root_disp_us =
        (result.root_disp >> 16) * 1000000 + ((result.root_disp & 0xFFFF) * 1000000 / 65536);

    furi_string_cat_printf(
        app->tool_text,
        "Delay:%lu Disp:%lu us\n",
        (unsigned long)root_delay_us,
        (unsigned long)root_disp_us);
    furi_string_cat_printf(
        app->tool_text, "RTT:%lu us Prec:2^%d\n", (unsigned long)result.rtt_us, result.precision);

    if(result.unix_time) {
        DateTime ntp_dt;
        datetime_timestamp_to_datetime(result.unix_time, &ntp_dt);
        furi_string_cat_printf(
            app->tool_text,
            "Time: %04d-%02d-%02d %02d:%02d:%02d\n",
            ntp_dt.year,
            ntp_dt.month,
            ntp_dt.day,
            ntp_dt.hour,
            ntp_dt.minute,
            ntp_dt.second);

        DateTime flip_dt;
        furi_hal_rtc_get_datetime(&flip_dt);
        int32_t diff =
            (int32_t)result.unix_time - (int32_t)datetime_datetime_to_timestamp(&flip_dt);
        furi_string_cat_printf(app->tool_text, "Diff: %+ld sec\n", (long)diff);

        /* Store for NTP Sync */
        app->ntp_unix_time = result.unix_time;
        app->ntp_query_tick = furi_get_tick();
    }

    lan_tester_save_and_notify(app, "ntp.txt", app->tool_text);
}

static void lan_tester_do_netbios_query(LanTesterApp* app) {
    if(!lan_tester_check_w5500(app)) return;

    char ip_str[16];
    snprintf(
        ip_str,
        sizeof(ip_str),
        "%d.%d.%d.%d",
        app->netbios_target[0],
        app->netbios_target[1],
        app->netbios_target[2],
        app->netbios_target[3]);
    furi_string_cat_printf(app->tool_text, "[NetBIOS] %s\n", ip_str);
    lan_tester_update_view(app->text_box_tool, app->tool_text);

    NetbiosQueryResult result;
    if(!netbios_node_status(app->netbios_target, &result)) {
        furi_string_cat(app->tool_text, "No NetBIOS response.\nHost may not run SMB/CIFS.\n");
        return;
    }

    if(result.computer_name[0]) {
        furi_string_cat_printf(app->tool_text, "Computer: %s\n", result.computer_name);
    }
    if(result.workgroup[0]) {
        furi_string_cat_printf(app->tool_text, "Workgroup: %s\n", result.workgroup);
    }
    if(result.has_unit_id) {
        furi_string_cat_printf(
            app->tool_text,
            "MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
            result.unit_id[0],
            result.unit_id[1],
            result.unit_id[2],
            result.unit_id[3],
            result.unit_id[4],
            result.unit_id[5]);
    }

    furi_string_cat_printf(app->tool_text, "Names(%d):\n", result.name_count);
    for(uint8_t i = 0; i < result.name_count; i++) {
        NetbiosName* n = &result.names[i];
        furi_string_cat_printf(
            app->tool_text,
            "  %-15s <%02X> %s\n",
            n->name,
            n->suffix,
            n->is_group ? "GROUP" : "UNIQUE");
    }

    lan_tester_save_and_notify(app, "netbios.txt", app->tool_text);
}
