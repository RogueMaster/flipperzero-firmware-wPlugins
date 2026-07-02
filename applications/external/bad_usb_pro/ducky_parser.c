/* ducky_parser.c — Extended DuckyScript parser for BadUSB Pro.
 * Supports OS_DETECT, IF/END_IF, FUNCTION/END_FUNCTION/CALL, VAR, LED_CHECK,
 * CONSUMER_KEY, and template variable substitution. */

#include "ducky_parser.h"
#include <furi_hal_usb_hid.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* strtok_r replacement – not available in Flipper SDK */
static char* my_strtok_r(char* str, const char* delim, char** saveptr) {
    char* s = str ? str : *saveptr;
    if(!s) return NULL;
    /* Skip leading delimiters */
    while(*s && strchr(delim, *s))
        s++;
    if(*s == '\0') {
        *saveptr = NULL;
        return NULL;
    }
    char* tok = s;
    while(*s && !strchr(delim, *s))
        s++;
    if(*s) {
        *s = '\0';
        s++;
    }
    *saveptr = s;
    return tok;
}

/* ════════════════════════════════════════════════════════════════════════════
 *  Key-name → HID-keycode lookup table
 * ════════════════════════════════════════════════════════════════════════════ */
typedef struct {
    const char* name;
    uint16_t keycode;
} KeyMapping;

static const KeyMapping key_map[] = {
    /* Navigation / editing */
    {"ENTER", HID_KEYBOARD_RETURN},
    {"RETURN", HID_KEYBOARD_RETURN},
    {"TAB", HID_KEYBOARD_TAB},
    {"ESCAPE", HID_KEYBOARD_ESCAPE},
    {"ESC", HID_KEYBOARD_ESCAPE},
    {"SPACE", HID_KEYBOARD_SPACEBAR},
    {"BACKSPACE", HID_KEYBOARD_DELETE},
    {"DELETE", HID_KEYBOARD_DELETE_FORWARD},
    {"DEL", HID_KEYBOARD_DELETE_FORWARD},
    {"HOME", HID_KEYBOARD_HOME},
    {"END", HID_KEYBOARD_END},
    {"INSERT", HID_KEYBOARD_INSERT},
    {"PAGEUP", HID_KEYBOARD_PAGE_UP},
    {"PAGE_UP", HID_KEYBOARD_PAGE_UP},
    {"PAGEDOWN", HID_KEYBOARD_PAGE_DOWN},
    {"PAGE_DOWN", HID_KEYBOARD_PAGE_DOWN},
    {"UPARROW", HID_KEYBOARD_UP_ARROW},
    {"UP", HID_KEYBOARD_UP_ARROW},
    {"DOWNARROW", HID_KEYBOARD_DOWN_ARROW},
    {"DOWN", HID_KEYBOARD_DOWN_ARROW},
    {"LEFTARROW", HID_KEYBOARD_LEFT_ARROW},
    {"LEFT", HID_KEYBOARD_LEFT_ARROW},
    {"RIGHTARROW", HID_KEYBOARD_RIGHT_ARROW},
    {"RIGHT", HID_KEYBOARD_RIGHT_ARROW},
    {"PRINTSCREEN", HID_KEYBOARD_PRINT_SCREEN},
    {"PAUSE", HID_KEYBOARD_PAUSE},
    {"BREAK", HID_KEYBOARD_PAUSE},
    {"CAPSLOCK", HID_KEYBOARD_CAPS_LOCK},
    {"CAPS_LOCK", HID_KEYBOARD_CAPS_LOCK},
    {"NUMLOCK", HID_KEYPAD_NUMLOCK},
    {"NUM_LOCK", HID_KEYPAD_NUMLOCK},
    {"SCROLLLOCK", HID_KEYBOARD_SCROLL_LOCK},
    {"SCROLL_LOCK", HID_KEYBOARD_SCROLL_LOCK},
    {"MENU", HID_KEYBOARD_APPLICATION},
    {"APP", HID_KEYBOARD_APPLICATION},

    /* Function keys */
    {"F1", HID_KEYBOARD_F1},
    {"F2", HID_KEYBOARD_F2},
    {"F3", HID_KEYBOARD_F3},
    {"F4", HID_KEYBOARD_F4},
    {"F5", HID_KEYBOARD_F5},
    {"F6", HID_KEYBOARD_F6},
    {"F7", HID_KEYBOARD_F7},
    {"F8", HID_KEYBOARD_F8},
    {"F9", HID_KEYBOARD_F9},
    {"F10", HID_KEYBOARD_F10},
    {"F11", HID_KEYBOARD_F11},
    {"F12", HID_KEYBOARD_F12},

    /* Modifiers as standalone keys — use KEY_MOD_* so the modifier byte is set */
    {"GUI", KEY_MOD_LEFT_GUI},
    {"WINDOWS", KEY_MOD_LEFT_GUI},
    {"COMMAND", KEY_MOD_LEFT_GUI},
    {"ALT", KEY_MOD_LEFT_ALT},
    {"CTRL", KEY_MOD_LEFT_CTRL},
    {"CONTROL", KEY_MOD_LEFT_CTRL},
    {"SHIFT", KEY_MOD_LEFT_SHIFT},
};

