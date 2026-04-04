#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * PCAP Traffic Dump for ETH Bridge
 *
 * Writes captured Ethernet frames to a .pcap file on the SD card
 * in standard libpcap format (compatible with Wireshark/tcpdump).
 *
 * Usage:
 *   1. pcap_dump_start() — create file, write global header
 *   2. pcap_dump_frame() — append each frame with timestamp
 *   3. pcap_dump_stop()  — flush and close
 *
 * File is written incrementally (no RAM ring buffer needed).
 * Frames are flushed to SD after each write for crash safety.
 */

/** Dump statistics */
typedef struct {
    uint32_t frames_written;
    uint32_t frames_dropped; /* write errors */
    uint32_t bytes_written;
    bool active;
} PcapDumpState;

/**
 * Start PCAP capture: create timestamped .pcap file, write global header.
 * state: zeroed and populated on success
 * Returns true on success.
 */
bool pcap_dump_start(PcapDumpState* state);

/**
 * Write a single Ethernet frame to the PCAP file.
 * frame: raw Ethernet frame (dest MAC + src MAC + ethertype + payload)
 * len: frame length in bytes
 */
void pcap_dump_frame(PcapDumpState* state, const uint8_t* frame, uint16_t len);

/**
 * Stop capture: close file, log final stats.
 */
void pcap_dump_stop(PcapDumpState* state);
