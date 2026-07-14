#include "lan_tester_app.h"
#include "hal/w5500_hal.h"
#include "protocols/lldp.h"
#include "protocols/cdp.h"
#include "protocols/arp_scan.h"
#include "protocols/dhcp_discover.h"
#include "protocols/icmp.h"
#include "protocols/dns_lookup.h"
#include "protocols/wol.h"
#include "protocols/ping_graph.h"
#include "protocols/port_scan.h"
#include "protocols/mac_changer.h"
#include "protocols/traceroute.h"
#include "protocols/discovery.h"
#include "protocols/stp_vlan.h"
#include "protocols/history.h"
#include "protocols/pxe_server.h"
#include "protocols/file_manager.h"
#include "protocols/snmp_client.h"
#include "protocols/ntp_diag.h"
#include "protocols/netbios_query.h"
#include "protocols/dns_poison.h"
#include "protocols/arp_watch.h"
#include "protocols/rogue_dhcp.h"
#include "protocols/rogue_ra.h"
#include "protocols/dhcp_fingerprint.h"
#include "protocols/eapol_probe.h"
#include "protocols/vlan_hop.h"
#include "protocols/tftp_client.h"
#include "protocols/ipmi_client.h"
#include "protocols/http_download.h"
#include "bridge/eth_bridge.h"
#include "usb_eth/usb_eth.h"
#include "utils/packet_utils.h"
#include "utils/oui_lookup.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_random.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>

#include <socket.h>
#include <dhcp.h>
#include <wizchip_conf.h>

#include <string.h>
#include <stdio.h>

#define TAG "ETH"

/* Internal worker operations (beyond LanTesterMenuItem range) */
#define WORKER_OP_PING_SWEEP_DETECT 100

/* Custom events sent from worker to main thread */
#define CUSTOM_EVENT_PING_SWEEP_READY 1
#define CUSTOM_EVENT_HISTORY_DELETE   2
#define CUSTOM_EVENT_CONT_PING_BACK   3
#define CUSTOM_EVENT_SHOW_HOST_LIST   4

/* File-backed discovered hosts (replaces in-memory array) */
#define SCAN_RESULTS_PATH APP_DATA_PATH("last_scan.txt")

/* Pagination for host list submenu */
#define HOST_LIST_PAGE_SIZE 24
#define HOST_LIST_IDX_PREV  0xFFFE
#define HOST_LIST_IDX_NEXT  0xFFFF

/** Clear scan results file. */
static void scan_results_clear(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_remove(storage, SCAN_RESULTS_PATH);
    furi_record_close(RECORD_STORAGE);
}

/* Persistent scan results writer — opened once, closed after scan */
static Storage* scan_storage = NULL;
static File* scan_file = NULL;

static bool scan_results_open_writer(void) {
    scan_storage = furi_record_open(RECORD_STORAGE);
    scan_file = storage_file_alloc(scan_storage);
    if(!storage_file_open(scan_file, SCAN_RESULTS_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        storage_file_free(scan_file);
        scan_file = NULL;
        furi_record_close(RECORD_STORAGE);
        scan_storage = NULL;
        return false;
    }
    return true;
}

static void scan_results_close_writer(void) {
    if(scan_file) {
        storage_file_close(scan_file);
        storage_file_free(scan_file);
        scan_file = NULL;
    }
    if(scan_storage) {
        furi_record_close(RECORD_STORAGE);
        scan_storage = NULL;
    }
}

/** Append one host. Call between open_writer/close_writer. */
static void scan_results_add(const uint8_t ip[4], const uint8_t* mac) {
    if(!scan_file) return;
    char line[36];
    int len;
    if(mac) {
        len = snprintf(
            line,
            sizeof(line),
            "%d.%d.%d.%d,%02X:%02X:%02X:%02X:%02X:%02X\n",
            ip[0],
            ip[1],
            ip[2],
            ip[3],
            mac[0],
            mac[1],
            mac[2],
            mac[3],
            mac[4],
            mac[5]);
    } else {
        len = snprintf(line, sizeof(line), "%d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
    }
    storage_file_write(scan_file, line, (uint16_t)len);
}

/** Read host at index from scan results. Returns false if index out of range. */
static bool
    scan_results_read(uint16_t index, uint8_t ip_out[4], uint8_t mac_out[6], bool* has_mac) {
    bool found = false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, SCAN_RESULTS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line[36];
        uint16_t line_idx = 0;
        uint16_t pos = 0;
        char ch;
        while(storage_file_read(f, &ch, 1) == 1) {
            if(ch == '\n') {
                if(line_idx == index) {
                    line[pos] = '\0';
                    /* Parse IP */
                    unsigned a, b, c, d;
                    if(sscanf(line, "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
                        ip_out[0] = a;
                        ip_out[1] = b;
                        ip_out[2] = c;
                        ip_out[3] = d;
                        /* Check for MAC after comma */
                        char* comma = strchr(line, ',');
                        if(comma && mac_out) {
                            unsigned m[6];
                            if(sscanf(
                                   comma + 1,
                                   "%02X:%02X:%02X:%02X:%02X:%02X",
                                   &m[0],
                                   &m[1],
                                   &m[2],
                                   &m[3],
                                   &m[4],
                                   &m[5]) == 6) {
                                for(int i = 0; i < 6; i++)
                                    mac_out[i] = (uint8_t)m[i];
                                if(has_mac) *has_mac = true;
                            } else {
                                if(has_mac) *has_mac = false;
                            }
                        } else {
                            if(has_mac) *has_mac = false;
                        }
                        found = true;
                    }
                    break;
                }
                line_idx++;
                pos = 0;
            } else if(pos < sizeof(line) - 1) {
                line[pos++] = ch;
            }
        }
    }
    storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
    return found;
}

/* Global app pointer for navigation callbacks (single-instance app) */
static LanTesterApp* g_app = NULL;

/* ==================== WIZnet library compatibility stubs ==================== */

/*
 * eth_printf: called by WIZnet DHCP/ICMP library for debug output.
 * We forward it to Flipper's FURI_LOG system.
 */
void eth_printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    FuriString* fstr = furi_string_alloc_vprintf(format, args);
    va_end(args);
    FURI_LOG_D(TAG, "%s", furi_string_get_cstr(fstr));
    furi_string_free(fstr);
}

/*
 * ping_wait_ms: called by WIZnet ping/traceroute library.
 */
void ping_wait_ms(int ms) {
    furi_delay_ms(ms);
}

/*
 * DHCP timer callback for FuriTimer (1 second periodic).
 */
static void dhcp_timer_callback(void* context) {
    UNUSED(context);
    DHCP_time_handler();
}

/*
 * Generate a default MAC from the device's unique hardware name/id.
 * Uses WIZnet OUI (00:08:DC) + 3 bytes derived from furi_hal_random,
 * seeded implicitly by hardware RNG. Generated once and saved to SD.
 * If no saved MAC exists, a fresh one is created.
 */
static void lan_tester_generate_default_mac(uint8_t mac[6]) {
    mac[0] = 0x00;
    mac[1] = 0x08;
    mac[2] = 0xDC;
    /* Generate unique lower 3 bytes from hardware RNG */
    furi_hal_random_fill_buf(mac + 3, 3);
}

/* Frame receive buffer size */
#define FRAME_BUF_SIZE 1600

/* Settings file path */
#define SETTINGS_PATH APP_DATA_PATH("settings.conf")

/* ==================== Settings persistence ==================== */

static bool lan_tester_parse_ip(const char* str, uint8_t ip[4]);

/* Helper: extract string value for "key=" from buffer into dst (max dst_sz-1 chars).
 * Matches only at line start (beginning of buffer or after \n) to avoid
 * "ping_ip=" matching inside "cont_ping_ip=". */
static bool settings_parse_str(const char* buf, const char* key, char* dst, size_t dst_sz) {
    size_t klen = strlen(key);
    const char* p = buf;
    while((p = strstr(p, key)) != NULL) {
        if(p == buf || *(p - 1) == '\n') {
            p += klen;
            int j = 0;
            while(p[j] && p[j] != '\n' && j < (int)(dst_sz - 1)) {
                dst[j] = p[j];
                j++;
            }
            dst[j] = '\0';
            return j > 0;
        }
        p += klen;
    }
    return false;
}

/* Helper: extract IP value for "key=" from buffer into ip[4], also copy text to txt */
static bool
    settings_parse_ip(const char* buf, const char* key, uint8_t ip[4], char* txt, size_t txt_sz) {
    char tmp[16];
    if(!settings_parse_str(buf, key, tmp, sizeof(tmp))) return false;
    if(!lan_tester_parse_ip(tmp, ip)) return false;
    strncpy(txt, tmp, txt_sz);
    txt[txt_sz - 1] = '\0';
    return true;
}

static void lan_tester_settings_load(LanTesterApp* app) {
    /* Defaults — general settings */
    app->setting_autosave = true;
    app->setting_sound = true;
    app->dns_custom_enabled = false;
    app->dns_custom_server[0] = 8;
    app->dns_custom_server[1] = 8;
    app->dns_custom_server[2] = 8;
    app->dns_custom_server[3] = 8;
    strncpy(app->dns_custom_ip_input, "8.8.8.8", sizeof(app->dns_custom_ip_input));
    app->ping_count = 4;
    app->ping_timeout_ms = 3000;
    app->ping_interval_ms = 1000;
    strncpy(app->autotest_dns_host, "google.com", sizeof(app->autotest_dns_host));
    app->autotest_lldp_wait_s = 30;
    app->autotest_arp_enabled = true;

    /* Defaults — tool targets */
    strncpy(app->ping_ip_input, "8.8.8.8", sizeof(app->ping_ip_input));
    strncpy(app->cont_ping_ip_input, "8.8.8.8", sizeof(app->cont_ping_ip_input));
    strncpy(app->dns_hostname_input, "google.com", sizeof(app->dns_hostname_input));
    strncpy(app->traceroute_ip_input, "8.8.8.8", sizeof(app->traceroute_ip_input));
    strncpy(app->traceroute_host_input, "8.8.8.8", sizeof(app->traceroute_host_input));
    app->port_scan_ip_input[0] = '\0';
    app->ping_sweep_ip_input[0] = '\0';
    strncpy(app->snmp_ip_input, "192.168.1.1", sizeof(app->snmp_ip_input));
    strncpy(app->ntp_ip_input, "192.168.1.1", sizeof(app->ntp_ip_input));
    app->ntp_tz_hours = 0;
    app->ntp_tz_minutes = 0;
    strncpy(app->netbios_ip_input, "192.168.1.1", sizeof(app->netbios_ip_input));
    strncpy(app->dns_poison_host_input, "google.com", sizeof(app->dns_poison_host_input));
    strncpy(app->tftp_ip_input, "192.168.1.1", sizeof(app->tftp_ip_input));
    strncpy(app->tftp_filename_input, "config.cfg", sizeof(app->tftp_filename_input));
    strncpy(app->ipmi_ip_input, "192.168.1.1", sizeof(app->ipmi_ip_input));
    strncpy(app->vlan_hop_input, "1,10,20,50,100", sizeof(app->vlan_hop_input));

    /* Defaults — PXE */
    strncpy(app->pxe_server_ip_input, "192.168.77.1", sizeof(app->pxe_server_ip_input));
    strncpy(app->pxe_client_ip_input, "192.168.77.10", sizeof(app->pxe_client_ip_input));
    strncpy(app->pxe_subnet_input, "255.255.255.0", sizeof(app->pxe_subnet_input));

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char* buf = malloc(768);
        if(!buf) {
            storage_file_close(file);
            storage_file_free(file);
            furi_record_close(RECORD_STORAGE);
            return;
        }
        uint16_t read = storage_file_read(file, buf, 767);
        buf[read] = '\0';
        storage_file_close(file);

        /* General settings */
        if(strstr(buf, "autosave=0")) app->setting_autosave = false;
        if(strstr(buf, "sound=0")) app->setting_sound = false;
        if(strstr(buf, "dns_custom=1")) app->dns_custom_enabled = true;
        settings_parse_ip(
            buf,
            "dns_ip=",
            app->dns_custom_server,
            app->dns_custom_ip_input,
            sizeof(app->dns_custom_ip_input));
        char* pc = strstr(buf, "ping_count=");
        if(pc) {
            int val = atoi(pc + 11);
            if(val >= 1 && val <= 100) app->ping_count = (uint8_t)val;
        }
        char* pt = strstr(buf, "ping_timeout=");
        if(pt) {
            int val = atoi(pt + 13);
            if(val >= 500 && val <= 10000) app->ping_timeout_ms = (uint16_t)val;
        }
        char* pi = strstr(buf, "ping_interval=");
        if(pi) {
            int val = atoi(pi + 14);
            if(val >= 200 && val <= 5000) app->ping_interval_ms = (uint16_t)val;
        }
        settings_parse_str(
            buf, "autotest_dns=", app->autotest_dns_host, sizeof(app->autotest_dns_host));
        char* at_lldp = strstr(buf, "autotest_lldp_wait=");
        if(at_lldp) {
            int val = atoi(at_lldp + 19);
            if(val >= 10 && val <= 60) app->autotest_lldp_wait_s = (uint8_t)val;
        }
        if(strstr(buf, "autotest_arp=0")) app->autotest_arp_enabled = false;

        /* MAC address */
        char mac_str[18];
        if(settings_parse_str(buf, "mac=", mac_str, sizeof(mac_str))) {
            unsigned int m[6];
            if(sscanf(
                   mac_str,
                   "%02X:%02X:%02X:%02X:%02X:%02X",
                   &m[0],
                   &m[1],
                   &m[2],
                   &m[3],
                   &m[4],
                   &m[5]) == 6) {
                for(int i = 0; i < 6; i++)
                    app->mac_addr[i] = (uint8_t)m[i];
            }
        }

        /* Tool targets */
        settings_parse_str(buf, "ping_ip=", app->ping_ip_input, sizeof(app->ping_ip_input));
        settings_parse_str(
            buf, "cont_ping_ip=", app->cont_ping_ip_input, sizeof(app->cont_ping_ip_input));
        settings_parse_str(
            buf, "dns_host=", app->dns_hostname_input, sizeof(app->dns_hostname_input));
        settings_parse_str(
            buf,
            "traceroute_host=",
            app->traceroute_host_input,
            sizeof(app->traceroute_host_input));
        /* Keep traceroute_ip_input in sync for compat */
        strncpy(
            app->traceroute_ip_input,
            app->traceroute_host_input,
            sizeof(app->traceroute_ip_input));
        settings_parse_str(
            buf, "port_scan_ip=", app->port_scan_ip_input, sizeof(app->port_scan_ip_input));
        settings_parse_str(
            buf, "ping_sweep=", app->ping_sweep_ip_input, sizeof(app->ping_sweep_ip_input));
        settings_parse_str(buf, "snmp_ip=", app->snmp_ip_input, sizeof(app->snmp_ip_input));
        settings_parse_str(buf, "ntp_ip=", app->ntp_ip_input, sizeof(app->ntp_ip_input));
        char tz_tmp[8];
        if(settings_parse_str(buf, "ntp_tz_h=", tz_tmp, sizeof(tz_tmp))) {
            int v = atoi(tz_tmp);
            if(v >= -12 && v <= 14) app->ntp_tz_hours = (int8_t)v;
        }
        if(settings_parse_str(buf, "ntp_tz_m=", tz_tmp, sizeof(tz_tmp))) {
            int v = atoi(tz_tmp);
            if(v >= 0 && v <= 45) app->ntp_tz_minutes = (int8_t)v;
        }
        settings_parse_str(
            buf, "netbios_ip=", app->netbios_ip_input, sizeof(app->netbios_ip_input));
        settings_parse_str(
            buf,
            "dns_poison_host=",
            app->dns_poison_host_input,
            sizeof(app->dns_poison_host_input));
        settings_parse_str(buf, "tftp_ip=", app->tftp_ip_input, sizeof(app->tftp_ip_input));
        settings_parse_str(
            buf, "tftp_file=", app->tftp_filename_input, sizeof(app->tftp_filename_input));
        settings_parse_str(buf, "ipmi_ip=", app->ipmi_ip_input, sizeof(app->ipmi_ip_input));
        settings_parse_str(buf, "vlan_hop=", app->vlan_hop_input, sizeof(app->vlan_hop_input));

        /* PXE settings */
        settings_parse_str(
            buf, "pxe_server_ip=", app->pxe_server_ip_input, sizeof(app->pxe_server_ip_input));
        settings_parse_str(
            buf, "pxe_client_ip=", app->pxe_client_ip_input, sizeof(app->pxe_client_ip_input));
        settings_parse_str(
            buf, "pxe_subnet=", app->pxe_subnet_input, sizeof(app->pxe_subnet_input));

        free(buf);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    /* Parse PXE IP arrays from loaded text */
    lan_tester_parse_ip(app->pxe_server_ip_input, app->pxe_server_ip);
    lan_tester_parse_ip(app->pxe_client_ip_input, app->pxe_client_ip);
    lan_tester_parse_ip(app->pxe_subnet_input, app->pxe_subnet);
}

static void lan_tester_settings_save(LanTesterApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, APP_DATA_PATH(""));
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char* buf = malloc(768);
        if(!buf) {
            storage_file_close(file);
            storage_file_free(file);
            furi_record_close(RECORD_STORAGE);
            return;
        }
        int len = snprintf(
            buf,
            768,
            "autosave=%d\nsound=%d\ndns_custom=%d\ndns_ip=%s\n"
            "ping_count=%d\nping_timeout=%d\nping_interval=%d\n"
            "autotest_dns=%s\nautotest_lldp_wait=%d\nautotest_arp=%d\n"
            "mac=%02X:%02X:%02X:%02X:%02X:%02X\n"
            "ping_ip=%s\ncont_ping_ip=%s\ndns_host=%s\n"
            "traceroute_host=%s\nport_scan_ip=%s\nping_sweep=%s\n"
            "snmp_ip=%s\nntp_ip=%s\nntp_tz_h=%d\nntp_tz_m=%d\nnetbios_ip=%s\n"
            "dns_poison_host=%s\ntftp_ip=%s\ntftp_file=%s\n"
            "ipmi_ip=%s\nvlan_hop=%s\n"
            "pxe_server_ip=%s\npxe_client_ip=%s\npxe_subnet=%s\n",
            app->setting_autosave ? 1 : 0,
            app->setting_sound ? 1 : 0,
            app->dns_custom_enabled ? 1 : 0,
            app->dns_custom_ip_input,
            app->ping_count,
            app->ping_timeout_ms,
            app->ping_interval_ms,
            app->autotest_dns_host,
            app->autotest_lldp_wait_s,
            app->autotest_arp_enabled ? 1 : 0,
            app->mac_addr[0],
            app->mac_addr[1],
            app->mac_addr[2],
            app->mac_addr[3],
            app->mac_addr[4],
            app->mac_addr[5],
            app->ping_ip_input,
            app->cont_ping_ip_input,
            app->dns_hostname_input,
            app->traceroute_host_input,
            app->port_scan_ip_input,
            app->ping_sweep_ip_input,
            app->snmp_ip_input,
            app->ntp_ip_input,
            app->ntp_tz_hours,
            app->ntp_tz_minutes,
            app->netbios_ip_input,
            app->dns_poison_host_input,
            app->tftp_ip_input,
            app->tftp_filename_input,
            app->ipmi_ip_input,
            app->vlan_hop_input,
            app->pxe_server_ip_input,
            app->pxe_client_ip_input,
            app->pxe_subnet_input);
        storage_file_write(file, buf, len);
        storage_file_close(file);
        free(buf);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

/* ==================== Forward declarations ==================== */

static void lan_tester_submenu_callback(void* context, uint32_t index);
static uint32_t lan_tester_navigation_exit_callback(void* context);
static uint32_t lan_tester_navigation_submenu_callback(void* context);
static uint32_t lan_tester_nav_back_autotest(void* context);
static uint32_t lan_tester_nav_back_portinfo(void* context);
static uint32_t lan_tester_nav_back_scan(void* context);
static uint32_t lan_tester_nav_back_diag(void* context);
static uint32_t lan_tester_nav_back_traffic(void* context);
static uint32_t lan_tester_nav_back_utilities(void* context);
static uint32_t lan_tester_nav_back_port_scan_mode(void* context);
static uint32_t lan_tester_nav_back_settings(void* context);
static uint32_t lan_tester_nav_back_host_list(void* context);
static uint32_t lan_tester_nav_back_host_actions(void* context);
static bool lan_tester_nav_event_cb(void* context);
static bool lan_tester_custom_event_cb(void* context, uint32_t event);
static int32_t lan_tester_worker_fn(void* context);
static void lan_tester_worker_stop(LanTesterApp* app);
static void lan_tester_worker_start(LanTesterApp* app, uint32_t op, LanTesterView result_view);
static void lan_tester_update_view(TextBox* tb, FuriString* text);
static void lan_tester_show_view(
    LanTesterApp* app,
    TextBox* tb,
    LanTesterView view,
    FuriString* text,
    const char* initial);
static bool lan_tester_ensure_w5500(LanTesterApp* app);
static bool lan_tester_check_w5500(LanTesterApp* app);
static bool lan_tester_check_dhcp(LanTesterApp* app);

static void lan_tester_do_link_info(LanTesterApp* app);
static void lan_tester_do_lldp_cdp(LanTesterApp* app);
static void lan_tester_do_arp_scan(LanTesterApp* app);
static void lan_tester_do_dhcp_analyze(LanTesterApp* app);
static void lan_tester_do_ping(LanTesterApp* app);
static void lan_tester_do_stats(LanTesterApp* app);
static void lan_tester_do_dns_lookup(LanTesterApp* app);
static void lan_tester_do_wol(LanTesterApp* app);
static void lan_tester_do_cont_ping(LanTesterApp* app);
static void lan_tester_do_port_scan(LanTesterApp* app);
static void lan_tester_do_mac_changer(LanTesterApp* app);
static void lan_tester_do_traceroute(LanTesterApp* app);
static void lan_tester_do_discovery(LanTesterApp* app);
static void lan_tester_do_ping_sweep(LanTesterApp* app);
static void lan_tester_do_ping_sweep_detect(LanTesterApp* app);
static void lan_tester_do_stp_vlan(LanTesterApp* app);
static void lan_tester_do_eth_bridge(LanTesterApp* app);
static void lan_tester_do_pxe_server(LanTesterApp* app);
static void lan_tester_do_file_manager(LanTesterApp* app);
static void lan_tester_do_packet_capture(LanTesterApp* app);
static void lan_tester_do_autotest(LanTesterApp* app);
static void lan_tester_do_snmp_get(LanTesterApp* app);
static void lan_tester_do_ntp_diag(LanTesterApp* app);
static void lan_tester_do_netbios_query(LanTesterApp* app);
static void lan_tester_do_dns_poison_check(LanTesterApp* app);
static void lan_tester_do_arp_watch(LanTesterApp* app);
static void lan_tester_do_rogue_dhcp(LanTesterApp* app);
static void lan_tester_do_rogue_ra(LanTesterApp* app);
static void lan_tester_do_dhcp_fingerprint(LanTesterApp* app);
static void lan_tester_do_eapol_probe(LanTesterApp* app);
static void lan_tester_do_vlan_hop(LanTesterApp* app);
static void lan_tester_do_tftp_client(LanTesterApp* app);
static void lan_tester_do_ipmi_client(LanTesterApp* app);
static void lan_tester_do_pxe_download(LanTesterApp* app);
static uint32_t lan_tester_nav_back_tool(void* context);
static void lan_tester_history_populate(LanTesterApp* app);
static void lan_tester_populate_host_list(LanTesterApp* app);
static void lan_tester_port_scan_start_callback(void* context);
static void lan_tester_history_file_callback(void* context, uint32_t index);
static void lan_tester_mac_changer_input_callback(void* context);
static void lan_tester_ntp_sync_hours_callback(void* context, int32_t number);
static void lan_tester_ntp_sync_minutes_callback(void* context, int32_t number);
static void lan_tester_stop_worker_on_back(void);
static void lan_tester_count_frame(LanTesterApp* app, const uint8_t* frame, uint16_t len);
static bool lan_tester_save_results(const char* filename, const char* content);
static void lan_tester_save_and_notify(LanTesterApp* app, const char* type, FuriString* text);

/* ==================== ETH Bridge view model & callbacks ==================== */

typedef struct {
    LanTesterApp* app;
    bool active; /* bridge is running */
    bool usb_connected;
    bool lan_link_up;
    uint8_t lan_speed; /* 0=10M, 1=100M */
    uint8_t lan_duplex; /* 0=half, 1=full */
    uint32_t frames_to_eth;
    uint32_t frames_to_usb;
    uint32_t errors;
    const char* status_line; /* "Starting...", "Running", "Stopped" */
    bool dump_active; /* PCAP dump is recording */
    uint32_t dump_frames; /* frames written to pcap */
    uint32_t dump_dropped; /* frames dropped (write errors) */
} BridgeViewModel;

static void bridge_draw_callback(Canvas* canvas, void* model) {
    BridgeViewModel* vm = model;
    canvas_clear(canvas);

    /* Title */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "ETH Bridge");

    canvas_set_font(canvas, FontSecondary);
    char buf[40];

    if(!vm->active) {
        /* Show status message when not active (init/error/stopped) */
        canvas_draw_str(canvas, 2, 24, vm->status_line ? vm->status_line : "");
        return;
    }

    /* USB status */
    snprintf(buf, sizeof(buf), "USB: %s", vm->usb_connected ? "Connected" : "Waiting...");
    canvas_draw_str(canvas, 2, 16, buf);

    /* LAN status */
    snprintf(
        buf,
        sizeof(buf),
        "LAN: %s %s/%s",
        vm->lan_link_up ? "Up" : "Down",
        vm->lan_speed ? "100M" : "10M",
        vm->lan_duplex ? "FD" : "HD");
    canvas_draw_str(canvas, 2, 26, buf);

    /* Frame counters */
    snprintf(buf, sizeof(buf), "> LAN: %lu", (unsigned long)vm->frames_to_eth);
    canvas_draw_str(canvas, 2, 38, buf);

    snprintf(buf, sizeof(buf), "< LAN: %lu", (unsigned long)vm->frames_to_usb);
    canvas_draw_str(canvas, 2, 48, buf);

    if(vm->errors > 0) {
        snprintf(buf, sizeof(buf), "Err: %lu", (unsigned long)vm->errors);
        canvas_draw_str(canvas, 80, 48, buf);
    }

    /* PCAP dump status */
    if(vm->dump_active) {
        snprintf(buf, sizeof(buf), "REC %lu", (unsigned long)vm->dump_frames);
        canvas_draw_str(canvas, 80, 38, buf);
    }

    /* Footer */
    snprintf(buf, sizeof(buf), "[OK] %s  [<] Stop", vm->dump_active ? "Stop rec" : "Record");
    canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, buf);
}

static bool bridge_input_callback(InputEvent* event, void* context) {
    LanTesterApp* app = context;
    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        if(app->worker_running) {
            /* Bridge is active — signal it to stop */
            if(app->bridge_state) {
                app->bridge_state->running = false;
            }
            app->worker_running = false;
            return true; /* consumed — worker will clean up */
        }
        /* Bridge already stopped — let the default previous_callback
         * handle Back navigation (return to Tools menu) */
        return false;
    }
    /* OK button toggles PCAP dump */
    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->worker_running && app->bridge_state) {
            EthBridgeState* bs = app->bridge_state;
            if(!bs->dump_enabled) {
                /* Start PCAP dump */
                if(pcap_dump_start(&bs->pcap)) {
                    bs->dump_enabled = true;
                    if(app->setting_sound) {
                        notification_message(app->notifications, &sequence_success);
                    }
                }
            } else {
                /* Stop PCAP dump */
                bs->dump_enabled = false;
                pcap_dump_stop(&bs->pcap);
                if(app->setting_sound) {
                    notification_message(app->notifications, &sequence_success);
                }
            }
        }
        return true;
    }
    return false;
}

/* ==================== Host List / Host Actions callbacks ==================== */

/* Host action menu item indices */
#define HOST_ACTION_INFO          0
#define HOST_ACTION_PING          1
#define HOST_ACTION_CONT_PING     2
#define HOST_ACTION_TRACEROUTE    3
#define HOST_ACTION_PORT_SCAN_20  4
#define HOST_ACTION_PORT_SCAN_100 5
#define HOST_ACTION_NETBIOS       6
#define HOST_ACTION_SNMP          7
#define HOST_ACTION_IPMI          8
#define HOST_ACTION_PORT_SCAN_CUS 9
#define HOST_ACTION_WOL           10

static uint32_t lan_tester_nav_back_host_list(void* context) {
    UNUSED(context);
    return LanTesterViewCatScan;
}

static uint32_t lan_tester_nav_back_host_actions(void* context) {
    UNUSED(context);
    return LanTesterViewHostList;
}

static void lan_tester_host_action_callback(void* context, uint32_t index) {
    LanTesterApp* app = context;
    if(app->selected_host_idx >= app->discovered_host_count) return;

    uint8_t host_ip[4], host_mac[6];
    bool host_has_mac = false;
    if(!scan_results_read(app->selected_host_idx, host_ip, host_mac, &host_has_mac)) return;
    char ip_str[16];
    pkt_format_ip(host_ip, ip_str);

    /* Back from tool result returns to host actions menu */
    app->tool_back_view = LanTesterViewHostActions;

    switch(index) {
    case HOST_ACTION_INFO: {
        furi_string_reset(app->tool_text);
        furi_string_cat_printf(app->tool_text, "Host Info\n\nIP: %s\n", ip_str);
        if(host_has_mac) {
            char mac_str[18];
            pkt_format_mac(host_mac, mac_str);
            furi_string_cat_printf(app->tool_text, "MAC: %s\n", mac_str);
            const char* vendor = oui_lookup(host_mac);
            furi_string_cat_printf(app->tool_text, "Vendor: %s\n", vendor);
        } else {
            furi_string_cat(app->tool_text, "MAC: unknown\n");
        }
        text_box_set_text(app->text_box_tool, furi_string_get_cstr(app->tool_text));
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewToolResult);
        break;
    }
    case HOST_ACTION_PING:
        memcpy(app->ping_ip_custom, host_ip, 4);
        strncpy(app->ping_ip_input, ip_str, sizeof(app->ping_ip_input));
        lan_tester_show_view(
            app, app->text_box_tool, LanTesterViewToolResult, app->tool_text, "Initializing...\n");
        lan_tester_worker_start(app, LanTesterMenuItemPing, LanTesterViewToolResult);
        break;
    case HOST_ACTION_CONT_PING:
        memcpy(app->cont_ping_target, host_ip, 4);
        strncpy(app->cont_ping_ip_input, ip_str, sizeof(app->cont_ping_ip_input));
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewContPing);
        lan_tester_worker_start(app, LanTesterMenuItemContPing, LanTesterViewContPing);
        break;
    case HOST_ACTION_TRACEROUTE:
        memcpy(app->traceroute_target, host_ip, 4);
        strncpy(app->traceroute_host_input, ip_str, sizeof(app->traceroute_host_input));
        app->traceroute_is_hostname = false;
        furi_string_set(app->tool_text, "Initializing...\n");
        text_box_set_text(app->text_box_tool, furi_string_get_cstr(app->tool_text));
        lan_tester_worker_start(app, LanTesterMenuItemTraceroute, LanTesterViewToolResult);
        break;
    case HOST_ACTION_PORT_SCAN_20:
        memcpy(app->port_scan_target, host_ip, 4);
        strncpy(app->port_scan_ip_input, ip_str, sizeof(app->port_scan_ip_input));
        app->port_scan_top100 = false;
        app->port_scan_custom = false;
        furi_string_set(app->tool_text, "Initializing...\n");
        text_box_set_text(app->text_box_tool, furi_string_get_cstr(app->tool_text));
        lan_tester_worker_start(app, LanTesterMenuItemPortScan, LanTesterViewToolResult);
        break;
    case HOST_ACTION_PORT_SCAN_100:
        memcpy(app->port_scan_target, host_ip, 4);
        strncpy(app->port_scan_ip_input, ip_str, sizeof(app->port_scan_ip_input));
        app->port_scan_top100 = true;
        app->port_scan_custom = false;
        furi_string_set(app->tool_text, "Initializing...\n");
        text_box_set_text(app->text_box_tool, furi_string_get_cstr(app->tool_text));
        lan_tester_worker_start(app, LanTesterMenuItemPortScan, LanTesterViewToolResult);
        break;
    case HOST_ACTION_PORT_SCAN_CUS:
        memcpy(app->port_scan_target, host_ip, 4);
        strncpy(app->port_scan_ip_input, ip_str, sizeof(app->port_scan_ip_input));
        app->port_scan_custom = true;
        app->tool_back_view = LanTesterViewHostActions;
        text_input_reset(app->text_input_tool);
        text_input_set_header_text(app->text_input_tool, "Start port (1-65535):");
        text_input_set_result_callback(
            app->text_input_tool,
            lan_tester_port_scan_start_callback,
            app,
            app->port_scan_start_input,
            sizeof(app->port_scan_start_input),
            false);
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewToolInput);
        break;
    case HOST_ACTION_NETBIOS:
        memcpy(app->netbios_target, host_ip, 4);
        strncpy(app->netbios_ip_input, ip_str, sizeof(app->netbios_ip_input));
        lan_tester_show_view(
            app,
            app->text_box_tool,
            LanTesterViewToolResult,
            app->tool_text,
            "Querying NetBIOS...\n");
        lan_tester_worker_start(app, LanTesterMenuItemNetbiosQuery, LanTesterViewToolResult);
        break;
    case HOST_ACTION_SNMP:
        memcpy(app->snmp_target, host_ip, 4);
        strncpy(app->snmp_ip_input, ip_str, sizeof(app->snmp_ip_input));
        lan_tester_show_view(
            app, app->text_box_tool, LanTesterViewToolResult, app->tool_text, "Querying SNMP...\n");
        lan_tester_worker_start(app, LanTesterMenuItemSnmpGet, LanTesterViewToolResult);
        break;
    case HOST_ACTION_IPMI:
        memcpy(app->ipmi_target, host_ip, 4);
        strncpy(app->ipmi_ip_input, ip_str, sizeof(app->ipmi_ip_input));
        lan_tester_show_view(
            app, app->text_box_tool, LanTesterViewToolResult, app->tool_text, "Querying IPMI...\n");
        lan_tester_worker_start(app, LanTesterMenuItemIpmiClient, LanTesterViewToolResult);
        break;
    case HOST_ACTION_WOL:
        if(host_has_mac) {
            memcpy(app->wol_mac_input, host_mac, 6);
            furi_string_set(app->tool_text, "Sending WOL...\n");
            text_box_set_text(app->text_box_tool, furi_string_get_cstr(app->tool_text));
            lan_tester_worker_start(app, LanTesterMenuItemWol, LanTesterViewToolResult);
        }
        break;
    }
}