static const size_t key_map_size = sizeof(key_map) / sizeof(key_map[0]);

/* Modifier name → HID modifier bit */
typedef struct {
    const char* name;
    uint16_t mod_keycode;
} ModMapping;

static const ModMapping mod_map[] = {
    {"CTRL", KEY_MOD_LEFT_CTRL},
    {"CONTROL", KEY_MOD_LEFT_CTRL},
    {"ALT", KEY_MOD_LEFT_ALT},
    {"SHIFT", KEY_MOD_LEFT_SHIFT},
    {"GUI", KEY_MOD_LEFT_GUI},
    {"WINDOWS", KEY_MOD_LEFT_GUI},
    {"COMMAND", KEY_MOD_LEFT_GUI},
};
static const size_t mod_map_size = sizeof(mod_map) / sizeof(mod_map[0]);

/* ════════════════════════════════════════════════════════════════════════════
 *  Helpers
 * ════════════════════════════════════════════════════════════════════════════ */

/** Case-insensitive string compare (public, shared with script_engine.c) */
int ducky_strcicmp(const char* a, const char* b) {
    while(*a && *b) {
        int d = toupper((unsigned char)*a) - toupper((unsigned char)*b);
        if(d != 0) return d;
        a++;
        b++;
    }
    return toupper((unsigned char)*a) - toupper((unsigned char)*b);
}

/** Skip leading whitespace, return pointer into same buffer (public, shared) */
const char* ducky_skip_ws(const char* s) {
    while(*s == ' ' || *s == '\t')
        s++;
    return s;
}

