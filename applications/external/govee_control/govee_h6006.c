#include "govee_h6006.h"
#include <string.h>
#include <stdint.h>

uint8_t govee_calculate_checksum(const uint8_t* packet) {
    uint8_t checksum = 0;
    for(int i = 0; i < GOVEE_PACKET_SIZE - 1; i++) {
        checksum ^= packet[i];
    }
    return checksum;
}

bool govee_validate_packet(const uint8_t* packet) {
    return packet[GOVEE_PACKET_SIZE - 1] == govee_calculate_checksum(packet);
}

void govee_h6006_create_power_packet(uint8_t* packet, bool on) {
    memset(packet, 0, GOVEE_PACKET_SIZE);
    packet[0] = 0x33;
    packet[1] = GOVEE_CMD_POWER;
    packet[2] = on ? 0x01 : 0x00;
    packet[19] = govee_calculate_checksum(packet);
}

void govee_h6006_create_brightness_packet(uint8_t* packet, uint8_t brightness) {
    memset(packet, 0, GOVEE_PACKET_SIZE);
    packet[0] = 0x33;
    packet[1] = GOVEE_CMD_BRIGHTNESS;
    packet[2] = brightness; // 0x00 to 0xFE (0-254)
    packet[19] = govee_calculate_checksum(packet);
}

void govee_h6006_create_color_packet(uint8_t* packet, uint8_t r, uint8_t g, uint8_t b) {
    memset(packet, 0, GOVEE_PACKET_SIZE);
    packet[0] = 0x33;
    packet[1] = GOVEE_CMD_COLOR;
    packet[2] = 0x02; // RGB mode
    packet[3] = r;
    packet[4] = g;
    packet[5] = b;
    packet[19] = govee_calculate_checksum(packet);
}

void govee_h6006_create_white_packet(uint8_t* packet, uint16_t temperature) {
    // Temperature range: 2000K-9000K
    // Map to 0x00-0xFE
    uint8_t temp_value = (uint8_t)((temperature - 2000) * 254 / 7000);

    memset(packet, 0, GOVEE_PACKET_SIZE);
    packet[0] = 0x33;
    packet[1] = GOVEE_CMD_COLOR;
    packet[2] = 0x01; // White mode
    packet[3] = temp_value;
    packet[19] = govee_calculate_checksum(packet);
}

void govee_h6006_create_keepalive_packet(uint8_t* packet) {
    memset(packet, 0, GOVEE_PACKET_SIZE);
    packet[0] = 0xAA;
    packet[1] = 0x01;
    packet[2] = 0x00;
    packet[19] = govee_calculate_checksum(packet);
}
