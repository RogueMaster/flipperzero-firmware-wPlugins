#pragma once

// Optional compiler optimization directives
//#pragma GCC optimize("O3")           // Maximum optimization level
//#pragma GCC optimize("tree-vectorize") // Enable vectorization
//#pragma GCC optimize("unroll-loops")   // Basic loop unrolling
//#pragma GCC optimize("unroll-all-loops") // Aggressive loop unrolling

#include <lib/drivers/st25r3916.h>

// Timeout definitions in microseconds
#define NFC_TIMEOUT_DEFAULT 70 // Standard communication timeout
// not doing anything?
#define NFC_TIMEOUT_AUTH    1 // Authentication-specific timeout

// ST25R3916 register configuration for ISO14443A
// Masks and configurations for transmit/receive parity settings
#define ST25R3916_ISO14443A_MASK \
    (ST25R3916_REG_ISO14443A_NFC_no_tx_par | ST25R3916_REG_ISO14443A_NFC_no_rx_par)

#define ST25R3916_ISO14443A_CONFIG \
    (ST25R3916_REG_ISO14443A_NFC_no_tx_par_off | ST25R3916_REG_ISO14443A_NFC_no_rx_par_off)

// MIFARE Ultralight C protocol commands
typedef enum {
    ULC_CMD_WUPA = 0x52, // Wake Up Type A (7-bit command)
    ULC_CMD_READ = 0x30, // Read memory block
    ULC_CMD_AUTH1 = 0x1A, // First authentication step
    ULC_CMD_AUTH2 = 0xAF // Second authentication step
} UlcCommand;

/**
 * Prepares authentication data for AUTH2 command
 * Combines fixed random A values with received random B values
 * @param auth2_data_32 Pointer to output buffer (16 bytes)
 * @param rnd_b Pointer to received random B data (8 bytes)
 */
void setup_auth2_data(uint32_t* auth2_data_32, const uint8_t* rnd_b) {
    auth2_data_32[0] = 0x420FADED; // Fixed RndA part 1
    auth2_data_32[1] = 0xDEADC0DE; // Fixed RndA part 2
    auth2_data_32[2] = *((uint32_t*)rnd_b); // First 4 bytes of random B
    auth2_data_32[3] = *((uint32_t*)(rnd_b + 4)); // Last 4 bytes of random B
}

/**
 * Efficiently clears a receive buffer
 * Uses direct assignment for small buffers, memset for larger ones
 * @param buffer Buffer to clear
 * @param bits Number of bits to clear (converted to bytes)
 */
void clear_rx_buffer(uint8_t* buffer, size_t bits) {
    size_t bytes = (bits + 7) >> 3; // Convert bits to bytes, rounding up

    // Use 32-bit operations when possible
    uint32_t* buf32 = (uint32_t*)buffer;
    size_t words = bytes >> 2;

    for(size_t i = 0; i < words; i++) {
        if(buf32[i] == 0) return; // Stop if we find a zero word
        buf32[i] = 0;
    }

    // Handle remaining bytes
    for(size_t i = words << 2; i < bytes; i++) {
        if(buffer[i] == 0) return; // Stop if we find a zero byte
        buffer[i] = 0;
    }
}

/**
 * Sends WUPA command and receives response - Optimized version
 * @param handle SPI bus handle
 * @param response Buffer for card response
 * @param response_length Length of received response in bytes
 * @return 0 on success, negative error code on failure
 */
int32_t nfc_send_wupa(FuriHalSpiBusHandle* handle, uint8_t* response, size_t* response_length) {
    // Reset communication state
    st25r3916_direct_cmd(handle, ST25R3916_CMD_CLEAR_FIFO);
    st25r3916_get_irq(handle);

    // Configure ISO14443A parameters
    st25r3916_change_reg_bits(
        handle, ST25R3916_REG_ISO14443A_NFC, ST25R3916_ISO14443A_MASK, ST25R3916_ISO14443A_CONFIG);

    // Send WUPA command
    st25r3916_direct_cmd(handle, ST25R3916_CMD_TRANSMIT_WUPA);

    // Wait for response with timeout
    uint32_t irqs = st25r3916_get_irq(handle);
    uint32_t timeout = NFC_TIMEOUT_DEFAULT;
    while(timeout--) {
        if(irqs & ST25R3916_IRQ_MASK_RXE) break;
        furi_delay_us(1);
        irqs = st25r3916_get_irq(handle);
    }

    if(timeout == 0) return -1; // Timeout error

    // Read response
    size_t bits = 0;
    if(!st25r3916_read_fifo(handle, response, 32, &bits)) {
        return -2; // FIFO read error
    }
    *response_length = (bits + 7) / 8;

    return 0;
}

