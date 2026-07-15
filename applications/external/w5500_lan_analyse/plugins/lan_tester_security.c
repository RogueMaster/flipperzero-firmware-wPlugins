/*
 * Security category plugin (DNS Poison Check, ARP Watch, Rogue DHCP, Rogue RA,
 * DHCP Fingerprint, EAPOL Probe, VLAN Hop). Protocols stay in the host
 * (ioLibrary); this plugin holds the tool orchestration.
 */

#include "../lan_tester_app.h"
#include "../lan_tester_plugin.h"
#include "../hal/w5500_hal.h"
#include "../utils/packet_utils.h"
#include "../utils/oui_lookup.h"
#include "../protocols/dns_poison.h"
#include "../protocols/arp_watch.h"
#include "../protocols/rogue_dhcp.h"
#include "../protocols/rogue_ra.h"
#include "../protocols/dhcp_fingerprint.h"
#include "../protocols/eapol_probe.h"
#include "../protocols/vlan_hop.h"
#include "../protocols/dns_lookup.h"
#include "../protocols/arp_scan.h"
#include "../protocols/dhcp_discover.h"
#include "../api/lan_tester_ioshim.h"
#include <furi.h>
#include <furi_hal.h>
#include <flipper_application/flipper_application.h>

#define TAG "ETH"

static void lan_tester_do_dns_poison_check(LanTesterApp* app);
static void lan_tester_do_arp_watch(LanTesterApp* app);
static void lan_tester_do_rogue_dhcp(LanTesterApp* app);
static void lan_tester_do_rogue_ra(LanTesterApp* app);
static void lan_tester_do_dhcp_fingerprint(LanTesterApp* app);
static void lan_tester_do_eapol_probe(LanTesterApp* app);
static void lan_tester_do_vlan_hop(LanTesterApp* app);

static void security_run(LanTesterApp* app, uint32_t op) {
    switch(op) {
    case LanTesterMenuItemDnsPoisonCheck:
        lan_tester_do_dns_poison_check(app);
        break;
    case LanTesterMenuItemArpWatch:
        lan_tester_do_arp_watch(app);
        break;
    case LanTesterMenuItemRogueDhcp:
        lan_tester_do_rogue_dhcp(app);
        break;
    case LanTesterMenuItemRogueRa:
        lan_tester_do_rogue_ra(app);
        break;
    case LanTesterMenuItemDhcpFingerprint:
        lan_tester_do_dhcp_fingerprint(app);
        break;
    case LanTesterMenuItemEapolProbe:
        lan_tester_do_eapol_probe(app);
        break;
    case LanTesterMenuItemVlanHopTop10:
    case LanTesterMenuItemVlanHopCustom:
        lan_tester_do_vlan_hop(app);
        break;
    default:
        break;
    }
}

static const LanTesterCategoryPlugin security_plugin = {
    .name = "security",
    .run = security_run,
};

static const FlipperAppPluginDescriptor security_plugin_descriptor = {
    .appid = LAN_TESTER_PLUGIN_APP_ID,
    .ep_api_version = LAN_TESTER_PLUGIN_API_VERSION,
    .entry_point = &security_plugin,
};

const FlipperAppPluginDescriptor* lan_tester_security_plugin_ep(void) {
    return &security_plugin_descriptor;
}

/* ==================== moved from lan_tester_app.c ==================== */

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
