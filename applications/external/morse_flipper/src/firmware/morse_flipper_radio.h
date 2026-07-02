/*
 * Purpose: Publish the low-level Morse radio backend API.
 * Owns: radio profile enum, backend state, and TX/RX control declarations.
 * Depends on: host-safe integer types; CC1101 code is isolated in the .c file.
 * Tests: tests/test_rf.c.
 */

#ifndef yo3gnd_radio_2230ff
#define yo3gnd_radio_2230ff

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MorseFlipperRadioProfileOokData = 0,
    MorseFlipperRadioProfileCarrierSense = 1,
} MorseFlipperRadioProfile;

typedef struct {
    bool ready;
    bool tx_on;
    bool rx_on;
    bool tx_level;
    uint32_t freq_hz;
    uint32_t tuned_hz;
    MorseFlipperRadioProfile profile;
    void (*rx_cb)(void* ctx, bool level, uint16_t duration_ms);
    void* rx_ctx;
    bool rx_mark[64];
    uint16_t rx_ms[64];
    uint8_t rx_wr;
    uint8_t rx_rd;
} MorseFlipperRadio;

void morse_flipper_radio_init(MorseFlipperRadio* radio);
void morse_flipper_radio_deinit(MorseFlipperRadio* radio);
void morse_flipper_radio_set_rx_callback(
    MorseFlipperRadio* radio,
    void (*cb)(void* ctx, bool level, uint16_t duration_ms),
    void* ctx);
void morse_flipper_radio_sync_live(
    MorseFlipperRadio* radio,
    uint32_t freq_hz,
    bool active,
    bool tx_on,
    MorseFlipperRadioProfile profile);
void morse_flipper_radio_drain_rx(MorseFlipperRadio* radio);
void morse_flipper_radio_set_tx_level(MorseFlipperRadio* radio, bool level);
bool morse_flipper_radio_read_rx_level(const MorseFlipperRadio* radio);

#endif
