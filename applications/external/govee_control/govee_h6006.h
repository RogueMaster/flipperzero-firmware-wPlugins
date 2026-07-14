#pragma once

#include <stdint.h>
#include <stdbool.h>

#define GOVEE_PACKET_SIZE     20
#define GOVEE_SERVICE_UUID    "000102030405060708090a0b0c0d1910"
#define GOVEE_CHAR_WRITE_UUID "000102030405060708090a0b0c0d2b10"

// H6006 Commands
#define GOVEE_CMD_POWER      0x01
#define GOVEE_CMD_BRIGHTNESS 0x04
#define GOVEE_CMD_COLOR      0x05
#define GOVEE_CMD_KEEPALIVE  0xAA

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t warm_white;
    uint8_t cold_white;
} GoveeColor;

typedef struct {
    char name[32];
    uint8_t address[6];
    bool connected;
    uint8_t brightness;
    bool power_on;
    GoveeColor color;
} GoveeH6006;

// Packet creation functions
void govee_h6006_create_power_packet(uint8_t* packet, bool on);
void govee_h6006_create_brightness_packet(uint8_t* packet, uint8_t brightness);
void govee_h6006_create_color_packet(uint8_t* packet, uint8_t r, uint8_t g, uint8_t b);
void govee_h6006_create_white_packet(uint8_t* packet, uint16_t temperature);
void govee_h6006_create_keepalive_packet(uint8_t* packet);

// Utility functions
uint8_t govee_calculate_checksum(const uint8_t* packet);
bool govee_validate_packet(const uint8_t* packet);
