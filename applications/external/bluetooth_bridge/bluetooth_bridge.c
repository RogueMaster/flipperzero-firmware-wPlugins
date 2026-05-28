/*
 * BLE UART Bridge for Flipper Zero
 *
 * Bridges BLE serial (phone) <-> GPIO UART (ESP32 Marauder).
 * Replaces the built-in UART Bridge which only bridges USB <-> UART.
 *
 * UART: USART1 (GPIO pins 13 TX / 14 RX), 115200 baud
 *
 * Self-update: send magic "\x00FAP" + 4-byte LE size + 4-byte LE CRC32
 * + raw .fap data over BLE serial. CRC32 verified before flash write.
 *
 * Battery level is reported via standard BLE Battery Service (0x180F)
 * and shown on screen.
 */

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <furi_ble/profile_interface.h>
#include <bt/bt_service/bt.h>
#include <profiles/serial_profile.h>
#include <storage/storage.h>
#include <loader/loader.h>
#include <power/power_service/power.h>
#include <toolbox/crc32_calc.h>

#define TAG "BleUART"

#include "commit_hash.h"
#ifndef COMMIT_HASH
#define COMMIT_HASH "dev"
#endif

#define BRIDGE_UART_ID FuriHalSerialIdUsart
#define BRIDGE_BAUD    115200
#define UART_BUF_SIZE  512
#define BLE_BUF_SIZE   8192  /* larger buffer for BLE RX — holds full .fap during update */

/* Magic prefix for self-update: "\x00FAP" */
static const uint8_t UPDATE_MAGIC[] = {0x00, 'F', 'A', 'P'};
#define UPDATE_MAGIC_LEN 4

typedef enum {
    UpdateIdle,
    UpdateMagic,
    UpdateSize,
    UpdateReceiving,
    UpdateDone,
    UpdateError,
} UpdateState;

typedef struct {
    Bt* bt;
    FuriHalBleProfileBase* ble_profile;
    FuriHalSerialHandle* serial;
    FuriStreamBuffer* uart_rx_stream;
    FuriStreamBuffer* ble_rx_stream;
    Gui* gui;
    ViewPort* view_port;
    volatile bool running;
    volatile uint32_t rx_count;
    volatile uint32_t tx_count;

    UpdateState update_state;
    Storage* storage;
    FuriString* self_path;
    FuriString* tmp_path;
    uint32_t update_size;
    uint32_t update_received;
    uint32_t update_crc;  /* expected CRC32 from header */
    uint8_t* update_buf;  /* RAM buffer for incoming .fap data */
    uint8_t magic_pos;
    uint8_t header_buf[8]; /* 4 bytes size + 4 bytes CRC32 */
    uint8_t header_pos;
    uint32_t update_last_rx_tick; /* tick of last update data received */

    Power* power;
    uint8_t battery_level;
} BleUartBridge;

/* ---------- UART RX callback (ISR context) ---------- */

static void uart_rx_callback(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    void* context) {
    BleUartBridge* app = context;
    if(!app->running) return;
    if(event & FuriHalSerialRxEventData) {
        while(furi_hal_serial_async_rx_available(handle)) {
            uint8_t byte = furi_hal_serial_async_rx(handle);
            furi_stream_buffer_send(app->uart_rx_stream, &byte, 1, 0);
        }
    }
}

/* ---------- BLE RX callback ---------- */

static uint16_t ble_rx_callback(SerialServiceEvent event, void* context) {
    BleUartBridge* app = context;
    if(!app->running) return 0;
    if(event.event == SerialServiceEventTypeDataReceived && event.data.size > 0) {
        furi_stream_buffer_send(
            app->ble_rx_stream, event.data.buffer, event.data.size, 0);
    }
    return 0;
}

/* Re-set our serial event callback whenever BT status changes.
   The firmware's BT service overrides our callback with an RPC handler
   on every BLE connect (bt_on_gap_event_callback → rpc_session_open →
   ble_profile_serial_set_event_callback). If we don't reclaim it, our
   update data gets fed to rpc_session_feed as invalid Protobuf and
   the RPC parser crashes (furi_check). */
