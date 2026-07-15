/*
 * Utilities-2 category plugin — Auto Test. Split out of lan_tester_util (and
 * since further split: File Manager → lan_tester_filemgr, PXE Server/Download →
 * lan_tester_pxe) so each .fal is small enough to load into the tight runtime
 * heap.
 */

#include "../lan_tester_app.h"
#include "../lan_tester_plugin.h"
#include "../hal/w5500_hal.h"
#include "../utils/packet_utils.h"
#include "../utils/oui_lookup.h"
#include "../protocols/lldp.h"
#include "../protocols/cdp.h"
#include "../protocols/arp_scan.h"
#include "../protocols/icmp.h"
#include "../protocols/dns_lookup.h"
#include "../protocols/port_scan.h"
#include "../api/lan_tester_ioshim.h"
#include <furi.h>
#include <furi_hal.h>
#include <flipper_application/flipper_application.h>

#define TAG "ETH"

static void lan_tester_do_autotest(LanTesterApp* app);

static void util2_run(LanTesterApp* app, uint32_t op) {
    switch(op) {
    case LanTesterMenuItemAutoTest:
        lan_tester_do_autotest(app);
        break;
    default:
        break;
    }
}

static const LanTesterCategoryPlugin util2_plugin = {
    .name = "util2",
    .run = util2_run,
};

static const FlipperAppPluginDescriptor util2_plugin_descriptor = {
    .appid = LAN_TESTER_PLUGIN_APP_ID,
    .ep_api_version = LAN_TESTER_PLUGIN_API_VERSION,
    .entry_point = &util2_plugin,
};

const FlipperAppPluginDescriptor* lan_tester_util2_plugin_ep(void) {
    return &util2_plugin_descriptor;
}

/* ==================== moved from lan_tester_app.c ==================== */

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
                    "%s: %d.%d.%d.%d/%d\n"
                    "GW:   %d.%d.%d.%d\n"
                    "DNS:  %d.%d.%d.%d\n",
                    app->net_manual_enabled ? "Stat" : "DHCP",
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

            /* Step 5: Internet reachability, against the target from Settings
             * (AT Internet IP). We deliberately do NOT ping the step-4
             * DNS-resolved host: many web hosts drop ICMP, which produced a
             * false "Internet: FAIL" on a working connection.
             *
             * Both checks always run and both are reported, since they answer
             * different questions: ICMP gives a true round-trip, while the TCP
             * handshake (on the AT TCP port setting) still succeeds on networks
             * whose firewall drops ICMP. Any TCP answer proves the path is up:
             * Open means the port accepted us, Closed means the host replied
             * RST. Only a timeout (Filtered) is no answer. Internet is only
             * FAIL when neither replies. */
            if(gw_ok && w5500_hal_get_link_status() && app->autotest_running) {
                /* ICMP */
                PingResult ir = {0};
                bool icmp_ok = icmp_ping(
                    W5500_PING_SOCKET,
                    app->autotest_inet_ip,
                    2,
                    app->ping_timeout_ms,
                    &ir,
                    &app->worker_running);
                if(icmp_ok) {
                    furi_string_cat_printf(body, "Inet ICMP: %lums\n", (unsigned long)ir.rtt_ms);
                } else {
                    furi_string_cat_str(body, "Inet ICMP: no reply\n");
                }
                furi_string_set(app->autotest_text, "[Auto Test]\n");
                furi_string_cat(app->autotest_text, body);
                lan_tester_update_view(app->text_box_autotest, app->autotest_text);

                /* TCP — run it even when ICMP answered, so both are reported */
                bool tcp_ok = false;
                if(app->autotest_running) {
                    uint32_t t0 = furi_get_tick();
                    PortState ps = port_scan_tcp(
                        W5500_SCAN_SOCKET_BASE,
                        app->autotest_inet_ip,
                        app->autotest_tcp_port,
                        app->ping_timeout_ms);
                    uint32_t rtt = furi_get_tick() - t0;
                    tcp_ok = (ps != PortStateFiltered);
                    if(tcp_ok) {
                        furi_string_cat_printf(
                            body,
                            "Inet TCP:%d %lums\n",
                            app->autotest_tcp_port,
                            (unsigned long)rtt);
                    } else {
                        furi_string_cat_printf(
                            body, "Inet TCP:%d no reply\n", app->autotest_tcp_port);
                    }
                }

                if(!icmp_ok && !tcp_ok) furi_string_cat_str(body, "Internet: FAIL\n");

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
