#include "arp_module.h"
#include "../libraries/generals/ethernet_generals.h"
#include "../libraries/protocol_tools/ethernet_protocol.h"
#include "../libraries/protocol_tools/arp.h"
#include "../libraries/scanner/scanner_session.h"

// Packets sent per second
uint8_t packet_rate = 120;

// My Mac
uint8_t my_mac[6] = {0};

// My Ip
uint8_t my_ip[4] = {0};

// For function arp_get_requested
uint8_t mac_dest[6] = {0};

// For function arp_get_requested
uint8_t IP_SRC_REQUESTED[4] = {0};

void arp_set_my_mac_address(uint8_t* MAC) {
    if(MAC == NULL) return;
    memcpy(my_mac, MAC, 6);
}

void arp_set_my_ip_address(uint8_t* ip) {
    if(ip == NULL) return;
    memcpy(my_ip, ip, 4);
}

void set_arp_message_for_attack_all(
    uint8_t* buffer,
    uint8_t* MAC,
    uint8_t* ip_for_router,
    uint16_t* len) {
    set_ethernet_header(buffer, MAC, MAC_BROADCAST, 0x806);

    // For the ArpSpoofing we need to send this continuosly
    arp_set_header(
        buffer + ETHERNET_HEADER_LEN,
        MAC_SPOOF,
        MAC_BROADCAST,
        ip_for_router,
        IP_BROADCAST,
        0x0002);

    // Set the length of the message
    *len = ETHERNET_HEADER_LEN + ARP_LEN;
}

void arp_set_message_attack(
    uint8_t* buffer,
    uint8_t* ip_src,
    uint8_t* mac_src,
    uint8_t* ip_dst,
    uint8_t* mac_dst,
    uint16_t* len) {
    set_ethernet_header(buffer, mac_src, mac_dst, 0x806);

    arp_set_header(buffer + ETHERNET_HEADER_LEN, MAC_SPOOF, mac_dst, ip_src, ip_dst, 0x0002);

    *len = ETHERNET_HEADER_LEN + ARP_LEN;
}

void send_arp_spoofing(enc28j60_t* ethernet, uint8_t* buffer, uint16_t len) {
    static uint32_t prev_time = 0;

    if((furi_get_tick()) > (prev_time + (1000 / packet_rate))) {
        send_packet(ethernet, buffer, len); // Send packet
        prev_time = furi_get_tick();
    }
}

bool set_arp_request(uint8_t* buffer, uint16_t* len, uint8_t* target_ip) {
    if(!buffer || !target_ip) return false;

    // Set up Ethernet header (ARP type = 0x0806)
    if(!set_ethernet_header(buffer, my_mac, MAC_BROADCAST, 0x0806)) {
        return false;
    }

    // Set up ARP header
    // ARP operation code 1 = request
    if(!arp_set_header(buffer + ETHERNET_HEADER_LEN, my_mac, MAC_ZEROS, my_ip, target_ip, 1)) {
        return false;
    }

    *len = ETHERNET_HEADER_LEN + ARP_LEN;

    return true;
}

bool set_arp_reply(uint8_t* buffer, uint8_t* mac_dst, uint8_t* target_ip) {
    if(!buffer || !target_ip) return false;

    // Set the Ethernet header
    set_ethernet_header(buffer, my_mac, mac_dest, 0x0806);

    // Set the ARP header
    arp_header_t arp_header = {0};

    arp_header.hardware_type[0] = 0x00;
    arp_header.hardware_type[1] = 0x01; // Ethernet

    arp_header.protocol_type[0] = 0x08;
    arp_header.protocol_type[1] = 0x00; // IPv4

    arp_header.hardware_length = 6; // MAC length
    arp_header.protocol_length = 4; // IP length

    arp_header.operation_code[0] = 0x00;
    arp_header.operation_code[1] = 0x02; // Reply

    memcpy(arp_header.mac_source, my_mac, 6);
    memcpy(arp_header.ip_source, my_ip, 4);
    memcpy(arp_header.mac_destiny, mac_dst, 6);
    memcpy(arp_header.ip_destiny, target_ip, 4);

    memcpy(buffer + ETHERNET_HEADER_LEN, &arp_header, ARP_LEN);

    return true;
}

