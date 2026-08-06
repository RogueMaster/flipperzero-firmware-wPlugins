#include "tcp_module.h"

#include "app_user.h"
#include "arp_module.h"
#include "../libraries/protocol_tools/tcp.h"
#include "../libraries/protocol_tools/ethernet_protocol.h"
#include "../libraries/protocol_tools/ipv4.h"
#include "../libraries/protocol_tools/arp.h"

#define TEXT_PORT_FORMAT "%lu%s"
#define TEXT_POINTS      "..."

// F0.6 — gated behind DEV_MODE. Was a manual toggle that re-enabled
// printf hex dumps inside scan/handshake; keeping it gated prevents
// accidental release with debug spew.
#if DEV_MODE
#define DEBUG 1
#else
#define DEBUG 0
#endif

#define SCAN_TTL 64

typedef enum {
    TCP_HS_SYN,
    TCP_HS_SYN_ACK,
    TCP_HS_ACK,
} TCP_HANDSHAKE;

typedef enum {
    TCP_TN_FIN_ACK,
    TCP_TN_ACK,
} TCP_TERMINATION;

bool tcp_send_syn(
    enc28j60_t* ethernet,
    uint8_t* source_mac,
    uint8_t* source_ip,
    uint8_t* target_mac,
    uint8_t* target_ip,
    uint16_t source_port,
    uint16_t dest_port,
    uint32_t sequence,
    uint32_t ack_number) {
    uint16_t tcp_len = 0;
    uint16_t window_size;

    // Variación realista tipo Nmap
    uint8_t r = furi_hal_random_get() % 3;

    if(r == 0)
        window_size = 64240; // Linux típico
    else if(r == 1)
        window_size = 65535; // BSD / iOS
    else
        window_size = 8192; // Windows antiguo
    if(!set_tcp_header_syn(
           ethernet->tx_buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN,
           source_ip,
           target_ip,
           source_port,
           dest_port,
           sequence,
           ack_number,
           window_size,
           0,
           &tcp_len))
        return false;

    if(!set_ipv4_header(
           ethernet->tx_buffer + ETHERNET_HEADER_LEN,
           6,
           tcp_len,
           source_ip,
           target_ip,
           0,
           0x4000,
           SCAN_TTL))
        return false;

    if(!set_ethernet_header(ethernet->tx_buffer, source_mac, target_mac, 0x800)) return false;

    send_packet(ethernet, ethernet->tx_buffer, ETHERNET_HEADER_LEN + IP_HEADER_LEN + tcp_len);

#if DEBUG
    // F0.5g — was `buffer[i]` referencing an undefined symbol; would
    // not compile if anyone enabled DEBUG. The actual frame lives in
    // ethernet->tx_buffer.
    printf("TCP SYN ENVIADO: ");
    for(uint16_t i = 0; i < (ETHERNET_HEADER_LEN + IP_HEADER_LEN + tcp_len); i++) {
        printf(
            "%02X%c",
            ethernet->tx_buffer[i],
            i == (ETHERNET_HEADER_LEN + IP_HEADER_LEN + tcp_len - 1) ? '\n' : ' ');
    }
#endif

    return true;
}

