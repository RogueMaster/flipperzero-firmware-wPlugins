#include "vk_thermo_nfc.h"
#include <furi_hal.h>
#include <toolbox/bit_buffer.h>

#define TAG "VkThermoNfc"

// ISO15693 flags
#define ISO15693_FLAG_HIGH_DATA_RATE 0x02
#define ISO15693_FLAG_ADDRESSED      0x20

// Timing
#define VK_THERMO_EH_DELAY_MS  50
#define VK_THERMO_I2C_DELAY_MS 10

typedef enum {
    VkThermoNfcStateIdle,
    VkThermoNfcStateScanning,
    VkThermoNfcStateTagDetected,
    VkThermoNfcStatePolling,
    VkThermoNfcStateEnableEH,
    VkThermoNfcStateWaitEH,
    VkThermoNfcStateReadTemp,
    VkThermoNfcStateSuccess,
    VkThermoNfcStateError,
} VkThermoNfcState;

struct VkThermoNfc {
    Nfc* nfc;
    NfcScanner* scanner;
    NfcPoller* poller;

    volatile VkThermoNfcState state;
    NfcProtocol detected_protocol;

    VkThermoNfcCallback callback;
    void* callback_context;

    VkThermoNfcData last_data;
    uint32_t eh_start_time;
    uint32_t eh_timeout_seconds;
    uint8_t retry_count;
    bool debug_mode;
    volatile bool stop_requested;
};

// EH status for distinguishing "waiting" from "tag lost"
typedef enum {
    VkThermoEhStatusLoadOk, // LOAD_OK is set, proceed with read
    VkThermoEhStatusWaiting, // Tag present but LOAD_OK not yet set
    VkThermoEhStatusTagLost, // NFC communication failed, tag likely gone
} VkThermoEhStatus;

// Forward declarations
static void vk_thermo_nfc_scanner_callback(NfcScannerEvent event, void* context);
static NfcCommand vk_thermo_nfc_poller_callback(NfcGenericEvent event, void* context);
static VkThermoEhStatus vk_thermo_nfc_check_eh_ready(Iso15693_3Poller* poller, const uint8_t* uid);
static bool vk_thermo_nfc_write_config(
    Iso15693_3Poller* poller,
    const uint8_t* uid,
    uint8_t address,
    const uint8_t* block_data);

// Convert Flipper NFC layer error to human-readable string
// These are transport-level errors from iso15693_3_poller_send_frame()
static const char* vk_thermo_nfc_error_str(Iso15693_3Error error) {
    switch(error) {
    case Iso15693_3ErrorNone:
        return "Success";
    case Iso15693_3ErrorNotPresent:
        return "Tag not present (removed from field)";
    case Iso15693_3ErrorBufferOverflow:
        return "Buffer overflow (response too large)";
    case Iso15693_3ErrorFieldOff:
        return "NFC field off (hardware issue)";
    case Iso15693_3ErrorWrongCrc:
        return "CRC error (corrupted response)";
    case Iso15693_3ErrorTimeout:
        return "Timeout (tag didn't respond - may need SELECTED mode or tag doesn't support command)";
    case Iso15693_3ErrorFormat:
        return "Format error (malformed response)";
    case Iso15693_3ErrorNotSupported:
        return "Command not supported by tag";
    case Iso15693_3ErrorUidMismatch:
        return "UID mismatch (wrong tag)";
    case Iso15693_3ErrorUnexpectedResponse:
        return "Unexpected response format";
    case Iso15693_3ErrorInternal:
        return "Internal Flipper NFC error";
    default:
        return "Unknown NFC error";
    }
}

// Convert ISO15693 response error code to human-readable string
// When response flag byte has bit 0 set, byte 1 contains error code
static const char* vk_thermo_nfc_iso_error_str(uint8_t error_code) {
    switch(error_code) {
    case 0x01:
        return "Command not supported";
    case 0x02:
        return "Command not recognized (format error)";
    case 0x03:
        return "Option not supported";
    case 0x0F:
        return "Unknown error";
    case 0x10:
        return "Block not available";
    case 0x11:
        return "Block already locked";
    case 0x12:
        return "Block locked, content cannot be changed";
    case 0x13:
        return "Block not successfully programmed";
    case 0x14:
        return "Block not successfully locked";
    default:
        if(error_code >= 0xA0 && error_code <= 0xDF) {
            return "Custom command error (NXP-specific)";
        }
        return "Unknown ISO15693 error";
    }
}

// Convert raw TMP117 value to Celsius
// TMP117: 16-bit signed value, 0.0078125°C per LSB
static float vk_thermo_tmp117_raw_to_celsius(int16_t raw) {
    return (float)raw * 0.0078125f;
}

// Convert raw TMP112 value to Celsius
// TMP112: 12-bit signed value, left-justified (bits 15:4), 0.0625°C per LSB
static float vk_thermo_tmp112_raw_to_celsius(int16_t raw) {
    int16_t temp_12bit = raw >> 4;
    return (float)temp_12bit * 0.0625f;
}

