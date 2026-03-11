#include "script_engine.h"
#include "ducky_parser.h"
#include <furi_hal_usb_hid.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ════════════════════════════════════════════════════════════════════════════
 *  Consumer key name → HID consumer usage ID mapping
 * ════════════════════════════════════════════════════════════════════════════ */
typedef struct {
    const char* name;
    uint16_t usage_id;
} ConsumerKeyMapping;

static const ConsumerKeyMapping consumer_key_map[] = {
    /* Media transport */
    {"PLAY", 0xB0},
    {"PAUSE", 0xB1},
    {"PLAY_PAUSE", 0xCD},
    {"STOP", 0xB7},
    {"RECORD", 0xB2},
    {"NEXT_TRACK", 0xB5},
    {"PREV_TRACK", 0xB6},
    {"PREVIOUS_TRACK", 0xB6},
    {"FAST_FORWARD", 0xB3},
    {"FF", 0xB3},
    {"REWIND", 0xB4},
    {"RW", 0xB4},
    {"EJECT", 0xB8},
    {"RANDOM_PLAY", 0xB9},
    {"REPEAT", 0xBC},
    /* Volume */
    {"VOLUME_UP", 0xE9},
    {"VOL_UP", 0xE9},
    {"VOLUME_DOWN", 0xEA},
    {"VOL_DOWN", 0xEA},
    {"MUTE", 0xE2},
    {"BASS_BOOST", 0xE5},
    /* Power */
    {"POWER", 0x30},
    {"SLEEP", 0x32},
    /* Navigation */
    {"MENU", 0x40},
    {"MENU_PICK", 0x41},
    {"MENU_UP", 0x42},
    {"MENU_DOWN", 0x43},
    {"MENU_LEFT", 0x44},
    {"MENU_RIGHT", 0x45},
    {"MENU_ESCAPE", 0x46},
    /* App launchers */
    {"EMAIL", 0x18A},
    {"CALCULATOR", 0x192},
    {"MY_COMPUTER", 0x194},
    {"EXPLORER", 0x194},
    {"BROWSER", 0x196},
    {"INTERNET", 0x196},
    /* Application controls */
    {"AC_SEARCH", 0x221},
    {"AC_HOME", 0x223},
    {"AC_BACK", 0x224},
    {"AC_FORWARD", 0x225},
    {"AC_STOP", 0x226},
    {"AC_REFRESH", 0x227},
    {"AC_BOOKMARKS", 0x22A},
    {"AC_ZOOM_IN", 0x22D},
    {"AC_ZOOM_OUT", 0x22E},
    /* Browser aliases */
    {"BROWSER_HOME", 0x223},
    {"BROWSER_BACK", 0x224},
    {"BROWSER_FORWARD", 0x225},
    {"BROWSER_STOP", 0x226},
    {"BROWSER_REFRESH", 0x227},
    {"BROWSER_SEARCH", 0x221},
    {"BROWSER_BOOKMARKS", 0x22A},
    {"BROWSER_FAVORITES", 0x22A},
    /* Misc */
    {"SNAPSHOT", 0x65},
};

static const size_t consumer_key_map_size = sizeof(consumer_key_map) / sizeof(consumer_key_map[0]);

static uint16_t resolve_consumer_key(const char* name) {
    const char* trimmed = ducky_skip_ws(name);
    for(size_t i = 0; i < consumer_key_map_size; i++) {
        if(ducky_strcicmp(trimmed, consumer_key_map[i].name) == 0) {
            return consumer_key_map[i].usage_id;
        }
    }
    /* Support raw hex values (e.g. "0xCD") */
    if(trimmed[0] == '0' && (trimmed[1] == 'x' || trimmed[1] == 'X')) {
        return (uint16_t)strtol(trimmed, NULL, 16);
    }
    return 0;
}

