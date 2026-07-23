#include <furi.h>
#include <furi_hal.h>
#include <cli/cli_vcp.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <gui/view_port.h>
#include <storage/storage.h>
#include <targets/f7/furi_hal/furi_hal_usb_cdc.h>
#include <stm32wbxx_ll_i2c.h>

#include <stdio.h>
#include <string.h>

#include "postcode_db.h"

#define TAG "XboxPostReader"

#define MAX6958_ADDRESS              0x38U
#define MAX6958_REGISTER_COUNT       0x25U
#define MAX6958_REG_FACTORY_RESERVED 0x05U
#define MAX6958_REG_DIGIT0           0x20U
#define MAX6958_REG_DIGIT1           0x21U
#define MAX6958_REG_DIGIT2           0x22U
#define MAX6958_REG_DIGIT3           0x23U
#define MAX6958_REG_SEGMENTS         0x24U

#define POST_HISTORY_SIZE     128U
#define INPUT_QUEUE_SIZE      16U
#define POST_QUEUE_SIZE       128U
#define SERIAL_QUEUE_SIZE     64U
#define MENU_ITEM_COUNT       6U
#define MENU_VISIBLE_ROWS     5U
#define KNOWN_LIST_ROWS       3U
#define SERIAL_VCP_CHANNEL    0U
#define SAVED_LOG_ROWS        4U
#define SAVED_LOG_BUFFER_SIZE 4096U
#define SAVED_LOG_MAX_NUMBER  999U
#define SAVED_LOG_DIRECTORY   APP_DATA_PATH("logs")

typedef struct {
    uint16_t code;
    uint8_t segment;
    uint32_t elapsed_ms;
} PostEntry;

typedef struct {
    FuriMessageQueue* queue;
    FuriThread* thread;
    FuriSemaphore* tx_sem;
    FuriMutex* usb_mutex;
    CliVcp* cli_vcp;
    volatile bool running;
    volatile bool usb_connected;
    volatile bool host_ready;
    volatile bool announce_pending;
    volatile bool rx_pending;
    volatile PostcodeConsole console;
    uint32_t dropped_events;
} XboxPostSerial;

typedef struct {
    FuriMessageQueue* post_queue;
    FuriThread* thread;
    volatile bool running;
    volatile uint32_t dropped_events;
    uint8_t registers[MAX6958_REGISTER_COUNT];
    int16_t current_register;
    uint32_t started_at;
} XboxPostReader;

typedef enum {
    AppPageMenu,
    AppPageReader,
    AppPageKnownCodes,
    AppPageKnownDetail,
    AppPageSavedLogs,
    AppPageSavedLogView,
    AppPageWiring,
    AppPageWeb,
    AppPageAbout,
} AppPage;

typedef enum {
    ReaderViewSingle,
    ReaderViewLog,
    ReaderViewDecode,
} ReaderView;

typedef enum {
    KnownCategoryAll,
    KnownCategoryPost,
    KnownCategoryError,
    KnownCategorySmc,
    KnownCategorySp,
    KnownCategoryCpu,
    KnownCategoryOs,
    KnownCategoryCount,
} KnownCategory;

typedef struct {
    FuriMessageQueue* input_queue;
    FuriMessageQueue* post_queue;
    ViewPort* view_port;
    Gui* gui;
    Storage* storage;
    XboxPostReader reader;
    XboxPostSerial serial;
    AppPage page;
    ReaderView reader_view;
    ReaderView decode_return_view;
    uint8_t menu_index;
    PostcodeConsole console;
    uint8_t decode_scroll;
    bool listening;
    bool confirm_clear;
    bool exit_requested;
    PostEntry history[POST_HISTORY_SIZE];
    uint8_t history_count;
    uint8_t history_head;
    uint8_t history_offset;
    uint16_t known_index;
    uint8_t known_scroll;
    KnownCategory known_category;
    uint16_t saved_log_count;
    uint16_t saved_log_numbers[SAVED_LOG_MAX_NUMBER];
    uint16_t saved_log_index;
    uint16_t saved_log_scroll;
    uint16_t saved_log_line_count;
    char saved_log_buffer[SAVED_LOG_BUFFER_SIZE];
    char save_notice_text[32];
    bool save_notice;
} XboxPostApp;

static const char* const menu_items[] = {
    "Reader",
    "Known Codes",
    "Saved Logs",
    "Wiring",
    "Web Decoder",
    "About",
};

static const char* const known_category_names[] = {
    "ALL",
    "POST",
    "ERROR",
    "SMC",
    "SP",
    "CPU",
    "OS",
};

/* QR version 3-L for https://demo.coolshrimpmodz.com/WebXboxPOSTTool/ */
#define WEB_QR_SIZE      29U
#define WEB_QR_ROW_BYTES 4U
static const uint8_t web_qr[WEB_QR_SIZE * WEB_QR_ROW_BYTES] = {
    0x7F, 0xD2, 0xC8, 0x1F, 0x41, 0xB9, 0x53, 0x10, 0x5D, 0x1F, 0x57, 0x17, 0x5D, 0x4E, 0x45,
    0x17, 0x5D, 0x41, 0x47, 0x17, 0x41, 0x8B, 0x4C, 0x10, 0x7F, 0x55, 0xD5, 0x1F, 0x00, 0x53,
    0x07, 0x00, 0xCB, 0x74, 0xC9, 0x0D, 0x9F, 0x4C, 0xF1, 0x12, 0xD8, 0x66, 0xFB, 0x0E, 0x05,
    0xF9, 0xF5, 0x0D, 0x79, 0x18, 0x63, 0x1A, 0x31, 0x45, 0x3A, 0x00, 0xFE, 0x1C, 0x42, 0x1F,
    0x9F, 0x3B, 0x4B, 0x0B, 0x5B, 0x3F, 0x33, 0x08, 0xBE, 0xDD, 0xE7, 0x12, 0x65, 0x22, 0xA6,
    0x19, 0x90, 0x53, 0x0A, 0x18, 0x4D, 0xD0, 0xF5, 0x05, 0x00, 0x71, 0x10, 0x1D, 0x7F, 0x43,
    0x53, 0x09, 0x41, 0xB8, 0x11, 0x0F, 0x5D, 0x9E, 0xF3, 0x11, 0x5D, 0x57, 0xC2, 0x07, 0x5D,
    0x5C, 0x1C, 0x17, 0x41, 0x3B, 0xDF, 0x08, 0x7F, 0xA3, 0xF2, 0x0B,
};

static const char* post_flavor(uint8_t segment) {
    switch(segment & 0xF0U) {
    case 0x10:
        return "CPU";
    case 0x30:
        return "SP";
    case 0x70:
        return "SMC";
    case 0xF0:
        return "OS";
    default:
        return "Unknown";
    }
}

static bool post_serial_send_line(XboxPostSerial* serial, const char* line) {
    if(!serial->host_ready || !line || !line[0]) return false;
    if(furi_semaphore_acquire(serial->tx_sem, 100) != FuriStatusOk) {
        serial->dropped_events++;
        return false;
    }

    const size_t length = strlen(line);
    furi_check(furi_mutex_acquire(serial->usb_mutex, FuriWaitForever) == FuriStatusOk);
    furi_hal_cdc_send(SERIAL_VCP_CHANNEL, (uint8_t*)line, (uint16_t)length);
    furi_check(furi_mutex_release(serial->usb_mutex) == FuriStatusOk);
    return true;
}

