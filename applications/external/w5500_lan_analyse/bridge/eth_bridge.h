#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "pcap_dump.h"

/**
 * Ethernet Bridge Engine
 *
 * Bridges Ethernet frames between the USB CDC-ECM interface
 * (connected to phone/PC) and the W5500 MACRAW socket (connected to LAN).
 *
 * Operates at Layer 2 — transparently forwards all Ethernet frames.
 * The phone gets an IP from the LAN's DHCP server.
 *
 * Optional PCAP dump: when enabled, all forwarded frames are written
 * to a .pcap file on the SD card (Wireshark-compatible).
 */

typedef struct {
    volatile bool running;
    uint32_t frames_usb_to_eth; /* Frames forwarded: USB host -> W5500 LAN */
    uint32_t frames_eth_to_usb; /* Frames forwarded: W5500 LAN -> USB host */
    uint32_t bytes_usb_to_eth; /* Bytes forwarded: USB host -> W5500 LAN */
    uint32_t bytes_eth_to_usb; /* Bytes forwarded: W5500 LAN -> USB host */
    uint32_t errors; /* Total forwarding errors */
    bool usb_connected; /* Last known USB connection state */
    bool lan_link_up; /* Last known LAN link state */
    bool dump_enabled; /* PCAP dump toggle */
    PcapDumpState pcap; /* PCAP dump state */
} EthBridgeState;

/**
 * Initialize bridge state.
 */
void eth_bridge_init(EthBridgeState* state);

/**
 * Single poll cycle: check both interfaces for frames and forward.
 * Call this repeatedly from the worker thread.
 *
 * frame_buf: shared buffer for frame transfer (must be >= 1518 bytes)
 * buf_size: size of frame_buf
 */
void eth_bridge_poll(EthBridgeState* state, uint8_t* frame_buf, uint16_t buf_size);
