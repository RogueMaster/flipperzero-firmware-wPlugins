#pragma once

#include <stdint.h>
#include <stdbool.h>

/* Wake-on-LAN port */
#define WOL_PORT 9

/* WoL socket on W5500 */
#define W5500_WOL_SOCKET 3

/* Magic packet size: 6 bytes 0xFF + 16 * 6 bytes MAC = 102 bytes */
#define WOL_PACKET_SIZE 102

/**
 * Build a Wake-on-LAN magic packet.
 * buf: output buffer (must be at least WOL_PACKET_SIZE bytes)
 * target_mac: MAC address of the target machine
 * Returns packet length (always WOL_PACKET_SIZE).
 */
uint16_t wol_build_magic_packet(uint8_t* buf, const uint8_t target_mac[6]);

/**
 * Send a Wake-on-LAN magic packet via UDP broadcast.
 * socket_num: W5500 socket to use
 * target_mac: MAC address of the target machine
 * Returns true if packet was sent successfully.
 */
bool wol_send(uint8_t socket_num, const uint8_t target_mac[6]);
