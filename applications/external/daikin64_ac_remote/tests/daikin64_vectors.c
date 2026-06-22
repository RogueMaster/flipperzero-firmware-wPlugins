#include "../lib/hvac_daikin64/hvac_daikin64.h"

#include <assert.h>
#include <stdint.h>

static void assert_packet(const Daikin64State* state, const uint8_t expected[8]) {
    uint8_t packet[8];
    daikin64_build_packet(state, packet);

    for(uint8_t i = 0; i < 8; i++) {
        assert(packet[i] == expected[i]);
    }

    assert(daikin64_verify_checksum(packet));
}

static void assert_valid_capture(const uint8_t packet[8]) {
    assert(daikin64_verify_checksum(packet));
}

int main(void) {
    Daikin64State state;
    daikin64_init(&state);

    state.power = false;
    state.mode = Daikin64ModeAuto;
    state.temperature = 27;
    state.fan = Daikin64FanLow;
    state.swing = false;
    const uint8_t auto_packet[8] = {0x75, 0x27, 0x18, 0x10, 0x10, 0x41, 0x8A, 0x16};
    assert_packet(&state, auto_packet);

    state.temperature = 28;
    const uint8_t temp_28_packet[8] = {0x85, 0x28, 0x18, 0x10, 0x10, 0x41, 0x8A, 0x16};
    assert_packet(&state, temp_28_packet);

    state.temperature = 27;
    state.mode = Daikin64ModeCool;
    const uint8_t cool_packet[8] = {0xF5, 0x27, 0x18, 0x10, 0x10, 0x41, 0x82, 0x16};
    assert_packet(&state, cool_packet);

    state.mode = Daikin64ModeDry;
    const uint8_t dry_packet[8] = {0xE5, 0x27, 0x18, 0x10, 0x10, 0x41, 0x81, 0x16};
    assert_packet(&state, dry_packet);

    state.mode = Daikin64ModeFan;
    const uint8_t fan_mode_packet[8] = {0x15, 0x27, 0x18, 0x10, 0x10, 0x41, 0x84, 0x16};
    assert_packet(&state, fan_mode_packet);

    state.mode = Daikin64ModeHeat;
    const uint8_t heat_packet[8] = {0x55, 0x27, 0x18, 0x10, 0x10, 0x41, 0x88, 0x16};
    assert_packet(&state, heat_packet);

    state.mode = Daikin64ModeAuto;
    state.swing = true;
    const uint8_t swing_packet[8] = {0x75, 0x27, 0x18, 0x10, 0x10, 0x41, 0x8A, 0x16};
    assert_packet(&state, swing_packet);

    const uint8_t captured_power_on[8] = {0xFD, 0x27, 0x18, 0x10, 0x10, 0x41, 0x8A, 0x16};
    const uint8_t captured_power_off[8] = {0x0D, 0x27, 0x18, 0x10, 0x10, 0x42, 0x8A, 0x16};

    assert_valid_capture(captured_power_on);
    assert_valid_capture(captured_power_off);
    assert_valid_capture(swing_packet);

    return 0;
}