static void lan_tester_host_list_callback(void* context, uint32_t index) {
    LanTesterApp* app = context;

    /* Handle pagination */
    if(index == HOST_LIST_IDX_PREV) {
        if(app->host_list_page > 0) app->host_list_page--;
        lan_tester_populate_host_list(app);
        return;
    }
    if(index == HOST_LIST_IDX_NEXT) {
        app->host_list_page++;
        lan_tester_populate_host_list(app);
        return;
    }

    if(index >= app->discovered_host_count) return;

    app->selected_host_idx = (uint16_t)index;

    uint8_t host_ip[4], host_mac[6];
    bool host_has_mac = false;
    if(!scan_results_read(index, host_ip, host_mac, &host_has_mac)) return;

    /* Populate host actions submenu */
    submenu_reset(app->submenu_host_actions);

    char ip_str[16];
    pkt_format_ip(host_ip, ip_str);
    submenu_set_header(app->submenu_host_actions, ip_str);

    submenu_add_item(
        app->submenu_host_actions,
        "Host Info",
        HOST_ACTION_INFO,
        lan_tester_host_action_callback,
        app);
    submenu_add_item(
        app->submenu_host_actions, "Ping", HOST_ACTION_PING, lan_tester_host_action_callback, app);
    submenu_add_item(
        app->submenu_host_actions,
        "Continuous Ping",
        HOST_ACTION_CONT_PING,
        lan_tester_host_action_callback,
        app);
    submenu_add_item(
        app->submenu_host_actions,
        "Traceroute",
        HOST_ACTION_TRACEROUTE,
        lan_tester_host_action_callback,
        app);
    submenu_add_item(
        app->submenu_host_actions,
        "Port Scan (Top 20)",
        HOST_ACTION_PORT_SCAN_20,
        lan_tester_host_action_callback,
        app);
    submenu_add_item(
        app->submenu_host_actions,
        "Port Scan (Top 100)",
        HOST_ACTION_PORT_SCAN_100,
        lan_tester_host_action_callback,
        app);
    submenu_add_item(
        app->submenu_host_actions,
        "Port Scan (Custom)",
        HOST_ACTION_PORT_SCAN_CUS,
        lan_tester_host_action_callback,
        app);
    submenu_add_item(
        app->submenu_host_actions,
        "NetBIOS Query",
        HOST_ACTION_NETBIOS,
        lan_tester_host_action_callback,
        app);
    submenu_add_item(
        app->submenu_host_actions,
        "SNMP GET",
        HOST_ACTION_SNMP,
        lan_tester_host_action_callback,
        app);
    submenu_add_item(
        app->submenu_host_actions,
        "IPMI Query",
        HOST_ACTION_IPMI,
        lan_tester_host_action_callback,
        app);

    if(host_has_mac) {
        submenu_add_item(
            app->submenu_host_actions,
            "Wake-on-LAN",
            HOST_ACTION_WOL,
            lan_tester_host_action_callback,
            app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewHostActions);
}

/* Populate host list submenu from scan results file (one page, single file read) */
static void lan_tester_populate_host_list(LanTesterApp* app) {
    submenu_reset(app->submenu_host_list);

    uint16_t total = app->discovered_host_count;
    uint16_t page = app->host_list_page;
    uint16_t start = page * HOST_LIST_PAGE_SIZE;
    uint16_t end = start + HOST_LIST_PAGE_SIZE;
    if(end > total) end = total;
    uint16_t total_pages = (total + HOST_LIST_PAGE_SIZE - 1) / HOST_LIST_PAGE_SIZE;

    char header[32];
    if(total_pages <= 1) {
        snprintf(header, sizeof(header), "Hosts: %d", total);
    } else {
        snprintf(header, sizeof(header), "Hosts: %d (%d/%d)", total, page + 1, total_pages);
    }
    submenu_set_header(app->submenu_host_list, header);

    if(page > 0) {
        submenu_add_item(
            app->submenu_host_list,
            "< Prev page",
            HOST_LIST_IDX_PREV,
            lan_tester_host_list_callback,
            app);
    }

    /* Read one page in a single file pass */
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, SCAN_RESULTS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line[36];
        uint16_t line_idx = 0;
        uint16_t pos = 0;
        char ch;
        while(storage_file_read(f, &ch, 1) == 1) {
            if(ch == '\n') {
                if(line_idx >= start && line_idx < end) {
                    line[pos] = '\0';
                    /* Parse IP and optional MAC */
                    unsigned a, b, c, d;
                    if(sscanf(line, "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
                        char label[40];
                        char* comma = strchr(line, ',');
                        if(comma) {
                            unsigned m[6];
                            if(sscanf(
                                   comma + 1,
                                   "%02X:%02X:%02X:%02X:%02X:%02X",
                                   &m[0],
                                   &m[1],
                                   &m[2],
                                   &m[3],
                                   &m[4],
                                   &m[5]) == 6) {
                                uint8_t mac[6];
                                for(int i = 0; i < 6; i++)
                                    mac[i] = (uint8_t)m[i];
                                const char* vendor = oui_lookup(mac);
                                snprintf(
                                    label, sizeof(label), "%u.%u.%u.%u (%s)", a, b, c, d, vendor);
                            } else {
                                snprintf(label, sizeof(label), "%u.%u.%u.%u", a, b, c, d);
                            }
                        } else {
                            snprintf(label, sizeof(label), "%u.%u.%u.%u", a, b, c, d);
                        }
                        submenu_add_item(
                            app->submenu_host_list,
                            label,
                            line_idx,
                            lan_tester_host_list_callback,
                            app);
                    }
                }
                line_idx++;
                pos = 0;
                if(line_idx >= end) break;
            } else if(pos < sizeof(line) - 1) {
                line[pos++] = ch;
            }
        }
    }
    storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);

    if(end < total) {
        submenu_add_item(
            app->submenu_host_list,
            "Next page >",
            HOST_LIST_IDX_NEXT,
            lan_tester_host_list_callback,
            app);
    }
}

/* ==================== Packet Capture view model & callbacks ==================== */

typedef struct {
    LanTesterApp* app;
} PacketCaptureViewModel;

static void packet_capture_draw_callback(Canvas* canvas, void* model) {
    PacketCaptureViewModel* vm = model;
    LanTesterApp* app = vm->app;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, "Packet Capture");

    canvas_set_font(canvas, FontSecondary);

    if(app->pcap_state.active) {
        uint32_t elapsed = (furi_get_tick() - app->pcap_start_tick) / 1000;
        char buf[48];

        canvas_draw_str(canvas, 2, 26, "Status: RECORDING");

        snprintf(buf, sizeof(buf), "Frames: %lu", (unsigned long)app->pcap_state.frames_written);
        canvas_draw_str(canvas, 2, 38, buf);

        snprintf(
            buf, sizeof(buf), "Size: %lu bytes", (unsigned long)app->pcap_state.bytes_written);
        canvas_draw_str(canvas, 2, 48, buf);

        snprintf(buf, sizeof(buf), "Time: %lu sec", (unsigned long)elapsed);
        canvas_draw_str(canvas, 2, 58, buf);

        canvas_draw_str(canvas, 80, 58, "[OK] Stop");
    } else {
        canvas_draw_str(canvas, 2, 26, "Status: Idle");

        if(app->pcap_state.frames_written > 0) {
            char buf[48];
            snprintf(
                buf,
                sizeof(buf),
                "Last: %lu frames, %lu B",
                (unsigned long)app->pcap_state.frames_written,
                (unsigned long)app->pcap_state.bytes_written);
            canvas_draw_str(canvas, 2, 38, buf);
        }

        canvas_draw_str(canvas, 2, 58, "[OK] Start  [<] Back");
    }
}

static bool packet_capture_input_callback(InputEvent* event, void* context) {
    LanTesterApp* app = context;

    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(!app->pcap_state.active) {
            /* Start capture */
            memset(&app->pcap_state, 0, sizeof(app->pcap_state));
            app->pcap_start_tick = furi_get_tick();
            lan_tester_worker_start(
                app, LanTesterMenuItemPacketCapture, LanTesterViewPacketCapture);
        } else {
            /* Stop capture */
            app->worker_running = false;
        }
        return true;
    }

    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        if(app->pcap_state.active) {
            app->worker_running = false;
            return true;
        }
        return false; /* let ViewDispatcher handle back navigation */
    }

    return false;
}

/* ==================== Continuous Ping view model & callbacks ==================== */

typedef struct {
    LanTesterApp* app;
} ContPingViewModel;

static void cont_ping_draw_callback(Canvas* canvas, void* model) {
    ContPingViewModel* vm = model;
    LanTesterApp* app = vm->app;
    PingGraphState* pg = app->ping_graph;

    canvas_clear(canvas);

    if(!pg) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 12, "Initializing...");
        return;
    }

    canvas_set_font(canvas, FontSecondary);

    char buf[64];

    /* Show target IP */
    snprintf(
        buf,
        sizeof(buf),
        "Ping %d.%d.%d.%d",
        app->cont_ping_target[0],
        app->cont_ping_target[1],
        app->cont_ping_target[2],
        app->cont_ping_target[3]);
    canvas_draw_str(canvas, 0, 7, buf);

    uint32_t cur = 0;
    if(pg->sample_count > 0) {
        uint32_t last = ping_graph_get_sample(pg, pg->sample_count - 1);
        cur = (last == PING_RTT_TIMEOUT) ? 0 : last;
    }
    uint32_t avg = ping_graph_avg_rtt(pg);
    uint8_t loss = ping_graph_loss_percent(pg);

    snprintf(
        buf, sizeof(buf), "%lums avg:%lu loss:%d%%", (unsigned long)cur, (unsigned long)avg, loss);
    canvas_draw_str(canvas, 0, 16, buf);

    uint8_t graph_top = 22;
    uint8_t graph_bottom = 63;
    uint8_t graph_height = graph_bottom - graph_top;
    uint8_t graph_width = 128;

    canvas_draw_line(canvas, 0, graph_top, 0, graph_bottom);
    canvas_draw_line(canvas, 0, graph_bottom, graph_width - 1, graph_bottom);

    uint16_t count = ping_graph_visible_count(pg);
    if(count == 0) return;

    /* Determine how many samples to display (at most graph_width) */
    uint16_t visible = (count > graph_width) ? graph_width : count;
    uint16_t start_sample = count - visible;

    uint32_t max_rtt = 1;
    for(uint16_t i = 0; i < visible; i++) {
        uint32_t s = ping_graph_get_sample(pg, start_sample + i);
        if(s != PING_RTT_TIMEOUT && s > max_rtt) max_rtt = s;
    }
    max_rtt = max_rtt + max_rtt / 10 + 1;

    /* Draw from right edge to left: newest sample at x = graph_width-1,
     * oldest at x = graph_width - visible. When few samples, left side is empty. */
    uint16_t x_offset = graph_width - visible;

    int16_t prev_y = -1;
    for(uint16_t i = 0; i < visible; i++) {
        uint32_t rtt = ping_graph_get_sample(pg, start_sample + i);
        uint8_t x = (uint8_t)(x_offset + i);

        if(rtt == PING_RTT_TIMEOUT) {
            canvas_draw_dot(canvas, x, graph_top + 1);
            canvas_draw_dot(canvas, x, graph_top + 2);
            prev_y = -1;
        } else {
            uint32_t scaled = (rtt * graph_height) / max_rtt;
            if(scaled > graph_height) scaled = graph_height;
            int16_t y = (int16_t)(graph_bottom - scaled);

            if(prev_y >= 0) {
                canvas_draw_line(canvas, x - 1, (uint8_t)prev_y, x, (uint8_t)y);
            } else {
                canvas_draw_dot(canvas, x, (uint8_t)y);
            }
            prev_y = y;
        }
    }
}

static bool cont_ping_input_callback(InputEvent* event, void* context) {
    LanTesterApp* app = context;

    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        if(app->ping_graph) {
            app->ping_graph->running = false;
        }
        app->worker_running = false;
        view_dispatcher_send_custom_event(app->view_dispatcher, CUSTOM_EVENT_CONT_PING_BACK);
        return true;
    }

    return false;
}

/* ==================== App alloc / free ==================== */

/* ==================== Settings view callbacks ==================== */

static const char* const setting_onoff[] = {"OFF", "ON"};

static void settings_autosave_changed(VariableItem* item) {
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, setting_onoff[idx]);
    if(g_app) {
        g_app->setting_autosave = (idx == 1);
        lan_tester_settings_save(g_app);
    }
}

static void settings_sound_changed(VariableItem* item) {
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, setting_onoff[idx]);
    if(g_app) {
        g_app->setting_sound = (idx == 1);
        lan_tester_settings_save(g_app);
    }
}

static void settings_dns_custom_changed(VariableItem* item) {
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, setting_onoff[idx]);
    if(g_app) {
        g_app->dns_custom_enabled = (idx == 1);
        lan_tester_settings_save(g_app);
    }
}

static void settings_ping_count_changed(VariableItem* item) {
    uint8_t idx = variable_item_get_current_value_index(item);
    uint8_t count = idx + 1; /* 0 -> 1, 99 -> 100 */
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", count);
    variable_item_set_current_value_text(item, buf);
    if(g_app) {
        g_app->ping_count = count;
        lan_tester_settings_save(g_app);
    }
}

static void settings_ping_timeout_changed(VariableItem* item) {
    uint8_t idx = variable_item_get_current_value_index(item);
    uint16_t timeout = (idx + 1) * 500; /* 500, 1000, ..., 10000 */
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", timeout);
    variable_item_set_current_value_text(item, buf);
    if(g_app) {
        g_app->ping_timeout_ms = timeout;
        lan_tester_settings_save(g_app);
    }
}

static void settings_ping_interval_changed(VariableItem* item) {
    uint8_t idx = variable_item_get_current_value_index(item);
    uint16_t interval = (idx + 1) * 200; /* 200, 400, ..., 5000 */
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", interval);
    variable_item_set_current_value_text(item, buf);
    if(g_app) {
        g_app->ping_interval_ms = interval;
        lan_tester_settings_save(g_app);
    }
}

static const uint8_t autotest_lldp_wait_options[] = {10, 20, 30, 60};
#define AUTOTEST_LLDP_WAIT_COUNT 4

static void settings_autotest_lldp_wait_changed(VariableItem* item) {
    uint8_t idx = variable_item_get_current_value_index(item);
    if(idx >= AUTOTEST_LLDP_WAIT_COUNT) idx = 2; /* default 30s */
    char buf[8];
    snprintf(buf, sizeof(buf), "%d s", autotest_lldp_wait_options[idx]);
    variable_item_set_current_value_text(item, buf);
    if(g_app) {
        g_app->autotest_lldp_wait_s = autotest_lldp_wait_options[idx];
        lan_tester_settings_save(g_app);
    }
}

static void settings_autotest_arp_changed(VariableItem* item) {
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, setting_onoff[idx]);
    if(g_app) {
        g_app->autotest_arp_enabled = (idx == 1);
        lan_tester_settings_save(g_app);
    }
}

static void autotest_dns_host_input_callback(void* context) {
    LanTesterApp* app = context;
    furi_assert(app);
    lan_tester_settings_save(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewSettings);
}

static void dns_custom_ip_input_callback(void* context) {
    LanTesterApp* app = context;
    furi_assert(app);
    lan_tester_parse_ip(app->dns_custom_ip_input, app->dns_custom_server);
    lan_tester_settings_save(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewSettings);
}

/* Helper: get active DNS server (custom if enabled, else DHCP) */
static void lan_tester_get_dns_server(LanTesterApp* app, uint8_t out_ip[4]) {
    if(app->dns_custom_enabled) {
        memcpy(out_ip, app->dns_custom_server, 4);
    } else {
        memcpy(out_ip, app->dhcp_dns, 4);
    }
}

/* Settings item indices — keep in sync with variable_item_list_add order */
typedef enum {
    LanTesterSettingsItemAutosave = 0,
    LanTesterSettingsItemSound = 1,
    LanTesterSettingsItemDnsCustom = 2,
    LanTesterSettingsItemDnsServer = 3,
    LanTesterSettingsItemPingCount = 4,
    LanTesterSettingsItemPingTimeout = 5,
    LanTesterSettingsItemPingInterval = 6,
    LanTesterSettingsItemClearHistory = 7,
    LanTesterSettingsItemMacChanger = 8,
    LanTesterSettingsItemAutoTestDnsHost = 9,
    LanTesterSettingsItemAutoTestLldpWait = 10,
    LanTesterSettingsItemAutoTestArpScan = 11,
    LanTesterSettingsItemAbout = 12,
    LanTesterSettingsItemCount,
} LanTesterSettingsItem;

static void settings_enter_callback(void* context, uint32_t index) {
    LanTesterApp* app = context;
    if(index == LanTesterSettingsItemDnsServer) {
        ip_keyboard_setup(
            app->ip_keyboard,
            "DNS Server IP:",
            app->dns_custom_ip_input,
            false,
            dns_custom_ip_input_callback,
            app,
            app->dns_custom_ip_input,
            sizeof(app->dns_custom_ip_input),
            lan_tester_nav_back_settings);
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewIpKeyboard);
    } else if(index == LanTesterSettingsItemClearHistory) {
        /* Delete all .txt files from history dir without loading full list into RAM */
        Storage* storage = furi_record_open(RECORD_STORAGE);
        File* dir = storage_file_alloc(storage);
        /* Static to avoid 256B combined stack usage (dir_path + fpath) */
        static char dir_path[128];
        snprintf(dir_path, sizeof(dir_path), "%s", HISTORY_DIR);
        size_t plen = strlen(dir_path);
        if(plen > 1 && dir_path[plen - 1] == '/') dir_path[plen - 1] = '\0';
        if(storage_dir_open(dir, dir_path)) {
            FileInfo finfo;
            char name[HISTORY_FILENAME_LEN];
            static char fpath[128];
            while(storage_dir_read(dir, &finfo, name, sizeof(name))) {
                if(finfo.flags & FSF_DIRECTORY) continue;
                uint16_t nlen = strlen(name);
                if(nlen > 4 && strcmp(&name[nlen - 4], ".txt") == 0) {
                    snprintf(fpath, sizeof(fpath), APP_DATA_PATH("%s"), name);
                    storage_simply_remove(storage, fpath);
                }
            }
            storage_dir_close(dir);
        }
        storage_file_free(dir);
        furi_record_close(RECORD_STORAGE);
        if(app->setting_sound) {
            notification_message(app->notifications, &sequence_success);
        }
    } else if(index == LanTesterSettingsItemMacChanger) {
        mac_changer_generate_random(app->mac_changer_input);
        byte_input_set_header_text(app->byte_input_tool, "New MAC (edit or OK):");
        byte_input_set_result_callback(
            app->byte_input_tool,
            lan_tester_mac_changer_input_callback,
            NULL,
            app,
            app->mac_changer_input,
            6);
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewToolByteInput);
    } else if(index == LanTesterSettingsItemAutoTestDnsHost) {
        text_input_reset(app->text_input_tool);
        text_input_set_header_text(app->text_input_tool, "AutoTest DNS host:");
        text_input_set_result_callback(
            app->text_input_tool,
            autotest_dns_host_input_callback,
            app,
            app->autotest_dns_host,
            sizeof(app->autotest_dns_host),
            false);
        /* Override back navigation to return to Settings (not Diagnostics) */
        view_set_previous_callback(
            text_input_get_view(app->text_input_tool), lan_tester_nav_back_settings);
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewToolInput);
    } else if(index == LanTesterSettingsItemAbout) {
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewAbout);
    }
}

/* ==================== PXE Settings callbacks ==================== */

static uint32_t lan_tester_nav_back_settings(void* context) {
    UNUSED(context);
    lan_tester_stop_worker_on_back();
    return LanTesterViewSettings;
}

static uint32_t lan_tester_nav_back_pxe_settings(void* context) {
    UNUSED(context);
    return LanTesterViewPxeSettings;
}

static void pxe_server_ip_input_callback(void* context) {
    LanTesterApp* app = context;
    furi_assert(app);
    lan_tester_parse_ip(app->pxe_server_ip_input, app->pxe_server_ip);
    variable_item_set_current_value_text(app->pxe_item_sip, app->pxe_server_ip_input);
    view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewPxeSettings);
}

static void pxe_client_ip_input_callback(void* context) {
    LanTesterApp* app = context;
    furi_assert(app);
    lan_tester_parse_ip(app->pxe_client_ip_input, app->pxe_client_ip);
    variable_item_set_current_value_text(app->pxe_item_cip, app->pxe_client_ip_input);
    view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewPxeSettings);
}

static void pxe_subnet_input_callback(void* context) {
    LanTesterApp* app = context;
    furi_assert(app);
    lan_tester_parse_ip(app->pxe_subnet_input, app->pxe_subnet);
    variable_item_set_current_value_text(app->pxe_item_sub, app->pxe_subnet_input);
    view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewPxeSettings);
}

static void pxe_dhcp_toggle_callback(VariableItem* item) {
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, setting_onoff[idx]);
    if(g_app) {
        g_app->pxe_dhcp_enabled = (idx == 1);
    }
}

/* Boot file selection cycling callback */
static void pxe_boot_file_changed(VariableItem* item) {
    if(!g_app) return;
    uint8_t idx = variable_item_get_current_value_index(item);
    LanTesterApp* app = g_app;
    PxeServerState* scan = &app->pxe_scan;

    if(idx < scan->boot_file_count) {
        app->pxe_boot_file_idx = idx;
        char info[96];
        snprintf(
            info,
            sizeof(info),
            "%s (%luB)",
            scan->boot_files[idx].filename,
            (unsigned long)scan->boot_files[idx].file_size);
        variable_item_set_current_value_text(item, info);
    }
}

static void pxe_settings_enter_callback(void* context, uint32_t index) {
    LanTesterApp* app = context;
    furi_assert(app);

    switch(index) {
    case 0: /* >>> Start PXE <<< */ {
        /* Apply selected boot file to pxe_scan before starting */
        uint8_t bi = app->pxe_boot_file_idx;
        if(bi < app->pxe_scan.boot_file_count) {
            strncpy(
                app->pxe_scan.boot_filename,
                app->pxe_scan.boot_files[bi].filename,
                sizeof(app->pxe_scan.boot_filename) - 1);
            app->pxe_scan.boot_file_size = app->pxe_scan.boot_files[bi].file_size;
        }
        furi_string_set(app->tool_text, "Starting PXE Server...\n");
        text_box_set_text(app->text_box_tool, furi_string_get_cstr(app->tool_text));
        lan_tester_worker_start(app, LanTesterMenuItemPxeServer, LanTesterViewToolResult);
        break;
    }
    case 1: /* DHCP Server toggle — handled by change_callback */
        break;
    case 2: /* Boot File — cycling handled by change_callback */
        break;
    case 3: /* Server IP */
        ip_keyboard_setup(
            app->ip_keyboard,
            "Server IP:",
            app->pxe_server_ip_input,
            false,
            pxe_server_ip_input_callback,
            app,
            app->pxe_server_ip_input,
            sizeof(app->pxe_server_ip_input),
            lan_tester_nav_back_pxe_settings);
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewIpKeyboard);
        break;
    case 4: /* Client IP */
        ip_keyboard_setup(
            app->ip_keyboard,
            "Client IP:",
            app->pxe_client_ip_input,
            false,
            pxe_client_ip_input_callback,
            app,
            app->pxe_client_ip_input,
            sizeof(app->pxe_client_ip_input),
            lan_tester_nav_back_pxe_settings);
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewIpKeyboard);
        break;
    case 5: /* Subnet Mask */
        ip_keyboard_setup(
            app->ip_keyboard,
            "Subnet Mask:",
            app->pxe_subnet_input,
            false,
            pxe_subnet_input_callback,
            app,
            app->pxe_subnet_input,
            sizeof(app->pxe_subnet_input),
            lan_tester_nav_back_pxe_settings);
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewIpKeyboard);
        break;
    case 6: /* Download Boot Files */
        app->tool_back_view = LanTesterViewPxeSettings;
        furi_string_set(app->tool_text, "[PXE Download]\nStarting...\n");
        text_box_set_text(app->text_box_tool, furi_string_get_cstr(app->tool_text));
        lan_tester_worker_start(app, LanTesterMenuItemPxeDownload, LanTesterViewToolResult);
        break;
    }
}

/* Refresh boot file list and DHCP defaults for PXE settings screen */
static void pxe_settings_refresh(LanTesterApp* app) {
    /* Scan for boot files */
    memset(&app->pxe_scan, 0, sizeof(app->pxe_scan));
    bool found = pxe_detect_boot_file(&app->pxe_scan);

    if(found && app->pxe_scan.boot_file_count > 0) {
        /* Set up cycling for boot file selection */
        app->pxe_boot_file_idx = 0;
        variable_item_set_values_count(app->pxe_item_boot, app->pxe_scan.boot_file_count);
        variable_item_set_current_value_index(app->pxe_item_boot, 0);
        char info[96];
        snprintf(
            info,
            sizeof(info),
            "%s (%luB)",
            app->pxe_scan.boot_files[0].filename,
            (unsigned long)app->pxe_scan.boot_files[0].file_size);
        variable_item_set_current_value_text(app->pxe_item_boot, info);
    } else {
        variable_item_set_values_count(app->pxe_item_boot, 0);
        variable_item_set_current_value_text(app->pxe_item_boot, "Not found!");
    }

    /* Probe external DHCP once per session to auto-populate IP fields.
     * Requires W5500 initialized and link up. */
    if(!app->pxe_dhcp_probed && app->w5500_initialized && w5500_hal_get_link_status()) {
        app->pxe_dhcp_probed = true;

        PxeExternalDhcp ext;
        if(pxe_detect_external_dhcp(W5500_DHCP_SOCKET, app->mac_addr, &ext)) {
            /* External DHCP found — disable own DHCP, populate from detected subnet */
            app->pxe_dhcp_enabled = false;
            variable_item_set_current_value_index(app->pxe_item_dhcp, 0);
            variable_item_set_current_value_text(app->pxe_item_dhcp, "OFF");

            /* Server IP: offered + 100 (stay in subnet, avoid conflicts) */
            uint8_t sip[4];
            memcpy(sip, ext.offered_ip, 4);
            sip[3] = (uint8_t)(ext.offered_ip[3] + 100);
            if(sip[3] < ext.offered_ip[3]) sip[3] = 250;

            snprintf(
                app->pxe_server_ip_input,
                sizeof(app->pxe_server_ip_input),
                "%d.%d.%d.%d",
                sip[0],
                sip[1],
                sip[2],
                sip[3]);
            lan_tester_parse_ip(app->pxe_server_ip_input, app->pxe_server_ip);
            variable_item_set_current_value_text(app->pxe_item_sip, app->pxe_server_ip_input);

            /* Client IP: use offered IP */
            snprintf(
                app->pxe_client_ip_input,
                sizeof(app->pxe_client_ip_input),
                "%d.%d.%d.%d",
                ext.offered_ip[0],
                ext.offered_ip[1],
                ext.offered_ip[2],
                ext.offered_ip[3]);
            lan_tester_parse_ip(app->pxe_client_ip_input, app->pxe_client_ip);
            variable_item_set_current_value_text(app->pxe_item_cip, app->pxe_client_ip_input);

            /* Subnet from DHCP */
            if(ext.subnet[0] | ext.subnet[1] | ext.subnet[2] | ext.subnet[3]) {
                snprintf(
                    app->pxe_subnet_input,
                    sizeof(app->pxe_subnet_input),
                    "%d.%d.%d.%d",
                    ext.subnet[0],
                    ext.subnet[1],
                    ext.subnet[2],
                    ext.subnet[3]);
                lan_tester_parse_ip(app->pxe_subnet_input, app->pxe_subnet);
                variable_item_set_current_value_text(app->pxe_item_sub, app->pxe_subnet_input);
            }

            FURI_LOG_I(TAG, "PXE: ext DHCP detected, defaults updated");
        }
        /* If no external DHCP, keep the hardcoded defaults (192.168.77.x) */
    }
}

