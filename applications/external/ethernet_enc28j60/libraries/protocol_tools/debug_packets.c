#include "debug_packets.h"
#include "ethernet_protocol.h"
#include "ipv4.h"
#include "arp.h"
#include "icmp.h"
#include "udp.h"
#include "dhcp.h"
#include "tcp.h"

#define PRINT(fmt, ...)   printf(fmt, ##__VA_ARGS__)
#define PRINTLN(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)

#define PRINT_MAC(buffer)                \
    do {                                 \
        for(uint8_t i = 0; i < 6; i++) { \
            PRINT("%02x", (buffer)[i]);  \
            if(i < 5) PRINT(":");        \
        }                                \
        printf("\n");                    \
    } while(0)

#define PRINT_IP(buffer)                 \
    do {                                 \
        for(uint8_t i = 0; i < 4; i++) { \
            PRINT("%u", (buffer)[i]);    \
            if(i < 3) PRINT(".");        \
        }                                \
        printf("\n");                    \
    } while(0)

/**
 * functions to debug frames
 */

void print_buffer(uint8_t* buffer, uint16_t len) {
    for(uint16_t i = 0; i < len; i++) {
        printf("%02x ", buffer[i]);
    }
    printf("\n");
}

void print_payload(uint8_t* buffer, uint16_t len) {
    for(uint16_t i = 0; i < len; i++) {
        printf("%02x ", buffer[i]);
        if((i + 1) % 8 == 0) printf("  ");
        if((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n");
}

// Printe the ethernet information
void print_ethernet_info(uint8_t* buffer, uint16_t len) {
    UNUSED(buffer);
    UNUSED(len);
#if DEBUG_ETHERNET
    ethernet_header_t header = ethernet_get_header(buffer);

    PRINTLN("===============ETHERNET HEADER ===================================");
    PRINT("MAC SRC: ");
    PRINT_MAC(header.mac_source);
    PRINT("MAC DST: ");
    PRINT_MAC(header.mac_destination);
    PRINT("TYPE: ");
    print_buffer(header.type, 2);

#if DEBUG_ETHERNET_PAYLOAD
    print_payload(buffer + ETHERNET_HEADER_LEN, len - ETHERNET_HEADER_LEN);
#endif

#endif
}

// Print the IPv4 information
void print_ipv4_info(uint8_t* buffer, uint16_t len) {
    UNUSED(buffer);
    UNUSED(len);
#if DEBUG_IPV4
    ipv4_header_t ipv4_header = ipv4_get_header(buffer);

    PRINTLN("=============== IPV4 HEADER ===================================");

    // Print type of service
    PRINTLN("> Type Of Service %02x", ipv4_header.type_of_service);

    // Print length
    PRINT("> TOTAL LENGHT = ");
    print_buffer(ipv4_header.total_length, 2);

    // Print Protocol
    PRINT("> PROTOCOL: %02x", ipv4_header.protocol);

    // print IP src
    PRINT("> IP SRC: ");
    PRINT_IP(ipv4_header.source_ip);

    // print IP src
    PRINT("> IP DST: ");
    PRINT_IP(ipv4_header.dest_ip);

#if DEBUG_IPV4_PAYLOAD
    print_payload(
        buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN, len - ETHERNET_HEADER_LEN - IP_HEADER_LEN);
#endif

#endif
}

// Print ARP information
void print_arp_info(uint8_t* buffer, uint16_t len) {
    UNUSED(buffer);
    UNUSED(len);
#if DEBUG_ARP
    arp_header_t arp_header = arp_get_header(buffer);

    PRINTLN("=============== ARP HEADER ===================================");

    // HARDWARE TYPE
    PRINT("> HW Type: ");
    print_buffer(arp_header.hardware_type, 2);

    // PROTOCOL TYPE
    PRINT("> PROTOCOL Type: ");
    print_buffer(arp_header.protocol_type, 2);

    // PROTOCOL LENGTH
    PRINTLN("> HW lenght: %02x", arp_header.hardware_length);

    // PROTOCOL LENGTH
    PRINTLN("> PROTOCOL lenght: %02x", arp_header.protocol_length);

    // Operation code
    PRINTLN(
        "> OPERATION CODE: %02x%02x", arp_header.operation_code[0], arp_header.operation_code[1]);

    // MAC SRC
    PRINT("> MAC SRC: ");
    PRINT_MAC(arp_header.mac_source);

    // IP SRC
    PRINT("> IP SRC: ");
    PRINT_IP(arp_header.ip_source);

    // MAC DST
    PRINT("> MAC DST: ");
    PRINT_MAC(arp_header.mac_destiny);

    // IP DST
    PRINT("> IP DST: ");
    PRINT_IP(arp_header.ip_destiny);

#if DEBUG_ARP_PAYLOAD
    print_payload(buffer + ETHERNET_HEADER_LEN + ARP_LEN, len - ETHERNET_HEADER_LEN - ARP_LEN);
#endif

#endif
}

// Print ICMP information
void print_icmp_info(uint8_t* buffer, uint16_t len) {
    UNUSED(buffer);
    UNUSED(len);

#if DEBUG_ICMP
    icmp_header_t icmp_header = icmp_get_header(buffer);

    PRINTLN("=============== ICMP HEADER ===================================");

    // Type
    PRINTLN("> Type: %02x", icmp_header.type);

    // Code
    PRINTLN("> Code: %02x", icmp_header.code);

    // HARDWARE TYPE
    PRINT("> Identifier: ");
    print_buffer(icmp_header.identifier, 2);

    // PROTOCOL TYPE
    PRINT("> Sequence: ");
    print_buffer(icmp_header.sequence, 2);

#if DEBUG_ICMP_PAYLOAD
    print_payload(
        buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN + ICMP_HEADER_LEN,
        len - ETHERNET_HEADER_LEN - IP_HEADER_LEN - ICMP_HEADER_LEN);
#endif

#endif
}

// Print UDP information
void print_udp_info(uint8_t* buffer, uint16_t len) {
    UNUSED(buffer);
    UNUSED(len);

#if DEBUG_UDP
    udp_header_t udp_header = udp_get_header(buffer);

    PRINTLN("=============== UDP HEADER ===================================");

    // Source Port
    PRINT("> SRC PORT: ");
    print_buffer(udp_header.source_port, 2);

    // Destination Port
    PRINT("> DST PORT: ");
    print_buffer(udp_header.dest_port, 2);

    // Lenght of the payload
    PRINT("> LENGHT: ");
    print_buffer(udp_header.length, 2);

#if DEBUG_UDP_PAYLOAD
    print_payload(
        buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN + UDP_HEADER_LEN,
        len - (ETHERNET_HEADER_LEN + IP_HEADER_LEN + UDP_HEADER_LEN))
#endif

#endif
}

// Prin TCP, by the moment will be empty and UNUSED
void print_tcp_info(uint8_t* buffer, uint16_t len) {
    UNUSED(buffer);
    UNUSED(len);
#if DEBUG_TCP
    PRINTLN("=============== TCP HEADER ===================================");
#endif
}

// Print DHCP Information
void print_dhcp_info(uint8_t* buffer, uint16_t len) {
    UNUSED(buffer);
    UNUSED(len);

#if DEBUG_DHCP
    dhcp_message_t dhcp_message = dhcp_deconstruct_dhcp_message(buffer);

    PRINTLN("=============== DHCP MESSAGE HEADER ===================================");

    PRINTLN("> OPERATION:  %02x", dhcp_message.operation);

    PRINTLN(
        "> TRANSACTION ID: %02x%02x%02x%02x",
        dhcp_message.xid[0],
        dhcp_message.xid[1],
        dhcp_message.xid[2],
        dhcp_message.xid[3]);

    PRINT("> TYPE : %02x  ", dhcp_message.dhcp_options[2]);

    switch(dhcp_message.dhcp_options[2]) {
    case DHCP_DISCOVER:
        PRINTLN("DISCOVER MESSAGE");
        break;

    case DHCP_OFFER:
        PRINTLN("DISCOVER OFFER");
        break;

    case DHCP_REQUEST:
        PRINTLN("DISCOVER REQUEST");
        break;

    case DHCP_ACKNOLEDGE:
        PRINTLN("DISCOVER ACKNOLEDGE");
        break;

    default:
        PRINTLN("UNKNWON TYPE");
        break;
    }

    PRINT("> CLIENT ADRESS: ");
    PRINT_IP(dhcp_message.ciaddr);

    PRINT("> YOUR ADRESS: ");
    PRINT_IP(dhcp_message.yiaddr);

    PRINT("> GATEWAY ADRESS: ");
    PRINT_IP(dhcp_message.giaddr);

#endif
}

// Analize the packet
void analize_packet(uint8_t* buffer, uint16_t len) {
    print_ethernet_info(buffer, len);
    if(is_ipv4(buffer)) {
        print_ipv4_info(buffer, len);
    } else if(is_arp(buffer)) {
        print_arp_info(buffer, len);
        return;
    } else {
#if DEBUG_UNKNWON
        PRINTLN("=================UNKNWON MESSAGE ==============================");
        print_payload(buffer + ETHERNET_HEADER_LEN, len - ETHERNET_HEADER_LEN);
#endif
        return;
    }

    if(is_udp(buffer)) {
        print_udp_info(buffer, len);
    } else if(is_icmp(buffer)) {
        print_icmp_info(buffer, len);
        return;
    } else if(is_tcp(buffer)) {
        print_tcp_info(buffer, len);
        return;
    } else {
#if DEBUG_UNKNWON
        PRINTLN("=================UNKNWON MESSAGE ==============================");
        print_payload(buffer + ETHERNET_HEADER_LEN, len - ETHERNET_HEADER_LEN);
#endif
        return;
    }

    if(is_dhcp(buffer)) {
        print_dhcp_info(buffer, len);
    }
}

// To show all packet
void show_packet(uint8_t* buffer, uint16_t len) {
    if(len == 0) return;

    PRINTLN("==========================PACKET============================");
    print_payload(buffer, len);
}
