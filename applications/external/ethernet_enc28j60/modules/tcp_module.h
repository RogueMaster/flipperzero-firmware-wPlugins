#ifndef TCP_MODULE_H_
#define TCP_MODULE_H_

// Note: This module will be in development
#include <furi.h>
#include <furi_hal.h>

#include "../libraries/chip/enc28j60.h"

bool tcp_handshake_process(void* app, uint8_t* target_ip, uint16_t source_port, uint16_t dest_port);
bool tcp_handshake_process_spoof(
    void* app,
    uint8_t* target_ip,
    uint16_t source_port,
    uint16_t dest_port);
bool tcp_os_detector(void* context, uint8_t* target_ip, uint16_t source_port, uint16_t dest_port);

void tcp_syn_scan(void* context, uint8_t* target_ip, uint16_t init_port, uint16_t range_port);

bool tcp_send_syn(
    enc28j60_t* ethernet,
    uint8_t* source_mac,
    uint8_t* source_ip,
    uint8_t* target_mac,
    uint8_t* target_ip,
    uint16_t source_port,
    uint16_t dest_port,
    uint32_t sequence,
    uint32_t ack_number);

bool tcp_send_ack_probe(
    enc28j60_t* ethernet,
    uint8_t* source_mac,
    uint8_t* source_ip,
    uint8_t* target_mac,
    uint8_t* target_ip,
    uint16_t source_port,
    uint16_t dest_port,
    uint32_t sequence);

bool tcp_send_fin_probe(
    enc28j60_t* ethernet,
    uint8_t* source_mac,
    uint8_t* source_ip,
    uint8_t* target_mac,
    uint8_t* target_ip,
    uint16_t source_port,
    uint16_t dest_port,
    uint32_t sequence);

bool tcp_send_null_probe(
    enc28j60_t* ethernet,
    uint8_t* source_mac,
    uint8_t* source_ip,
    uint8_t* target_mac,
    uint8_t* target_ip,
    uint16_t source_port,
    uint16_t dest_port,
    uint32_t sequence);

bool tcp_send_xmas_probe(
    enc28j60_t* ethernet,
    uint8_t* source_mac,
    uint8_t* source_ip,
    uint8_t* target_mac,
    uint8_t* target_ip,
    uint16_t source_port,
    uint16_t dest_port,
    uint32_t sequence);

#endif