// Send NXP custom command via ISO15693
// Uses ADDRESSED mode since tag is in Quiet state after inventory
// Reference: ISO15693-3 says tags in Quiet state respond to addressed commands
static bool vk_thermo_nfc_send_custom_cmd(
    Iso15693_3Poller* poller,
    const uint8_t* uid,
    uint8_t cmd,
    const uint8_t* params,
    size_t params_len,
    uint8_t* response,
    size_t* response_len) {
    BitBuffer* tx_buffer = bit_buffer_alloc(32);
    BitBuffer* rx_buffer = bit_buffer_alloc(32);
    bool success = false;

    // Build command: [FLAGS][CMD][MANUF_CODE][UID 8 bytes LSB-first][...params...]
    // Addressed mode (0x22) - tag in Quiet state responds to commands with its UID
    bit_buffer_append_byte(tx_buffer, ISO15693_FLAG_HIGH_DATA_RATE | ISO15693_FLAG_ADDRESSED);
    bit_buffer_append_byte(tx_buffer, cmd);
    bit_buffer_append_byte(tx_buffer, NXP_MANUF_CODE);

    // UID must be sent LSB-first (reversed from how Flipper stores it)
    // Flipper stores UID MSB-first after inventory response parsing
    for(int i = ISO15693_3_UID_SIZE - 1; i >= 0; i--) {
        bit_buffer_append_byte(tx_buffer, uid[i]);
    }

    // Parameters
    if(params && params_len > 0) {
        bit_buffer_append_bytes(tx_buffer, params, params_len);
    }

    // Debug: log what we're sending
    size_t tx_len = bit_buffer_get_size_bytes(tx_buffer);
    FURI_LOG_D(TAG, "TX cmd=0x%02X len=%zu:", cmd, tx_len);
    for(size_t i = 0; i < tx_len && i < 16; i++) {
        FURI_LOG_D(TAG, "  TX[%zu]=0x%02X", i, bit_buffer_get_byte(tx_buffer, i));
    }

    // Send command
    Iso15693_3Error error =
        iso15693_3_poller_send_frame(poller, tx_buffer, rx_buffer, ISO15693_3_FDT_POLL_FC);

    FURI_LOG_D(TAG, "Send result: error=%d", error);

    if(error == Iso15693_3ErrorNone) {
        size_t rx_len = bit_buffer_get_size_bytes(rx_buffer);
        FURI_LOG_D(TAG, "RX len=%zu", rx_len);

        // Debug: log what we received
        for(size_t i = 0; i < rx_len && i < 16; i++) {
            FURI_LOG_D(TAG, "  RX[%zu]=0x%02X", i, bit_buffer_get_byte(rx_buffer, i));
        }

        if(rx_len > 0) {
            // Check response flag byte (ISO15693 response format)
            // Bit 0 = Error flag (1 = error, next byte is error code)
            uint8_t resp_flag = bit_buffer_get_byte(rx_buffer, 0);
            if((resp_flag & 0x01) == 0) {
                // Success - copy response data (skip flag byte)
                if(response && response_len && rx_len > 1) {
                    *response_len = rx_len - 1;
                    for(size_t i = 0; i < *response_len; i++) {
                        response[i] = bit_buffer_get_byte(rx_buffer, i + 1);
                    }
                }
                success = true;
                FURI_LOG_I(TAG, "Command 0x%02X success, resp_len=%zu", cmd, rx_len - 1);
            } else {
                // Error response - decode error code
                if(rx_len > 1) {
                    uint8_t error_code = bit_buffer_get_byte(rx_buffer, 1);
                    FURI_LOG_E(
                        TAG,
                        "Command 0x%02X error: %s (code=0x%02X, flag=0x%02X)",
                        cmd,
                        vk_thermo_nfc_iso_error_str(error_code),
                        error_code,
                        resp_flag);
                } else {
                    FURI_LOG_E(
                        TAG, "Command 0x%02X error: flag=0x%02X (no error code)", cmd, resp_flag);
                }
            }
        } else {
            FURI_LOG_W(TAG, "Command 0x%02X: empty response (0 bytes)", cmd);
        }
    } else {
        FURI_LOG_E(
            TAG, "Command 0x%02X send error: %s (%d)", cmd, vk_thermo_nfc_error_str(error), error);
    }

    bit_buffer_free(tx_buffer);
    bit_buffer_free(rx_buffer);
    return success;
}

// Test standard ISO15693 GET_SYSTEM_INFO (0x2B) - should work on all ISO15693 tags
static bool vk_thermo_nfc_test_get_system_info(Iso15693_3Poller* poller, const uint8_t* uid) {
    FURI_LOG_I(TAG, "Reading tag info with GET_SYSTEM_INFO(0x2B)");

    BitBuffer* tx_buffer = bit_buffer_alloc(16);
    BitBuffer* rx_buffer = bit_buffer_alloc(32);
    bool success = false;

    // Standard ISO15693 GET_SYSTEM_INFO: [FLAGS][0x2B][UID if addressed]
    bit_buffer_append_byte(tx_buffer, ISO15693_FLAG_HIGH_DATA_RATE | ISO15693_FLAG_ADDRESSED);
    bit_buffer_append_byte(tx_buffer, 0x2B); // GET_SYSTEM_INFO

    // UID LSB-first
    for(int i = ISO15693_3_UID_SIZE - 1; i >= 0; i--) {
        bit_buffer_append_byte(tx_buffer, uid[i]);
    }

    Iso15693_3Error error =
        iso15693_3_poller_send_frame(poller, tx_buffer, rx_buffer, ISO15693_3_FDT_POLL_FC);

    if(error == Iso15693_3ErrorNone) {
        size_t rx_len = bit_buffer_get_size_bytes(rx_buffer);
        FURI_LOG_I(TAG, "GET_SYSTEM_INFO success! rx_len=%zu", rx_len);
        if(rx_len >= 2) {
            uint8_t flags = bit_buffer_get_byte(rx_buffer, 0);
            uint8_t info_flags = bit_buffer_get_byte(rx_buffer, 1);
            FURI_LOG_I(TAG, "  Response flags: 0x%02X, Info flags: 0x%02X", flags, info_flags);

            // Log raw response bytes for analysis
            FURI_LOG_D(TAG, "  Raw response:");
            for(size_t i = 0; i < rx_len && i < 16; i++) {
                FURI_LOG_D(TAG, "    [%zu] = 0x%02X", i, bit_buffer_get_byte(rx_buffer, i));
            }
        }
        success = true;
    } else {
        FURI_LOG_E(TAG, "GET_SYSTEM_INFO failed: %s (%d)", vk_thermo_nfc_error_str(error), error);
    }

    bit_buffer_free(tx_buffer);
    bit_buffer_free(rx_buffer);
    return success;
}

