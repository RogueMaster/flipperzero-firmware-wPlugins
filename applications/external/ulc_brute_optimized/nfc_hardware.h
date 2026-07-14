#pragma once

// Optional compiler optimization directives (currently disabled)
//#pragma GCC optimize("O3")           // Maximum optimization level
//#pragma GCC optimize("tree-vectorize") // Enable vectorization
//#pragma GCC optimize("unroll-loops")   // Basic loop unrolling
//#pragma GCC optimize("unroll-all-loops") // Aggressive loop unrolling

/**
 * Initialize ST25R3916 NFC reader with optimal high-performance settings
 * Configures the chip for maximum sensitivity and fastest possible communication
 * 
 * @param handle SPI bus handle for communication with ST25R3916
 * @return 0 on success, error code on failure
 */
int32_t nfc_init(FuriHalSpiBusHandle* handle) {
    FURI_LOG_D("NFC", "Initializing NFC field");

    // Reset chip to known state
    st25r3916_direct_cmd(handle, ST25R3916_CMD_SET_DEFAULT); // Reset all registers to defaults
    st25r3916_direct_cmd(handle, ST25R3916_CMD_CLEAR_FIFO); // Clear any pending data

    // Disable all interrupts during initialization for clean setup
    st25r3916_mask_irq(handle, ST25R3916_IRQ_MASK_NONE);

    // Configure receiver for maximum sensitivity
    st25r3916_write_reg(
        handle,
        ST25R3916_REG_RX_CONF1,
        ST25R3916_REG_RX_CONF1_h80 | // Set highest available gain (80dB)
            ST25R3916_REG_RX_CONF1_z600k // Set highest available input impedance
    );

    // Configure Automatic Gain Control (AGC) and squelch for optimal reception
    st25r3916_write_reg(
        handle,
        ST25R3916_REG_RX_CONF2,
        ST25R3916_REG_RX_CONF2_agc_en | // Enable Automatic Gain Control
            ST25R3916_REG_RX_CONF2_agc_m | // Use fast AGC mode for quick response
            ST25R3916_REG_RX_CONF2_sqm_dyn // Enable dynamic squelch for better noise handling
    );

    // Set operation mode to ISO14443A (standard for MIFARE cards)
    st25r3916_write_reg(handle, ST25R3916_REG_MODE, ST25R3916_REG_MODE_om_iso14443a);

    // Configure ISO14443A timing parameters for fastest possible communication
    st25r3916_write_reg(
        handle,
        ST25R3916_REG_ISO14443A_NFC,
        ST25R3916_REG_ISO14443A_NFC_no_tx_par | // Disable TX parity for speed
            ST25R3916_REG_ISO14443A_NFC_no_rx_par // Disable RX parity for speed
    );

    // Clear any pending interrupts and prepare for reception
    st25r3916_get_irq(handle); // Clear any pending interrupts
    st25r3916_direct_cmd(handle, ST25R3916_CMD_UNMASK_RECEIVE_DATA); // Enable data reception

    // Enable NFC field with both transmit and receive capabilities
    st25r3916_write_reg(
        handle,
        ST25R3916_REG_OP_CONTROL,
        ST25R3916_REG_OP_CONTROL_tx_en | // Enable transmitter
            ST25R3916_REG_OP_CONTROL_rx_en | // Enable receiver
            ST25R3916_REG_OP_CONTROL_en // Enable NFC field
    );

    // Wait for field to stabilize
    furi_delay_us(10000); // 10ms delay ensures stable field before operations

    FURI_LOG_D("NFC", "NFC field initialized");
    return 0;
}

/**
 * Safely shut down the NFC field and reset the ST25R3916
 * Ensures clean shutdown of NFC operations
 * 
 * @param handle SPI bus handle for communication with ST25R3916
 */
void nfc_deinit(FuriHalSpiBusHandle* handle) {
    FURI_LOG_I(TAG, "Disabling NFC field");

    // Disable all RF operations
    st25r3916_write_reg(handle, ST25R3916_REG_OP_CONTROL, 0x00); // Turn off TX, RX, and field

    // Reset chip state
    st25r3916_direct_cmd(handle, ST25R3916_CMD_SET_DEFAULT); // Reset registers to defaults
    st25r3916_direct_cmd(handle, ST25R3916_CMD_CLEAR_FIFO); // Clear any remaining data
    st25r3916_get_irq(handle); // Clear any pending interrupts

    // Wait for field to fully collapse
    furi_delay_ms(1); // 1ms delay ensures field is fully down

    FURI_LOG_I(TAG, "NFC field disabled");
}
