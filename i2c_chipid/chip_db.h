#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define CHIP_MAX_ADDRS 4
#define CHIP_MAX_CHECKS 4

typedef struct {
    uint8_t reg;
    uint16_t expected;
    uint16_t mask; // 0 means 0xFF/0xFFFF depending on width
    bool wide; // true = 16-bit big-endian read of reg and reg+1
} IdCheck;

typedef struct {
    const char* name;
    uint8_t addrs[CHIP_MAX_ADDRS]; // 0xFF = end of list
    uint8_t range_lo; // inclusive contiguous address range, 0 = unused
    uint8_t range_hi;
    const IdCheck* checks; // NULL = chip has no ID register
    uint8_t check_count;
    const char* note; // short caveat shown on the detail screen, or NULL
} ChipEntry;

typedef enum {
    VerdictGenuine, // all ID registers match
    VerdictWrongChip, // device answers but IDs match no known candidate
    VerdictDetectedNoId, // address belongs to a chip without an ID register
    VerdictUnknown, // address not in the database
    VerdictNoAnswer, // device stopped answering register reads
} ChipVerdict;

typedef struct {
    uint8_t reg;
    uint16_t expected;
    uint16_t actual;
    bool wide;
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

// Number of chips in the database, for the About screen.
size_t chip_db_count(void);