/* strtok_r replacement – not available in Flipper SDK */
static char* my_strtok_r(char* str, const char* delim, char** saveptr) {
    char* s = str ? str : *saveptr;
    if(!s) return NULL;
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
 *  LED bitmask definitions (from furi_hal_hid_get_led_state)
 * ════════════════════════════════════════════════════════════════════════════ */
#define LED_NUM_LOCK_BIT    (1 << 0)
#define LED_CAPS_LOCK_BIT   (1 << 1)
#define LED_SCROLL_LOCK_BIT (1 << 2)

/* ════════════════════════════════════════════════════════════════════════════
 *  Mouse button values
 * ════════════════════════════════════════════════════════════════════════════ */
#define MOUSE_BTN_LEFT   (1 << 0)
#define MOUSE_BTN_RIGHT  (1 << 1)
#define MOUSE_BTN_MIDDLE (1 << 2)

/* ════════════════════════════════════════════════════════════════════════════
 *  Helpers
 * ════════════════════════════════════════════════════════════════════════════ */

/** Apply speed multiplier to a delay in ms */
static uint32_t adjusted_delay(ScriptEngine* engine, uint32_t ms) {
    if(engine->speed_multiplier <= 0.0f) return ms;
    float result = (float)ms / engine->speed_multiplier;
    if(result < 1.0f) result = 1.0f;
    return (uint32_t)result;
}

/** Notify the UI callback */
static void notify_ui(ScriptEngine* engine) {
    if(engine->status_callback) {
        engine->status_callback(engine->callback_ctx);
    }
}

/* ════════════════════════════════════════════════════════════════════════════
 *  Variable helpers
 * ════════════════════════════════════════════════════════════════════════════ */

const char* script_engine_get_var(ScriptEngine* engine, const char* name) {
    for(uint8_t i = 0; i < engine->var_count; i++) {
        if(strcmp(engine->vars[i].name, name) == 0) {
            return engine->vars[i].value;
        }
    }
    return NULL;
}

bool script_engine_set_var(ScriptEngine* engine, const char* name, const char* value) {
    /* Update existing */
    for(uint8_t i = 0; i < engine->var_count; i++) {
        if(strcmp(engine->vars[i].name, name) == 0) {
            strncpy(engine->vars[i].value, value, BADUSB_PRO_VAR_VAL_LEN - 1);
            engine->vars[i].value[BADUSB_PRO_VAR_VAL_LEN - 1] = '\0';
            return true;
        }
    }
    /* Create new */
    if(engine->var_count >= BADUSB_PRO_MAX_VARS) return false;
    strncpy(engine->vars[engine->var_count].name, name, BADUSB_PRO_VAR_NAME_LEN - 1);
    engine->vars[engine->var_count].name[BADUSB_PRO_VAR_NAME_LEN - 1] = '\0';
    strncpy(engine->vars[engine->var_count].value, value, BADUSB_PRO_VAR_VAL_LEN - 1);
    engine->vars[engine->var_count].value[BADUSB_PRO_VAR_VAL_LEN - 1] = '\0';
    engine->var_count++;
    return true;
}

/** Perform variable substitution on a string.
 *  Replaces $VARNAME or ${VARNAME} with variable values.
 *  Writes result into out (must be >= BADUSB_PRO_MAX_LINE_LEN). */
static void substitute_vars(ScriptEngine* engine, const char* input, char* out, size_t out_size) {
    size_t oi = 0;
    const char* p = input;

    while(*p && oi < out_size - 1) {
        if(*p == '$') {
            p++;
            bool braced = false;
            if(*p == '{') {
                braced = true;
                p++;
            }
            /* Extract variable name */
            char vname[BADUSB_PRO_VAR_NAME_LEN];
            size_t vi = 0;
            while(*p && vi < BADUSB_PRO_VAR_NAME_LEN - 1) {
                if(braced) {
                    if(*p == '}') {
                        p++;
                        break;
                    }
                } else {
                    if(!isalnum((unsigned char)*p) && *p != '_') break;
                }
                vname[vi++] = *p++;
            }
            vname[vi] = '\0';

            const char* val = script_engine_get_var(engine, vname);
            if(val) {
                size_t vlen = strlen(val);
                if(oi + vlen < out_size - 1) {
                    memcpy(out + oi, val, vlen);
                    oi += vlen;
                }
            }
        } else {
            out[oi++] = *p++;
        }
    }
    out[oi] = '\0';
}

/* ════════════════════════════════════════════════════════════════════════════
 *  Condition evaluator  —  used by IF and WHILE
 *  Supports:  $var == value,  $var != value,  TRUE,  FALSE
 * ════════════════════════════════════════════════════════════════════════════ */
static bool evaluate_condition(ScriptEngine* engine, const char* cond_str) {
    char buf[BADUSB_PRO_MAX_LINE_LEN];
    substitute_vars(engine, cond_str, buf, sizeof(buf));

    /* TRUE / FALSE literals */
    const char* p = ducky_skip_ws(buf);
    if(ducky_strcicmp(p, "TRUE") == 0) return true;
    if(ducky_strcicmp(p, "FALSE") == 0) return false;

    /* Find operator */
    const char* eq = strstr(p, "==");
    const char* ne = strstr(p, "!=");

    if(eq) {
        /* LHS == RHS */
        char lhs[128] = {0};
        size_t llen = (size_t)(eq - p);
        if(llen > 127) llen = 127;
        strncpy(lhs, p, llen);
        /* Trim trailing spaces from LHS */
        while(llen > 0 && (lhs[llen - 1] == ' ' || lhs[llen - 1] == '\t'))
            lhs[--llen] = '\0';

        const char* rhs = ducky_skip_ws(eq + 2);
        /* Trim trailing spaces from RHS copy */
        char rhs_buf[128];
        strncpy(rhs_buf, rhs, sizeof(rhs_buf) - 1);
        rhs_buf[sizeof(rhs_buf) - 1] = '\0';
        size_t rlen = strlen(rhs_buf);
        while(rlen > 0 && (rhs_buf[rlen - 1] == ' ' || rhs_buf[rlen - 1] == '\t'))
            rhs_buf[--rlen] = '\0';

        return strcmp(lhs, rhs_buf) == 0;
    }

    if(ne) {
        char lhs[128] = {0};
        size_t llen = (size_t)(ne - p);
        if(llen > 127) llen = 127;
        strncpy(lhs, p, llen);
        while(llen > 0 && (lhs[llen - 1] == ' ' || lhs[llen - 1] == '\t'))
            lhs[--llen] = '\0';

        const char* rhs = ducky_skip_ws(ne + 2);
        char rhs_buf[128];
        strncpy(rhs_buf, rhs, sizeof(rhs_buf) - 1);
        rhs_buf[sizeof(rhs_buf) - 1] = '\0';
        size_t rlen = strlen(rhs_buf);
        while(rlen > 0 && (rhs_buf[rlen - 1] == ' ' || rhs_buf[rlen - 1] == '\t'))
            rhs_buf[--rlen] = '\0';

        return strcmp(lhs, rhs_buf) != 0;
    }

    /* Numeric non-zero check */
    int v = atoi(p);
    return v != 0;
}

/* ════════════════════════════════════════════════════════════════════════════
 *  HID key press/release helpers
 * ════════════════════════════════════════════════════════════════════════════ */

/** Type a single character (press + release, handles shift for uppercase/symbols) */
static void type_char(char ch) {
    if((unsigned char)ch < 0x20 || (unsigned char)ch > 0x7E) return;

    /* Use the badusb_ascii_to_hid table declared in ducky_parser.h */
    uint16_t mapped = badusb_ascii_to_hid[(unsigned char)ch - 0x20];
    bool need_shift = (mapped & 0x8000) != 0;
    uint16_t keycode = mapped & 0x7FFF;

    if(need_shift) {
        furi_hal_hid_kb_press(KEY_MOD_LEFT_SHIFT | keycode);
        furi_delay_ms(2);
        furi_hal_hid_kb_release(KEY_MOD_LEFT_SHIFT | keycode);
    } else {
        furi_hal_hid_kb_press(keycode);
        furi_delay_ms(2);
        furi_hal_hid_kb_release(keycode);
    }
}

/** Type a string character-by-character with optional inter-character delay */
static void type_string(ScriptEngine* engine, const char* str) {
    for(size_t i = 0; str[i]; i++) {
        type_char(str[i]);
        if(engine->default_string_delay > 0) {
            furi_delay_ms(adjusted_delay(engine, engine->default_string_delay));
        }
    }
}

/** Press a key combo (array of KEY_MOD_* modifier bits + regular key).
 *  OR all entries together into one combined 16-bit value so the modifier
 *  byte and keycode byte are both set in a single HID report. */
static void press_key_combo(const uint16_t* keycodes, uint8_t count) {
    uint16_t combo = 0;
    for(uint8_t i = 0; i < count; i++) {
        combo |= keycodes[i];
    }
    furi_hal_hid_kb_press(combo);
    furi_delay_ms(30);
    furi_hal_hid_kb_release(combo);
}

/** Press and release a single HID key */
static void press_single_key(uint16_t keycode) {
    furi_hal_hid_kb_press(keycode);
    furi_delay_ms(10);
    furi_hal_hid_kb_release(keycode);
}

/* ════════════════════════════════════════════════════════════════════════════
 *  Flow control helpers
 * ════════════════════════════════════════════════════════════════════════════ */

/** Skip to matching END_IF or ELSE, handling nesting */
static uint16_t find_else_or_endif(ScriptEngine* engine, uint16_t from) {
    int depth = 1;
    for(uint16_t i = from; i < engine->token_count; i++) {
        if(engine->tokens[i].type == TokenIf)
            depth++;
        else if(engine->tokens[i].type == TokenEndIf) {
            depth--;
            if(depth == 0) return i;
        } else if(engine->tokens[i].type == TokenElse && depth == 1) {
            return i;
        }
    }
    return engine->token_count; /* not found — will trigger error */
}

/** Skip to matching END_IF from an ELSE block */
static uint16_t find_endif(ScriptEngine* engine, uint16_t from) {
    int depth = 1;
    for(uint16_t i = from; i < engine->token_count; i++) {
        if(engine->tokens[i].type == TokenIf)
            depth++;
        else if(engine->tokens[i].type == TokenEndIf) {
            depth--;
            if(depth == 0) return i;
        }
    }
    return engine->token_count;
}

/** Skip to matching END_WHILE */
static uint16_t find_end_while(ScriptEngine* engine, uint16_t from) {
    int depth = 1;
    for(uint16_t i = from; i < engine->token_count; i++) {
        if(engine->tokens[i].type == TokenWhile)
            depth++;
        else if(engine->tokens[i].type == TokenEndWhile) {
            depth--;
            if(depth == 0) return i;
        }
    }
    return engine->token_count;
}

/** Find the WHILE that matches an END_WHILE (search backwards) */
static uint16_t find_matching_while(ScriptEngine* engine, uint16_t end_while_idx) {
    int depth = 1;
    for(int16_t i = (int16_t)(end_while_idx - 1); i >= 0; i--) {
        if(engine->tokens[i].type == TokenEndWhile)
            depth++;
        else if(engine->tokens[i].type == TokenWhile) {
            depth--;
            if(depth == 0) return (uint16_t)i;
        }
    }
    return 0;
}

/* ════════════════════════════════════════════════════════════════════════════
 *  OS detection via LED timing
 * ════════════════════════════════════════════════════════════════════════════ */
static void do_os_detect(ScriptEngine* engine) {
    /* Toggle Caps Lock and measure response time */
    uint8_t before = furi_hal_hid_get_led_state();

    /* Press Caps Lock */
    furi_hal_hid_kb_press(HID_KEYBOARD_CAPS_LOCK);
    furi_hal_hid_kb_release(HID_KEYBOARD_CAPS_LOCK);

    /* Wait for LED change, measure time */
    uint32_t start = furi_get_tick();
    uint32_t elapsed = 0;
    bool changed = false;

    while(elapsed < 500) { /* 500ms timeout */
        uint8_t now = furi_hal_hid_get_led_state();
        if(now != before) {
            changed = true;
            break;
        }
        furi_delay_ms(1);
        elapsed = furi_get_tick() - start;
    }

    /* Toggle back */
    furi_hal_hid_kb_press(HID_KEYBOARD_CAPS_LOCK);
    furi_hal_hid_kb_release(HID_KEYBOARD_CAPS_LOCK);
    furi_delay_ms(100);

    const char* os = "UNKNOWN";
    if(changed) {
        if(elapsed <= 25) {
            os = "MAC";
        } else if(elapsed <= 70) {
            os = "WIN";
        } else {
            os = "LINUX";
        }
    }

    script_engine_set_var(engine, "OS", os);
}

/* ════════════════════════════════════════════════════════════════════════════
 *  LED monitoring helpers
 * ════════════════════════════════════════════════════════════════════════════ */

static void poll_led_state(ScriptEngine* engine) {
    engine->led_state = furi_hal_hid_get_led_state();
}

/** LED_CHECK — read a specific LED into a variable
 *  str_value contains: "CAPS", "NUM", or "SCROLL" */
static void do_led_check(ScriptEngine* engine, const char* which) {
    poll_led_state(engine);

    const char* trimmed = ducky_skip_ws(which);
    char name[32];
    const char* val;

    if(ducky_strcicmp(trimmed, "CAPS") == 0) {
        snprintf(name, sizeof(name), "LED_CAPS");
        val = (engine->led_state & LED_CAPS_LOCK_BIT) ? "1" : "0";
    } else if(ducky_strcicmp(trimmed, "NUM") == 0) {
        snprintf(name, sizeof(name), "LED_NUM");
        val = (engine->led_state & LED_NUM_LOCK_BIT) ? "1" : "0";
    } else if(ducky_strcicmp(trimmed, "SCROLL") == 0) {
        snprintf(name, sizeof(name), "LED_SCROLL");
        val = (engine->led_state & LED_SCROLL_LOCK_BIT) ? "1" : "0";
    } else {
        return; /* unknown LED */
    }

    script_engine_set_var(engine, name, val);
}

/** LED_WAIT — block until a specific LED reaches a desired state.
 *  str_value contains e.g. "CAPS ON" or "NUM OFF" */
static void do_led_wait(ScriptEngine* engine, const char* args) {
    char buf[64];
    strncpy(buf, args, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char* saveptr = NULL;
    char* led_name = my_strtok_r(buf, " \t", &saveptr);
    char* state_str = my_strtok_r(NULL, " \t", &saveptr);

    if(!led_name || !state_str) return;

    uint8_t mask = 0;
    if(ducky_strcicmp(led_name, "CAPS") == 0)
        mask = LED_CAPS_LOCK_BIT;
    else if(ducky_strcicmp(led_name, "NUM") == 0)
        mask = LED_NUM_LOCK_BIT;
    else if(ducky_strcicmp(led_name, "SCROLL") == 0)
        mask = LED_SCROLL_LOCK_BIT;
    else
        return;

    bool want_on = (ducky_strcicmp(state_str, "ON") == 0 || strcmp(state_str, "1") == 0);

    /* Poll until condition met or script stopped */
    while(engine->state == ScriptStateRunning) {
        poll_led_state(engine);
        bool is_on = (engine->led_state & mask) != 0;
        if(is_on == want_on) break;
        furi_delay_ms(50);
        notify_ui(engine);
    }
}

/* ════════════════════════════════════════════════════════════════════════════
 *  Mouse helpers
 * ════════════════════════════════════════════════════════════════════════════ */
static uint8_t resolve_mouse_button(const char* name) {
    if(ducky_strcicmp(name, "LEFT") == 0) return MOUSE_BTN_LEFT;
    if(ducky_strcicmp(name, "RIGHT") == 0) return MOUSE_BTN_RIGHT;
    if(ducky_strcicmp(name, "MIDDLE") == 0) return MOUSE_BTN_MIDDLE;
    return MOUSE_BTN_LEFT; /* default */
}

/* ════════════════════════════════════════════════════════════════════════════
 *  VAR assignment
 *  Syntax: "$name = value"  (value may reference other $vars)
 * ════════════════════════════════════════════════════════════════════════════ */
static bool do_var_assign(ScriptEngine* engine, const char* expr) {
    const char* p = ducky_skip_ws(expr);

    /* Expect $name */
    if(*p != '$') return false;
    p++;

    char name[BADUSB_PRO_VAR_NAME_LEN];
    size_t ni = 0;
    while(*p && *p != ' ' && *p != '=' && ni < BADUSB_PRO_VAR_NAME_LEN - 1) {
        name[ni++] = *p++;
    }
    name[ni] = '\0';

    /* Skip to '=' */
    while(*p == ' ' || *p == '\t')
        p++;
    if(*p != '=') return false;
    p++;
    while(*p == ' ' || *p == '\t')
        p++;

    /* Substitute variables in value */
    char value[BADUSB_PRO_VAR_VAL_LEN];
    substitute_vars(engine, p, value, sizeof(value));

    return script_engine_set_var(engine, name, value);
}

/* ════════════════════════════════════════════════════════════════════════════
 *  Initialisation
 * ════════════════════════════════════════════════════════════════════════════ */

void script_engine_init(ScriptEngine* engine) {
    /* Free any previously allocated tokens */
    if(engine->tokens) {
        free(engine->tokens);
    }
    memset(engine, 0, sizeof(*engine));
    engine->tokens = NULL;
    engine->token_count = 0;
    engine->token_capacity = 0;
    engine->state = ScriptStateIdle;
    engine->speed_multiplier = 1.0f;
    engine->default_delay = 0;
    engine->default_string_delay = 0;
}

/* ════════════════════════════════════════════════════════════════════════════
 *  Load tokens + discover function blocks
 * ════════════════════════════════════════════════════════════════════════════ */

void script_engine_load(
    ScriptEngine* engine,
    ScriptToken* tokens,
    uint32_t count,
    uint32_t capacity) {
    /* Free any previously held token buffer */
    if(engine->tokens) {
        free(engine->tokens);
    }

    /* Take ownership of the dynamically allocated token array */
    engine->tokens = tokens;
    engine->token_count = count;
    engine->token_capacity = capacity;
    engine->pc = 0;
    engine->state = ScriptStateLoaded;
    engine->var_count = 0;
    engine->func_count = 0;
    engine->call_depth = 0;
    engine->error_msg[0] = '\0';
    engine->error_line = 0;

    /* Scan for FUNCTION / END_FUNCTION blocks and register them */
    for(uint32_t i = 0; i < count; i++) {
        if(engine->tokens[i].type == TokenFunction) {
            if(engine->func_count >= BADUSB_PRO_MAX_FUNCS) break;
            ScriptFunc* f = &engine->funcs[engine->func_count];
            strncpy(f->name, engine->tokens[i].str_value, BADUSB_PRO_FUNC_NAME_LEN - 1);
            f->name[BADUSB_PRO_FUNC_NAME_LEN - 1] = '\0';
            f->start_index = i + 1; /* first token inside the function */

            /* Find matching END_FUNCTION — if not found, set end_index to -1 */
            f->end_index = -1;
            for(uint32_t j = i + 1; j < count; j++) {
                if(engine->tokens[j].type == TokenEndFunction) {
                    f->end_index = (int32_t)j;
                    break;
                }
            }

            /* Fix #10: If END_FUNCTION was not found, report error and skip registration */
            if(f->end_index < 0) {
                snprintf(
                    engine->error_msg,
                    sizeof(engine->error_msg),
                    "Unmatched FUNCTION '%s' at line %d",
                    f->name,
                    engine->tokens[i].source_line);
                engine->error_line = engine->tokens[i].source_line;
                engine->state = ScriptStateError;
                return;
            }

            engine->func_count++;
        }
    }
}

/* ════════════════════════════════════════════════════════════════════════════
 *  Callbacks / speed
 * ════════════════════════════════════════════════════════════════════════════ */

void script_engine_set_callback(ScriptEngine* engine, void (*callback)(void* ctx), void* ctx) {
    engine->status_callback = callback;
    engine->callback_ctx = ctx;
}

void script_engine_set_speed(ScriptEngine* engine, float multiplier) {
    engine->speed_multiplier = multiplier;
}

/* ════════════════════════════════════════════════════════════════════════════
 *  Pause / Resume / Stop
 * ════════════════════════════════════════════════════════════════════════════ */

void script_engine_pause(ScriptEngine* engine) {
    if(engine->state == ScriptStateRunning) {
        engine->state = ScriptStatePaused;
        notify_ui(engine);
    }
}

void script_engine_resume(ScriptEngine* engine) {
    if(engine->state == ScriptStatePaused) {
        engine->state = ScriptStateRunning;
        notify_ui(engine);
    }
}

void script_engine_stop(ScriptEngine* engine) {
    if(engine->state == ScriptStateRunning || engine->state == ScriptStatePaused) {
        engine->state = ScriptStateDone;
        furi_hal_hid_kb_release_all();
        furi_hal_hid_consumer_key_release_all();
        notify_ui(engine);
    }
}

/* ════════════════════════════════════════════════════════════════════════════
 *  Main execution loop
 * ════════════════════════════════════════════════════════════════════════════ */

void script_engine_run(ScriptEngine* engine) {
    if(engine->state != ScriptStateLoaded && engine->state != ScriptStatePaused) return;

    engine->state = ScriptStateRunning;
    notify_ui(engine);

    /* Skip past FUNCTION blocks at the top level — they are called, not fallen-into */
    bool in_func_def = false;

    while(engine->pc < engine->token_count && engine->state == ScriptStateRunning) {
        /* Handle pause: spin until resumed or stopped */
        while(engine->state == ScriptStatePaused) {
            furi_delay_ms(100);
        }
        if(engine->state != ScriptStateRunning) break;

        ScriptToken* tok = &engine->tokens[engine->pc];

        /* If we're inside a function definition at the top level, skip until END_FUNCTION */
        if(in_func_def) {
            if(tok->type == TokenEndFunction) {
                in_func_def = false;
            }
            engine->pc++;
            continue;
        }

        /* Skip FUNCTION definitions encountered at top level */
        if(tok->type == TokenFunction) {
            in_func_def = true;
            engine->pc++;
            continue;
        }

        /* Poll LED state for UI */
        poll_led_state(engine);
        notify_ui(engine);

        /* ─── Execute token ─────────────────────────────────── */
        switch(tok->type) {
        case TokenRem:
            /* Comment — nothing to do */
            break;

        case TokenString: {
            char expanded[BADUSB_PRO_MAX_LINE_LEN];
            substitute_vars(engine, tok->str_value, expanded, sizeof(expanded));
            type_string(engine, expanded);
            break;
        }

        case TokenStringLn: {
            char expanded[BADUSB_PRO_MAX_LINE_LEN];
            substitute_vars(engine, tok->str_value, expanded, sizeof(expanded));
            type_string(engine, expanded);
            press_single_key(HID_KEYBOARD_RETURN);
            break;
        }

        case TokenDelay:
            furi_delay_ms(adjusted_delay(engine, (uint32_t)tok->int_value));
            break;

        case TokenDefaultDelay:
            engine->default_delay = (uint16_t)tok->int_value;
            break;

        case TokenDefaultStringDelay:
            engine->default_string_delay = (uint16_t)tok->int_value;
            break;

        case TokenKey:
        case TokenEnter:
        case TokenTab:
        case TokenEscape:
        case TokenSpace:
        case TokenBackspace:
        case TokenDelete:
        case TokenHome:
        case TokenEnd:
        case TokenInsert:
        case TokenPageUp:
        case TokenPageDown:
        case TokenUpArrow:
        case TokenDownArrow:
        case TokenLeftArrow:
        case TokenRightArrow:
        case TokenPrintScreen:
        case TokenPause:
        case TokenBreak:
        case TokenCapsLock:
        case TokenNumLock:
        case TokenScrollLock:
        case TokenMenu:
        case TokenF1:
        case TokenF2:
        case TokenF3:
        case TokenF4:
        case TokenF5:
        case TokenF6:
        case TokenF7:
        case TokenF8:
        case TokenF9:
        case TokenF10:
        case TokenF11:
        case TokenF12:
        case TokenGui:
        case TokenAlt:
        case TokenCtrl:
        case TokenShift:
            if(tok->keycode_count > 0) {
                press_single_key(tok->keycodes[0]);
            } else {
                /* Resolve from str_value */
                uint16_t kc = ducky_parser_resolve_keyname(tok->str_value);
                if(kc) press_single_key(kc);
            }
            break;

        case TokenKeyCombo:
            press_key_combo(tok->keycodes, tok->keycode_count);
            break;

        case TokenRepeat: {
            /* Repeat the previous command N times */
            if(engine->pc > 0) {
                uint16_t prev = engine->pc - 1;
                ScriptToken* prev_tok = &engine->tokens[prev];
                /* Save and temporarily re-point */
                uint16_t save_pc = engine->pc;
                for(int32_t r = 0; r < tok->int_value && engine->state == ScriptStateRunning;
                    r++) {
                    engine->pc = prev;
                    /* We'll re-enter the switch for that token type, so just inline the basic ones */
                    if(prev_tok->type == TokenString) {
                        char expanded[BADUSB_PRO_MAX_LINE_LEN];
                        substitute_vars(engine, prev_tok->str_value, expanded, sizeof(expanded));
                        type_string(engine, expanded);
                    } else if(prev_tok->type == TokenStringLn) {
                        char expanded[BADUSB_PRO_MAX_LINE_LEN];
                        substitute_vars(engine, prev_tok->str_value, expanded, sizeof(expanded));
                        type_string(engine, expanded);
                        press_single_key(HID_KEYBOARD_RETURN);
                    } else if(prev_tok->type == TokenDelay) {
                        furi_delay_ms(adjusted_delay(engine, (uint32_t)prev_tok->int_value));
                    } else if(prev_tok->type == TokenKeyCombo) {
                        press_key_combo(prev_tok->keycodes, prev_tok->keycode_count);
                    } else if(
                        prev_tok->type == TokenKey ||
                        (prev_tok->type >= TokenEnter && prev_tok->type <= TokenShift)) {
                        if(prev_tok->keycode_count > 0) {
                            press_single_key(prev_tok->keycodes[0]);
                        } else {
                            uint16_t kc = ducky_parser_resolve_keyname(prev_tok->str_value);
                            if(kc) press_single_key(kc);
                        }
                    }
                    if(engine->default_delay > 0) {
                        furi_delay_ms(adjusted_delay(engine, engine->default_delay));
                    }
                }
                engine->pc = save_pc;
            }
            break;
        }

        case TokenStop:
            engine->state = ScriptStateDone;
            furi_hal_hid_kb_release_all();
            notify_ui(engine);
            return;

        case TokenIf: {
            bool cond = evaluate_condition(engine, tok->str_value);
            if(!cond) {
                /* Skip to ELSE or END_IF */
                uint16_t target = find_else_or_endif(engine, engine->pc + 1);
                if(target >= engine->token_count) {
                    snprintf(
                        engine->error_msg,
                        sizeof(engine->error_msg),
                        "Unmatched IF at line %d",
                        tok->source_line);
                    engine->error_line = tok->source_line;
                    engine->state = ScriptStateError;
                    notify_ui(engine);
                    return;
                }
                engine->pc = target; /* will be incremented at bottom */
                if(engine->tokens[target].type == TokenElse) {
                    /* Continue from after ELSE */
                }
            }
            break;
        }

        case TokenElse: {
            /* If we hit ELSE during execution it means the IF was true and we executed
             * the true branch — so skip to END_IF */
            uint16_t target = find_endif(engine, engine->pc + 1);
            if(target >= engine->token_count) {
                snprintf(
                    engine->error_msg,
                    sizeof(engine->error_msg),
                    "Unmatched ELSE at line %d",
                    tok->source_line);
                engine->error_line = tok->source_line;
                engine->state = ScriptStateError;
                notify_ui(engine);
                return;
            }
            engine->pc = target;
            break;
        }

        case TokenEndIf:
            /* Nothing to do — flow just continues */
            break;

        case TokenWhile: {
            bool cond = evaluate_condition(engine, tok->str_value);
            if(!cond) {
                /* Skip to END_WHILE */
                uint16_t target = find_end_while(engine, engine->pc + 1);
                if(target >= engine->token_count) {
                    snprintf(
                        engine->error_msg,
                        sizeof(engine->error_msg),
                        "Unmatched WHILE at line %d",
                        tok->source_line);
                    engine->error_line = tok->source_line;
                    engine->state = ScriptStateError;
                    notify_ui(engine);
                    return;
                }
                engine->pc = target;
            }
            break;
        }

        case TokenEndWhile: {
            /* Jump back to the matching WHILE */
            uint16_t while_idx = find_matching_while(engine, engine->pc);
            engine->pc = while_idx;
            continue; /* don't increment pc — we want to re-evaluate the WHILE */
        }

        case TokenVar:
            if(!do_var_assign(engine, tok->str_value)) {
                snprintf(
                    engine->error_msg,
                    sizeof(engine->error_msg),
                    "VAR syntax error at line %d",
                    tok->source_line);
                engine->error_line = tok->source_line;
                engine->state = ScriptStateError;
                notify_ui(engine);
                return;
            }
            break;

        case TokenFunction:
            /* Should not reach here (skipped above), but just in case */
            in_func_def = true;
            break;

        case TokenEndFunction:
            /* Return from CALL */
            if(engine->call_depth > 0) {
                engine->call_depth--;
                engine->pc = engine->call_stack[engine->call_depth];
                /* pc will be incremented below */
            }
            break;

        case TokenCall: {
            /* Find function by name */
            bool found = false;
            for(uint8_t fi = 0; fi < engine->func_count; fi++) {
                if(strcmp(engine->funcs[fi].name, tok->str_value) == 0) {
                    if(engine->call_depth >= BADUSB_PRO_MAX_STACK) {
                        snprintf(
                            engine->error_msg,
                            sizeof(engine->error_msg),
                            "Call stack overflow at line %d",
                            tok->source_line);
                        engine->error_line = tok->source_line;
                        engine->state = ScriptStateError;
                        notify_ui(engine);
                        return;
                    }
                    /* Push return address */
                    engine->call_stack[engine->call_depth++] = engine->pc;
                    engine->pc = engine->funcs[fi].start_index;
                    found = true;
                    break; /* found the matching function, stop searching */
                }
            }
            if(!found) {
                snprintf(
                    engine->error_msg,
                    sizeof(engine->error_msg),
                    "Unknown function: %.100s",
                    tok->str_value);
                engine->error_line = tok->source_line;
                engine->state = ScriptStateError;
                notify_ui(engine);
                return;
            }
            continue; /* skip the pc++ below since we already jumped */
        }

        case TokenLedCheck:
            do_led_check(engine, tok->str_value);
            break;

        case TokenLedWait:
            do_led_wait(engine, tok->str_value);
            break;

        case TokenOsDetect:
            do_os_detect(engine);
            break;

        case TokenMouseMove: {
            int32_t mx = tok->int_value;
            int32_t my = tok->int_value2;
            if(mx < INT8_MIN) mx = INT8_MIN;
            if(mx > INT8_MAX) mx = INT8_MAX;
            if(my < INT8_MIN) my = INT8_MIN;
            if(my > INT8_MAX) my = INT8_MAX;
            furi_hal_hid_mouse_move((int8_t)mx, (int8_t)my);
            break;
        }

        case TokenMouseClick: {
            uint8_t btn = resolve_mouse_button(tok->str_value);
            furi_hal_hid_mouse_press(btn);
            furi_delay_ms(10);
            furi_hal_hid_mouse_release(btn);
            break;
        }

        case TokenMouseScroll:
            furi_hal_hid_mouse_scroll((int8_t)tok->int_value);
            break;

        case TokenConsumerKey: {
            uint16_t consumer_id = resolve_consumer_key(tok->str_value);
            if(consumer_id != 0) {
                furi_hal_hid_consumer_key_press(consumer_id);
                furi_delay_ms(10);
                furi_hal_hid_consumer_key_release(consumer_id);
            }
            break;
        }

        case TokenRestart:
            /* Reset program counter to restart from the beginning */
            engine->pc = 0;
            engine->call_depth = 0;
            continue; /* skip pc++ */

        default:
            break;
        }

        /* Advance program counter */
        engine->pc++;

        /* Apply default inter-command delay */
        if(engine->default_delay > 0 && engine->state == ScriptStateRunning) {
            furi_delay_ms(adjusted_delay(engine, engine->default_delay));
        }
    }

    /* Finished */
    if(engine->state == ScriptStateRunning) {
        engine->state = ScriptStateDone;
    }
    furi_hal_hid_kb_release_all();
    furi_hal_hid_consumer_key_release_all();
    notify_ui(engine);
}
