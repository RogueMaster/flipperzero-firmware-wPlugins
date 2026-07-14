#pragma once

#include "badusb_pro.h"

#ifdef __cplusplus
extern "C" {
#endif

/** ASCII (0x20..0x7E) to HID keycode lookup table.
 *  Bit 15 (0x8000) means "hold Shift". Defined in ducky_parser.c. */
extern const uint16_t badusb_ascii_to_hid[95];

/** Case-insensitive string compare */
int ducky_strcicmp(const char* a, const char* b);

/** Skip leading whitespace, return pointer into same buffer */
const char* ducky_skip_ws(const char* s);

/** Strip trailing whitespace / CR / LF in-place */
void ducky_strip_trailing(char* s);

/**
 * Parse a DuckyScript file from the SD card into an array of ScriptTokens.
 *
 * @param storage    open Storage pointer
 * @param path       full path on the SD card  (e.g. "/ext/badusb_pro/payload.ds")
 * @param tokens     output array (caller provides)
 * @param max_tokens capacity of the tokens array
 * @param out_count  number of tokens written
 * @param err_msg    buffer for error message (>= 128 chars)
 * @param err_line   set to the 1-based line number of first parse error, 0 on success
 * @return true on success
 */
bool ducky_parser_parse_file(
    Storage* storage,
    const char* path,
    ScriptToken* tokens,
    uint16_t max_tokens,
    uint16_t* out_count,
    char* err_msg,
    uint16_t* err_line);

/**
 * Parse a single DuckyScript line into a token.
 *
 * @param line       null-terminated line text (leading/trailing whitespace stripped)
 * @param token      output token
 * @param err_msg    buffer for error string
 * @return true on success
 */
bool ducky_parser_parse_line(const char* line, ScriptToken* token, char* err_msg);

/**
 * Resolve a key name string (e.g. "ENTER", "a", "F5") to a HID keycode.
 *
 * @param name  key name (case-insensitive for special keys, case-sensitive for printable)
 * @return HID keycode, or 0 on failure
 */
uint16_t ducky_parser_resolve_keyname(const char* name);

/**
 * Count the number of lines in a script file.
 */
uint16_t ducky_parser_count_lines(Storage* storage, const char* path);

#ifdef __cplusplus
}
#endif