static void post_serial_send_identity(XboxPostSerial* serial) {
    char line[64];
    post_serial_send_line(serial, "DEVICE: Xbox POST Code Reader\r\n");
    post_serial_send_line(serial, "FW: Xbox POST Code Reader v0.14\r\n");
    snprintf(
        line,
        sizeof(line),
        "CONFIG: console=%s database=%u mode=POST\r\n",
        postcode_db_console_short(serial->console),
        (unsigned int)POSTCODE_DB_RECORD_COUNT);
    post_serial_send_line(serial, line);
}

static void post_serial_process_rx(XboxPostSerial* serial) {
    uint8_t input[CDC_DATA_SZ + 1U];
    serial->rx_pending = false;
    furi_check(furi_mutex_acquire(serial->usb_mutex, FuriWaitForever) == FuriStatusOk);
    const int32_t length = furi_hal_cdc_receive(SERIAL_VCP_CHANNEL, input, CDC_DATA_SZ);
    furi_check(furi_mutex_release(serial->usb_mutex) == FuriStatusOk);
    if(length <= 0) return;

    input[length] = '\0';
    if(strstr((char*)input, "hello")) {
        post_serial_send_identity(serial);
    }
    if(strstr((char*)input, "version")) {
        post_serial_send_line(serial, "FW: Xbox POST Code Reader v0.14\r\n");
    }
    if(strstr((char*)input, "config")) {
        char line[64];
        snprintf(
            line,
            sizeof(line),
            "CONFIG: console=%s database=%u mode=POST\r\n",
            postcode_db_console_short(serial->console),
            (unsigned int)POSTCODE_DB_RECORD_COUNT);
        post_serial_send_line(serial, line);
    }
}

static int32_t post_serial_worker(void* context) {
    XboxPostSerial* serial = context;
    PostEntry entry;
    char line[64];

    while(serial->running) {
        if(serial->rx_pending) post_serial_process_rx(serial);
        if(furi_message_queue_get(serial->queue, &entry, 20) == FuriStatusOk &&
           serial->host_ready) {
            snprintf(
                line,
                sizeof(line),
                "%s (%u): 0x%04X (%lu ms)\r\n",
                post_flavor(entry.segment),
                entry.segment & 0x0FU,
                entry.code,
                (unsigned long)entry.elapsed_ms);
            post_serial_send_line(serial, line);
        }
    }
    return 0;
}

static void post_serial_tx_complete(void* context) {
    XboxPostSerial* serial = context;
    furi_semaphore_release(serial->tx_sem);
}

static void post_serial_rx_ready(void* context) {
    XboxPostSerial* serial = context;
    serial->rx_pending = true;
}

static void post_serial_state_changed(void* context, CdcState state) {
    XboxPostSerial* serial = context;
    serial->usb_connected = state == CdcStateConnected;
    if(!serial->usb_connected) {
        serial->host_ready = false;
        furi_semaphore_release(serial->tx_sem);
    }
}

static void post_serial_ctrl_line_changed(void* context, CdcCtrlLine ctrl_lines) {
    XboxPostSerial* serial = context;
    const bool ready = (ctrl_lines & CdcCtrlLineDTR) != 0;
    /* Do not transmit an unsolicited greeting here. uFBT still owns the CDC
       channel for a brief moment after launching the FAP and interprets app
       text as a malformed RPC response. Hosts can request the same identity
       safely by sending "hello" after they connect. */
    serial->announce_pending = false;
    serial->host_ready = ready;
}

static const CdcCallbacks post_serial_callbacks = {
    .tx_ep_callback = post_serial_tx_complete,
    .rx_ep_callback = post_serial_rx_ready,
    .state_callback = post_serial_state_changed,
    .ctrl_line_callback = post_serial_ctrl_line_changed,
    .config_callback = NULL,
};

static bool post_serial_start(XboxPostSerial* serial) {
    memset(serial, 0, sizeof(XboxPostSerial));
    serial->console = PostcodeConsoleXboxOneS;
    serial->queue = furi_message_queue_alloc(SERIAL_QUEUE_SIZE, sizeof(PostEntry));
    serial->tx_sem = furi_semaphore_alloc(1U, 1U);
    serial->usb_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    serial->thread = furi_thread_alloc_ex("XboxPostUsb", 1024U, post_serial_worker, serial);
    serial->cli_vcp = furi_record_open(RECORD_CLI_VCP);

    furi_hal_usb_unlock();
    cli_vcp_disable(serial->cli_vcp);
    if(!furi_hal_usb_set_config(&usb_cdc_single, NULL)) {
        cli_vcp_enable(serial->cli_vcp);
        furi_record_close(RECORD_CLI_VCP);
        furi_thread_free(serial->thread);
        furi_mutex_free(serial->usb_mutex);
        furi_semaphore_free(serial->tx_sem);
        furi_message_queue_free(serial->queue);
        memset(serial, 0, sizeof(XboxPostSerial));
        return false;
    }

    furi_hal_cdc_set_callbacks(SERIAL_VCP_CHANNEL, (CdcCallbacks*)&post_serial_callbacks, serial);
    serial->running = true;
    furi_thread_start(serial->thread);
    return true;
}

static void post_serial_stop(XboxPostSerial* serial) {
    if(!serial->thread) return;
    serial->running = false;
    furi_thread_join(serial->thread);
    furi_hal_cdc_set_callbacks(SERIAL_VCP_CHANNEL, NULL, NULL);
    furi_hal_usb_unlock();
    furi_check(furi_hal_usb_set_config(&usb_cdc_single, NULL));
    cli_vcp_enable(serial->cli_vcp);
    furi_record_close(RECORD_CLI_VCP);

    furi_thread_free(serial->thread);
    furi_mutex_free(serial->usb_mutex);
    furi_semaphore_free(serial->tx_sem);
    furi_message_queue_free(serial->queue);
    memset(serial, 0, sizeof(XboxPostSerial));
}

static void post_serial_enqueue(XboxPostSerial* serial, const PostEntry* entry) {
    if(!serial->queue || furi_message_queue_put(serial->queue, entry, 0) != FuriStatusOk) {
        serial->dropped_events++;
    }
}

static uint16_t reader_registers_to_code(const XboxPostReader* reader) {
    uint16_t code = 0;
    code |= reader->registers[MAX6958_REG_DIGIT0] & 0x0FU;
    code |= (uint16_t)(reader->registers[MAX6958_REG_DIGIT1] & 0x0FU) << 4;
    code |= (uint16_t)(reader->registers[MAX6958_REG_DIGIT2] & 0x0FU) << 8;
    code |= (uint16_t)(reader->registers[MAX6958_REG_DIGIT3] & 0x0FU) << 12;
    return code;
}

static void reader_publish_code(XboxPostReader* reader, uint8_t segment) {
    if((segment & 0x0FU) == 0U) return;

    PostEntry entry = {
        .code = reader_registers_to_code(reader),
        .segment = segment,
        .elapsed_ms = furi_get_tick() - reader->started_at,
    };

    if(furi_message_queue_put(reader->post_queue, &entry, 0) != FuriStatusOk) {
        reader->dropped_events++;
    }
}

