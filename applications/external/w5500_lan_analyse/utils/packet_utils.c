#include "packet_utils.h"
#include <string.h>
#include <furi.h>

uint16_t pkt_get_ethertype(const uint8_t* frame) {
    return ((uint16_t)frame[12] << 8) | frame[13];
}

void pkt_get_dst_mac(const uint8_t* frame, uint8_t dst[6]) {
    memcpy(dst, frame, 6);
}

void pkt_get_src_mac(const uint8_t* frame, uint8_t src[6]) {
    memcpy(src, frame + 6, 6);
}

bool pkt_is_broadcast(const uint8_t mac[6]) {
    for(uint8_t i = 0; i < 6; i++) {
        if(mac[i] != 0xFF) return false;
    }
    return true;
}

bool pkt_is_multicast(const uint8_t mac[6]) {
    return (mac[0] & 0x01) != 0;
}

void pkt_format_mac(const uint8_t mac[6], char* buf) {
    snprintf(
        buf, 18, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void pkt_format_ip(const uint8_t ip[4], char* buf) {
    snprintf(buf, 16, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
}

uint16_t pkt_read_u16_be(const uint8_t* buf) {
    return ((uint16_t)buf[0] << 8) | buf[1];
}

uint32_t pkt_read_u32_be(const uint8_t* buf) {
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) | buf[3];
}

void pkt_write_u16_be(uint8_t* buf, uint16_t val) {
    buf[0] = (uint8_t)(val >> 8);
    buf[1] = (uint8_t)(val & 0xFF);
}

void pkt_write_u32_be(uint8_t* buf, uint32_t val) {
    buf[0] = (uint8_t)(val >> 24);
    buf[1] = (uint8_t)(val >> 16);
    buf[2] = (uint8_t)(val >> 8);
    buf[3] = (uint8_t)(val & 0xFF);
}

uint16_t pkt_checksum(const uint8_t* buf, uint16_t len) {
    uint32_t sum = 0;
    uint16_t i;

    /* Sum up 16-bit words */
    for(i = 0; i + 1 < len; i += 2) {
        sum += ((uint16_t)buf[i] << 8) | buf[i + 1];
    }

    /* Handle odd byte */
    if(i < len) {
        sum += (uint16_t)buf[i] << 8;
    }

    /* Fold 32-bit sum to 16 bits */
    while(sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (uint16_t)(~sum);
}