static LanTesterApp* lan_tester_app_alloc(void) {
    LanTesterApp* app = malloc(sizeof(LanTesterApp));
    if(!app) return NULL;
    memset(app, 0, sizeof(LanTesterApp));
    g_app = app;

    /* Frame buffer lazy-allocated in ensure_w5500() to save 1.6KB at idle */
    app->frame_buf = NULL;

    /* Set default MAC (derived from device UID for uniqueness) */
    lan_tester_generate_default_mac(app->mac_addr);

    /* DHCP timer: 1 second periodic, needed by WIZnet DHCP_run() */
    app->dhcp_timer = furi_timer_alloc(dhcp_timer_callback, FuriTimerTypePeriodic, NULL);
    furi_timer_start(app->dhcp_timer, 1000);

    /* Allocate shared text buffer (single FuriString for all tools) */
    app->tool_text = furi_string_alloc();

    /* Open GUI */
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    /* ViewDispatcher */
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, lan_tester_nav_event_cb);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, lan_tester_custom_event_cb);

    /* Main menu (Submenu view) */
    app->submenu = submenu_alloc();
    /* Main menu: grouped categories */
    submenu_add_item(
        app->submenu, "Auto Test", LanTesterMenuItemAutoTest, lan_tester_submenu_callback, app);
    submenu_add_item(app->submenu, "Port Info", 100, lan_tester_submenu_callback, app);
    submenu_add_item(app->submenu, "Scan", 101, lan_tester_submenu_callback, app);
    submenu_add_item(app->submenu, "Diagnostics", 102, lan_tester_submenu_callback, app);
    submenu_add_item(app->submenu, "Traffic", 105, lan_tester_submenu_callback, app);
    submenu_add_item(app->submenu, "Utilities", 103, lan_tester_submenu_callback, app);
    submenu_add_item(app->submenu, "Security", 107, lan_tester_submenu_callback, app);
    submenu_add_item(
        app->submenu, "History", LanTesterMenuItemHistory, lan_tester_submenu_callback, app);
    submenu_add_item(app->submenu, "Settings", 104, lan_tester_submenu_callback, app);
    view_set_previous_callback(
        submenu_get_view(app->submenu), lan_tester_navigation_exit_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, LanTesterViewMainMenu, submenu_get_view(app->submenu));

    /* Category: Port Info */
    app->submenu_cat_portinfo = submenu_alloc();
    submenu_add_item(
        app->submenu_cat_portinfo,
        "Link Info",
        LanTesterMenuItemLinkInfo,
        lan_tester_submenu_callback,
        app);
    submenu_add_item(
        app->submenu_cat_portinfo,
        "DHCP Analyze",
        LanTesterMenuItemDhcpAnalyze,
        lan_tester_submenu_callback,
        app);
    submenu_add_item(
        app->submenu_cat_portinfo,
        "LLDP/CDP",
        LanTesterMenuItemLldpCdp,
        lan_tester_submenu_callback,
        app);
    submenu_add_item(
        app->submenu_cat_portinfo,
        "STP/VLAN",
        LanTesterMenuItemStpVlan,
        lan_tester_submenu_callback,
        app);
    submenu_add_item(
        app->submenu_cat_portinfo,
        "SNMP GET",
        LanTesterMenuItemSnmpGet,
        lan_tester_submenu_callback,
        app);
    view_set_previous_callback(
        submenu_get_view(app->submenu_cat_portinfo), lan_tester_navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher,
        LanTesterViewCatPortInfo,
        submenu_get_view(app->submenu_cat_portinfo));

    /* Category: Scan */
    app->submenu_cat_scan = submenu_alloc();
    submenu_add_item(
        app->submenu_cat_scan,
        "ARP Scan",
        LanTesterMenuItemArpScan,
        lan_tester_submenu_callback,
        app);
    submenu_add_item(
        app->submenu_cat_scan,
        "Ping Sweep",
        LanTesterMenuItemPingSweep,
        lan_tester_submenu_callback,
        app);
    submenu_add_item(
        app->submenu_cat_scan,
        "mDNS/SSDP",
        LanTesterMenuItemDiscovery,
        lan_tester_submenu_callback,
        app);
    submenu_add_item(
        app->submenu_cat_scan,
        "NetBIOS Query",
        LanTesterMenuItemNetbiosQuery,
        lan_tester_submenu_callback,
        app);
    submenu_add_item(app->submenu_cat_scan, "Port Scan", 106, lan_tester_submenu_callback, app);
    view_set_previous_callback(
        submenu_get_view(app->submenu_cat_scan), lan_tester_navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, LanTesterViewCatScan, submenu_get_view(app->submenu_cat_scan));

    /* Category: Diagnostics */
    app->submenu_cat_diag = submenu_alloc();
    submenu_add_item(
        app->submenu_cat_diag, "Ping", LanTesterMenuItemPing, lan_tester_submenu_callback, app);
    submenu_add_item(
        app->submenu_cat_diag,
        "Continuous Ping",
        LanTesterMenuItemContPing,
        lan_tester_submenu_callback,
        app);
    submenu_add_item(
        app->submenu_cat_diag,
        "DNS Lookup",
        LanTesterMenuItemDnsLookup,
        lan_tester_submenu_callback,
        app);
    submenu_add_item(
        app->submenu_cat_diag,
        "Traceroute",
        LanTesterMenuItemTraceroute,
        lan_tester_submenu_callback,
        app);
    submenu_add_item(
        app->submenu_cat_diag,
        "NTP Diagnostics",
        LanTesterMenuItemNtpDiag,
        lan_tester_submenu_callback,
        app);
    submenu_add_item(
        app->submenu_cat_diag,
        "Apply NTP Sync",
        LanTesterMenuItemNtpSync,
        lan_tester_submenu_callback,
        app);
    submenu_add_item(
        app->submenu_cat_diag,
        "DNS Poison Check",
        LanTesterMenuItemDnsPoisonCheck,
        lan_tester_submenu_callback,
        app);
    view_set_previous_callback(
        submenu_get_view(app->submenu_cat_diag), lan_tester_navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, LanTesterViewCatDiag, submenu_get_view(app->submenu_cat_diag));

    /* Category: Traffic */
    app->submenu_cat_traffic = submenu_alloc();
    submenu_add_item(
        app->submenu_cat_traffic,
        "Packet Capture",
        LanTesterMenuItemPacketCapture,
        lan_tester_submenu_callback,
        app);
    submenu_add_item(
        app->submenu_cat_traffic,
        "ETH Bridge",
        LanTesterMenuItemEthBridge,
        lan_tester_submenu_callback,
        app);
    submenu_add_item(
        app->submenu_cat_traffic,
        "Statistics",
        LanTesterMenuItemStats,
        lan_tester_submenu_callback,
        app);
    view_set_previous_callback(
        submenu_get_view(app->submenu_cat_traffic), lan_tester_navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, LanTesterViewCatTraffic, submenu_get_view(app->submenu_cat_traffic));

    /* Category: Utilities */
    app->submenu_cat_utilities = submenu_alloc();
    submenu_add_item(
        app->submenu_cat_utilities,
        "Wake-on-LAN",
        LanTesterMenuItemWol,
        lan_tester_submenu_callback,
        app);
    submenu_add_item(
        app->submenu_cat_utilities,
        "PXE Server",
        LanTesterMenuItemPxeServer,
        lan_tester_submenu_callback,
        app);
    submenu_add_item(
        app->submenu_cat_utilities,
        "File Manager",
        LanTesterMenuItemFileManager,
        lan_tester_submenu_callback,
        app);
    submenu_add_item(
        app->submenu_cat_utilities,
        "TFTP Client",
        LanTesterMenuItemTftpClient,
        lan_tester_submenu_callback,
        app);
    submenu_add_item(
        app->submenu_cat_utilities,
        "IPMI Query",
        LanTesterMenuItemIpmiClient,
        lan_tester_submenu_callback,
        app);
    view_set_previous_callback(
        submenu_get_view(app->submenu_cat_utilities), lan_tester_navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher,
        LanTesterViewCatUtilities,
        submenu_get_view(app->submenu_cat_utilities));

    /* Category: Security */
    app->submenu_cat_security = submenu_alloc();
    submenu_add_item(
        app->submenu_cat_security,
        "ARP Watch",
        LanTesterMenuItemArpWatch,
        lan_tester_submenu_callback,
        app);
    submenu_add_item(
        app->submenu_cat_security,
        "Rogue DHCP",
        LanTesterMenuItemRogueDhcp,
        lan_tester_submenu_callback,
        app);
    submenu_add_item(
        app->submenu_cat_security,
        "Rogue RA (IPv6)",
        LanTesterMenuItemRogueRa,
        lan_tester_submenu_callback,
        app);
    submenu_add_item(
        app->submenu_cat_security,
        "DHCP Fingerprint",
        LanTesterMenuItemDhcpFingerprint,
        lan_tester_submenu_callback,
        app);
    submenu_add_item(
        app->submenu_cat_security,
        "802.1X Probe",
        LanTesterMenuItemEapolProbe,
        lan_tester_submenu_callback,
        app);
    submenu_add_item(
        app->submenu_cat_security,
        "VLAN Hop Top10",
        LanTesterMenuItemVlanHopTop10,
        lan_tester_submenu_callback,
        app);
    submenu_add_item(
        app->submenu_cat_security,
        "VLAN Hop Custom",
        LanTesterMenuItemVlanHopCustom,
        lan_tester_submenu_callback,
        app);
    view_set_previous_callback(
        submenu_get_view(app->submenu_cat_security), lan_tester_navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher,
        LanTesterViewCatSecurity,
        submenu_get_view(app->submenu_cat_security));

    /* Port Scan Mode submenu */
    app->submenu_port_scan_mode = submenu_alloc();
    submenu_add_item(
        app->submenu_port_scan_mode,
        "Top 20",
        LanTesterMenuItemPortScan,
        lan_tester_submenu_callback,
        app);
    submenu_add_item(
        app->submenu_port_scan_mode,
        "Top 100",
        LanTesterMenuItemPortScanFull,
        lan_tester_submenu_callback,
        app);
    submenu_add_item(
        app->submenu_port_scan_mode,
        "Custom Range",
        LanTesterMenuItemPortScanCustom,
        lan_tester_submenu_callback,
        app);
    view_set_previous_callback(
        submenu_get_view(app->submenu_port_scan_mode), lan_tester_nav_back_scan);
    view_dispatcher_add_view(
        app->view_dispatcher,
        LanTesterViewPortScanMode,
        submenu_get_view(app->submenu_port_scan_mode));

    /* Shared TextBox for ALL tool results (allocated once, reused) */
    app->text_box_tool = text_box_alloc();
    text_box_set_font(app->text_box_tool, TextBoxFontText);
    view_set_previous_callback(text_box_get_view(app->text_box_tool), lan_tester_nav_back_tool);
    view_dispatcher_add_view(
        app->view_dispatcher, LanTesterViewToolResult, text_box_get_view(app->text_box_tool));
    app->tool_back_view = LanTesterViewMainMenu;

    /* Shared TextInput for all text entry (hostnames, filenames, ports) */
    app->text_input_tool = text_input_alloc();
    view_set_previous_callback(
        text_input_get_view(app->text_input_tool), lan_tester_nav_back_tool);
    view_dispatcher_add_view(
        app->view_dispatcher, LanTesterViewToolInput, text_input_get_view(app->text_input_tool));

    /* Shared ByteInput for MAC address entry (WOL, MAC changer) */
    app->byte_input_tool = byte_input_alloc();

    /* Shared NumberInput for integer entry */
    app->number_input_tool = number_input_alloc();
    view_set_previous_callback(
        number_input_get_view(app->number_input_tool), lan_tester_nav_back_tool);
    view_dispatcher_add_view(
        app->view_dispatcher,
        LanTesterViewNumberInput,
        number_input_get_view(app->number_input_tool));

    /* IP Keyboard (shared custom view for all IP address inputs) */
    app->ip_keyboard = ip_keyboard_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, LanTesterViewIpKeyboard, ip_keyboard_get_view(app->ip_keyboard));

    /* Ping and DNS defaults set in settings_load() */

    /* Continuous Ping views */
    app->view_cont_ping = view_alloc();
    view_allocate_model(app->view_cont_ping, ViewModelTypeLocking, sizeof(ContPingViewModel));
    view_set_draw_callback(app->view_cont_ping, cont_ping_draw_callback);
    view_set_input_callback(app->view_cont_ping, cont_ping_input_callback);
    view_set_context(app->view_cont_ping, app);
    with_view_model(app->view_cont_ping, ContPingViewModel * vm, { vm->app = app; }, false);
    view_dispatcher_add_view(app->view_dispatcher, LanTesterViewContPing, app->view_cont_ping);

    /* Cont ping and port scan defaults set in settings_load() */
    app->port_scan_custom_start = 1;
    app->port_scan_custom_end = 1024;
    strncpy(app->port_scan_start_input, "1", sizeof(app->port_scan_start_input));
    strncpy(app->port_scan_end_input, "1024", sizeof(app->port_scan_end_input));

    /* ByteInput for MAC address entry (WOL, MAC changer) */
    view_set_previous_callback(
        byte_input_get_view(app->byte_input_tool), lan_tester_nav_back_tool);
    view_dispatcher_add_view(
        app->view_dispatcher,
        LanTesterViewToolByteInput,
        byte_input_get_view(app->byte_input_tool));

    /* ETH Bridge view (custom View with draw_callback, no TextBox) */
    app->view_bridge = view_alloc();
    view_allocate_model(app->view_bridge, ViewModelTypeLocking, sizeof(BridgeViewModel));
    view_set_draw_callback(app->view_bridge, bridge_draw_callback);
    view_set_input_callback(app->view_bridge, bridge_input_callback);
    view_set_context(app->view_bridge, app);
    view_set_previous_callback(app->view_bridge, lan_tester_nav_back_traffic);
    with_view_model(
        app->view_bridge,
        BridgeViewModel * vm,
        {
            vm->app = app;
            vm->status_line = "Starting...";
        },
        false);
    view_dispatcher_add_view(app->view_dispatcher, LanTesterViewEthBridge, app->view_bridge);
    app->bridge_state = malloc(sizeof(EthBridgeState));
    if(app->bridge_state) {
        memset(app->bridge_state, 0, sizeof(EthBridgeState));
    }

    /* Persistent worker thread — allocated once, reused for all tools.
     * Eliminates 4 KB alloc/free per tool launch (prevents heap fragmentation). */
    app->worker_thread = furi_thread_alloc_ex("LanWorker", 4 * 1024, lan_tester_worker_fn, app);

    /* PXE Server views */

    /* PXE text defaults + IP arrays set in settings_load(), just set DHCP flag */
    app->pxe_dhcp_enabled = true;

    /* PXE Help TextBox (unused, kept for view ID compatibility) */
    app->text_box_pxe_help = text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, LanTesterViewPxeHelp, text_box_get_view(app->text_box_pxe_help));

    /* PXE Settings (VariableItemList) — reordered: Start first */
    app->pxe_settings_list = variable_item_list_alloc();
    view_set_previous_callback(
        variable_item_list_get_view(app->pxe_settings_list), lan_tester_nav_back_utilities);
    view_dispatcher_add_view(
        app->view_dispatcher,
        LanTesterViewPxeSettings,
        variable_item_list_get_view(app->pxe_settings_list));

    /* Index 0: Start PXE */
    variable_item_list_add(app->pxe_settings_list, ">>> Start PXE <<<", 0, NULL, app);

    /* Index 1: DHCP Server toggle */
    app->pxe_item_dhcp = variable_item_list_add(
        app->pxe_settings_list, "DHCP Server", 2, pxe_dhcp_toggle_callback, app);
    variable_item_set_current_value_index(app->pxe_item_dhcp, 1); /* ON by default */
    variable_item_set_current_value_text(app->pxe_item_dhcp, "ON");

    /* Index 2: Boot File (cycling if multiple files detected) */
    app->pxe_item_boot =
        variable_item_list_add(app->pxe_settings_list, "Boot File", 0, pxe_boot_file_changed, app);
    variable_item_set_current_value_text(app->pxe_item_boot, "Detecting...");

    /* Index 3: Server IP */
    app->pxe_item_sip = variable_item_list_add(app->pxe_settings_list, "Server IP", 0, NULL, app);
    variable_item_set_current_value_text(app->pxe_item_sip, app->pxe_server_ip_input);

    /* Index 4: Client IP */
    app->pxe_item_cip = variable_item_list_add(app->pxe_settings_list, "Client IP", 0, NULL, app);
    variable_item_set_current_value_text(app->pxe_item_cip, app->pxe_client_ip_input);

    /* Index 5: Subnet Mask */
    app->pxe_item_sub =
        variable_item_list_add(app->pxe_settings_list, "Subnet Mask", 0, NULL, app);
    variable_item_set_current_value_text(app->pxe_item_sub, app->pxe_subnet_input);

    /* Index 6: Download Boot Files */
    variable_item_list_add(app->pxe_settings_list, "Download Files", 0, NULL, app);

    variable_item_list_set_enter_callback(
        app->pxe_settings_list, pxe_settings_enter_callback, app);

    /* File Manager views */
    /* Packet Capture view */
    app->view_packet_capture = view_alloc();
    view_allocate_model(
        app->view_packet_capture, ViewModelTypeLocking, sizeof(PacketCaptureViewModel));
    view_set_draw_callback(app->view_packet_capture, packet_capture_draw_callback);
    view_set_input_callback(app->view_packet_capture, packet_capture_input_callback);
    view_set_context(app->view_packet_capture, app);
    view_set_previous_callback(app->view_packet_capture, lan_tester_nav_back_traffic);
    with_view_model(
        app->view_packet_capture, PacketCaptureViewModel * vm, { vm->app = app; }, false);
    view_dispatcher_add_view(
        app->view_dispatcher, LanTesterViewPacketCapture, app->view_packet_capture);
    memset(&app->pcap_state, 0, sizeof(app->pcap_state));

    /* Host List / Host Actions submenus */
    app->submenu_host_list = submenu_alloc();
    view_set_previous_callback(
        submenu_get_view(app->submenu_host_list), lan_tester_nav_back_host_list);
    view_dispatcher_add_view(
        app->view_dispatcher, LanTesterViewHostList, submenu_get_view(app->submenu_host_list));

    app->submenu_host_actions = submenu_alloc();
    view_set_previous_callback(
        submenu_get_view(app->submenu_host_actions), lan_tester_nav_back_host_actions);
    view_dispatcher_add_view(
        app->view_dispatcher,
        LanTesterViewHostActions,
        submenu_get_view(app->submenu_host_actions));

    app->discovered_host_count = 0;
    app->host_list_page = 0;

    /* Traceroute, ping sweep defaults set in settings_load() */

    /* mDNS/SSDP Discovery view */
    /* Auto Test view */
    app->autotest_text = furi_string_alloc();
    app->text_box_autotest = text_box_alloc();
    text_box_set_font(app->text_box_autotest, TextBoxFontText);
    view_set_previous_callback(
        text_box_get_view(app->text_box_autotest), lan_tester_nav_back_autotest);
    view_dispatcher_add_view(
        app->view_dispatcher, LanTesterViewAutoTest, text_box_get_view(app->text_box_autotest));

    /* Shared result view for all new analysis tools (saves ~12 TextBox allocs) */
    app->tool_back_view = LanTesterViewMainMenu;

    /* Tool input defaults set in settings_load() */
    app->vlan_hop_custom = false;
    app->autotest_running = false;

    /* History views */
    app->submenu_history = submenu_alloc();
    view_set_previous_callback(
        submenu_get_view(app->submenu_history), lan_tester_navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, LanTesterViewHistory, submenu_get_view(app->submenu_history));
    app->history_state = NULL;

    /* STP/VLAN Detection view */
    /* About view */
    app->text_box_about = text_box_alloc();
    text_box_set_font(app->text_box_about, TextBoxFontText);
    text_box_set_text(
        app->text_box_about,
        "[LAN Tester]\n"
        "Ethernet analyzer &\n"
        "security toolkit for\n"
        "Flipper Zero + W5500.\n"
        "34 tools: scan, ping,\n"
        "SNMP, DHCP, LLDP/CDP,\n"
        "802.1X, VLAN, IPMI,\n"
        "TFTP, NTP,\n"
        "PXE boot/download,\n"
        "rogue DHCP/RA detect.\n"
        "v2.8.0 | by dok2d\n"
        "github.com/dok2d/\n"
        "fz-W5500-lan-analyse\n");
    view_set_previous_callback(
        text_box_get_view(app->text_box_about), lan_tester_nav_back_settings);
    view_dispatcher_add_view(
        app->view_dispatcher, LanTesterViewAbout, text_box_get_view(app->text_box_about));

    /* Settings view (VariableItemList) */
    app->settings_list = variable_item_list_alloc();
    view_set_previous_callback(
        variable_item_list_get_view(app->settings_list), lan_tester_navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher,
        LanTesterViewSettings,
        variable_item_list_get_view(app->settings_list));

    VariableItem* item_autosave = variable_item_list_add(
        app->settings_list, "Auto-save results", 2, settings_autosave_changed, app);
    VariableItem* item_sound = variable_item_list_add(
        app->settings_list, "Sound & vibro", 2, settings_sound_changed, app);

    /* Custom DNS toggle (index 2) */
    VariableItem* item_dns_custom = variable_item_list_add(
        app->settings_list, "Custom DNS", 2, settings_dns_custom_changed, app);

    /* Custom DNS server IP (index 3) — opens ip_keyboard on OK press */
    VariableItem* item_dns_ip =
        variable_item_list_add(app->settings_list, "DNS Server", 0, NULL, app);
    variable_item_set_current_value_text(item_dns_ip, app->dns_custom_ip_input);

    /* Ping count (index 4) — 1..100 */
    VariableItem* item_ping_count = variable_item_list_add(
        app->settings_list, "Ping Count", 100, settings_ping_count_changed, app);

    /* Ping timeout (index 5) — 500..10000 step 500 */
    VariableItem* item_ping_timeout = variable_item_list_add(
        app->settings_list, "Ping Timeout ms", 20, settings_ping_timeout_changed, app);

    /* Continuous ping interval (index 6) — 200..5000 step 200 */
    VariableItem* item_ping_interval = variable_item_list_add(
        app->settings_list, "Cont.Ping Int ms", 25, settings_ping_interval_changed, app);

    /* "Clear History" — no value cycling, action on OK press (index 7) */
    VariableItem* item_clear =
        variable_item_list_add(app->settings_list, "Clear History", 0, NULL, app);
    variable_item_set_current_value_text(item_clear, "Press OK");

    /* MAC Changer — opens byte input for MAC address */
    variable_item_list_add(app->settings_list, "MAC Changer", 0, NULL, app);

    /* AutoTest DNS host — opens text input */
    VariableItem* item_at_dns =
        variable_item_list_add(app->settings_list, "AT DNS host", 0, NULL, app);
    variable_item_set_current_value_text(item_at_dns, app->autotest_dns_host);

    /* AutoTest LLDP wait — 10/20/30/60 seconds */
    VariableItem* item_at_lldp = variable_item_list_add(
        app->settings_list,
        "AT LLDP wait",
        AUTOTEST_LLDP_WAIT_COUNT,
        settings_autotest_lldp_wait_changed,
        app);

    /* AutoTest ARP scan — On/Off */
    VariableItem* item_at_arp = variable_item_list_add(
        app->settings_list, "AT ARP scan", 2, settings_autotest_arp_changed, app);

    /* About — last item in Settings */
    variable_item_list_add(app->settings_list, "About", 0, NULL, app);

    variable_item_list_set_enter_callback(app->settings_list, settings_enter_callback, app);

    /* Load settings from SD */
    lan_tester_settings_load(app);
    variable_item_set_current_value_index(item_autosave, app->setting_autosave ? 1 : 0);
    variable_item_set_current_value_text(
        item_autosave, setting_onoff[app->setting_autosave ? 1 : 0]);
    variable_item_set_current_value_index(item_sound, app->setting_sound ? 1 : 0);
    variable_item_set_current_value_text(item_sound, setting_onoff[app->setting_sound ? 1 : 0]);
    variable_item_set_current_value_index(item_dns_custom, app->dns_custom_enabled ? 1 : 0);
    variable_item_set_current_value_text(
        item_dns_custom, setting_onoff[app->dns_custom_enabled ? 1 : 0]);
    variable_item_set_current_value_text(item_dns_ip, app->dns_custom_ip_input);

    /* Ping count: index = count - 1 */
    variable_item_set_current_value_index(item_ping_count, app->ping_count - 1);
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", app->ping_count);
        variable_item_set_current_value_text(item_ping_count, buf);
    }
    /* Ping timeout: index = timeout/500 - 1 */
    variable_item_set_current_value_index(item_ping_timeout, app->ping_timeout_ms / 500 - 1);
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", app->ping_timeout_ms);
        variable_item_set_current_value_text(item_ping_timeout, buf);
    }
    /* Ping interval: index = interval/200 - 1 */
    variable_item_set_current_value_index(item_ping_interval, app->ping_interval_ms / 200 - 1);
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", app->ping_interval_ms);
        variable_item_set_current_value_text(item_ping_interval, buf);
    }

    /* AutoTest LLDP wait: find matching index */
    {
        uint8_t lldp_idx = 2; /* default 30s = index 2 */
        for(uint8_t i = 0; i < AUTOTEST_LLDP_WAIT_COUNT; i++) {
            if(autotest_lldp_wait_options[i] == app->autotest_lldp_wait_s) {
                lldp_idx = i;
                break;
            }
        }
        variable_item_set_current_value_index(item_at_lldp, lldp_idx);
        char buf[8];
        snprintf(buf, sizeof(buf), "%d s", autotest_lldp_wait_options[lldp_idx]);
        variable_item_set_current_value_text(item_at_lldp, buf);
    }
    /* AutoTest ARP scan */
    variable_item_set_current_value_index(item_at_arp, app->autotest_arp_enabled ? 1 : 0);
    variable_item_set_current_value_text(
        item_at_arp, setting_onoff[app->autotest_arp_enabled ? 1 : 0]);
    /* AutoTest DNS host — already set from load */
    variable_item_set_current_value_text(item_at_dns, app->autotest_dns_host);

    /* MAC loaded in settings_load() (with mac.conf backward compat).
     * If still zero (fresh install), generate and save immediately. */
    if(!(app->mac_addr[0] | app->mac_addr[1] | app->mac_addr[2] | app->mac_addr[3] |
         app->mac_addr[4] | app->mac_addr[5])) {
        lan_tester_generate_default_mac(app->mac_addr);
        lan_tester_settings_save(app);
        FURI_LOG_I(TAG, "Generated and saved new unique MAC");
    }

    return app;
}

static void lan_tester_app_free(LanTesterApp* app) {
    furi_assert(app);

    /* Save all settings + last-used targets + MAC on exit */
    lan_tester_settings_save(app);

    /* Stop and free worker thread */
    lan_tester_worker_stop(app);
    if(app->worker_thread) {
        furi_thread_free(app->worker_thread);
        app->worker_thread = NULL;
    }

    /* Remove and free views */
    view_dispatcher_remove_view(app->view_dispatcher, LanTesterViewMainMenu);
    view_dispatcher_remove_view(app->view_dispatcher, LanTesterViewToolResult);
    view_dispatcher_remove_view(app->view_dispatcher, LanTesterViewToolInput);
    view_dispatcher_remove_view(app->view_dispatcher, LanTesterViewContPing);
    view_dispatcher_remove_view(app->view_dispatcher, LanTesterViewIpKeyboard);
    view_dispatcher_remove_view(app->view_dispatcher, LanTesterViewHistory);
    view_dispatcher_remove_view(app->view_dispatcher, LanTesterViewAbout);
    view_dispatcher_remove_view(app->view_dispatcher, LanTesterViewCatPortInfo);
    view_dispatcher_remove_view(app->view_dispatcher, LanTesterViewCatScan);
    view_dispatcher_remove_view(app->view_dispatcher, LanTesterViewCatDiag);
    view_dispatcher_remove_view(app->view_dispatcher, LanTesterViewCatTraffic);
    view_dispatcher_remove_view(app->view_dispatcher, LanTesterViewCatUtilities);
    view_dispatcher_remove_view(app->view_dispatcher, LanTesterViewPortScanMode);
    view_dispatcher_remove_view(app->view_dispatcher, LanTesterViewSettings);
    view_dispatcher_remove_view(app->view_dispatcher, LanTesterViewEthBridge);
    view_dispatcher_remove_view(app->view_dispatcher, LanTesterViewPxeSettings);
    view_dispatcher_remove_view(app->view_dispatcher, LanTesterViewPxeHelp);
    view_dispatcher_remove_view(app->view_dispatcher, LanTesterViewPacketCapture);
    view_dispatcher_remove_view(app->view_dispatcher, LanTesterViewHostList);
    view_dispatcher_remove_view(app->view_dispatcher, LanTesterViewHostActions);
    view_dispatcher_remove_view(app->view_dispatcher, LanTesterViewAutoTest);
    view_dispatcher_remove_view(app->view_dispatcher, LanTesterViewCatSecurity);
    view_dispatcher_remove_view(app->view_dispatcher, LanTesterViewToolByteInput);
    view_dispatcher_remove_view(app->view_dispatcher, LanTesterViewNumberInput);

    submenu_free(app->submenu);
    submenu_free(app->submenu_cat_portinfo);
    submenu_free(app->submenu_cat_scan);
    submenu_free(app->submenu_cat_diag);
    submenu_free(app->submenu_cat_traffic);
    submenu_free(app->submenu_cat_utilities);
    submenu_free(app->submenu_cat_security);
    submenu_free(app->submenu_port_scan_mode);
    variable_item_list_free(app->settings_list);
    text_box_free(app->text_box_tool);
    text_input_free(app->text_input_tool);
    byte_input_free(app->byte_input_tool);
    number_input_free(app->number_input_tool);
    view_free(app->view_cont_ping);
    view_free(app->view_bridge);
    view_free(app->view_packet_capture);
    submenu_free(app->submenu_host_list);
    submenu_free(app->submenu_host_actions);
    if(app->bridge_state) free(app->bridge_state);
    text_box_free(app->text_box_pxe_help);
    variable_item_list_free(app->pxe_settings_list);
    ip_keyboard_free(app->ip_keyboard);
    submenu_free(app->submenu_history);
    if(app->history_state) free(app->history_state);
    text_box_free(app->text_box_about);
    text_box_free(app->text_box_autotest);

    view_dispatcher_free(app->view_dispatcher);

    /* Free text buffers */
    furi_string_free(app->tool_text);
    /* history_text removed — history now uses submenu */
    furi_string_free(app->autotest_text);

    /* Stop and free DHCP timer */
    furi_timer_stop(app->dhcp_timer);
    furi_timer_free(app->dhcp_timer);

    /* Deinit W5500 — always call to release SPI bus and OTG power,
     * even if init was partial (e.g. chip_init failed after SPI acquired) */
    w5500_hal_deinit();

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    free(app->frame_buf);
    g_app = NULL;
    free(app);
}

/* ==================== Navigation callbacks ==================== */

/* Update main menu header with link status */
static void lan_tester_update_menu_header(LanTesterApp* app) {
    if(app->w5500_initialized) {
        bool link = w5500_hal_get_link_status();
        if(link) {
            uint8_t speed = 0, duplex = 0;
            bool up = false;
            w5500_hal_get_phy_info(&up, &speed, &duplex);
            submenu_set_header(
                app->submenu,
                speed ? (duplex ? "LAN [UP 100M FD]" : "LAN [UP 100M HD]") :
                        (duplex ? "LAN [UP 10M FD]" : "LAN [UP 10M HD]"));
        } else {
            submenu_set_header(app->submenu, "LAN [NO LINK]");
        }
    } else {
        submenu_set_header(app->submenu, "LAN Tester");
    }
}