/**
 * Debug version of WUPA command with extensive logging
 * Same parameters as non-debug version but adds detailed logging
 */
int32_t
    nfc_send_wupa_debug(FuriHalSpiBusHandle* handle, uint8_t* response, size_t* response_length) {
    FURI_LOG_D("NFC", "Starting WUPA sequence");

    // Clear FIFO & interrupts
    st25r3916_direct_cmd(handle, ST25R3916_CMD_CLEAR_FIFO);
    uint32_t initial_irqs = st25r3916_get_irq(handle);
    FURI_LOG_D("NFC", "Initial IRQ state: 0x%08lx", initial_irqs);

    // Log current register states
    uint8_t iso14443a_reg = 0;
    st25r3916_read_reg(handle, ST25R3916_REG_ISO14443A_NFC, &iso14443a_reg);
    FURI_LOG_D("NFC", "ISO14443A register before config: 0x%02x", iso14443a_reg);

    // Configure for short frame with minimal overhead using predefined masks
    st25r3916_change_reg_bits(
        handle, ST25R3916_REG_ISO14443A_NFC, ST25R3916_ISO14443A_MASK, ST25R3916_ISO14443A_CONFIG);

    // Verify configuration
    st25r3916_read_reg(handle, ST25R3916_REG_ISO14443A_NFC, &iso14443a_reg);
    FURI_LOG_D("NFC", "ISO14443A register after config: 0x%02x", iso14443a_reg);

    // Log operation control register state
    uint8_t op_control = 0;
    st25r3916_read_reg(handle, ST25R3916_REG_OP_CONTROL, &op_control);
    FURI_LOG_D("NFC", "Operation Control register: 0x%02x", op_control);

    // Send WUPA command
    FURI_LOG_D("NFC", "Sending WUPA command");
    st25r3916_direct_cmd(handle, ST25R3916_CMD_TRANSMIT_WUPA);

    // Wait for response with logging
    uint32_t irqs = st25r3916_get_irq(handle);
    uint32_t timeout = NFC_TIMEOUT_DEFAULT;
    FURI_LOG_D("NFC", "Starting response wait loop, initial IRQs: 0x%08lx", irqs);

    while(timeout--) {
        if(irqs & ST25R3916_IRQ_MASK_RXE) {
            FURI_LOG_D("NFC", "Received RXE interrupt at timeout-%lu", timeout);
            break;
        }
        if(irqs & ST25R3916_IRQ_MASK_TXE) {
            FURI_LOG_D("NFC", "Transmit complete at timeout-%lu", timeout);
        }
        if(irqs & ST25R3916_IRQ_MASK_COL) {
            FURI_LOG_D("NFC", "Collision detected at timeout-%lu", timeout);
        }
        if(irqs & ST25R3916_IRQ_MASK_ERR1 || irqs & ST25R3916_IRQ_MASK_ERR2) {
            FURI_LOG_D("NFC", "Error flags detected: 0x%08lx at timeout-%lu", irqs, timeout);
        }
        furi_delay_us(1);
        irqs = st25r3916_get_irq(handle);
    }

    if(timeout == 0) {
        FURI_LOG_D("NFC", "Timeout waiting for response, final IRQs: 0x%08lx", irqs);
        return -1;
    }

    // Check FIFO status before reading
    uint8_t fifo_status[2];
    st25r3916_read_burst_regs(handle, ST25R3916_REG_FIFO_STATUS1, fifo_status, 2);
    FURI_LOG_D("NFC", "FIFO Status registers: 0x%02x 0x%02x", fifo_status[0], fifo_status[1]);

    // Read response
    size_t bits = 0;
    bool fifo_read_success = st25r3916_read_fifo(handle, response, 32, &bits);

    if(!fifo_read_success) {
        FURI_LOG_D("NFC", "FIFO read failed");
        return -2;
    }

    *response_length = (bits + 7) / 8;
    FURI_LOG_D("NFC", "FIFO read success - Bits: %d, Bytes: %d", bits, *response_length);

    // Log response data
    if(*response_length > 0) {
        char hex_response[97] = {0}; // 32 bytes * 3 chars per byte + null
        for(uint8_t i = 0; i < *response_length && i < 32; i++) {
            snprintf(hex_response + (i * 3), 4, "%02X ", response[i]);
        }
        FURI_LOG_D("NFC", "Response data: %s", hex_response);
    } else {
        FURI_LOG_D("NFC", "No response data received");
    }

    // Final IRQ state
    irqs = st25r3916_get_irq(handle);
    FURI_LOG_D("NFC", "Final IRQ state: 0x%08lx", irqs);

    return 0;
}