static void reader_parse_byte(XboxPostReader* reader, uint8_t byte) {
    if((reader->current_register < 0) ||
       (reader->current_register == MAX6958_REG_FACTORY_RESERVED) ||
       (reader->current_register >= (int16_t)MAX6958_REGISTER_COUNT)) {
        reader->current_register = byte;
        return;
    }

    reader->registers[reader->current_register] = byte;
    if(reader->current_register == MAX6958_REG_SEGMENTS) {
        reader_publish_code(reader, byte);
    }
    reader->current_register++;
}

static void reader_service_i2c(XboxPostReader* reader) {
    if(LL_I2C_IsActiveFlag_ADDR(I2C3)) {
        reader->current_register = -1;
        LL_I2C_ClearFlag_ADDR(I2C3);
    }

    while(LL_I2C_IsActiveFlag_RXNE(I2C3)) {
        reader_parse_byte(reader, LL_I2C_ReceiveData8(I2C3));
    }

    /* The Xbox only writes to this emulated display. Return zero safely if a
       diagnostic master unexpectedly attempts a read. */
    if(LL_I2C_IsActiveFlag_TXIS(I2C3)) {
        LL_I2C_TransmitData8(I2C3, 0);
    }

    if(LL_I2C_IsActiveFlag_STOP(I2C3)) {
        LL_I2C_ClearFlag_STOP(I2C3);
        reader->current_register = -1;
    }
    if(LL_I2C_IsActiveFlag_NACK(I2C3)) LL_I2C_ClearFlag_NACK(I2C3);
    if(LL_I2C_IsActiveFlag_BERR(I2C3)) {
        LL_I2C_ClearFlag_BERR(I2C3);
        reader->current_register = -1;
    }
    if(LL_I2C_IsActiveFlag_ARLO(I2C3)) {
        LL_I2C_ClearFlag_ARLO(I2C3);
        reader->current_register = -1;
    }
    if(LL_I2C_IsActiveFlag_OVR(I2C3)) {
        LL_I2C_ClearFlag_OVR(I2C3);
        reader->current_register = -1;
    }
}

static int32_t reader_thread_callback(void* context) {
    XboxPostReader* reader = context;

    while(reader->running) {
        reader_service_i2c(reader);
        /* Always yield after servicing hardware. The worker runs at low
           priority, while input and rendering must remain responsive. */
        furi_thread_yield();
    }

    return 0;
}

static void reader_start(XboxPostReader* reader, FuriMessageQueue* post_queue) {
    if(reader->running) return;

    memset(reader, 0, sizeof(XboxPostReader));
    reader->post_queue = post_queue;
    reader->current_register = -1;
    reader->started_at = furi_get_tick();

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    LL_I2C_Disable(I2C3);
    LL_I2C_DisableOwnAddress1(I2C3);
    LL_I2C_SetOwnAddress1(I2C3, MAX6958_ADDRESS << 1, LL_I2C_OWNADDRESS1_7BIT);
    LL_I2C_EnableOwnAddress1(I2C3);
    LL_I2C_DisableOwnAddress2(I2C3);
    LL_I2C_DisableGeneralCall(I2C3);
    LL_I2C_EnableClockStretching(I2C3);
    LL_I2C_AcknowledgeNextData(I2C3, LL_I2C_ACK);
    LL_I2C_DisableIT_TX(I2C3);
    LL_I2C_DisableIT_RX(I2C3);
    LL_I2C_DisableIT_ADDR(I2C3);
    LL_I2C_DisableIT_NACK(I2C3);
    LL_I2C_DisableIT_STOP(I2C3);
    LL_I2C_DisableIT_ERR(I2C3);
    I2C3->ICR = I2C_ICR_ADDRCF | I2C_ICR_NACKCF | I2C_ICR_STOPCF | I2C_ICR_BERRCF |
                I2C_ICR_ARLOCF | I2C_ICR_OVRCF;
    LL_I2C_Enable(I2C3);

    reader->running = true;
    reader->thread = furi_thread_alloc_ex("XboxPostI2C", 2048, reader_thread_callback, reader);
    furi_thread_set_priority(reader->thread, FuriThreadPriorityLow);
    furi_thread_start(reader->thread);
    FURI_LOG_I(TAG, "Listening as MAX6958 at I2C address 0x38");
}

static void reader_stop(XboxPostReader* reader) {
    if(!reader->running) return;

    reader->running = false;
    furi_thread_join(reader->thread);
    furi_thread_free(reader->thread);
    reader->thread = NULL;

    LL_I2C_Disable(I2C3);
    LL_I2C_DisableOwnAddress1(I2C3);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);
    FURI_LOG_I(TAG, "Capture stopped");
}

static void draw_header(Canvas* canvas, const char* title) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 13);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 3, 11, title);
    canvas_set_color(canvas, ColorBlack);
}

static void draw_menu(Canvas* canvas, const XboxPostApp* app) {
    draw_header(canvas, "Xbox POST Reader");
    canvas_set_font(canvas, FontSecondary);

    const uint8_t first =
        app->menu_index >= MENU_VISIBLE_ROWS ? MENU_ITEM_COUNT - MENU_VISIBLE_ROWS : 0U;
    for(uint8_t row = 0; row < MENU_VISIBLE_ROWS; row++) {
        const uint8_t i = first + row;
        const int32_t y = 14 + (row * 10);
        if(i == app->menu_index) {
            canvas_draw_rbox(canvas, 2, y, 124, 10, 2);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 8, y + 8, menu_items[i]);
        if(i == app->menu_index) canvas_set_color(canvas, ColorBlack);
    }
    elements_scrollbar(canvas, app->menu_index, MENU_ITEM_COUNT);
}

static const PostEntry* app_selected_entry(const XboxPostApp* app) {
    if(app->history_count == 0U) return NULL;
    const uint8_t offset = app->history_offset < app->history_count ? app->history_offset :
                                                                      app->history_count - 1U;
    const uint8_t index =
        (uint8_t)((app->history_head + POST_HISTORY_SIZE - 1U - offset) % POST_HISTORY_SIZE);
    return &app->history[index];
}

static void draw_reader_single(Canvas* canvas, const XboxPostApp* app) {
    char text[40];
    draw_header(canvas, app->listening ? "Reader: LIVE" : "Reader: PAUSED");
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 94, 11, "^Save");
    canvas_set_color(canvas, ColorBlack);

    const PostEntry* entry = app_selected_entry(app);
    if(!entry) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 31, AlignCenter, AlignCenter, "Waiting for POST...");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignCenter, "I2C 0x38  SDA15 SCL16");
    } else {
        snprintf(text, sizeof(text), "%04X", entry->code);
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, text);

        canvas_set_font(canvas, FontSecondary);
        snprintf(
            text,
            sizeof(text),
            "%s  seg %u  %lums",
            post_flavor(entry->segment),
            entry->segment & 0x0FU,
            (unsigned long)entry->elapsed_ms);
        canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignCenter, text);
        elements_scrollbar(canvas, app->history_offset, app->history_count);
    }

    elements_button_left(canvas, "Log");
    elements_button_center(canvas, app->listening ? "Stop" : "Start");
    if(entry) elements_button_right(canvas, "Decode");
}

