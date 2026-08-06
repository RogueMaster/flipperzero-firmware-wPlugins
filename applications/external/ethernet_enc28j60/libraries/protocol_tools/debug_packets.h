#ifndef DEBUG_PACKETS_H_
#define DEBUG_PACKETS_H_

#include <furi.h>
#include <furi_hal.h>

#define DEBUG_ALL_PACKETS 0

#define PRINT_ALL_PAYLOAD

#if DEBUG_ALL_PACKETS
#define DEBUG_ETHERNET 1
#define DEBUG_IPV4     1
#define DEBUG_ICMP     1
#define DEBUG_ARP      1
#define DEBUG_UDP      1
#define DEBUG_TCP      1
#define DEBUG_DHCP     1
#else
#define DEBUG_ETHERNET 0
#define DEBUG_IPV4     0
#define DEBUG_ICMP     0
#define DEBUG_ARP      0
#define DEBUG_UDP      0
#define DEBUG_TCP      0
#define DEBUG_DHCP     0
#endif

#if DEBUG_ETHERNET
#define DEBUG_ETHERNET_PAYLOAD 0
#endif

#if DEBUG_IPV4
#define DEBUG_IPV4_PAYLOAD 0
#endif

#if DEBUG_ARP
#define DEBUG_ARP_PAYLOAD 0
#endif

#if DEBUG_ICMP
#define DEBUG_ICMP_PAYLOAD 0
#endif

#if DEBUG_UDP
#define DEBUG_UDP_PAYLOAD 0
#endif

#if DEBUG_TCP
#define DEBUG_TCP_PAYLOAD 0
#endif

#if DEBUG_DHCP
#define DEBUG_DHCP_PAYLOAD 0
#endif

#define DEBUG_UNKNWON 0

/**
 * @brief Analyzes and processes a network packet.
 *
 * This function takes a raw network packet from a buffer and performs
 * a detailed analysis of its headers, from the lowest layer (Ethernet) up to
 * the transport or application layer. It identifies the packet's type (e.g., ARP, IPv4, IPv6),
 * and, if applicable, the higher-level protocols (e.g., TCP, UDP, ICMP).
 *
 * @param buffer A pointer to the buffer containing the raw network packet.
 * @param len The total length of the packet in bytes.
 */
void analize_packet(uint8_t* buffer, uint16_t len);

/**
 * @brief Prints the contents of a network packet for debugging.
 *
 * This function provides a formatted output of a raw network packet.
 * It typically displays the packet's data in a hex dump.
 *
 * @param buffer A pointer to the buffer containing the raw network packet.
 * @param len The total length of the packet in bytes.
 */
void show_packet(uint8_t* buffer, uint16_t len);

#endif