bool tcp_send_fin(
    enc28j60_t* ethernet,
    uint8_t* target_ip,
    uint16_t source_port,
    uint16_t dest_port,
    uint8_t* ip_gateway,
    uint32_t sequence,
    uint32_t ack_number) {
    if(ethernet == NULL || target_ip == NULL || ip_gateway == NULL) return false;

    uint8_t* buffer = calloc(1, ETHERNET_HEADER_LEN + IP_HEADER_LEN + sizeof(tcp_header_t));

    uint8_t target_mac[6] = {0};

    if(!arp_get_specific_mac(
           ethernet,
           ethernet->ip_address,
           (*(uint32_t*)ethernet->ip_address & *(uint32_t*)ethernet->subnet_mask) ==
                   (*(uint32_t*)target_ip & *(uint32_t*)ethernet->subnet_mask) ?
               target_ip :
               ip_gateway,
           ethernet->mac_address,
           target_mac))
        return false;

    uint16_t tcp_len;
    if(!set_tcp_header_fin(
           buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN,
           ethernet->ip_address,
           target_ip,
           source_port,
           dest_port,
           sequence,
           ack_number,
           0xFFFF,
           0,
           &tcp_len))
        return false;

    if(!set_ipv4_header(
           buffer + ETHERNET_HEADER_LEN,
           6,
           tcp_len,
           ethernet->ip_address,
           target_ip,
           0,
           0x4000,
           SCAN_TTL))
        return false;

    if(!set_ethernet_header(buffer, ethernet->mac_address, target_mac, 0x800)) return false;

    send_packet(ethernet, buffer, ETHERNET_HEADER_LEN + IP_HEADER_LEN + tcp_len);

#if DEBUG

    printf("TCP FIN ENVIADO: ");
    for(uint16_t i = 0; i < (ETHERNET_HEADER_LEN + IP_HEADER_LEN + tcp_len); i++) {
        printf(
            "%02X%c",
            buffer[i],
            i == (ETHERNET_HEADER_LEN + IP_HEADER_LEN + tcp_len - 1) ? '\n' : ' ');
    }

#endif

    free(buffer);

    return true;
}

bool tcp_send_ack(
    enc28j60_t* ethernet,
    uint8_t* source_mac,
    uint8_t* source_ip,
    uint8_t* target_mac,
    uint8_t* target_ip,
    uint16_t source_port,
    uint16_t dest_port,
    uint32_t sequence,
    uint32_t ack_number) {
    if(ethernet == NULL || target_ip == NULL) return false;

    uint8_t* buffer = calloc(1, ETHERNET_HEADER_LEN + IP_HEADER_LEN + sizeof(tcp_header_t));

    uint16_t tcp_len;
    if(!set_tcp_header_ack(
           buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN,
           source_ip,
           target_ip,
           source_port,
           dest_port,
           sequence,
           ack_number,
           0xFFFF,
           0,
           &tcp_len))
        return false;

    if(!set_ipv4_header(
           buffer + ETHERNET_HEADER_LEN, 6, tcp_len, source_ip, target_ip, 0, 0x4000, SCAN_TTL))
        return false;

    if(!set_ethernet_header(buffer, source_mac, target_mac, 0x800)) return false;

    send_packet(ethernet, buffer, ETHERNET_HEADER_LEN + IP_HEADER_LEN + tcp_len);

#if DEBUG

    printf("TCP ACK ENVIADO: ");
    for(uint16_t i = 0; i < (ETHERNET_HEADER_LEN + IP_HEADER_LEN + tcp_len); i++) {
        printf(
            "%02X%c",
            buffer[i],
            i == (ETHERNET_HEADER_LEN + IP_HEADER_LEN + tcp_len - 1) ? '\n' : ' ');
    }

#endif

    free(buffer);

    return true;
}

// Predicate context for tcp_syn_scan's wait_for_packet calls.
// `port_open` is an OUT field — true on SYN-ACK match, false on RST-ACK
// match. Predicate returns true in either case so the wait can break.
typedef struct {
    enc28j60_t* ethernet;
    uint8_t* my_mac;
    uint16_t expected_source_port;
    bool port_open;
} tcp_scan_pred_ctx_t;

