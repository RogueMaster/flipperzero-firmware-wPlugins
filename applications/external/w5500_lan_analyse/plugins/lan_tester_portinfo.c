/*
 * Port Info category plugin.
 *
 * Compiled into an embedded .fal that the host loads on demand. The tool code
 * calls the shared app/W5500 helpers (resolved at load via the host's API
 * table) and firmware APIs directly; app state is reached through the passed
 * LanTesterApp pointer.
 */

#include "../lan_tester_app.h"
#include "../lan_tester_plugin.h"
#include "../hal/w5500_hal.h"
#include "../utils/packet_utils.h"
#include "../utils/oui_lookup.h"
#include "../protocols/lldp.h"
#include "../protocols/cdp.h"
#include "../protocols/stp_vlan.h"
#include "../protocols/snmp_client.h"
#include "../protocols/dhcp_discover.h"
#include "../api/lan_tester_ioshim.h"
#include <furi.h>
#include <furi_hal.h>
#include <flipper_application/flipper_application.h>

#define TAG "ETH"

/* moved from lan_tester_app.c — bodies appended below */
static void lan_tester_do_lldp_cdp(LanTesterApp* app);
static void lan_tester_do_dhcp_analyze(LanTesterApp* app);
static void lan_tester_do_stp_vlan(LanTesterApp* app);
static void lan_tester_do_snmp_get(LanTesterApp* app);

static void portinfo_do_link_info(LanTesterApp* app) {
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

static void portinfo_run(LanTesterApp* app, uint32_t op) {
    switch(op) {
    case LanTesterMenuItemLinkInfo:
        portinfo_do_link_info(app);
        break;
    case LanTesterMenuItemLldpCdp:
        lan_tester_do_lldp_cdp(app);
        break;
    case LanTesterMenuItemDhcpAnalyze:
        lan_tester_do_dhcp_analyze(app);
        break;
    case LanTesterMenuItemStpVlan:
        lan_tester_do_stp_vlan(app);
        break;
    case LanTesterMenuItemSnmpGet:
        lan_tester_do_snmp_get(app);
        break;
    default:
        break;
    }
}

/* app<>plugin interface implementation */
static const LanTesterCategoryPlugin portinfo_plugin = {
    .name = "portinfo",
    .run = portinfo_run,
};

static const FlipperAppPluginDescriptor portinfo_plugin_descriptor = {
    .appid = LAN_TESTER_PLUGIN_APP_ID,
    .ep_api_version = LAN_TESTER_PLUGIN_API_VERSION,
    .entry_point = &portinfo_plugin,
};

/* Plugin entry point (referenced by application.fam) */
const FlipperAppPluginDescriptor* lan_tester_portinfo_plugin_ep(void) {
    return &portinfo_plugin_descriptor;
}

/* ==================== moved from lan_tester_app.c ==================== */

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