// Test NXP READ_CONFIG (0xC0) - read configuration block
static bool vk_thermo_nfc_test_read_config(Iso15693_3Poller* poller, const uint8_t* uid) {
    FURI_LOG_I(TAG, "Reading NXP config with READ_CONFIG(0xC0) addr=0x37");

    BitBuffer* tx_buffer = bit_buffer_alloc(16);
    BitBuffer* rx_buffer = bit_buffer_alloc(32);
    bool success = false;

    // NXP READ_CONFIG: [FLAGS][0xC0][MANUF][UID if addressed][address][num_blocks-1]
    bit_buffer_append_byte(tx_buffer, ISO15693_FLAG_HIGH_DATA_RATE | ISO15693_FLAG_ADDRESSED);
    bit_buffer_append_byte(tx_buffer, NXP_CMD_READ_CONFIG); // 0xC0
    bit_buffer_append_byte(tx_buffer, NXP_MANUF_CODE); // 0x04

    // UID LSB-first
    for(int i = ISO15693_3_UID_SIZE - 1; i >= 0; i--) {
        bit_buffer_append_byte(tx_buffer, uid[i]);
    }

    bit_buffer_append_byte(tx_buffer, 0x37); // CONFIG block address
    bit_buffer_append_byte(tx_buffer, 0x00); // Read 1 block

    Iso15693_3Error error =
        iso15693_3_poller_send_frame(poller, tx_buffer, rx_buffer, ISO15693_3_FDT_POLL_FC);

    if(error == Iso15693_3ErrorNone) {
        size_t rx_len = bit_buffer_get_size_bytes(rx_buffer);
        FURI_LOG_I(TAG, "READ_CONFIG(0x37) success! rx_len=%zu", rx_len);
        if(rx_len > 1) {
            uint8_t flags = bit_buffer_get_byte(rx_buffer, 0);
            FURI_LOG_I(TAG, "  Response flags: 0x%02X", flags);
            if((flags & 0x01) == 0 && rx_len >= 5) {
                // Success - parse config bytes
                uint8_t cfg0 = bit_buffer_get_byte(rx_buffer, 1);
                uint8_t cfg1 = bit_buffer_get_byte(rx_buffer, 2);
                uint8_t cfg2 = bit_buffer_get_byte(rx_buffer, 3);
                FURI_LOG_I(TAG, "  CONFIG: cfg0=0x%02X cfg1=0x%02X cfg2=0x%02X", cfg0, cfg1, cfg2);
                FURI_LOG_I(TAG, "    I2C Master mode: %s", (cfg1 & 0x10) ? "yes" : "no");
                FURI_LOG_I(TAG, "    SRAM enabled: %s", (cfg1 & 0x02) ? "yes" : "no");
            }
        }
        success = true;
    } else {
        FURI_LOG_E(
            TAG, "READ_CONFIG(0x37) failed: %s (%d)", vk_thermo_nfc_error_str(error), error);
    }

    bit_buffer_free(tx_buffer);
    bit_buffer_free(rx_buffer);
    return success;
}

// Test NXP custom command (NXP_SYSTEM_INFO 0xAB) - read-only, should work on any NXP tag
static bool vk_thermo_nfc_test_nxp_command(Iso15693_3Poller* poller, const uint8_t* uid) {
    FURI_LOG_I(TAG, "Testing NXP custom command NXP_SYSTEM_INFO(0xAB)");

    BitBuffer* tx_buffer = bit_buffer_alloc(16);
    BitBuffer* rx_buffer = bit_buffer_alloc(32);
    bool success = false;

    // NXP custom command format: [FLAGS][CMD][MANUF_CODE][UID if addressed]
    // Try NON-addressed mode first (like Python reference)
    bit_buffer_append_byte(tx_buffer, ISO15693_FLAG_HIGH_DATA_RATE); // 0x02 - non-addressed
    bit_buffer_append_byte(tx_buffer, 0xAB); // NXP_CMD_SYSTEM_INFO
    bit_buffer_append_byte(tx_buffer, NXP_MANUF_CODE); // 0x04

    FURI_LOG_D(TAG, "TX NXP_SYSTEM_INFO: flags=0x02 (non-addressed), cmd=0xAB, manuf=0x04");

    Iso15693_3Error error =
        iso15693_3_poller_send_frame(poller, tx_buffer, rx_buffer, ISO15693_3_FDT_POLL_FC);

    if(error == Iso15693_3ErrorNone) {
        size_t rx_len = bit_buffer_get_size_bytes(rx_buffer);
        FURI_LOG_I(TAG, "NXP_SYSTEM_INFO (non-addressed) success! rx_len=%zu", rx_len);
        success = true;
    } else {
        FURI_LOG_W(
            TAG,
            "NXP_SYSTEM_INFO (non-addressed) failed: %s (%d)",
            vk_thermo_nfc_error_str(error),
            error);

        // Try addressed mode
        bit_buffer_reset(tx_buffer);
        bit_buffer_reset(rx_buffer);

        bit_buffer_append_byte(
            tx_buffer, ISO15693_FLAG_HIGH_DATA_RATE | ISO15693_FLAG_ADDRESSED); // 0x22
        bit_buffer_append_byte(tx_buffer, 0xAB); // NXP_CMD_SYSTEM_INFO
        bit_buffer_append_byte(tx_buffer, NXP_MANUF_CODE); // 0x04

        // UID LSB-first
        for(int i = ISO15693_3_UID_SIZE - 1; i >= 0; i--) {
            bit_buffer_append_byte(tx_buffer, uid[i]);
        }

        FURI_LOG_D(TAG, "TX NXP_SYSTEM_INFO: flags=0x22 (addressed), cmd=0xAB, manuf=0x04");

        error = iso15693_3_poller_send_frame(poller, tx_buffer, rx_buffer, ISO15693_3_FDT_POLL_FC);

        if(error == Iso15693_3ErrorNone) {
            size_t rx_len = bit_buffer_get_size_bytes(rx_buffer);
            FURI_LOG_I(TAG, "NXP_SYSTEM_INFO (addressed) success! rx_len=%zu", rx_len);
            success = true;
        } else {
            FURI_LOG_E(
                TAG,
                "NXP_SYSTEM_INFO (addressed) also failed: %s (%d)",
                vk_thermo_nfc_error_str(error),
                error);
        }
    }

    bit_buffer_free(tx_buffer);
    bit_buffer_free(rx_buffer);
    return success;
}

