#ifndef _ETHERNET_H_
#define _ETHERNET_H_

#include <furi.h>
#include <furi_hal.h>

#define WIN_TTL             128
#define ETHERNET_HEADER_LEN 14

typedef struct {
    uint8_t mac_destination[6];
    uint8_t mac_source[6];
    uint8_t type[2];
} ethernet_header_t;

/**
 * @brief Populates an Ethernet header within a given buffer.
 *
 * This function takes a raw buffer and fills the first 14 bytes
 * with a complete Ethernet header, including the source and destination
 * MAC addresses and the EtherType.
 *
 * @param buffer A pointer to the buffer where the Ethernet header will be written.
 * @param mac_origin A pointer to the 6-byte source MAC address.
 * @param mac_destination A pointer to the 6-byte destination MAC address.
 * @param type The 16-bit EtherType value (e.g., 0x0800 for IPv4, 0x0806 for ARP).
 * @return `true` if the header was set successfully, `false` otherwise.
 */
bool set_ethernet_header(
    uint8_t* buffer,
    uint8_t* mac_origin,
    uint8_t* mac_destination,
    uint16_t type);

/**
 * @brief Parses a network packet and extracts its Ethernet header.
 *
 * This function reads the first 14 bytes from a raw packet buffer
 * and populates an `ethernet_header_t` structure with the corresponding
 * information (destination MAC, source MAC, and EtherType).
 *
 * @param buffer A pointer to the buffer containing the network packet.
 * @return An `ethernet_header_t` structure containing the parsed header information.
 */
ethernet_header_t ethernet_get_header(uint8_t* buffer);

#endif