static void draw_reader_log(Canvas* canvas, const XboxPostApp* app) {
    char text[32];
    draw_header(canvas, app->listening ? "Log: LIVE" : "Log: PAUSED");
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 94, 11, "^Save");
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    if(app->history_count == 0U) {
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignCenter, "Waiting for codes...");
    } else {
        for(uint8_t row = 0; row < 4U; row++) {
            const uint16_t offset = app->history_offset + (3U - row);
            if(offset >= app->history_count) continue;

            const uint8_t index = (uint8_t)((app->history_head + POST_HISTORY_SIZE - 1U - offset) %
                                            POST_HISTORY_SIZE);
            const PostEntry* entry = &app->history[index];
            const int32_t y = 14 + (row * 10);

            if(offset == app->history_offset) {
                canvas_draw_rbox(canvas, 1, y, 125, 10, 1);
                canvas_set_color(canvas, ColorWhite);
            }
            snprintf(
                text,
                sizeof(text),
                "%04X  %-3s  %7lums",
                entry->code,
                post_flavor(entry->segment),
                (unsigned long)entry->elapsed_ms);
            canvas_draw_str(canvas, 4, y + 8, text);
            if(offset == app->history_offset) canvas_set_color(canvas, ColorBlack);
        }
        elements_scrollbar(canvas, app->history_offset, app->history_count);
    }

    elements_button_left(canvas, "Clear");
    elements_button_center(canvas, app->listening ? "Stop" : "Start");
    if(app->history_count > 0U) elements_button_right(canvas, "Decode");
}

static size_t find_identifier_wrap(const char* text, size_t remaining, size_t limit) {
    if(remaining <= limit) return remaining;

    size_t best = limit;
    for(size_t i = 8U; i < limit; i++) {
        if(text[i] == '_' || text[i] == ' ') {
            best = i;
        } else if(
            i > 0U && text[i] >= 'A' && text[i] <= 'Z' && text[i - 1U] >= 'a' &&
            text[i - 1U] <= 'z') {
            best = i;
        }
    }
    return best;
}

static void draw_wrapped_identifier(Canvas* canvas, const char* text) {
    const size_t max_chars = 21U;
    const uint8_t max_lines = 2U;
    size_t cursor = 0U;
    const size_t length = strlen(text);
    char line[max_chars + 1U];

    canvas_set_font(canvas, FontSecondary);
    for(uint8_t row = 0; row < max_lines && cursor < length; row++) {
        while(cursor < length && (text[cursor] == '_' || text[cursor] == ' '))
            cursor++;
        const size_t remaining = length - cursor;
        size_t take = find_identifier_wrap(&text[cursor], remaining, max_chars);
        if(take == 0U) take = remaining < max_chars ? remaining : max_chars;

        memcpy(line, &text[cursor], take);
        line[take] = '\0';
        cursor += take;

        if(row == max_lines - 1U && cursor < length && take >= 3U) {
            line[take - 3U] = '.';
            line[take - 2U] = '.';
            line[take - 1U] = '.';
        }
        canvas_draw_str(canvas, 3, 32 + (row * 10), line);
    }
}

static bool wrap_next_line(
    const char* text,
    size_t* cursor,
    char* line,
    size_t line_size,
    size_t max_chars) {
    const size_t length = strlen(text);
    while(*cursor < length && (text[*cursor] == ' ' || text[*cursor] == '\n'))
        (*cursor)++;
    if(*cursor >= length) return false;

    const size_t remaining = length - *cursor;
    size_t take = remaining < max_chars ? remaining : max_chars;
    if(remaining > max_chars) {
        size_t best = 0;
        for(size_t i = 1; i <= max_chars; i++) {
            if(text[*cursor + i] == ' ' || text[*cursor + i] == '\n') best = i;
        }
        if(best > 0U) take = best;
    }
    if(take >= line_size) take = line_size - 1U;
    memcpy(line, &text[*cursor], take);
    line[take] = '\0';
    *cursor += take;
    return true;
}

static uint8_t wrapped_line_count(const char* text, size_t max_chars) {
    if(!text || !text[0]) return 0U;
    size_t cursor = 0U;
    uint8_t count = 0U;
    char line[24];
    while(wrap_next_line(text, &cursor, line, sizeof(line), max_chars) && count < UINT8_MAX)
        count++;
    return count;
}

static void draw_description_lines(
    Canvas* canvas,
    const char* text,
    uint8_t skip,
    uint8_t max_lines,
    int32_t first_y,
    uint8_t line_spacing) {
    size_t cursor = 0U;
    uint8_t line_index = 0U;
    uint8_t drawn = 0U;
    char line[24];

    while(wrap_next_line(text, &cursor, line, sizeof(line), 21U)) {
        if(line_index++ < skip) continue;
        canvas_draw_str(canvas, 3, first_y + (drawn * line_spacing), line);
        if(++drawn >= max_lines) break;
    }
}

static void draw_reader_decode(Canvas* canvas, const XboxPostApp* app) {
    char title[24];
    char meta[32];
    char name[160];
    const char* description = "";
    bool is_error = false;
    const PostEntry* entry = app_selected_entry(app);

    if(!entry) {
        draw_header(canvas, "Decode");
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, "No code selected");
        return;
    }

    snprintf(title, sizeof(title), "Decode: %04X", entry->code);
    draw_header(canvas, title);
    const bool known = postcode_db_format(
        entry->code, entry->segment, app->console, name, sizeof(name), &description, &is_error);

    canvas_set_font(canvas, FontSecondary);
    snprintf(
        meta,
        sizeof(meta),
        "%s | %s | %s",
        postcode_db_console_short(app->console),
        post_flavor(entry->segment),
        is_error ? "ERROR" : (known ? "KNOWN" : "UNKNOWN"));
    canvas_draw_str(canvas, 3, 22, meta);

    /* Keep the code name stable while Up/Down scrolls long repair notes. */
    draw_wrapped_identifier(canvas, name);
    canvas_draw_line(canvas, 2, 43, 125, 43);

    const char* detail = description && description[0] ? description : "No repair notes yet.";
    draw_description_lines(canvas, detail, app->decode_scroll, 2U, 52, 10U);
    const uint8_t detail_lines = wrapped_line_count(detail, 21U);
    if(detail_lines > 2U) {
        elements_scrollbar(canvas, app->decode_scroll, detail_lines - 1U);
    }
}

static void draw_reader(Canvas* canvas, const XboxPostApp* app) {
    switch(app->reader_view) {
    case ReaderViewSingle:
        draw_reader_single(canvas, app);
        break;
    case ReaderViewLog:
        draw_reader_log(canvas, app);
        break;
    case ReaderViewDecode:
        draw_reader_decode(canvas, app);
        break;
    }

    if(app->confirm_clear) {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 13, 19, 102, 32);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rframe(canvas, 13, 19, 102, 32, 3);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 31, AlignCenter, AlignCenter, "Clear history?");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 43, AlignCenter, AlignCenter, "OK Yes    Back No");
    } else if(app->save_notice) {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 10, 19, 108, 32);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rframe(canvas, 10, 19, 108, 32, 3);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 31, AlignCenter, AlignCenter, "SD Log");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 43, AlignCenter, AlignCenter, app->save_notice_text);
    }
}

