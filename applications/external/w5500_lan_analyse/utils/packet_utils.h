#pragma once

#include <stdint.h>
#include <stdbool.h>

/* Ethernet header size */
#define ETH_HEADER_SIZE 14

/* Common EtherType values */
#define ETHERTYPE_IPV4 0x0800
#define ETHERTYPE_ARP  0x0806
#define ETHERTYPE_IPV6 0x86DD
#define ETHERTYPE_LLDP 0x88CC

/* Maximum Ethernet frame size */
#define ETH_MAX_FRAME_SIZE 1518

/**
 * Extract EtherType from an Ethernet frame.
 * frame: raw Ethernet frame (at least 14 bytes)
 * Returns EtherType in host byte order.
 */
uint16_t pkt_get_ethertype(const uint8_t* frame);

/**
 * Extract destination MAC from Ethernet frame.
 */
void pkt_get_dst_mac(const uint8_t* frame, uint8_t dst[6]);

/**
 * Extract source MAC from Ethernet frame.
 */
void pkt_get_src_mac(const uint8_t* frame, uint8_t src[6]);

/**
 * Check if a MAC is broadcast (FF:FF:FF:FF:FF:FF).
 */
bool pkt_is_broadcast(const uint8_t mac[6]);

/**
 * Check if a MAC is multicast (bit 0 of first byte is 1).
 */
bool pkt_is_multicast(const uint8_t mac[6]);

/**
 * Format a MAC address as XX:XX:XX:XX:XX:XX.
 * buf must be at least 18 bytes.
 */
void pkt_format_mac(const uint8_t mac[6], char* buf);

/**
 * Format an IPv4 address as d.d.d.d.
 * buf must be at least 16 bytes.
 */
void pkt_format_ip(const uint8_t ip[4], char* buf);

/**
 * Read a big-endian uint16 from buffer.
 */
uint16_t pkt_read_u16_be(const uint8_t* buf);

/**
 * Read a big-endian uint32 from buffer.
 */
uint32_t pkt_read_u32_be(const uint8_t* buf);

/**
 * Write a big-endian uint16 to buffer.
 */
void pkt_write_u16_be(uint8_t* buf, uint16_t val);

/**
 * Write a big-endian uint32 to buffer.
 */
void pkt_write_u32_be(uint8_t* buf, uint32_t val);

/**
 * Compute Internet checksum (RFC 1071) over a buffer.
 */
uint16_t pkt_checksum(const uint8_t* buf, uint16_t len);
