#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DAIKIN64_PACKET_SIZE 8
#define DAIKIN64_MIN_TEMP 16
#define DAIKIN64_MAX_TEMP 30

typedef enum {
    Daikin64ModeAuto = 0x0A,
    Daikin64ModeCool = 0x02,
    Daikin64ModeHeat = 0x08,
    Daikin64ModeDry = 0x01,
    Daikin64ModeFan = 0x04,
} Daikin64Mode;

typedef enum {
    Daikin64FanAuto = 0x01,
    Daikin64FanLow = 0x08,
    Daikin64FanMedium = 0x04,
    Daikin64FanHigh = 0x02,
    Daikin64FanTurbo = 0x03,
    Daikin64FanQuiet = 0x09,
} Daikin64Fan;

typedef struct {
    bool power;
    uint8_t mode;
    uint8_t temperature;
    uint8_t fan;
    bool swing;
    bool sleep;
    bool on_timer_enabled;
    bool off_timer_enabled;
    uint16_t on_timer_minutes;
    uint16_t off_timer_minutes;
} Daikin64State;

void daikin64_init(Daikin64State* state);
void daikin64_build_packet(const Daikin64State* state, uint8_t packet[DAIKIN64_PACKET_SIZE]);
void daikin64_build_power_packet(
    const Daikin64State* state,
    uint8_t packet[DAIKIN64_PACKET_SIZE]);
bool daikin64_verify_checksum(const uint8_t packet[DAIKIN64_PACKET_SIZE]);
void daikin64_send(const Daikin64State* state);
void daikin64_send_power_toggle(const Daikin64State* state);

#ifdef __cplusplus
}
#endif
