/* os_detect.c — Target OS detection for FlipperPwn.
 * Uses USB CDC device enumeration responses to identify Windows, macOS,
 * or Linux on the connected host. */

#include "flipperpwn.h"
#include <furi_hal_usb_cdc.h>
#include <string.h>

#define TAG "FPwn"

/* Poll timeout in ms — how long to wait for the host to echo an LED change.
 * 1500 ms to accommodate Windows 11 which can be slow to process HID LED
 * reports, especially on the first interaction after enumeration. */
#define LED_POLL_TIMEOUT_MS 1500

/* ----------------------------------------------------------------------------
 * wait_for_led_change()
 *
 * Polls furi_hal_hid_get_led_state() at 2 ms intervals until the masked bits
 * differ from `before`, or `timeout_ms` elapses.
 *
 * Returns the new LED state, or `before` unchanged if the timeout expired.
 * --------------------------------------------------------------------------*/
static uint8_t wait_for_led_change(uint8_t before, uint8_t mask, uint32_t timeout_ms) {
    uint32_t start = furi_get_tick();
    while((furi_get_tick() - start) < furi_ms_to_ticks(timeout_ms)) {
        uint8_t now = furi_hal_hid_get_led_state();
        if((now ^ before) & mask) {
            return now;
        }
        furi_delay_ms(2);
    }
    return before; /* unchanged */
}

/* ----------------------------------------------------------------------------
 * toggle_key()
 *
 * Press and release a HID key, then poll for the given LED mask to change.
 * Returns true if the LED bit changed within the timeout.
 * --------------------------------------------------------------------------*/
static bool toggle_key_and_check(uint16_t key, uint8_t led_mask) {
    uint8_t before = furi_hal_hid_get_led_state();

    furi_hal_hid_kb_press(key);
    furi_hal_hid_kb_release(key);

    uint8_t after = wait_for_led_change(before, led_mask, LED_POLL_TIMEOUT_MS);
    return ((after ^ before) & led_mask) != 0;
}

/* ----------------------------------------------------------------------------
 * restore_key()
 *
 * Toggle a key back and wait for the LED to return to its original state.
 * --------------------------------------------------------------------------*/
static void restore_key(uint16_t key, uint8_t led_mask) {
    uint8_t before = furi_hal_hid_get_led_state();

    furi_hal_hid_kb_press(key);
    furi_hal_hid_kb_release(key);

    /* Wait for the restore to be acknowledged; ignore if it times out. */
    wait_for_led_change(before, led_mask, LED_POLL_TIMEOUT_MS);
}

/* ----------------------------------------------------------------------------
 * fpwn_os_detect()
 *
 * Phase 0 — CapsLock probe (connectivity check)
 *   If the host does not echo a CapsLock LED change within 500 ms the USB
 *   HID stack is not active.  Return FPwnOSUnknown.
 *
 * Phase 1 — NumLock probe (macOS discriminator)
 *   macOS ignores NumLock on external keyboards.  No LED echo → macOS.
 *
 * Phase 2 — ScrollLock probe (Windows vs Linux)
 *   Windows reflects ScrollLock LED; Linux desktop environments do not.
 *
 * All toggled keys are restored before returning.
 * --------------------------------------------------------------------------*/
