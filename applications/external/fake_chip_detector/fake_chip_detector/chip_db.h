#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define CHIP_MAX_ADDRS  4
#define CHIP_MAX_CHECKS 4

typedef struct {
    uint16_t reg; // register index, 8-bit unless reg16 is set
    uint16_t expected;
    uint16_t mask; // 0 means 0xFF/0xFFFF depending on width
    bool wide; // true = 16-bit big-endian value
    bool reg16; // true = the index itself is 16-bit big-endian (ST ToF, Goodix)
} IdCheck;

typedef struct {
    const char* name;
    const char* kind; // what the part does, in plain words
    uint8_t addrs[CHIP_MAX_ADDRS]; // 0xFF = end of list
    uint8_t range_lo; // inclusive contiguous address range, 0 = unused
    uint8_t range_hi;
    const IdCheck* checks; // NULL = chip has no ID register
    uint8_t check_count;
    const char* note; // short caveat shown on the detail screen, or NULL
} ChipEntry;

typedef enum {
    VerdictGenuine, // all ID registers match
    VerdictWrongChip, // some IDs of a known chip match and others do not
    VerdictNoMatch, // answers, but nothing matched any candidate here
    VerdictDetectedNoId, // address belongs to a chip without an ID register
    VerdictUnknown, // address not in the database
    VerdictNoAnswer, // device stopped answering register reads
} ChipVerdict;

typedef struct {
    uint16_t reg;
    uint16_t expected;
    uint16_t actual;
    bool wide;
    bool reg16;
    bool has_expected; // false for raw probe reads of unknown devices
    bool read_ok; // distinguishes "read 0x00" from "could not read"
    bool match;
} IdReadResult;

typedef struct {
    const ChipEntry* chip; // best match, NULL for unknown
    ChipVerdict verdict;
    IdReadResult reads[CHIP_MAX_CHECKS];
    uint8_t read_count;
} ChipIdentification;

// Probes the device at addr7 and fills out the identification result.
void chip_db_identify(uint8_t addr7, ChipIdentification* out);

const char* chip_verdict_str(ChipVerdict verdict);
const char* chip_verdict_short_str(ChipVerdict verdict);

// One-word headline for the summary screen.
const char* chip_verdict_headline(ChipVerdict verdict);

// Two short lines of plain language saying what the verdict actually means,
// so the user is never left holding a word they have to interpret.
void chip_verdict_explain(ChipVerdict verdict, const char** line1, const char** line2);

// True when the verdict means "nothing is wrong here".
bool chip_verdict_is_good(ChipVerdict verdict);

// Number of chips in the database, for the About screen.
size_t chip_db_count(void);

// Iteration, for the "what does this know?" browser.
const ChipEntry* chip_db_get(size_t index);
