#pragma once

#include <infrared_transmit.h>
#include <infrared_worker.h>

#include "furi_hal.h"

// Standard (14-byte) Samsung A/C frame: two 7-byte sections, each with its
// own checksum. Field layout, timings and checksum reverse-engineered in
// IRremoteESP8266 (ir_Samsung.cpp), originally documented in issue #1538.
#define HVAC_SAMSUNG_PACKET_SIZE     14
#define HVAC_SAMSUNG_SECTION_SIZE    7
typedef uint8_t* HvacSamsungPacket;

HvacSamsungPacket hvac_samsung_create_packet(void);
void hvac_samsung_free_packet(HvacSamsungPacket packet);

typedef enum {
    HvacSamsungModeCool,
    HvacSamsungModeHeat,
    HvacSamsungModeDry,
    HvacSamsungModeFan,
    HvacSamsungModeAuto,
} HvacSamsungMode;
void hvac_samsung_set_mode(HvacSamsungPacket packet, HvacSamsungMode mode);

typedef enum {
    HvacSamsungFanAuto,
    HvacSamsungFanLow,
    HvacSamsungFanMed,
    HvacSamsungFanHigh,
} HvacSamsungFan;
void hvac_samsung_set_fan(HvacSamsungPacket packet, HvacSamsungFan fan);

typedef uint8_t HvacSamsungTemperature;
#define HVAC_SAMSUNG_TEMPERATURE_MIN     (HvacSamsungTemperature)16
#define HVAC_SAMSUNG_TEMPERATURE_MAX     (HvacSamsungTemperature)30
#define HVAC_SAMSUNG_TEMPERATURE_DEFAULT (HvacSamsungTemperature)24
void hvac_samsung_set_temperature(HvacSamsungPacket packet, HvacSamsungTemperature temperature);

void hvac_samsung_set_power(HvacSamsungPacket packet, bool on);
void hvac_samsung_set_swing(HvacSamsungPacket packet, bool on);

#define HVAC_SAMSUNG_TRANSMIT_FREQUENCY  38000
#define HVAC_SAMSUNG_TRANSMIT_DUTY_CYCLE 0.5

#define HVAC_SAMSUNG_HDR_MARK      690
#define HVAC_SAMSUNG_HDR_SPACE     17844
#define HVAC_SAMSUNG_SECTION_MARK  3086
#define HVAC_SAMSUNG_SECTION_SPACE 8864
#define HVAC_SAMSUNG_BIT_MARK      586
#define HVAC_SAMSUNG_ONE_SPACE     1432
#define HVAC_SAMSUNG_ZERO_SPACE    436
#define HVAC_SAMSUNG_SECTION_GAP   2886

// header mark+space, then per section: section mark+space, 56 bits (mark+space
// each), footer mark+gap
#define HVAC_SAMSUNG_TIMINGS_LEN \
    (2 + (HVAC_SAMSUNG_PACKET_SIZE / HVAC_SAMSUNG_SECTION_SIZE) * (2 + 7 * 8 * 2 + 2))

// Recomputes both section checksums then transmits the frame once.
void hvac_samsung_send(HvacSamsungPacket packet);