// Test standard ISO15693 addressed command (READ_SINGLE_BLOCK 0x20)
// This verifies our addressed mode implementation works before trying NXP commands
static bool vk_thermo_nfc_test_addressed_mode(Iso15693_3Poller* poller, const uint8_t* uid) {
    FURI_LOG_I(TAG, "Testing addressed mode with READ_SINGLE_BLOCK(0x20) block 0");

    BitBuffer* tx_buffer = bit_buffer_alloc(16);
    BitBuffer* rx_buffer = bit_buffer_alloc(16);
    bool success = false;

    // Standard ISO15693 READ_SINGLE_BLOCK: [FLAGS][0x20][UID 8 bytes][block_num]
    // No manufacturer code for standard commands
    bit_buffer_append_byte(tx_buffer, ISO15693_FLAG_HIGH_DATA_RATE | ISO15693_FLAG_ADDRESSED);
    bit_buffer_append_byte(tx_buffer, 0x20); // READ_SINGLE_BLOCK

    // UID LSB-first (reversed from Flipper storage)
    for(int i = ISO15693_3_UID_SIZE - 1; i >= 0; i--) {
        bit_buffer_append_byte(tx_buffer, uid[i]);
    }

    bit_buffer_append_byte(tx_buffer, 0x00); // Block 0

    // Log what we're sending
    FURI_LOG_D(TAG, "TX READ_SINGLE_BLOCK: flags=0x22, cmd=0x20, block=0");

    Iso15693_3Error error =
        iso15693_3_poller_send_frame(poller, tx_buffer, rx_buffer, ISO15693_3_FDT_POLL_FC);

    if(error == Iso15693_3ErrorNone) {
        size_t rx_len = bit_buffer_get_size_bytes(rx_buffer);
        FURI_LOG_I(TAG, "READ_SINGLE_BLOCK success! rx_len=%zu", rx_len);
        if(rx_len > 0) {
            uint8_t flag = bit_buffer_get_byte(rx_buffer, 0);
            FURI_LOG_D(TAG, "  Response flag: 0x%02X", flag);
            if(rx_len > 1) {
                FURI_LOG_D(TAG, "  First data byte: 0x%02X", bit_buffer_get_byte(rx_buffer, 1));
            }
        }
        success = true;
    } else {
        FURI_LOG_E(
            TAG, "READ_SINGLE_BLOCK failed: %s (%d)", vk_thermo_nfc_error_str(error), error);
    }

    bit_buffer_free(tx_buffer);
    bit_buffer_free(rx_buffer);
    return success;
}

// Send NXP WRITE_CONFIG command - NOTE: may timeout but still succeed!
// Reference: ntag5sensor says "These write commands work despite the reader claiming they timed out"
static bool vk_thermo_nfc_write_config(
    Iso15693_3Poller* poller,
    const uint8_t* uid,
    uint8_t address,
    const uint8_t* block_data) { // 4 bytes

    BitBuffer* tx_buffer = bit_buffer_alloc(20);
    BitBuffer* rx_buffer = bit_buffer_alloc(16);

    // NXP WRITE_CONFIG: [FLAGS][0xC1][MANUF][UID][address][4-byte data]
    bit_buffer_append_byte(tx_buffer, ISO15693_FLAG_HIGH_DATA_RATE | ISO15693_FLAG_ADDRESSED);
    bit_buffer_append_byte(tx_buffer, NXP_CMD_WRITE_CONFIG);
    bit_buffer_append_byte(tx_buffer, NXP_MANUF_CODE);

    // UID LSB-first
    for(int i = ISO15693_3_UID_SIZE - 1; i >= 0; i--) {
        bit_buffer_append_byte(tx_buffer, uid[i]);
    }

    bit_buffer_append_byte(tx_buffer, address);
    bit_buffer_append_bytes(tx_buffer, block_data, 4);

    FURI_LOG_I(
        TAG,
        "WRITE_CONFIG addr=0x%02X data=[0x%02X,0x%02X,0x%02X,0x%02X]",
        address,
        block_data[0],
        block_data[1],
        block_data[2],
        block_data[3]);

    Iso15693_3Error error =
        iso15693_3_poller_send_frame(poller, tx_buffer, rx_buffer, ISO15693_3_FDT_POLL_FC);

    bit_buffer_free(tx_buffer);
    bit_buffer_free(rx_buffer);

    if(error == Iso15693_3ErrorNone) {
        FURI_LOG_I(TAG, "WRITE_CONFIG got response (unusual but good)");
        return true;
    } else if(error == Iso15693_3ErrorTimeout || error == Iso15693_3ErrorNotPresent) {
        // IMPORTANT: Timeout/NotPresent is expected for writes! The write may have succeeded anyway.
        // ntag5sensor says "These write commands work despite the reader claiming they timed out"
        FURI_LOG_W(
            TAG,
            "WRITE_CONFIG %s - this may be NORMAL, write may have succeeded",
            error == Iso15693_3ErrorTimeout ? "timeout" : "tag not present");
        return true; // Assume success, caller should verify by reading back
    } else {
        FURI_LOG_E(TAG, "WRITE_CONFIG error: %s (%d)", vk_thermo_nfc_error_str(error), error);
        return false;
    }
}

// Enable energy harvesting (two-step process like ntag5sensor)
// Step 1: TRIGGER only, wait for LOAD_OK (capacitor charges)
// Step 2: TRIGGER + ENABLE (activates voltage output)
// Returns false if LOAD_OK never sets or stop was requested - caller should NOT attempt sensor read
static bool vk_thermo_nfc_enable_eh(
    VkThermoNfc* instance,
    Iso15693_3Poller* poller,
    const uint8_t* uid,
    uint32_t timeout_seconds) {
    // Step 1: Write TRIGGER only (no ENABLE yet)
    FURI_LOG_I(TAG, "EH Step 1: Triggering (charging capacitor)");
    uint8_t trigger_data[4] = {
        NXP_EH_TRIGGER, // bit3=trigger, enable=0
        0x00,
        0x00,
        0x00};

    if(!vk_thermo_nfc_write_config(poller, uid, NXP_CONFIG_ADDR_EH_CONFIG_REG, trigger_data)) {
        FURI_LOG_E(TAG, "EH trigger write failed");
        return false;
    }

    // Poll for LOAD_OK (capacitor charged)
    // timeout_seconds == 0 means indefinite (poll forever)
    bool indefinite = (timeout_seconds == 0);
    uint32_t poll_count = indefinite ? UINT32_MAX : (timeout_seconds * 10); // 100ms intervals
    FURI_LOG_I(TAG, "Waiting for EH LOAD_OK (timeout: %s)...", indefinite ? "indefinite" : "");
    if(!indefinite) {
        FURI_LOG_I(TAG, "  timeout: %lus", (unsigned long)timeout_seconds);
    }
    bool load_ok = false;
    for(uint32_t i = 0; i < poll_count; i++) {
        if(instance->stop_requested) {
            FURI_LOG_I(TAG, "EH polling aborted (stop requested)");
            return false;
        }
        furi_delay_ms(100);
        VkThermoEhStatus status = vk_thermo_nfc_check_eh_ready(poller, uid);
        if(status == VkThermoEhStatusLoadOk) {
            FURI_LOG_I(TAG, "LOAD_OK set after %lums", (unsigned long)((i + 1) * 100));
            load_ok = true;
            break;
        } else if(status == VkThermoEhStatusTagLost) {
            FURI_LOG_E(TAG, "Tag lost during EH polling");
            return false;
        }
        // VkThermoEhStatusWaiting: tag present but LOAD_OK not yet set, keep polling
    }

    if(!load_ok) {
        FURI_LOG_E(TAG, "EH LOAD_OK never set - cannot read sensor");
        return false;
    }

    // Step 2: Write TRIGGER + ENABLE (activate output)
    FURI_LOG_I(TAG, "EH Step 2: Enabling output");
    uint8_t enable_data[4] = {
        NXP_EH_TRIGGER | NXP_EH_ENABLE, // bit3=trigger, bit0=enable
        0x00,
        0x00,
        0x00};

    if(!vk_thermo_nfc_write_config(poller, uid, NXP_CONFIG_ADDR_EH_CONFIG_REG, enable_data)) {
        FURI_LOG_E(TAG, "EH enable write failed");
        return false;
    }

    // Brief delay for voltage output to stabilize
    furi_delay_ms(50);
    FURI_LOG_I(TAG, "EH enabled");
    return true;
}

