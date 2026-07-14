#pragma once

// Optional compiler optimization directives (currently disabled)
//#pragma GCC optimize("O3")           // Maximum optimization level
//#pragma GCC optimize("tree-vectorize") // Enable vectorization
//#pragma GCC optimize("unroll-loops")   // Basic loop unrolling
//#pragma GCC optimize("unroll-all-loops") // Aggressive loop unrolling

#include "nfc_protocol.h"
#include "crypto.h"

/**
 * Tests a single key against the ULC card
 * Implements the complete authentication sequence:
 * 1. Wake up card (WUPA)
 * 2. Read page 0
 * 3. First authentication (AUTH1)
 * 4. Second authentication (AUTH2)
 * 
 * @param handle SPI bus handle for NFC communication
 * @param app Application context containing key data and state
 * @return true if key is found, false otherwise
 */
bool test_next_key(FuriHalSpiBusHandle* handle, AppContext* app) {
    // Pre-declare all variables at the top to avoid stack reallocations
    size_t rx_bits = 0;
    uint8_t read_page_cmd[2] = {ULC_CMD_READ, 0x00}; // Command to read page 0
    uint8_t auth1_cmd[2] = {ULC_CMD_AUTH1, 0x00}; // First authentication step
    static uint8_t auth2_cmd[17] = {ULC_CMD_AUTH2}; // Second auth with alignment
    uint8_t rnd_b[8]; // Storage for random B from card
    uint8_t iv[8] = {0}; // Initialization vector (zeros)
    uint8_t enc_rnd_b[8]; // Storage for encrypted random B

    // Step 1: Wake up card with WUPA command
    // Note: WUPA only needs to be called once per attempt
    if(!nfc_send_wupa_and_validate(handle)) {
        return false;
    }

    // Clear receive buffer between operations
    // doesnt seem to be required
    clear_rx_buffer(app->shared_buffer, rx_bits);
    rx_bits = 0;

    // Step 2: Read page 0 to ensure card is still responding
    if(!nfc_transceive_and_validate(
           handle, read_page_cmd, 16, app->shared_buffer, &rx_bits, 144, 0x00)) {
        return false;
    }

    // doesnt seem to be required
    clear_rx_buffer(app->shared_buffer, rx_bits);
    rx_bits = 0;

    // Step 3: Send AUTH1 command and receive encrypted random B
    if(!nfc_transceive_and_validate(
           handle, auth1_cmd, 16, app->shared_buffer, &rx_bits, 88, 0xAF)) {
        return false;
    }

    // Extract encrypted RndB using 32-bit operations for efficiency
    uint32_t* enc_rnd_b_32 = (uint32_t*)enc_rnd_b;
    enc_rnd_b_32[0] = *((uint32_t*)(app->shared_buffer + 1));
    enc_rnd_b_32[1] = *((uint32_t*)(app->shared_buffer + 5));

    // Calculate key for current attempt and decrypt random B
    calculate_key_from_index(app->current_key_index, app->key_mode, app->key);
    mf_ultralight_3des_decrypt(&app->ctx, app->key, iv, enc_rnd_b, 8, rnd_b);
    mf_ultralight_3des_shift_data(rnd_b);

    // Prepare AUTH2 data: concatenate RndA || RndB using 32-bit operations
    uint32_t* auth2_data_32 = (uint32_t*)app->auth2_data;
    setup_auth2_data(auth2_data_32, rnd_b);

    // Encrypt AUTH2 data for transmission
    mf_ultralight_3des_encrypt(&app->ctx, app->key, enc_rnd_b, app->auth2_data, 16, auth2_cmd + 1);

    // Step 4: Send AUTH2 command and check if key is correct
    if(!nfc_transceive_and_validate(
           handle, auth2_cmd, 136, app->shared_buffer, &rx_bits, 0, 0x00)) {
        return false;
    }

    bool success = (rx_bits > 16) & (app->shared_buffer[0] == 0x00);
    if(success) {
        uint8_t offset = 4 * app->key_mode;
        FURI_LOG_I(
            TAG,
            "Key found: %02X%02X%02X%02X",
            app->key[offset],
            app->key[offset + 1],
            app->key[offset + 2],
            app->key[offset + 3]);
        app->state = AppStateBruteComplete;
        return true;
    }

    // Update key index or mark as complete if we've tried all keys
    bool has_more_keys = app->current_key_index < (1 << 28);
    app->current_key_index += has_more_keys;
    app->state = has_more_keys ? app->state : AppStateBruteComplete;
    return false;
}