static bool tcp_scan_match(const uint8_t* frame, uint16_t len, void* ctx) {
    UNUSED(len);
    tcp_scan_pred_ctx_t* c = (tcp_scan_pred_ctx_t*)ctx;

    // Auto-reply to incidental ARP requests during the scan; do not match.
    if(is_arp((uint8_t*)frame)) {
        arp_reply_requested(c->ethernet, (uint8_t*)frame, c->ethernet->ip_address);
        return false;
    }

    if(!is_tcp((uint8_t*)frame)) return false;

    // Frame's destination MAC == our MAC?
    if((*(uint16_t*)(c->my_mac + 4) != *(uint16_t*)(frame + 4)) ||
       (*(uint32_t*)c->my_mac != *(uint32_t*)frame))
        return false;

    tcp_header_t hdr = tcp_get_header((uint8_t*)frame);

    uint16_t data_offset_flags = 0;
    bytes_to_uint(&data_offset_flags, hdr.data_offset_flags, sizeof(uint16_t));
    data_offset_flags &= 0x1FF;

    uint16_t source_port = 0;
    bytes_to_uint(&source_port, hdr.source_port, sizeof(uint16_t));

    if(source_port != c->expected_source_port) return false;

    uint16_t syn_ack = (uint16_t)(TCP_SYN | TCP_ACK);

    if((data_offset_flags & syn_ack) == syn_ack) {
        c->port_open = true;
        return true;
    }
    // RST-ACK detection intentionally NOT done here. Pre-F0.3e the code
    // attempted to early-break on RST-ACK but the bitmask made it dead code,
    // so closed ports always took the full 100 ms timeout. Re-introducing
    // the early-break tripled the rate of submenu mutations from this
    // worker thread, which surfaced a pre-existing GUI-from-worker race
    // (the scan would hang and the device reset). Until F0.4 moves the
    // GUI updates onto the dispatcher thread, the safer choice is to
    // preserve the original 100 ms-per-port pacing.
    return false;
}

// F0.5d — closure-style trigger for tcp_send_syn. Used by tcp_syn_scan
// to send the SYN AFTER the rx predicate is registered, closing the
// race against rx_dispatch draining a fast SYN-ACK.
typedef struct {
    enc28j60_t* eth;
    uint8_t* source_mac;
    uint8_t* source_ip;
    uint8_t* target_mac;
    uint8_t* target_ip;
    uint16_t source_port;
    uint16_t dest_port;
    uint32_t sequence;
    uint32_t ack_number;
} tcp_syn_trigger_ctx_t;

static void tcp_syn_trigger(void* ctx) {
    tcp_syn_trigger_ctx_t* c = (tcp_syn_trigger_ctx_t*)ctx;
    tcp_send_syn(
        c->eth,
        c->source_mac,
        c->source_ip,
        c->target_mac,
        c->target_ip,
        c->source_port,
        c->dest_port,
        c->sequence,
        c->ack_number);
}

