#include "hvac_daikin64.h"

#ifndef HVAC_DAIKIN64_NO_FLIPPER
#include <furi_hal_rtc.h>
#include <infrared_transmit.h>
#endif

#include <stddef.h>

#define DAIKIN64_FREQ 38000
#define DAIKIN64_DUTY_CYCLE 0.50f

#define DAIKIN64_LEADER_MARK 9800
#define DAIKIN64_LEADER_SPACE 9800
#define DAIKIN64_HDR_MARK 4600
#define DAIKIN64_HDR_SPACE 2500
#define DAIKIN64_BIT_MARK 350
#define DAIKIN64_ONE_SPACE 954
#define DAIKIN64_ZERO_SPACE 382
#define DAIKIN64_GAP 20300

#define DAIKIN64_TIMINGS_COUNT ((2 * 2) + 2 + (64 * 2) + 2 + 1)

// APGS02 neutral frame bytes in protocol order, least-significant byte first.
static const uint8_t daikin64_base_lsb[DAIKIN64_PACKET_SIZE] =
    {0x16, 0x8A, 0x41, 0x10, 0x10, 0x18, 0x27, 0x05};

static uint8_t daikin64_clamp_temp(uint8_t temperature) {
    if(temperature < DAIKIN64_MIN_TEMP) return DAIKIN64_MIN_TEMP;
    if(temperature > DAIKIN64_MAX_TEMP) return DAIKIN64_MAX_TEMP;
    return temperature;
}

static uint8_t daikin64_bcd(uint8_t value) {
    return (uint8_t)(((value / 10U) << 4) | (value % 10U));
}

static uint16_t daikin64_valid_timer_minutes(uint16_t minutes) {
    if(minutes >= 24U * 60U) return 0;
    return minutes;
}

static uint8_t daikin64_timer_lsb(uint16_t minutes, bool enabled) {
    minutes = daikin64_valid_timer_minutes(minutes);

    uint8_t value = daikin64_bcd(minutes / 60U) & 0x3F;
    if((minutes % 60U) >= 30U) value |= 0x40;
    if(enabled) value |= 0x80;
    return value;
}

static void daikin64_set_clock_lsb(uint8_t frame[DAIKIN64_PACKET_SIZE], uint8_t hour, uint8_t minute) {
    frame[2] = daikin64_bcd(minute % 60U);
    frame[3] = daikin64_bcd(hour % 24U);
}

static uint8_t daikin64_valid_mode(uint8_t mode) {
    switch(mode) {
    case Daikin64ModeAuto:
    case Daikin64ModeCool:
    case Daikin64ModeHeat:
    case Daikin64ModeDry:
    case Daikin64ModeFan:
        return mode;
    default:
        return Daikin64ModeCool;
    }
}

static uint8_t daikin64_valid_fan(uint8_t fan) {
    switch(fan) {
    case Daikin64FanAuto:
    case Daikin64FanLow:
    case Daikin64FanMedium:
    case Daikin64FanHigh:
    case Daikin64FanTurbo:
    case Daikin64FanQuiet:
        return fan;
    default:
        return Daikin64FanLow;
    }
}

static uint8_t daikin64_checksum_lsb(const uint8_t frame[DAIKIN64_PACKET_SIZE]) {
    uint8_t sum = 0;

    for(size_t i = 0; i < DAIKIN64_PACKET_SIZE - 1; i++) {
        sum += frame[i] & 0x0F;
        sum += frame[i] >> 4;
    }

    sum += frame[DAIKIN64_PACKET_SIZE - 1] & 0x0F;
    return sum & 0x0F;
}

static void daikin64_lsb_to_packet(
    const uint8_t frame[DAIKIN64_PACKET_SIZE],
    uint8_t packet[DAIKIN64_PACKET_SIZE]) {
    for(size_t i = 0; i < DAIKIN64_PACKET_SIZE; i++) {
        packet[i] = frame[DAIKIN64_PACKET_SIZE - 1U - i];
    }
}

static void daikin64_packet_to_lsb(
    const uint8_t packet[DAIKIN64_PACKET_SIZE],
    uint8_t frame[DAIKIN64_PACKET_SIZE]) {
    for(size_t i = 0; i < DAIKIN64_PACKET_SIZE; i++) {
        frame[i] = packet[DAIKIN64_PACKET_SIZE - 1U - i];
    }
}

static void daikin64_refresh_clock_and_checksum(uint8_t packet[DAIKIN64_PACKET_SIZE]) {
    uint8_t frame[DAIKIN64_PACKET_SIZE];
    daikin64_packet_to_lsb(packet, frame);

#ifndef HVAC_DAIKIN64_NO_FLIPPER
    DateTime datetime;
    furi_hal_rtc_get_datetime(&datetime);
    daikin64_set_clock_lsb(frame, datetime.hour, datetime.minute);
#endif

    frame[7] &= 0x0F;
    frame[7] |= (uint8_t)(daikin64_checksum_lsb(frame) << 4);
    daikin64_lsb_to_packet(frame, packet);
}