/**
 * Validates WUPA response against expected ATQA
 * @param handle SPI bus handle
 * @return true if valid ATQA received (2 bytes, first byte 0x44)
 */
bool nfc_send_wupa_and_validate(FuriHalSpiBusHandle* handle) {
    uint8_t rx_buffer[2];
    size_t rx_length = 0;

    if(nfc_send_wupa(handle, rx_buffer, &rx_length) != 0) return false;
    return (rx_length == 2 && rx_buffer[0] == 0x44);
}

/**
 * Debug version of WUPA validation with error logging
 */
bool nfc_send_wupa_and_validate_debug(FuriHalSpiBusHandle* handle) {
    uint8_t rx_buffer[2];
    size_t rx_length = 0;

    if(nfc_send_wupa_debug(handle, rx_buffer, &rx_length) != 0) {
        FURI_LOG_E("NFC", "WUPA failed");
        return false;
    }

    if(rx_length != 2 || rx_buffer[0] != 0x44) {
        FURI_LOG_E("NFC", "Invalid ATQA: %02X%02X", rx_buffer[0], rx_buffer[1]);
        return false;
    }

    return true;
}

/**
 * Basic NFC data exchange function
 * @param handle SPI bus handle
 * @param tx_buffer Data to send
 * @param tx_bits Number of bits to transmit
 * @param rx_buffer Buffer for received data
 * @param rx_bits Number of bits received
 * @return true on successful exchange
 */
bool nfc_transceive(
    FuriHalSpiBusHandle* handle,
    uint8_t* tx_buffer,
    uint8_t tx_bits,
    uint8_t* rx_buffer,
    size_t* rx_bits) {
    // Combined error masks for faster checking
    const uint32_t TX_ERROR_MASK =
        (ST25R3916_IRQ_MASK_COL | ST25R3916_IRQ_MASK_ERR1 | ST25R3916_IRQ_MASK_ERR2);
    const uint32_t RX_ERROR_MASK = (ST25R3916_IRQ_MASK_NRE | ST25R3916_IRQ_MASK_COL);

    // Clear FIFO and IRQ in one operation
    st25r3916_direct_cmd(handle, ST25R3916_CMD_CLEAR_FIFO);
    uint32_t irqs = st25r3916_get_irq(handle);

    // Write and transmit in sequence
    st25r3916_write_fifo(handle, tx_buffer, tx_bits);
    st25r3916_direct_cmd(handle, ST25R3916_CMD_TRANSMIT_WITH_CRC);

    // Wait for transmission complete
    uint32_t timeout = NFC_TIMEOUT_DEFAULT;

    do {
        irqs = st25r3916_get_irq(handle);
        // Check for completion or errors in single operation
        if(irqs & (ST25R3916_IRQ_MASK_TXE | TX_ERROR_MASK)) {
            // Exit if error or complete
            if(irqs & TX_ERROR_MASK) return false;
            if(irqs & ST25R3916_IRQ_MASK_TXE) break;
        }
        furi_delay_us(1); // Reduced delay for faster polling
    } while(--timeout);

    if(timeout == 0) return false;

    // Optimized receive wait loop
    timeout = NFC_TIMEOUT_DEFAULT;
    do {
        irqs = st25r3916_get_irq(handle);
        // Check for completion or errors in single operation
        if(irqs & (ST25R3916_IRQ_MASK_RXE | RX_ERROR_MASK)) {
            // Exit if error or complete
            if(irqs & RX_ERROR_MASK) return false;
            if(irqs & ST25R3916_IRQ_MASK_RXE) break;
        }
        furi_delay_us(1); // Reduced delay for faster polling
    } while(--timeout);

    if(timeout == 0) return false;

    // Read response
    size_t bits = 0;
    if(!st25r3916_read_fifo(handle, rx_buffer, 32, &bits)) return false;
    *rx_bits = bits;

    return true;
}

