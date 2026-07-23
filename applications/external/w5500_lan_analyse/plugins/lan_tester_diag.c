/*
 * Diagnostics category plugin (Ping, DNS Lookup, Traceroute, TCP Ping).
 * Continuous Ping stays in the host for now (it drives a custom view).
 * The protocols use the ioLibrary and stay in the host, reached via the table.
 */

#include "../lan_tester_app.h"
#include "../lan_tester_plugin.h"
#include "../hal/w5500_hal.h"
#include "../utils/packet_utils.h"
#include "../protocols/icmp.h"
#include "../protocols/dns_lookup.h"
#include "../protocols/traceroute.h"
#include "../protocols/port_scan.h"
#include "../api/lan_tester_ioshim.h"
#include <furi.h>
#include <furi_hal.h>
#include <flipper_application/flipper_application.h>

#define TAG "ETH"

/* moved from lan_tester_app.c — bodies appended below */
static void lan_tester_do_ping(LanTesterApp* app);
static void lan_tester_do_dns_lookup(LanTesterApp* app);
static void lan_tester_do_traceroute(LanTesterApp* app);
static void lan_tester_do_tcp_ping(LanTesterApp* app);

static void diag_run(LanTesterApp* app, uint32_t op) {
    switch(op) {
    case LanTesterMenuItemPing:
        lan_tester_do_ping(app);
        break;
    case LanTesterMenuItemDnsLookup:
        lan_tester_do_dns_lookup(app);
        break;
    case LanTesterMenuItemTraceroute:
        lan_tester_do_traceroute(app);
        break;
    case LanTesterMenuItemTcpPing:
        lan_tester_do_tcp_ping(app);
        break;
    default:
        break;
    }
}

/*
 * TCP Ping — connect test for networks that filter ICMP.
 *
 * A TCP handshake proves reachability where ping cannot: an accepted connection
 * (open) and a refusal (RST -> closed) both mean the host answered us. Only a
 * timeout means the host is unreachable or the port is silently dropped.
 * Repeats ping_count times and reports each attempt's round-trip.
 */
static void lan_tester_do_tcp_ping(LanTesterApp* app) {
    FuriString* out = app->tool_text;
    furi_string_reset(out);

    furi_string_printf(out, "%s\n", lan_tester_net_acquire_msg(app));
    lan_tester_update_view(app->text_box_tool, out);

    /* check_dhcp (not ensure_dhcp) so a missing module / link reports why */
    if(!lan_tester_check_dhcp(app)) return;

    furi_string_printf(
        out,
        "[TCP Ping] %d.%d.%d.%d:%d\n",
        app->tcp_ping_target[0],
        app->tcp_ping_target[1],
        app->tcp_ping_target[2],
        app->tcp_ping_target[3],
        app->tcp_ping_port);
    lan_tester_update_view(app->text_box_tool, out);

    uint16_t replies = 0;
    uint32_t rtt_sum = 0;
    uint32_t rtt_min = UINT32_MAX;
    uint32_t rtt_max = 0;

    for(uint16_t i = 1; i <= app->ping_count && app->worker_running; i++) {
        uint32_t t0 = furi_get_tick();
        PortState ps = port_scan_tcp(
            W5500_SCAN_SOCKET_BASE, app->tcp_ping_target, app->tcp_ping_port, app->ping_timeout_ms);
        uint32_t rtt = furi_get_tick() - t0;

        if(ps == PortStateFiltered) {
            furi_string_cat_printf(out, "%d: timeout\n", i);
        } else {
            replies++;
            rtt_sum += rtt;
            if(rtt < rtt_min) rtt_min = rtt;
            if(rtt > rtt_max) rtt_max = rtt;
            furi_string_cat_printf(
                out, "%d: %lums %s\n", i, (unsigned long)rtt, ps == PortStateOpen ? "open" : "RST");
        }
        lan_tester_update_view(app->text_box_tool, out);

        if(i < app->ping_count && app->worker_running) furi_delay_ms(app->ping_interval_ms);
    }

    if(replies) {
        furi_string_cat_printf(
            out,
            "%d/%d ok  %lu/%lu/%lums\n",
            replies,
            app->ping_count,
            (unsigned long)rtt_min,
            (unsigned long)(rtt_sum / replies),
            (unsigned long)rtt_max);
    } else {
        furi_string_cat_str(out, "No response.\nHost down or filtered.\n");
    }

    lan_tester_save_and_notify(app, "tcp_ping.txt", out);
}

static const LanTesterCategoryPlugin diag_plugin = {
    .name = "diag",
    .run = diag_run,
};

static const FlipperAppPluginDescriptor diag_plugin_descriptor = {
    .appid = LAN_TESTER_PLUGIN_APP_ID,
    .ep_api_version = LAN_TESTER_PLUGIN_API_VERSION,
    .entry_point = &diag_plugin,
};

const FlipperAppPluginDescriptor* lan_tester_diag_plugin_ep(void) {
    return &diag_plugin_descriptor;
}

/* ==================== moved from lan_tester_app.c ==================== */

static void lan_tester_do_ping(LanTesterApp* app) {
    furi_string_reset(app->tool_text);

    furi_string_printf(app->tool_text, "%s\n", lan_tester_net_acquire_msg(app));
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

static void lan_tester_do_dns_lookup(LanTesterApp* app) {
    furi_string_reset(app->tool_text);

    furi_string_printf(app->tool_text, "%s\n", lan_tester_net_acquire_msg(app));
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

static void lan_tester_do_traceroute(LanTesterApp* app) {
    furi_string_reset(app->tool_text);

    furi_string_printf(app->tool_text, "%s\n", lan_tester_net_acquire_msg(app));
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