void daikin64_init(Daikin64State* state) {
    if(!state) return;

    state->power = false;
    state->mode = Daikin64ModeCool;
    state->temperature = 27;
    state->fan = Daikin64FanLow;
    state->swing = true;
    state->sleep = false;
    state->on_timer_enabled = false;
    state->off_timer_enabled = false;
    state->on_timer_minutes = 10U * 60U;
    state->off_timer_minutes = 18U * 60U;
}

static void daikin64_build(
    const Daikin64State* state,
    uint8_t packet[DAIKIN64_PACKET_SIZE],
    bool power_toggle) {
    if(!packet) return;

    Daikin64State fallback;
    if(!state) {
        daikin64_init(&fallback);
        state = &fallback;
    }

    uint8_t frame[DAIKIN64_PACKET_SIZE];
    for(size_t i = 0; i < DAIKIN64_PACKET_SIZE; i++) {
        frame[i] = daikin64_base_lsb[i];
    }

    frame[1] = (uint8_t)((daikin64_valid_fan(state->fan) << 4) | daikin64_valid_mode(state->mode));
    frame[4] = daikin64_timer_lsb(state->on_timer_minutes, state->on_timer_enabled);
    frame[5] = daikin64_timer_lsb(state->off_timer_minutes, state->off_timer_enabled);
    frame[6] = daikin64_bcd(daikin64_clamp_temp(state->temperature));
    frame[7] = 0x04;
    if(state->swing) frame[7] |= 0x01;
    if(state->sleep) frame[7] |= 0x02;
    if(power_toggle || state->power) frame[7] |= 0x08;
    frame[7] |= (uint8_t)(daikin64_checksum_lsb(frame) << 4);

    daikin64_lsb_to_packet(frame, packet);
}

void daikin64_build_packet(const Daikin64State* state, uint8_t packet[DAIKIN64_PACKET_SIZE]) {
    daikin64_build(state, packet, false);
}

bool daikin64_verify_checksum(const uint8_t packet[DAIKIN64_PACKET_SIZE]) {
    if(!packet) return false;

    uint8_t frame[DAIKIN64_PACKET_SIZE];
    daikin64_packet_to_lsb(packet, frame);

    return ((frame[7] >> 4) & 0x0F) == daikin64_checksum_lsb(frame);
}

void daikin64_build_power_packet(
    const Daikin64State* state,
    uint8_t packet[DAIKIN64_PACKET_SIZE]) {
    daikin64_build(state, packet, true);
}

static void daikin64_send_packet(const uint8_t packet[DAIKIN64_PACKET_SIZE]) {
#ifndef HVAC_DAIKIN64_NO_FLIPPER
    uint32_t timings[DAIKIN64_TIMINGS_COUNT];
    size_t timing_index = 0;

    for(uint8_t i = 0; i < 2; i++) {
        timings[timing_index++] = DAIKIN64_LEADER_MARK;
        timings[timing_index++] = DAIKIN64_LEADER_SPACE;
    }

    timings[timing_index++] = DAIKIN64_HDR_MARK;
    timings[timing_index++] = DAIKIN64_HDR_SPACE;
    for(int byte_index = DAIKIN64_PACKET_SIZE - 1; byte_index >= 0; byte_index--) {
        for(uint8_t bit = 0; bit < 8; bit++) {
            timings[timing_index++] = DAIKIN64_BIT_MARK;
            timings[timing_index++] =
                (packet[byte_index] & (1U << bit)) ? DAIKIN64_ONE_SPACE : DAIKIN64_ZERO_SPACE;
        }
    }

    timings[timing_index++] = DAIKIN64_BIT_MARK;
    timings[timing_index++] = DAIKIN64_GAP;
    timings[timing_index++] = DAIKIN64_HDR_MARK;

    infrared_send_raw_ext(
        timings, (uint32_t)timing_index, true, DAIKIN64_FREQ, DAIKIN64_DUTY_CYCLE);
#else
    (void)packet;
#endif
}

void daikin64_send(const Daikin64State* state) {
    uint8_t packet[DAIKIN64_PACKET_SIZE];
    daikin64_build_packet(state, packet);
    daikin64_refresh_clock_and_checksum(packet);
    daikin64_send_packet(packet);
}

void daikin64_send_power_toggle(const Daikin64State* state) {
    uint8_t packet[DAIKIN64_PACKET_SIZE];
    daikin64_build_power_packet(state, packet);
    daikin64_refresh_clock_and_checksum(packet);
    daikin64_send_packet(packet);
}