// Check if energy harvesting load is OK
// Sends READ_CONFIG (0xC0) to EH_CONFIG_REG (0xA7) and checks LOAD_OK bit
// Returns VkThermoEhStatusLoadOk, VkThermoEhStatusWaiting, or VkThermoEhStatusTagLost
static VkThermoEhStatus
    vk_thermo_nfc_check_eh_ready(Iso15693_3Poller* poller, const uint8_t* uid) {
    // NTAG5Link READ_CONFIG format: [address][num_blocks-1]
    uint8_t params[2] = {
        NXP_CONFIG_ADDR_EH_CONFIG_REG, // Config block address (0xA7)
        0x00 // Read 1 block (num_blocks - 1)
    };
    uint8_t response[8];
    size_t response_len = 0;

    FURI_LOG_D(
        TAG,
        "Checking EH status: READ_CONFIG(0xC0) from addr 0x%02X",
        NXP_CONFIG_ADDR_EH_CONFIG_REG);

    if(vk_thermo_nfc_send_custom_cmd(
           poller, uid, NXP_CMD_READ_CONFIG, params, sizeof(params), response, &response_len)) {
        if(response_len >= 1) {
            bool load_ok = (response[0] & NXP_EH_LOAD_OK) != 0;
            FURI_LOG_I(TAG, "EH status: 0x%02X, LOAD_OK=%s", response[0], load_ok ? "yes" : "no");
            return load_ok ? VkThermoEhStatusLoadOk : VkThermoEhStatusWaiting;
        }
    }
    FURI_LOG_W(TAG, "Failed to read EH status - tag likely lost");
    return VkThermoEhStatusTagLost;
}

// Check if I2C bus is busy (read status register)
static bool vk_thermo_nfc_i2c_is_busy(Iso15693_3Poller* poller, const uint8_t* uid) {
    uint8_t params[2] = {NXP_CONFIG_ADDR_I2C_M_STATUS_REG, 0x00};
    uint8_t resp[8];
    size_t resp_len = 0;

    if(!vk_thermo_nfc_send_custom_cmd(
           poller, uid, NXP_CMD_READ_CONFIG, params, 2, resp, &resp_len)) {
        return false; // Can't read status, assume not busy
    }

    if(resp_len > 0) {
        bool busy = (resp[0] & NXP_I2C_M_BUSY_MASK) != 0;
        if(busy) FURI_LOG_D(TAG, "I2C bus busy (status: 0x%02X)", resp[0]);
        return busy;
    }
    return false;
}

// Check I2C write result (read status register, verify transaction succeeded)
static bool vk_thermo_nfc_i2c_check_result(Iso15693_3Poller* poller, const uint8_t* uid) {
    uint8_t params[2] = {NXP_CONFIG_ADDR_I2C_M_STATUS_REG, 0x00};
    uint8_t resp[8];
    size_t resp_len = 0;

    if(!vk_thermo_nfc_send_custom_cmd(
           poller, uid, NXP_CMD_READ_CONFIG, params, 2, resp, &resp_len)) {
        FURI_LOG_E(TAG, "Failed to read I2C status");
        return false;
    }

    if(resp_len < 1) {
        FURI_LOG_E(TAG, "I2C status: no data");
        return false;
    }

    uint8_t status = resp[0];
    uint8_t trans_status = status & NXP_I2C_M_TRANS_STATUS_MASK;

    if(trans_status == NXP_I2C_M_TRANS_STATUS_SUCCESS) {
        FURI_LOG_D(TAG, "I2C transaction success (status: 0x%02X)", status);
        return true;
    }

    FURI_LOG_E(
        TAG, "I2C transaction failed (status: 0x%02X, trans: 0x%02X)", status, trans_status);
    return false;
}

// Wait for I2C bus to be free
static bool vk_thermo_nfc_i2c_wait_ready(Iso15693_3Poller* poller, const uint8_t* uid) {
    for(int i = 0; i < 10; i++) {
        if(!vk_thermo_nfc_i2c_is_busy(poller, uid)) return true;
        furi_delay_ms(10);
    }
    FURI_LOG_W(TAG, "I2C busy timeout");
    return false;
}

// Send WRITE_I2C command (sends data to I2C slave)
static bool vk_thermo_nfc_i2c_write(
    Iso15693_3Poller* poller,
    const uint8_t* uid,
    uint8_t i2c_addr,
    const uint8_t* data,
    size_t data_len) {
    if(data_len < 1 || data_len > 8) return false;

    uint8_t params[10];
    params[0] = i2c_addr & 0x7F; // Address with stop condition (bit7=0)
    params[1] = data_len - 1; // Number of bytes - 1
    memcpy(&params[2], data, data_len);

    if(!vk_thermo_nfc_send_custom_cmd(
           poller, uid, NXP_CMD_WRITE_I2C, params, 2 + data_len, NULL, NULL)) {
        FURI_LOG_E(TAG, "WRITE_I2C send failed");
        return false;
    }

    // Verify the I2C write succeeded
    furi_delay_ms(5);
    return vk_thermo_nfc_i2c_check_result(poller, uid);
}