static void format_console_mask(uint8_t mask, char* output, size_t output_size) {
    if(mask == 0x01U) {
        snprintf(output, output_size, "ONE");
    } else if(mask == 0x02U) {
        snprintf(output, output_size, "ONE S");
    } else if(mask == 0x04U) {
        snprintf(output, output_size, "ONE X");
    } else if(mask == 0x18U) {
        snprintf(output, output_size, "SERIES S/X");
    } else if(mask == 0x07U) {
        snprintf(output, output_size, "ONE FAMILY");
    } else {
        snprintf(output, output_size, "ALL XBOX");
    }
}

static bool known_category_matches(const PostcodeDbRecord* record, KnownCategory category) {
    const char* type = postcode_db_type_short(record->type);
    switch(category) {
    case KnownCategoryPost:
        return !record->is_error;
    case KnownCategoryError:
        return record->is_error;
    case KnownCategorySmc:
        return strcmp(type, "SMC") == 0;
    case KnownCategorySp:
        return strcmp(type, "SP") == 0;
    case KnownCategoryCpu:
        return strcmp(type, "CPU") == 0;
    case KnownCategoryOs:
        return strcmp(type, "OS") == 0;
    case KnownCategoryAll:
    default:
        return true;
    }
}

static uint16_t known_category_count(KnownCategory category) {
    uint16_t count = 0U;
    PostcodeDbRecord record;
    for(uint16_t index = 0U; index < POSTCODE_DB_RECORD_COUNT; index++) {
        if(postcode_db_record_at(index, &record) && known_category_matches(&record, category)) {
            count++;
        }
    }
    return count;
}

static bool
    known_category_record_at(KnownCategory category, uint16_t position, PostcodeDbRecord* output) {
    uint16_t matched = 0U;
    PostcodeDbRecord record;
    for(uint16_t index = 0U; index < POSTCODE_DB_RECORD_COUNT; index++) {
        if(!postcode_db_record_at(index, &record) || !known_category_matches(&record, category)) {
            continue;
        }
        if(matched++ == position) {
            *output = record;
            return true;
        }
    }
    return false;
}

static void draw_known_codes(Canvas* canvas, const XboxPostApp* app) {
    char category_text[24];
    char row_text[32];
    const uint16_t record_count = known_category_count(app->known_category);
    draw_header(canvas, "Known Codes");
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 91, 11, "OK Open");
    canvas_set_color(canvas, ColorBlack);

    canvas_draw_rbox(canvas, 2, 14, 14, 11, 2);
    canvas_draw_rframe(canvas, 18, 14, 92, 11, 2);
    canvas_draw_rbox(canvas, 112, 14, 14, 11, 2);
    canvas_set_font(canvas, FontSecondary);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str_aligned(canvas, 9, 23, AlignCenter, AlignBottom, "<");
    canvas_draw_str_aligned(canvas, 119, 23, AlignCenter, AlignBottom, ">");
    canvas_set_color(canvas, ColorBlack);
    snprintf(
        category_text,
        sizeof(category_text),
        "%s  %u/%u",
        known_category_names[app->known_category],
        record_count ? (unsigned int)(app->known_index + 1U) : 0U,
        (unsigned int)record_count);
    canvas_draw_str_aligned(canvas, 64, 23, AlignCenter, AlignBottom, category_text);

    const uint16_t first = (uint16_t)((app->known_index / KNOWN_LIST_ROWS) * KNOWN_LIST_ROWS);
    for(uint8_t row = 0; row < KNOWN_LIST_ROWS; row++) {
        const uint16_t position = first + row;
        if(position >= record_count) break;

        PostcodeDbRecord record;
        if(!known_category_record_at(app->known_category, position, &record)) continue;
        const int32_t y = 27 + (row * 12);
        if(position == app->known_index) {
            canvas_draw_rbox(canvas, 1, y - 2, 125, 11, 1);
            canvas_set_color(canvas, ColorWhite);
        }
        snprintf(
            row_text,
            sizeof(row_text),
            "%04X %-3s %.11s",
            record.code,
            postcode_db_type_short(record.type),
            record.name);
        canvas_draw_str(canvas, 4, y + 7, row_text);
        if(position == app->known_index) canvas_set_color(canvas, ColorBlack);
    }
    if(record_count > 0U) elements_scrollbar(canvas, app->known_index, record_count);
}

static void draw_known_detail(Canvas* canvas, const XboxPostApp* app) {
    PostcodeDbRecord record;
    if(!known_category_record_at(app->known_category, app->known_index, &record)) {
        draw_header(canvas, "Known Code");
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, "Record unavailable");
        return;
    }

    char title[24];
    char meta[32];
    char consoles[16];
    snprintf(title, sizeof(title), "Known: %04X", record.code);
    draw_header(canvas, title);
    format_console_mask(record.consoles, consoles, sizeof(consoles));
    snprintf(
        meta,
        sizeof(meta),
        "%s | %s | %s",
        consoles,
        postcode_db_type_short(record.type),
        record.is_error ? "ERROR" : "KNOWN");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 3, 22, meta);
    draw_wrapped_identifier(canvas, record.name);
    canvas_draw_line(canvas, 2, 43, 125, 43);

    const char* detail = record.description && record.description[0] ? record.description :
                                                                       "No repair notes yet.";
    draw_description_lines(canvas, detail, app->known_scroll, 3U, 49, 7U);
    const uint8_t detail_lines = wrapped_line_count(detail, 21U);
    if(detail_lines > 3U) elements_scrollbar(canvas, app->known_scroll, detail_lines - 2U);
}

static void saved_log_path(uint16_t number, char* output, size_t output_size) {
    snprintf(output, output_size, SAVED_LOG_DIRECTORY "/post_log_%03u.txt", number);
}

static void app_refresh_saved_logs(XboxPostApp* app) {
    app->saved_log_count = 0U;
    File* directory = storage_file_alloc(app->storage);
    const bool opened = storage_dir_open(directory, SAVED_LOG_DIRECTORY);
    if(opened) {
        FileInfo file_info;
        char name[64];
        while(app->saved_log_count < SAVED_LOG_MAX_NUMBER &&
              storage_dir_read(directory, &file_info, name, sizeof(name))) {
            unsigned int number;
            char extra;
            if(file_info_is_dir(&file_info) ||
               sscanf(name, "post_log_%u.txt%c", &number, &extra) != 1 || number == 0U ||
               number > SAVED_LOG_MAX_NUMBER) {
                continue;
            }

            /* Insert in numeric order once. Drawing and scrolling then use only
               this RAM cache and never block on SD-card operations. */
            uint16_t position = app->saved_log_count;
            while(position > 0U && app->saved_log_numbers[position - 1U] > number) {
                app->saved_log_numbers[position] = app->saved_log_numbers[position - 1U];
                position--;
            }
            app->saved_log_numbers[position] = (uint16_t)number;
            app->saved_log_count++;
        }
    }
    storage_dir_close(directory);
    storage_file_free(directory);

    if(app->saved_log_count == 0U) {
        app->saved_log_index = 0U;
    } else if(app->saved_log_index >= app->saved_log_count) {
        app->saved_log_index = app->saved_log_count - 1U;
    }
}