void tcp_syn_scan(void* context, uint8_t* target_ip, uint16_t init_port, uint16_t range_port) {
    App* app = context;

    // F0.3e — scanner session (subnet-aware ARP + cache + cancel + wait).
    scanner_session_t scanner;
    scanner_session_init(&scanner, app);

    uint8_t target_mac[6] = {0};
    if(!scanner_resolve_next_hop(&scanner, target_ip, target_mac)) {
        scanner_session_deinit(&scanner);
        return;
    }

    uint32_t sequence = furi_get_tick() ^ (rand() << 16);
    uint32_t ack_number = 0;

    uint32_t submenu_index = 0;
    submenu_add_item(app->submenu, "", submenu_index, NULL, NULL);

    for(uint32_t i = init_port; i < (init_port + range_port); i++) {
        if(scanner_cancel_requested(&scanner)) break;

        furi_string_reset(app->text);
        furi_string_cat_printf(app->text, TEXT_PORT_FORMAT, i, TEXT_POINTS);
        submenu_change_item_label(app->submenu, submenu_index, furi_string_get_cstr(app->text));

        uint16_t src_port = 32768 + (rand() % 28232);

        // F0.5d — predicate registered before the SYN goes out, via the
        // trigger param of scanner_wait_for_packet.
        tcp_scan_pred_ctx_t pred_ctx = {
            .ethernet = app->ethernet,
            .my_mac = app->ethernet->mac_address,
            .expected_source_port = (uint16_t)i,
            .port_open = false,
        };
        tcp_syn_trigger_ctx_t trigger_ctx = {
            .eth = app->ethernet,
            .source_mac = app->ethernet->mac_address,
            .source_ip = app->ethernet->ip_address,
            .target_mac = target_mac,
            .target_ip = target_ip,
            .source_port = src_port,
            .dest_port = (uint16_t)i,
            .sequence = sequence,
            .ack_number = ack_number,
        };
        uint16_t got = 0;
        if(scanner_wait_for_packet(
               &scanner, tcp_scan_match, &pred_ctx, tcp_syn_trigger, &trigger_ctx, &got, 100) &&
           pred_ctx.port_open) {
            // SYN-ACK observed — record open port.
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

bool tcp_handshake_process(
    void* context,
    uint8_t* target_ip,
    uint16_t source_port,
    uint16_t dest_port) {
    App* app = context;

    bool result = false;

    uint8_t target_mac[6] = {0};

    if(!arp_get_specific_mac(
           app->ethernet,
           app->ethernet->ip_address,
           (*(uint32_t*)app->ip_gateway & *(uint32_t*)app->ethernet->subnet_mask) ==
                   (*(uint32_t*)target_ip & *(uint32_t*)app->ethernet->subnet_mask) ?
               target_ip :
               app->ip_gateway,
           app->ethernet->mac_address,
           target_mac))
        return false;

    uint32_t sequence = furi_get_tick() ^ (rand() << 16);
    uint32_t ack_number = 0;

    uint32_t last_time = 0;
    uint8_t state = TCP_HS_SYN;
    switch(state) {
    case TCP_HS_SYN:
        if(tcp_send_syn(
               app->ethernet,
               app->ethernet->mac_address,
               app->ethernet->ip_address,
               target_mac,
               target_ip,
               source_port,
               dest_port,
               sequence,
               ack_number)) {
            // Get time
            last_time = furi_get_tick();

            state = TCP_HS_SYN_ACK;
        } else {
            result = false;
        }
    case TCP_HS_SYN_ACK:
        while(!(furi_get_tick() - last_time > 3000)) {
            uint16_t packen_len = 0;

            packen_len = receive_packet(app->ethernet, app->ethernet->rx_buffer, 1500);

            if(packen_len) {
                if(is_arp(app->ethernet->rx_buffer)) {
                    arp_reply_requested(
                        app->ethernet, app->ethernet->rx_buffer, app->ethernet->ip_address);
                } else if(is_tcp(app->ethernet->rx_buffer)) {
                    // Packet is for me
                    if((*(uint16_t*)(app->ethernet->mac_address + 4) ==
                        *(uint16_t*)(app->ethernet->rx_buffer + 4)) &&
                       (*(uint32_t*)app->ethernet->mac_address ==
                        *(uint32_t*)app->ethernet->rx_buffer)) {
                        tcp_header_t tcp_header = tcp_get_header(app->ethernet->rx_buffer);

                        uint16_t data_offset_flags = 0;
                        bytes_to_uint(
                            &data_offset_flags, tcp_header.data_offset_flags, sizeof(uint16_t));
                        data_offset_flags &= 0x1FF;
                        if((data_offset_flags & (uint16_t)(TCP_SYN | TCP_ACK)) ==
                           (uint16_t)(TCP_SYN | TCP_ACK)) {
                            bytes_to_uint(&sequence, tcp_header.sequence, sizeof(uint32_t));
                            bytes_to_uint(&ack_number, tcp_header.ack_number, sizeof(uint32_t));

                            state = TCP_HS_ACK;
                            break;
#if DEBUG

                            printf("SEQUENCE: %lu\n", sequence);
                            printf("ACK: %lu\n", ack_number);

                            printf("RECIBIDO: ");
                            for(uint16_t i = 0; i < packen_len; i++) {
                                printf(
                                    "%02X%c",
                                    app->ethernet->rx_buffer[i],
                                    i == (packen_len - 1) ? '\n' : ' ');
                            }

#endif
                        } else if(
                            (data_offset_flags & (uint16_t)(TCP_RST | TCP_ACK)) ==
                            (uint16_t)(TCP_RST | TCP_ACK)) {
                            result = false;
                            break;

                        } else {
                            result = false;
                        }
                    }
                }
            }
        }
        if(state == TCP_HS_ACK) {
            if(tcp_send_ack(
                   app->ethernet,
                   app->ethernet->mac_address,
                   app->ethernet->ip_address,
                   target_mac,
                   target_ip,
                   source_port,
                   dest_port,
                   ack_number,
                   sequence + 1))
                result = true;
        }
    }
    return result;
}

bool tcp_handshake_process_spoof(
    void* context,
    uint8_t* target_ip,
    uint16_t source_port,
    uint16_t dest_port) {
    App* app = context;

    bool result = false;

    uint8_t target_mac[6] = {0};

    if(!arp_get_specific_mac(
           app->ethernet,
           app->ethernet->ip_address,
           (*(uint32_t*)app->ip_gateway & *(uint32_t*)app->ethernet->subnet_mask) ==
                   (*(uint32_t*)target_ip & *(uint32_t*)app->ethernet->subnet_mask) ?
               target_ip :
               app->ip_gateway,
           app->ethernet->mac_address,
           target_mac))
        return false;

    uint32_t sequence = furi_get_tick() ^ (rand() << 16);
    uint32_t ack_number = 0;

    uint32_t last_time = 0;
    uint8_t state = TCP_HS_SYN;
    switch(state) {
    case TCP_HS_SYN:
        if(tcp_send_syn(
               app->ethernet,
               app->mac_gateway,
               app->ip_gateway,
               target_mac,
               target_ip,
               source_port,
               dest_port,
               sequence,
               ack_number)) {
            // Get time
            last_time = furi_get_tick();

            state = TCP_HS_SYN_ACK;
        } else {
            result = false;
        }
    case TCP_HS_SYN_ACK:
        while(!(furi_get_tick() - last_time > 3000)) {
            uint16_t packen_len = 0;

            packen_len = receive_packet(app->ethernet, app->ethernet->rx_buffer, 1500);

            if(packen_len) {
                if(is_arp(app->ethernet->rx_buffer)) {
                    arp_reply_requested(
                        app->ethernet, app->ethernet->rx_buffer, app->ethernet->ip_address);
                } else if(is_tcp(app->ethernet->rx_buffer)) {
                    // Packet is for me
                    if((*(uint16_t*)(app->ethernet->mac_address + 4) ==
                        *(uint16_t*)(app->ethernet->rx_buffer + 4)) &&
                       (*(uint32_t*)app->ethernet->mac_address ==
                        *(uint32_t*)app->ethernet->rx_buffer)) {
                        tcp_header_t tcp_header = tcp_get_header(app->ethernet->rx_buffer);

                        uint16_t data_offset_flags = 0;
                        bytes_to_uint(
                            &data_offset_flags, tcp_header.data_offset_flags, sizeof(uint16_t));
                        data_offset_flags &= 0x1FF;
                        if((data_offset_flags & (uint16_t)(TCP_SYN | TCP_ACK)) ==
                           (uint16_t)(TCP_SYN | TCP_ACK)) {
                            bytes_to_uint(&sequence, tcp_header.sequence, sizeof(uint32_t));
                            bytes_to_uint(&ack_number, tcp_header.ack_number, sizeof(uint32_t));

                            state = TCP_HS_ACK;
                            break;
#if DEBUG

                            printf("SEQUENCE: %lu\n", sequence);
                            printf("ACK: %lu\n", ack_number);

                            printf("RECIBIDO: ");
                            for(uint16_t i = 0; i < packen_len; i++) {
                                printf(
                                    "%02X%c",
                                    app->ethernet->rx_buffer[i],
                                    i == (packen_len - 1) ? '\n' : ' ');
                            }

#endif
                        } else if(
                            (data_offset_flags & (uint16_t)(TCP_RST | TCP_ACK)) ==
                            (uint16_t)(TCP_RST | TCP_ACK)) {
                            result = false;
                            break;

                        } else {
                            result = false;
                        }
                    }
                }
            }
        }
        if(state == TCP_HS_ACK) {
            if(tcp_send_ack(
                   app->ethernet,
                   app->mac_gateway,
                   app->ip_gateway,
                   target_mac,
                   target_ip,
                   source_port,
                   dest_port,
                   ack_number,
                   sequence + 1))
                result = true;
        }
    }
    return result;
}

bool tcp_os_detector(void* context, uint8_t* target_ip, uint16_t source_port, uint16_t dest_port) {
    App* app = context;

    bool result = false;

    uint8_t target_mac[6] = {0};

    if(!arp_get_specific_mac(
           app->ethernet,
           app->ethernet->ip_address,
           (*(uint32_t*)app->ip_gateway & *(uint32_t*)app->ethernet->subnet_mask) ==
                   (*(uint32_t*)target_ip & *(uint32_t*)app->ethernet->subnet_mask) ?
               target_ip :
               app->ip_gateway,
           app->ethernet->mac_address,
           target_mac))
        return false;

    uint32_t sequence = furi_get_tick() ^ (rand() << 16);
    uint32_t ack_number = 0;

    uint32_t last_time = 0;
    uint8_t state = TCP_HS_SYN;
    switch(state) {
    case TCP_HS_SYN:
        if(tcp_send_syn(
               app->ethernet,
               app->mac_gateway,
               app->ip_gateway,
               target_mac,
               target_ip,
               source_port,
               dest_port,
               sequence,
               ack_number)) {
            // Get time
            last_time = furi_get_tick();

            state = TCP_HS_SYN_ACK;
        }
    case TCP_HS_SYN_ACK:
        while(!(furi_get_tick() - last_time > 3000)) {
            uint16_t packen_len = 0;

            packen_len = receive_packet(app->ethernet, app->ethernet->rx_buffer, 1500);

            if(packen_len) {
                if(is_arp(app->ethernet->rx_buffer)) {
                    arp_reply_requested(
                        app->ethernet, app->ethernet->rx_buffer, app->ethernet->ip_address);
                } else if(is_tcp(app->ethernet->rx_buffer)) {
                    // Packet is for me
                    if((*(uint16_t*)(app->ethernet->mac_address + 4) ==
                        *(uint16_t*)(app->ethernet->rx_buffer + 4)) &&
                       (*(uint32_t*)app->ethernet->mac_address ==
                        *(uint32_t*)app->ethernet->rx_buffer)) {
                        result = true;
                        break;
#if DEBUG
                        printf("SEQUENCE: %lu\n", sequence);
                        printf("ACK: %lu\n", ack_number);

                        printf("RECIBIDO: ");
                        for(uint16_t i = 0; i < packen_len; i++) {
                            printf(
                                "%02X%c",
                                app->ethernet->rx_buffer[i],
                                i == (packen_len - 1) ? '\n' : ' ');
                        }
#endif
                    } else {
                        result = false;
                    }
                }
            }
        }
    }
    return result;
}

bool tcp_send_ack_probe(
    enc28j60_t* ethernet,
    uint8_t* source_mac,
    uint8_t* source_ip,
    uint8_t* target_mac,
    uint8_t* target_ip,
    uint16_t source_port,
    uint16_t dest_port,
    uint32_t sequence) {
    uint16_t tcp_len = 0;

    if(!set_tcp_header_ack(
           ethernet->tx_buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN,
           source_ip,
           target_ip,
           source_port,
           dest_port,
           sequence,
           0,
           1024,
           0,
           &tcp_len))
        return false;

    if(!set_ipv4_header(
           ethernet->tx_buffer + ETHERNET_HEADER_LEN,
           6,
           tcp_len,
           source_ip,
           target_ip,
           0,
           0x4000,
           SCAN_TTL))
        return false;

    if(!set_ethernet_header(ethernet->tx_buffer, source_mac, target_mac, 0x800)) return false;

    send_packet(ethernet, ethernet->tx_buffer, ETHERNET_HEADER_LEN + IP_HEADER_LEN + tcp_len);

    return true;
}

bool tcp_send_fin_probe(
    enc28j60_t* ethernet,
    uint8_t* source_mac,
    uint8_t* source_ip,
    uint8_t* target_mac,
    uint8_t* target_ip,
    uint16_t source_port,
    uint16_t dest_port,
    uint32_t sequence) {
    uint16_t tcp_len = 0;

    if(!set_tcp_header_fin(
           ethernet->tx_buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN,
           source_ip,
           target_ip,
           source_port,
           dest_port,
           sequence,
           0,
           1024,
           0,
           &tcp_len))
        return false;

    if(!set_ipv4_header(
           ethernet->tx_buffer + ETHERNET_HEADER_LEN,
           6,
           tcp_len,
           source_ip,
           target_ip,
           0,
           0x4000,
           SCAN_TTL))
        return false;

    if(!set_ethernet_header(ethernet->tx_buffer, source_mac, target_mac, 0x800)) return false;

    send_packet(ethernet, ethernet->tx_buffer, ETHERNET_HEADER_LEN + IP_HEADER_LEN + tcp_len);

    return true;
}

bool tcp_send_null_probe(
    enc28j60_t* ethernet,
    uint8_t* source_mac,
    uint8_t* source_ip,
    uint8_t* target_mac,
    uint8_t* target_ip,
    uint16_t source_port,
    uint16_t dest_port,
    uint32_t sequence) {
    uint16_t tcp_len = 0;
    uint16_t options_size = 0;
    uint8_t* options_vector = NULL;

    if(!set_tcp_header_tseq(
           ethernet->tx_buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN,
           source_ip,
           target_ip,
           source_port,
           dest_port,
           sequence,
           0,
           1024,
           0,
           &options_size,
           options_vector,
           &tcp_len)) {
        return false;
    }
    if(!set_ipv4_header(
           ethernet->tx_buffer + ETHERNET_HEADER_LEN,
           6,
           tcp_len,
           source_ip,
           target_ip,
           0,
           0x4000,
           SCAN_TTL))
        return false;

    if(!set_ethernet_header(ethernet->tx_buffer, source_mac, target_mac, 0x800)) return false;

    send_packet(ethernet, ethernet->tx_buffer, ETHERNET_HEADER_LEN + IP_HEADER_LEN + tcp_len);

    return true;
}

bool tcp_send_xmas_probe(
    enc28j60_t* ethernet,
    uint8_t* source_mac,
    uint8_t* source_ip,
    uint8_t* target_mac,
    uint8_t* target_ip,
    uint16_t source_port,
    uint16_t dest_port,
    uint32_t sequence) {
    uint16_t tcp_len = 0;

    uint16_t options_size = 0;

    if(!set_tcp_header_tseq(
           ethernet->tx_buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN,
           source_ip,
           target_ip,
           source_port,
           dest_port,
           sequence,
           0,
           1024,
           0,
           &options_size,
           NULL,
           &tcp_len)) {
        return false;
    }

    if(!set_ipv4_header(
           ethernet->tx_buffer + ETHERNET_HEADER_LEN,
           6,
           tcp_len,
           source_ip,
           target_ip,
           0,
           0x4000,
           SCAN_TTL)) {
        return false;
    }

    if(!set_ethernet_header(ethernet->tx_buffer, source_mac, target_mac, 0x800)) {
        return false;
    }

    send_packet(ethernet, ethernet->tx_buffer, ETHERNET_HEADER_LEN + IP_HEADER_LEN + tcp_len);

    return true; // F0.7 — was `return false` after a successful send.
}
