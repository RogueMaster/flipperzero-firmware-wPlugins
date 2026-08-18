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
    // Nothing has been decided yet, and it is first so that zero means this.
    // GENUINE used to be the zero value, which made "this chip is real" the
    // answer any uninitialised or partly filled ChipIdentification gave --
    // including the one chip_db_identify memsets before it starts work. Every
    // path through that function does assign a verdict today, so nothing shows
    // it; the point is that the app's worst possible output should never be one
    // missing return statement away.
    VerdictNotChecked,
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

// A pin that can take a part off the I2C bus entirely while leaving it in
// perfect health. Two flavours: a protocol select that hands the part to SPI
// or UART, and an enable pin that holds it in reset. Either way an I2C sweep
// finds nothing, which is indistinguishable from a dead chip unless the app
// says otherwise -- and someone has already returned a working BNO055 over
// exactly this.
typedef enum {
    ModePinProtocol, // picks which bus the part speaks
    ModePinEnable, // holds the part in reset or shutdown
} ModePinKind;

typedef enum {
    ModeAltSpi,
    ModeAltUart,
    ModeAltOff, // not another protocol: the part simply is not running
} ModeAlt;

typedef struct {
    const char* chip; // exact ChipEntry.name; the doc generator checks this
    const char* pad; // what the breakout silkscreens: "CSB", "CS", "PS1"
    uint8_t kind; // ModePinKind
    uint8_t alt; // ModeAlt: where the part goes when the pad is wrong
    bool i2c_high; // level this pad needs for the part to speak I2C
    // Sampled at reset. Strapping the pad is then not enough on its own: the
    // part keeps whatever it latched until the power is cycled, which is why
    // the fix screen offers to cycle the rail rather than just say "rescan".
    bool latched;
} ChipModePin;

// NULL is the normal answer: most parts have no such pin.
const ChipModePin* chip_mode_pin_for(const char* chip_name);

// Iteration, for the docs generator and the silent-bus screens.
size_t chip_mode_pin_count(void);
const ChipModePin* chip_mode_pin_get(size_t index);

// Number of chips in the database, for the About screen.
size_t chip_db_count(void);

// Iteration, for the "what does this know?" browser.
const ChipEntry* chip_db_get(size_t index);