FPwnOS fpwn_os_detect(void) {
    /* Phase 0: CapsLock connectivity check — retry once on failure since
     * Windows 11 can be slow to process the first HID LED report after
     * enumeration. */
    bool caps_ok = toggle_key_and_check(HID_KEYBOARD_CAPS_LOCK, HID_KB_LED_CAPS);

    if(!caps_ok) {
        /* First attempt failed — restore CapsLock and retry after a pause. */
        furi_hal_hid_kb_press(HID_KEYBOARD_CAPS_LOCK);
        furi_hal_hid_kb_release(HID_KEYBOARD_CAPS_LOCK);
        furi_delay_ms(500);

        caps_ok = toggle_key_and_check(HID_KEYBOARD_CAPS_LOCK, HID_KB_LED_CAPS);
    }

    if(!caps_ok) {
        FURI_LOG_I(TAG, "OS detect: no CapsLock echo — USB HID not active");
        furi_hal_hid_kb_press(HID_KEYBOARD_CAPS_LOCK);
        furi_hal_hid_kb_release(HID_KEYBOARD_CAPS_LOCK);
        furi_delay_ms(50);
        return FPwnOSUnknown;
    }

    /* Restore CapsLock */
    restore_key(HID_KEYBOARD_CAPS_LOCK, HID_KB_LED_CAPS);

    /* Phase 1: NumLock probe */
    bool num_ok = toggle_key_and_check(HID_KEYPAD_NUMLOCK, HID_KB_LED_NUM);

    if(!num_ok) {
        /* macOS does not reflect NumLock — no restore needed. */
        FURI_LOG_I(TAG, "OS detect: NumLock unchanged → macOS");
        return FPwnOSMac;
    }

    /* Restore NumLock */
    restore_key(HID_KEYPAD_NUMLOCK, HID_KB_LED_NUM);

    /* Phase 2: ScrollLock probe */
    bool scroll_ok = toggle_key_and_check(HID_KEYBOARD_SCROLL_LOCK, HID_KB_LED_SCROLL);

    if(scroll_ok) {
        restore_key(HID_KEYBOARD_SCROLL_LOCK, HID_KB_LED_SCROLL);
        FURI_LOG_I(TAG, "OS detect: ScrollLock echoed → Windows");
        return FPwnOSWindows;
    }

    FURI_LOG_I(TAG, "OS detect: ScrollLock unchanged → Linux");
    return FPwnOSLinux;
}

/* ----------------------------------------------------------------------------
 * fpwn_os_name()
 * --------------------------------------------------------------------------*/
const char* fpwn_os_name(FPwnOS os) {
    switch(os) {
    case FPwnOSWindows:
        return "Windows";
    case FPwnOSMac:
        return "macOS";
    case FPwnOSLinux:
        return "Linux";
    default:
        return "Unknown";
    }
}

/* ============================================================================
 * CDC-based OS detection — sequential fallback when LED heuristics fail
 * ============================================================================
 * Tries each OS (Windows → Linux → Mac) by:
 *   1. Opening a terminal via OS-specific HID keystrokes
 *   2. Typing a detection script that sends "FPWN:WIN/LNX/MAC" + EOT via CDC
 *   3. Switching USB to CDC to receive the response
 *   4. Switching back to HID and cleaning up
 * ========================================================================== */

/* CDC ISR flag — independent from payload_engine's exfil flag */
static volatile bool s_detect_cdc_rx = false;

static void fpwn_detect_cdc_rx_cb(void* context) {
    UNUSED(context);
    __DMB(); /* Ensure DMA writes to SRAM are visible before flag is read */
    s_detect_cdc_rx = true;
}

static void fpwn_detect_cdc_state_cb(void* context, CdcState state) {
    UNUSED(context);
    UNUSED(state);
}

/* HID key press + release helper */
static void press_release(uint16_t key) {
    furi_hal_hid_kb_press(key);
    furi_delay_ms(2);
    furi_hal_hid_kb_release(key);
}

/* HID modifier + key combo */
static void press_release_mod(uint16_t mod_key) {
    furi_hal_hid_kb_press(mod_key);
    furi_delay_ms(2);
    furi_hal_hid_kb_release(mod_key);
}

/**
 * Attempt CDC-based detection for a single candidate OS.
 *
 * Opens a terminal, types a script that sends "FPWN:XXX\x04" over the CDC
 * serial port, switches to CDC to receive, and parses the response.
 *
 * Returns the detected OS, or FPwnOSUnknown if this attempt failed (timeout).
 */
/**
 * Ensure CapsLock is OFF before typing.  If the host CapsLock LED is on,
 * toggle it off so that typed characters have the correct case.
 */