/**
 * Debug version of basic NFC transceive
 */
bool nfc_transceive_debug(
    FuriHalSpiBusHandle* handle,
    uint8_t* tx_buffer,
    uint8_t tx_bits,
    uint8_t* rx_buffer,
    size_t* rx_bits) {
    FURI_LOG_D("NFC", "Starting transceive operation");

    // Log transmission details
    char tx_hex[97] = {0};
    for(uint8_t i = 0; i < (tx_bits + 7) / 8 && i < 32; i++) {
        snprintf(tx_hex + (i * 3), 4, "%02X ", tx_buffer[i]);
    }
    FURI_LOG_D("NFC", "TX Data (%d bits): %s", tx_bits, tx_hex);

    // Clear FIFO and check initial state
    st25r3916_direct_cmd(handle, ST25R3916_CMD_CLEAR_FIFO);
    uint32_t initial_irqs = st25r3916_get_irq(handle);
    FURI_LOG_D("NFC", "Initial IRQ state: 0x%08lx", initial_irqs);

    // Write and transmit
    st25r3916_write_fifo(handle, tx_buffer, tx_bits);
    FURI_LOG_D("NFC", "Data written to FIFO, starting transmission");
    st25r3916_direct_cmd(handle, ST25R3916_CMD_TRANSMIT_WITH_CRC);

    // Wait for transmission complete
    uint32_t irqs = st25r3916_get_irq(handle);
    uint32_t timeout = NFC_TIMEOUT_DEFAULT;
    FURI_LOG_D("NFC", "Waiting for transmission complete, initial IRQs: 0x%08lx", irqs);

    while(timeout--) {
        if(irqs & ST25R3916_IRQ_MASK_TXE) {
            FURI_LOG_D("NFC", "Transmission complete at timeout-%lu", timeout);
            break;
        }
        if(irqs & ST25R3916_IRQ_MASK_COL) {
            FURI_LOG_E("NFC", "Collision detected during transmission");
            return false;
        }
        if(irqs & (ST25R3916_IRQ_MASK_ERR1 | ST25R3916_IRQ_MASK_ERR2)) {
            FURI_LOG_E("NFC", "Error flags during transmission: 0x%08lx", irqs);
            return false;
        }
        furi_delay_us(1);
        irqs = st25r3916_get_irq(handle);
    }

    if(timeout == 0) {
        FURI_LOG_E("NFC", "Transmission timeout, final IRQs: 0x%08lx", irqs);
        return false;
    }

    // Wait for receive complete
    timeout = NFC_TIMEOUT_DEFAULT;
    FURI_LOG_D("NFC", "Waiting for reception, starting IRQs: 0x%08lx", irqs);

    while(timeout--) {
        if(irqs & ST25R3916_IRQ_MASK_RXE) {
            FURI_LOG_D("NFC", "Reception complete at timeout-%lu", timeout);
            break;
        }
        if(irqs & ST25R3916_IRQ_MASK_NRE) {
            FURI_LOG_E("NFC", "No response timeout");
            return false;
        }
        if(irqs & ST25R3916_IRQ_MASK_COL) {
            FURI_LOG_E("NFC", "Collision during reception");
            return false;
        }
        furi_delay_us(1);
        irqs = st25r3916_get_irq(handle);
    }

    if(timeout == 0) {
        FURI_LOG_E("NFC", "Reception timeout, final IRQs: 0x%08lx", irqs);
        return false;
    }

    // Check FIFO status
    uint8_t fifo_status[2];
    st25r3916_read_burst_regs(handle, ST25R3916_REG_FIFO_STATUS1, fifo_status, 2);
    FURI_LOG_D("NFC", "FIFO Status registers: 0x%02x 0x%02x", fifo_status[0], fifo_status[1]);

    // Read response
    size_t bits = 0;
    if(!st25r3916_read_fifo(handle, rx_buffer, 32, &bits)) {
        FURI_LOG_E("NFC", "FIFO read failed");
        return false;
    }
    *rx_bits = bits;

    // Log response data
    char rx_hex[97] = {0};
    for(uint8_t i = 0; i < (bits + 7) / 8 && i < 32; i++) {
        snprintf(rx_hex + (i * 3), 4, "%02X ", rx_buffer[i]);
    }
    FURI_LOG_D("NFC", "RX Data (%d bits): %s", bits, rx_hex);

    return true;
}

