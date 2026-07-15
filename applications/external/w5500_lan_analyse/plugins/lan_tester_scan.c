/*
 * Scan category plugin (ARP Scan, Ping Sweep, Discovery, Port Scan).
 * Its protocols use the ioLibrary (or are shared with the host), so they stay
 * in the host and are reached through the API table; this plugin holds only
 * the tool orchestration moved out of lan_tester_app.c.
 */

#include "../lan_tester_app.h"
#include "../lan_tester_plugin.h"
#include "../hal/w5500_hal.h"
#include "../utils/packet_utils.h"
#include "../utils/oui_lookup.h"
#include "../protocols/arp_scan.h"
#include "../protocols/icmp.h"
#include "../protocols/discovery.h"
#include "../protocols/port_scan.h"
#include "../api/lan_tester_ioshim.h"
#include <furi.h>
#include <furi_hal.h>
#include <flipper_application/flipper_application.h>

#define TAG "ETH"

/* moved from lan_tester_app.c — bodies appended below */
static void lan_tester_do_arp_scan(LanTesterApp* app);
static void lan_tester_do_ping_sweep(LanTesterApp* app);
static void lan_tester_do_ping_sweep_detect(LanTesterApp* app);
static void lan_tester_do_discovery(LanTesterApp* app);
static void lan_tester_do_port_scan(LanTesterApp* app);

static void scan_run(LanTesterApp* app, uint32_t op) {
    switch(op) {
    case LanTesterMenuItemArpScan:
        lan_tester_do_arp_scan(app);
        break;
    case LanTesterMenuItemPingSweep:
        lan_tester_do_ping_sweep(app);
        break;
    case LanTesterMenuItemDiscovery:
        lan_tester_do_discovery(app);
        break;
    case LanTesterMenuItemPortScan:
    case LanTesterMenuItemPortScanFull:
    case LanTesterMenuItemPortScanCustom:
        lan_tester_do_port_scan(app);
        break;
    case WORKER_OP_PING_SWEEP_DETECT:
        lan_tester_do_ping_sweep_detect(app);
        break;
    default:
        break;
    }
}

static const LanTesterCategoryPlugin scan_plugin = {
    .name = "scan",
    .run = scan_run,
};

static const FlipperAppPluginDescriptor scan_plugin_descriptor = {
    .appid = LAN_TESTER_PLUGIN_APP_ID,
    .ep_api_version = LAN_TESTER_PLUGIN_API_VERSION,
    .entry_point = &scan_plugin,
};

const FlipperAppPluginDescriptor* lan_tester_scan_plugin_ep(void) {
    return &scan_plugin_descriptor;
}

/* ==================== moved from lan_tester_app.c ==================== */

static void lan_tester_do_arp_scan(LanTesterApp* app) {
    furi_string_reset(app->tool_text);

    furi_string_printf(app->tool_text, "%s\n", lan_tester_net_acquire_msg(app));
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

static void lan_tester_do_ping_sweep_detect(LanTesterApp* app) {
    furi_string_reset(app->tool_text);

    furi_string_printf(app->tool_text, "%s\n", lan_tester_net_acquire_msg(app));
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

static void lan_tester_do_ping_sweep(LanTesterApp* app) {
    furi_string_reset(app->tool_text);

    furi_string_printf(app->tool_text, "%s\n", lan_tester_net_acquire_msg(app));
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

static void lan_tester_do_discovery(LanTesterApp* app) {
    furi_string_reset(app->tool_text);

    furi_string_printf(app->tool_text, "%s\n", lan_tester_net_acquire_msg(app));
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

static void lan_tester_do_port_scan(LanTesterApp* app) {
    furi_string_reset(app->tool_text);

    furi_string_printf(app->tool_text, "%s\n", lan_tester_net_acquire_msg(app));
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