static void app_set_save_notice(XboxPostApp* app, const char* text) {
    snprintf(app->save_notice_text, sizeof(app->save_notice_text), "%s", text);
    app->save_notice = true;
}

static bool write_log_text(File* file, const char* text) {
    const size_t length = strlen(text);
    return storage_file_write(file, text, length) == length;
}

static bool app_save_history(XboxPostApp* app) {
    if(app->history_count == 0U) {
        /* An accidental held Up on an empty reader is a true no-op. */
        app->save_notice = false;
        return false;
    }

    storage_common_mkdir(app->storage, SAVED_LOG_DIRECTORY);
    char path[64];
    uint16_t log_number = 1U;
    for(uint16_t index = 0U; index < app->saved_log_count; index++) {
        if(app->saved_log_numbers[index] == log_number) {
            log_number++;
        } else if(app->saved_log_numbers[index] > log_number) {
            break;
        }
    }
    while(log_number <= SAVED_LOG_MAX_NUMBER) {
        saved_log_path(log_number, path, sizeof(path));
        if(!storage_file_exists(app->storage, path)) break;
        log_number++;
    }
    if(log_number > SAVED_LOG_MAX_NUMBER) {
        app_set_save_notice(app, "Log limit reached");
        return false;
    }

    File* file = storage_file_alloc(app->storage);
    bool success = storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_NEW);
    char line[96];
    if(success) {
        snprintf(
            line,
            sizeof(line),
            "Xbox POST Code Reader Log\nConsole %s\nEntries %u\n",
            postcode_db_console_name(app->console),
            (unsigned int)app->history_count);
        success = write_log_text(file, line);
    }

    for(uint8_t offset = 0U; success && offset < app->history_count; offset++) {
        const uint8_t index =
            (uint8_t)((app->history_head + POST_HISTORY_SIZE - app->history_count + offset) %
                      POST_HISTORY_SIZE);
        const PostEntry* entry = &app->history[index];
        snprintf(
            line,
            sizeof(line),
            "%07lums %04X %-3s %u\n",
            (unsigned long)entry->elapsed_ms,
            entry->code,
            post_flavor(entry->segment),
            (unsigned int)entry->segment);
        success = write_log_text(file, line);
    }

    if(success) success = storage_file_sync(file);
    storage_file_close(file);
    storage_file_free(file);

    if(success) {
        snprintf(
            app->save_notice_text,
            sizeof(app->save_notice_text),
            "Saved post_log_%03u",
            log_number);
        app->save_notice = true;
        app_refresh_saved_logs(app);
    } else {
        app_set_save_notice(app, "SD save failed");
    }
    return success;
}

static uint16_t count_text_lines(const char* text) {
    if(!text[0]) return 0U;
    uint16_t count = 1U;
    for(const char* cursor = text; *cursor; cursor++) {
        if(*cursor == '\n' && cursor[1]) count++;
    }
    return count;
}

static bool app_load_saved_log(XboxPostApp* app) {
    if(app->saved_log_index >= app->saved_log_count) {
        return false;
    }
    const uint16_t number = app->saved_log_numbers[app->saved_log_index];

    char path[64];
    saved_log_path(number, path, sizeof(path));
    File* file = storage_file_alloc(app->storage);
    bool success = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING);
    size_t length = 0U;
    if(success) {
        const uint64_t file_size = storage_file_size(file);
        length = file_size < (SAVED_LOG_BUFFER_SIZE - 1U) ? (size_t)file_size :
                                                            SAVED_LOG_BUFFER_SIZE - 1U;
        length = storage_file_read(file, app->saved_log_buffer, length);
        app->saved_log_buffer[length] = '\0';
    }
    storage_file_close(file);
    storage_file_free(file);

    if(success) {
        app->saved_log_scroll = 0U;
        app->saved_log_line_count = count_text_lines(app->saved_log_buffer);
    }
    return success;
}

static void draw_saved_logs(Canvas* canvas, const XboxPostApp* app) {
    char title[24];
    char row_text[24];
    snprintf(title, sizeof(title), "Saved Logs (%u)", app->saved_log_count);
    draw_header(canvas, title);
    canvas_set_font(canvas, FontSecondary);

    if(app->saved_log_count == 0U) {
        canvas_draw_str_aligned(canvas, 64, 37, AlignCenter, AlignCenter, "No saved logs");
        canvas_draw_str_aligned(canvas, 64, 49, AlignCenter, AlignCenter, "Hold Up in Reader");
        return;
    }

    const uint16_t first = (uint16_t)((app->saved_log_index / SAVED_LOG_ROWS) * SAVED_LOG_ROWS);
    for(uint8_t row = 0U; row < SAVED_LOG_ROWS; row++) {
        const uint16_t position = first + row;
        if(position >= app->saved_log_count) break;
        const uint16_t number = app->saved_log_numbers[position];
        const int32_t y = 15 + (row * 10);
        if(position == app->saved_log_index) {
            canvas_draw_rbox(canvas, 1, y, 125, 10, 1);
            canvas_set_color(canvas, ColorWhite);
        }
        snprintf(row_text, sizeof(row_text), "post_log_%03u.txt", number);
        canvas_draw_str(canvas, 5, y + 8, row_text);
        if(position == app->saved_log_index) canvas_set_color(canvas, ColorBlack);
    }
    elements_scrollbar(canvas, app->saved_log_index, app->saved_log_count);
    elements_button_center(canvas, "Open");
}

static void draw_saved_log_view(Canvas* canvas, const XboxPostApp* app) {
    char title[24];
    const uint16_t number = app->saved_log_index < app->saved_log_count ?
                                app->saved_log_numbers[app->saved_log_index] :
                                0U;
    snprintf(title, sizeof(title), "Log %03u", number);
    draw_header(canvas, title);
    canvas_set_font(canvas, FontSecondary);

    const char* cursor = app->saved_log_buffer;
    uint16_t line_number = 0U;
    uint8_t visible_row = 0U;
    while(*cursor && visible_row < 5U) {
        const char* end = strchr(cursor, '\n');
        const size_t length = end ? (size_t)(end - cursor) : strlen(cursor);
        if(line_number >= app->saved_log_scroll) {
            char line[23];
            const size_t copy_length = length < 21U ? length : 21U;
            memcpy(line, cursor, copy_length);
            line[copy_length] = '\0';
            canvas_draw_str(canvas, 2, 22 + (visible_row * 10), line);
            visible_row++;
        }
        line_number++;
        if(!end) break;
        cursor = end + 1;
    }
    if(app->saved_log_line_count > 5U) {
        elements_scrollbar(canvas, app->saved_log_scroll, app->saved_log_line_count - 4U);
    }
}