bool is_the_ip(uint8_t* current_ip, uint8_t* compare_ip) {
    uint32_t ip_get_it = current_ip[0] << 24 | current_ip[1] << 16 | current_ip[2] << 8 |
                         current_ip[3];
    uint32_t ip_propused = compare_ip[0] << 24 | compare_ip[1] << 16 | compare_ip[2] << 8 |
                           compare_ip[3];

    // And check if it is the same
    if(ip_get_it != ip_propused) return false;

    return true;
}

bool get_arp_reply(uint8_t* target_ip, uint8_t* target_mac, uint8_t* buffer, uint16_t len) {
    if(!buffer || len < ETHERNET_HEADER_LEN + ARP_LEN) return false;

    // Get ARP header
    arp_header_t arp_header = arp_get_header(buffer);

    // Check if this is an ARP reply (operation code 2)
    uint16_t opcode = (arp_header.operation_code[0] << 8) | arp_header.operation_code[1];
    if(opcode != 2) return false;

    // And check if it is the same ip destiny
    if(!is_the_ip(my_ip, arp_header.ip_destiny)) return false;

    // IP to get from the source
    if(!is_the_ip(target_ip, arp_header.ip_source)) return false;

    // Copy the MAC Address
    memcpy(target_mac, arp_header.mac_source, 6);

    return true;
}

bool get_arp_requested(uint8_t* buffer, uint8_t* dst_ip) {
    if(!buffer || !dst_ip) return false;

    if(!is_arp(buffer)) return false;

    arp_header_t arp_header = arp_get_header(buffer);

    // Check if it is a request
    uint16_t opcode = (arp_header.operation_code[0] << 8) | arp_header.operation_code[1];
    if(opcode != 1) return false;

    // Check if the IP requested is the same as the destination IP
    if(!is_the_ip(dst_ip, arp_header.ip_destiny)) return false;

    // MAC source
    memcpy(mac_dest, arp_header.mac_source, 6);

    // IP source requested
    memcpy(IP_SRC_REQUESTED, arp_header.ip_source, 4);

    return true;
}

// F0.4f — moved typedef + predicate to arp_module.h so scanner_session
// can reuse them. Same body, public name.
bool arp_reply_match_predicate(const uint8_t* frame, uint16_t len, void* ctx) {
    arp_reply_match_ctx_t* c = (arp_reply_match_ctx_t*)ctx;
    return get_arp_reply(c->target_ip, c->target_mac, (uint8_t*)frame, len);
}

void arp_scan_network(
    struct ScannerSession* scanner,
    arp_list* list,
    uint8_t init_ip[4],
    uint8_t* list_count,
    uint8_t range) {
    enc28j60_t* ethernet = scanner->ethernet;

    arp_set_my_mac_address(ethernet->mac_address);
    arp_set_my_ip_address(ethernet->ip_address);

    uint8_t* tx_buffer = ethernet->tx_buffer;

    uint8_t start_list[4] = {0};
    memcpy(start_list, init_ip, 4);

    uint8_t counter = 0;
    uint16_t packet_len = 0;

    for(uint8_t i = 0; i < range; i++) {
        // F0.3c — honor cancel between IPs.
        if(scanner_cancel_requested(scanner)) break;

        // F0.5d — register first, send second (via trigger). The previous
        // order (send → register inside scanner_wait_for_packet) had a
        // sub-millisecond window where a fast reply could be drained by
        // rx_dispatch before any predicate matched it.
        set_arp_request(tx_buffer, &packet_len, start_list);

        scanner_send_trigger_ctx_t trigger_ctx = {
            .eth = ethernet,
            .buf = tx_buffer,
            .len = packet_len,
        };
        arp_reply_match_ctx_t ctx = {
            .target_ip = start_list,
            .target_mac = list[counter].mac,
        };
        uint16_t got = 0;
        if(scanner_wait_for_packet(
               scanner,
               arp_reply_match_predicate,
               &ctx,
               scanner_send_packet_trigger,
               &trigger_ctx,
               &got,
               1000)) {
            memcpy(list[counter].ip, start_list, 4);
            counter++;
        }

        // Check if the ip address is the last
        if(start_list[3] == 255) {
            start_list[2]++;
            start_list[3] = 1;
        }

        // Add one more
        start_list[3]++;
    }

    *list_count = counter;
}

