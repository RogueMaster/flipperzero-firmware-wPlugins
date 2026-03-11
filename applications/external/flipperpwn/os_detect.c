#include "flipperpwn.h"

#define TAG "FPwn"

/* Poll timeout in ms — how long to wait for the host to echo an LED change. */
#define LED_POLL_TIMEOUT_MS 500

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
    /* Phase 0: CapsLock connectivity check */
    bool caps_ok = toggle_key_and_check(HID_KEYBOARD_CAPS_LOCK, HID_KB_LED_CAPS);

    if(!caps_ok) {
        FURI_LOG_I(TAG, "OS detect: no CapsLock echo — USB HID not active");
        /* CapsLock was sent but host didn't echo.  Toggle it back so the
         * host state is restored even if the LED report was simply slow. */
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