static void bt_status_changed(BtStatus status, void* context) {
    BleUartBridge* app = context;
    FURI_LOG_I(TAG, "BT status: %d", status);
    if(app->ble_profile) {
        ble_profile_serial_set_event_callback(
            app->ble_profile, BLE_BUF_SIZE, ble_rx_callback, app);
    }
}

/* ---------- BLE RX data processing ---------- */

static void ble_process_data(BleUartBridge* app, const uint8_t* data, size_t len) {
    size_t pos = 0;
    while(pos < len) {
        switch(app->update_state) {
        case UpdateIdle:
        case UpdateMagic:
        case UpdateError: {
            /* In UpdateError, allow re-entry via new magic sequence */
            uint8_t byte = data[pos++];
            if(byte == UPDATE_MAGIC[app->magic_pos]) {
                app->magic_pos++;
                if(app->update_state != UpdateMagic) {
                    app->update_state = UpdateMagic;
                }
                if(app->magic_pos == UPDATE_MAGIC_LEN) {
                    app->update_state = UpdateSize;
                    app->header_pos = 0;
                }
            } else {
                if(app->serial && app->update_state != UpdateError) {
                    if(app->magic_pos > 0) {
                        furi_hal_serial_tx(app->serial, UPDATE_MAGIC, app->magic_pos);
                        app->tx_count += app->magic_pos;
                    }
                    furi_hal_serial_tx(app->serial, &byte, 1);
                    app->tx_count++;
                }
                app->magic_pos = 0;
                if(app->update_state == UpdateMagic) {
                    app->update_state = UpdateIdle;
                }
                /* Stay in UpdateError if was in UpdateError and magic didn't match */
            }
            break;
        }
        case UpdateSize: {
            app->header_buf[app->header_pos++] = data[pos++];
            if(app->header_pos == 8) {
                app->update_size = (uint32_t)app->header_buf[0] |
                                   ((uint32_t)app->header_buf[1] << 8) |
                                   ((uint32_t)app->header_buf[2] << 16) |
                                   ((uint32_t)app->header_buf[3] << 24);
                app->update_crc  = (uint32_t)app->header_buf[4] |
                                   ((uint32_t)app->header_buf[5] << 8) |
                                   ((uint32_t)app->header_buf[6] << 16) |
                                   ((uint32_t)app->header_buf[7] << 24);
                if(app->update_size == 0 || app->update_size > 512 * 1024) {
                    app->update_state = UpdateError;
                    return;
                }
                /* Allocate RAM buffer — no flash writes during BLE reception */
                app->update_buf = malloc(app->update_size);
                if(!app->update_buf) {
                    app->update_state = UpdateError;
                    return;
                }
                app->update_state = UpdateReceiving;
                app->update_received = 0;
            }
            break;
        }
        case UpdateReceiving: {
            size_t remaining_file = app->update_size - app->update_received;
            size_t remaining_buf = len - pos;
            size_t to_copy = remaining_buf < remaining_file ? remaining_buf : remaining_file;
            memcpy(app->update_buf + app->update_received, data + pos, to_copy);
            app->update_received += to_copy;
            pos += to_copy;
            if(app->update_received >= app->update_size) {
                /* Verify CRC32 before writing to flash */
                uint32_t actual_crc = crc32_calc_buffer(
                    0, app->update_buf, app->update_size);
                if(actual_crc != app->update_crc) {
                    FURI_LOG_E(TAG, "CRC32 mismatch: expected %08lx got %08lx",
                        (unsigned long)app->update_crc, (unsigned long)actual_crc);
                    free(app->update_buf);
                    app->update_buf = NULL;
                    if(app->ble_profile) {
                        uint8_t err_msg[] = {'E', 'R', '\n'};
                        ble_profile_serial_tx(app->ble_profile, err_msg, 3);
                    }
                    app->update_state = UpdateError;
                    return;
                }
                /* All data received and verified — write to flash */
                FURI_LOG_I(TAG, "Update %lu bytes CRC OK, writing...",
                    (unsigned long)app->update_size);

                if(!app->storage) {
                    app->storage = furi_record_open(RECORD_STORAGE);
                }

                /* Write directly to self_path (overwrite in place) */
                File* f = storage_file_alloc(app->storage);
                bool write_ok = false;
                if(storage_file_open(f, furi_string_get_cstr(app->self_path),
                       FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
                    size_t written = storage_file_write(
                        f, app->update_buf, app->update_size);
                    write_ok = (written == app->update_size);
                    if(!write_ok) {
                        FURI_LOG_E(TAG, "Write failed: %lu/%lu",
                            (unsigned long)written, (unsigned long)app->update_size);
                    }
                    storage_file_close(f);
                } else {
                    FURI_LOG_E(TAG, "Failed to open %s for writing",
                        furi_string_get_cstr(app->self_path));
                }
                storage_file_free(f);

                free(app->update_buf);
                app->update_buf = NULL;

                if(write_ok) {
                    /* Send OK indication so deploy.py disconnects immediately */
                    if(app->ble_profile) {
                        uint8_t ok_msg[] = {'O', 'K', '\n'};
                        ble_profile_serial_tx(app->ble_profile, ok_msg, 3);
                        furi_delay_ms(200); /* let indication reach the phone */
                    }
                    app->update_state = UpdateDone;
                    /* Exit main loop — relaunch the updated FAP */
                    app->running = false;
                } else {
                    if(app->ble_profile) {
                        uint8_t err_msg[] = {'E', 'R', '\n'};
                        ble_profile_serial_tx(app->ble_profile, err_msg, 3);
                    }
                    app->update_state = UpdateError;
                }
            }
            break;
        }
        case UpdateDone:
            return;
        }
    }
}

/* ---------- GUI ---------- */

static void draw_callback(Canvas* canvas, void* context) {
    BleUartBridge* app = context;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, "Bluetooth Bridge");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 126, 12, AlignRight, AlignBottom, COMMIT_HASH);

    if(!app->ble_profile && !app->serial) {
        canvas_draw_str(canvas, 2, 26, "BLE: FAILED  UART: FAILED");
        canvas_draw_str(canvas, 2, 40, "Resources busy, try again");
        canvas_draw_str(canvas, 2, 64, "Press BACK to exit");
        return;
    }

    switch(app->update_state) {
    case UpdateIdle:
    case UpdateMagic: {
        char buf[44];
        snprintf(buf, sizeof(buf), "USART1  115200 8N1  %u%%", app->battery_level);
        canvas_draw_str(canvas, 2, 26, buf);
        if(!app->ble_profile) {
            canvas_draw_str(canvas, 2, 40, "BLE: waiting...");
        } else if(!app->serial) {
            canvas_draw_str(canvas, 2, 40, "UART: waiting...");
        } else {
            snprintf(buf, sizeof(buf), "ESP32 -> Phone: %lu B", (unsigned long)app->rx_count);
            canvas_draw_str(canvas, 2, 40, buf);
            snprintf(buf, sizeof(buf), "Phone -> ESP32: %lu B", (unsigned long)app->tx_count);
            canvas_draw_str(canvas, 2, 52, buf);
        }
        canvas_draw_str(canvas, 2, 64, "Press BACK to exit");
        break;
    }
    case UpdateSize:
    case UpdateReceiving: {
        char buf[40];
        uint32_t pct = app->update_size > 0
                           ? (app->update_received * 100 / app->update_size) : 0;
        snprintf(buf, sizeof(buf), "Updating: %lu / %lu B",
            (unsigned long)app->update_received, (unsigned long)app->update_size);
        canvas_draw_str(canvas, 2, 30, buf);
        /* Progress bar: 2px margin, 124px wide, 12px tall */
        canvas_draw_frame(canvas, 2, 38, 124, 12);
        uint32_t fill_w = pct * 120 / 100;
        if(fill_w > 0) {
            canvas_draw_box(canvas, 4, 40, fill_w, 8);
        }
        snprintf(buf, sizeof(buf), "%lu%%", (unsigned long)pct);
        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignBottom, buf);
        break;
    }
    case UpdateDone:
        canvas_draw_str(canvas, 2, 26, "Update complete!");
        canvas_draw_str(canvas, 2, 40, "Press BACK to exit.");
        break;
    case UpdateError:
        canvas_draw_str(canvas, 2, 26, "Update FAILED");
        canvas_draw_str(canvas, 2, 40, "Check file & retry.");
        canvas_draw_str(canvas, 2, 64, "BACK=Exit");
        break;
    }
}