static uint32_t lan_tester_navigation_exit_callback(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

/* Stop worker helper used by all back-navigation callbacks */
static void lan_tester_stop_worker_on_back(void) {
    if(g_app) {
        if(g_app->worker_thread &&
           furi_thread_get_state(g_app->worker_thread) != FuriThreadStateStopped) {
            submenu_set_header(g_app->submenu, "Stopping...");
        }
        g_app->worker_running = false;
        /* Force-close HTTP socket to unblock WIZnet's blocking send()/recv().
         * Without this, the worker thread hangs in send()'s internal while(1)
         * loop waiting for TX buffer free space, and furi_thread_join() blocks
         * forever causing the Flipper to freeze. Socket 3 is shared across
         * multiple tools that never run concurrently, so this is safe. */
        if(g_app->worker_op == LanTesterMenuItemFileManager) {
            close(FILEMGR_HTTP_SOCKET);
        }
        lan_tester_update_menu_header(g_app);
    }
}

static uint32_t lan_tester_navigation_submenu_callback(void* context) {
    UNUSED(context);
    lan_tester_stop_worker_on_back();
    return LanTesterViewMainMenu;
}

static uint32_t lan_tester_nav_back_autotest(void* context) {
    UNUSED(context);
    if(g_app) {
        g_app->autotest_running = false;
        /* worker_running is NOT touched here — worker loop checks autotest_running */
    }
    lan_tester_stop_worker_on_back();
    return LanTesterViewMainMenu;
}

static uint32_t lan_tester_nav_back_portinfo(void* context) {
    UNUSED(context);
    lan_tester_stop_worker_on_back();
    return LanTesterViewCatPortInfo;
}

static uint32_t lan_tester_nav_back_scan(void* context) {
    UNUSED(context);
    lan_tester_stop_worker_on_back();
    return LanTesterViewCatScan;
}

static uint32_t lan_tester_nav_back_diag(void* context) {
    UNUSED(context);
    lan_tester_stop_worker_on_back();
    return LanTesterViewCatDiag;
}

static uint32_t lan_tester_nav_back_traffic(void* context) {
    UNUSED(context);
    lan_tester_stop_worker_on_back();
    return LanTesterViewCatTraffic;
}

static uint32_t lan_tester_nav_back_utilities(void* context) {
    UNUSED(context);
    lan_tester_stop_worker_on_back();
    return LanTesterViewCatUtilities;
}

static uint32_t lan_tester_nav_back_port_scan_mode(void* context) {
    UNUSED(context);
    lan_tester_stop_worker_on_back();
    return LanTesterViewPortScanMode;
}

/* ==================== Worker thread ==================== */

/* Navigation event callback: stop worker on app exit */
static bool lan_tester_nav_event_cb(void* context) {
    LanTesterApp* app = context;
    /* Stop any running worker before exiting */
    lan_tester_worker_stop(app);
    return false; /* Allow app to exit */
}

static void lan_tester_ping_sweep_input_callback(void* context);

static bool lan_tester_custom_event_cb(void* context, uint32_t event) {
    LanTesterApp* app = context;

    if(event == CUSTOM_EVENT_SHOW_HOST_LIST) {
        if(app->discovered_host_count > 0) {
            lan_tester_populate_host_list(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewHostList);
        }
        return true;
    }

    if(event == CUSTOM_EVENT_CONT_PING_BACK) {
        /* Worker is stopping (worker_running = false). Wait for it to finish,
         * then navigate back to Diagnostics submenu. */
        lan_tester_worker_stop(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewCatDiag);
        return true;
    }

    if(event == CUSTOM_EVENT_PING_SWEEP_READY) {
        /* DHCP detection done — show input with pre-filled CIDR */
        ip_keyboard_setup(
            app->ip_keyboard,
            "Scan range (CIDR):",
            app->ping_sweep_ip_input,
            true,
            lan_tester_ping_sweep_input_callback,
            app,
            app->ping_sweep_ip_input,
            sizeof(app->ping_sweep_ip_input),
            lan_tester_nav_back_scan);
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewIpKeyboard);
        return true;
    }

    return false;
}

static int32_t lan_tester_worker_fn(void* context) {
    LanTesterApp* app = context;

    /* Dispatch to the appropriate operation */
    switch(app->worker_op) {
    case LanTesterMenuItemAutoTest:
        lan_tester_do_autotest(app);
        break;
    case LanTesterMenuItemLinkInfo:
        lan_tester_do_link_info(app);
        lan_tester_update_view(app->text_box_tool, app->tool_text);
        break;
    case LanTesterMenuItemLldpCdp:
        lan_tester_do_lldp_cdp(app);
        lan_tester_update_view(app->text_box_tool, app->tool_text);
        break;
    case LanTesterMenuItemArpScan:
        lan_tester_do_arp_scan(app);
        lan_tester_update_view(app->text_box_tool, app->tool_text);
        break;
    case LanTesterMenuItemDhcpAnalyze:
        lan_tester_do_dhcp_analyze(app);
        lan_tester_update_view(app->text_box_tool, app->tool_text);
        break;
    case LanTesterMenuItemPing:
        lan_tester_do_ping(app);
        lan_tester_update_view(app->text_box_tool, app->tool_text);
        break;
    case LanTesterMenuItemStats:
        lan_tester_do_stats(app);
        lan_tester_update_view(app->text_box_tool, app->tool_text);
        break;
    case LanTesterMenuItemDnsLookup:
        lan_tester_do_dns_lookup(app);
        lan_tester_update_view(app->text_box_tool, app->tool_text);
        break;
    case LanTesterMenuItemWol:
        lan_tester_do_wol(app);
        lan_tester_update_view(app->text_box_tool, app->tool_text);
        break;
    case LanTesterMenuItemContPing:
        lan_tester_do_cont_ping(app);
        break; /* Uses custom view, not TextBox */
    case LanTesterMenuItemPortScan:
        lan_tester_do_port_scan(app);
        lan_tester_update_view(app->text_box_tool, app->tool_text);
        break;
    case LanTesterMenuItemMacChanger:
        lan_tester_do_mac_changer(app);
        lan_tester_update_view(app->text_box_tool, app->tool_text);
        break;
    case LanTesterMenuItemTraceroute:
        lan_tester_do_traceroute(app);
        lan_tester_update_view(app->text_box_tool, app->tool_text);
        break;
    case LanTesterMenuItemPingSweep:
        lan_tester_do_ping_sweep(app);
        lan_tester_update_view(app->text_box_tool, app->tool_text);
        break;
    case LanTesterMenuItemDiscovery:
        lan_tester_do_discovery(app);
        lan_tester_update_view(app->text_box_tool, app->tool_text);
        break;
    case LanTesterMenuItemStpVlan:
        lan_tester_do_stp_vlan(app);
        lan_tester_update_view(app->text_box_tool, app->tool_text);
        break;
    case LanTesterMenuItemEthBridge:
        lan_tester_do_eth_bridge(app);
        break; /* Uses custom view, not TextBox */
    case LanTesterMenuItemPxeServer:
        lan_tester_do_pxe_server(app);
        break;
    case LanTesterMenuItemPxeDownload:
        lan_tester_do_pxe_download(app);
        lan_tester_update_view(app->text_box_tool, app->tool_text);
        break;
    case LanTesterMenuItemFileManager:
        lan_tester_do_file_manager(app);
        break;
    case LanTesterMenuItemPacketCapture:
        lan_tester_do_packet_capture(app);
        break;
    case LanTesterMenuItemSnmpGet:
        lan_tester_do_snmp_get(app);
        lan_tester_update_view(app->text_box_tool, app->tool_text);
        break;
    case LanTesterMenuItemNtpDiag:
        lan_tester_do_ntp_diag(app);
        lan_tester_update_view(app->text_box_tool, app->tool_text);
        break;
    case LanTesterMenuItemNetbiosQuery:
        lan_tester_do_netbios_query(app);
        lan_tester_update_view(app->text_box_tool, app->tool_text);
        break;
    case LanTesterMenuItemDnsPoisonCheck:
        lan_tester_do_dns_poison_check(app);
        lan_tester_update_view(app->text_box_tool, app->tool_text);
        break;
    case LanTesterMenuItemArpWatch:
        lan_tester_do_arp_watch(app);
        lan_tester_update_view(app->text_box_tool, app->tool_text);
        break;
    case LanTesterMenuItemRogueDhcp:
        lan_tester_do_rogue_dhcp(app);
        lan_tester_update_view(app->text_box_tool, app->tool_text);
        break;
    case LanTesterMenuItemRogueRa:
        lan_tester_do_rogue_ra(app);
        lan_tester_update_view(app->text_box_tool, app->tool_text);
        break;
    case LanTesterMenuItemDhcpFingerprint:
        lan_tester_do_dhcp_fingerprint(app);
        lan_tester_update_view(app->text_box_tool, app->tool_text);
        break;
    case LanTesterMenuItemEapolProbe:
        lan_tester_do_eapol_probe(app);
        lan_tester_update_view(app->text_box_tool, app->tool_text);
        break;
    case LanTesterMenuItemVlanHopTop10:
    case LanTesterMenuItemVlanHopCustom:
        lan_tester_do_vlan_hop(app);
        lan_tester_update_view(app->text_box_tool, app->tool_text);
        break;
    case LanTesterMenuItemTftpClient:
        lan_tester_do_tftp_client(app);
        lan_tester_update_view(app->text_box_tool, app->tool_text);
        break;
    case LanTesterMenuItemIpmiClient:
        lan_tester_do_ipmi_client(app);
        lan_tester_update_view(app->text_box_tool, app->tool_text);
        break;
    case LanTesterMenuItemHistory:
        break; /* History uses synchronous submenu, no worker needed */
    case WORKER_OP_PING_SWEEP_DETECT:
        lan_tester_do_ping_sweep_detect(app);
        break;
    default:
        break;
    }
    return 0;
}

static void lan_tester_worker_stop(LanTesterApp* app) {
    if(app->worker_thread) {
        app->worker_running = false;
        /* Force-close sockets to unblock blocking send/recv */
        if(app->worker_op == LanTesterMenuItemFileManager) {
            close(FILEMGR_HTTP_SOCKET);
        }
        if(app->worker_op == LanTesterMenuItemPxeDownload) {
            close(HTTP_CLIENT_SOCKET);
        }
        furi_thread_join(app->worker_thread);
    }
}

/* Free leftover tool state to reclaim heap before launching a new tool.
 * Called from lan_tester_worker_start() so every tool starts with maximum
 * available memory and minimal fragmentation. */
static void lan_tester_cleanup_tool_state(LanTesterApp* app) {
    if(app->history_state) {
        free(app->history_state);
        app->history_state = NULL;
    }
    if(app->ping_graph) {
        free(app->ping_graph);
        app->ping_graph = NULL;
    }
    furi_string_reset(app->tool_text);
}

static void lan_tester_worker_start(LanTesterApp* app, uint32_t op, LanTesterView result_view) {
    /* Wait for previous worker to finish */
    if(app->worker_thread && furi_thread_get_state(app->worker_thread) != FuriThreadStateStopped) {
        app->worker_running = false;
        if(app->worker_op == LanTesterMenuItemFileManager) {
            close(FILEMGR_HTTP_SOCKET);
        }
        if(app->worker_op == LanTesterMenuItemPxeDownload) {
            close(HTTP_CLIENT_SOCKET);
        }
        furi_thread_join(app->worker_thread);
    }

    /* Free leftover state from previous tools */
    lan_tester_cleanup_tool_state(app);

    app->worker_op = op;
    app->worker_running = true;

    /* Switch to result view BEFORE starting thread */
    view_dispatcher_switch_to_view(app->view_dispatcher, result_view);

    /* Reuse persistent worker thread (allocated once in app_alloc, 4 KB stack) */
    furi_thread_start(app->worker_thread);
}

/* ==================== W5500 initialization helper ==================== */

static bool lan_tester_ensure_w5500(LanTesterApp* app) {
    if(app->w5500_initialized) return true;

    FURI_LOG_I(TAG, "Initializing W5500...");

    if(!w5500_hal_init()) {
        FURI_LOG_E(TAG, "W5500 HAL init failed");
        return false;
    }

    w5500_hal_hw_reset();

    if(!w5500_hal_chip_init()) {
        FURI_LOG_E(TAG, "W5500 chip init failed");
        w5500_hal_deinit();
        return false;
    }

    if(!w5500_hal_check_version()) {
        FURI_LOG_E(TAG, "W5500 not found (bad VERSIONR)");
        w5500_hal_deinit();
        return false;
    }

    /* Lazy-allocate frame buffer on first W5500 use */
    if(!app->frame_buf) {
        app->frame_buf = malloc(FRAME_BUF_SIZE);
        if(!app->frame_buf) {
            FURI_LOG_E(TAG, "frame_buf malloc failed");
            w5500_hal_deinit();
            return false;
        }
    }

    w5500_hal_set_mac(app->mac_addr);
    app->w5500_initialized = true;

    FURI_LOG_I(TAG, "W5500 initialized successfully");
    return true;
}

/* ==================== Shared DHCP helper ==================== */

/**
 * Ensure we have a valid DHCP lease. Returns true if dhcp_valid.
 * Uses cached result if available; only runs DHCP once per session
 * (or after link state change).
 */
static bool lan_tester_ensure_dhcp(LanTesterApp* app) {
    if(!lan_tester_ensure_w5500(app)) {
        if(app->setting_sound) notification_message(app->notifications, &sequence_error);
        return false;
    }

    if(!w5500_hal_get_link_status()) {
        if(app->setting_sound) notification_message(app->notifications, &sequence_error);
        return false;
    }

    /* Use cached DHCP if available */
    if(app->dhcp_valid) {
        /* Re-apply cached network config to W5500 */
        wiz_NetInfo net_info;
        wizchip_getnetinfo(&net_info);
        memcpy(net_info.ip, app->dhcp_ip, 4);
        memcpy(net_info.sn, app->dhcp_mask, 4);
        memcpy(net_info.gw, app->dhcp_gw, 4);
        memcpy(net_info.dns, app->dhcp_dns, 4);
        net_info.dhcp = NETINFO_DHCP;
        wizchip_setnetinfo(&net_info);
        return true;
    }

    /* DHCP needs its own buffer — WIZnet library keeps the pointer for DHCP_run().
     * Cannot share with frame_buf which is used for ping/ARP/MACRAW. */
    uint8_t* dhcp_buffer = malloc(576);
    if(!dhcp_buffer) return false;

    wiz_NetInfo net_info;
    wizchip_getnetinfo(&net_info);
    net_info.dhcp = NETINFO_DHCP;
    memset(net_info.ip, 0, 4);
    memset(net_info.sn, 0, 4);
    memset(net_info.gw, 0, 4);
    wizchip_setnetinfo(&net_info);

    DHCP_init(W5500_DHCP_SOCKET, dhcp_buffer);

    bool got_ip = false;
    uint32_t dhcp_start = furi_get_tick();
    while(furi_get_tick() - dhcp_start < 15000 && app->worker_running) {
        uint8_t dhcp_ret = DHCP_run();
        if(dhcp_ret == DHCP_IP_LEASED || dhcp_ret == DHCP_IP_ASSIGN ||
           dhcp_ret == DHCP_IP_CHANGED) {
            getIPfromDHCP(net_info.ip);
            getSNfromDHCP(net_info.sn);
            getGWfromDHCP(net_info.gw);
            getDNSfromDHCP(net_info.dns);
            net_info.dhcp = NETINFO_DHCP;
            wizchip_setnetinfo(&net_info);
            got_ip = true;

            memcpy(app->dhcp_ip, net_info.ip, 4);
            memcpy(app->dhcp_mask, net_info.sn, 4);
            memcpy(app->dhcp_gw, net_info.gw, 4);
            memcpy(app->dhcp_dns, net_info.dns, 4);
            app->dhcp_valid = true;
            break;
        }
        if(dhcp_ret == DHCP_FAILED) break;
        furi_delay_ms(10);
    }
    DHCP_stop();
    free(dhcp_buffer);

    if(!got_ip && app->setting_sound) {
        notification_message(app->notifications, &sequence_error);
    }

    return got_ip;
}

/* ==================== Common init checks ==================== */

/**
 * Reset tool_text + ensure W5500. Sets error message on failure.
 */
static bool lan_tester_check_w5500(LanTesterApp* app) {
    furi_string_reset(app->tool_text);
    if(!lan_tester_ensure_w5500(app)) {
        furi_string_set(app->tool_text, "W5500 Not Found!\n");
        return false;
    }
    return true;
}

/**
 * Ensure DHCP (includes W5500+link). Sets diagnostic error in tool_text on failure.
 * Does NOT reset tool_text — caller may have set a "loading" message before this.
 */
static bool lan_tester_check_dhcp(LanTesterApp* app) {
    if(!lan_tester_ensure_dhcp(app)) {
        furi_string_set(
            app->tool_text,
            !app->w5500_initialized      ? "W5500 Not Found!\n" :
            !w5500_hal_get_link_status() ? "No Link!\nConnect cable.\n" :
                                           "DHCP failed.\n");
        return false;
    }
    return true;
}

/* ==================== ASCII progress bar ==================== */

/**
 * Generate a fixed-width progress bar: "######======45%"
 * bar_len: number of #/= chars (pick to fit remaining line width).
 * buf must be at least bar_len + 4 bytes.
 */
static void lan_tester_progress_bar(char* buf, uint8_t bar_len, uint16_t current, uint16_t total) {
    if(total == 0) total = 1;
    uint8_t pct = (uint8_t)((current * 100) / total);
    if(pct > 99) pct = 99;
    uint8_t filled = (uint8_t)((uint32_t)current * bar_len / total);
    if(filled > bar_len) filled = bar_len;
    for(uint8_t i = 0; i < bar_len; i++) {
        buf[i] = (i < filled) ? '#' : '=';
    }
    snprintf(buf + bar_len, 5, "%02d%%", pct);
}

/* ==================== View update helpers ==================== */

static void lan_tester_show_view(
    LanTesterApp* app,
    TextBox* tb,
    LanTesterView view,
    FuriString* text,
    const char* initial) {
    furi_string_set(text, initial);
    text_box_set_text(tb, furi_string_get_cstr(text));
    view_dispatcher_switch_to_view(app->view_dispatcher, view);
    furi_delay_ms(1);
}

static void lan_tester_update_view(TextBox* tb, FuriString* text) {
    text_box_set_text(tb, furi_string_get_cstr(text));
    furi_delay_ms(1);
}

/* ==================== Ping IP input callback ==================== */

static bool lan_tester_parse_ip(const char* str, uint8_t ip[4]) {
    unsigned int a, b, c, d;
    if(sscanf(str, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) return false;
    if(a > 255 || b > 255 || c > 255 || d > 255) return false;
    ip[0] = (uint8_t)a;
    ip[1] = (uint8_t)b;
    ip[2] = (uint8_t)c;
    ip[3] = (uint8_t)d;
    return true;
}

static void lan_tester_ping_ip_input_callback(void* context) {
    LanTesterApp* app = context;
    furi_assert(app);

    if(lan_tester_parse_ip(app->ping_ip_input, app->ping_ip_custom)) {
        furi_string_set(app->tool_text, "Initializing...\n");
        text_box_set_text(app->text_box_tool, furi_string_get_cstr(app->tool_text));
        lan_tester_worker_start(app, LanTesterMenuItemPing, LanTesterViewToolResult);
    } else {
        furi_string_set(app->tool_text, "Invalid IP address!\n");
        text_box_set_text(app->text_box_tool, furi_string_get_cstr(app->tool_text));
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewToolResult);
    }
}

/* ==================== Continuous Ping IP input callback ==================== */

static void lan_tester_cont_ping_ip_input_callback(void* context) {
    LanTesterApp* app = context;
    furi_assert(app);

    if(!lan_tester_parse_ip(app->cont_ping_ip_input, app->cont_ping_target)) {
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewMainMenu);
        return;
    }

    lan_tester_worker_start(app, LanTesterMenuItemContPing, LanTesterViewContPing);
}

/* ==================== Traceroute IP input callback ==================== */

static void lan_tester_traceroute_input_callback(void* context) {
    LanTesterApp* app = context;
    furi_assert(app);

    /* Try parsing as IP first */
    if(lan_tester_parse_ip(app->traceroute_host_input, app->traceroute_target)) {
        app->traceroute_is_hostname = false;
    } else if(strlen(app->traceroute_host_input) > 0) {
        /* Treat as hostname — DNS resolve will happen in worker */
        app->traceroute_is_hostname = true;
    } else {
        furi_string_set(app->tool_text, "Empty input!\n");
        text_box_set_text(app->text_box_tool, furi_string_get_cstr(app->tool_text));
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewToolResult);
        return;
    }

    furi_string_set(app->tool_text, "Initializing...\n");
    text_box_set_text(app->text_box_tool, furi_string_get_cstr(app->tool_text));
    lan_tester_worker_start(app, LanTesterMenuItemTraceroute, LanTesterViewToolResult);
}

/* ==================== Port scan IP input callback ==================== */

static void lan_tester_port_scan_ip_input_callback(void* context) {
    LanTesterApp* app = context;
    furi_assert(app);

    if(!lan_tester_parse_ip(app->port_scan_ip_input, app->port_scan_target)) {
        furi_string_set(app->tool_text, "Invalid IP address!\n");
        text_box_set_text(app->text_box_tool, furi_string_get_cstr(app->tool_text));
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewToolResult);
        return;
    }

    furi_string_set(app->tool_text, "Initializing...\n");
    text_box_set_text(app->text_box_tool, furi_string_get_cstr(app->tool_text));
    lan_tester_worker_start(app, LanTesterMenuItemPortScan, LanTesterViewToolResult);
}

/* Custom port scan: step 3 — IP entered, start scan */
static void lan_tester_port_scan_custom_ip_callback(void* context) {
    LanTesterApp* app = context;
    furi_assert(app);

    if(!lan_tester_parse_ip(app->port_scan_ip_input, app->port_scan_target)) {
        furi_string_set(app->tool_text, "Invalid IP address!\n");
        text_box_set_text(app->text_box_tool, furi_string_get_cstr(app->tool_text));
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewToolResult);
        return;
    }

    app->port_scan_custom = true;
    furi_string_set(app->tool_text, "Initializing...\n");
    text_box_set_text(app->text_box_tool, furi_string_get_cstr(app->tool_text));
    lan_tester_worker_start(app, LanTesterMenuItemPortScan, LanTesterViewToolResult);
}

/* Custom port scan: step 2 — end port entered, ask for IP */
static void lan_tester_port_scan_end_callback(void* context) {
    LanTesterApp* app = context;
    furi_assert(app);

    int end = atoi(app->port_scan_end_input);
    if(end < 1 || end > 65535) end = 1024;
    app->port_scan_custom_end = (uint16_t)end;

    if(app->port_scan_custom_end < app->port_scan_custom_start) {
        app->port_scan_custom_end = app->port_scan_custom_start;
    }

    /* Now ask for IP */
    if(app->dhcp_valid &&
       (app->dhcp_gw[0] | app->dhcp_gw[1] | app->dhcp_gw[2] | app->dhcp_gw[3])) {
        snprintf(
            app->port_scan_ip_input,
            sizeof(app->port_scan_ip_input),
            "%d.%d.%d.%d",
            app->dhcp_gw[0],
            app->dhcp_gw[1],
            app->dhcp_gw[2],
            app->dhcp_gw[3]);
    }
    ip_keyboard_setup(
        app->ip_keyboard,
        "Target IP (Custom):",
        app->port_scan_ip_input,
        false,
        lan_tester_port_scan_custom_ip_callback,
        app,
        app->port_scan_ip_input,
        sizeof(app->port_scan_ip_input),
        lan_tester_nav_back_diag);
    view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewIpKeyboard);
}

/* Custom port scan: step 1 — start port entered, ask for end port */
static void lan_tester_port_scan_start_callback(void* context) {
    LanTesterApp* app = context;
    furi_assert(app);

    int start = atoi(app->port_scan_start_input);
    if(start < 1 || start > 65535) start = 1;
    app->port_scan_custom_start = (uint16_t)start;

    /* Ask for end port */
    text_input_reset(app->text_input_tool);
    text_input_set_header_text(app->text_input_tool, "End port (1-65535):");
    text_input_set_result_callback(
        app->text_input_tool,
        lan_tester_port_scan_end_callback,
        app,
        app->port_scan_end_input,
        sizeof(app->port_scan_end_input),
        false);
    view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewToolInput);
}

/* ==================== Ping sweep CIDR input callback ==================== */

static void lan_tester_ping_sweep_input_callback(void* context) {
    LanTesterApp* app = context;
    furi_assert(app);

    furi_string_set(app->tool_text, "Starting ping sweep...\n");
    text_box_set_text(app->text_box_tool, furi_string_get_cstr(app->tool_text));
    lan_tester_worker_start(app, LanTesterMenuItemPingSweep, LanTesterViewToolResult);
}

/* ==================== DNS hostname input callback ==================== */

static void lan_tester_dns_input_callback(void* context) {
    LanTesterApp* app = context;
    furi_assert(app);

    furi_string_set(app->tool_text, "Initializing...\n");
    text_box_set_text(app->text_box_tool, furi_string_get_cstr(app->tool_text));
    lan_tester_worker_start(app, LanTesterMenuItemDnsLookup, LanTesterViewToolResult);
}

/* ==================== MAC Changer input callback ==================== */

static void lan_tester_mac_changer_input_callback(void* context) {
    LanTesterApp* app = context;
    furi_assert(app);

    /* Apply the MAC from byte input */
    memcpy(app->mac_addr, app->mac_changer_input, 6);
    if(app->w5500_initialized) {
        w5500_hal_set_mac(app->mac_addr);
    }
    lan_tester_settings_save(app);

    /* Invalidate DHCP cache since MAC changed */
    app->dhcp_valid = false;

    char new_mac_str[18];
    pkt_format_mac(app->mac_addr, new_mac_str);

    furi_string_printf(
        app->tool_text,
        "MAC changed to:\n"
        "%s\n\n"
        "Saved to SD card.\n"
        "Full effect on next\n"
        "DHCP/reconnect.\n",
        new_mac_str);
    text_box_set_text(app->text_box_tool, furi_string_get_cstr(app->tool_text));
    view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewToolResult);
    if(app->setting_sound) notification_message(app->notifications, &sequence_success);
}

/* ==================== WoL MAC input callback ==================== */

static void lan_tester_wol_input_callback(void* context) {
    LanTesterApp* app = context;
    furi_assert(app);

    furi_string_set(app->tool_text, "Sending WoL...\n");
    text_box_set_text(app->text_box_tool, furi_string_get_cstr(app->tool_text));
    lan_tester_worker_start(app, LanTesterMenuItemWol, LanTesterViewToolResult);
}

/* Dynamic back callback for the shared tool result/input views */
static uint32_t lan_tester_nav_back_tool(void* context) {
    UNUSED(context);
    if(g_app && g_app->worker_thread &&
       furi_thread_get_state(g_app->worker_thread) != FuriThreadStateStopped) {
        /* Worker still running: first press stops it but stays on results */
        g_app->worker_running = false;
        furi_string_cat_printf(g_app->tool_text, "\nStopped by user.\n");
        lan_tester_update_view(g_app->text_box_tool, g_app->tool_text);
        return LanTesterViewToolResult;
    }
    lan_tester_stop_worker_on_back();
    if(!g_app) return LanTesterViewMainMenu;

    /* Returning to History list — repopulate since state was freed */
    if(g_app->tool_back_view == LanTesterViewHistory) {
        lan_tester_history_populate(g_app);
    }

    return g_app->tool_back_view;
}

/* ==================== SNMP/NTP/NetBIOS/DNS Poison input callbacks ==================== */

static void lan_tester_snmp_ip_input_callback(void* context) {
    LanTesterApp* app = context;
    furi_assert(app);
    if(lan_tester_parse_ip(app->snmp_ip_input, app->snmp_target)) {
        lan_tester_show_view(
            app, app->text_box_tool, LanTesterViewToolResult, app->tool_text, "Querying SNMP...\n");
        lan_tester_worker_start(app, LanTesterMenuItemSnmpGet, LanTesterViewToolResult);
    } else {
        furi_string_set(app->tool_text, "Invalid IP address!\n");
        text_box_set_text(app->text_box_tool, furi_string_get_cstr(app->tool_text));
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewToolResult);
    }
}

static void lan_tester_ntp_ip_input_callback(void* context) {
    LanTesterApp* app = context;
    furi_assert(app);
    if(lan_tester_parse_ip(app->ntp_ip_input, app->ntp_target)) {
        lan_tester_show_view(
            app, app->text_box_tool, LanTesterViewToolResult, app->tool_text, "Querying NTP...\n");
        lan_tester_worker_start(app, LanTesterMenuItemNtpDiag, LanTesterViewToolResult);
    } else {
        furi_string_set(app->tool_text, "Invalid IP address!\n");
        text_box_set_text(app->text_box_tool, furi_string_get_cstr(app->tool_text));
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewToolResult);
    }
}

static void lan_tester_ntp_sync_hours_callback(void* context, int32_t number) {
    LanTesterApp* app = context;
    furi_assert(app);
    app->ntp_tz_hours = (int8_t)number;

    /* Step 2: ask for minutes */
    number_input_set_header_text(app->number_input_tool, "TZ minutes (0/15/30/45):");
    number_input_set_result_callback(
        app->number_input_tool,
        lan_tester_ntp_sync_minutes_callback,
        app,
        app->ntp_tz_minutes,
        0,
        45);
    view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewNumberInput);
}

static void lan_tester_ntp_sync_minutes_callback(void* context, int32_t number) {
    LanTesterApp* app = context;
    furi_assert(app);
    /* Round to nearest 15 */
    app->ntp_tz_minutes = (int8_t)((number + 7) / 15 * 15);

    /* Apply NTP time + timezone offset */
    int32_t tz_offset_sec = (int32_t)app->ntp_tz_hours * 3600 +
                            (int32_t)app->ntp_tz_minutes * (app->ntp_tz_hours < 0 ? -60 : 60);
    uint32_t elapsed = (furi_get_tick() - app->ntp_query_tick) / 1000;
    uint32_t utc_time = app->ntp_unix_time + elapsed;
    uint32_t local_time = (uint32_t)((int32_t)utc_time + tz_offset_sec);

    DateTime flip_dt;
    furi_hal_rtc_get_datetime(&flip_dt);
    int32_t diff_sec = (int32_t)local_time - (int32_t)datetime_datetime_to_timestamp(&flip_dt);

    DateTime local_dt;
    datetime_timestamp_to_datetime(local_time, &local_dt);
    furi_hal_rtc_set_datetime(&local_dt);

    furi_string_reset(app->tool_text);
    furi_string_cat_printf(
        app->tool_text,
        "[NTP Sync] UTC%+d:%02d\n"
        "Flipper: %04d-%02d-%02d %02d:%02d:%02d\n"
        "NTP:     %04d-%02d-%02d %02d:%02d:%02d\n"
        "Diff: %+ld sec\n"
        "Clock synced!\n",
        app->ntp_tz_hours,
        app->ntp_tz_minutes,
        flip_dt.year,
        flip_dt.month,
        flip_dt.day,
        flip_dt.hour,
        flip_dt.minute,
        flip_dt.second,
        local_dt.year,
        local_dt.month,
        local_dt.day,
        local_dt.hour,
        local_dt.minute,
        local_dt.second,
        (long)diff_sec);

    app->ntp_unix_time = 0;
    text_box_set_text(app->text_box_tool, furi_string_get_cstr(app->tool_text));
    if(app->setting_sound) {
        notification_message(app->notifications, &sequence_success);
    }
    lan_tester_settings_save(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewToolResult);
}

static void lan_tester_netbios_ip_input_callback(void* context) {
    LanTesterApp* app = context;
    furi_assert(app);
    if(lan_tester_parse_ip(app->netbios_ip_input, app->netbios_target)) {
        lan_tester_show_view(
            app,
            app->text_box_tool,
            LanTesterViewToolResult,
            app->tool_text,
            "Querying NetBIOS...\n");
        lan_tester_worker_start(app, LanTesterMenuItemNetbiosQuery, LanTesterViewToolResult);
    } else {
        furi_string_set(app->tool_text, "Invalid IP address!\n");
        text_box_set_text(app->text_box_tool, furi_string_get_cstr(app->tool_text));
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewToolResult);
    }
}

static void lan_tester_dns_poison_input_callback(void* context) {
    LanTesterApp* app = context;
    furi_assert(app);
    if(app->dns_poison_host_input[0] != '\0') {
        lan_tester_show_view(
            app, app->text_box_tool, LanTesterViewToolResult, app->tool_text, "Checking DNS...\n");
        lan_tester_worker_start(app, LanTesterMenuItemDnsPoisonCheck, LanTesterViewToolResult);
    }
}

/* ==================== VLAN Hop custom input callback ==================== */

static void lan_tester_vlan_hop_custom_input_callback(void* context) {
    LanTesterApp* app = context;
    furi_assert(app);
    if(app->vlan_hop_input[0] != '\0') {
        app->vlan_hop_custom = true;
        lan_tester_show_view(
            app, app->text_box_tool, LanTesterViewToolResult, app->tool_text, "Testing VLANs...\n");
        lan_tester_worker_start(app, LanTesterMenuItemVlanHopCustom, LanTesterViewToolResult);
    }
}

/* ==================== TFTP/IPMI input callbacks ==================== */