void fpwn_ensure_capslock_off(void) {
    uint8_t leds = furi_hal_hid_get_led_state();
    if(leds & HID_KB_LED_CAPS) {
        FURI_LOG_I(TAG, "CapsLock ON — toggling off before typing");
        furi_hal_hid_kb_press(HID_KEYBOARD_CAPS_LOCK);
        furi_hal_hid_kb_release(HID_KEYBOARD_CAPS_LOCK);
        /* Wait for the LED to clear */
        wait_for_led_change(leds, HID_KB_LED_CAPS, 500);
    }
}

static FPwnOS fpwn_cdc_detect_attempt(FPwnApp* app, FPwnOS candidate) {
    /* Phase 0: Ensure CapsLock is OFF so typed text has correct case */
    fpwn_ensure_capslock_off();

    /* Phase 1: Open a terminal via HID */
    switch(candidate) {
    case FPwnOSWindows:
        /* GUI+R → Run dialog → powershell */
        press_release(KEY_MOD_LEFT_GUI | HID_KEYBOARD_R);
        furi_delay_ms(800);
        fpwn_type_string("powershell -nop -ep bypass");
        press_release(HID_KEYBOARD_RETURN);
        furi_delay_ms(1200);
        break;
    case FPwnOSLinux:
        /* CTRL+ALT+T → terminal */
        press_release_mod(KEY_MOD_LEFT_CTRL | KEY_MOD_LEFT_ALT | HID_KEYBOARD_T);
        furi_delay_ms(1400);
        break;
    case FPwnOSMac:
        /* GUI+SPACE → Spotlight → "Terminal" */
        press_release(KEY_MOD_LEFT_GUI | HID_KEYBOARD_SPACEBAR);
        furi_delay_ms(700);
        fpwn_type_string("Terminal");
        press_release(HID_KEYBOARD_RETURN);
        furi_delay_ms(1400);
        break;
    default:
        return FPwnOSUnknown;
    }

    /* Phase 2: Type the detection script.
     * Each script captures a tag string, snapshots existing serial ports,
     * polls for a NEW port (the Flipper re-enumerating as CDC), writes
     * the tag + EOT, and exits. */
    switch(candidate) {
    case FPwnOSWindows:
        /* Filter for COM ports only — Windows 11 can return device paths
         * like \\?\USB#VID_... that .NET SerialPort rejects with
         * "does not start with COM/com". */
        fpwn_type_string("$_t=[IO.Ports.SerialPort]; "
                         "$_d='FPWN:WIN'; "
                         "$_p=$_t::GetPortNames()|?{$_ -match '^COM\\d+$'}; "
                         "1..20|%{sleep -m 500; "
                         "$_n=$_t::GetPortNames()|?{$_ -match '^COM\\d+$' -and $_ -notin $_p}; "
                         "if($_n){"
                         "try{$_s=$_t::new($_n[0],115200); "
                         "$_s.Open(); "
                         "[byte[]]$_b=[Text.Encoding]::ASCII.GetBytes($_d+[char]4); "
                         "$_s.Write($_b,0,$_b.Length); "
                         "$_s.Close()}catch{}; break}}; exit");
        break;
    case FPwnOSLinux:
        fpwn_type_string("_p=$(ls -1 /dev/ttyACM* 2>/dev/null); "
                         "for _i in $(seq 20); do sleep .5; "
                         "for _v in /dev/ttyACM*; do [ -c \"$_v\" ] || continue; "
                         "echo \"$_p\"|grep -qxF \"$_v\" && continue; "
                         "stty -F \"$_v\" 115200 raw -echo 2>/dev/null && "
                         "{ printf 'FPWN:LNX\\004' > \"$_v\"; break 2; }; "
                         "done; done; exit");
        break;
    case FPwnOSMac:
        fpwn_type_string("_p=$(ls -1 /dev/cu.usbmodem* 2>/dev/null); "
                         "for _i in $(seq 20); do sleep .5; "
                         "for _v in /dev/cu.usbmodem*; do [ -c \"$_v\" ] || continue; "
                         "echo \"$_p\"|grep -qxF \"$_v\" && continue; "
                         "stty -f \"$_v\" 115200 raw 2>/dev/null && "
                         "{ printf 'FPWN:MAC\\004' > \"$_v\"; break 2; }; "
                         "done; done; exit");
        break;
    default:
        break;
    }

    /* Press Enter to execute the script */
    press_release(HID_KEYBOARD_RETURN);

    /* Phase 3: Wait for script to execute and snapshot existing ports.
     * Split into smaller increments so abort is responsive. */
    for(uint32_t w = 0; w < 2000 && !app->abort_requested; w += 100) {
        furi_delay_ms(100);
    }
    if(app->abort_requested) return FPwnOSUnknown;

    /* Phase 4: Switch USB from HID to CDC.
     * Register callbacks BEFORE the profile switch so the first packet
     * from a fast-enumerating host (e.g. Linux with cdc_acm) is not lost. */
    FURI_LOG_I(TAG, "CDC detect: trying %s — switching to CDC", fpwn_os_name(candidate));

    s_detect_cdc_rx = false;

    CdcCallbacks cdc_cb = {
        .tx_ep_callback = NULL,
        .rx_ep_callback = fpwn_detect_cdc_rx_cb,
        .state_callback = fpwn_detect_cdc_state_cb,
        .ctrl_line_callback = NULL,
        .config_callback = NULL,
    };
    furi_hal_cdc_set_callbacks(0, &cdc_cb, NULL);

    furi_hal_usb_unlock();
    furi_hal_usb_set_config(&usb_cdc_single, NULL);
    furi_delay_ms(100);

    /* Phase 5: Receive loop — wait up to 12s for the tag */
    char rxbuf_all[64];
    uint32_t rxpos = 0;
    memset(rxbuf_all, 0, sizeof(rxbuf_all));

    uint32_t rx_start = furi_get_tick();
    const uint32_t rx_timeout_ms = 8000;
    bool got_eot = false;

    while(!got_eot && !app->abort_requested) {
        if(furi_get_tick() - rx_start > furi_ms_to_ticks(rx_timeout_ms)) {
            FURI_LOG_I(TAG, "CDC detect: timeout for %s", fpwn_os_name(candidate));
            break;
        }

        if(s_detect_cdc_rx) {
            s_detect_cdc_rx = false;
            __DMB();

            uint8_t chunk[CDC_DATA_SZ];
            int32_t n;
            while((n = furi_hal_cdc_receive(0, chunk, CDC_DATA_SZ)) > 0 && !got_eot) {
                for(int32_t i = 0; i < n && !got_eot; i++) {
                    if(chunk[i] == 0x04) {
                        got_eot = true;
                    } else if(rxpos < sizeof(rxbuf_all) - 1) {
                        rxbuf_all[rxpos++] = (char)chunk[i];
                    }
                }
            }
        }
        furi_delay_ms(5);
    }
    rxbuf_all[rxpos] = '\0';

    /* Unregister CDC callbacks */
    furi_hal_cdc_set_callbacks(0, NULL, NULL);

    /* Phase 6: Switch back to HID */
    furi_hal_usb_unlock();
    furi_hal_usb_set_config(&usb_hid, NULL);
    /* HID re-enumeration — split into small increments so abort is responsive */
    for(uint32_t w = 0; w < 2000 && !app->abort_requested; w += 100) {
        furi_delay_ms(100);
    }

    /* Phase 7: Parse the response.
     * Case-insensitive matching — if CapsLock was on during typing, the
     * tag string is case-inverted ('fpwn:win' instead of 'FPWN:WIN'). */
    FPwnOS result = FPwnOSUnknown;
    if(got_eot && rxpos > 0) {
        /* Normalize to uppercase for matching */
        for(uint32_t i = 0; i < rxpos; i++) {
            if(rxbuf_all[i] >= 'a' && rxbuf_all[i] <= 'z') {
                rxbuf_all[i] -= 32;
            }
        }
        if(strstr(rxbuf_all, "FPWN:WIN"))
            result = FPwnOSWindows;
        else if(strstr(rxbuf_all, "FPWN:LNX"))
            result = FPwnOSLinux;
        else if(strstr(rxbuf_all, "FPWN:MAC"))
            result = FPwnOSMac;
        FURI_LOG_I(TAG, "CDC detect: received '%s' → %s", rxbuf_all, fpwn_os_name(result));
    }

    /* Phase 8: Cleanup — close the terminal window after a failed attempt.
     * Wait for HID to be ready before sending cleanup keystrokes. */
    if(result == FPwnOSUnknown) {
        /* Probe CapsLock to confirm HID is responsive (up to 1s extra) */
        bool hid_ready = toggle_key_and_check(HID_KEYBOARD_CAPS_LOCK, HID_KB_LED_CAPS);
        if(hid_ready) {
            restore_key(HID_KEYBOARD_CAPS_LOCK, HID_KB_LED_CAPS);
        } else {
            furi_delay_ms(1000); /* Extra wait for slow hosts */
        }
        /* CTRL+C to kill any hung command, then close the terminal.
         * Escape alone doesn't close PowerShell/Terminal — must type "exit"
         * and also send ALT+F4 to close the window. */
        press_release_mod(KEY_MOD_LEFT_CTRL | HID_KEYBOARD_C);
        furi_delay_ms(300);
        fpwn_type_string("exit");
        press_release(HID_KEYBOARD_RETURN);
        furi_delay_ms(500);
        /* ALT+F4 as a fallback to close any remaining window */
        press_release_mod(KEY_MOD_LEFT_ALT | HID_KEYBOARD_F4);
        furi_delay_ms(500);
    }

    return result;
}