static void input_callback(InputEvent* event, void* context) {
    BleUartBridge* app = context;
    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        app->running = false;
    }
}

/* ---------- App entry ---------- */

int32_t bluetooth_bridge_app(void* p) {
    UNUSED(p);
    FURI_LOG_I(TAG, "=== App starting ===");

    BleUartBridge* app = malloc(sizeof(BleUartBridge));
    memset(app, 0, sizeof(BleUartBridge));
    app->running = true;
    app->update_state = UpdateIdle;

    // --- Resolve own path for self-update ---
    Loader* loader = furi_record_open(RECORD_LOADER);
    app->self_path = furi_string_alloc();
    loader_get_application_launch_path(loader, app->self_path);
    furi_record_close(RECORD_LOADER);

    app->tmp_path = furi_string_alloc();
    furi_string_printf(app->tmp_path, "%s.tmp", furi_string_get_cstr(app->self_path));

    // --- GUI ---
    app->gui = furi_record_open(RECORD_GUI);
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    // --- BLE ---
    app->bt = furi_record_open(RECORD_BT);
    for(int attempt = 0; attempt < 30 && app->running; attempt++) {
        app->ble_profile = bt_profile_start(app->bt, ble_profile_serial, NULL);
        if(app->ble_profile) {
            FURI_LOG_I(TAG, "BLE started on attempt %d", attempt);
            break;
        }
        FURI_LOG_W(TAG, "bt_profile_start attempt %d failed", attempt);
        furi_delay_ms(200);
    }
    if(app->ble_profile) {
        ble_profile_serial_set_event_callback(
            app->ble_profile, BLE_BUF_SIZE, ble_rx_callback, app);
        /* Reclaim our callback whenever BT service overrides it on connect */
        bt_set_status_changed_callback(app->bt, bt_status_changed, app);
    }

    // --- UART ---
    app->uart_rx_stream = furi_stream_buffer_alloc(UART_BUF_SIZE, 1);
    app->ble_rx_stream = furi_stream_buffer_alloc(BLE_BUF_SIZE, 1);
    for(int attempt = 0; attempt < 30 && app->running; attempt++) {
        app->serial = furi_hal_serial_control_acquire(BRIDGE_UART_ID);
        if(app->serial) {
            FURI_LOG_I(TAG, "Serial acquired on attempt %d", attempt);
            break;
        }
        FURI_LOG_W(TAG, "serial acquire attempt %d failed", attempt);
        furi_delay_ms(200);
    }
    if(app->serial) {
        furi_hal_serial_init(app->serial, BRIDGE_BAUD);
        furi_hal_serial_async_rx_start(app->serial, uart_rx_callback, app, false);
    }

    // --- 5V power for WiFi devboard ---
    furi_hal_power_enable_otg();

    // --- Battery ---
    app->power = furi_record_open(RECORD_POWER);
    FURI_LOG_I(TAG, "=== Init complete ===");

    /*
     * Single-threaded main loop: no worker thread = no deadlock on exit.
     * Polls stream buffers with timeout=0, sleeps 5ms between iterations.
     * ~200 Hz is more than enough for 115200 baud UART.
     */
    uint8_t buf[BLE_SVC_SERIAL_CHAR_VALUE_LEN_MAX];
    uint32_t tick = 0;

    while(app->running) {
        /* UART RX -> BLE TX (pause during self-update to avoid indication collision) */
        size_t uart_len = furi_stream_buffer_receive(
            app->uart_rx_stream, buf, sizeof(buf), 0);
        if(uart_len > 0 && app->running && app->ble_profile &&
           app->update_state == UpdateIdle) {
            ble_profile_serial_tx(app->ble_profile, buf, (uint16_t)uart_len);
            app->rx_count += uart_len;
        }
        if(!app->running) break;

        /* BLE RX -> UART TX / update */
        size_t ble_len = furi_stream_buffer_receive(
            app->ble_rx_stream, buf, sizeof(buf), 0);
        if(ble_len > 0 && app->running) {
            ble_process_data(app, buf, ble_len);
            app->update_last_rx_tick = furi_get_tick();
        }

        /* Reset stale update state (e.g. after BLE disconnect mid-transfer) */
        if((app->update_state == UpdateReceiving || app->update_state == UpdateSize) &&
           (furi_get_tick() - app->update_last_rx_tick) > 5000) {
            FURI_LOG_W(TAG, "Update timeout — resetting to idle");
            if(app->update_buf) {
                free(app->update_buf);
                app->update_buf = NULL;
            }
            app->update_state = UpdateIdle;
            app->magic_pos = 0;
            app->header_pos = 0;
            app->update_received = 0;
        }

        /* Battery & screen every ~500ms */
        if(++tick >= 100) {
            tick = 0;
            PowerInfo info;
            power_get_info(app->power, &info);
            app->battery_level = info.charge;
            view_port_update(app->view_port);
        }

        furi_delay_ms(5);
    }

    FURI_LOG_I(TAG, "=== Cleanup ===");

    /* Disable UI first */
    view_port_enabled_set(app->view_port, false);

    /* Stop UART ISR before anything else */
    if(app->serial) {
        furi_hal_serial_async_rx_stop(app->serial);
        furi_hal_serial_deinit(app->serial);
        furi_hal_serial_control_release(app->serial);
        app->serial = NULL;
        FURI_LOG_D(TAG, "Serial released");
    }

    /* Tear down BLE — must fully disconnect before profile restore. */
    if(app->ble_profile) {
        ble_profile_serial_set_event_callback(app->ble_profile, 0, NULL, NULL);
        bt_disconnect(app->bt);
        FURI_LOG_D(TAG, "Waiting for BLE disconnect to complete...");
        furi_delay_ms(3000);
        FURI_LOG_D(TAG, "Restoring BT profile...");
        bt_profile_restore_default(app->bt);
        app->ble_profile = NULL;
        FURI_LOG_D(TAG, "BT profile restored");
    }

    furi_stream_buffer_free(app->uart_rx_stream);
    furi_stream_buffer_free(app->ble_rx_stream);

    if(app->update_buf) {
        free(app->update_buf);
    }
    if(app->storage) {
        furi_record_close(RECORD_STORAGE);
    }

    /* Relaunch self if we just updated */
    bool should_relaunch = (app->update_state == UpdateDone);
    FuriString* relaunch_path = NULL;
    if(should_relaunch) {
        relaunch_path = furi_string_alloc_set(app->self_path);
    }

    furi_string_free(app->self_path);
    furi_string_free(app->tmp_path);
    furi_record_close(RECORD_POWER);
    furi_hal_power_disable_otg();
    bt_set_status_changed_callback(app->bt, NULL, NULL);
    furi_record_close(RECORD_BT);

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);

    free(app);
    FURI_LOG_I(TAG, "=== Done ===");

    if(should_relaunch) {
        Loader* loader = furi_record_open(RECORD_LOADER);
        loader_enqueue_launch(loader, furi_string_get_cstr(relaunch_path), NULL, LoaderDeferredLaunchFlagNone);
        furi_record_close(RECORD_LOADER);
        furi_string_free(relaunch_path);
    }

    return 0;
}