// Send READ_I2C command, then read result from SRAM
static bool vk_thermo_nfc_i2c_read(
    Iso15693_3Poller* poller,
    const uint8_t* uid,
    uint8_t i2c_addr,
    uint8_t num_bytes,
    uint8_t* response,
    size_t* response_len) {
    // Step 1: Send READ_I2C to tell NTAG5Link to read from I2C slave
    uint8_t params[2] = {
        i2c_addr & 0x7F, // Address with stop condition (bit7=0)
        num_bytes - 1 // Number of bytes - 1
    };

    if(!vk_thermo_nfc_send_custom_cmd(
           poller, uid, NXP_CMD_READ_I2C, params, sizeof(params), NULL, NULL)) {
        FURI_LOG_E(TAG, "READ_I2C send failed");
        return false;
    }

    // Step 2: Read data from SRAM where NTAG5Link stored the I2C response
    // SRAM is organized in 4-byte blocks
    uint8_t num_blocks = (num_bytes + 3) / 4; // ceil(num_bytes / 4)
    uint8_t sram_params[2] = {
        0x00, // SRAM start address
        num_blocks - 1 // Number of blocks - 1
    };

    uint8_t sram_data[16];
    size_t sram_len = 0;

    if(!vk_thermo_nfc_send_custom_cmd(
           poller, uid, NXP_CMD_READ_SRAM, sram_params, sizeof(sram_params), sram_data, &sram_len)) {
        FURI_LOG_E(TAG, "READ_SRAM failed");
        return false;
    }

    FURI_LOG_I(TAG, "SRAM read: %zu bytes", sram_len);
    for(size_t i = 0; i < sram_len && i < 8; i++) {
        FURI_LOG_D(TAG, "  SRAM[%zu]=0x%02X", i, sram_data[i]);
    }

    // Copy requested bytes
    if(sram_len >= num_bytes) {
        memcpy(response, sram_data, num_bytes);
        *response_len = num_bytes;
        return true;
    }

    FURI_LOG_E(TAG, "SRAM returned %zu bytes, expected %u", sram_len, num_bytes);
    return false;
}

// Identify sensor by reading TMP117 Device ID register (0x0F).
// TMP117 returns 0x0117; TMP112 only has registers 0x00-0x03 so this will NAK or return garbage.
static VkThermoSensorType
    vk_thermo_nfc_identify_sensor(Iso15693_3Poller* poller, const uint8_t* uid) {
    // Set register pointer to Device ID (0x0F)
    uint8_t reg_ptr[1] = {TMP117_REG_DEVICE_ID};
    if(!vk_thermo_nfc_i2c_write(poller, uid, TEMP_SENSOR_I2C_ADDR, reg_ptr, 1)) {
        FURI_LOG_I(TAG, "Device ID register write failed — assuming TMP112");
        return VkThermoSensorTmp112;
    }

    // Read 2 bytes
    uint8_t id_data[4];
    size_t id_len = 0;
    if(!vk_thermo_nfc_i2c_read(poller, uid, TEMP_SENSOR_I2C_ADDR, 2, id_data, &id_len)) {
        FURI_LOG_I(TAG, "Device ID register read failed — assuming TMP112");
        return VkThermoSensorTmp112;
    }

    if(id_len >= 2) {
        uint16_t device_id = (uint16_t)((id_data[0] << 8) | id_data[1]);
        FURI_LOG_I(TAG, "Device ID register: 0x%04X", device_id);
        if((device_id & TMP117_DEVICE_ID_MASK) == TMP117_DEVICE_ID) {
            FURI_LOG_I(TAG, "Sensor identified: TMP117");
            return VkThermoSensorTmp117;
        }
    }

    FURI_LOG_I(TAG, "Device ID not TMP117 — assuming TMP112");
    return VkThermoSensorTmp112;
}

// Read temperature from sensor (identifies sensor via Device ID register)
// Flow: check busy → identify sensor (read 0x0F) → read temp (0x00) → convert
static bool vk_thermo_nfc_read_temperature(
    Iso15693_3Poller* poller,
    const uint8_t* uid,
    float* temperature,
    VkThermoSensorType* sensor_type) {
    // Wait for I2C bus to be free
    if(!vk_thermo_nfc_i2c_wait_ready(poller, uid)) {
        FURI_LOG_W(TAG, "I2C bus busy, trying anyway");
    }

    // Identify sensor type by reading TMP117 Device ID register (0x0F)
    // TMP117 returns 0x0117; TMP112 has no register 0x0F so this will fail
    VkThermoSensorType sensor = vk_thermo_nfc_identify_sensor(poller, uid);

    // Set register pointer to temperature result (0x00) — same for both sensors
    uint8_t reg_ptr[1] = {TEMP_SENSOR_REG_RESULT};
    if(!vk_thermo_nfc_i2c_write(poller, uid, TEMP_SENSOR_I2C_ADDR, reg_ptr, 1)) {
        FURI_LOG_E(TAG, "Failed to set temperature register pointer");
        return false;
    }

    // Read 2 bytes of temperature data via I2C → SRAM
    uint8_t temp_data[4];
    size_t temp_len = 0;
    if(!vk_thermo_nfc_i2c_read(poller, uid, TEMP_SENSOR_I2C_ADDR, 2, temp_data, &temp_len)) {
        FURI_LOG_E(TAG, "Failed to read temperature sensor");
        return false;
    }

    if(temp_len >= 2) {
        int16_t raw = (int16_t)((temp_data[0] << 8) | temp_data[1]);
        float celsius;

        if(sensor == VkThermoSensorTmp117) {
            celsius = vk_thermo_tmp117_raw_to_celsius(raw);
        } else {
            celsius = vk_thermo_tmp112_raw_to_celsius(raw);
        }

        FURI_LOG_I(
            TAG,
            "Sensor: %s, Raw: 0x%04X, Temp: %.2f C",
            sensor == VkThermoSensorTmp117 ? "TMP117" : "TMP112",
            (uint16_t)raw,
            (double)celsius);

        *temperature = celsius;
        *sensor_type = sensor;
        return true;
    }

    FURI_LOG_E(TAG, "Insufficient temperature data: %zu bytes", temp_len);
    return false;
}

// Scanner callback - tag detected
static void vk_thermo_nfc_scanner_callback(NfcScannerEvent event, void* context) {
    furi_assert(context);
    VkThermoNfc* instance = context;

    if(event.type == NfcScannerEventTypeDetected) {
        FURI_LOG_I(TAG, "Tag detected, protocols: %zu", event.data.protocol_num);

        // Look for ISO15693
        for(size_t i = 0; i < event.data.protocol_num; i++) {
            FURI_LOG_D(TAG, "  Protocol[%zu]: %d", i, event.data.protocols[i]);
            if(event.data.protocols[i] == NfcProtocolIso15693_3) {
                instance->detected_protocol = NfcProtocolIso15693_3;
                instance->state = VkThermoNfcStateTagDetected;
                FURI_LOG_I(TAG, "ISO15693 tag found");
                return;
            }
        }

        FURI_LOG_W(TAG, "No ISO15693 protocol found");
    }
}