/* ----------------------------------------------------------------------------
 * fpwn_os_detect_cdc()
 *
 * Enhanced OS detection: LED heuristics first, CDC serial fallback.
 *
 * 1. Calls fpwn_os_detect() — the fast LED-based method (~1.5s).
 * 2. If that returns a concrete OS, use it immediately.
 * 3. If Unknown, sequentially try CDC detection for Windows → Linux → Mac.
 *
 * Updates app->detected_os with the result.
 * --------------------------------------------------------------------------*/
FPwnOS fpwn_os_detect_cdc(FPwnApp* app) {
    /* Fast path: LED heuristics */
    FPwnOS result = fpwn_os_detect();

    if(result != FPwnOSUnknown) {
        FURI_LOG_I(TAG, "OS detect (LED): %s", fpwn_os_name(result));
        app->detected_os = result;
        return result;
    }

    FURI_LOG_I(TAG, "LED detection failed, falling back to CDC serial");

    /* Update the execute view status if available */
    if(app->execute_view) {
        with_view_model(
            app->execute_view,
            FPwnExecModel * m,
            { strncpy(m->status, "CDC OS detect...", sizeof(m->status) - 1); },
            true);
    }

    /* Try each OS in order: Windows → Linux → Mac */
    static const FPwnOS candidates[] = {FPwnOSWindows, FPwnOSLinux, FPwnOSMac};
    for(uint8_t ci = 0; ci < 3; ci++) {
        if(app->abort_requested) break;

        result = fpwn_cdc_detect_attempt(app, candidates[ci]);
        if(result != FPwnOSUnknown) {
            FURI_LOG_I(TAG, "OS detect (CDC): %s", fpwn_os_name(result));
            app->detected_os = result;
            return result;
        }
    }

    FURI_LOG_W(TAG, "OS detect: all methods failed");
    app->detected_os = FPwnOSUnknown;
    return FPwnOSUnknown;
}
