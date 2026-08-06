#include "udp_module.h"

#include "app_user.h"
#include "arp_module.h"
#include "../libraries/protocol_tools/ethernet_protocol.h"
#include "../libraries/protocol_tools/ipv4.h"
#include "../libraries/protocol_tools/icmp.h"
#include "../libraries/protocol_tools/udp.h"
#include "../libraries/protocol_tools/arp.h"

#define TEXT_PORT_FORMAT "%lu%s"
#define TEXT_POINTS      "..."

// F0.6 — gated behind DEV_MODE. See tcp_module.c for rationale.
#if DEV_MODE
#define DEBUG 1
#else
#define DEBUG 0
#endif

bool send_empty_udp_packet(
    uint8_t* buffer,
    enc28j60_t* ethernet,
    uint8_t* ip_gateway,
    uint8_t* source_mac,
    uint8_t* target_mac,
    uint8_t* source_ip,
    uint8_t* target_ip,
    uint16_t source_port,
    uint16_t target_port) {
    UNUSED(ip_gateway);
    // F0.5g — caller is expected to pass an already-resolved target_mac
    // (udp_port_scan does this via scanner_resolve_next_hop before the
    // loop). Pre-fix this function ignored the passed target_mac and
    // re-ran arp_get_specific_mac, which (a) wasted the cached MAC,
    // (b) opened a race with rx_dispatch since arp_get_specific_mac
    // polls receive_packet directly, and (c) could add up to 20 s per
    // probed port if the local ARP failed (10 attempts × 2 s).

    if(!set_udp_header(
           buffer + sizeof(ethernet_header_t) + sizeof(ipv4_header_t),
           source_port,
           target_port,
           sizeof(udp_header_t)))
        return false;

    if(!set_ipv4_header(
           buffer + sizeof(ethernet_header_t),
           17,
           sizeof(udp_header_t),
           source_ip,
           target_ip,
           0,
           0x4000,
           WIN_TTL))
        return false;

    if(!set_ethernet_header(buffer, source_mac, target_mac, 0x0800)) return false;

    send_packet(
        ethernet,
        buffer,
        sizeof(ethernet_header_t) + sizeof(ipv4_header_t) + sizeof(udp_header_t));

#if DEBUG
    printf("EMPTY UDP PACKET: \n");
    for(uint16_t i = 0;
        i < (sizeof(ethernet_header_t) + sizeof(ipv4_header_t) + sizeof(udp_header_t));
        i++) {
        printf(
            "%02X%c",
            buffer[i],
            i == (sizeof(ethernet_header_t) + sizeof(ipv4_header_t) + sizeof(udp_header_t) - 1) ?
                '\n' :
                ' ');
    }
#endif

    return true;
}

// Predicate context for udp_port_scan's wait_for_packet calls.
typedef struct {
    enc28j60_t* ethernet;
    uint8_t* my_mac;
    uint16_t expected_source_port;
    uint16_t expected_dest_port;
} udp_scan_pred_ctx_t;

static bool udp_scan_match(const uint8_t* frame, uint16_t len, void* ctx) {
    UNUSED(len);
    udp_scan_pred_ctx_t* c = (udp_scan_pred_ctx_t*)ctx;

    // Auto-reply to incidental ARP requests during the scan; do not match.
    if(is_arp((uint8_t*)frame)) {
        arp_reply_requested(c->ethernet, (uint8_t*)frame, c->ethernet->ip_address);
        return false;
    }

    if(!is_udp((uint8_t*)frame)) return false;

    // Frame's destination MAC == our MAC?
    if((*(uint16_t*)(c->my_mac + 4) != *(uint16_t*)(frame + 4)) ||
       (*(uint32_t*)c->my_mac != *(uint32_t*)frame))
        return false;

    udp_header_t hdr = udp_get_header((uint8_t*)frame);
    uint16_t src_port = 0;
    uint16_t dst_port = 0;
    bytes_to_uint(&src_port, hdr.source_port, sizeof(uint16_t));
    bytes_to_uint(&dst_port, hdr.dest_port, sizeof(uint16_t));

    return (src_port == c->expected_source_port && dst_port == c->expected_dest_port);
}