static void lan_tester_tftp_filename_input_callback(void* context) {
    LanTesterApp* app = context;
    furi_assert(app);
    /* Static to avoid 128B stack usage */
    static char save_path[128];
    snprintf(save_path, sizeof(save_path), APP_DATA_PATH("tftp/%s"), app->tftp_filename_input);
    lan_tester_show_view(
        app, app->text_box_tool, LanTesterViewToolResult, app->tool_text, "Downloading...\n");
    lan_tester_worker_start(app, LanTesterMenuItemTftpClient, LanTesterViewToolResult);
}

static void lan_tester_tftp_ip_input_callback(void* context) {
    LanTesterApp* app = context;
    furi_assert(app);
    if(lan_tester_parse_ip(app->tftp_ip_input, app->tftp_target)) {
        /* Next: ask for filename */
        text_input_reset(app->text_input_tool);
        text_input_set_header_text(app->text_input_tool, "Remote filename:");
        text_input_set_result_callback(
            app->text_input_tool,
            lan_tester_tftp_filename_input_callback,
            app,
            app->tftp_filename_input,
            sizeof(app->tftp_filename_input),
            false);
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewToolInput);
    }
}

static void lan_tester_ipmi_ip_input_callback(void* context) {
    LanTesterApp* app = context;
    furi_assert(app);
    if(lan_tester_parse_ip(app->ipmi_ip_input, app->ipmi_target)) {
        lan_tester_show_view(
            app, app->text_box_tool, LanTesterViewToolResult, app->tool_text, "Querying IPMI...\n");
        lan_tester_worker_start(app, LanTesterMenuItemIpmiClient, LanTesterViewToolResult);
    }
}

/* ==================== Submenu callback ==================== */

static void lan_tester_submenu_callback(void* context, uint32_t index) {
    LanTesterApp* app = context;
    furi_assert(app);

    switch(index) {
    case LanTesterMenuItemAutoTest:
        app->autotest_running = true;
        furi_string_set(app->autotest_text, "Waiting for link...\n");
        text_box_set_text(app->text_box_autotest, furi_string_get_cstr(app->autotest_text));
        lan_tester_worker_start(app, LanTesterMenuItemAutoTest, LanTesterViewAutoTest);
        break;

    case LanTesterMenuItemLinkInfo:
        app->tool_back_view = LanTesterViewCatPortInfo;
        lan_tester_show_view(
            app,
            app->text_box_tool,
            LanTesterViewToolResult,
            app->tool_text,
            "Reading link status...\n");
        lan_tester_worker_start(app, LanTesterMenuItemLinkInfo, LanTesterViewToolResult);
        break;

    case LanTesterMenuItemLldpCdp:
        app->tool_back_view = LanTesterViewCatPortInfo;
        lan_tester_show_view(
            app,
            app->text_box_tool,
            LanTesterViewToolResult,
            app->tool_text,
            "Listening for LLDP/CDP...\n");
        lan_tester_worker_start(app, LanTesterMenuItemLldpCdp, LanTesterViewToolResult);
        break;

    case LanTesterMenuItemArpScan:
        app->tool_back_view = LanTesterViewCatScan;
        lan_tester_show_view(
            app,
            app->text_box_tool,
            LanTesterViewToolResult,
            app->tool_text,
            "Initializing W5500...\n");
        lan_tester_worker_start(app, LanTesterMenuItemArpScan, LanTesterViewToolResult);
        break;

    case LanTesterMenuItemDhcpAnalyze:
        app->tool_back_view = LanTesterViewCatPortInfo;
        lan_tester_show_view(
            app,
            app->text_box_tool,
            LanTesterViewToolResult,
            app->tool_text,
            "Initializing W5500...\n");
        lan_tester_worker_start(app, LanTesterMenuItemDhcpAnalyze, LanTesterViewToolResult);
        break;

    case LanTesterMenuItemPing:
        app->tool_back_view = LanTesterViewCatDiag;
        /* Pre-populate with gateway if DHCP available and no custom target set */
        if(app->dhcp_valid && strcmp(app->ping_ip_input, "8.8.8.8") == 0) {
            snprintf(
                app->ping_ip_input,
                sizeof(app->ping_ip_input),
                "%d.%d.%d.%d",
                app->dhcp_gw[0],
                app->dhcp_gw[1],
                app->dhcp_gw[2],
                app->dhcp_gw[3]);
        }
        ip_keyboard_setup(
            app->ip_keyboard,
            "Ping target IP:",
            app->ping_ip_input,
            false,
            lan_tester_ping_ip_input_callback,
            app,
            app->ping_ip_input,
            sizeof(app->ping_ip_input),
            lan_tester_nav_back_diag);
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewIpKeyboard);
        break;

    case LanTesterMenuItemStats:
        app->tool_back_view = LanTesterViewCatTraffic;
        lan_tester_show_view(
            app,
            app->text_box_tool,
            LanTesterViewToolResult,
            app->tool_text,
            "Initializing W5500...\n");
        lan_tester_worker_start(app, LanTesterMenuItemStats, LanTesterViewToolResult);
        break;

    case LanTesterMenuItemDnsLookup:
        app->tool_back_view = LanTesterViewCatDiag;
        text_input_reset(app->text_input_tool);
        text_input_set_header_text(app->text_input_tool, "Hostname to resolve:");
        text_input_set_result_callback(
            app->text_input_tool,
            lan_tester_dns_input_callback,
            app,
            app->dns_hostname_input,
            sizeof(app->dns_hostname_input),
            false);
        /* Restore back navigation to Diagnostics (may have been changed by AT settings) */
        view_set_previous_callback(
            text_input_get_view(app->text_input_tool), lan_tester_nav_back_diag);
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewToolInput);
        break;

    case LanTesterMenuItemWol:
        app->tool_back_view = LanTesterViewCatUtilities;
        byte_input_set_header_text(app->byte_input_tool, "Target MAC address:");
        byte_input_set_result_callback(
            app->byte_input_tool, lan_tester_wol_input_callback, NULL, app, app->wol_mac_input, 6);
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewToolByteInput);
        break;

    case LanTesterMenuItemHistory:
        lan_tester_history_populate(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewHistory);
        break;

    case LanTesterMenuItemStpVlan:
        app->tool_back_view = LanTesterViewCatPortInfo;
        lan_tester_show_view(
            app, app->text_box_tool, LanTesterViewToolResult, app->tool_text, "Listening...\n");
        lan_tester_worker_start(app, LanTesterMenuItemStpVlan, LanTesterViewToolResult);
        break;

    case LanTesterMenuItemDiscovery:
        app->tool_back_view = LanTesterViewCatScan;
        lan_tester_show_view(
            app, app->text_box_tool, LanTesterViewToolResult, app->tool_text, "Scanning...\n");
        lan_tester_worker_start(app, LanTesterMenuItemDiscovery, LanTesterViewToolResult);
        break;

    case LanTesterMenuItemPingSweep:
        app->tool_back_view = LanTesterViewCatScan;
        if(app->dhcp_valid) {
            /* Already have DHCP — go straight to input */
            uint8_t net[4];
            for(int i = 0; i < 4; i++)
                net[i] = app->dhcp_ip[i] & app->dhcp_mask[i];
            uint8_t pfx = arp_mask_to_prefix(app->dhcp_mask);
            snprintf(
                app->ping_sweep_ip_input,
                sizeof(app->ping_sweep_ip_input),
                "%d.%d.%d.%d/%d",
                net[0],
                net[1],
                net[2],
                net[3],
                pfx);
            ip_keyboard_setup(
                app->ip_keyboard,
                "Scan range (CIDR):",
                app->ping_sweep_ip_input,
                true,
                lan_tester_ping_sweep_input_callback,
                app,
                app->ping_sweep_ip_input,
                sizeof(app->ping_sweep_ip_input),
                lan_tester_nav_back_scan);
            view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewIpKeyboard);
        } else {
            /* No DHCP yet — detect network first, then show input */
            lan_tester_show_view(
                app,
                app->text_box_tool,
                LanTesterViewToolResult,
                app->tool_text,
                "Detecting network...\n");
            lan_tester_worker_start(app, WORKER_OP_PING_SWEEP_DETECT, LanTesterViewToolResult);
        }
        break;

    case LanTesterMenuItemTraceroute:
        app->tool_back_view = LanTesterViewCatDiag;
        if(app->dhcp_valid && strcmp(app->traceroute_host_input, "8.8.8.8") == 0) {
            snprintf(
                app->traceroute_host_input,
                sizeof(app->traceroute_host_input),
                "%d.%d.%d.%d",
                app->dhcp_gw[0],
                app->dhcp_gw[1],
                app->dhcp_gw[2],
                app->dhcp_gw[3]);
        }
        text_input_reset(app->text_input_tool);
        text_input_set_header_text(app->text_input_tool, "IP or hostname:");
        text_input_set_result_callback(
            app->text_input_tool,
            lan_tester_traceroute_input_callback,
            app,
            app->traceroute_host_input,
            sizeof(app->traceroute_host_input),
            false);
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewToolInput);
        break;

    case LanTesterMenuItemMacChanger:
        app->tool_back_view = LanTesterViewSettings;
        /* Now handled via Settings; kept here for safety */
        break;

    case LanTesterMenuItemPortScanFull:
        app->tool_back_view = LanTesterViewPortScanMode;
        app->port_scan_top100 = true;
        /* fall through */
    case LanTesterMenuItemPortScan:
        app->tool_back_view = LanTesterViewPortScanMode;
        if(index == LanTesterMenuItemPortScan) app->port_scan_top100 = false;
        app->port_scan_custom = false;
        /* Pre-populate target with DHCP gateway if available */
        if(app->dhcp_valid &&
           (app->dhcp_gw[0] | app->dhcp_gw[1] | app->dhcp_gw[2] | app->dhcp_gw[3])) {
            snprintf(
                app->port_scan_ip_input,
                sizeof(app->port_scan_ip_input),
                "%d.%d.%d.%d",
                app->dhcp_gw[0],
                app->dhcp_gw[1],
                app->dhcp_gw[2],
                app->dhcp_gw[3]);
        }
        ip_keyboard_setup(
            app->ip_keyboard,
            app->port_scan_top100 ? "Target IP (Top 100):" : "Target IP (Top 20):",
            app->port_scan_ip_input,
            false,
            lan_tester_port_scan_ip_input_callback,
            app,
            app->port_scan_ip_input,
            sizeof(app->port_scan_ip_input),
            lan_tester_nav_back_port_scan_mode);
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewIpKeyboard);
        break;

    case LanTesterMenuItemPortScanCustom:
        app->tool_back_view = LanTesterViewPortScanMode;
        app->port_scan_custom = true;
        text_input_reset(app->text_input_tool);
        text_input_set_header_text(app->text_input_tool, "Start port (1-65535):");
        text_input_set_result_callback(
            app->text_input_tool,
            lan_tester_port_scan_start_callback,
            app,
            app->port_scan_start_input,
            sizeof(app->port_scan_start_input),
            false);
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewToolInput);
        break;

    case LanTesterMenuItemContPing:
        app->tool_back_view = LanTesterViewCatDiag;
        if(app->dhcp_valid && strcmp(app->cont_ping_ip_input, "8.8.8.8") == 0) {
            snprintf(
                app->cont_ping_ip_input,
                sizeof(app->cont_ping_ip_input),
                "%d.%d.%d.%d",
                app->dhcp_gw[0],
                app->dhcp_gw[1],
                app->dhcp_gw[2],
                app->dhcp_gw[3]);
        }
        ip_keyboard_setup(
            app->ip_keyboard,
            "Ping target IP:",
            app->cont_ping_ip_input,
            false,
            lan_tester_cont_ping_ip_input_callback,
            app,
            app->cont_ping_ip_input,
            sizeof(app->cont_ping_ip_input),
            lan_tester_nav_back_diag);
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewIpKeyboard);
        break;

    case LanTesterMenuItemEthBridge:
        with_view_model(
            app->view_bridge,
            BridgeViewModel * vm,
            {
                vm->active = false;
                vm->status_line = "Starting ETH Bridge...";
            },
            true);
        lan_tester_worker_start(app, LanTesterMenuItemEthBridge, LanTesterViewEthBridge);
        break;

    case LanTesterMenuItemPxeServer:
        app->tool_back_view = LanTesterViewCatUtilities;
        pxe_settings_refresh(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewPxeSettings);
        break;

    case LanTesterMenuItemFileManager:
        app->tool_back_view = LanTesterViewCatUtilities;
        furi_string_set(app->tool_text, "Starting File Manager...\n");
        text_box_set_text(app->text_box_tool, furi_string_get_cstr(app->tool_text));
        lan_tester_worker_start(app, LanTesterMenuItemFileManager, LanTesterViewToolResult);
        break;

    case LanTesterMenuItemPacketCapture:
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewPacketCapture);
        break;

    case LanTesterMenuItemAbout:
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewAbout);
        break;

    case LanTesterMenuItemSnmpGet:
        app->tool_back_view = LanTesterViewCatPortInfo;
        if(app->dhcp_valid) {
            snprintf(
                app->snmp_ip_input,
                sizeof(app->snmp_ip_input),
                "%d.%d.%d.%d",
                app->dhcp_gw[0],
                app->dhcp_gw[1],
                app->dhcp_gw[2],
                app->dhcp_gw[3]);
        }
        ip_keyboard_setup(
            app->ip_keyboard,
            "SNMP target IP:",
            app->snmp_ip_input,
            false,
            lan_tester_snmp_ip_input_callback,
            app,
            app->snmp_ip_input,
            sizeof(app->snmp_ip_input),
            lan_tester_nav_back_portinfo);
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewIpKeyboard);
        break;

    case LanTesterMenuItemNtpDiag:
        app->tool_back_view = LanTesterViewCatDiag;
        if(app->dhcp_valid && app->ntp_ip_input[0] == '\0') {
            snprintf(
                app->ntp_ip_input,
                sizeof(app->ntp_ip_input),
                "%d.%d.%d.%d",
                app->dhcp_gw[0],
                app->dhcp_gw[1],
                app->dhcp_gw[2],
                app->dhcp_gw[3]);
        }
        ip_keyboard_setup(
            app->ip_keyboard,
            "NTP server IP:",
            app->ntp_ip_input,
            false,
            lan_tester_ntp_ip_input_callback,
            app,
            app->ntp_ip_input,
            sizeof(app->ntp_ip_input),
            lan_tester_nav_back_diag);
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewIpKeyboard);
        break;

    case LanTesterMenuItemNtpSync:
        app->tool_back_view = LanTesterViewCatDiag;
        if(!app->ntp_unix_time || (furi_get_tick() - app->ntp_query_tick) >= 120000) {
            furi_string_reset(app->tool_text);
            furi_string_set(
                app->tool_text, "[NTP Sync]\nNo recent NTP data.\nRun NTP Diagnostics first.\n");
            text_box_set_text(app->text_box_tool, furi_string_get_cstr(app->tool_text));
            view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewToolResult);
        } else {
            /* Auto-detect timezone from diff if between 1h and 12h */
            uint32_t elapsed = (furi_get_tick() - app->ntp_query_tick) / 1000;
            uint32_t adjusted = app->ntp_unix_time + elapsed;
            DateTime flip_dt;
            furi_hal_rtc_get_datetime(&flip_dt);
            int32_t diff_sec =
                (int32_t)adjusted - (int32_t)datetime_datetime_to_timestamp(&flip_dt);
            int32_t abs_diff = diff_sec < 0 ? -diff_sec : diff_sec;
            if(abs_diff >= 3600 && abs_diff <= 43200) {
                int32_t tz_total = (diff_sec + (diff_sec > 0 ? 450 : -450)) / 900 * 15;
                tz_total = -tz_total;
                app->ntp_tz_hours = (int8_t)(tz_total / 60);
                app->ntp_tz_minutes = (int8_t)((tz_total < 0 ? -tz_total : tz_total) % 60);
            }
            /* Step 1: ask for hours */
            number_input_set_header_text(app->number_input_tool, "TZ hours (-12..+14):");
            number_input_set_result_callback(
                app->number_input_tool,
                lan_tester_ntp_sync_hours_callback,
                app,
                app->ntp_tz_hours,
                -12,
                14);
            view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewNumberInput);
        }
        break;

    case LanTesterMenuItemNetbiosQuery:
        app->tool_back_view = LanTesterViewCatScan;
        if(app->dhcp_valid) {
            snprintf(
                app->netbios_ip_input,
                sizeof(app->netbios_ip_input),
                "%d.%d.%d.%d",
                app->dhcp_gw[0],
                app->dhcp_gw[1],
                app->dhcp_gw[2],
                app->dhcp_gw[3]);
        }
        ip_keyboard_setup(
            app->ip_keyboard,
            "NetBIOS target IP:",
            app->netbios_ip_input,
            false,
            lan_tester_netbios_ip_input_callback,
            app,
            app->netbios_ip_input,
            sizeof(app->netbios_ip_input),
            lan_tester_nav_back_scan);
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewIpKeyboard);
        break;

    case LanTesterMenuItemDnsPoisonCheck:
        app->tool_back_view = LanTesterViewCatDiag;
        text_input_reset(app->text_input_tool);
        text_input_set_header_text(app->text_input_tool, "Hostname to check:");
        text_input_set_result_callback(
            app->text_input_tool,
            lan_tester_dns_poison_input_callback,
            app,
            app->dns_poison_host_input,
            sizeof(app->dns_poison_host_input),
            false);
        view_set_previous_callback(
            text_input_get_view(app->text_input_tool), lan_tester_nav_back_diag);
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewToolInput);
        break;

    case 100: /* Port Info category */
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewCatPortInfo);
        break;
    case 101: /* Scan category */
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewCatScan);
        break;
    case 102: /* Diagnostics category */
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewCatDiag);
        break;
    case 103: /* Utilities category */
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewCatUtilities);
        break;
    case 104: /* Settings */
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewSettings);
        break;
    case 105: /* Traffic category */
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewCatTraffic);
        break;
    case 106: /* Port Scan mode submenu */
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewPortScanMode);
        break;
    case 107: /* Security category */
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewCatSecurity);
        break;

    case LanTesterMenuItemArpWatch:
        app->tool_back_view = LanTesterViewCatSecurity;
        lan_tester_show_view(
            app,
            app->text_box_tool,
            LanTesterViewToolResult,
            app->tool_text,
            "Listening for ARP...\n");
        lan_tester_worker_start(app, LanTesterMenuItemArpWatch, LanTesterViewToolResult);
        break;

    case LanTesterMenuItemRogueDhcp:
        app->tool_back_view = LanTesterViewCatSecurity;
        lan_tester_show_view(
            app,
            app->text_box_tool,
            LanTesterViewToolResult,
            app->tool_text,
            "Sending DHCP Discover...\n");
        lan_tester_worker_start(app, LanTesterMenuItemRogueDhcp, LanTesterViewToolResult);
        break;

    case LanTesterMenuItemRogueRa:
        app->tool_back_view = LanTesterViewCatSecurity;
        lan_tester_show_view(
            app,
            app->text_box_tool,
            LanTesterViewToolResult,
            app->tool_text,
            "Listening for IPv6 RA...\n");
        lan_tester_worker_start(app, LanTesterMenuItemRogueRa, LanTesterViewToolResult);
        break;

    case LanTesterMenuItemDhcpFingerprint:
        app->tool_back_view = LanTesterViewCatSecurity;
        lan_tester_show_view(
            app,
            app->text_box_tool,
            LanTesterViewToolResult,
            app->tool_text,
            "Listening for DHCP...\n");
        lan_tester_worker_start(app, LanTesterMenuItemDhcpFingerprint, LanTesterViewToolResult);
        break;

    case LanTesterMenuItemEapolProbe:
        app->tool_back_view = LanTesterViewCatSecurity;
        lan_tester_show_view(
            app,
            app->text_box_tool,
            LanTesterViewToolResult,
            app->tool_text,
            "Sending EAPOL-Start...\n");
        lan_tester_worker_start(app, LanTesterMenuItemEapolProbe, LanTesterViewToolResult);
        break;

    case LanTesterMenuItemVlanHopTop10:
        app->tool_back_view = LanTesterViewCatSecurity;
        app->vlan_hop_custom = false;
        lan_tester_show_view(
            app,
            app->text_box_tool,
            LanTesterViewToolResult,
            app->tool_text,
            "Testing VLAN isolation...\n");
        lan_tester_worker_start(app, LanTesterMenuItemVlanHopTop10, LanTesterViewToolResult);
        break;

    case LanTesterMenuItemVlanHopCustom:
        app->tool_back_view = LanTesterViewCatSecurity;
        text_input_reset(app->text_input_tool);
        text_input_set_header_text(app->text_input_tool, "VLANs (comma sep):");
        text_input_set_result_callback(
            app->text_input_tool,
            lan_tester_vlan_hop_custom_input_callback,
            app,
            app->vlan_hop_input,
            sizeof(app->vlan_hop_input),
            false);
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewToolInput);
        break;

    case LanTesterMenuItemTftpClient:
        app->tool_back_view = LanTesterViewCatUtilities;
        if(app->dhcp_valid) {
            snprintf(
                app->tftp_ip_input,
                sizeof(app->tftp_ip_input),
                "%d.%d.%d.%d",
                app->dhcp_gw[0],
                app->dhcp_gw[1],
                app->dhcp_gw[2],
                app->dhcp_gw[3]);
        }
        ip_keyboard_setup(
            app->ip_keyboard,
            "TFTP server IP:",
            app->tftp_ip_input,
            false,
            lan_tester_tftp_ip_input_callback,
            app,
            app->tftp_ip_input,
            sizeof(app->tftp_ip_input),
            lan_tester_nav_back_utilities);
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewIpKeyboard);
        break;

    case LanTesterMenuItemIpmiClient:
        app->tool_back_view = LanTesterViewCatUtilities;
        if(app->dhcp_valid) {
            snprintf(
                app->ipmi_ip_input,
                sizeof(app->ipmi_ip_input),
                "%d.%d.%d.%d",
                app->dhcp_gw[0],
                app->dhcp_gw[1],
                app->dhcp_gw[2],
                app->dhcp_gw[3]);
        }
        ip_keyboard_setup(
            app->ip_keyboard,
            "BMC/IPMI IP:",
            app->ipmi_ip_input,
            false,
            lan_tester_ipmi_ip_input_callback,
            app,
            app->ipmi_ip_input,
            sizeof(app->ipmi_ip_input),
            lan_tester_nav_back_utilities);
        view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewIpKeyboard);
        break;

    default:
        break;
    }
}

/* ==================== Feature implementations ==================== */

static void lan_tester_do_link_info(LanTesterApp* app) {
    furi_string_reset(app->tool_text);

    if(!lan_tester_ensure_w5500(app)) {
        furi_string_set(app->tool_text, "W5500 Not Found!\nCheck SPI wiring.\n");
        return;
    }

    /* Read PHY info */
    bool link_up = false;
    uint8_t speed = 0, duplex = 0;
    w5500_hal_get_phy_info(&link_up, &speed, &duplex);
    app->link_up = link_up;
    app->link_speed = speed;
    app->link_duplex = duplex;

    /* Read current MAC */
    uint8_t mac[6];
    w5500_hal_get_mac(mac);

    char mac_str[18];
    pkt_format_mac(mac, mac_str);

    furi_string_printf(
        app->tool_text,
        "[Link Info]\n"
        "Link: %s\n"
        "Speed: %s\n"
        "Duplex: %s\n"
        "MAC: %s\n"
        "W5500: OK (v0x04)\n",
        link_up ? "UP" : "DOWN",
        speed ? "100 Mbps" : "10 Mbps",
        duplex ? "Full" : "Half",
        mac_str);
}

static void lan_tester_do_lldp_cdp(LanTesterApp* app) {
    if(!lan_tester_check_w5500(app)) return;

    if(!w5500_hal_get_link_status()) {
        furi_string_set(app->tool_text, "No Link!\nConnect cable.\n");
        return;
    }

    furi_string_set(app->tool_text, "Listening for\nLLDP/CDP...\n(up to 60 sec)\n");
    lan_tester_update_view(app->text_box_tool, app->tool_text);

    /* Open MACRAW socket */
    if(!w5500_hal_open_macraw()) {
        furi_string_set(app->tool_text, "Failed to open\nMACRAW socket!\n");
        return;
    }

    LldpNeighbor lldp_neighbor;
    CdpNeighbor cdp_neighbor;
    memset(&lldp_neighbor, 0, sizeof(lldp_neighbor));
    memset(&cdp_neighbor, 0, sizeof(cdp_neighbor));

    uint32_t start_tick = furi_get_tick();
    uint32_t timeout_ms = 60000; /* 60 seconds */
    bool found = false;
    uint32_t last_countdown = 0;

    while(furi_get_tick() - start_tick < timeout_ms && app->worker_running) {
        /* Update countdown every second */
        uint32_t elapsed_sec = (furi_get_tick() - start_tick) / 1000;
        if(elapsed_sec != last_countdown) {
            last_countdown = elapsed_sec;
            uint32_t remaining = 60 - elapsed_sec;
            furi_string_printf(
                app->tool_text,
                "Listening for\nLLDP/CDP...\n(%lus remaining)\n",
                (unsigned long)remaining);
            lan_tester_update_view(app->text_box_tool, app->tool_text);
        }

        uint16_t recv_len = w5500_hal_macraw_recv(app->frame_buf, FRAME_BUF_SIZE);
        if(recv_len >= ETH_HEADER_SIZE) {
            /* Count frame for statistics */
            lan_tester_count_frame(app, app->frame_buf, recv_len);

            uint16_t ethertype = pkt_get_ethertype(app->frame_buf);

            /* Check for LLDP */
            if(ethertype == ETHERTYPE_LLDP && !lldp_neighbor.valid) {
                FURI_LOG_I(TAG, "LLDP frame received (%d bytes)", recv_len);
                if(lldp_parse(
                       app->frame_buf + ETH_HEADER_SIZE,
                       recv_len - ETH_HEADER_SIZE,
                       &lldp_neighbor)) {
                    lldp_neighbor.last_seen_tick = furi_get_tick();
                    found = true;
                }
            }

            /* Check for CDP (LLC/SNAP) */
            if(!cdp_neighbor.valid) {
                uint16_t cdp_offset = cdp_check_frame(app->frame_buf, recv_len);
                if(cdp_offset > 0) {
                    FURI_LOG_I(TAG, "CDP frame received (%d bytes)", recv_len);
                    if(cdp_parse(
                           app->frame_buf + cdp_offset, recv_len - cdp_offset, &cdp_neighbor)) {
                        cdp_neighbor.last_seen_tick = furi_get_tick();
                        found = true;
                    }
                }
            }

            /* Stop early if we have both */
            if(lldp_neighbor.valid && cdp_neighbor.valid) break;
        }

        furi_delay_ms(100);
    }

    w5500_hal_close_macraw();

    /* Format results */
    furi_string_reset(app->tool_text);

    /* Heap-allocate formatting buffer to avoid 1 KB of stack usage (2x512) */
    if(lldp_neighbor.valid || cdp_neighbor.valid) {
        char* fmt_buf = malloc(512);
        if(fmt_buf) {
            if(lldp_neighbor.valid) {
                lldp_format_neighbor(&lldp_neighbor, fmt_buf, 512);
                furi_string_cat_str(app->tool_text, fmt_buf);
            }
            if(cdp_neighbor.valid) {
                cdp_format_neighbor(&cdp_neighbor, fmt_buf, 512);
                if(lldp_neighbor.valid) furi_string_cat_str(app->tool_text, "\n");
                furi_string_cat_str(app->tool_text, fmt_buf);
            }
            free(fmt_buf);
        }
    }

    if(!found) {
        furi_string_set(app->tool_text, "No LLDP/CDP neighbors\ndetected (waited 60s)\n");
    }

    /* Save results to SD card */
    lan_tester_save_and_notify(app, "lldp_cdp.txt", app->tool_text);
}

static void lan_tester_do_arp_scan(LanTesterApp* app) {
    furi_string_reset(app->tool_text);

    furi_string_set(app->tool_text, "Getting IP via DHCP...\n");
    lan_tester_update_view(app->text_box_tool, app->tool_text);

    if(!lan_tester_check_dhcp(app)) return;

    wiz_NetInfo net_info;
    wizchip_getnetinfo(&net_info);

    FURI_LOG_I(
        TAG, "Got IP: %d.%d.%d.%d", net_info.ip[0], net_info.ip[1], net_info.ip[2], net_info.ip[3]);

    /* Calculate scan range */
    uint8_t start_ip[4], end_ip[4];
    uint16_t num_hosts = arp_calc_scan_range(net_info.ip, net_info.sn, start_ip, end_ip);
    uint8_t prefix = arp_mask_to_prefix(net_info.sn);

    if(num_hosts == 0) {
        furi_string_set(app->tool_text, "No hosts to scan\n(point-to-point link?)\n");
        return;
    }

    /* Dedup array: 2 bytes per host (last 2 octets). 1024 hosts = 2 KB. */
    uint16_t max_dedup = (num_hosts < 1024) ? num_hosts : 1024;

    char ip_str[16];
    pkt_format_ip(net_info.ip, ip_str);
    furi_string_printf(
        app->tool_text, "My IP: %s/%d\nScanning %d hosts...\n", ip_str, prefix, num_hosts);
    lan_tester_update_view(app->text_box_tool, app->tool_text);

    /* Open MACRAW for sending ARP requests and receiving replies */
    if(!w5500_hal_open_macraw()) {
        furi_string_set(app->tool_text, "Failed to open\nMACRAW!\n");
        return;
    }

    /* Allocate dedup array: last 2 octets per host (same subnet, first 2 always match) */
    uint8_t(*dedup_ips)[2] = malloc(2 * max_dedup);
    if(!dedup_ips) {
        furi_string_set(app->tool_text, "Memory alloc failed!\n");
        w5500_hal_close_macraw();
        return;
    }
    uint16_t found_count = 0;
    uint16_t total_sent = 0;
    scan_results_clear();
    app->discovered_host_count = 0;
    app->host_list_page = 0;
    scan_results_open_writer();

    /* Send ARP requests in batches */
    uint32_t scan_start_tick = furi_get_tick();
    uint8_t arp_frame[42];
    uint32_t current_ip = pkt_read_u32_be(start_ip);
    uint32_t last_ip = pkt_read_u32_be(end_ip);
    uint16_t batch_count = 0;

    while(current_ip <= last_ip && app->worker_running) {
        /* Build and send ARP request */
        uint8_t target[4];
        pkt_write_u32_be(target, current_ip);
        arp_build_request(arp_frame, net_info.mac, net_info.ip, target);
        w5500_hal_macraw_send(arp_frame, 42);
        total_sent++;
        current_ip++;
        batch_count++;

        /* After each batch, pause and collect replies */
        if(batch_count >= ARP_BATCH_SIZE) {
            batch_count = 0;
            furi_delay_ms(ARP_BATCH_DELAY_MS);

            /* Update progress */
            furi_string_printf(
                app->tool_text,
                "My IP: %s/%d\nScanning: %d/%d sent\nFound: %d hosts\n",
                ip_str,
                prefix,
                total_sent,
                num_hosts,
                found_count);
            lan_tester_update_view(app->text_box_tool, app->tool_text);

            /* Collect any pending replies */
            for(uint8_t i = 0; i < 20; i++) {
                uint16_t recv_len = w5500_hal_macraw_recv(app->frame_buf, FRAME_BUF_SIZE);
                if(recv_len == 0) break;

                uint8_t sender_mac[6], sender_ip[4];
                if(arp_parse_reply(app->frame_buf, recv_len, sender_mac, sender_ip)) {
                    if(found_count < max_dedup) {
                        memcpy(dedup_ips[found_count], sender_ip + 2, 2);
                        scan_results_add(sender_ip, sender_mac);
                        found_count++;
                        app->discovered_host_count++;
                    }
                }
            }
        }
    }

    /* Wait for late replies (skip if interrupted) */
    uint32_t tail_start = furi_get_tick();
    if(app->worker_running) {
        furi_string_printf(
            app->tool_text,
            "My IP: %s/%d\nAll %d sent, waiting\nfor replies... (%d found)\n",
            ip_str,
            prefix,
            num_hosts,
            found_count);
        lan_tester_update_view(app->text_box_tool, app->tool_text);
    }
    while(furi_get_tick() - tail_start < ARP_TAIL_WAIT_MS && app->worker_running) {
        uint16_t recv_len = w5500_hal_macraw_recv(app->frame_buf, FRAME_BUF_SIZE);
        if(recv_len > 0) {
            uint8_t sender_mac[6], sender_ip[4];
            if(arp_parse_reply(app->frame_buf, recv_len, sender_mac, sender_ip)) {
                /* Check for duplicate (compare last 2 octets — same subnet) */
                bool duplicate = false;
                for(uint16_t j = 0; j < found_count; j++) {
                    if(memcmp(dedup_ips[j], sender_ip + 2, 2) == 0) {
                        duplicate = true;
                        break;
                    }
                }
                if(!duplicate && found_count < max_dedup) {
                    memcpy(dedup_ips[found_count], sender_ip + 2, 2);
                    scan_results_add(sender_ip, sender_mac);
                    found_count++;
                    app->discovered_host_count++;
                }
            }
        }
        furi_delay_ms(50);
    }

    w5500_hal_close_macraw();
    scan_results_close_writer();
    free(dedup_ips);

    uint32_t elapsed_ms = furi_get_tick() - scan_start_tick;

    /* Summary only — full host list available in Discovered Hosts */
    furi_string_printf(
        app->tool_text,
        "[ARP Scan] Done\n"
        "%s/%d\n"
        "Found: %d hosts\n"
        "Time: %lu.%lus\n",
        ip_str,
        prefix,
        found_count,
        (unsigned long)(elapsed_ms / 1000),
        (unsigned long)((elapsed_ms % 1000) / 100));

    if(found_count == 0) {
        furi_string_cat(app->tool_text, "No hosts found.\n");
    }

    lan_tester_save_and_notify(app, "arp_scan.txt", app->tool_text);
    furi_string_reset(app->tool_text);

    /* Show interactive host list if hosts were found (even if scan was interrupted) */
    if(app->discovered_host_count > 0) {
        view_dispatcher_send_custom_event(app->view_dispatcher, CUSTOM_EVENT_SHOW_HOST_LIST);
    }
}