// Poller callback - process tag
static NfcCommand vk_thermo_nfc_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    VkThermoNfc* instance = context;

    // Once we have valid temperature data OR a terminal state, ignore all further poller events.
    // last_data.valid is the authoritative indicator — if we have a temperature, we're done.
    if(instance->last_data.valid || instance->state == VkThermoNfcStateSuccess ||
       instance->state == VkThermoNfcStateError) {
        return NfcCommandStop;
    }

    if(event.protocol != NfcProtocolIso15693_3) {
        return NfcCommandContinue;
    }

    const Iso15693_3PollerEvent* iso_event = event.event_data;

    if(iso_event->type == Iso15693_3PollerEventTypeReady) {
        Iso15693_3Poller* poller = event.instance;
        const Iso15693_3Data* iso_data = nfc_poller_get_data(instance->poller);

        if(!iso_data) {
            FURI_LOG_E(TAG, "No ISO15693 data");
            instance->state = VkThermoNfcStateError;
            return NfcCommandStop;
        }

        // Store UID
        memcpy(instance->last_data.uid, iso_data->uid, VK_THERMO_NFC_UID_LEN);

        FURI_LOG_I(
            TAG,
            "UID: %02X%02X%02X%02X%02X%02X%02X%02X",
            iso_data->uid[0],
            iso_data->uid[1],
            iso_data->uid[2],
            iso_data->uid[3],
            iso_data->uid[4],
            iso_data->uid[5],
            iso_data->uid[6],
            iso_data->uid[7]);

        // === DIAGNOSTIC TESTS (debug mode only) ===
        if(instance->debug_mode) {
            FURI_LOG_I(TAG, "=== Running diagnostic tests ===");

            // Test 1: Standard ISO15693 READ_SINGLE_BLOCK (addressed)
            if(!vk_thermo_nfc_test_addressed_mode(poller, iso_data->uid)) {
                FURI_LOG_E(TAG, "Test 1 FAILED: READ_SINGLE_BLOCK");
            } else {
                FURI_LOG_I(TAG, "Test 1 PASSED: READ_SINGLE_BLOCK");
            }

            // Test 2: Standard ISO15693 GET_SYSTEM_INFO
            if(!vk_thermo_nfc_test_get_system_info(poller, iso_data->uid)) {
                FURI_LOG_E(TAG, "Test 2 FAILED: GET_SYSTEM_INFO");
            } else {
                FURI_LOG_I(TAG, "Test 2 PASSED: GET_SYSTEM_INFO");
            }

            // Test 3: NXP SYSTEM_INFO (0xAB)
            if(!vk_thermo_nfc_test_nxp_command(poller, iso_data->uid)) {
                FURI_LOG_E(
                    TAG,
                    "Test 3 FAILED: NXP_SYSTEM_INFO - tag may not support NXP custom commands");
            } else {
                FURI_LOG_I(TAG, "Test 3 PASSED: NXP_SYSTEM_INFO");
            }

            // Test 4: NXP READ_CONFIG (0xC0) - read-only config command
            if(!vk_thermo_nfc_test_read_config(poller, iso_data->uid)) {
                FURI_LOG_E(TAG, "Test 4 FAILED: READ_CONFIG");
            } else {
                FURI_LOG_I(TAG, "Test 4 PASSED: READ_CONFIG");
            }

            FURI_LOG_I(TAG, "=== Diagnostics complete ===");
        }
        // === END DIAGNOSTICS ===

        // Enable energy harvesting (handles LOAD_OK polling internally)
        FURI_LOG_I(TAG, "Enabling energy harvesting...");
        if(!vk_thermo_nfc_enable_eh(
               instance, poller, iso_data->uid, instance->eh_timeout_seconds)) {
            FURI_LOG_E(TAG, "EH failed - sensor not powered, aborting read");
            instance->state = VkThermoNfcStateError;
            return NfcCommandStop;
        }

        // Read temperature (auto-detects TMP117 vs TMP112)
        float temperature = 0.0f;
        VkThermoSensorType sensor_type = VkThermoSensorUnknown;
        if(vk_thermo_nfc_read_temperature(poller, iso_data->uid, &temperature, &sensor_type)) {
            instance->last_data.temperature_celsius = temperature;
            instance->last_data.sensor_type = sensor_type;
            instance->last_data.valid = true;
            instance->state = VkThermoNfcStateSuccess;
            FURI_LOG_I(TAG, "Read success: %.2f C", (double)temperature);
        } else if(!instance->stop_requested) {
            // Retry with longer delay
            furi_delay_ms(VK_THERMO_EH_DELAY_MS * 2);
            if(!instance->stop_requested &&
               vk_thermo_nfc_read_temperature(poller, iso_data->uid, &temperature, &sensor_type)) {
                instance->last_data.temperature_celsius = temperature;
                instance->last_data.sensor_type = sensor_type;
                instance->last_data.valid = true;
                instance->state = VkThermoNfcStateSuccess;
                FURI_LOG_I(TAG, "Read success (retry): %.2f C", (double)temperature);
            } else {
                instance->last_data.valid = false;
                instance->state = VkThermoNfcStateError;
                FURI_LOG_E(TAG, "Failed to read temperature");
            }
        }

        return NfcCommandStop;

    } else if(iso_event->type == Iso15693_3PollerEventTypeError) {
        FURI_LOG_E(TAG, "Poller error");
        instance->state = VkThermoNfcStateError;
        return NfcCommandStop;
    }

    return NfcCommandContinue;
}

// Start poller for detected tag
static void vk_thermo_nfc_start_poller(VkThermoNfc* instance) {
    furi_assert(instance);

    // Stop scanner
    if(instance->scanner) {
        nfc_scanner_stop(instance->scanner);
        nfc_scanner_free(instance->scanner);
        instance->scanner = NULL;
    }

    // Start poller
    instance->poller = nfc_poller_alloc(instance->nfc, instance->detected_protocol);
    if(instance->poller) {
        instance->state = VkThermoNfcStatePolling;
        nfc_poller_start(instance->poller, vk_thermo_nfc_poller_callback, instance);
        FURI_LOG_I(TAG, "Poller started");
    } else {
        FURI_LOG_E(TAG, "Failed to allocate poller");
        instance->state = VkThermoNfcStateError;
    }
}

