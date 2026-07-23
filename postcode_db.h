#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define POSTCODE_DB_RECORD_COUNT 452U
#define POSTCODE_DB_EXACT_COUNT  191U
#define POSTCODE_DB_MASKED_COUNT 261U
#define POSTCODE_DB_FALLBACK_COUNT 5U

typedef enum {
    PostcodeConsoleXboxOnePhat = 0,
    PostcodeConsoleXboxOneS,
    PostcodeConsoleXboxOneX,
    PostcodeConsoleXboxSeriesS,
    PostcodeConsoleXboxSeriesX,
    PostcodeConsoleCount,
} PostcodeConsole;

typedef struct {
    uint16_t code;
    uint16_t mask;
    uint8_t consoles;
    uint8_t type;
    bool is_error;
    const char* name;
    const char* description;
} PostcodeDbRecord;

const char* postcode_db_console_name(PostcodeConsole console);
const char* postcode_db_console_short(PostcodeConsole console);
const char* postcode_db_type_short(uint8_t type);
bool postcode_db_record_at(size_t index, PostcodeDbRecord* output);

/**
 * Look up an Xbox POST code using its bus segment and selected console.
 *
 * name_output is always populated. description points to static database
 * storage and may be an empty string. Returns true for a database match.
 */
bool postcode_db_format(
    uint16_t code,
    uint8_t segment,
    PostcodeConsole console,
    char* name_output,
    size_t name_output_size,
    const char** description,
    bool* is_error);