static void lan_tester_do_dhcp_analyze(LanTesterApp* app) {
    if(!lan_tester_check_w5500(app)) return;

    if(!w5500_hal_get_link_status()) {
        furi_string_set(app->tool_text, "No Link!\nConnect cable.\n");
        return;
    }

    furi_string_set(app->tool_text, "Sending DHCP\nDiscover...\n");
    lan_tester_update_view(app->text_box_tool, app->tool_text);

    /*
     * Use UDP socket directly to send DHCP Discover and receive Offer
     * without going through the full DHCP state machine.
     * We do NOT send DHCP Request - just analyze the Offer.
     */
    uint8_t dhcp_socket = W5500_DHCP_SOCKET;

    /* Set our IP to 0.0.0.0 for DHCP discovery */
    wiz_NetInfo net_info;
    wizchip_getnetinfo(&net_info);
    uint8_t saved_ip[4], saved_sn[4], saved_gw[4];
    memcpy(saved_ip, net_info.ip, 4);
    memcpy(saved_sn, net_info.sn, 4);
    memcpy(saved_gw, net_info.gw, 4);
    memset(net_info.ip, 0, 4);
    memset(net_info.sn, 0, 4);
    memset(net_info.gw, 0, 4);
    wizchip_setnetinfo(&net_info);

    /* Open UDP socket on port 68 */
    close(dhcp_socket);
    int8_t ret = socket(dhcp_socket, Sn_MR_UDP, DHCP_CLIENT_PORT, 0);
    if(ret != dhcp_socket) {
        furi_string_set(app->tool_text, "Failed to open\nUDP socket!\n");
        return;
    }

    /* Build DHCP Discover — reuse frame_buf (1600 bytes) */
    uint32_t xid;
    furi_hal_random_fill_buf((uint8_t*)&xid, sizeof(xid));
    uint16_t pkt_len = dhcp_build_discover(app->frame_buf, app->mac_addr, xid);

    /* Send to broadcast 255.255.255.255:67 */
    uint8_t bcast_ip[4] = {255, 255, 255, 255};
    int32_t sent = sendto(dhcp_socket, app->frame_buf, pkt_len, bcast_ip, DHCP_SERVER_PORT);
    if(sent <= 0) {
        furi_string_set(app->tool_text, "Failed to send\nDHCP Discover!\n");
        close(dhcp_socket);
        return;
    }

    FURI_LOG_I(TAG, "DHCP Discover sent (xid=0x%08lX)", (unsigned long)xid);
    furi_string_set(app->tool_text, "Waiting for DHCP\nOffer... (10s)\n");
    lan_tester_update_view(app->text_box_tool, app->tool_text);

    /* Wait for DHCP Offer */
    DhcpAnalyzeResult dhcp_result;
    bool got_offer = false;
    uint32_t start_tick = furi_get_tick();
    /* Reuse frame_buf for receiving DHCP Offer */
    while(furi_get_tick() - start_tick < 10000 && app->worker_running) {
        uint16_t rx_size = getSn_RX_RSR(dhcp_socket);
        if(rx_size > 0) {
            uint8_t from_ip[4];
            uint16_t from_port;
            int32_t received =
                recvfrom(dhcp_socket, app->frame_buf, FRAME_BUF_SIZE, from_ip, &from_port);
            if(received > 0) {
                if(dhcp_parse_offer(app->frame_buf, (uint16_t)received, xid, &dhcp_result)) {
                    got_offer = true;
                    break;
                }
            }
        }
        furi_delay_ms(50);
    }

    close(dhcp_socket);

    /* Restore network settings */
    memcpy(net_info.ip, saved_ip, 4);
    memcpy(net_info.sn, saved_sn, 4);
    memcpy(net_info.gw, saved_gw, 4);
    wizchip_setnetinfo(&net_info);

    /* Format results */
    furi_string_reset(app->tool_text);

    if(got_offer) {
        /* Reuse frame_buf as temporary formatting buffer */
        dhcp_format_result(&dhcp_result, (char*)app->frame_buf, FRAME_BUF_SIZE);
        furi_string_set(app->tool_text, (char*)app->frame_buf);
    } else {
        furi_string_set(app->tool_text, "No DHCP server found.\n(waited 10 sec)\n");
    }

    /* Save results to SD card */
    lan_tester_save_and_notify(app, "dhcp_analyze.txt", app->tool_text);
}

static void lan_tester_do_ping(LanTesterApp* app) {
    furi_string_reset(app->tool_text);

    furi_string_set(app->tool_text, "Getting IP via DHCP...\n");
    lan_tester_update_view(app->text_box_tool, app->tool_text);

    if(!lan_tester_check_dhcp(app)) return;

    wiz_NetInfo net_info;
    wizchip_getnetinfo(&net_info);

    /* Use custom IP if set, otherwise ping the gateway */
    uint8_t target_ip[4];
    if(app->ping_ip_custom[0] != 0) {
        memcpy(target_ip, app->ping_ip_custom, 4);
    } else {
        memcpy(target_ip, net_info.gw, 4);
    }

    char target_str[16], my_ip_str[16];
    pkt_format_ip(target_ip, target_str);
    pkt_format_ip(net_info.ip, my_ip_str);

    furi_string_printf(app->tool_text, "Ping %s (me:%s)\n", target_str, my_ip_str);
    lan_tester_update_view(app->text_box_tool, app->tool_text);

    /* Send pings (count from settings) */
    for(uint16_t i = 1; i <= app->ping_count && app->worker_running; i++) {
        PingResult result;
        bool ok = icmp_ping(
            W5500_PING_SOCKET, target_ip, i, app->ping_timeout_ms, &result, &app->worker_running);
        if(ok) {
            furi_string_cat_printf(
                app->tool_text, "#%d: %lu ms\n", i, (unsigned long)result.rtt_ms);
        } else {
            furi_string_cat_printf(app->tool_text, "#%d: timeout\n", i);
        }
        lan_tester_update_view(app->text_box_tool, app->tool_text);
        furi_delay_ms(100);
    }
}

/* ==================== DNS Lookup ==================== */

static void lan_tester_do_dns_lookup(LanTesterApp* app) {
    furi_string_reset(app->tool_text);

    furi_string_set(app->tool_text, "Getting IP via DHCP...\n");
    lan_tester_update_view(app->text_box_tool, app->tool_text);

    if(!lan_tester_check_dhcp(app)) return;

    wiz_NetInfo net_info;
    wizchip_getnetinfo(&net_info);

    /* Get DNS server: custom if enabled, otherwise from DHCP */
    uint8_t dns_ip[4];
    lan_tester_get_dns_server(app, dns_ip);

    /* Check DNS server is valid */
    if(dns_ip[0] == 0 && dns_ip[1] == 0 && dns_ip[2] == 0 && dns_ip[3] == 0) {
        furi_string_set(app->tool_text, "No DNS server\navailable.\n");
        return;
    }

    memcpy(app->dns_server_ip, dns_ip, 4);

    char dns_str[16];
    pkt_format_ip(dns_ip, dns_str);

    furi_string_printf(app->tool_text, "[DNS] %s via %s\n", app->dns_hostname_input, dns_str);
    lan_tester_update_view(app->text_box_tool, app->tool_text);

    DnsLookupResult dns_result;
    bool ok = dns_lookup(W5500_DNS_SOCKET, dns_ip, app->dns_hostname_input, &dns_result);

    if(ok) {
        char ip_str[16];
        pkt_format_ip(dns_result.resolved_ip, ip_str);
        furi_string_cat_printf(app->tool_text, "-> %s\n", ip_str);
    } else {
        furi_string_cat_printf(
            app->tool_text,
            "%s\n",
            dns_result.rcode == DNS_RCODE_NXDOMAIN ? "NXDOMAIN" : "Timeout");
    }

    lan_tester_save_and_notify(app, "dns_lookup.txt", app->tool_text);
}

/* ==================== Wake-on-LAN ==================== */

static void lan_tester_do_wol(LanTesterApp* app) {
    furi_string_reset(app->tool_text);

    furi_string_set(app->tool_text, "Getting IP via DHCP...\n");
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

/* ==================== MAC Changer ==================== */

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

/* ==================== Traceroute ==================== */

static void lan_tester_do_traceroute(LanTesterApp* app) {
    furi_string_reset(app->tool_text);

    furi_string_set(app->tool_text, "Getting IP via DHCP...\n");
    lan_tester_update_view(app->text_box_tool, app->tool_text);

    if(!lan_tester_check_dhcp(app)) return;

    /* If input is a hostname, resolve via DNS first */
    if(app->traceroute_is_hostname) {
        furi_string_printf(app->tool_text, "Resolving %s...\n", app->traceroute_host_input);
        lan_tester_update_view(app->text_box_tool, app->tool_text);

        uint8_t dns_ip[4];
        lan_tester_get_dns_server(app, dns_ip);

        if(dns_ip[0] == 0 && dns_ip[1] == 0 && dns_ip[2] == 0 && dns_ip[3] == 0) {
            furi_string_set(app->tool_text, "No DNS server available.\n");
            return;
        }

        DnsLookupResult dns_result;
        bool resolved =
            dns_lookup(W5500_DNS_SOCKET, dns_ip, app->traceroute_host_input, &dns_result);

        if(!resolved) {
            furi_string_set(app->tool_text, "DNS resolve failed.\n");
            return;
        }

        memcpy(app->traceroute_target, dns_result.resolved_ip, 4);

        char ip_str[16];
        pkt_format_ip(dns_result.resolved_ip, ip_str);
        furi_string_printf(app->tool_text, "%s -> %s\n\n", app->traceroute_host_input, ip_str);
        lan_tester_update_view(app->text_box_tool, app->tool_text);
    }

    char target_str[16];
    pkt_format_ip(app->traceroute_target, target_str);

    furi_string_cat_printf(
        app->tool_text,
        "[Traceroute]\n"
        "Target: %s\n\n",
        target_str);
    lan_tester_update_view(app->text_box_tool, app->tool_text);

    /* Run traceroute */
    for(uint8_t ttl = 1; ttl <= TRACEROUTE_MAX_TTL && app->worker_running; ttl++) {
        TracerouteHop hop;
        bool got_reply = traceroute_send_hop(
            W5500_TRACEROUTE_SOCKET,
            app->traceroute_target,
            ttl,
            ttl,
            TRACEROUTE_HOP_TIMEOUT_MS,
            &hop);

        if(got_reply) {
            char hop_ip_str[16];
            pkt_format_ip(hop.hop_ip, hop_ip_str);
            furi_string_cat_printf(
                app->tool_text, "%2d  %s  %lu ms\n", ttl, hop_ip_str, (unsigned long)hop.rtt_ms);
        } else {
            furi_string_cat_printf(app->tool_text, "%2d  * * *\n", ttl);
        }

        lan_tester_update_view(app->text_box_tool, app->tool_text);

        /* Stop if destination reached */
        if(got_reply && hop.is_destination) {
            furi_string_cat_str(app->tool_text, "\nDestination reached.\n");
            break;
        }
    }

    lan_tester_save_and_notify(app, "traceroute.txt", app->tool_text);
}

/* ==================== Ping Sweep ==================== */

/* Parse CIDR notation "192.168.1.0/24" into base IP and prefix length */
static bool parse_cidr(const char* str, uint8_t base_ip[4], uint8_t* prefix) {
    unsigned int a, b, c, d, p;
    if(sscanf(str, "%u.%u.%u.%u/%u", &a, &b, &c, &d, &p) != 5) return false;
    if(a > 255 || b > 255 || c > 255 || d > 255 || p > 32) return false;
    base_ip[0] = (uint8_t)a;
    base_ip[1] = (uint8_t)b;
    base_ip[2] = (uint8_t)c;
    base_ip[3] = (uint8_t)d;
    *prefix = (uint8_t)p;
    return true;
}

/* Phase 1: detect network via DHCP, then signal main thread to show input */
static void lan_tester_do_ping_sweep_detect(LanTesterApp* app) {
    furi_string_reset(app->tool_text);

    furi_string_set(app->tool_text, "Getting IP via DHCP...\n");
    lan_tester_update_view(app->text_box_tool, app->tool_text);

    if(!lan_tester_check_dhcp(app)) {
        lan_tester_update_view(app->text_box_tool, app->tool_text);
        return;
    }

    /* Populate CIDR from detected network */
    uint8_t net[4];
    for(int i = 0; i < 4; i++)
        net[i] = app->dhcp_ip[i] & app->dhcp_mask[i];
    uint8_t pfx = arp_mask_to_prefix(app->dhcp_mask);
    snprintf(
        app->ping_sweep_ip_input,
        sizeof(app->ping_sweep_ip_input),
        "%d.%d.%d.%d/%d",
        net[0],
        net[1],
        net[2],
        net[3],
        pfx);

    /* Signal main thread to show input */
    view_dispatcher_send_custom_event(app->view_dispatcher, CUSTOM_EVENT_PING_SWEEP_READY);
}

/* Phase 2: actual ping sweep scan */
static void lan_tester_do_ping_sweep(LanTesterApp* app) {
    furi_string_reset(app->tool_text);

    furi_string_set(app->tool_text, "Getting IP via DHCP...\n");
    lan_tester_update_view(app->text_box_tool, app->tool_text);

    if(!lan_tester_check_dhcp(app)) return;

    wiz_NetInfo net_info;
    wizchip_getnetinfo(&net_info);

    /* Parse CIDR input; if invalid, auto-detect from DHCP network */
    uint8_t base_ip[4];
    uint8_t prefix;
    uint8_t mask[4];

    if(parse_cidr(app->ping_sweep_ip_input, base_ip, &prefix)) {
        /* User provided valid CIDR */
        uint32_t mask32 = prefix ? (0xFFFFFFFF << (32 - prefix)) : 0;
        mask[0] = (uint8_t)(mask32 >> 24);
        mask[1] = (uint8_t)(mask32 >> 16);
        mask[2] = (uint8_t)(mask32 >> 8);
        mask[3] = (uint8_t)(mask32);
    } else {
        /* Auto-detect from DHCP */
        wiz_NetInfo auto_info;
        wizchip_getnetinfo(&auto_info);
        memcpy(base_ip, auto_info.ip, 4);
        memcpy(mask, auto_info.sn, 4);
        prefix = arp_mask_to_prefix(mask);
        /* Calculate network address */
        for(int i = 0; i < 4; i++)
            base_ip[i] &= mask[i];
        snprintf(
            app->ping_sweep_ip_input,
            sizeof(app->ping_sweep_ip_input),
            "%d.%d.%d.%d/%d",
            base_ip[0],
            base_ip[1],
            base_ip[2],
            base_ip[3],
            prefix);
    }

    uint8_t start_ip[4], end_ip[4];
    uint16_t num_hosts = arp_calc_scan_range(base_ip, mask, start_ip, end_ip);

    if(num_hosts == 0) {
        furi_string_set(app->tool_text, "No hosts in range.\n");
        return;
    }

    furi_string_printf(
        app->tool_text,
        "[PingSweep]=======00%%\n"
        "Alive: 0/0/%d\n",
        num_hosts);
    lan_tester_update_view(app->text_box_tool, app->tool_text);

    /* Sweep — results written to file, no memory cap */
    uint32_t current = pkt_read_u32_be(start_ip);
    uint32_t last = pkt_read_u32_be(end_ip);
    uint16_t scanned = 0;
    uint16_t alive = 0;
    scan_results_clear();
    app->discovered_host_count = 0;
    app->host_list_page = 0;
    scan_results_open_writer();

    /* Keep last 4 IPs in a ring for on-screen display */
    uint8_t recent_ips[4][4];
    uint8_t recent_count = 0;

    while(current <= last && scanned < num_hosts && app->worker_running) {
        uint8_t target[4];
        pkt_write_u32_be(target, current);

        PingResult result;
        bool ok = icmp_ping(
            W5500_PING_SOCKET,
            target,
            (uint16_t)(scanned + 1),
            app->ping_timeout_ms,
            &result,
            &app->worker_running);
        scanned++;

        if(ok) {
            alive++;
            scan_results_add(target, NULL);
            app->discovered_host_count++;
            /* Update ring of recent IPs */
            memcpy(recent_ips[recent_count % 4], target, 4);
            recent_count++;
        }

        /* Update progress every 5 hosts */
        if(scanned % 5 == 0 || current == last) {
            char progress[20];
            lan_tester_progress_bar(progress, 7, scanned, num_hosts);

            furi_string_printf(
                app->tool_text,
                "[PingSweep]%s\n"
                "Alive: %d/%d/%d\n",
                progress,
                alive,
                scanned,
                num_hosts);

            /* Show last few discovered hosts from ring */
            uint8_t show = recent_count < 4 ? recent_count : 4;
            uint8_t start = recent_count < 4 ? 0 : recent_count % 4;
            for(uint8_t j = 0; j < show; j++) {
                char ip_str[16];
                pkt_format_ip(recent_ips[(start + j) % 4], ip_str);
                furi_string_cat_printf(app->tool_text, "%s\n", ip_str);
            }
            lan_tester_update_view(app->text_box_tool, app->tool_text);
        }

        current++;
    }

    scan_results_close_writer();

    /* Final results */
    furi_string_printf(
        app->tool_text,
        "[PingSweep] Done\n"
        "%s\n"
        "Scanned: %d\n"
        "Alive: %d\n",
        app->ping_sweep_ip_input,
        scanned,
        alive);

    if(alive == 0) {
        furi_string_cat(app->tool_text, "(none)\n");
    }

    lan_tester_save_and_notify(app, "ping_sweep.txt", app->tool_text);
    furi_string_reset(app->tool_text);

    /* Show interactive host list if hosts were found (even if scan was interrupted) */
    if(app->discovered_host_count > 0) {
        view_dispatcher_send_custom_event(app->view_dispatcher, CUSTOM_EVENT_SHOW_HOST_LIST);
    }
}

/* ==================== mDNS / SSDP Discovery ==================== */

static void lan_tester_do_discovery(LanTesterApp* app) {
    furi_string_reset(app->tool_text);

    furi_string_set(app->tool_text, "Getting IP via DHCP...\n");
    lan_tester_update_view(app->text_box_tool, app->tool_text);

    if(!lan_tester_check_dhcp(app)) return;

    wiz_NetInfo net_info;
    wizchip_getnetinfo(&net_info);

    furi_string_set(app->tool_text, "Sending mDNS + SSDP...\n");
    lan_tester_update_view(app->text_box_tool, app->tool_text);

    /* Send both queries */
    bool mdns_ok = mdns_send_query(W5500_MDNS_SOCKET);
    bool ssdp_ok = ssdp_send_msearch(W5500_SSDP_SOCKET);

    if(!mdns_ok && !ssdp_ok) {
        furi_string_set(app->tool_text, "Failed to send queries!\n");
        return;
    }

    furi_string_set(app->tool_text, "[Discovery]\nListening...\n");
    lan_tester_update_view(app->text_box_tool, app->tool_text);

/* Compact dedup array: only IP+source (5 bytes each vs 88 bytes per DiscoveryDevice).
     * Static to keep it off the 4KB worker stack. */
#define DISCOVERY_MAX_SEEN 32
    static struct {
        uint8_t ip[4];
        uint8_t source;
    } seen[DISCOVERY_MAX_SEEN];
    uint16_t seen_count = 0;
    uint16_t device_count = 0;

    uint8_t* recv_buf = app->frame_buf;
    uint32_t start_tick = furi_get_tick();

    while(furi_get_tick() - start_tick < DISCOVERY_TIMEOUT_MS && seen_count < DISCOVERY_MAX_SEEN &&
          app->worker_running) {
        /* Check mDNS socket */
        if(mdns_ok) {
            uint16_t rx = getSn_RX_RSR(W5500_MDNS_SOCKET);
            if(rx > 0) {
                uint8_t from_ip[4];
                uint16_t from_port;
                int32_t received =
                    recvfrom(W5500_MDNS_SOCKET, recv_buf, FRAME_BUF_SIZE, from_ip, &from_port);
                if(received > 0) {
                    DiscoveryDevice dev;
                    if(mdns_parse_response(recv_buf, (uint16_t)received, from_ip, &dev)) {
                        bool dup = false;
                        for(uint16_t i = 0; i < seen_count; i++) {
                            if(memcmp(seen[i].ip, dev.ip, 4) == 0 &&
                               seen[i].source == (uint8_t)dev.source) {
                                dup = true;
                                break;
                            }
                        }
                        if(!dup) {
                            memcpy(seen[seen_count].ip, dev.ip, 4);
                            seen[seen_count].source = (uint8_t)dev.source;
                            seen_count++;
                            device_count++;
                            char ip_str[16];
                            pkt_format_ip(dev.ip, ip_str);
                            furi_string_cat_printf(
                                app->tool_text,
                                "%s [mDNS]\n %s\n %s\n",
                                ip_str,
                                dev.name,
                                dev.service_type);
                            lan_tester_update_view(app->text_box_tool, app->tool_text);
                        }
                    }
                }
            }
        }

        /* Check SSDP socket */
        if(ssdp_ok) {
            uint16_t rx = getSn_RX_RSR(W5500_SSDP_SOCKET);
            if(rx > 0) {
                uint8_t from_ip[4];
                uint16_t from_port;
                int32_t received =
                    recvfrom(W5500_SSDP_SOCKET, recv_buf, FRAME_BUF_SIZE, from_ip, &from_port);
                if(received > 0) {
                    DiscoveryDevice dev;
                    if(ssdp_parse_response(recv_buf, (uint16_t)received, from_ip, &dev)) {
                        bool dup = false;
                        for(uint16_t i = 0; i < seen_count; i++) {
                            if(memcmp(seen[i].ip, dev.ip, 4) == 0 &&
                               seen[i].source == (uint8_t)dev.source) {
                                dup = true;
                                break;
                            }
                        }
                        if(!dup) {
                            memcpy(seen[seen_count].ip, dev.ip, 4);
                            seen[seen_count].source = (uint8_t)dev.source;
                            seen_count++;
                            device_count++;
                            char ip_str[16];
                            pkt_format_ip(dev.ip, ip_str);
                            furi_string_cat_printf(
                                app->tool_text,
                                "%s [SSDP]\n %s\n %s\n",
                                ip_str,
                                dev.name,
                                dev.service_type);
                            lan_tester_update_view(app->text_box_tool, app->tool_text);
                        }
                    }
                }
            }
        }

        furi_delay_ms(50);
    }

    /* Close sockets */
    if(mdns_ok) close(W5500_MDNS_SOCKET);
    if(ssdp_ok) close(W5500_SSDP_SOCKET);

    /* Final header with count */
    {
        FuriString* result = furi_string_alloc();
        furi_string_printf(result, "[Discovery]\nFound %d device(s)\n", device_count);
        /* Append the accumulated device lines (skip the old header) */
        const char* body = furi_string_get_cstr(app->tool_text);
        const char* first_dev = strchr(body, '\n');
        if(first_dev) {
            first_dev = strchr(first_dev + 1, '\n');
            if(first_dev) furi_string_cat_str(result, first_dev);
        }
        furi_string_set(app->tool_text, result);
        furi_string_free(result);
    }

    if(device_count == 0) {
        furi_string_cat_str(app->tool_text, "No devices found.\n");
    }

    lan_tester_save_and_notify(app, "discovery.txt", app->tool_text);
}

/* ==================== STP/BPDU + VLAN Detection ==================== */

static void lan_tester_do_stp_vlan(LanTesterApp* app) {
    if(!lan_tester_check_w5500(app)) return;

    if(!w5500_hal_get_link_status()) {
        furi_string_set(app->tool_text, "No Link!\nConnect cable.\n");
        return;
    }

    furi_string_set(app->tool_text, "Listening for BPDU\nand VLAN tags...\n(30s remaining)\n");
    lan_tester_update_view(app->text_box_tool, app->tool_text);

    /* Open MACRAW socket */
    if(!w5500_hal_open_macraw()) {
        furi_string_set(app->tool_text, "Failed to open\nMACRAW socket!\n");
        return;
    }

    BpduInfo bpdu;
    memset(&bpdu, 0, sizeof(bpdu));

    VlanState vlan_state;
    vlan_state_init(&vlan_state);

    uint32_t start_tick = furi_get_tick();
    uint32_t timeout_ms = 30000;
    uint32_t last_update = 0;
    uint32_t last_countdown = 0;

    while(furi_get_tick() - start_tick < timeout_ms && app->worker_running) {
        /* Update countdown */
        uint32_t elapsed_sec = (furi_get_tick() - start_tick) / 1000;
        if(elapsed_sec != last_countdown && !bpdu.valid) {
            last_countdown = elapsed_sec;
            furi_string_printf(
                app->tool_text,
                "Listening for BPDU\nand VLAN tags...\n(%lus remaining)\n",
                (unsigned long)(30 - elapsed_sec));
            lan_tester_update_view(app->text_box_tool, app->tool_text);
        }

        uint16_t recv_len = w5500_hal_macraw_recv(app->frame_buf, FRAME_BUF_SIZE);
        if(recv_len >= ETH_HEADER_SIZE) {
            /* Count frame for stats */
            lan_tester_count_frame(app, app->frame_buf, recv_len);

            /* Check for BPDU */
            if(!bpdu.valid) {
                stp_parse_bpdu(app->frame_buf, recv_len, &bpdu);
            }

            /* Check for 802.1Q VLAN tag */
            uint16_t vlan_id;
            if(vlan_extract_tag(app->frame_buf, recv_len, &vlan_id)) {
                vlan_state_add(&vlan_state, vlan_id);
            }
        }

        /* Update display every 2 seconds */
        uint32_t elapsed = furi_get_tick() - start_tick;
        if(elapsed - last_update > 2000) {
            last_update = elapsed;
            furi_string_reset(app->tool_text);
            furi_string_printf(
                app->tool_text,
                "Listening... %lus/%lus\n\n",
                (unsigned long)(elapsed / 1000),
                (unsigned long)(timeout_ms / 1000));

            if(bpdu.valid) {
                /* Static to avoid 256B stack usage; worker is single-threaded */
                static char bpdu_buf[256];
                stp_format_bpdu(&bpdu, bpdu_buf, sizeof(bpdu_buf));
                furi_string_cat_str(app->tool_text, bpdu_buf);
            } else {
                furi_string_cat_str(app->tool_text, "No BPDU detected yet.\n");
            }

            furi_string_cat_str(app->tool_text, "\n--- VLANs ---\n");
            if(vlan_state.vlan_count > 0) {
                for(uint16_t i = 0; i < vlan_state.vlan_count; i++) {
                    furi_string_cat_printf(
                        app->tool_text,
                        "VLAN %d: %lu frames\n",
                        vlan_state.vlans[i].vlan_id,
                        (unsigned long)vlan_state.vlans[i].frame_count);
                }
            } else {
                furi_string_cat_str(app->tool_text, "No 802.1Q tags.\n");
            }

            lan_tester_update_view(app->text_box_tool, app->tool_text);
        }

        furi_delay_ms(50);
    }

    w5500_hal_close_macraw();

    /* Format final results */
    furi_string_reset(app->tool_text);

    if(bpdu.valid) {
        /* Static to avoid 256B stack usage; worker is single-threaded */
        static char bpdu_buf[256];
        stp_format_bpdu(&bpdu, bpdu_buf, sizeof(bpdu_buf));
        furi_string_cat_str(app->tool_text, bpdu_buf);
    } else {
        furi_string_set(app->tool_text, "[STP/VLAN]\nNo BPDU detected.\n");
    }

    furi_string_cat_str(app->tool_text, "\n--- VLANs ---\n");
    if(vlan_state.vlan_count > 0) {
        furi_string_cat_printf(
            app->tool_text, "Tagged frames: %lu\n", (unsigned long)vlan_state.total_tagged_frames);
        for(uint16_t i = 0; i < vlan_state.vlan_count; i++) {
            furi_string_cat_printf(
                app->tool_text,
                "VLAN %d: %lu frames\n",
                vlan_state.vlans[i].vlan_id,
                (unsigned long)vlan_state.vlans[i].frame_count);
        }
    } else {
        furi_string_cat_str(app->tool_text, "No 802.1Q tags detected.\n(Not on trunk port?)\n");
    }

    lan_tester_save_and_notify(app, "stp_vlan.txt", app->tool_text);
}

/* ==================== History Browser ==================== */

static void lan_tester_history_populate(LanTesterApp* app) {
    submenu_reset(app->submenu_history);

    /* Free previous state if any */
    if(app->history_state) {
        free(app->history_state);
        app->history_state = NULL;
    }

    app->history_state = malloc(sizeof(HistoryState));
    if(!app->history_state) return;

    uint16_t count = history_list(app->history_state);

    if(count == 0) {
        submenu_add_item(app->submenu_history, "No saved results", 0, NULL, NULL);
        return;
    }

    for(uint16_t i = 0; i < count; i++) {
        HistoryEntry* e = &app->history_state->files[i];
        /* Label already built by history_list() */
        submenu_add_item(app->submenu_history, e->label, i, lan_tester_history_file_callback, app);
    }
}

static void lan_tester_history_file_callback(void* context, uint32_t index) {
    LanTesterApp* app = context;
    furi_assert(app);

    if(!app->history_state || index >= app->history_state->file_count) return;

    app->history_selected = index;
    app->tool_back_view = LanTesterViewHistory;

    /* Copy filename before freeing history_state */
    char filename[HISTORY_FILENAME_LEN];
    strncpy(filename, app->history_state->files[index].filename, sizeof(filename));

    /* Free history_state to reclaim ~1 KB before reading file */
    free(app->history_state);
    app->history_state = NULL;

    /* Shrink tool_text to release bloated FuriString buffer from prior tools */
    furi_string_reset(app->tool_text);

    /* Use frame_buf if available (W5500 was initialized), else small malloc */
    char* buf;
    uint16_t buf_size;
    bool need_free = false;
    if(app->frame_buf) {
        buf = (char*)app->frame_buf;
        buf_size = FRAME_BUF_SIZE;
    } else {
        buf = malloc(512);
        buf_size = 512;
        need_free = true;
    }

    if(!buf) {
        furi_string_set(app->tool_text, "Out of memory!\n");
    } else if(history_read_file(filename, buf, buf_size)) {
        furi_string_set(app->tool_text, buf);
    } else {
        furi_string_printf(app->tool_text, "Read failed: %s\n", filename);
    }
    if(need_free) free(buf);

    text_box_reset(app->text_box_tool);
    text_box_set_text(app->text_box_tool, furi_string_get_cstr(app->tool_text));
    view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewToolResult);
}