// F0.5d — closure-style trigger for send_empty_udp_packet, used by
// udp_port_scan to fire the probe after the rx predicate is registered.
typedef struct {
    enc28j60_t* eth;
    uint8_t* ip_gateway;
    uint8_t* source_mac;
    uint8_t* target_mac;
    uint8_t* source_ip;
    uint8_t* target_ip;
    uint16_t source_port;
    uint16_t target_port;
} udp_probe_trigger_ctx_t;

static void udp_probe_trigger(void* ctx) {
    udp_probe_trigger_ctx_t* c = (udp_probe_trigger_ctx_t*)ctx;
    send_empty_udp_packet(
        c->eth->tx_buffer,
        c->eth,
        c->ip_gateway,
        c->source_mac,
        c->target_mac,
        c->source_ip,
        c->target_ip,
        c->source_port,
        c->target_port);
}

void udp_port_scan(void* context, uint8_t* target_ip, uint16_t init_port, uint16_t range_port) {
    App* app = context;

    // F0.3d — scanner session (subnet-aware ARP + cache + cancel + wait).
    scanner_session_t scanner;
    scanner_session_init(&scanner, app);

    uint8_t target_mac[6] = {0};
    if(!scanner_resolve_next_hop(&scanner, target_ip, target_mac)) {
        scanner_session_deinit(&scanner);
        return;
    }

    uint32_t submenu_index = 0;
    submenu_add_item(app->submenu, "", submenu_index, NULL, NULL);

    for(uint32_t i = init_port; i < (init_port + range_port); i++) {
        if(scanner_cancel_requested(&scanner)) break;

        furi_string_reset(app->text);
        furi_string_cat_printf(app->text, TEXT_PORT_FORMAT, i, TEXT_POINTS);
        submenu_change_item_label(app->submenu, submenu_index, furi_string_get_cstr(app->text));

        // F0.5d — predicate registered before the probe goes out, via the
        // trigger param of scanner_wait_for_packet.
        udp_scan_pred_ctx_t pred_ctx = {
            .ethernet = app->ethernet,
            .my_mac = app->ethernet->mac_address,
            .expected_source_port = (uint16_t)i,
            .expected_dest_port = 64892,
        };
        udp_probe_trigger_ctx_t trigger_ctx = {
            .eth = app->ethernet,
            .ip_gateway = app->ip_gateway,
            .source_mac = app->ethernet->mac_address,
            .target_mac = target_mac,
            .source_ip = app->ethernet->ip_address,
            .target_ip = target_ip,
            .source_port = 64892,
            .target_port = (uint16_t)i,
        };
        uint16_t got = 0;
        if(scanner_wait_for_packet(
               &scanner, udp_scan_match, &pred_ctx, udp_probe_trigger, &trigger_ctx, &got, 100)) {
            // Port responded — append result.
            furi_string_reset(app->text);
            furi_string_cat_printf(app->text, TEXT_PORT_FORMAT, (uint32_t)i, "\0");
            submenu_change_item_label(
                app->submenu, submenu_index, furi_string_get_cstr(app->text));
            submenu_index++;
            furi_string_reset(app->text);
            furi_string_cat_printf(app->text, TEXT_PORT_FORMAT, i, TEXT_POINTS);
            submenu_add_item(
                app->submenu, furi_string_get_cstr(app->text), submenu_index, NULL, NULL);
            submenu_set_selected_item(app->submenu, submenu_index);
        }
    }
    furi_string_reset(app->text);
    submenu_change_item_label(app->submenu, submenu_index, "FINISH");

    scanner_session_deinit(&scanner);
}
