#pragma once

#include <stdint.h>

// Compact OUI -> short vendor table. Filled from a text file on the SD card.
// Not the IEEE registry. Unknown prefixes return NULL. The name pointer stays
// valid until the next g5_oui_reset().

#define G5_OUI_MAX  384
// 12 visible ASCII characters + NUL. Longer names are cut.
#define G5_OUI_NAME 13

typedef enum {
    G5OuiSkip = 0,
    G5OuiAdded = 1,
    G5OuiFull = 2,
} G5OuiAddResult;

void g5_oui_reset(void);
// One text line: "AA:BB:CC Vendor", "AABBCC Vendor", or a full MAC plus vendor.
// '#' comments and blank lines are skipped. A repeated OUI replaces the name.
// G5OuiFull means this line is a new OUI and the table is already at G5_OUI_MAX.
G5OuiAddResult g5_oui_add_line(const char* line);
// "AA:BB:CC:DD:EE:FF" or "AABBCCDDEEFF". NULL when the prefix is unknown.
const char* g5_oui_lookup(const char* mac);
uint16_t g5_oui_count(void);