/* ==================== Port Scanner ==================== */

static void lan_tester_do_port_scan(LanTesterApp* app) {
    furi_string_reset(app->tool_text);

    furi_string_set(app->tool_text, "Getting IP via DHCP...\n");
    lan_tester_update_view(app->text_box_tool, app->tool_text);

    if(!lan_tester_check_dhcp(app)) return;

    wiz_NetInfo net_info;
    wizchip_getnetinfo(&net_info);

    char target_str[16];
    pkt_format_ip(app->port_scan_target, target_str);

    /* Select port list: preset or custom range */
    const uint16_t* ports = NULL;
    uint16_t port_count;
    uint16_t custom_start = 0, custom_end = 0;

    if(app->port_scan_custom) {
        custom_start = app->port_scan_custom_start;
        custom_end = app->port_scan_custom_end;
        port_count = custom_end - custom_start + 1;
    } else if(app->port_scan_top100) {
        ports = PORT_PRESET_TOP100;
        port_count = PORT_PRESET_TOP100_COUNT;
    } else {
        ports = PORT_PRESET_TOP20;
        port_count = PORT_PRESET_TOP20_COUNT;
    }

    if(app->port_scan_custom) {
        furi_string_printf(
            app->tool_text,
            "[Port Scan]\n"
            "Target: %s\n"
            "Range: %d-%d\n\n"
            "Scanning...\n",
            target_str,
            custom_start,
            custom_end);
    } else {
        furi_string_printf(
            app->tool_text,
            "[Port Scan]\n"
            "Target: %s\n"
            "Ports: Top %d\n\n"
            "Scanning...\n",
            target_str,
            port_count);
    }
    lan_tester_update_view(app->text_box_tool, app->tool_text);

    /* Scan ports and collect results */
    uint16_t open_count = 0;
    uint16_t closed_count = 0;
    uint16_t filtered_count = 0;

    /* Build results string progressively */
    FuriString* results = furi_string_alloc();

    for(uint16_t i = 0; i < port_count && app->worker_running; i++) {
        uint16_t port = app->port_scan_custom ? (custom_start + i) : ports[i];

        PortState state = port_scan_tcp(
            W5500_SCAN_SOCKET_BASE, app->port_scan_target, port, PORT_SCAN_TIMEOUT_MS);

        const char* state_str;
        switch(state) {
        case PortStateOpen:
            state_str = "OPEN";
            open_count++;
            break;
        case PortStateClosed:
            state_str = "CLOSED";
            closed_count++;
            break;
        default:
            state_str = "FILTERED";
            filtered_count++;
            break;
        }

        /* Only show open ports in detail, summarize others */
        if(state == PortStateOpen) {
            furi_string_cat_printf(results, "  %d: %s\n", port, state_str);
        }

        /* Update progress */
        {
            char progress[28];
            lan_tester_progress_bar(progress, 16, i + 1, port_count);
            furi_string_printf(
                app->tool_text,
                "[Port Scan] %s\n"
                "%s\n\n"
                "Open ports:\n%s",
                target_str,
                progress,
                furi_string_get_cstr(results));
        }
        lan_tester_update_view(app->text_box_tool, app->tool_text);
    }

    /* Final results */
    furi_string_printf(
        app->tool_text,
        "[Port Scan]\n"
        "Target: %s\n"
        "Scanned: %d ports\n\n"
        "Open: %d  Closed: %d\n"
        "Filtered: %d\n\n",
        target_str,
        port_count,
        open_count,
        closed_count,
        filtered_count);

    if(open_count > 0) {
        furi_string_cat_str(app->tool_text, "Open ports:\n");
        furi_string_cat(app->tool_text, results);
    } else {
        furi_string_cat_str(app->tool_text, "No open ports found.\n");
    }

    furi_string_free(results);

    lan_tester_save_and_notify(app, "port_scan.txt", app->tool_text);
}

/* ==================== Continuous Ping ==================== */

static void lan_tester_do_cont_ping(LanTesterApp* app) {
    if(!lan_tester_ensure_dhcp(app)) return;

    /* Allocate ping graph state */
    PingGraphState* pg = malloc(sizeof(PingGraphState));
    if(!pg) return;
    ping_graph_init(pg);
    app->ping_graph = pg;

    /* Update view model */
    with_view_model(app->view_cont_ping, ContPingViewModel * vm, { vm->app = app; }, true);

    /* Continuous ping loop */
    uint16_t seq = 1;
    while(app->worker_running) {
        PingResult result;
        bool ok = icmp_ping(
            W5500_PING_SOCKET,
            app->cont_ping_target,
            seq,
            app->ping_timeout_ms,
            &result,
            &app->worker_running);

        if(ok) {
            ping_graph_add_sample(pg, result.rtt_ms);
        } else {
            ping_graph_add_sample(pg, PING_RTT_TIMEOUT);
        }

        /* Trigger view redraw */
        with_view_model(app->view_cont_ping, ContPingViewModel * vm, { UNUSED(vm); }, true);

        seq++;

        /* Wait for the remainder of the interval (account for ping duration) */
        uint32_t elapsed = ok ? result.rtt_ms : (uint32_t)app->ping_timeout_ms;
        if(elapsed < app->ping_interval_ms) {
            /* Check running flag periodically during wait */
            uint32_t remaining = app->ping_interval_ms - elapsed;
            uint32_t wait_start = furi_get_tick();
            while(app->worker_running && (furi_get_tick() - wait_start < remaining)) {
                furi_delay_ms(50);
            }
        }
    }

    /* Save results to SD card */
    FuriString* log = furi_string_alloc();
    char target_str[16];
    pkt_format_ip(app->cont_ping_target, target_str);
    furi_string_printf(
        log,
        "Continuous Ping: %s\n"
        "Sent: %lu Received: %lu\n"
        "Loss: %d%%\n"
        "Min: %lu ms Avg: %lu ms Max: %lu ms\n",
        target_str,
        (unsigned long)pg->total_sent,
        (unsigned long)pg->total_received,
        ping_graph_loss_percent(pg),
        (unsigned long)((pg->rtt_min == UINT32_MAX) ? 0 : pg->rtt_min),
        (unsigned long)ping_graph_avg_rtt(pg),
        (unsigned long)pg->rtt_max);
    if(app->setting_autosave) {
        lan_tester_save_results("cont_ping.txt", furi_string_get_cstr(log));
    }
    furi_string_free(log);

    /* Cleanup */
    app->ping_graph = NULL;
    free(pg);
}

/* ==================== Packet statistics ==================== */

static void lan_tester_count_frame(LanTesterApp* app, const uint8_t* frame, uint16_t len) {
    if(len < ETH_HEADER_SIZE) return;

    app->stats.total_frames++;

    /* Classify by destination MAC */
    uint8_t dst[6];
    pkt_get_dst_mac(frame, dst);
    if(pkt_is_broadcast(dst)) {
        app->stats.broadcast_frames++;
    } else if(pkt_is_multicast(dst)) {
        app->stats.multicast_frames++;
    } else {
        app->stats.unicast_frames++;
    }

    /* Classify by EtherType */
    uint16_t ethertype = pkt_get_ethertype(frame);
    switch(ethertype) {
    case ETHERTYPE_IPV4:
        app->stats.ipv4_frames++;
        break;
    case ETHERTYPE_ARP:
        app->stats.arp_frames++;
        break;
    case ETHERTYPE_IPV6:
        app->stats.ipv6_frames++;
        break;
    case ETHERTYPE_LLDP:
        app->stats.lldp_frames++;
        break;
    default:
        /* Check for CDP (length field + LLC/SNAP) */
        if(ethertype < 0x0600 && len >= 22) {
            const uint8_t cdp_mac[] = CDP_DST_MAC;
            if(memcmp(frame, cdp_mac, 6) == 0) {
                app->stats.cdp_frames++;
                break;
            }
        }
        app->stats.unknown_frames++;
        break;
    }
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

/* ==================== Save results to SD card ==================== */

static bool lan_tester_save_results(const char* type, const char* content) {
    /* Extract scan type from filename (remove .txt extension if present) */
    char scan_type[32];
    strncpy(scan_type, type, sizeof(scan_type) - 1);
    scan_type[sizeof(scan_type) - 1] = '\0';
    uint16_t len = strlen(scan_type);
    if(len > 4 && strcmp(&scan_type[len - 4], ".txt") == 0) {
        scan_type[len - 4] = '\0';
    }

    return history_save(scan_type, content);
}

/* Save results and append status to the display text, with optional LED/vibro feedback */
static void lan_tester_save_and_notify(LanTesterApp* app, const char* type, FuriString* text) {
    if(app->setting_autosave) {
        bool ok = lan_tester_save_results(type, furi_string_get_cstr(text));
        furi_string_cat_str(text, ok ? "Saved to History\n" : "Save failed\n");
    }
    if(app->setting_sound) {
        notification_message(app->notifications, &sequence_success);
    }
}

/* ==================== ETH Bridge ==================== */

static void lan_tester_do_eth_bridge(LanTesterApp* app) {
/* Helper macro for status updates */
#define BRIDGE_SET_STATUS(msg)       \
    with_view_model(                 \
        app->view_bridge,            \
        BridgeViewModel* vm,         \
        {                            \
            vm->active = false;      \
            vm->status_line = (msg); \
        },                           \
        true)

    /* Guard: bridge_state must have been allocated at startup */
    if(!app->bridge_state) {
        BRIDGE_SET_STATUS("Bridge state alloc\nfailed at startup.");
        if(app->setting_sound) notification_message(app->notifications, &sequence_error);
        return;
    }

    /* Step 1: Initialize W5500 */
    if(!lan_tester_ensure_w5500(app)) {
        BRIDGE_SET_STATUS("W5500 Not Found!\nCheck SPI wiring.");
        if(app->setting_sound) notification_message(app->notifications, &sequence_error);
        return;
    }

    /* Step 2: Check link */
    if(!w5500_hal_get_link_status()) {
        BRIDGE_SET_STATUS("No LAN link!\nConnect Ethernet cable.");
        if(app->setting_sound) notification_message(app->notifications, &sequence_error);
        return;
    }

    /* Read PHY info */
    bool link_up = false;
    uint8_t speed = 0, duplex = 0;
    w5500_hal_get_phy_info(&link_up, &speed, &duplex);

    /* Step 3: Open MACRAW socket */
    if(!w5500_hal_open_macraw()) {
        BRIDGE_SET_STATUS("Failed to open MACRAW!");
        if(app->setting_sound) notification_message(app->notifications, &sequence_error);
        return;
    }

    /* Step 4: Initialize USB CDC-ECM */
    BRIDGE_SET_STATUS("Starting USB Network...");

    if(!usb_eth_init()) {
        BRIDGE_SET_STATUS("USB init failed!");
        w5500_hal_close_macraw();
        if(app->setting_sound) notification_message(app->notifications, &sequence_error);
        return;
    }

    /* Step 5: Initialize bridge state and activate the view */
    eth_bridge_init(app->bridge_state);

    with_view_model(
        app->view_bridge,
        BridgeViewModel * vm,
        {
            vm->active = true;
            vm->usb_connected = false;
            vm->lan_link_up = link_up;
            vm->lan_speed = speed;
            vm->lan_duplex = duplex;
            vm->frames_to_eth = 0;
            vm->frames_to_usb = 0;
            vm->errors = 0;
        },
        true);

    if(app->setting_sound) notification_message(app->notifications, &sequence_success);

    /* Step 6: Bridge loop */
    uint32_t update_tick = 0;
    while(app->worker_running) {
        eth_bridge_poll(app->bridge_state, app->frame_buf, 1518);

        /* Update display every ~500ms (256 * 100us ≈ 25ms, so use 0x1FFF ≈ 800ms) */
        update_tick++;
        if((update_tick & 0x1FFF) == 0) {
            EthBridgeState* bs = app->bridge_state;
            with_view_model(
                app->view_bridge,
                BridgeViewModel * vm,
                {
                    vm->usb_connected = bs->usb_connected;
                    vm->lan_link_up = bs->lan_link_up;
                    vm->frames_to_eth = bs->frames_usb_to_eth;
                    vm->frames_to_usb = bs->frames_eth_to_usb;
                    vm->errors = bs->errors;
                    vm->dump_active = bs->dump_enabled && bs->pcap.active;
                    vm->dump_frames = bs->pcap.frames_written;
                    vm->dump_dropped = bs->pcap.frames_dropped;
                },
                true);
        }

        furi_delay_us(100);
    }

    /* Stop PCAP dump if active */
    if(app->bridge_state->dump_enabled) {
        app->bridge_state->dump_enabled = false;
        pcap_dump_stop(&app->bridge_state->pcap);
    }

    /* Cleanup */
    usb_eth_deinit();
    w5500_hal_close_macraw();

    EthBridgeState* bs = app->bridge_state;
    FURI_LOG_I(
        TAG,
        "ETH Bridge stopped: USB->ETH=%lu ETH->USB=%lu err=%lu",
        bs->frames_usb_to_eth,
        bs->frames_eth_to_usb,
        bs->errors);

    /* Show final stats */
    with_view_model(
        app->view_bridge,
        BridgeViewModel * vm,
        {
            vm->active = false;
            vm->status_line = "Bridge stopped. USB restored.";
        },
        true);

#undef BRIDGE_SET_STATUS
}

/* ==================== PXE Server ==================== */

static void lan_tester_do_pxe_server(LanTesterApp* app) {
    FuriString* out = app->tool_text;
    furi_string_reset(out);

    /* Step 1: Init W5500 */
    if(!lan_tester_ensure_w5500(app)) {
        furi_string_cat(out, "[PXE] W5500 Not Found!\nCheck SPI wiring.\n");
        return;
    }

    /* Step 2: Check link */
    if(!w5500_hal_get_link_status()) {
        furi_string_cat(out, "[PXE] No LAN link!\nConnect Ethernet cable.\n");
        return;
    }

    /* Step 3: Use boot file selected in settings (already scanned on entry) */
    PxeServerState state;
    memset(&state, 0, sizeof(state));

    if(!app->pxe_scan.boot_file_found) {
        furi_string_printf(
            out,
            "[PXE] No boot file!\n"
            "Place .kpxe or .efi in:\n"
            "%s/\n"
            "Recommended:\n"
            "undionly.kpxe from\n"
            "netboot.xyz (~70KB)\n",
            PXE_BOOT_DIR);
        return;
    }

    /* Copy selected boot file info */
    strncpy(state.boot_filename, app->pxe_scan.boot_filename, sizeof(state.boot_filename) - 1);
    state.boot_file_size = app->pxe_scan.boot_file_size;
    state.boot_file_found = true;

    /* Step 4: Build config from settings */
    state.config.dhcp_enabled = app->pxe_dhcp_enabled;
    memcpy(state.config.client_ip, app->pxe_client_ip, 4);
    memcpy(state.config.subnet, app->pxe_subnet, 4);

    /* Step 5: When built-in DHCP is OFF and we already have a DHCP lease,
     * use the real IP so the network can reach our TFTP server. */
    if(!state.config.dhcp_enabled && app->dhcp_valid) {
        memcpy(state.config.server_ip, app->dhcp_ip, 4);
        memcpy(state.config.subnet, app->dhcp_mask, 4);
        w5500_hal_set_net_info(app->dhcp_ip, app->dhcp_mask, app->dhcp_gw, app->dhcp_dns);
    } else {
        memcpy(state.config.server_ip, app->pxe_server_ip, 4);
        w5500_hal_set_net_info(
            state.config.server_ip,
            state.config.subnet,
            state.config.server_ip,
            state.config.server_ip);
    }

    /* Step 6: Open sockets */
    if(!pxe_server_start(&state)) {
        furi_string_cat(out, "\n[PXE] Failed to open sockets!\n");
        lan_tester_update_view(app->text_box_tool, out);
        return;
    }

    /* Step 7: Initial status */
    furi_string_printf(
        out,
        "[PXE Server]\n"
        "IP: %d.%d.%d.%d\n"
        "DHCP: %s  Files: %d\n"
        "Waiting for client...\n",
        state.config.server_ip[0],
        state.config.server_ip[1],
        state.config.server_ip[2],
        state.config.server_ip[3],
        state.config.dhcp_enabled ? "ON" : "OFF",
        state.boot_file_count);
    lan_tester_update_view(app->text_box_tool, out);

    /* Step 8: Main loop */
    state.running = true;
    PxeState prev_state = PxeStateIdle;
    uint32_t prev_blocks = 0;

    while(app->worker_running && state.running) {
        pxe_server_poll(&state, app->frame_buf, 1024);

        /* Update UI on state change or every 16 blocks */
        bool need_update = (state.state != prev_state) ||
                           (state.tftp_blocks_sent - prev_blocks >= 16);

        if(need_update) {
            prev_state = state.state;
            prev_blocks = state.tftp_blocks_sent;

            furi_string_reset(out);
            furi_string_printf(
                out,
                "[PXE Server]\nIP: %d.%d.%d.%d  DHCP:%s\n",
                state.config.server_ip[0],
                state.config.server_ip[1],
                state.config.server_ip[2],
                state.config.server_ip[3],
                state.config.dhcp_enabled ? "ON" : "OFF");
            if(state.boot_file_count > 1) {
                furi_string_cat_printf(out, "Boot: auto (%d files)\n", state.boot_file_count);
            } else {
                furi_string_cat_printf(out, "Boot: %s\n", state.boot_filename);
            }

            switch(state.state) {
            case PxeStateIdle:
                furi_string_cat(out, "Waiting for client...\n");
                break;
            case PxeStateDhcpOfferSent:
            case PxeStateDhcpAckSent:
                furi_string_cat_printf(
                    out,
                    "Client: %02X:%02X:%02X:%02X:%02X:%02X\n"
                    "DHCP handshake...\n",
                    state.client_mac[0],
                    state.client_mac[1],
                    state.client_mac[2],
                    state.client_mac[3],
                    state.client_mac[4],
                    state.client_mac[5]);
                break;
            case PxeStateTftpTransfer: {
                uint8_t pct = state.boot_file_size ?
                                  (uint8_t)((state.tftp.bytes_sent * 100) / state.boot_file_size) :
                                  0;
                uint8_t filled = pct / 5;
                char bar[23];
                bar[0] = '[';
                for(int i = 0; i < 20; i++)
                    bar[i + 1] = (i < filled) ? '#' : '.';
                bar[21] = ']';
                bar[22] = 0;
                furi_string_cat_printf(out, "%s %d%%\n", bar, pct);
                furi_string_cat_printf(
                    out,
                    "Blk %d/%d (%lu/%lu B)\n",
                    state.tftp.block_num,
                    (uint16_t)((state.boot_file_size + TFTP_BLOCK_SIZE - 1) / TFTP_BLOCK_SIZE),
                    state.tftp.bytes_sent,
                    state.boot_file_size);
                break;
            }
            case PxeStateDone:
                furi_string_cat_printf(
                    out,
                    "COMPLETE! %lu B in %lu blk\n",
                    state.tftp.bytes_sent,
                    state.tftp_blocks_sent);
                break;
            case PxeStateError:
                furi_string_cat_printf(out, "ERROR! Errs: %lu\n", state.tftp_errors);
                break;
            }
            lan_tester_update_view(app->text_box_tool, out);
        }

        /* After Done → reset to Idle for next client */
        if(state.state == PxeStateDone) {
            furi_delay_ms(2000); /* Show "COMPLETE" for 2 sec */
            state.state = PxeStateIdle;
            state.client_seen = false;
        }

        furi_delay_ms(10);
    }

    /* Cleanup */
    pxe_server_stop(&state);

    furi_string_printf(
        out,
        "[PXE Stopped]\nDHCP: %lu disc, %lu req\nTFTP: %lu req, %lu blk\nErr: %lu\n",
        state.dhcp_discovers,
        state.dhcp_requests,
        state.tftp_requests,
        state.tftp_blocks_sent,
        state.tftp_errors);
    if(app->setting_sound) notification_message(app->notifications, &sequence_success);
}

/* ==================== Packet Capture ==================== */

/* ==================== Auto Test ==================== */

typedef enum {
    AutoTestStateIdle,
    AutoTestStateTesting,
    AutoTestStateDone,
} AutoTestState;

static void lan_tester_do_autotest(LanTesterApp* app) {
    if(!lan_tester_ensure_w5500(app)) {
        furi_string_set(app->autotest_text, "W5500 Not Found!\nCheck SPI wiring.\n");
        lan_tester_update_view(app->text_box_autotest, app->autotest_text);
        return;
    }

    AutoTestState state = AutoTestStateIdle;

    /* Main loop: IDLE → TESTING → DONE → wait for link loss → IDLE */
    while(app->autotest_running && app->worker_running) {
        bool link = w5500_hal_get_link_status();

        if(state == AutoTestStateIdle) {
            if(!link) {
                furi_string_set(app->autotest_text, "Waiting for link...\n");
                lan_tester_update_view(app->text_box_autotest, app->autotest_text);
                while(app->autotest_running && app->worker_running) {
                    if(w5500_hal_get_link_status()) break;
                    furi_delay_ms(200);
                }
                if(!app->autotest_running || !app->worker_running) break;
                /* Small delay for link to stabilize */
                furi_delay_ms(500);
            }
            state = AutoTestStateTesting;
        }

        if(state == AutoTestStateTesting) {
            FuriString* body = furi_string_alloc();
            bool dhcp_ok = false;
            bool gw_ok = false;
            bool dns_ok = false;

            furi_string_set(app->autotest_text, "[Auto Test]\n");
            lan_tester_update_view(app->text_box_autotest, app->autotest_text);

            /* Step 1: Link Info */
            if(!w5500_hal_get_link_status() || !app->autotest_running) {
                furi_string_free(body);
                state = AutoTestStateIdle;
                continue;
            }
            {
                bool link_up = false;
                uint8_t speed = 0, duplex = 0;
                w5500_hal_get_phy_info(&link_up, &speed, &duplex);
                app->link_up = link_up;
                app->link_speed = speed;
                app->link_duplex = duplex;
                furi_string_cat_printf(
                    body, "Link: UP %sM %s\n", speed ? "100" : "10", duplex ? "Full" : "Half");
            }
            furi_string_set(app->autotest_text, "[Auto Test]\n");
            furi_string_cat(app->autotest_text, body);
            lan_tester_update_view(app->text_box_autotest, app->autotest_text);

            /* Step 2: DHCP */
            if(!w5500_hal_get_link_status() || !app->autotest_running) {
                furi_string_free(body);
                state = AutoTestStateIdle;
                continue;
            }
            app->dhcp_valid = false; /* force fresh DHCP */
            dhcp_ok = lan_tester_ensure_dhcp(app);
            if(dhcp_ok) {
                uint8_t pfx = arp_mask_to_prefix(app->dhcp_mask);
                furi_string_cat_printf(
                    body,
                    "DHCP: %d.%d.%d.%d/%d\n"
                    "GW:   %d.%d.%d.%d\n"
                    "DNS:  %d.%d.%d.%d\n",
                    app->dhcp_ip[0],
                    app->dhcp_ip[1],
                    app->dhcp_ip[2],
                    app->dhcp_ip[3],
                    pfx,
                    app->dhcp_gw[0],
                    app->dhcp_gw[1],
                    app->dhcp_gw[2],
                    app->dhcp_gw[3],
                    app->dhcp_dns[0],
                    app->dhcp_dns[1],
                    app->dhcp_dns[2],
                    app->dhcp_dns[3]);
            } else {
                furi_string_cat_str(body, "DHCP: FAIL\n");
            }
            furi_string_set(app->autotest_text, "[Auto Test]\n");
            furi_string_cat(app->autotest_text, body);
            lan_tester_update_view(app->text_box_autotest, app->autotest_text);

            /* Step 3: Ping Gateway (Socket 2 — no conflict) */
            if(dhcp_ok && w5500_hal_get_link_status() && app->autotest_running) {
                PingResult pr;
                gw_ok = icmp_ping(
                    W5500_PING_SOCKET,
                    app->dhcp_gw,
                    1,
                    app->ping_timeout_ms,
                    &pr,
                    &app->worker_running);
                if(gw_ok) {
                    furi_string_cat_printf(body, "GW ping: %lums\n", (unsigned long)pr.rtt_ms);
                } else {
                    furi_string_cat_str(body, "GW ping: FAIL\n");
                }
                furi_string_set(app->autotest_text, "[Auto Test]\n");
                furi_string_cat(app->autotest_text, body);
                lan_tester_update_view(app->text_box_autotest, app->autotest_text);
            }

            /* Step 4: DNS Resolve (Socket 3 — no conflict) */
            DnsLookupResult dr = {0};
            if(dhcp_ok && w5500_hal_get_link_status() && app->autotest_running) {
                uint8_t dns_ip[4];
                if(app->dns_custom_enabled) {
                    memcpy(dns_ip, app->dns_custom_server, 4);
                } else {
                    memcpy(dns_ip, app->dhcp_dns, 4);
                }
                dns_ok = dns_lookup(W5500_DNS_SOCKET, dns_ip, app->autotest_dns_host, &dr);
                if(dns_ok) {
                    furi_string_cat_printf(
                        body,
                        "DNS: %s -> %d.%d.%d.%d\n",
                        app->autotest_dns_host,
                        dr.resolved_ip[0],
                        dr.resolved_ip[1],
                        dr.resolved_ip[2],
                        dr.resolved_ip[3]);
                } else {
                    furi_string_cat_str(body, "DNS: FAIL\n");
                }
                furi_string_set(app->autotest_text, "[Auto Test]\n");
                furi_string_cat(app->autotest_text, body);
                lan_tester_update_view(app->text_box_autotest, app->autotest_text);
            }

            /* Step 5: Internet Ping (Socket 2 — only if GW ping OK) */
            if(gw_ok && w5500_hal_get_link_status() && app->autotest_running) {
                uint8_t inet_target[4];
                if(dns_ok) {
                    memcpy(inet_target, dr.resolved_ip, 4);
                } else {
                    inet_target[0] = 8;
                    inet_target[1] = 8;
                    inet_target[2] = 8;
                    inet_target[3] = 8;
                }
                PingResult ir;
                bool inet_ok = icmp_ping(
                    W5500_PING_SOCKET,
                    inet_target,
                    2,
                    app->ping_timeout_ms,
                    &ir,
                    &app->worker_running);
                if(inet_ok) {
                    furi_string_cat_printf(body, "Internet: %lums\n", (unsigned long)ir.rtt_ms);
                } else {
                    furi_string_cat_str(body, "Internet: FAIL\n");
                }
                furi_string_set(app->autotest_text, "[Auto Test]\n");
                furi_string_cat(app->autotest_text, body);
                lan_tester_update_view(app->text_box_autotest, app->autotest_text);
            }

            /* Step 6: LLDP/CDP (inline, uses frame_buf — no extra alloc) */
            if(w5500_hal_get_link_status() && app->autotest_running) {
                furi_string_set(app->autotest_text, "[Auto Test]\n");
                furi_string_cat(app->autotest_text, body);
                furi_string_cat_str(app->autotest_text, "LLDP: listening...\n");
                lan_tester_update_view(app->text_box_autotest, app->autotest_text);

                if(w5500_hal_open_macraw()) {
                    LldpNeighbor lldp = {0};
                    CdpNeighbor cdp = {0};
                    bool found_lldp = false;
                    bool found_cdp = false;
                    uint32_t lldp_start = furi_get_tick();
                    uint32_t lldp_timeout_ms = (uint32_t)app->autotest_lldp_wait_s * 1000;

                    while(app->autotest_running &&
                          (furi_get_tick() - lldp_start < lldp_timeout_ms)) {
                        uint16_t recv_len = w5500_hal_macraw_recv(app->frame_buf, FRAME_BUF_SIZE);
                        if(recv_len >= ETH_HEADER_SIZE) {
                            uint16_t ethertype = pkt_get_ethertype(app->frame_buf);
                            if(ethertype == ETHERTYPE_LLDP && !found_lldp) {
                                if(lldp_parse(
                                       app->frame_buf + ETH_HEADER_SIZE,
                                       recv_len - ETH_HEADER_SIZE,
                                       &lldp)) {
                                    found_lldp = true;
                                    break;
                                }
                            }
                            if(!found_cdp) {
                                uint16_t cdp_offset = cdp_check_frame(app->frame_buf, recv_len);
                                if(cdp_offset > 0) {
                                    if(cdp_parse(
                                           app->frame_buf + cdp_offset,
                                           recv_len - cdp_offset,
                                           &cdp)) {
                                        found_cdp = true;
                                    }
                                }
                            }
                        } else {
                            furi_delay_ms(50);
                        }
                    }
                    w5500_hal_close_macraw();

                    if(found_lldp) {
                        furi_string_cat_printf(
                            body,
                            "LLDP: %s %s\n",
                            lldp.system_name[0] ? lldp.system_name : "?",
                            lldp.port_id[0] ? lldp.port_id : "");
                    } else if(found_cdp) {
                        furi_string_cat_printf(
                            body,
                            "CDP: %s %s\n",
                            cdp.device_id[0] ? cdp.device_id : "?",
                            cdp.port_id[0] ? cdp.port_id : "");
                    } else {
                        furi_string_cat_str(body, "LLDP: none\n");
                    }
                    furi_string_set(app->autotest_text, "[Auto Test]\n");
                    furi_string_cat(app->autotest_text, body);
                    lan_tester_update_view(app->text_box_autotest, app->autotest_text);
                }
            }

            /* Step 7: ARP Host Count (Socket 0 — AFTER LLDP) */
            if(dhcp_ok && app->autotest_arp_enabled && w5500_hal_get_link_status() &&
               app->autotest_running) {
                wiz_NetInfo net_info;
                wizchip_getnetinfo(&net_info);
                uint8_t start_ip[4], end_ip[4];
                uint16_t num_hosts =
                    arp_calc_scan_range(net_info.ip, net_info.sn, start_ip, end_ip);
                if(num_hosts > 0 && w5500_hal_open_macraw()) {
                    uint32_t current_ip = pkt_read_u32_be(start_ip);
                    uint32_t last_ip = pkt_read_u32_be(end_ip);
                    uint16_t found_count = 0;
                    uint8_t arp_frame[42];
                    uint16_t batch_count = 0;

                    /* Send ARP requests in batches */
                    while(current_ip <= last_ip && app->autotest_running) {
                        uint8_t target[4];
                        pkt_write_u32_be(target, current_ip);
                        arp_build_request(arp_frame, net_info.mac, net_info.ip, target);
                        w5500_hal_macraw_send(arp_frame, 42);
                        current_ip++;
                        batch_count++;
                        if(batch_count >= ARP_BATCH_SIZE) {
                            batch_count = 0;
                            furi_delay_ms(ARP_BATCH_DELAY_MS);
                            /* Collect replies */
                            for(uint8_t i = 0; i < 20; i++) {
                                uint16_t recv_len =
                                    w5500_hal_macraw_recv(app->frame_buf, FRAME_BUF_SIZE);
                                if(recv_len == 0) break;
                                uint8_t s_mac[6], s_ip[4];
                                if(arp_parse_reply(app->frame_buf, recv_len, s_mac, s_ip)) {
                                    found_count++;
                                }
                            }
                        }
                    }
                    /* Wait for late replies */
                    uint32_t tail_start = furi_get_tick();
                    while(furi_get_tick() - tail_start < ARP_TAIL_WAIT_MS &&
                          app->autotest_running) {
                        uint16_t recv_len = w5500_hal_macraw_recv(app->frame_buf, FRAME_BUF_SIZE);
                        if(recv_len > 0) {
                            uint8_t s_mac[6], s_ip[4];
                            if(arp_parse_reply(app->frame_buf, recv_len, s_mac, s_ip)) {
                                found_count++;
                            }
                        } else {
                            furi_delay_ms(50);
                        }
                    }
                    w5500_hal_close_macraw();
                    furi_string_cat_printf(body, "Hosts: %d in subnet\n", found_count);
                    furi_string_set(app->autotest_text, "[Auto Test]\n");
                    furi_string_cat(app->autotest_text, body);
                    lan_tester_update_view(app->text_box_autotest, app->autotest_text);
                }
            }

            /* Final render with verdict (steps 2-4; internet ping not counted) */
            bool all_ok = dhcp_ok && gw_ok && dns_ok;
            furi_string_reset(app->autotest_text);
            furi_string_cat_str(app->autotest_text, all_ok ? "[Auto Test] OK\n" : "[Auto Test]\n");
            furi_string_cat(app->autotest_text, body);
            furi_string_free(body);
            lan_tester_update_view(app->text_box_autotest, app->autotest_text);

            /* Save to history */
            lan_tester_save_and_notify(app, "autotest.txt", app->autotest_text);

            state = AutoTestStateDone;
        }

        if(state == AutoTestStateDone) {
            /* Wait for link loss */
            while(app->autotest_running && app->worker_running) {
                if(!w5500_hal_get_link_status()) {
                    state = AutoTestStateIdle;
                    break;
                }
                furi_delay_ms(200);
            }
        }
    }
}

/* ==================== Packet Capture ==================== */

static void lan_tester_do_packet_capture(LanTesterApp* app) {
    if(!lan_tester_ensure_w5500(app)) return;

    if(!w5500_hal_open_macraw()) {
        return;
    }

    if(!pcap_dump_start(&app->pcap_state)) {
        w5500_hal_close_macraw();
        return;
    }

    /* Trigger initial draw */
    with_view_model(app->view_packet_capture, PacketCaptureViewModel * vm, { UNUSED(vm); }, true);

    while(app->worker_running) {
        uint16_t recv_len = w5500_hal_macraw_recv(app->frame_buf, FRAME_BUF_SIZE);
        if(recv_len > 0) {
            pcap_dump_frame(&app->pcap_state, app->frame_buf, recv_len);

            /* Trigger view redraw periodically */
            with_view_model(
                app->view_packet_capture, PacketCaptureViewModel * vm, { UNUSED(vm); }, true);
        } else {
            furi_delay_ms(10);
        }
    }

    pcap_dump_stop(&app->pcap_state);
    w5500_hal_close_macraw();

    /* Final redraw to show stopped state */
    with_view_model(app->view_packet_capture, PacketCaptureViewModel * vm, { UNUSED(vm); }, true);

    if(app->setting_sound) {
        notification_message(app->notifications, &sequence_success);
    }
}

/* ==================== File Manager ==================== */

static void lan_tester_do_file_manager(LanTesterApp* app) {
    FuriString* out = app->tool_text;
    furi_string_reset(out);

    /* Step 1: Init W5500 */
    if(!lan_tester_ensure_w5500(app)) {
        furi_string_cat(out, "[File Manager] W5500 Not Found!\nCheck SPI wiring.\n");
        return;
    }

    /* Step 2: Check link */
    if(!w5500_hal_get_link_status()) {
        furi_string_cat(out, "[File Manager] No LAN link!\nConnect Ethernet cable.\n");
        return;
    }

    /* Step 3: Run DHCP to get IP */
    furi_string_set(out, "[File Manager]\nRunning DHCP...\n");
    lan_tester_update_view(app->text_box_tool, out);

    if(!lan_tester_ensure_dhcp(app)) {
        furi_string_set(out, "[File Manager]\nDHCP failed!\n");
        return;
    }

    /* Step 5: Start HTTP server */
    FileManagerState fm_state;
    if(!file_manager_start(&fm_state)) {
        furi_string_cat(out, "[File Manager]\nFailed to start HTTP!\n");
        return;
    }

    /* Step 6: Show compact status with auth token */
    furi_string_printf(
        out,
        "[File Manager] Running\n"
        "http://%d.%d.%d.%d/?t=%s\n"
        "Req:0 Tx:0 Rx:0\n"
        "Press BACK to stop.",
        app->dhcp_ip[0],
        app->dhcp_ip[1],
        app->dhcp_ip[2],
        app->dhcp_ip[3],
        fm_state.auth_token);
    lan_tester_update_view(app->text_box_tool, out);

    /* Step 7: Main loop */
    uint32_t last_status = furi_get_tick();
    while(app->worker_running && fm_state.running) {
        file_manager_poll(&fm_state, app->frame_buf, 1024);

        /* Update status every 2 seconds */
        if(furi_get_tick() - last_status >= 2000) {
            last_status = furi_get_tick();
            furi_string_printf(
                out,
                "[File Manager] Running\n"
                "http://%d.%d.%d.%d/?t=%s\n"
                "Req:%lu Tx:%lu Rx:%lu\n"
                "%s\n"
                "Press BACK to stop.",
                app->dhcp_ip[0],
                app->dhcp_ip[1],
                app->dhcp_ip[2],
                app->dhcp_ip[3],
                fm_state.auth_token,
                (unsigned long)fm_state.requests_served,
                (unsigned long)fm_state.bytes_sent,
                (unsigned long)fm_state.bytes_received,
                fm_state.errors ? "Errors!" : "");
            lan_tester_update_view(app->text_box_tool, out);
        }
    }

    /* Cleanup */
    file_manager_stop(&fm_state);

    furi_string_printf(
        out,
        "[File Manager] Stopped\n"
        "Req:%lu Tx:%lu Rx:%lu",
        (unsigned long)fm_state.requests_served,
        (unsigned long)fm_state.bytes_sent,
        (unsigned long)fm_state.bytes_received);
    if(app->setting_sound) notification_message(app->notifications, &sequence_success);
}

/* ==================== PXE Boot File Download ==================== */

/* Progress callback context */
typedef struct {
    LanTesterApp* app;
    const char* filename;
    size_t base_len; /* length of tool_text before "downloading..." line */
} PxeDownloadCtx;

static void pxe_download_progress_cb(uint32_t bytes_received, void* ctx) {
    PxeDownloadCtx* pctx = ctx;
    /* Truncate back to base text, then append progress line */
    furi_string_left(pctx->app->tool_text, pctx->base_len);
    if(bytes_received < 1024) {
        furi_string_cat_printf(
            pctx->app->tool_text, "%s: %lu B\n", pctx->filename, (unsigned long)bytes_received);
    } else {
        furi_string_cat_printf(
            pctx->app->tool_text,
            "%s: %lu KB\n",
            pctx->filename,
            (unsigned long)(bytes_received / 1024));
    }
    lan_tester_update_view(pctx->app->text_box_tool, pctx->app->tool_text);
}

static void lan_tester_do_pxe_download(LanTesterApp* app) {
    FuriString* out = app->tool_text;
    furi_string_reset(out);

    /* Step 1: Init W5500 */
    if(!lan_tester_ensure_w5500(app)) {
        furi_string_set(out, "[PXE Download] W5500 Not Found!\nCheck SPI wiring.\n");
        return;
    }

    /* Step 2: Check link */
    if(!w5500_hal_get_link_status()) {
        furi_string_set(out, "[PXE Download] No LAN link!\nConnect Ethernet cable.\n");
        return;
    }

    /* Step 3: Run DHCP */
    furi_string_set(out, "[PXE Download]\nRunning DHCP...\n");
    lan_tester_update_view(app->text_box_tool, out);

    if(!lan_tester_ensure_dhcp(app)) {
        furi_string_set(out, "[PXE Download]\nDHCP failed!\n");
        return;
    }

    /* Step 4: Create pxe directory */
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, PXE_BOOT_DIR);
    furi_record_close(RECORD_STORAGE);

    /* Step 5: Download each boot file.
     * ipxe.pxe = native driver (best for Legacy BIOS)
     * undionly.kpxe = UNDI fallback (Legacy BIOS)
     * snponly.efi = UEFI (small, uses firmware SNP)
     * ipxe.efi = UEFI (full native drivers) */
    static const char* filenames[] = {"ipxe.pxe", "undionly.kpxe", "snponly.efi", "ipxe.efi"};
    static const char* url_paths[] = {
        "/ipxe.pxe",
        "/undionly.kpxe",
        "/x86_64-efi/snponly.efi",
        "/x86_64-efi/ipxe.efi",
    };
    static const uint8_t file_count = 4;
    uint8_t ok_count = 0;
    uint8_t skip_count = 0;

    furi_string_set(out, "[PXE Download]\n");
    lan_tester_update_view(app->text_box_tool, out);

    for(uint8_t i = 0; i < file_count && app->worker_running; i++) {
        /* Static to avoid 128B stack usage; worker is single-threaded */
        static char save_path[128];
        snprintf(save_path, sizeof(save_path), PXE_BOOT_DIR "/%s", filenames[i]);

        /* Check if file already exists */
        Storage* st = furi_record_open(RECORD_STORAGE);
        bool exists = (storage_common_stat(st, save_path, NULL) == FSE_OK);
        furi_record_close(RECORD_STORAGE);

        if(exists) {
            furi_string_cat_printf(out, "%s: exists\n", filenames[i]);
            lan_tester_update_view(app->text_box_tool, out);
            skip_count++;
            continue;
        }

        /* Set up progress callback */
        PxeDownloadCtx pctx = {
            .app = app,
            .filename = filenames[i],
            .base_len = furi_string_size(out),
        };
        furi_string_cat_printf(out, "%s: connecting...\n", filenames[i]);
        lan_tester_update_view(app->text_box_tool, out);

        HttpDownloadResult result;
        bool ok = http_download_file(
            W5500_DNS_SOCKET,
            HTTP_CLIENT_SOCKET,
            app->dhcp_dns,
            "boot.ipxe.org",
            url_paths[i],
            save_path,
            app->frame_buf,
            1024,
            &result,
            &app->worker_running,
            pxe_download_progress_cb,
            &pctx);

        /* Replace progress line with final status */
        furi_string_left(out, pctx.base_len);
        if(ok) {
            if(result.bytes_received < 1024) {
                furi_string_cat_printf(
                    out, "%s: OK %lu B\n", filenames[i], (unsigned long)result.bytes_received);
            } else {
                furi_string_cat_printf(
                    out,
                    "%s: OK %lu KB\n",
                    filenames[i],
                    (unsigned long)(result.bytes_received / 1024));
            }
            ok_count++;
        } else {
            furi_string_cat_printf(out, "%s: %s\n", filenames[i], result.error_msg);
        }
        lan_tester_update_view(app->text_box_tool, out);
    }

    furi_string_cat_printf(out, "Done: %d OK, %d skipped\n", ok_count, skip_count);

    if(app->setting_sound) notification_message(app->notifications, &sequence_success);
}

