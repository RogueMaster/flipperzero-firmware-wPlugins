/* card_parser.c — .ccid card profile parser for CCID Emulator.
 * Loads AID-to-APDU response mappings from SD card profile files. */

#include "card_parser.h"

#include <furi.h>
#include <storage/storage.h>
#include <toolbox/stream/file_stream.h>
#include <toolbox/stream/buffered_file_stream.h>

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ---------------------------------------------------------------------------
 * Internal helpers
 * --------------------------------------------------------------------------- */

/** Convert a single hex character to its 4-bit value, or -1 on error. */
static int hex_char_to_nibble(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/**
 * Parse a space-separated hex string (e.g. "00 A4 04 00 07") into a byte
 * array.  Returns the number of bytes written, or 0 on error.
 *
 * @param hex_str   input string  (may contain leading/trailing whitespace)
 * @param out       output buffer (must be large enough)
 * @param out_max   maximum bytes to write
 */
static uint16_t parse_hex_string(const char* hex_str, uint8_t* out, uint16_t out_max) {
    uint16_t count = 0;
    const char* p = hex_str;

    while(*p && count < out_max) {
        /* skip whitespace */
        while(*p == ' ' || *p == '\t')
            p++;
        if(*p == '\0' || *p == '\n' || *p == '\r') break;

        int hi = hex_char_to_nibble(*p);
        if(hi < 0) return 0;
        p++;
        if(*p == '\0') return 0; /* need two nibbles */
        int lo = hex_char_to_nibble(*p);
        if(lo < 0) return 0;
        p++;

        out[count++] = (uint8_t)((hi << 4) | lo);
    }
    return count;
}

/**
 * Parse a command pattern hex string that may contain "??" wildcard tokens.
 * Produces both the byte array and a parallel mask array:
 *   mask[i] = 0xFF  -> exact match required
 *   mask[i] = 0x00  -> wildcard (match any byte)
 *
 * Returns the number of bytes, or 0 on error.
 */
static uint16_t
    parse_hex_pattern(const char* hex_str, uint8_t* out, uint8_t* mask, uint16_t out_max) {
    uint16_t count = 0;
    const char* p = hex_str;

    while(*p && count < out_max) {
        while(*p == ' ' || *p == '\t')
            p++;
        if(*p == '\0' || *p == '\n' || *p == '\r') break;

        if(p[0] == '?' && p[1] == '?') {
            out[count] = 0x00;
            mask[count] = 0x00; /* wildcard */
            count++;
            p += 2;
        } else {
            int hi = hex_char_to_nibble(*p);
            if(hi < 0) return 0;
            p++;
            if(*p == '\0') return 0;
            int lo = hex_char_to_nibble(*p);
            if(lo < 0) return 0;
            p++;

            out[count] = (uint8_t)((hi << 4) | lo);
            mask[count] = 0xFF; /* exact */
            count++;
        }
    }
    return count;
}

/* ---------------------------------------------------------------------------
 * Line-level helpers
 * --------------------------------------------------------------------------- */

/** Strip leading/trailing whitespace in place and return pointer to first
 *  non-whitespace character.  Modifies the buffer by null-terminating early. */
static char* strip(char* s) {
    while(*s == ' ' || *s == '\t')
        s++;
    if(*s == '\0') return s;
    char* end = s + strlen(s) - 1;
    while(end > s && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
        *end = '\0';
        end--;
    }
    return s;
}

/** Return true if the line (already stripped) is a section header like
 *  "[card]", and write the section name (without brackets) into `name`. */
static bool is_section_header(const char* line, char* name, size_t name_max) {
    if(line[0] != '[') return false;
    const char* close = strchr(line, ']');
    if(!close) return false;
    size_t len = (size_t)(close - line - 1);
    if(len == 0 || len >= name_max) return false;
    memcpy(name, line + 1, len);
    name[len] = '\0';
    return true;
}

/* ---------------------------------------------------------------------------
 * Section parsers
 * --------------------------------------------------------------------------- */

typedef enum {
    SectionNone,
    SectionCard,
    SectionRules,
    SectionDefault,
} Section;

/** Parse a "key = value" line inside [card]. */
static void parse_card_kv(CcidCard* card, const char* key, const char* value) {
    if(strcmp(key, "name") == 0) {
        strncpy(card->name, value, CCID_EMU_MAX_NAME_LEN - 1);
        card->name[CCID_EMU_MAX_NAME_LEN - 1] = '\0';
    } else if(strcmp(key, "description") == 0) {
        strncpy(card->description, value, CCID_EMU_MAX_DESC_LEN - 1);
        card->description[CCID_EMU_MAX_DESC_LEN - 1] = '\0';
    } else if(strcmp(key, "atr") == 0) {
        card->atr_len = parse_hex_string(value, card->atr, CCID_EMU_MAX_ATR_LEN);
    }
}

/** Parse a rule line:  COMMAND_HEX = RESPONSE_HEX */
static void parse_rule_line(CcidCard* card, const char* line) {
    if(card->rule_count >= CCID_EMU_MAX_RULES) return;

    /* Find the '=' separator.  We must be careful because hex strings also
       contain letters, so we look for " = " (with surrounding spaces). */
    const char* eq = strstr(line, "=");
    if(!eq) return;

    /* Copy left-hand side (command pattern) */
    size_t cmd_part_len = (size_t)(eq - line);
    char cmd_buf[CCID_EMU_MAX_HEX_STR];
    if(cmd_part_len >= sizeof(cmd_buf)) cmd_part_len = sizeof(cmd_buf) - 1;
    memcpy(cmd_buf, line, cmd_part_len);
    cmd_buf[cmd_part_len] = '\0';

    /* Right-hand side (response) */
    const char* resp_part = eq + 1;

    char* cmd_stripped = strip(cmd_buf);
    /* resp_part may have leading spaces -- strip copies into itself */
    char resp_buf[CCID_EMU_MAX_HEX_STR];
    strncpy(resp_buf, resp_part, sizeof(resp_buf) - 1);
    resp_buf[sizeof(resp_buf) - 1] = '\0';
    char* resp_stripped = strip(resp_buf);

    CcidRule* rule = &card->rules[card->rule_count];

    rule->command_len =
        parse_hex_pattern(cmd_stripped, rule->command, rule->mask, CCID_EMU_MAX_APDU_LEN);
    if(rule->command_len == 0) return;

    rule->response_len = parse_hex_string(resp_stripped, rule->response, CCID_EMU_MAX_APDU_LEN);
    if(rule->response_len == 0) return;

    card->rule_count++;
}

/** Parse the [default] section key/value. */
static void parse_default_kv(CcidCard* card, const char* key, const char* value) {
    if(strcmp(key, "response") == 0) {
        card->default_response_len =
            parse_hex_string(value, card->default_response, CCID_EMU_MAX_APDU_LEN);
    }
}

/* ---------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------- */

CcidCard* ccid_card_load(Storage* storage, const char* path) {
    furi_assert(storage);
    furi_assert(path);

    CcidCard* card = malloc(sizeof(CcidCard));
    furi_assert(card);
    memset(card, 0, sizeof(CcidCard));

    /* Set built-in default response: 6A 82 (file/application not found) */
    card->default_response[0] = 0x6A;
    card->default_response[1] = 0x82;
    card->default_response_len = 2;

    Stream* stream = buffered_file_stream_alloc(storage);
    if(!buffered_file_stream_open(stream, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E("CcidParser", "Cannot open %s", path);
        stream_free(stream);
        free(card);
        return NULL;
    }

    FuriString* line_buf = furi_string_alloc();
    Section section = SectionNone;

    while(stream_read_line(stream, line_buf)) {
        char line[256];
        size_t line_len = furi_string_size(line_buf);
        if(line_len >= sizeof(line)) line_len = sizeof(line) - 1;
        memcpy(line, furi_string_get_cstr(line_buf), line_len);
        line[line_len] = '\0';

        char* stripped = strip(line);

        /* Skip blank lines and comments */
        if(stripped[0] == '\0' || stripped[0] == '#' || stripped[0] == ';') continue;

        /* Check for section header */
        char sec_name[32];
        if(is_section_header(stripped, sec_name, sizeof(sec_name))) {
            if(strcmp(sec_name, "card") == 0) {
                section = SectionCard;
            } else if(strcmp(sec_name, "rules") == 0) {
                section = SectionRules;
            } else if(strcmp(sec_name, "default") == 0) {
                section = SectionDefault;
            } else {
                section = SectionNone;
            }
            continue;
        }

        /* Process line according to current section */
        switch(section) {
        case SectionCard:
        case SectionDefault: {
            /* key = value */
            char* eq = strchr(stripped, '=');
            if(!eq) break;
            *eq = '\0';
            char* key = strip(stripped);
            char* val = strip(eq + 1);
            if(section == SectionCard) {
                parse_card_kv(card, key, val);
            } else {
                parse_default_kv(card, key, val);
            }
            break;
        }
        case SectionRules:
            parse_rule_line(card, stripped);
            break;
        default:
            break;
        }
    }

    furi_string_free(line_buf);
    stream_free(stream);

    /* If no name was set, use filename */
    if(card->name[0] == '\0') {
        const char* slash = strrchr(path, '/');
        const char* fname = slash ? slash + 1 : path;
        strncpy(card->name, fname, CCID_EMU_MAX_NAME_LEN - 1);
        card->name[CCID_EMU_MAX_NAME_LEN - 1] = '\0';
    }

    FURI_LOG_I(
        "CcidParser",
        "Loaded card \"%s\": ATR %u bytes, %u rules",
        card->name,
        card->atr_len,
        card->rule_count);

    return card;
}

void ccid_card_free(CcidCard* card) {
    if(card) {
        free(card);
    }
}

/* ---------------------------------------------------------------------------
 * Sample card writer
 * --------------------------------------------------------------------------- */

static const char sample_card_content[] =
    "# CCID Emulator - sample smartcard definition\n"
    "#\n"
    "# Lines starting with # or ; are comments.\n"
    "# Hex bytes are space-separated.  ?? is a single-byte wildcard.\n"
    "\n"
    "[card]\n"
    "name = Test Card\n"
    "description = Basic test card with SELECT responses\n"
    "atr = 3B 90 95 80 1F C3 59\n"
    "\n"
    "[rules]\n"
    "# SELECT by AID (MasterFile)\n"
    "00 A4 04 00 07 A0 00 00 00 04 10 10 = 6F 18 84 07 A0 00 00 00 04 10 10 A5 0D 50 04 56 49 53 41 87 01 01 9F 11 01 01 90 00\n"
    "# SELECT PSE\n"
    "00 A4 04 00 0E 31 50 41 59 2E 53 59 53 2E 44 44 46 30 31 = 6F 1C 84 0E 31 50 41 59 2E 53 59 53 2E 44 44 46 30 31 A5 0A 88 01 01 5F 2D 04 65 6E 66 72 90 00\n"
    "# GET PROCESSING OPTIONS\n"
    "80 A8 00 00 02 83 00 = 77 0A 82 02 19 80 94 04 08 01 01 00 90 00\n"
    "# READ RECORD (wildcard on P1/P2)\n"
    "00 B2 ?? ?? 00 = 70 00 90 00\n"
    "# GET RESPONSE (any Le)\n"
    "00 C0 00 00 ?? = 90 00\n"
    "\n"
    "[default]\n"
    "response = 6A 82\n";

/* ---------------------------------------------------------------------------
 * Additional sample card definitions
 * --------------------------------------------------------------------------- */

#define CCID_EMU_PIV_FILE      EXT_PATH("ccid_emulator/cards/piv_card.ccid")
#define CCID_EMU_JAVACARD_FILE EXT_PATH("ccid_emulator/cards/javacard.ccid")

static const char piv_card_content[] =
    "# CCID Emulator - NIST PIV card emulation\n"
    "#\n"
    "# Implements a read-only subset of FIPS 201 PIV (Personal Identity Verification).\n"
    "# Commands follow SP 800-73-4 Part 2.\n"
    "\n"
    "[card]\n"
    "name = PIV Card\n"
    "description = NIST PIV applet (read-only emulation)\n"
    "atr = 3B 7F 96 00 00 80 31 80 65 B0 85 03 00 EF 12 00 F6 82 90 00\n"
    "\n"
    "[rules]\n"
    "# SELECT PIV applet AID (CLA=00 INS=A4 P1=04 P2=00 Lc=0B)\n"
    "00 A4 04 00 0B A0 00 00 03 08 00 00 10 00 01 00 = 61 11 4F 06 00 00 10 00 01 00 79 07 4F 05 A0 00 00 03 08 90 00\n"
    "# GET DATA - Card Holder Unique Identifier (CLA=00 INS=CB P1=3F P2=FF)\n"
    "00 CB 3F FF 05 5C 03 5F C1 02 = 53 10 30 19 D4 E7 39 DA 73 9C ED 39 CE 73 9D 83 68 58 90 00\n"
    "# GET RESPONSE (wildcard Le)\n"
    "00 C0 00 00 ?? = 90 00\n"
    "\n"
    "[default]\n"
    "response = 6A 82\n";

static const char javacard_content[] =
    "# CCID Emulator - Generic Java Card emulation\n"
    "#\n"
    "# Responds to ISD (Issuer Security Domain) selection and GET STATUS.\n"
    "# Useful for testing Java Card aware readers.\n"
    "\n"
    "[card]\n"
    "name = Generic Java Card\n"
    "description = Generic Java Card with ISD selection\n"
    "atr = 3B 68 00 00 00 73 C8 40 12 00 90 00\n"
    "\n"
    "[rules]\n"
    "# SELECT ISD (Issuer Security Domain)\n"
    "00 A4 04 00 08 A0 00 00 01 51 00 00 00 = 6F 10 84 08 A0 00 00 01 51 00 00 00 A5 04 9F 65 01 FF 90 00\n"
    "# GET STATUS - ISD\n"
    "80 F2 80 00 02 4F 00 00 = 08 A0 00 00 01 51 00 00 00 07 9E 90 00\n"
    "\n"
    "[default]\n"
    "response = 6A 82\n";

/** Write a single sample file if it does not already exist. */
static void write_sample_file(Storage* storage, const char* path, const char* content) {
    if(storage_file_exists(storage, path)) return;

    Stream* stream = file_stream_alloc(storage);
    if(file_stream_open(stream, path, FSAM_WRITE, FSOM_CREATE_NEW)) {
        stream_write_cstring(stream, content);
        FURI_LOG_I("CcidParser", "Sample card written to %s", path);
    } else {
        FURI_LOG_E("CcidParser", "Failed to write sample card: %s", path);
    }
    stream_free(stream);
}

void ccid_card_write_sample(Storage* storage) {
    furi_assert(storage);

    /* Make sure directory tree exists */
    storage_simply_mkdir(storage, EXT_PATH("ccid_emulator"));
    storage_simply_mkdir(storage, CCID_EMU_CARDS_DIR);

    write_sample_file(storage, CCID_EMU_SAMPLE_FILE, sample_card_content);
    write_sample_file(storage, CCID_EMU_PIV_FILE, piv_card_content);
    write_sample_file(storage, CCID_EMU_JAVACARD_FILE, javacard_content);
}