/** Strip trailing whitespace / CR / LF in-place (public, shared) */
void ducky_strip_trailing(char* s) {
    size_t len = strlen(s);
    while(len > 0 &&
          (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\r' || s[len - 1] == '\n')) {
        s[--len] = '\0';
    }
}

/** Check if a word is a modifier name */
static bool is_modifier(const char* word) {
    for(size_t i = 0; i < mod_map_size; i++) {
        if(ducky_strcicmp(word, mod_map[i].name) == 0) return true;
    }
    return false;
}

/** Look up modifier keycode */
static uint16_t modifier_keycode(const char* word) {
    for(size_t i = 0; i < mod_map_size; i++) {
        if(ducky_strcicmp(word, mod_map[i].name) == 0) return mod_map[i].mod_keycode;
    }
    return 0;
}

/* ════════════════════════════════════════════════════════════════════════════
 *  ASCII → HID keycode (for STRING command)
 *  Characters that require Shift have bit 0x8000 set.
 * ════════════════════════════════════════════════════════════════════════════ */

/* Map printable ASCII 0x20..0x7E to HID keycodes.
 * Bit 15 (0x8000) means "hold Shift". */
const uint16_t badusb_ascii_to_hid[95] = {
    /* 0x20 ' ' */ HID_KEYBOARD_SPACEBAR,
    /* 0x21 '!' */ HID_KEYBOARD_1 | 0x8000,
    /* 0x22 '"' */ HID_KEYBOARD_APOSTROPHE | 0x8000,
    /* 0x23 '#' */ HID_KEYBOARD_3 | 0x8000,
    /* 0x24 '$' */ HID_KEYBOARD_4 | 0x8000,
    /* 0x25 '%' */ HID_KEYBOARD_5 | 0x8000,
    /* 0x26 '&' */ HID_KEYBOARD_7 | 0x8000,
    /* 0x27 '\''*/ HID_KEYBOARD_APOSTROPHE,
    /* 0x28 '(' */ HID_KEYBOARD_9 | 0x8000,
    /* 0x29 ')' */ HID_KEYBOARD_0 | 0x8000,
    /* 0x2A '*' */ HID_KEYBOARD_8 | 0x8000,
    /* 0x2B '+' */ HID_KEYBOARD_EQUAL_SIGN | 0x8000,
    /* 0x2C ',' */ HID_KEYBOARD_COMMA,
    /* 0x2D '-' */ HID_KEYBOARD_MINUS,
    /* 0x2E '.' */ HID_KEYBOARD_DOT,
    /* 0x2F '/' */ HID_KEYBOARD_SLASH,
    /* 0x30-0x39 '0'-'9' */
    HID_KEYBOARD_0,
    HID_KEYBOARD_1,
    HID_KEYBOARD_2,
    HID_KEYBOARD_3,
    HID_KEYBOARD_4,
    HID_KEYBOARD_5,
    HID_KEYBOARD_6,
    HID_KEYBOARD_7,
    HID_KEYBOARD_8,
    HID_KEYBOARD_9,
    /* 0x3A ':' */ HID_KEYBOARD_SEMICOLON | 0x8000,
    /* 0x3B ';' */ HID_KEYBOARD_SEMICOLON,
    /* 0x3C '<' */ HID_KEYBOARD_COMMA | 0x8000,
    /* 0x3D '=' */ HID_KEYBOARD_EQUAL_SIGN,
    /* 0x3E '>' */ HID_KEYBOARD_DOT | 0x8000,
    /* 0x3F '?' */ HID_KEYBOARD_SLASH | 0x8000,
    /* 0x40 '@' */ HID_KEYBOARD_2 | 0x8000,
    /* 0x41-0x5A 'A'-'Z' */
    HID_KEYBOARD_A | 0x8000,
    HID_KEYBOARD_B | 0x8000,
    HID_KEYBOARD_C | 0x8000,
    HID_KEYBOARD_D | 0x8000,
    HID_KEYBOARD_E | 0x8000,
    HID_KEYBOARD_F | 0x8000,
    HID_KEYBOARD_G | 0x8000,
    HID_KEYBOARD_H | 0x8000,
    HID_KEYBOARD_I | 0x8000,
    HID_KEYBOARD_J | 0x8000,
    HID_KEYBOARD_K | 0x8000,
    HID_KEYBOARD_L | 0x8000,
    HID_KEYBOARD_M | 0x8000,
    HID_KEYBOARD_N | 0x8000,
    HID_KEYBOARD_O | 0x8000,
    HID_KEYBOARD_P | 0x8000,
    HID_KEYBOARD_Q | 0x8000,
    HID_KEYBOARD_R | 0x8000,
    HID_KEYBOARD_S | 0x8000,
    HID_KEYBOARD_T | 0x8000,
    HID_KEYBOARD_U | 0x8000,
    HID_KEYBOARD_V | 0x8000,
    HID_KEYBOARD_W | 0x8000,
    HID_KEYBOARD_X | 0x8000,
    HID_KEYBOARD_Y | 0x8000,
    HID_KEYBOARD_Z | 0x8000,
    /* 0x5B '[' */ HID_KEYBOARD_OPEN_BRACKET,
    /* 0x5C '\\' */ HID_KEYBOARD_BACKSLASH,
    /* 0x5D ']' */ HID_KEYBOARD_CLOSE_BRACKET,
    /* 0x5E '^' */ HID_KEYBOARD_6 | 0x8000,
    /* 0x5F '_' */ HID_KEYBOARD_MINUS | 0x8000,
    /* 0x60 '`' */ HID_KEYBOARD_GRAVE_ACCENT,
    /* 0x61-0x7A 'a'-'z' */
    HID_KEYBOARD_A,
    HID_KEYBOARD_B,
    HID_KEYBOARD_C,
    HID_KEYBOARD_D,
    HID_KEYBOARD_E,
    HID_KEYBOARD_F,
    HID_KEYBOARD_G,
    HID_KEYBOARD_H,
    HID_KEYBOARD_I,
    HID_KEYBOARD_J,
    HID_KEYBOARD_K,
    HID_KEYBOARD_L,
    HID_KEYBOARD_M,
    HID_KEYBOARD_N,
    HID_KEYBOARD_O,
    HID_KEYBOARD_P,
    HID_KEYBOARD_Q,
    HID_KEYBOARD_R,
    HID_KEYBOARD_S,
    HID_KEYBOARD_T,
    HID_KEYBOARD_U,
    HID_KEYBOARD_V,
    HID_KEYBOARD_W,
    HID_KEYBOARD_X,
    HID_KEYBOARD_Y,
    HID_KEYBOARD_Z,
    /* 0x7B '{' */ HID_KEYBOARD_OPEN_BRACKET | 0x8000,
    /* 0x7C '|' */ HID_KEYBOARD_BACKSLASH | 0x8000,
    /* 0x7D '}' */ HID_KEYBOARD_CLOSE_BRACKET | 0x8000,
    /* 0x7E '~' */ HID_KEYBOARD_GRAVE_ACCENT | 0x8000,
};

/* ════════════════════════════════════════════════════════════════════════════
 *  Public: resolve a key name to its HID keycode
 * ════════════════════════════════════════════════════════════════════════════ */
uint16_t ducky_parser_resolve_keyname(const char* name) {
    /* Check named keys first */
    for(size_t i = 0; i < key_map_size; i++) {
        if(ducky_strcicmp(name, key_map[i].name) == 0) {
            return key_map[i].keycode;
        }
    }

    /* Single printable ASCII character */
    if(strlen(name) == 1) {
        unsigned char ch = (unsigned char)name[0];
        if(ch >= 0x20 && ch <= 0x7E) {
            return badusb_ascii_to_hid[ch - 0x20];
        }
    }

    return 0;
}

/* ════════════════════════════════════════════════════════════════════════════
 *  Token for a combo line like "CTRL ALT DELETE" or "GUI r"
 *  Returns true if the line looks like a combo (starts with a modifier).
 * ════════════════════════════════════════════════════════════════════════════ */
static bool parse_key_combo(const char* line, ScriptToken* token, char* err_msg) {
    char buf[BADUSB_PRO_MAX_LINE_LEN];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    /* Tokenise by spaces */
    char* words[8];
    uint8_t word_count = 0;

    char* saveptr = NULL;
    char* w = my_strtok_r(buf, " \t", &saveptr);
    while(w && word_count < 8) {
        words[word_count++] = w;
        w = my_strtok_r(NULL, " \t", &saveptr);
    }

    if(word_count == 0) return false;

    /* First word must be a modifier for this to be treated as a combo */
    if(!is_modifier(words[0])) return false;

    /* If it's a single modifier with no other key, treat as a standalone key press */
    if(word_count == 1) {
        token->type = TokenKey;
        token->keycodes[0] = modifier_keycode(words[0]);
        token->keycode_count = 1;
        return true;
    }

    /* Build combo: all modifiers + last word is the actual key */
    token->type = TokenKeyCombo;
    token->keycode_count = 0;

    for(uint8_t i = 0; i < word_count; i++) {
        if(i < word_count - 1) {
            /* Should be a modifier */
            uint16_t mk = modifier_keycode(words[i]);
            if(mk == 0) {
                /* Not a modifier — try as key name */
                mk = ducky_parser_resolve_keyname(words[i]);
            }
            if(mk == 0) {
                snprintf(err_msg, 128, "Unknown key: %s", words[i]);
                return false;
            }
            token->keycodes[token->keycode_count++] = mk;
        } else {
            /* Last word: regular key */
            uint16_t kc = ducky_parser_resolve_keyname(words[i]);
            if(kc == 0) {
                snprintf(err_msg, 128, "Unknown key: %s", words[i]);
                return false;
            }
            token->keycodes[token->keycode_count++] = kc;
        }
    }

    return true;
}

/* ════════════════════════════════════════════════════════════════════════════
 *  Parse a single line
 * ════════════════════════════════════════════════════════════════════════════ */
bool ducky_parser_parse_line(const char* raw_line, ScriptToken* token, char* err_msg) {
    memset(token, 0, sizeof(*token));

    char line[BADUSB_PRO_MAX_LINE_LEN];
    strncpy(line, raw_line, sizeof(line) - 1);
    line[sizeof(line) - 1] = '\0';
    ducky_strip_trailing(line);

    const char* p = ducky_skip_ws(line);

    /* Empty line or comment */
    if(*p == '\0') {
        token->type = TokenRem;
        return true;
    }

    /* ── REM ─────────────────────────────────────────────── */
    if(strncmp(p, "REM ", 4) == 0 || strcmp(p, "REM") == 0) {
        token->type = TokenRem;
        if(strlen(p) > 4) {
            strncpy(token->str_value, p + 4, sizeof(token->str_value) - 1);
        }
        return true;
    }

    /* ── STRING / STRINGLN ───────────────────────────────── */
    if(strncmp(p, "STRINGLN ", 9) == 0 || strcmp(p, "STRINGLN") == 0) {
        token->type = TokenStringLn;
        if(strlen(p) > 9) {
            strncpy(token->str_value, p + 9, sizeof(token->str_value) - 1);
        }
        return true;
    }
    if(strncmp(p, "STRING ", 7) == 0 || strcmp(p, "STRING") == 0) {
        token->type = TokenString;
        if(strlen(p) > 7) {
            strncpy(token->str_value, p + 7, sizeof(token->str_value) - 1);
        }
        return true;
    }

    /* ── DELAY ───────────────────────────────────────────── */
    if(strncmp(p, "DELAY ", 6) == 0) {
        token->type = TokenDelay;
        token->int_value = atoi(p + 6);
        if(token->int_value < 0) token->int_value = 0;
        return true;
    }

    /* ── DEFAULT_DELAY ───────────────────────────────────── */
    if(strncmp(p, "DEFAULT_DELAY ", 14) == 0 || strncmp(p, "DEFAULTDELAY ", 13) == 0) {
        token->type = TokenDefaultDelay;
        const char* val = (p[7] == '_') ? p + 14 : p + 13;
        token->int_value = atoi(val);
        return true;
    }

    /* ── DEFAULT_STRING_DELAY ────────────────────────────── */
    if(strncmp(p, "DEFAULT_STRING_DELAY ", 21) == 0) {
        token->type = TokenDefaultStringDelay;
        token->int_value = atoi(p + 21);
        return true;
    }

    /* ── REPEAT ──────────────────────────────────────────── */
    if(strncmp(p, "REPEAT ", 7) == 0) {
        token->type = TokenRepeat;
        token->int_value = atoi(p + 7);
        if(token->int_value <= 0) token->int_value = 1;
        return true;
    }

    /* ── STOP ────────────────────────────────────────────── */
    if(strcmp(p, "STOP") == 0) {
        token->type = TokenStop;
        return true;
    }

    /* ── RESTART ─────────────────────────────────────────── */
    if(strcmp(p, "RESTART") == 0) {
        token->type = TokenRestart;
        return true;
    }

    /* ── IF / ELSE / END_IF ──────────────────────────────── */
    if(strncmp(p, "IF ", 3) == 0) {
        token->type = TokenIf;
        strncpy(token->str_value, p + 3, sizeof(token->str_value) - 1);
        return true;
    }
    if(strcmp(p, "ELSE") == 0) {
        token->type = TokenElse;
        return true;
    }
    if(strcmp(p, "END_IF") == 0) {
        token->type = TokenEndIf;
        return true;
    }

    /* ── WHILE / END_WHILE ───────────────────────────────── */
    if(strncmp(p, "WHILE ", 6) == 0) {
        token->type = TokenWhile;
        strncpy(token->str_value, p + 6, sizeof(token->str_value) - 1);
        return true;
    }
    if(strcmp(p, "END_WHILE") == 0) {
        token->type = TokenEndWhile;
        return true;
    }

    /* ── VAR ─────────────────────────────────────────────── */
    if(strncmp(p, "VAR ", 4) == 0) {
        token->type = TokenVar;
        strncpy(token->str_value, p + 4, sizeof(token->str_value) - 1);
        return true;
    }

    /* ── FUNCTION / END_FUNCTION / CALL ──────────────────── */
    if(strncmp(p, "FUNCTION ", 9) == 0) {
        token->type = TokenFunction;
        strncpy(token->str_value, p + 9, sizeof(token->str_value) - 1);
        return true;
    }
    if(strcmp(p, "END_FUNCTION") == 0) {
        token->type = TokenEndFunction;
        return true;
    }
    if(strncmp(p, "CALL ", 5) == 0) {
        token->type = TokenCall;
        strncpy(token->str_value, p + 5, sizeof(token->str_value) - 1);
        return true;
    }

    /* ── LED_CHECK / LED_WAIT ────────────────────────────── */
    if(strncmp(p, "LED_CHECK ", 10) == 0) {
        token->type = TokenLedCheck;
        strncpy(token->str_value, p + 10, sizeof(token->str_value) - 1);
        return true;
    }
    if(strncmp(p, "LED_WAIT ", 9) == 0) {
        token->type = TokenLedWait;
        strncpy(token->str_value, p + 9, sizeof(token->str_value) - 1);
        return true;
    }

    /* ── OS_DETECT ───────────────────────────────────────── */
    if(strcmp(p, "OS_DETECT") == 0) {
        token->type = TokenOsDetect;
        return true;
    }

    /* ── MOUSE commands ──────────────────────────────────── */
    if(strncmp(p, "MOUSE_MOVE ", 11) == 0) {
        token->type = TokenMouseMove;
        const char* args = p + 11;
        /* Parse "x y" and clamp to int8_t range */
        char* end = NULL;
        int32_t x = (int32_t)strtol(args, &end, 10);
        int32_t y = 0;
        if(end && *end) {
            y = (int32_t)strtol(ducky_skip_ws(end), NULL, 10);
        }
        if(x < INT8_MIN) x = INT8_MIN;
        if(x > INT8_MAX) x = INT8_MAX;
        if(y < INT8_MIN) y = INT8_MIN;
        if(y > INT8_MAX) y = INT8_MAX;
        token->int_value = x;
        token->int_value2 = y;
        return true;
    }
    if(strncmp(p, "MOUSE_CLICK ", 12) == 0) {
        token->type = TokenMouseClick;
        strncpy(token->str_value, p + 12, sizeof(token->str_value) - 1);
        return true;
    }
    if(strncmp(p, "MOUSE_SCROLL ", 13) == 0) {
        token->type = TokenMouseScroll;
        int32_t scroll = (int32_t)strtol(p + 13, NULL, 10);
        if(scroll < INT8_MIN) scroll = INT8_MIN;
        if(scroll > INT8_MAX) scroll = INT8_MAX;
        token->int_value = scroll;
        return true;
    }

    /* ── CONSUMER_KEY ────────────────────────────────────── */
    if(strncmp(p, "CONSUMER_KEY ", 13) == 0) {
        token->type = TokenConsumerKey;
        strncpy(token->str_value, p + 13, sizeof(token->str_value) - 1);
        return true;
    }

    /* ── Bare key names / key combos ─────────────────────── */
    /* Try to parse as key combo first (starts with modifier) */
    if(parse_key_combo(p, token, err_msg)) {
        return true;
    }

    /* Try as a single key name */
    uint16_t kc = ducky_parser_resolve_keyname(p);
    if(kc != 0) {
        token->type = TokenKey;
        token->keycodes[0] = kc;
        token->keycode_count = 1;
        strncpy(token->str_value, p, sizeof(token->str_value) - 1);
        return true;
    }

    /* Unknown command */
    snprintf(err_msg, 128, "Unknown command: %.60s", p);
    return false;
}

/* ════════════════════════════════════════════════════════════════════════════
 *  Parse an entire file
 * ════════════════════════════════════════════════════════════════════════════ */
bool ducky_parser_parse_file(
    Storage* storage,
    const char* path,
    ScriptToken* tokens,
    uint16_t max_tokens,
    uint16_t* out_count,
    char* err_msg,
    uint16_t* err_line) {
    *out_count = 0;
    *err_line = 0;
    err_msg[0] = '\0';

    Stream* stream = file_stream_alloc(storage);
    if(!file_stream_open(stream, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        strncpy(err_msg, "Cannot open file", 128);
        stream_free(stream);
        return false;
    }

    FuriString* line_buf = furi_string_alloc();
    uint16_t line_no = 0;
    bool success = true;

    while(stream_read_line(stream, line_buf)) {
        line_no++;
        const char* line_cstr = furi_string_get_cstr(line_buf);

        /* Skip empty lines silently */
        char trimmed[BADUSB_PRO_MAX_LINE_LEN];
        strncpy(trimmed, line_cstr, sizeof(trimmed) - 1);
        trimmed[sizeof(trimmed) - 1] = '\0';
        ducky_strip_trailing(trimmed);
        const char* tp = ducky_skip_ws(trimmed);
        if(*tp == '\0') continue;

        if(*out_count >= max_tokens) {
            snprintf(err_msg, 128, "Too many commands (max %d)", max_tokens);
            *err_line = line_no;
            success = false;
            break;
        }

        ScriptToken* tok = &tokens[*out_count];
        char parse_err[128] = {0};

        if(!ducky_parser_parse_line(line_cstr, tok, parse_err)) {
            strncpy(err_msg, parse_err, 128);
            *err_line = line_no;
            success = false;
            break;
        }

        tok->source_line = line_no;

        /* Skip pure comments in token stream */
        if(tok->type == TokenRem) continue;

        (*out_count)++;
    }

    furi_string_free(line_buf);
    stream_free(stream);
    return success;
}

/* ════════════════════════════════════════════════════════════════════════════
 *  Count lines in a file
 * ════════════════════════════════════════════════════════════════════════════ */
uint16_t ducky_parser_count_lines(Storage* storage, const char* path) {
    Stream* stream = file_stream_alloc(storage);
    if(!file_stream_open(stream, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        stream_free(stream);
        return 0;
    }

    uint16_t count = 0;
    FuriString* line_buf = furi_string_alloc();
    while(stream_read_line(stream, line_buf)) {
        count++;
    }
    furi_string_free(line_buf);
    stream_free(stream);
    return count;
}