static void draw_wiring(Canvas* canvas, const XboxPostApp* app) {
    draw_header(canvas, "Wiring");

    /* A framed selector makes it obvious that Left/Right changes console type. */
    canvas_draw_rbox(canvas, 2, 15, 14, 13, 2);
    canvas_draw_rframe(canvas, 18, 15, 92, 13, 2);
    canvas_draw_rbox(canvas, 112, 15, 14, 13, 2);

    canvas_set_font(canvas, FontPrimary);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str_aligned(canvas, 9, 25, AlignCenter, AlignBottom, "<");
    canvas_draw_str_aligned(canvas, 119, 25, AlignCenter, AlignBottom, ">");
    canvas_set_color(canvas, ColorBlack);

    canvas_draw_str_aligned(
        canvas, 64, 25, AlignCenter, AlignBottom, postcode_db_console_name(app->console));

    canvas_set_font(canvas, FontSecondary);
    if(app->console <= PostcodeConsoleXboxOneX) {
        canvas_draw_str(canvas, 3, 39, "GPIO 15 SDA -> FACET 26");
        canvas_draw_str(canvas, 3, 50, "GPIO 16 SCL -> FACET 25");
    } else {
        canvas_draw_str(canvas, 3, 39, "GPIO 15 SDA -> AARDVARK 3");
        canvas_draw_str(canvas, 3, 50, "GPIO 16 SCL -> AARDVARK 1");
    }
    canvas_draw_str(canvas, 3, 61, "GND -> GND   |   NO POWER");
}

static void draw_web(Canvas* canvas) {
    draw_header(canvas, "Web Decoder");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 3, 21, "Scan or open:");
    canvas_draw_str(canvas, 3, 31, "https://demo.");
    canvas_draw_str(canvas, 3, 41, "coolshrimpmodz");
    canvas_draw_str(canvas, 3, 51, ".com/WebXbox");
    canvas_draw_str(canvas, 3, 61, "POSTTool/");

    const int32_t qr_x = 94;
    const int32_t qr_y = 20;
    for(uint8_t y = 0; y < WEB_QR_SIZE; y++) {
        for(uint8_t x = 0; x < WEB_QR_SIZE; x++) {
            const uint8_t byte = web_qr[(y * WEB_QR_ROW_BYTES) + (x / 8U)];
            if(byte & (1U << (x % 8U))) canvas_draw_dot(canvas, qr_x + x, qr_y + y);
        }
    }
}

static void draw_about(Canvas* canvas) {
    draw_header(canvas, "About");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 3, 23, "Xbox POST Code Reader v0.14");
    canvas_draw_line(canvas, 3, 27, 124, 27);
    canvas_draw_str(canvas, 3, 38, "Flipper port by coolshrimp");
    canvas_draw_line(canvas, 3, 42, 124, 42);
    canvas_draw_str(canvas, 3, 53, "XboxResearch database");
    canvas_draw_str(canvas, 3, 63, "452 console-aware records");
}

static void draw_callback(Canvas* canvas, void* context) {
    XboxPostApp* app = context;
    canvas_clear(canvas);

    switch(app->page) {
    case AppPageMenu:
        draw_menu(canvas, app);
        break;
    case AppPageReader:
        draw_reader(canvas, app);
        break;
    case AppPageKnownCodes:
        draw_known_codes(canvas, app);
        break;
    case AppPageKnownDetail:
        draw_known_detail(canvas, app);
        break;
    case AppPageSavedLogs:
        draw_saved_logs(canvas, app);
        break;
    case AppPageSavedLogView:
        draw_saved_log_view(canvas, app);
        break;
    case AppPageWiring:
        draw_wiring(canvas, app);
        break;
    case AppPageWeb:
        draw_web(canvas);
        break;
    case AppPageAbout:
        draw_about(canvas);
        break;
    }
}

static void input_callback(InputEvent* input_event, void* context) {
    FuriMessageQueue* input_queue = context;
    if(input_event->type == InputTypeShort || input_event->type == InputTypeLong ||
       input_event->type == InputTypeRepeat) {
        furi_message_queue_put(input_queue, input_event, 0);
    }
}

static void app_clear_history(XboxPostApp* app) {
    app->history_count = 0;
    app->history_head = 0;
    app->history_offset = 0;
    app->confirm_clear = false;
    app->reader.dropped_events = 0;
}

static void app_add_history(XboxPostApp* app, const PostEntry* entry) {
    app->history[app->history_head] = *entry;
    app->history_head = (uint8_t)((app->history_head + 1U) % POST_HISTORY_SIZE);

    if(app->history_count < POST_HISTORY_SIZE) {
        app->history_count++;
    }

    if(app->history_offset > 0 && app->history_offset < (app->history_count - 1U)) {
        app->history_offset++;
    }
}

static void app_set_listening(XboxPostApp* app, bool listening) {
    if(listening == app->listening) return;
    if(listening) {
        reader_start(&app->reader, app->post_queue);
    } else {
        reader_stop(&app->reader);
    }
    app->listening = listening;
}

static void app_open_menu_item(XboxPostApp* app) {
    switch(app->menu_index) {
    case 0:
        app->page = AppPageReader;
        app->reader_view = ReaderViewSingle;
        app_set_listening(app, true);
        break;
    case 1:
        app->page = AppPageKnownCodes;
        break;
    case 2:
        app_refresh_saved_logs(app);
        app->page = AppPageSavedLogs;
        break;
    case 3:
        app->page = AppPageWiring;
        break;
    case 4:
        app->page = AppPageWeb;
        break;
    case 5:
        app->page = AppPageAbout;
        break;
    default:
        break;
    }
}