bool arp_get_specific_mac(
    enc28j60_t* ethernet,
    uint8_t* sender_ip,
    uint8_t* target_ip,
    uint8_t* sender_mac,
    uint8_t* target_mac) {
    uint16_t packet_send_len = 0;
    uint16_t packet_receive_len = 0;
    uint8_t packet_send[60] = {0};
    uint8_t packet_receive[60] = {0};

    // Flag to return
    bool ret = false;

    // Set my MAC
    arp_set_my_mac_address(sender_mac);

    // Set my IP
    arp_set_my_ip_address(sender_ip);

    // Set the ARP request
    set_arp_request(packet_send, &packet_send_len, target_ip);

    uint8_t attemps = 0;

    do {
        uint32_t last_time = furi_get_tick();

        // Send arp packet
        send_packet(ethernet, packet_send, packet_send_len);

        while(((furi_get_tick() - last_time) < 2000) && !ret) {
            packet_receive_len = receive_packet(ethernet, packet_receive, 1500);
            if(packet_receive_len) {
                if(is_arp(packet_receive)) {
                    ret = get_arp_reply(target_ip, target_mac, packet_receive, packet_receive_len);
                }
            }

            furi_delay_us(1);
        }

        attemps++;
    } while(attemps < 10 && !ret);

    return ret;
}

bool arp_reply_requested(enc28j60_t* ethernet, uint8_t* buffer, uint8_t* dst_ip) {
    if(!buffer || !dst_ip) return false;

    // Check if it is an ARP request
    if(!get_arp_requested(buffer, dst_ip)) return false;

    // Copy my mac address
    memcpy(my_mac, ethernet->mac_address, 6);

    // Copy my IP address
    memcpy(my_ip, dst_ip, 4);

    uint8_t buffer_reply[ETHERNET_HEADER_LEN + ARP_LEN] = {0};

    if(!set_arp_reply(buffer_reply, mac_dest, IP_SRC_REQUESTED)) return false;

    send_packet(ethernet, buffer_reply, ETHERNET_HEADER_LEN + ARP_LEN);

    return true;
}

void send_arp_gratuitous(enc28j60_t* ethernet, uint8_t* source_mac, uint8_t* source_ip) {
    uint8_t* buffer = calloc(42, sizeof(uint8_t));

    set_ethernet_header(buffer, source_mac, MAC_BROADCAST, 0x0806);
    arp_set_header(buffer + 14, source_mac, MAC_ZEROS, source_ip, source_ip, 1);

    send_packet(ethernet, buffer, 42);

    free(buffer);
}

uint8_t is_duplicated_ip(uint8_t* ip, arp_list* list, uint8_t total_list) {
    uint8_t count_of_duplicated = 0;
    for(uint8_t i = 0; i < total_list; i++) {
        if(is_the_ip(list[i].ip, ip)) count_of_duplicated++;
    }

    // F0.7 — was `return count - 1` which underflowed to 0xFF when the
    // IP wasn't in the list (count == 0), making callers that treat the
    // return as a bool see a "duplicated" answer for IPs that never
    // existed in the list.
    return count_of_duplicated == 0 ? 0 : count_of_duplicated - 1;
}