/* ==================== TFTP Client ==================== */

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

/* ==================== IPMI Client ==================== */

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

/* ==================== 802.1X EAPOL Probe ==================== */

static void lan_tester_do_eapol_probe(LanTesterApp* app) {
    if(!lan_tester_check_w5500(app)) return;

    furi_string_cat(app->tool_text, "[802.1X] Scanning...\n");
    lan_tester_update_view(app->text_box_tool, app->tool_text);

    EapolProbeResult result;
    eapol_probe_test(app->mac_addr, &result);

    furi_string_reset(app->tool_text);
    if(!result.eapol_response) {
        furi_string_cat(app->tool_text, "[802.1X] No response\n802.1X likely disabled.\n");
    } else {
        furi_string_cat(app->tool_text, "[802.1X] DETECTED!\n");
        furi_string_cat_printf(
            app->tool_text,
            "Auth: %02X:%02X:%02X:%02X:%02X:%02X\n",
            result.auth_mac[0],
            result.auth_mac[1],
            result.auth_mac[2],
            result.auth_mac[3],
            result.auth_mac[4],
            result.auth_mac[5]);
        if(result.eap_request) {
            const char* t = "Unknown";
            switch(result.eap_type) {
            case 1:
                t = "Identity";
                break;
            case 4:
                t = "MD5";
                break;
            case 13:
                t = "TLS";
                break;
            case 21:
                t = "TTLS";
                break;
            case 25:
                t = "PEAP";
                break;
            }
            furi_string_cat_printf(app->tool_text, "EAP: %s (%d)\n", t, result.eap_type);
        }
        if(result.eap_success) furi_string_cat(app->tool_text, "EAP-Success (open!)\n");
        if(result.eap_failure) furi_string_cat(app->tool_text, "EAP-Failure\n");
        furi_string_cat_printf(app->tool_text, "Frames: %d\n", result.frames_seen);
    }

    lan_tester_save_and_notify(app, "eapol.txt", app->tool_text);
}

/* ==================== VLAN Hopping Test ==================== */

static void lan_tester_do_vlan_hop(LanTesterApp* app) {
    if(!lan_tester_check_w5500(app)) return;

    uint8_t target_ip[4] = {0, 0, 0, 0};
    uint8_t our_ip[4] = {0, 0, 0, 0};
    if(app->dhcp_valid) {
        memcpy(target_ip, app->dhcp_gw, 4);
        memcpy(our_ip, app->dhcp_ip, 4);
    }

    /* Build VLAN list */
    uint16_t test_vlans[32];
    uint8_t num_tests = 0;

    if(app->vlan_hop_custom) {
        /* Parse comma-separated VLAN IDs from user input */
        const char* p = app->vlan_hop_input;
        while(*p && num_tests < 32) {
            while(*p == ' ' || *p == ',')
                p++;
            if(!*p) break;
            int v = atoi(p);
            if(v >= 1 && v <= 4094) {
                test_vlans[num_tests++] = (uint16_t)v;
            }
            while(*p && *p != ',')
                p++;
        }
    } else {
        /* Top 10 common VLANs */
        static const uint16_t top10[] = {1, 2, 10, 20, 50, 100, 150, 200, 300, 999};
        num_tests = 10;
        memcpy(test_vlans, top10, sizeof(top10));
    }

    if(num_tests == 0) {
        furi_string_set(app->tool_text, "No valid VLAN IDs.\n");
        return;
    }

    furi_string_cat_printf(app->tool_text, "[VLAN Hop] %d VLANs\n\n", num_tests);
    furi_string_cat(app->tool_text, "Scanning...\n");
    lan_tester_update_view(app->text_box_tool, app->tool_text);

    /* Collect results */
    uint16_t failed_vlans[32];
    uint8_t failed_count = 0;
    uint16_t stripped_vlans[32];
    uint8_t stripped_count = 0;
    uint16_t isolated_vlans[32];
    uint8_t isolated_count = 0;

    for(uint8_t t = 0; t < num_tests && app->worker_running; t++) {
        VlanHopResult result;
        vlan_hop_test(app->mac_addr, our_ip, target_ip, test_vlans[t], &result);

        if(result.tagged_reply) {
            if(failed_count < 32) failed_vlans[failed_count++] = test_vlans[t];
        } else if(result.native_reply) {
            if(stripped_count < 32) stripped_vlans[stripped_count++] = test_vlans[t];
        } else {
            if(isolated_count < 32) isolated_vlans[isolated_count++] = test_vlans[t];
        }
    }

    /* Compact output */
    furi_string_reset(app->tool_text);
    furi_string_cat_printf(app->tool_text, "[VLAN Hop] %d tested\n", num_tests);
    if(failed_count > 0) {
        furi_string_cat(app->tool_text, "FAIL: ");
        for(uint8_t i = 0; i < failed_count; i++)
            furi_string_cat_printf(app->tool_text, "%s%d", i ? "," : "", failed_vlans[i]);
        furi_string_cat(app->tool_text, "\n");
    }
    if(stripped_count > 0) {
        furi_string_cat(app->tool_text, "Stripped: ");
        for(uint8_t i = 0; i < stripped_count; i++)
            furi_string_cat_printf(app->tool_text, "%s%d", i ? "," : "", stripped_vlans[i]);
        furi_string_cat(app->tool_text, "\n");
    }
    if(isolated_count > 0) {
        furi_string_cat(app->tool_text, "OK: ");
        for(uint8_t i = 0; i < isolated_count; i++)
            furi_string_cat_printf(app->tool_text, "%s%d", i ? "," : "", isolated_vlans[i]);
        furi_string_cat(app->tool_text, "\n");
    }
    if(failed_count > 0)
        furi_string_cat(app->tool_text, "Isolation BROKEN!\n");
    else if(stripped_count > 0)
        furi_string_cat(app->tool_text, "Tags stripped.\n");
    else
        furi_string_cat(app->tool_text, "All isolated OK.\n");

    lan_tester_save_and_notify(app, "vlan_hop.txt", app->tool_text);
}

/* ==================== ARP Watch ==================== */

static void lan_tester_do_arp_watch(LanTesterApp* app) {
    if(!lan_tester_check_w5500(app)) return;

    furi_string_cat(app->tool_text, "[ARP Watch] Scanning...\n");
    lan_tester_update_view(app->text_box_tool, app->tool_text);

    if(!w5500_hal_open_macraw()) {
        furi_string_cat(app->tool_text, "MACRAW open failed!\n");
        return;
    }

    ArpWatchState watch;
    arp_watch_init(&watch);

    uint32_t start = furi_get_tick();
    uint32_t duration_ms = 15000;

    while(app->worker_running && (furi_get_tick() - start) < duration_ms) {
        uint16_t recv_len = w5500_hal_macraw_recv(app->frame_buf, FRAME_BUF_SIZE);
        if(recv_len > 0) {
            arp_watch_process_frame(&watch, app->frame_buf, recv_len);
        }
        furi_delay_ms(1);
    }

    w5500_hal_close_macraw();

    furi_string_cat_printf(
        app->tool_text,
        "ARP packets: %d\nUnique IPs: %d\n",
        watch.total_arp_seen,
        watch.entry_count);

    if(watch.duplicate_count > 0) {
        furi_string_cat_printf(app->tool_text, "\nDUPLICATE IPs: %d\n", watch.duplicate_count);
        for(uint16_t i = 0; i < watch.entry_count; i++) {
            if(watch.entries[i].is_duplicate) {
                furi_string_cat_printf(
                    app->tool_text,
                    "  %d.%d.%d.%d (spoofed!)\n",
                    watch.entries[i].ip[0],
                    watch.entries[i].ip[1],
                    watch.entries[i].ip[2],
                    watch.entries[i].ip[3]);
            }
        }
    }

    if(watch.gratuitous_count > 0) {
        furi_string_cat_printf(app->tool_text, "\nGratuitous ARP: %d\n", watch.gratuitous_count);
    }

    if(watch.storm_detected) {
        furi_string_cat(app->tool_text, "ARP STORM!\n");
    }

    if(watch.duplicate_count == 0 && !watch.storm_detected) {
        furi_string_cat(app->tool_text, "No anomalies.\n");
    }

    /* Show some entries */
    if(watch.entry_count > 0) {
        uint16_t show = watch.entry_count < 10 ? watch.entry_count : 10;
        furi_string_cat(app->tool_text, "Hosts:\n");
        for(uint16_t i = 0; i < show; i++) {
            furi_string_cat_printf(
                app->tool_text,
                "  %d.%d.%d.%d %02X:%02X:%02X:%02X:%02X:%02X (%d)\n",
                watch.entries[i].ip[0],
                watch.entries[i].ip[1],
                watch.entries[i].ip[2],
                watch.entries[i].ip[3],
                watch.entries[i].mac[0],
                watch.entries[i].mac[1],
                watch.entries[i].mac[2],
                watch.entries[i].mac[3],
                watch.entries[i].mac[4],
                watch.entries[i].mac[5],
                watch.entries[i].arp_count);
        }
    }

    lan_tester_save_and_notify(app, "arp_watch.txt", app->tool_text);
}

/* ==================== Rogue DHCP Detection ==================== */

static void lan_tester_do_rogue_dhcp(LanTesterApp* app) {
    if(!lan_tester_check_w5500(app)) return;

    furi_string_cat(app->tool_text, "[Rogue DHCP] Scanning...\n");
    lan_tester_update_view(app->text_box_tool, app->tool_text);

    RogueDhcpState state;
    rogue_dhcp_detect(app->mac_addr, &state, 5000);

    furi_string_reset(app->tool_text);
    furi_string_cat_printf(
        app->tool_text,
        "[Rogue DHCP] %d offer, %d srv\n",
        state.offers_received,
        state.server_count);

    if(state.server_count == 0) {
        furi_string_cat(app->tool_text, "No DHCP servers.\n");
    } else {
        for(uint8_t i = 0; i < state.server_count; i++) {
            RogueDhcpServer* srv = &state.servers[i];
            furi_string_cat_printf(
                app->tool_text,
                "#%d %d.%d.%d.%d",
                i + 1,
                srv->server_ip[0],
                srv->server_ip[1],
                srv->server_ip[2],
                srv->server_ip[3]);
            furi_string_cat_printf(
                app->tool_text,
                " ->%d.%d.%d.%d\n",
                srv->offered_ip[0],
                srv->offered_ip[1],
                srv->offered_ip[2],
                srv->offered_ip[3]);
            furi_string_cat_printf(
                app->tool_text,
                " GW %d.%d.%d.%d",
                srv->gateway[0],
                srv->gateway[1],
                srv->gateway[2],
                srv->gateway[3]);
            furi_string_cat_printf(
                app->tool_text,
                " DNS %d.%d.%d.%d\n",
                srv->dns[0],
                srv->dns[1],
                srv->dns[2],
                srv->dns[3]);
            if(srv->domain[0]) furi_string_cat_printf(app->tool_text, " %s", srv->domain);
            uint32_t ls = srv->lease_time;
            if(ls > 0)
                furi_string_cat_printf(app->tool_text, " %luh\n", (unsigned long)(ls / 3600));
            else
                furi_string_cat(app->tool_text, "\n");
        }
        if(state.multiple_servers)
            furi_string_cat(app->tool_text, "ROGUE DETECTED!\n");
        else
            furi_string_cat(app->tool_text, "Single server, OK.\n");
    }

    lan_tester_save_and_notify(app, "rogue_dhcp.txt", app->tool_text);
}

/* ==================== Rogue RA Detection ==================== */

static void lan_tester_do_rogue_ra(LanTesterApp* app) {
    if(!lan_tester_check_w5500(app)) return;

    furi_string_cat(app->tool_text, "[Rogue RA] Scanning...\n");
    lan_tester_update_view(app->text_box_tool, app->tool_text);

    if(!w5500_hal_open_macraw()) {
        furi_string_cat(app->tool_text, "MACRAW open failed!\n");
        return;
    }

    RogueRaState state;
    rogue_ra_init(&state);

    uint32_t start = furi_get_tick();
    uint32_t duration_ms = 15000;

    while(app->worker_running && (furi_get_tick() - start) < duration_ms) {
        uint16_t recv_len = w5500_hal_macraw_recv(app->frame_buf, FRAME_BUF_SIZE);
        if(recv_len > 0) {
            rogue_ra_process_frame(&state, app->frame_buf, recv_len);
        }
        furi_delay_ms(1);
    }

    w5500_hal_close_macraw();

    furi_string_cat_printf(
        app->tool_text, "RA:%d Routers:%d\n", state.total_ra_seen, state.router_count);

    if(state.router_count == 0) {
        furi_string_cat(app->tool_text, "No IPv6 routers.\n");
    } else {
        for(uint8_t i = 0; i < state.router_count; i++) {
            RogueRaRouter* r = &state.routers[i];
            furi_string_cat_printf(
                app->tool_text,
                "#%d %02X:%02X:%02X:%02X:%02X:%02X\n",
                i + 1,
                r->src_mac[0],
                r->src_mac[1],
                r->src_mac[2],
                r->src_mac[3],
                r->src_mac[4],
                r->src_mac[5]);
            furi_string_cat_printf(
                app->tool_text,
                " TTL:%ds %s%s",
                r->router_lifetime,
                r->managed_flag ? "M" : "",
                r->other_flag ? "O" : "");
            if(r->prefix_len > 0) furi_string_cat_printf(app->tool_text, " /%d", r->prefix_len);
            furi_string_cat(app->tool_text, "\n");
        }

        if(state.multiple_routers) {
            furi_string_cat(
                app->tool_text, "WARNING: Multiple IPv6\nrouters detected!\nPossible rogue RA.\n");
        }
    }

    lan_tester_save_and_notify(app, "rogue_ra.txt", app->tool_text);
}

/* ==================== DHCP Fingerprinting ==================== */

static void lan_tester_do_dhcp_fingerprint(LanTesterApp* app) {
    if(!lan_tester_check_w5500(app)) return;

    furi_string_cat(app->tool_text, "[DHCP FP] Listening...\n");
    lan_tester_update_view(app->text_box_tool, app->tool_text);

    if(!w5500_hal_open_macraw()) {
        furi_string_cat(app->tool_text, "MACRAW open failed!\n");
        return;
    }

    DhcpFpState state;
    dhcp_fp_init(&state);

    uint32_t start = furi_get_tick();
    uint32_t duration_ms = 30000;

    while(app->worker_running && (furi_get_tick() - start) < duration_ms) {
        uint16_t recv_len = w5500_hal_macraw_recv(app->frame_buf, FRAME_BUF_SIZE);
        if(recv_len > 0) {
            if(dhcp_fp_process_frame(&state, app->frame_buf, recv_len)) {
                /* Update display when new client found */
                furi_string_reset(app->tool_text);
                furi_string_cat_printf(
                    app->tool_text, "[DHCP FP] %d clients\n", state.client_count);
                for(uint16_t i = 0; i < state.client_count; i++) {
                    DhcpFpClient* c = &state.clients[i];
                    furi_string_cat_printf(
                        app->tool_text,
                        "..%02X:%02X:%02X %s\n",
                        c->mac[3],
                        c->mac[4],
                        c->mac[5],
                        c->os_guess);
                }
                lan_tester_update_view(app->text_box_tool, app->tool_text);
            }
        }
        furi_delay_ms(1);
    }

    w5500_hal_close_macraw();

    if(state.client_count == 0) {
        furi_string_cat(app->tool_text, "No DHCP clients detected.\n");
    }

    lan_tester_save_and_notify(app, "dhcp_fp.txt", app->tool_text);
}

/* ==================== SNMP GET ==================== */

static void lan_tester_do_snmp_get(LanTesterApp* app) {
    if(!lan_tester_check_w5500(app)) return;

    char ip_str[16];
    snprintf(
        ip_str,
        sizeof(ip_str),
        "%d.%d.%d.%d",
        app->snmp_target[0],
        app->snmp_target[1],
        app->snmp_target[2],
        app->snmp_target[3]);
    furi_string_cat_printf(app->tool_text, "[SNMP] %s\n", ip_str);
    lan_tester_update_view(app->text_box_tool, app->tool_text);

    SnmpGetResult result;
    bool ok = snmp_client_get(app->snmp_target, "public", true, &result);
    if(!ok) ok = snmp_client_get(app->snmp_target, "public", false, &result);

    if(!ok || !result.valid) {
        furi_string_cat(app->tool_text, "No SNMP response.\n");
        return;
    }

    if(result.has_sys_name) furi_string_cat_printf(app->tool_text, "Name: %s\n", result.sys_name);
    if(result.has_sys_descr)
        furi_string_cat_printf(app->tool_text, "Desc: %s\n", result.sys_descr);
    if(result.has_sys_uptime) {
        uint32_t s = result.sys_uptime / 100;
        furi_string_cat_printf(
            app->tool_text,
            "Up: %lud %luh %lum\n",
            (unsigned long)(s / 86400),
            (unsigned long)((s % 86400) / 3600),
            (unsigned long)((s % 3600) / 60));
    }
    if(result.has_if_status) {
        const char* st = "?";
        switch(result.if_oper_status) {
        case 1:
            st = "up";
            break;
        case 2:
            st = "down";
            break;
        case 3:
            st = "testing";
            break;
        case 5:
            st = "dormant";
            break;
        case 7:
            st = "lowerDown";
            break;
        }
        furi_string_cat_printf(app->tool_text, "ifStatus: %s\n", st);
    }

    lan_tester_save_and_notify(app, "snmp.txt", app->tool_text);
}

/* ==================== NTP Diagnostics ==================== */

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

/* ==================== NetBIOS Query ==================== */

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

/* ==================== DNS Poisoning Check ==================== */

static void lan_tester_do_dns_poison_check(LanTesterApp* app) {
    if(!lan_tester_check_w5500(app)) return;

    furi_string_cat_printf(app->tool_text, "[DNS Check] %s\n", app->dns_poison_host_input);
    lan_tester_update_view(app->text_box_tool, app->tool_text);

    /* Use DHCP DNS as local, 8.8.8.8 as public */
    uint8_t local_dns[4];
    if(app->dhcp_valid &&
       (app->dhcp_dns[0] | app->dhcp_dns[1] | app->dhcp_dns[2] | app->dhcp_dns[3])) {
        memcpy(local_dns, app->dhcp_dns, 4);
    } else if(app->dns_custom_enabled) {
        memcpy(local_dns, app->dns_custom_server, 4);
    } else {
        furi_string_cat(app->tool_text, "No local DNS available.\nRun DHCP first.\n");
        return;
    }

    uint8_t public_dns[4] = {8, 8, 8, 8};

    furi_string_cat_printf(
        app->tool_text,
        "Local: %d.%d.%d.%d\n",
        local_dns[0],
        local_dns[1],
        local_dns[2],
        local_dns[3]);
    furi_string_cat_printf(
        app->tool_text,
        "Public: %d.%d.%d.%d\n",
        public_dns[0],
        public_dns[1],
        public_dns[2],
        public_dns[3]);
    lan_tester_update_view(app->text_box_tool, app->tool_text);

    DnsPoisonResult result;
    dns_poison_check(app->dns_poison_host_input, local_dns, public_dns, &result);

    if(result.local_ok) {
        furi_string_cat(app->tool_text, "L: ");
        for(uint8_t i = 0; i < result.local_count; i++) {
            furi_string_cat_printf(
                app->tool_text,
                "%s%d.%d.%d.%d",
                i ? "," : "",
                result.local_addrs[i][0],
                result.local_addrs[i][1],
                result.local_addrs[i][2],
                result.local_addrs[i][3]);
        }
        furi_string_cat(app->tool_text, "\n");
    } else {
        furi_string_cat(app->tool_text, "L: no response\n");
    }
    if(result.public_ok) {
        furi_string_cat(app->tool_text, "P: ");
        for(uint8_t i = 0; i < result.public_count; i++) {
            furi_string_cat_printf(
                app->tool_text,
                "%s%d.%d.%d.%d",
                i ? "," : "",
                result.public_addrs[i][0],
                result.public_addrs[i][1],
                result.public_addrs[i][2],
                result.public_addrs[i][3]);
        }
        furi_string_cat(app->tool_text, "\n");
    } else {
        furi_string_cat(app->tool_text, "P: no response\n");
    }
    if(result.local_ok && result.public_ok)
        furi_string_cat(
            app->tool_text, result.match ? "MATCH - clean\n" : "MISMATCH! Poisoned?\n");
    else
        furi_string_cat(app->tool_text, "Incomplete comparison.\n");

    lan_tester_save_and_notify(app, "dns_poison.txt", app->tool_text);
}

/* ==================== Entry point ==================== */

int32_t lan_tester_app(void* p) {
    UNUSED(p);

    FURI_LOG_I(TAG, "LAN Tester starting");

    furi_hal_power_insomnia_enter();

    LanTesterApp* app = lan_tester_app_alloc();
    if(!app) {
        FURI_LOG_E(TAG, "Failed to allocate app struct");
        furi_hal_power_insomnia_exit();
        return -1;
    }

    /* Start on main menu */
    lan_tester_update_menu_header(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, LanTesterViewMainMenu);
    view_dispatcher_run(app->view_dispatcher);

    /* Cleanup */
    lan_tester_app_free(app);

    furi_hal_power_insomnia_exit();

    FURI_LOG_I(TAG, "LAN Tester stopped");
    return 0;
}