/**
 * Validate NFC transceive response against expected parameters
 */
bool nfc_validate_response(
    uint8_t* rx_buffer,
    size_t rx_bits,
    uint8_t expected_rx_bits,
    uint8_t expected_first_byte) {
    if(expected_rx_bits > 0 && rx_bits != expected_rx_bits) return false;
    if(expected_first_byte != 0 && rx_buffer[0] != expected_first_byte) return false;
    return true;
}

/**
 * Combined transceive and validate function
 */
bool nfc_transceive_and_validate(
    FuriHalSpiBusHandle* handle,
    uint8_t* tx_buffer,
    uint8_t tx_bits,
    uint8_t* rx_buffer,
    size_t* rx_bits,
    uint8_t expected_rx_bits,
    uint8_t expected_first_byte) {
    if(!nfc_transceive(handle, tx_buffer, tx_bits, rx_buffer, rx_bits)) return false;
    return nfc_validate_response(rx_buffer, *rx_bits, expected_rx_bits, expected_first_byte);
}

/**
 * Debug version of combined transceive and validate
 */
bool nfc_transceive_and_validate_debug(
    FuriHalSpiBusHandle* handle,
    uint8_t* tx_buffer,
    uint8_t tx_bits,
    uint8_t* rx_buffer,
    size_t* rx_bits,
    uint8_t expected_rx_bits,
    uint8_t expected_first_byte) {
    if(!nfc_transceive_debug(handle, tx_buffer, tx_bits, rx_buffer, rx_bits)) return false;

    bool valid = nfc_validate_response(rx_buffer, *rx_bits, expected_rx_bits, expected_first_byte);

    if(!valid) {
        if(expected_rx_bits != 0 && *rx_bits != expected_rx_bits) {
            FURI_LOG_E(
                "NFC",
                "Invalid response length: got %d bits, expected %d",
                *rx_bits,
                expected_rx_bits);
        }
        if(expected_first_byte != 0 && rx_buffer[0] != expected_first_byte) {
            FURI_LOG_E(
                "NFC",
                "Invalid first byte: got 0x%02X, expected 0x%02X",
                rx_buffer[0],
                expected_first_byte);
        }
    }

    return valid;
}

// Add new function for transmit-only operations (no FIFO read)
bool nfc_transmit_only(FuriHalSpiBusHandle* handle, uint8_t* tx_buffer, uint8_t tx_bits) {
    // Reset communication state
    st25r3916_direct_cmd(handle, ST25R3916_CMD_CLEAR_FIFO);
    st25r3916_get_irq(handle);

    // Configure ISO14443A parameters
    st25r3916_change_reg_bits(
        handle, ST25R3916_REG_ISO14443A_NFC, ST25R3916_ISO14443A_MASK, ST25R3916_ISO14443A_CONFIG);

    // Send command
    st25r3916_write_fifo(handle, tx_buffer, tx_bits);
    st25r3916_direct_cmd(handle, ST25R3916_CMD_TRANSMIT_WITH_CRC);

    // Wait for transmission complete only
    uint32_t timeout = NFC_TIMEOUT_DEFAULT;
    uint32_t irqs;

    do {
        irqs = st25r3916_get_irq(handle);
        if(irqs & ST25R3916_IRQ_MASK_TXE) break;
        if(irqs & (ST25R3916_IRQ_MASK_COL | ST25R3916_IRQ_MASK_ERR1 | ST25R3916_IRQ_MASK_ERR2)) {
            return false;
        }
        furi_delay_us(1);
    } while(--timeout);

    return timeout > 0;
}
