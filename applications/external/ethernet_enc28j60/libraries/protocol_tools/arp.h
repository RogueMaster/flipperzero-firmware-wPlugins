#ifndef ARP_H_
#define ARP_H_

#include <furi.h>
#include <furi_hal.h>

#define ARP_LEN 28

typedef struct {
    uint8_t hardware_type[2];
    uint8_t protocol_type[2];
    uint8_t hardware_length;
    uint8_t protocol_length;
    uint8_t operation_code[2];
    uint8_t mac_source[6];
    uint8_t ip_source[4];
    uint8_t mac_destiny[6];
    uint8_t ip_destiny[4];
} arp_header_t;

/**
 * @brief Fills an ARP packet header in a buffer.
 *
 * This function constructs an ARP (Address Resolution Protocol) header
 * within the provided buffer. It populates all the necessary fields, including
 * hardware and protocol addresses for both the source and destination, and the
 * ARP operation code (opcode).
 *
 * @param buffer Pointer to the buffer where the ARP header will be written.
 * @param MAC_SRC Pointer to the 6-byte source MAC address.
 * @param MAC_DES Pointer to the 6-byte destination MAC address.
 * @param IP_SRC Pointer to the 4-byte source IP address.
 * @param IP_DES Pointer to the 4-byte destination IP address.
 * @param opcode The 16-bit ARP operation code (e.g., ARP_REQUEST, ARP_REPLY).
 * @return `true` if the header was successfully created, `false` otherwise.
 */
bool arp_set_header(
    uint8_t* buffer,
    uint8_t* MAC_SRC,
    uint8_t* MAC_DES,
    uint8_t* IP_SRC,
    uint8_t* IP_DES,
    uint16_t opcode);

/**
 * @brief Checks if a network packet is an ARP packet.
 *
 * This function examines a network packet's header to determine if it is an
 * ARP (Address Resolution Protocol) packet.
 *
 * @param buffer A pointer to the buffer containing the start of the network packet.
 * @return `true` if the packet is an ARP packet, `false` otherwise.
 */
bool is_arp(uint8_t* buffer);

/**
 * @brief Parses an ARP packet and extracts its header information.
 *
 * This function takes a raw network packet buffer, verifies that it's an
 * ARP (Address Resolution Protocol) packet, and then populates an `arp_header_t`
 * structure with the relevant fields from the packet's header. This includes
 * hardware type, protocol type, hardware and protocol address lengths, opcode,
 * and the sender and target MAC and IP addresses.
 *
 * @param buffer A pointer to the buffer containing the ARP packet.
 * @return An `arp_header_t` structure containing the parsed ARP header information.
 */
arp_header_t arp_get_header(uint8_t* buffer);

#endif