static void app_handle_input(XboxPostApp* app, const InputEvent* input) {
    const bool short_press = input->type == InputTypeShort;
    const bool long_press = input->type == InputTypeLong;
    const bool nav_press = short_press || input->type == InputTypeRepeat;
    if(!short_press && !long_press && !nav_press) return;
    /* A held Up generates Repeat events after Long. Keep the save result visible
       and do not let those repeats immediately move the history selection. */
    if(app->save_notice && app->page == AppPageReader && input->key == InputKeyUp &&
       input->type == InputTypeRepeat) {
        return;
    }
    if(app->save_notice) app->save_notice = false;

    if(app->page == AppPageMenu) {
        if(nav_press && input->key == InputKeyUp) {
            app->menu_index = (app->menu_index + MENU_ITEM_COUNT - 1U) % MENU_ITEM_COUNT;
        } else if(nav_press && input->key == InputKeyDown) {
            app->menu_index = (app->menu_index + 1U) % MENU_ITEM_COUNT;
        } else if(short_press && input->key == InputKeyOk) {
            app_open_menu_item(app);
        } else if(short_press && input->key == InputKeyBack) {
            app->exit_requested = true;
        }
        return;
    }

    if(app->page == AppPageReader) {
        if(app->confirm_clear) {
            if(short_press && input->key == InputKeyOk) {
                app_clear_history(app);
            } else if(short_press && input->key == InputKeyBack) {
                app->confirm_clear = false;
            }
            return;
        }

        if(long_press && input->key == InputKeyUp && app->reader_view != ReaderViewDecode) {
            app_save_history(app);
        } else if(long_press && input->key == InputKeyLeft && app->reader_view != ReaderViewDecode) {
            if(app->history_count > 0U) app->confirm_clear = true;
        } else if(short_press && input->key == InputKeyOk) {
            app_set_listening(app, !app->listening);
        } else if(nav_press && input->key == InputKeyUp) {
            if(app->reader_view == ReaderViewDecode) {
                if(app->decode_scroll > 0U) app->decode_scroll--;
            } else if(app->history_offset + 1U < app->history_count) {
                app->history_offset++;
            }
        } else if(nav_press && input->key == InputKeyDown) {
            if(app->reader_view == ReaderViewDecode) {
                const PostEntry* entry = app_selected_entry(app);
                if(entry) {
                    char name[160];
                    const char* description = "";
                    bool is_error = false;
                    postcode_db_format(
                        entry->code,
                        entry->segment,
                        app->console,
                        name,
                        sizeof(name),
                        &description,
                        &is_error);
                    const uint8_t lines = wrapped_line_count(
                        description && description[0] ? description : "No repair notes yet.", 21U);
                    if(app->decode_scroll + 2U < lines) app->decode_scroll++;
                }
            } else if(app->history_offset > 0) {
                app->history_offset--;
            }
        } else if(short_press && input->key == InputKeyLeft) {
            if(app->reader_view == ReaderViewSingle) {
                app->reader_view = ReaderViewLog;
            } else if(app->reader_view == ReaderViewLog) {
                if(app->history_count > 0U) app->confirm_clear = true;
            } else {
                app->reader_view = app->decode_return_view;
            }
        } else if(short_press && input->key == InputKeyRight) {
            if(app->reader_view == ReaderViewDecode) {
                app->console = (PostcodeConsole)((app->console + 1U) % PostcodeConsoleCount);
                app->decode_scroll = 0U;
            } else if(app->history_count > 0U) {
                app->decode_return_view = app->reader_view;
                app->reader_view = ReaderViewDecode;
                app->decode_scroll = 0U;
            }
        } else if((short_press || long_press) && input->key == InputKeyBack) {
            if(app->reader_view == ReaderViewDecode) {
                app->reader_view = app->decode_return_view;
                app->decode_scroll = 0U;
            } else if(app->reader_view == ReaderViewLog) {
                app->reader_view = ReaderViewSingle;
            } else {
                app_set_listening(app, false);
                app->page = AppPageMenu;
            }
        }
        return;
    }

    if(app->page == AppPageSavedLogs) {
        if(nav_press && input->key == InputKeyUp) {
            if(app->saved_log_index > 0U) app->saved_log_index--;
        } else if(nav_press && input->key == InputKeyDown) {
            if(app->saved_log_index + 1U < app->saved_log_count) {
                app->saved_log_index++;
            }
        } else if(short_press && input->key == InputKeyOk) {
            if(app_load_saved_log(app)) app->page = AppPageSavedLogView;
        } else if(short_press && input->key == InputKeyBack) {
            app->page = AppPageMenu;
        }
        return;
    }

    if(app->page == AppPageSavedLogView) {
        if(nav_press && input->key == InputKeyUp) {
            if(app->saved_log_scroll > 0U) app->saved_log_scroll--;
        } else if(nav_press && input->key == InputKeyDown) {
            if(app->saved_log_scroll + 5U < app->saved_log_line_count) {
                app->saved_log_scroll++;
            }
        } else if(short_press && input->key == InputKeyBack) {
            app->page = AppPageSavedLogs;
        }
        return;
    }

    if(app->page == AppPageKnownCodes) {
        if(nav_press && input->key == InputKeyUp) {
            if(app->known_index > 0U) app->known_index--;
        } else if(nav_press && input->key == InputKeyDown) {
            const uint16_t count = known_category_count(app->known_category);
            if(app->known_index + 1U < count) app->known_index++;
        } else if(short_press && input->key == InputKeyLeft) {
            app->known_category = (KnownCategory)((app->known_category + KnownCategoryCount - 1U) %
                                                  KnownCategoryCount);
            app->known_index = 0U;
            app->known_scroll = 0U;
        } else if(short_press && input->key == InputKeyRight) {
            app->known_category = (KnownCategory)((app->known_category + 1U) % KnownCategoryCount);
            app->known_index = 0U;
            app->known_scroll = 0U;
        } else if(short_press && input->key == InputKeyOk) {
            if(known_category_count(app->known_category) > 0U) {
                app->known_scroll = 0U;
                app->page = AppPageKnownDetail;
            }
        } else if(short_press && input->key == InputKeyBack) {
            app->page = AppPageMenu;
        }
        return;
    }

    if(app->page == AppPageKnownDetail) {
        PostcodeDbRecord record;
        if(nav_press && input->key == InputKeyUp) {
            if(app->known_scroll > 0U) app->known_scroll--;
        } else if(
            nav_press && input->key == InputKeyDown &&
            known_category_record_at(app->known_category, app->known_index, &record)) {
            const char* detail = record.description && record.description[0] ?
                                     record.description :
                                     "No repair notes yet.";
            const uint8_t lines = wrapped_line_count(detail, 21U);
            if(app->known_scroll + 3U < lines) app->known_scroll++;
        } else if(short_press && input->key == InputKeyBack) {
            app->page = AppPageKnownCodes;
        }
        return;
    }

    if(app->page == AppPageWiring && nav_press && input->key == InputKeyLeft) {
        app->console =
            (PostcodeConsole)((app->console + PostcodeConsoleCount - 1U) % PostcodeConsoleCount);
    } else if(app->page == AppPageWiring && nav_press && input->key == InputKeyRight) {
        app->console = (PostcodeConsole)((app->console + 1U) % PostcodeConsoleCount);
    } else if(short_press && input->key == InputKeyBack) {
        app->page = AppPageMenu;
    }
}

int32_t app_main(void* argument) {
    UNUSED(argument);

    XboxPostApp* app = malloc(sizeof(XboxPostApp));
    furi_check(app);
    memset(app, 0, sizeof(XboxPostApp));
    app->console = PostcodeConsoleXboxOneS;
    app->storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(app->storage, SAVED_LOG_DIRECTORY);
    app_refresh_saved_logs(app);

    app->input_queue = furi_message_queue_alloc(INPUT_QUEUE_SIZE, sizeof(InputEvent));
    app->post_queue = furi_message_queue_alloc(POST_QUEUE_SIZE, sizeof(PostEntry));
    /* Let runfap.py receive the loader's successful launch response before
       replacing the firmware-owned CDC callbacks with the app protocol. */
    furi_delay_ms(750);
    post_serial_start(&app->serial);
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app->input_queue);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    InputEvent input;
    PostEntry post;
    while(!app->exit_requested) {
        bool redraw = false;

        /* Input has a dedicated queue, so a burst of POST traffic can never
           fill it and make the controls appear locked. */
        if(furi_message_queue_get(app->input_queue, &input, 20) == FuriStatusOk) {
            app_handle_input(app, &input);
            app->serial.console = app->console;
            redraw = true;
        }

        /* Drain a complete burst, then redraw once. */
        for(uint8_t i = 0; i < 32U; i++) {
            if(furi_message_queue_get(app->post_queue, &post, 0) != FuriStatusOk) break;
            app_add_history(app, &post);
            post_serial_enqueue(&app->serial, &post);
            redraw = true;
        }

        if(redraw) {
            view_port_update(app->view_port);
        }
    }

    app_set_listening(app, false);
    post_serial_stop(&app->serial);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);
    furi_message_queue_free(app->post_queue);
    furi_message_queue_free(app->input_queue);
    free(app);
    return 0;
}