VkThermoNfc* vk_thermo_nfc_alloc(void) {
    VkThermoNfc* instance = malloc(sizeof(VkThermoNfc));

    instance->nfc = nfc_alloc();
    instance->scanner = NULL;
    instance->poller = NULL;
    instance->state = VkThermoNfcStateIdle;
    instance->detected_protocol = NfcProtocolInvalid;
    instance->callback = NULL;
    instance->callback_context = NULL;
    instance->retry_count = 0;
    instance->eh_timeout_seconds = 5; // Default 5 seconds
    instance->debug_mode = false;
    instance->stop_requested = false;

    memset(&instance->last_data, 0, sizeof(VkThermoNfcData));

    FURI_LOG_I(TAG, "NFC allocated");
    return instance;
}

void vk_thermo_nfc_free(VkThermoNfc* instance) {
    furi_assert(instance);

    vk_thermo_nfc_stop(instance);

    if(instance->nfc) {
        nfc_free(instance->nfc);
        instance->nfc = NULL;
    }

    free(instance);
    FURI_LOG_I(TAG, "NFC freed");
}

void vk_thermo_nfc_set_callback(VkThermoNfc* instance, VkThermoNfcCallback callback, void* context) {
    furi_assert(instance);
    instance->callback = callback;
    instance->callback_context = context;
}

void vk_thermo_nfc_set_eh_timeout(VkThermoNfc* instance, uint32_t timeout_seconds) {
    furi_assert(instance);
    instance->eh_timeout_seconds = timeout_seconds;
    FURI_LOG_I(TAG, "EH timeout set to %lus", (unsigned long)timeout_seconds);
}

void vk_thermo_nfc_set_debug(VkThermoNfc* instance, bool enabled) {
    furi_assert(instance);
    instance->debug_mode = enabled;
    FURI_LOG_I(TAG, "Debug mode %s", enabled ? "ON" : "OFF");
}

void vk_thermo_nfc_start(VkThermoNfc* instance) {
    furi_assert(instance);

    if(instance->state != VkThermoNfcStateIdle) {
        FURI_LOG_W(TAG, "Already scanning");
        return;
    }

    // Cleanup any stale state
    if(instance->poller) {
        nfc_poller_stop(instance->poller);
        nfc_poller_free(instance->poller);
        instance->poller = NULL;
    }
    if(instance->scanner) {
        nfc_scanner_stop(instance->scanner);
        nfc_scanner_free(instance->scanner);
        instance->scanner = NULL;
    }

    // Reset state
    instance->stop_requested = false;
    instance->detected_protocol = NfcProtocolInvalid;
    instance->retry_count = 0;
    memset(&instance->last_data, 0, sizeof(VkThermoNfcData));

    // Start scanner
    instance->scanner = nfc_scanner_alloc(instance->nfc);
    if(!instance->scanner) {
        FURI_LOG_E(TAG, "Failed to allocate scanner");
        return;
    }

    nfc_scanner_start(instance->scanner, vk_thermo_nfc_scanner_callback, instance);
    instance->state = VkThermoNfcStateScanning;
    FURI_LOG_I(TAG, "Scanning started");
}

void vk_thermo_nfc_stop(VkThermoNfc* instance) {
    furi_assert(instance);

    // Signal poller callback to abort any blocking loops (e.g., EH polling)
    // Must be set BEFORE nfc_poller_stop() which blocks until callback returns
    instance->stop_requested = true;

    if(instance->poller) {
        nfc_poller_stop(instance->poller);
        nfc_poller_free(instance->poller);
        instance->poller = NULL;
    }

    if(instance->scanner) {
        nfc_scanner_stop(instance->scanner);
        nfc_scanner_free(instance->scanner);
        instance->scanner = NULL;
    }

    instance->state = VkThermoNfcStateIdle;
    instance->detected_protocol = NfcProtocolInvalid;
    FURI_LOG_I(TAG, "Stopped");
}

bool vk_thermo_nfc_is_scanning(VkThermoNfc* instance) {
    furi_assert(instance);
    return instance->state != VkThermoNfcStateIdle;
}

bool vk_thermo_nfc_tick(VkThermoNfc* instance) {
    furi_assert(instance);

    switch(instance->state) {
    case VkThermoNfcStateTagDetected:
        // Tag found, start poller
        FURI_LOG_I(TAG, "Tick: tag detected, starting poller");

        // Notify callback that tag was detected (for user feedback)
        if(instance->callback) {
            instance->callback(
                VkThermoNfcEventTagDetected, &instance->last_data, instance->callback_context);
        }

        vk_thermo_nfc_start_poller(instance);
        break;

    case VkThermoNfcStateSuccess:
    case VkThermoNfcStateError: {
        // NFC operation reached a terminal state.
        // Stop poller FIRST — this blocks until the callback has fully returned,
        // guaranteeing all writes (especially last_data.valid) are committed.
        if(instance->poller) {
            nfc_poller_stop(instance->poller);
            nfc_poller_free(instance->poller);
            instance->poller = NULL;
        }

        // NOW read valid — poller is fully stopped, all callback writes are visible.
        // Use last_data.valid as the AUTHORITATIVE indicator of success/failure.
        // If we have valid temperature data, this is ALWAYS a success — regardless of
        // whether the NFC state ended up as Error (e.g., tag lost after temp was read).
        bool have_temperature = instance->last_data.valid;
        VkThermoNfcEvent result = have_temperature ? VkThermoNfcEventSuccess :
                                                     VkThermoNfcEventError;

        FURI_LOG_I(
            TAG,
            "Tick: nfc_state=%s, data_valid=%s -> reporting %s",
            instance->state == VkThermoNfcStateSuccess ? "Success" : "Error",
            have_temperature ? "yes" : "no",
            have_temperature ? "SUCCESS" : "ERROR");

        // Reset to idle
        instance->state = VkThermoNfcStateIdle;
        instance->detected_protocol = NfcProtocolInvalid;

        // Notify callback
        if(instance->callback) {
            instance->callback(result, &instance->last_data, instance->callback_context);
        }
        return have_temperature;
    }

    default:
        break;
    }

    return false;
}
